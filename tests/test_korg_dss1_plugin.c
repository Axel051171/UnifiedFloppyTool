/**
 * @file test_korg_dss1_plugin.c
 * @brief Korg DSS-1 plugin: geometry + write->read round-trip (Phase-3, MF-347).
 *
 * Links the real Korg DSS-1 plugin (src/formats/korg/uft_korg_dss1.c). Verifies
 * the 80x2x5x1024 = 819200 geometry is read correctly (5 sectors of 1024 per
 * track/side — distinct from the D81 40x256 that shares the same total size),
 * that the probe returns a modest confidence (below D81's) so it does not
 * hijack D81 auto-detection, and that a modified sector round-trips.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_korg_dss1;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define KORG_SIZE 819200u
#define SS 1024u
#define SPT 5

static void get_temp_path(char *path, size_t n) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, n, "%s/uft_korg_%d.dss", dir, rand() % 100000);
}
static void free_ts(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors); tr->sectors = NULL; tr->sector_count = 0;
}
static int build_img(const char *path) {
    uint8_t *d = calloc(1, KORG_SIZE);
    if (!d) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) { free(d); return 0; }
    int ok = fwrite(d, 1, KORG_SIZE, f) == KORG_SIZE;
    fclose(f); free(d); return ok;
}

TEST(probe_confidence_below_d81) {
    /* size matches but confidence must be modest so D81 wins auto-detect */
    int c = 0;
    ASSERT(uft_format_plugin_korg_dss1.probe(NULL, 0, KORG_SIZE, &c) == true);
    ASSERT(c > 0 && c < 80);
    /* wrong size rejected */
    ASSERT(uft_format_plugin_korg_dss1.probe(NULL, 0, 174848, &c) == false);
}

TEST(geometry_5x1024) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_img(path));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = false;
    ASSERT(uft_format_plugin_korg_dss1.open(&disk, path, false) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 80);
    ASSERT(disk.geometry.heads == 2);
    ASSERT(disk.geometry.sectors == SPT);
    ASSERT(disk.geometry.sector_size == SS);
    ASSERT(disk.geometry.total_sectors == 800);
    if (uft_format_plugin_korg_dss1.close) uft_format_plugin_korg_dss1.close(&disk);
    remove(path);
}

TEST(write_persists_to_read) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_img(path));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = false;
    ASSERT(uft_format_plugin_korg_dss1.open(&disk, path, false) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_korg_dss1.read_track(&disk, 1, 1, &t) == UFT_OK);
    ASSERT(t.sector_count == SPT);
    ASSERT(t.sectors[2].data && t.sectors[2].data_len == SS);
    t.sectors[2].data[0] = 0xC7; t.sectors[2].data[SS-1] = 0x5A;
    ASSERT(uft_format_plugin_korg_dss1.write_track(&disk, 1, 1, &t) == UFT_OK);
    free_ts(&t);
    uft_track_t t2; memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_korg_dss1.read_track(&disk, 1, 1, &t2) == UFT_OK);
    ASSERT(t2.sectors[2].data[0] == 0xC7);
    ASSERT(t2.sectors[2].data[SS-1] == 0x5A);
    ASSERT(t2.sectors[1].data[0] == 0x00);
    free_ts(&t2);
    if (uft_format_plugin_korg_dss1.close) uft_format_plugin_korg_dss1.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== Korg DSS-1 plugin (Phase-3 new format) ===\n");
    RUN(probe_confidence_below_d81);
    RUN(geometry_5x1024);
    RUN(write_persists_to_read);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
