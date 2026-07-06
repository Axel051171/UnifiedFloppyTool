/**
 * @file test_d64_42track.c
 * @brief D64 extended 41/42-track variant support (MF-350).
 *
 * Links the real D64 plugin. Verifies the additive 41/42-track extension:
 * a 205312-byte (42-track, no error block) image opens with 42 cylinders and
 * the correct per-zone sector counts (track 42 = 17 sectors, track 1 = 21),
 * a modified sector on the extended track 42 round-trips, and the standard
 * 35-track image still opens as 35 tracks (no regression).
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
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define D64_35   174848u
#define D64_42   205312u   /* 802 sectors * 256, no error block */

static void get_temp_path(char *path, size_t n) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, n, "%s/uft_d64_42_%d.d64", dir, rand() % 100000);
}
static void free_ts(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors); tr->sectors = NULL; tr->sector_count = 0;
}
static int build_d64(const char *path, uint32_t size) {
    uint8_t *d = calloc(1, size);
    if (!d) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) { free(d); return 0; }
    int ok = fwrite(d, 1, size, f) == size;
    fclose(f); free(d); return ok;
}

TEST(probe_accepts_42track) {
    int c = 0;
    ASSERT(uft_format_plugin_d64.probe(NULL, 0, D64_42, &c) == true);
    ASSERT(c > 0);
    /* still rejects a non-D64 size */
    ASSERT(uft_format_plugin_d64.probe(NULL, 0, 123456, &c) == false);
}

TEST(open_42track_geometry) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_d64(path, D64_42));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = false;
    ASSERT(uft_format_plugin_d64.open(&disk, path, false) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 42);
    ASSERT(disk.geometry.total_sectors == 802);
    /* track 1 (cyl 0) = 21 sectors, track 42 (cyl 41) = 17 sectors */
    uft_track_t t0; memset(&t0, 0, sizeof(t0));
    ASSERT(uft_format_plugin_d64.read_track(&disk, 0, 0, &t0) == UFT_OK);
    ASSERT(t0.sector_count == 21);
    free_ts(&t0);
    uft_track_t t42; memset(&t42, 0, sizeof(t42));
    ASSERT(uft_format_plugin_d64.read_track(&disk, 41, 0, &t42) == UFT_OK);
    ASSERT(t42.sector_count == 17);
    free_ts(&t42);
    if (uft_format_plugin_d64.close) uft_format_plugin_d64.close(&disk);
    remove(path);
}

TEST(extended_track_roundtrip) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_d64(path, D64_42));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = false;
    ASSERT(uft_format_plugin_d64.open(&disk, path, false) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_d64.read_track(&disk, 41, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 17);
    t.sectors[5].data[0] = 0xC7; t.sectors[5].data[255] = 0x5A;
    ASSERT(uft_format_plugin_d64.write_track(&disk, 41, 0, &t) == UFT_OK);
    free_ts(&t);
    uft_track_t t2; memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_d64.read_track(&disk, 41, 0, &t2) == UFT_OK);
    ASSERT(t2.sectors[5].data[0] == 0xC7);
    ASSERT(t2.sectors[5].data[255] == 0x5A);
    free_ts(&t2);
    if (uft_format_plugin_d64.close) uft_format_plugin_d64.close(&disk);
    remove(path);
}

TEST(standard_35track_no_regression) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_d64(path, D64_35));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_d64.open(&disk, path, true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 35);
    ASSERT(disk.geometry.total_sectors == 683);
    if (uft_format_plugin_d64.close) uft_format_plugin_d64.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== D64 extended 41/42-track support (Phase-3) ===\n");
    RUN(probe_accepts_42track);
    RUN(open_42track_geometry);
    RUN(extended_track_roundtrip);
    RUN(standard_35track_no_regression);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
