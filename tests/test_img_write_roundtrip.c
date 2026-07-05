/**
 * @file test_img_write_roundtrip.c
 * @brief IBM PC IMG 1.44M write_track -> read_track round-trip (Phase-6, MF-342).
 *
 * Links the real plugin. Builds a synthetic image, reads a track, modifies a
 * sector, writes it back, re-reads and asserts the change persisted (write is
 * not a silent no-op) and a neighbour sector stayed untouched.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_img;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name();                         if (_last_fail == _fail) { printf("OK\n"); _pass++; }                         _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define IMG_SIZE 1474560u
#define SS 512u
#define SPT 18

static void get_temp_path(char *path, size_t n) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, n, "%s/uft_img_rt_%d.img", dir, rand() % 100000);
}
static void free_ts(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors); tr->sectors = NULL; tr->sector_count = 0;
}
static int build_img(const char *path) {
    uint8_t *d = calloc(1, IMG_SIZE);
    if (!d) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) { free(d); return 0; }
    int ok = fwrite(d, 1, IMG_SIZE, f) == IMG_SIZE;
    fclose(f); free(d); return ok;
}

TEST(write_persists_to_read) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_img(path));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = false;
    ASSERT(uft_format_plugin_img.open(&disk, path, false) == UFT_OK);
    ASSERT(disk.geometry.sector_size == SS);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_img.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == SPT);
    ASSERT(t.sectors[3].data && t.sectors[3].data_len == SS);
    t.sectors[3].data[0] = 0xC7; t.sectors[3].data[SS-1] = 0x5A;
    ASSERT(uft_format_plugin_img.write_track(&disk, 0, 0, &t) == UFT_OK);
    free_ts(&t);
    uft_track_t t2; memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_img.read_track(&disk, 0, 0, &t2) == UFT_OK);
    ASSERT(t2.sectors[3].data[0] == 0xC7);
    ASSERT(t2.sectors[3].data[SS-1] == 0x5A);
    ASSERT(t2.sectors[2].data[0] == 0x00);
    free_ts(&t2);
    if (uft_format_plugin_img.close) uft_format_plugin_img.close(&disk);
    remove(path);
}
int main(void) {
    printf("=== IBM PC IMG 1.44M write round-trip (Phase-6) ===\n");
    RUN(write_persists_to_read);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
