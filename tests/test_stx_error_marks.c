/**
 * @file test_stx_error_marks.c
 * @brief STX (Pasti) disk-error marking: read + represent (MF-335).
 *
 * Links the real STX plugin (src/formats/stx/uft_stx_plugin.c). STX stores the
 * WD1772 FDC status register per sector in its 16-byte sector descriptor at
 * offset 0x0E (Jean Louis-Guerin Pasti spec): bit3 (0x08) = data CRC error,
 * bit5 (0x20) = deleted data-address mark. STX is read-only (fuzzy-bit streams
 * cannot be re-synthesised on write), so this covers the read + represent half.
 *
 * This test also guards MF-335, which fixed two spec-offset bugs in the plugin:
 * it read the sector number from 0x08 (ID track C) instead of 0x0A (R) and the
 * FDC status from 0x0C (ID CRC) instead of 0x0E — so both the reported sector
 * IDs and the CRC-error detection were wrong, and the deleted mark was never
 * surfaced at all.
 *
 * The descriptor bytes below are laid out per the corrected (spec) offsets, so
 * the test fails if the plugin regresses to the wrong offsets.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_stx;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SS 256u
#define NSEC 3

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_stx_err_%d.stx", dir, rand() % 100000);
}

static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

static void put_le16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
static void put_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = v >> 24;
}

/* Minimal valid STX: 16B header + one track (16B track header + 3 sector
   descriptors + 3 sector data blocks). FDC status at descriptor 0x0E. */
static int build_stx(const char *path) {
    uint32_t trk_size = 16 + NSEC * 16 + NSEC * SS;   /* 832 */
    uint32_t total = 16 + trk_size;
    uint8_t *buf = calloc(1, total);
    if (!buf) return 0;

    /* file header */
    buf[0] = 'R'; buf[1] = 'S'; buf[2] = 'Y'; buf[3] = '\0';
    put_le16(buf + 4, 3);                 /* version */
    put_le16(buf + 10, 1);                /* track count = 1 */

    uint8_t *trk = buf + 16;
    put_le32(trk + 0, trk_size);          /* track record size */
    put_le32(trk + 4, 0);                 /* fuzzy count */
    put_le16(trk + 8, NSEC);              /* sector count */
    put_le16(trk + 10, 0);                /* track flags */

    uint8_t *descs = trk + 16;
    uint8_t *data  = descs + NSEC * 16;

    const uint8_t rnum[NSEC] = { 1, 2, 3 };
    const uint8_t fdc[NSEC]  = { 0x00, 0x20 /*deleted*/, 0x08 /*CRC err*/ };
    const uint8_t tag[NSEC]  = { 0xA1, 0xB2, 0xC3 };

    for (int s = 0; s < NSEC; s++) {
        uint8_t *d = descs + s * 16;
        put_le32(d + 0x00, (uint32_t)(s * SS));  /* data offset */
        d[0x08] = 0;                             /* ID track C */
        d[0x09] = 0;                             /* ID head H */
        d[0x0A] = rnum[s];                       /* ID sector R */
        d[0x0B] = 1;                             /* ID size N -> 256 */
        d[0x0E] = fdc[s];                        /* FDC status */
        uint8_t *sd = data + s * SS;
        memset(sd, 0x10, SS);
        sd[0] = tag[s];
    }

    FILE *f = fopen(path, "wb");
    if (!f) { free(buf); return 0; }
    int ok = fwrite(buf, 1, total, f) == total;
    fclose(f);
    free(buf);
    return ok;
}

static const uft_sector_t *find_by_tag(const uft_track_t *t, uint8_t tag) {
    for (size_t i = 0; i < t->sector_count; i++)
        if (t->sectors[i].data && t->sectors[i].data[0] == tag)
            return &t->sectors[i];
    return NULL;
}

TEST(read_surfaces_fdc_error_marks) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_stx(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_stx.open(&disk, path, true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_stx.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == NSEC);

    const uft_sector_t *n = find_by_tag(&t, 0xA1);
    const uft_sector_t *d = find_by_tag(&t, 0xB2);
    const uft_sector_t *e = find_by_tag(&t, 0xC3);
    ASSERT(n && d && e);
    ASSERT(n->crc_ok == true);
    ASSERT(n->deleted == false);
    ASSERT(d->deleted == true);          /* FDC bit5 -> deleted (was dropped) */
    ASSERT(e->crc_ok == false);          /* FDC bit3 -> CRC error */
    ASSERT(e->crc_valid == false);
    ASSERT(e->data_crc_ok == false);

    free_track_sectors(&t);
    if (uft_format_plugin_stx.close) uft_format_plugin_stx.close(&disk);
    remove(path);
}

/* The sector IDs must come from R (0x0A), not the track byte (0x08): all three
   sectors share track C=0, so the old 0x08 read collapsed every id to 0. */
TEST(sector_ids_come_from_r_field) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_stx(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_stx.open(&disk, path, true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_stx.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == NSEC);

    /* the three R values (1,2,3) must produce three distinct sector ids */
    int id0 = t.sectors[0].id.sector;
    int id1 = t.sectors[1].id.sector;
    int id2 = t.sectors[2].id.sector;
    ASSERT(id0 != id1 && id1 != id2 && id0 != id2);

    free_track_sectors(&t);
    if (uft_format_plugin_stx.close) uft_format_plugin_stx.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== STX (Pasti) disk-error marking read/represent ===\n");
    RUN(read_surfaces_fdc_error_marks);
    RUN(sector_ids_come_from_r_field);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
