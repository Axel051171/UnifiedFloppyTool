/**
 * @file test_edsk_error_marks.c
 * @brief EDSK disk-error marking: read, represent, preserve on write.
 *
 * Links the real Amstrad/Spectrum DSK plugin (src/formats/dsk_cpc/uft_dsk_cpc.c).
 * Phase-4 disk-error work package for a second error-aware format. Extended DSK
 * stores the uPD765 FDC result bytes ST1/ST2 per sector in the Track-Info
 * block: ST1 bit5 (0x20) or ST2 bit5 (0x20) = data CRC error, ST2 bit6 (0x40) =
 * control mark (deleted data). Requirement: read the marks, represent them on
 * uft_sector_t, preserve them on write.
 *
 * Construction: an EXTENDED DSK with one track of three 256-byte sectors —
 * normal, deleted (ST2=0x40), and CRC-error (ST1=0x20,ST2=0x20). We open, read
 * the track and assert the plugin surfaced deleted/crc_ok on the right sectors,
 * then write the track back and read again asserting the marks survive. The
 * DSK writer overwrites only sector data and leaves the Track-Info block (with
 * ST1/ST2) untouched, so a captured error is never silently downgraded.
 *
 * Documented limit (same as IMD): the writer preserves the existing ST1/ST2
 * status; it does not re-encode a caller-flipped status into new FDC bytes.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_dsk_cpc;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-36s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_edsk_err_%d.dsk", dir, rand() % 100000);
}

static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

/* Build a minimal EXTENDED DSK: 256B header + 1 track (256B Track-Info + three
   256B sectors). Sector 1 normal, sector 2 deleted (ST2 0x40), sector 3 CRC
   error (ST1 0x20 + ST2 0x20). First data byte of each sector is a tag. */
static int build_edsk(const char *path) {
    uint8_t header[256];
    memset(header, 0, sizeof(header));
    memcpy(header, "EXTENDED CPC DSK File\r\nDisk-Info\r\n", 34);
    header[0x30] = 1;          /* 1 track */
    header[0x31] = 1;          /* 1 side */
    header[0x34] = 5;          /* track 0 size = 5*256 = 1280 (info + 4 sectors) */

    uint8_t tinfo[256];
    memset(tinfo, 0, sizeof(tinfo));
    memcpy(tinfo, "Track-Info\r\n", 12);
    tinfo[0x10] = 0;           /* track number */
    tinfo[0x11] = 0;           /* side */
    tinfo[0x14] = 1;           /* sector size code 1 -> 256 bytes */
    tinfo[0x15] = 4;           /* 4 sectors */
    tinfo[0x17] = 0xE5;        /* filler */
    /* sector info list: C,H,R,N,ST1,ST2,size_lo,size_hi (8 bytes each).
     * uPD765: ST2 bit5 (0x20) = data-field CRC; ST1 bit5 (0x20) without ST2
     * bit5 = ID-field CRC; ST2 bit6 (0x40) = deleted. */
    uint8_t *si;
    si = &tinfo[0x18 + 0*8];   /* normal          */
    si[0]=0;si[1]=0;si[2]=0xC1;si[3]=1;si[4]=0x00;si[5]=0x00;si[6]=0x00;si[7]=0x01;
    si = &tinfo[0x18 + 1*8];   /* deleted         */
    si[0]=0;si[1]=0;si[2]=0xC2;si[3]=1;si[4]=0x00;si[5]=0x40;si[6]=0x00;si[7]=0x01;
    si = &tinfo[0x18 + 2*8];   /* data CRC error  */
    si[0]=0;si[1]=0;si[2]=0xC3;si[3]=1;si[4]=0x20;si[5]=0x20;si[6]=0x00;si[7]=0x01;
    si = &tinfo[0x18 + 3*8];   /* ID-field CRC    */
    si[0]=0;si[1]=0;si[2]=0xC4;si[3]=1;si[4]=0x20;si[5]=0x00;si[6]=0x00;si[7]=0x01;

    uint8_t sec[4][256];
    memset(sec, 0x10, sizeof(sec));
    sec[0][0] = 0xA1;          /* normal tag   */
    sec[1][0] = 0xB2;          /* deleted tag  */
    sec[2][0] = 0xC3;          /* data-crc tag */
    sec[3][0] = 0xD4;          /* id-crc tag   */

    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = fwrite(header, 1, 256, f) == 256 &&
             fwrite(tinfo, 1, 256, f) == 256 &&
             fwrite(sec[0], 1, 256, f) == 256 &&
             fwrite(sec[1], 1, 256, f) == 256 &&
             fwrite(sec[2], 1, 256, f) == 256 &&
             fwrite(sec[3], 1, 256, f) == 256;
    fclose(f);
    return ok;
}

static const uft_sector_t *find_by_tag(const uft_track_t *t, uint8_t tag) {
    for (size_t i = 0; i < t->sector_count; i++)
        if (t->sectors[i].data && t->sectors[i].data[0] == tag)
            return &t->sectors[i];
    return NULL;
}

static void assert_marks(uft_track_t *t) {
    ASSERT(t->sector_count == 4);
    const uft_sector_t *n = find_by_tag(t, 0xA1);
    const uft_sector_t *d = find_by_tag(t, 0xB2);
    const uft_sector_t *e = find_by_tag(t, 0xC3);   /* data CRC */
    const uft_sector_t *i = find_by_tag(t, 0xD4);   /* ID CRC   */
    ASSERT(n && d && e && i);
    /* normal */
    ASSERT(n->crc_ok == true);
    ASSERT(n->id_crc_ok == true);
    ASSERT(n->deleted == false);
    /* deleted */
    ASSERT(d->deleted == true);
    ASSERT(d->crc_ok == true);
    /* data-field CRC error: data bad, ID ok (separated) */
    ASSERT(e->crc_ok == false);
    ASSERT(e->crc_valid == false);
    ASSERT(e->data_crc_ok == false);
    ASSERT(e->id_crc_ok == true);
    /* ID-field CRC error: ID bad, data ok */
    ASSERT(i->id_crc_ok == false);
    ASSERT(i->crc_ok == true);
}

TEST(read_surfaces_error_marks) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_edsk(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_dsk_cpc.open(&disk, path, false) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_dsk_cpc.read_track(&disk, 0, 0, &t) == UFT_OK);
    assert_marks(&t);

    free_track_sectors(&t);
    if (uft_format_plugin_dsk_cpc.close) uft_format_plugin_dsk_cpc.close(&disk);
    remove(path);
}

TEST(marks_survive_write_read) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_edsk(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_dsk_cpc.open(&disk, path, false) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_dsk_cpc.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(uft_format_plugin_dsk_cpc.write_track(&disk, 0, 0, &t) == UFT_OK);
    free_track_sectors(&t);

    uft_track_t t2;
    memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_dsk_cpc.read_track(&disk, 0, 0, &t2) == UFT_OK);
    assert_marks(&t2);

    free_track_sectors(&t2);
    if (uft_format_plugin_dsk_cpc.close) uft_format_plugin_dsk_cpc.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== EDSK disk-error marking (read / represent / preserve) ===\n");
    RUN(read_surfaces_error_marks);
    RUN(marks_survive_write_read);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
