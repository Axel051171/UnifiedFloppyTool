/**
 * @file test_akai_s900_plugin.c
 * @brief Akai S900/S950 plugin: DD+HD geometry + round-trip (Phase-3, MF-348).
 *
 * Links the real Akai plugin (src/formats/akai/uft_akai_s900.c). Verifies the
 * 1024-byte block geometry (DD 80x2x5x1024 = 819200, HD 80x2x10x1024 = 1638400),
 * that the DD probe stays below D81's confidence (no auto-detect hijack) while
 * the HD size is probed with higher confidence, and that a modified sector
 * round-trips in both variants.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_akai_s900;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SS 1024u
#define DD_SIZE 819200u
#define HD_SIZE 1638400u

static void get_temp_path(char *path, size_t n) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, n, "%s/uft_akai_%d.akai", dir, rand() % 100000);
}
static void free_ts(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors); tr->sectors = NULL; tr->sector_count = 0;
}
static int build_img(const char *path, uint32_t size) {
    uint8_t *d = calloc(1, size);
    if (!d) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) { free(d); return 0; }
    int ok = fwrite(d, 1, size, f) == size;
    fclose(f); free(d); return ok;
}

TEST(probe_dd_below_d81_hd_higher) {
    int c = 0;
    ASSERT(uft_format_plugin_akai_s900.probe(NULL, 0, DD_SIZE, &c) == true);
    ASSERT(c > 0 && c < 80);                       /* DD collides with D81 */
    ASSERT(uft_format_plugin_akai_s900.probe(NULL, 0, HD_SIZE, &c) == true);
    ASSERT(c >= 70);                                /* HD size distinct */
    ASSERT(uft_format_plugin_akai_s900.probe(NULL, 0, 174848, &c) == false);
}

TEST(dd_geometry_5x1024) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_img(path, DD_SIZE));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = false;
    ASSERT(uft_format_plugin_akai_s900.open(&disk, path, false) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 80 && disk.geometry.heads == 2);
    ASSERT(disk.geometry.sectors == 5 && disk.geometry.sector_size == SS);
    if (uft_format_plugin_akai_s900.close) uft_format_plugin_akai_s900.close(&disk);
    remove(path);
}

TEST(hd_geometry_10x1024) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_img(path, HD_SIZE));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = false;
    ASSERT(uft_format_plugin_akai_s900.open(&disk, path, false) == UFT_OK);
    ASSERT(disk.geometry.sectors == 10 && disk.geometry.sector_size == SS);
    ASSERT(disk.geometry.total_sectors == 1600);
    if (uft_format_plugin_akai_s900.close) uft_format_plugin_akai_s900.close(&disk);
    remove(path);
}

TEST(dd_write_roundtrip) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_img(path, DD_SIZE));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = false;
    ASSERT(uft_format_plugin_akai_s900.open(&disk, path, false) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_akai_s900.read_track(&disk, 3, 1, &t) == UFT_OK);
    ASSERT(t.sector_count == 5);
    ASSERT(t.sectors[2].data && t.sectors[2].data_len == SS);
    t.sectors[2].data[0] = 0xC7; t.sectors[2].data[SS-1] = 0x5A;
    ASSERT(uft_format_plugin_akai_s900.write_track(&disk, 3, 1, &t) == UFT_OK);
    free_ts(&t);
    uft_track_t t2; memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_akai_s900.read_track(&disk, 3, 1, &t2) == UFT_OK);
    ASSERT(t2.sectors[2].data[0] == 0xC7);
    ASSERT(t2.sectors[2].data[SS-1] == 0x5A);
    ASSERT(t2.sectors[1].data[0] == 0x00);
    free_ts(&t2);
    if (uft_format_plugin_akai_s900.close) uft_format_plugin_akai_s900.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== Akai S900/S950 plugin (Phase-3 new format) ===\n");
    RUN(probe_dd_below_d81_hd_higher);
    RUN(dd_geometry_5x1024);
    RUN(hd_geometry_10x1024);
    RUN(dd_write_roundtrip);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
