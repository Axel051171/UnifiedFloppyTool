/**
 * @file test_atr_512.c
 * @brief ATR with 512-byte sectors (SpartaDOS X) is read, not clamped (MF-340).
 *
 * Links the real ATR plugin (src/formats/atr/uft_atr.c). ATR sector size can be
 * 128 (SD/ED), 256 (DD) or 512 (SpartaDOS X / large ATRs); the first 3 boot
 * sectors are always 128 B. The plugin used to clamp any size != 128/256 down
 * to 128, misreading every 512-byte ATR. This test builds a 512-byte-sector
 * ATR (3 boot sectors of 128 B + data sectors of 512 B) and asserts the plugin
 * reports sector_size 512 and reads a data sector from the correct offset.
 *
 * Phase-4 geometry-variant gap (found from the Phase-0 geometry audit).
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

#define BOOT_N  3
#define BOOT_SZ 128
#define DATA_N  5
#define DATA_SZ 512

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_atr512_%d.atr", dir, rand() % 100000);
}

static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

/* Build a 512-byte-sector ATR: 16-byte header + 3x128 boot + 5x512 data.
   The first data byte of data-sector 4 (the first 512-B sector) is a tag. */
static int build_atr512(const char *path, uint8_t data4_tag) {
    uint32_t disk_bytes = BOOT_N * BOOT_SZ + DATA_N * DATA_SZ;   /* 2944 */
    uint32_t paragraphs = disk_bytes / 16;                       /* 184 */
    uint8_t header[16];
    memset(header, 0, sizeof(header));
    header[0] = 0x96; header[1] = 0x02;              /* magic 0x0296 LE */
    header[2] = paragraphs & 0xFF; header[3] = (paragraphs >> 8) & 0xFF;
    header[4] = DATA_SZ & 0xFF; header[5] = (DATA_SZ >> 8) & 0xFF; /* sector size 512 */
    header[6] = (paragraphs >> 16) & 0xFF;

    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = fwrite(header, 1, 16, f) == 16;
    uint8_t boot[BOOT_SZ]; memset(boot, 0x11, sizeof(boot));
    for (int i = 0; i < BOOT_N; i++) ok = ok && fwrite(boot, 1, BOOT_SZ, f) == BOOT_SZ;
    for (int i = 0; i < DATA_N; i++) {
        uint8_t d[DATA_SZ];
        memset(d, 0x22, sizeof(d));
        if (i == 0) d[0] = data4_tag;                /* mark sector 4 */
        ok = ok && fwrite(d, 1, DATA_SZ, f) == DATA_SZ;
    }
    fclose(f);
    return ok;
}

TEST(reads_512_byte_sectors) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_atr512(path, 0xC7));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_atr.open(&disk, path, true) == UFT_OK);
    ASSERT(disk.geometry.sector_size == 512);         /* not clamped to 128 */
    ASSERT(disk.geometry.total_sectors == BOOT_N + DATA_N);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_atr.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == BOOT_N + DATA_N);        /* 8 sectors */

    /* boot sectors (index 0..2) are 128 B; data sectors (3..7) are 512 B */
    ASSERT(t.sectors[0].data_len == BOOT_SZ);
    ASSERT(t.sectors[2].data_len == BOOT_SZ);
    ASSERT(t.sectors[3].data_len == DATA_SZ);
    ASSERT(t.sectors[7].data_len == DATA_SZ);
    /* data sector 4 (index 3) read from the correct offset -> tag present */
    ASSERT(t.sectors[3].data[0] == 0xC7);

    free_track_sectors(&t);
    if (uft_format_plugin_atr.close) uft_format_plugin_atr.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== ATR 512-byte-sector read (SpartaDOS X) ===\n");
    RUN(reads_512_byte_sectors);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
