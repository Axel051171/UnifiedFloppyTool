/**
 * @file test_corpus_d71.c
 * @brief T1b cross-tool corpus test: D71 produced by VICE c1541 (MF-376).
 *
 * tests/corpus_free/vice_c1541_70trk.d71 was created by VICE 3.10 c1541
 * (`-format "uftcorpus,42" d71 ... -write marker`). Ground truth pinned by
 * independent python inspection BEFORE any UFT run: size 349696; BAM (track
 * 18, sector 0) begins 0x12 0x01 0x41 0x80 — the 0x80 at offset 3 is the
 * 1571 DOUBLE-SIDED flag (single-sided D64 has 0x00 there); disk name
 * "UFTCORPUS" (PETSCII, 0xA0-padded); directory entry "UFT MARKER" type 0x82.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_d71;

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static const char *img_path(void) {
    static char p[512];
    snprintf(p, sizeof(p), "%s/vice_c1541_70trk.d71", UFT_CORPUS_DIR);
    return p;
}
static void free_ts(uft_track_t *t) {
    for (size_t i = 0; i < t->sector_count; i++) free(t->sectors[i].data);
    free(t->sectors); t->sectors = NULL; t->sector_count = 0;
}
static bool track_contains(const uft_track_t *t, const uint8_t *pat, size_t n) {
    for (size_t s = 0; s < t->sector_count; s++) {
        const uft_sector_t *sec = &t->sectors[s];
        if (!sec->data || sec->data_len < n) continue;
        for (size_t i = 0; i + n <= sec->data_len; i++)
            if (memcmp(sec->data + i, pat, n) == 0) return true;
    }
    return false;
}

TEST(open_and_geometry) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_d71.open(&disk, img_path(), true) == UFT_OK);
    ASSERT(disk.geometry.heads == 2);            /* 1571 double-sided */
    ASSERT(disk.geometry.sector_size == 256);
    uft_format_plugin_d71.close(&disk);
}

TEST(bam_doubleside_flag_and_dir) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_d71.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_d71.read_track(&disk, 17, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 19);
    /* BAM: dir ptr 18/1, DOS 'A', double-sided flag 0x80 (c1541-written) */
    const uint8_t bam_sig[] = { 0x12, 0x01, 0x41, 0x80 };
    ASSERT(track_contains(&t, bam_sig, sizeof(bam_sig)));
    const uint8_t diskname[] = { 'U','F','T','C','O','R','P','U','S', 0xA0 };
    ASSERT(track_contains(&t, diskname, sizeof(diskname)));
    const uint8_t entry[] = { 'U','F','T',' ','M','A','R','K','E','R', 0xA0 };
    ASSERT(track_contains(&t, entry, sizeof(entry)));
    free_ts(&t);
    uft_format_plugin_d71.close(&disk);
}

TEST(second_side_readable) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_d71.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_d71.read_track(&disk, 0, 1, &t) == UFT_OK);
    ASSERT(t.sector_count == 21);                /* zone 1 on side 2 as well */
    ASSERT(t.sectors[0].data_len == 256);
    free_ts(&t);
    uft_format_plugin_d71.close(&disk);
}

int main(void) {
    printf("=== T1b corpus: D71 by VICE c1541 (MF-376) ===\n");
    RUN(open_and_geometry);
    RUN(bam_doubleside_flag_and_dir);
    RUN(second_side_readable);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
