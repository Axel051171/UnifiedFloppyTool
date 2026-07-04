/**
 * @file test_imd_write_roundtrip.c
 * @brief IMD plugin write_track -> read_track round-trip verification.
 *
 * Links the real IMD plugin (src/formats/imd/uft_imd_plugin.c) and proves a
 * written sector is read back: open a synthetic IMD, read a track, change a
 * sector byte, write_track, re-read, and assert the change persisted.
 *
 * This is the safety gate that exposed a real bug: uft_format_add_sector()
 * (used by read_track) populates uft_sector_t.data_size but NOT .data_len,
 * while imd_plugin_write_track() gated its memcpy on `.data_len >= ss` — so
 * every IMD write was a silent no-op for read-produced tracks.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_imd;

/* read_track fills a caller-owned (here stack) track: free the heap sectors
 * it allocated, but NOT the track struct itself (uft_track_free() would
 * free(track), which is wrong for a stack track). */
static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SS 128u

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_imd_wr_%d.imd", dir, rand() % 100000);
}

/* Build a minimal IMD: comment + 1 track (cyl 0, head 0), 2 raw 128B sectors. */
static int build_imd(const char *path, uint8_t s1_first, uint8_t s2_first) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    const char *comment = "IMD 1.18: uft test\r\n";
    fwrite(comment, 1, strlen(comment), f);
    fputc(0x1A, f);                 /* comment terminator */
    /* Track record */
    fputc(5, f);                    /* mode: MFM 250kbps */
    fputc(0, f);                    /* cylinder 0 */
    fputc(0, f);                    /* head 0 (no cyl/head maps) */
    fputc(2, f);                    /* 2 sectors */
    fputc(0, f);                    /* size code 0 -> 128 bytes */
    fputc(1, f); fputc(2, f);       /* sector numbering map: 1, 2 */
    /* sector 1: type 1 (raw normal) + 128 bytes */
    fputc(1, f);
    for (unsigned i = 0; i < SS; i++) fputc((i == 0) ? s1_first : (uint8_t)(0x10 + (i & 0x3F)), f);
    /* sector 2: type 1 (raw normal) + 128 bytes */
    fputc(1, f);
    for (unsigned i = 0; i < SS; i++) fputc((i == 0) ? s2_first : (uint8_t)(0x80 + (i & 0x3F)), f);
    fclose(f);
    return 1;
}

/* ── A. a written sector byte is read back ─────────────────────────── */

TEST(write_track_persists_to_read) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_imd(path, 0x11, 0x22));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_imd.open(&disk, path, false) == UFT_OK);

    /* read the track */
    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_imd.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 2);
    ASSERT(t.sectors[0].data != NULL);
    ASSERT(t.sectors[0].data[0] == 0x11);   /* original sector-1 first byte */

    /* change sector 1's first byte and write it back */
    t.sectors[0].data[0] = 0xAB;
    ASSERT(uft_format_plugin_imd.write_track(&disk, 0, 0, &t) == UFT_OK);
    free_track_sectors(&t);

    /* re-read: the change must have persisted (this failed before the fix) */
    uft_track_t t2;
    memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_imd.read_track(&disk, 0, 0, &t2) == UFT_OK);
    ASSERT(t2.sector_count == 2);
    ASSERT(t2.sectors[0].data[0] == 0xAB);   /* THE POINT: write took effect */
    ASSERT(t2.sectors[1].data[0] == 0x22);   /* sector 2 untouched */
    free_track_sectors(&t2);

    uft_format_plugin_imd.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== IMD write_track round-trip test ===\n");
    RUN(write_track_persists_to_read);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
