/**
 * @file test_atr_write_roundtrip.c
 * @brief ATR plugin write_track -> read_track round-trip (Phase-6, MF-341).
 *
 * Links the real ATR plugin (src/formats/atr/uft_atr.c). Standard SD ATR
 * (720 x 128-byte sectors + 16-byte header). Proves a modified data sector
 * persists through read -> modify -> write -> read (ATR write is not a silent
 * no-op), on top of the read-side coverage in test_atr_512.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_atr;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SS 128u
#define NSEC 720u

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_atr_rt_%d.atr", dir, rand() % 100000);
}

static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

static int build_atr_sd(const char *path) {
    uint32_t disk_bytes = NSEC * SS;                 /* 92160 */
    uint32_t paragraphs = disk_bytes / 16;           /* 5760 */
    uint8_t header[16];
    memset(header, 0, sizeof(header));
    header[0] = 0x96; header[1] = 0x02;              /* magic 0x0296 */
    header[2] = paragraphs & 0xFF; header[3] = (paragraphs >> 8) & 0xFF;
    header[4] = SS & 0xFF; header[5] = (SS >> 8) & 0xFF;  /* sector size 128 */

    uint8_t *data = calloc(1, disk_bytes);
    if (!data) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) { free(data); return 0; }
    int ok = fwrite(header, 1, 16, f) == 16 &&
             fwrite(data, 1, disk_bytes, f) == disk_bytes;
    fclose(f);
    free(data);
    return ok;
}

TEST(write_persists_to_read) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_atr_sd(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_atr.open(&disk, path, false) == UFT_OK);
    ASSERT(disk.geometry.sector_size == SS);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_atr.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 18);                    /* 18 sectors per pseudo-track */
    ASSERT(t.sectors[5].data != NULL && t.sectors[5].data_len == SS);
    t.sectors[5].data[0] = 0xC7;
    t.sectors[5].data[SS - 1] = 0x5A;
    ASSERT(uft_format_plugin_atr.write_track(&disk, 0, 0, &t) == UFT_OK);
    free_track_sectors(&t);

    uft_track_t t2;
    memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_atr.read_track(&disk, 0, 0, &t2) == UFT_OK);
    ASSERT(t2.sectors[5].data[0] == 0xC7);           /* persisted */
    ASSERT(t2.sectors[5].data[SS - 1] == 0x5A);
    ASSERT(t2.sectors[4].data[0] == 0x00);           /* neighbour untouched */
    free_track_sectors(&t2);

    if (uft_format_plugin_atr.close) uft_format_plugin_atr.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== ATR write_track round-trip (Phase-6) ===\n");
    RUN(write_persists_to_read);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
