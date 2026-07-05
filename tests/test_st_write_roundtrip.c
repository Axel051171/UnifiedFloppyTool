/**
 * @file test_st_write_roundtrip.c
 * @brief Atari ST (.st) write_track -> read_track round-trip (Phase-6, MF-341).
 *
 * Links the real ST plugin (src/formats/st/uft_st.c). Atari ST .st is a
 * headerless raw sector dump; 720K = 80x2x9x512. Proves a modified sector
 * persists through read -> modify -> write -> read (ST write is not a silent
 * no-op).
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_st;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SS 512u
#define SPT 9
#define ST_720K 737280u   /* 80 x 2 x 9 x 512 */

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_st_rt_%d.st", dir, rand() % 100000);
}

static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

static int build_st(const char *path) {
    uint8_t *d = calloc(1, ST_720K);
    if (!d) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) { free(d); return 0; }
    int ok = fwrite(d, 1, ST_720K, f) == ST_720K;
    fclose(f);
    free(d);
    return ok;
}

TEST(write_persists_to_read) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_st(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_st.open(&disk, path, false) == UFT_OK);
    ASSERT(disk.geometry.sectors == SPT);
    ASSERT(disk.geometry.heads == 2);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_st.read_track(&disk, 2, 1, &t) == UFT_OK);
    ASSERT(t.sector_count == SPT);
    ASSERT(t.sectors[4].data != NULL && t.sectors[4].data_len == SS);
    t.sectors[4].data[0] = 0xC7;
    t.sectors[4].data[SS - 1] = 0x5A;
    ASSERT(uft_format_plugin_st.write_track(&disk, 2, 1, &t) == UFT_OK);
    free_track_sectors(&t);

    uft_track_t t2;
    memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_st.read_track(&disk, 2, 1, &t2) == UFT_OK);
    ASSERT(t2.sectors[4].data[0] == 0xC7);           /* persisted */
    ASSERT(t2.sectors[4].data[SS - 1] == 0x5A);
    ASSERT(t2.sectors[3].data[0] == 0x00);           /* neighbour untouched */
    free_track_sectors(&t2);

    if (uft_format_plugin_st.close) uft_format_plugin_st.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== Atari ST write_track round-trip (Phase-6) ===\n");
    RUN(write_persists_to_read);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
