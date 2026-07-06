/**
 * @file test_lisa_twiggy_plugin.c
 * @brief Apple Lisa Twiggy plugin: zoned geometry + round-trip (Phase-3, MF-349).
 *
 * Links the real Twiggy plugin (src/formats/lisa/uft_lisa_twiggy.c). Verifies
 * the ZCAV zone table (46 tracks/side, sectors/track 22..15 by zone, 1702 total
 * = 871424 bytes) is read correctly per track/side, and that a modified sector
 * round-trips through the decoded-sector image.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_lisa_twiggy;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SS 512u
#define TWIGGY_SIZE 871424u

static void get_temp_path(char *path, size_t n) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, n, "%s/uft_twiggy_%d.twig", dir, rand() % 100000);
}
static void free_ts(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors); tr->sectors = NULL; tr->sector_count = 0;
}
static int build_img(const char *path) {
    uint8_t *d = calloc(1, TWIGGY_SIZE);
    if (!d) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) { free(d); return 0; }
    int ok = fwrite(d, 1, TWIGGY_SIZE, f) == TWIGGY_SIZE;
    fclose(f); free(d); return ok;
}

/* expected sectors per track for the verified zone table */
static int expect_spt(int cyl) {
    if (cyl <= 3) return 22;
    if (cyl <= 10) return 21;
    if (cyl <= 16) return 20;
    if (cyl <= 22) return 19;
    if (cyl <= 28) return 18;
    if (cyl <= 34) return 17;
    if (cyl <= 41) return 16;
    return 15;
}

TEST(probe_and_geometry) {
    int c = 0;
    ASSERT(uft_format_plugin_lisa_twiggy.probe(NULL, 0, TWIGGY_SIZE, &c) == true);
    ASSERT(c > 0);
    ASSERT(uft_format_plugin_lisa_twiggy.probe(NULL, 0, 143360, &c) == false);

    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_img(path));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = false;
    ASSERT(uft_format_plugin_lisa_twiggy.open(&disk, path, false) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 46 && disk.geometry.heads == 2);
    ASSERT(disk.geometry.sector_size == SS);
    ASSERT(disk.geometry.total_sectors == 1702);   /* 851/side * 2 */
    if (uft_format_plugin_lisa_twiggy.close) uft_format_plugin_lisa_twiggy.close(&disk);
    remove(path);
}

TEST(zoned_sector_counts) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_img(path));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_lisa_twiggy.open(&disk, path, true) == UFT_OK);
    const int probe_cyls[] = { 0, 3, 4, 16, 22, 34, 41, 45 };
    for (size_t k = 0; k < sizeof(probe_cyls)/sizeof(probe_cyls[0]); k++) {
        int cyl = probe_cyls[k];
        uft_track_t t; memset(&t, 0, sizeof(t));
        ASSERT(uft_format_plugin_lisa_twiggy.read_track(&disk, cyl, 0, &t) == UFT_OK);
        ASSERT((int)t.sector_count == expect_spt(cyl));
        free_ts(&t);
    }
    if (uft_format_plugin_lisa_twiggy.close) uft_format_plugin_lisa_twiggy.close(&disk);
    remove(path);
}

TEST(write_roundtrip_both_sides) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_img(path));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = false;
    ASSERT(uft_format_plugin_lisa_twiggy.open(&disk, path, false) == UFT_OK);
    /* modify a sector on cyl 20 head 1 (a mid zone, 19 sectors) */
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_lisa_twiggy.read_track(&disk, 20, 1, &t) == UFT_OK);
    ASSERT(t.sector_count == 19);
    t.sectors[7].data[0] = 0xC7; t.sectors[7].data[SS-1] = 0x5A;
    ASSERT(uft_format_plugin_lisa_twiggy.write_track(&disk, 20, 1, &t) == UFT_OK);
    free_ts(&t);
    uft_track_t t2; memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_lisa_twiggy.read_track(&disk, 20, 1, &t2) == UFT_OK);
    ASSERT(t2.sectors[7].data[0] == 0xC7);
    ASSERT(t2.sectors[7].data[SS-1] == 0x5A);
    free_ts(&t2);
    /* an adjacent track (cyl 20 head 0) must be unaffected by the head-1 write */
    uft_track_t t3; memset(&t3, 0, sizeof(t3));
    ASSERT(uft_format_plugin_lisa_twiggy.read_track(&disk, 20, 0, &t3) == UFT_OK);
    ASSERT(t3.sectors[7].data[0] == 0x00);
    free_ts(&t3);
    if (uft_format_plugin_lisa_twiggy.close) uft_format_plugin_lisa_twiggy.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== Apple Lisa Twiggy plugin (Phase-3 new format) ===\n");
    RUN(probe_and_geometry);
    RUN(zoned_sector_counts);
    RUN(write_roundtrip_both_sides);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
