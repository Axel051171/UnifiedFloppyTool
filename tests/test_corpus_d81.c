/**
 * @file test_corpus_d81.c
 * @brief T1b cross-tool corpus test: D81 produced by VICE c1541 (MF-376).
 *
 * tests/corpus_free/vice_c1541_80trk.d81 was created by VICE 3.10 c1541
 * (`-format "uftcorpus,42" d81 ... -write marker`). Ground truth pinned by
 * independent python inspection BEFORE any UFT run: size 819200 (80 tracks x
 * 40 x 256 logical); header sector at track 40, sector 0 begins
 * 0x28 0x03 0x44 0x00 (next 40/3, DOS version 'D') with the disk name
 * "UFTCORPUS" (PETSCII, 0xA0-padded) at offset 4; directory entry
 * "UFT MARKER" (type 0x82) in track 40, sector 3.
 *
 * Extra value: D81's 819200 bytes collide with the Korg/Akai sampler sizes —
 * this image proves the d81 plugin reads a REAL 1581 filesystem correctly.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_d81;

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
    snprintf(p, sizeof(p), "%s/vice_c1541_80trk.d81", UFT_CORPUS_DIR);
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
    ASSERT(uft_format_plugin_d81.open(&disk, img_path(), true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 80);
    ASSERT(disk.geometry.sectors == 40);
    ASSERT(disk.geometry.sector_size == 256);
    uft_format_plugin_d81.close(&disk);
}

TEST(header_track40_content) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_d81.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    /* track 40 = cylinder index 39 */
    ASSERT(uft_format_plugin_d81.read_track(&disk, 39, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 40);
    /* header sector: 28 03 44 00 + "UFTCORPUS" (c1541-written) */
    const uint8_t hdr[] = { 0x28, 0x03, 0x44, 0x00,
                            'U','F','T','C','O','R','P','U','S', 0xA0 };
    ASSERT(track_contains(&t, hdr, sizeof(hdr)));
    const uint8_t entry[] = { 'U','F','T',' ','M','A','R','K','E','R', 0xA0 };
    ASSERT(track_contains(&t, entry, sizeof(entry)));
    free_ts(&t);
    uft_format_plugin_d81.close(&disk);
}

TEST(first_and_last_track_read) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_d81.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_d81.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 40);
    free_ts(&t);
    uft_track_t t2; memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_d81.read_track(&disk, 79, 0, &t2) == UFT_OK);
    ASSERT(t2.sector_count == 40);
    free_ts(&t2);
    uft_format_plugin_d81.close(&disk);
}

int main(void) {
    printf("=== T1b corpus: D81 by VICE c1541 (MF-376) ===\n");
    RUN(open_and_geometry);
    RUN(header_track40_content);
    RUN(first_and_last_track_read);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
