/**
 * @file test_d88_error_marks.c
 * @brief D88 disk-error marking: read, represent, preserve on write (MF-336).
 *
 * Links the real NEC PC-88/98 D88 plugin (src/formats/d88/uft_d88.c). D88
 * stores per-sector status in its 16-byte sector header: +07 DDAM flag
 * (0x10 = deleted-data mark) and +08 FDC status code (0xA0 = ID CRC error,
 * 0xB0 = data CRC error). Bytes +09..0D are reserved.
 *
 * This guards MF-336, which fixed a spec-offset bug: the plugin read the
 * status from +0D (a reserved/RPM byte) instead of the DDAM flag at +07 and
 * the FDC status at +08 (verified against pc98.org and MAME d88_dsk), so
 * deleted and CRC-error marks were taken from the wrong byte.
 *
 * The test builds a valid D88 with a normal, a deleted and a CRC-error sector,
 * asserts the plugin surfaces the marks, then writes the track back and reads
 * again — the D88 writer overwrites only sector data and leaves the header
 * (status bytes) intact, so the marks must survive.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_d88;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define D88_HDR 0x2B0u
#define SS 256u
#define NSEC 4

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_d88_err_%d.d88", dir, rand() % 100000);
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

static int build_d88(const char *path) {
    uint32_t track_len = NSEC * (16 + SS);          /* 3 * 272 = 816 */
    uint32_t total = D88_HDR + track_len;
    uint8_t *buf = calloc(1, total);
    if (!buf) return 0;

    buf[0x1B] = 0x00;                                /* media = 2D */
    put_le32(buf + 0x1C, total);                     /* disk size */
    put_le32(buf + 0x20, D88_HDR);                   /* track[0] offset */

    const uint8_t rnum[NSEC] = { 1, 2, 3, 4 };
    const uint8_t ddam[NSEC] = { 0x00, 0x10 /*deleted*/, 0x00, 0x00 };
    const uint8_t fdc[NSEC]  = { 0x00, 0x00, 0xB0 /*data CRC*/, 0xA0 /*ID CRC*/ };
    const uint8_t tag[NSEC]  = { 0xA1, 0xB2, 0xC3, 0xD4 };

    uint8_t *t = buf + D88_HDR;
    for (int s = 0; s < NSEC; s++) {
        uint8_t *h = t + s * (16 + SS);
        h[0] = 0;                    /* C */
        h[1] = 0;                    /* H */
        h[2] = rnum[s];              /* R */
        h[3] = 1;                    /* N -> 256 */
        put_le16(h + 4, NSEC);       /* sectors in track */
        h[6] = 0x00;                 /* density: double */
        h[7] = ddam[s];              /* DDAM flag */
        h[8] = fdc[s];               /* FDC status */
        put_le16(h + 14, SS);        /* data size */
        uint8_t *d = h + 16;
        memset(d, 0x10, SS);
        d[0] = tag[s];
    }

    FILE *f = fopen(path, "wb");
    if (!f) { free(buf); return 0; }
    int ok = fwrite(buf, 1, total, f) == total;
    fclose(f);
    free(buf);
    return ok;
}

static const uft_sector_t *find_by_tag(const uft_track_t *tr, uint8_t tag) {
    for (size_t i = 0; i < tr->sector_count; i++)
        if (tr->sectors[i].data && tr->sectors[i].data[0] == tag)
            return &tr->sectors[i];
    return NULL;
}

static void assert_marks(uft_track_t *tr) {
    ASSERT(tr->sector_count == NSEC);
    const uft_sector_t *n = find_by_tag(tr, 0xA1);
    const uft_sector_t *d = find_by_tag(tr, 0xB2);
    const uft_sector_t *e = find_by_tag(tr, 0xC3);   /* data CRC (0xB0) */
    const uft_sector_t *i = find_by_tag(tr, 0xD4);   /* ID CRC   (0xA0) */
    ASSERT(n && d && e && i);
    /* normal */
    ASSERT(n->crc_ok == true);
    ASSERT(n->id_crc_ok == true);
    ASSERT(n->deleted == false);
    /* deleted */
    ASSERT(d->deleted == true);           /* DDAM +07 (was read from +0D) */
    ASSERT(d->crc_ok == true);
    ASSERT(d->id_crc_ok == true);
    /* data CRC error: data crc bad, ID crc still ok (separated, not collapsed) */
    ASSERT(e->crc_ok == false);           /* FDC status +08 = 0xB0 */
    ASSERT(e->crc_valid == false);
    ASSERT(e->data_crc_ok == false);
    ASSERT(e->id_crc_ok == true);
    /* ID-field CRC error: ID crc bad, DATA crc still ok (header vs data) */
    ASSERT(i->id_crc_ok == false);        /* FDC status +08 = 0xA0 */
    ASSERT(i->crc_ok == true);
}

TEST(read_surfaces_error_marks) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_d88(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_d88.open(&disk, path, false) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_d88.read_track(&disk, 0, 0, &t) == UFT_OK);
    assert_marks(&t);

    free_track_sectors(&t);
    if (uft_format_plugin_d88.close) uft_format_plugin_d88.close(&disk);
    remove(path);
}

TEST(marks_survive_write_read) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_d88(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_d88.open(&disk, path, false) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_d88.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(uft_format_plugin_d88.write_track(&disk, 0, 0, &t) == UFT_OK);
    free_track_sectors(&t);

    uft_track_t t2;
    memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_d88.read_track(&disk, 0, 0, &t2) == UFT_OK);
    assert_marks(&t2);

    free_track_sectors(&t2);
    if (uft_format_plugin_d88.close) uft_format_plugin_d88.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== D88 disk-error marking (read / represent / preserve) ===\n");
    RUN(read_surfaces_error_marks);
    RUN(marks_survive_write_read);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
