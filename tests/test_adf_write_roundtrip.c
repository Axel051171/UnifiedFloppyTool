/**
 * @file test_adf_write_roundtrip.c
 * @brief ADF plugin write_track -> read_track round-trip (Phase-6, MF-341).
 *
 * Links the real ADF plugin (src/formats/adf/uft_adf_plugin.c). Amiga ADF is a
 * headerless raw sector dump (DD: 80×2×11×512). This proves a modified sector
 * persists through a read -> modify -> write -> read cycle, confirming the
 * data_len write path (MF-321 root fix) on the Amiga family and that ADF write
 * is not a silent no-op.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_adf;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define ADF_DD_SIZE 901120u
#define SS 512u
#define SPT 11

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_adf_rt_%d.adf", dir, rand() % 100000);
}

static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

static int build_adf(const char *path) {
    uint8_t *d = calloc(1, ADF_DD_SIZE);
    if (!d) return 0;
    d[0] = 'D'; d[1] = 'O'; d[2] = 'S'; d[3] = 0;   /* bootblock magic */
    FILE *f = fopen(path, "wb");
    if (!f) { free(d); return 0; }
    int ok = fwrite(d, 1, ADF_DD_SIZE, f) == ADF_DD_SIZE;
    fclose(f);
    free(d);
    return ok;
}

TEST(write_persists_to_read) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_adf(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_adf.open(&disk, path, false) == UFT_OK);
    ASSERT(disk.geometry.sectors == SPT);
    ASSERT(disk.geometry.sector_size == SS);

    /* read cyl 1 (past the bootblock), modify sector 3, write back */
    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_adf.read_track(&disk, 1, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == SPT);
    ASSERT(t.sectors[3].data != NULL && t.sectors[3].data_len == SS);
    t.sectors[3].data[0] = 0xC7;
    t.sectors[3].data[SS - 1] = 0x5A;
    ASSERT(uft_format_plugin_adf.write_track(&disk, 1, 0, &t) == UFT_OK);
    free_track_sectors(&t);

    uft_track_t t2;
    memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_adf.read_track(&disk, 1, 0, &t2) == UFT_OK);
    ASSERT(t2.sectors[3].data[0] == 0xC7);          /* not a silent no-op */
    ASSERT(t2.sectors[3].data[SS - 1] == 0x5A);
    /* an untouched sector stays zero */
    ASSERT(t2.sectors[0].data[0] == 0x00);
    free_track_sectors(&t2);

    if (uft_format_plugin_adf.close) uft_format_plugin_adf.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== ADF write_track round-trip (Phase-6) ===\n");
    RUN(write_persists_to_read);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
