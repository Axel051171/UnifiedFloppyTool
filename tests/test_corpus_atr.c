/**
 * @file test_corpus_atr.c
 * @brief T1b cross-tool corpus test: ATR from the atrcopy project (MF-365).
 *
 * tests/corpus_free/atrcopy_dos2sd.atr is the pristine `dos2sd.atr` template
 * shipped by the atrcopy 10.1 project ("Atari 8-bit DOS 2 single density
 * (90K), empty VTOC") — a canonical third-party artifact, byte-identical to
 * the published pip package (provable provenance), NOT produced by UFT
 * (tier T1b, docs/VERIFICATION_PLAN.md).
 *
 * Ground truth pinned independently of UFT: 16-byte ATR header magic
 * 0x96 0x02, sector size 128, 720 sectors (92176 bytes total); DOS 2 VTOC at
 * sector 360 (1-based) begins 0x02 0xC3 0x02 0xC3 0x02 (version 2, 707 total,
 * 707 free). Sector 360 -> 0-based 359 -> cylinder 19, sector index 17 at
 * 18 sectors/track.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_atr;

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
    snprintf(p, sizeof(p), "%s/atrcopy_dos2sd.atr", UFT_CORPUS_DIR);
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
    ASSERT(uft_format_plugin_atr.open(&disk, img_path(), true) == UFT_OK);
    ASSERT(disk.geometry.sector_size == 128);       /* single density */
    ASSERT(disk.geometry.cylinders * disk.geometry.heads *
           disk.geometry.sectors == 720);           /* 90K SD */
    uft_format_plugin_atr.close(&disk);
}

TEST(vtoc_dos2_signature) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_atr.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    /* VTOC sector 360 (1-based) -> cylinder 19 at 18 spt */
    ASSERT(uft_format_plugin_atr.read_track(&disk, 19, 0, &t) == UFT_OK);
    ASSERT(t.sector_count > 0);
    /* DOS 2 VTOC: version 2, total 0x02C3, free 0x02C3 (empty disk) */
    const uint8_t vtoc[] = { 0x02, 0xC3, 0x02, 0xC3, 0x02, 0x00 };
    ASSERT(track_contains(&t, vtoc, sizeof(vtoc)));
    free_ts(&t);
    uft_format_plugin_atr.close(&disk);
}

TEST(first_track_reads) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_atr.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_atr.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 18);
    ASSERT(t.sectors[0].data_len == 128);
    free_ts(&t);
    uft_format_plugin_atr.close(&disk);
}

int main(void) {
    printf("=== T1b corpus: ATR (atrcopy dos2sd template) (MF-365) ===\n");
    RUN(open_and_geometry);
    RUN(vtoc_dos2_signature);
    RUN(first_track_reads);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
