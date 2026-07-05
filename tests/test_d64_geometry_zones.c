/**
 * @file test_d64_geometry_zones.c
 * @brief Phase-0 geometry scan: D64 GCR zoned per-track sector counts.
 *
 * Links the real D64 plugin (src/formats/d64/uft_d64_plugin.c). The uniform
 * uft_geometry_t reports a single `sectors` value (the max zone, 21 for a
 * 1541), which cannot express the GCR speed zones. This test proves the plugin
 * nonetheless reads the CORRECT per-track sector count from its zone table:
 *   tracks  1-17 : 21 sectors   (zone 0)
 *   tracks 18-24 : 19 sectors   (zone 1)
 *   tracks 25-30 : 18 sectors   (zone 2)
 *   tracks 31-35 : 17 sectors   (zone 3)
 * i.e. robust geometry detection despite the uniform summary field. Read
 * correctness is the Phase-0 prerequisite for reliable protection/disk-error
 * classification (a wrong sector count corrupts every downstream mark).
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_d64;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define D64_DATA_35 174848u

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_d64_geo_%d.d64", dir, rand() % 100000);
}

static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

static int build_d64(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    uint8_t *data = malloc(D64_DATA_35);
    if (!data) { fclose(f); return 0; }
    memset(data, 0xAA, D64_DATA_35);
    int ok = fwrite(data, 1, D64_DATA_35, f) == D64_DATA_35;
    free(data);
    fclose(f);
    return ok;
}

/* Expected sectors for a given 0-based cylinder (D64 track = cyl + 1). */
static int expected_sectors(int cyl) {
    int track = cyl + 1;
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;
}

TEST(zoned_per_track_sector_counts) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_d64(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_d64.open(&disk, path, true) == UFT_OK);

    /* one representative cylinder per zone + the zone boundaries */
    const int probe[] = { 0, 16, 17, 23, 24, 29, 30, 34 };
    for (size_t k = 0; k < sizeof(probe) / sizeof(probe[0]); k++) {
        int cyl = probe[k];
        uft_track_t t;
        memset(&t, 0, sizeof(t));
        ASSERT(uft_format_plugin_d64.read_track(&disk, cyl, 0, &t) == UFT_OK);
        ASSERT((int)t.sector_count == expected_sectors(cyl));
        free_track_sectors(&t);
    }

    if (uft_format_plugin_d64.close) uft_format_plugin_d64.close(&disk);
    remove(path);
}

TEST(total_sectors_matches_zone_sum) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_d64(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_d64.open(&disk, path, true) == UFT_OK);
    /* 17*21 + 7*19 + 6*18 + 5*17 = 357 + 133 + 108 + 85 = 683 */
    ASSERT(disk.geometry.total_sectors == 683);
    if (uft_format_plugin_d64.close) uft_format_plugin_d64.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== D64 GCR zoned geometry (Phase-0) ===\n");
    RUN(zoned_per_track_sector_counts);
    RUN(total_sectors_matches_zone_sum);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
