/**
 * @file test_td0_error_marks.c
 * @brief TD0 disk-error marking (read + represent) and the open-scan off-by-one.
 *
 * Links the real Teledisk TD0 plugin (src/formats/td0/uft_td0.c). TD0 encodes
 * per-sector status in the sector-header flag byte: bit0 (0x01) = data CRC
 * error, bit2 (0x04) = deleted-address-mark. TD0 is read-only (writing needs
 * re-compression), so this covers the read + represent half of the disk-error
 * work package.
 *
 * The existing test_td0_plugin.c only exercises the probe (magic bytes) and a
 * data-less header — it never walks a data-bearing track. This test builds a
 * NORMAL (uncompressed) TD0 with one track of three 256-byte data sectors
 * (normal / deleted / CRC-error) and asserts:
 *   - open() reports the correct geometry (cylinders == 1). This is the
 *     regression guard for MF-334: open()'s geometry scan used to skip
 *     `len - 1` bytes of each data record while read_track consumes `len`,
 *     drifting one byte per data sector and mis-scanning multi-sector tracks.
 *     A misaligned scan would not land on the 0xFF end marker with one track.
 *   - read_track() surfaces the deleted / CRC-error flags on the right sectors.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_td0;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SS 256u

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_td0_err_%d.td0", dir, rand() % 100000);
}

static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

/* One raw 256-byte data sector: 6-byte header + [len LE16] + method(0) + data.
   data record length = 1 (method) + 256 = 257. First data byte is a tag. */
static void put_sector(FILE *f, uint8_t sec_num, uint8_t flags, uint8_t tag) {
    uint8_t hdr[6] = { 0, 0, sec_num, 1 /*size code 1 = 256*/, flags, 0 };
    fwrite(hdr, 1, 6, f);
    uint16_t len = 1 + SS;
    uint8_t lb[2] = { (uint8_t)(len & 0xFF), (uint8_t)(len >> 8) };
    fwrite(lb, 1, 2, f);
    fputc(0, f);                                   /* encoding method 0 = raw */
    fputc(tag, f);
    for (unsigned i = 1; i < SS; i++) fputc(0x10, f);
}

static int build_td0(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    /* Signature 'T','D' = normal/RLE Teledisk. This fixture used to write
     * {0x44,0x54} ("DT"), matching the byte-swapped magic constant the plugin
     * carried until MF-389 — the test was green because both sides shared the
     * same mistake. See docs/KNOWN_ISSUES.md FMT-13. */
    uint8_t header[12] = { 0x54, 0x44, 0, 0, 0x00 /*version<0x10 => no comment*/,
                           0, 0, 0, 0, 1 /*sides*/, 0, 0 };
    fwrite(header, 1, 12, f);
    uint8_t trk_hdr[4] = { 3 /*num_sec*/, 0 /*cyl*/, 0 /*head*/, 0 /*crc*/ };
    fwrite(trk_hdr, 1, 4, f);
    put_sector(f, 1, 0x00, 0xA1);                  /* normal  */
    put_sector(f, 2, 0x04, 0xB2);                  /* deleted */
    put_sector(f, 3, 0x01, 0xC3);                  /* CRC err */
    uint8_t end[4] = { 0xFF, 0, 0, 0 };            /* end-of-tracks marker */
    fwrite(end, 1, 4, f);
    fclose(f);
    return 1;
}

static const uft_sector_t *find_by_tag(const uft_track_t *t, uint8_t tag) {
    for (size_t i = 0; i < t->sector_count; i++)
        if (t->sectors[i].data && t->sectors[i].data[0] == tag)
            return &t->sectors[i];
    return NULL;
}

TEST(open_geometry_scan_aligned) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_td0(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_td0.open(&disk, path, true) == UFT_OK);
    /* One track (cyl 0). A drifting scan (old len-1) would not land on the
       0xFF end marker and would mis-count the geometry. */
    ASSERT(disk.geometry.cylinders == 1);

    if (uft_format_plugin_td0.close) uft_format_plugin_td0.close(&disk);
    remove(path);
}

TEST(read_surfaces_error_marks) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_td0(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_td0.open(&disk, path, true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_td0.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 3);

    const uft_sector_t *n = find_by_tag(&t, 0xA1);
    const uft_sector_t *d = find_by_tag(&t, 0xB2);
    const uft_sector_t *e = find_by_tag(&t, 0xC3);
    ASSERT(n && d && e);
    ASSERT(n->crc_ok == true);
    ASSERT(n->deleted == false);
    ASSERT(d->deleted == true);
    ASSERT(e->crc_ok == false);
    ASSERT(e->crc_valid == false);
    ASSERT(e->data_crc_ok == false);

    free_track_sectors(&t);
    if (uft_format_plugin_td0.close) uft_format_plugin_td0.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== TD0 disk-error marking (read/represent) + scan alignment ===\n");
    RUN(open_geometry_scan_aligned);
    RUN(read_surfaces_error_marks);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
