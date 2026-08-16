/**
 * @file test_corpus_hfe.c
 * @brief T1b cross-tool corpus test: HFE produced by greaseweazle (MF-376).
 *
 * tests/corpus_free/gw_amigados.hfe was created by greaseweazle 1.23
 * (`gw convert --format=amiga.amigados <xdftool-ADF> out.hfe`) — the canonical
 * flux/bitstream tool encoded a REAL AmigaDOS filesystem into MFM. Ground
 * truth pinned by independent python inspection BEFORE any UFT run: header
 * "HXCPICFE" (HFE v1), 80 tracks, 2 sides, bitrate 253 kbit/s; the
 * bit-reversed track data contains dozens of 0x4489 Amiga MFM sync words
 * (36 in the first track region alone).
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_hfe;

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
    snprintf(p, sizeof(p), "%s/gw_amigados.hfe", UFT_CORPUS_DIR);
    return p;
}

TEST(open_and_header) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_hfe.open(&disk, img_path(), true) == UFT_OK);
    char v[32] = {0};
    if (uft_format_plugin_hfe.read_metadata) {
        ASSERT(uft_format_plugin_hfe.read_metadata(&disk, "version", v, sizeof(v)) == UFT_OK);
        ASSERT(strcmp(v, "HFEv1") == 0);         /* gw writes HXCPICFE */
    }
    uft_format_plugin_hfe.close(&disk);
}

TEST(track0_has_amiga_sync) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_hfe.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_hfe.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.raw_data != NULL && t.raw_size > 1000);
    /* pinned: the MSB-first (bit-reversed) stream carries 0x44 0x89 Amiga
     * sync pairs — prove UFT's bit-reversal produced the logical domain */
    int syncs = 0;
    for (size_t i = 0; i + 1 < t.raw_size; i++)
        if (t.raw_data[i] == 0x44 && t.raw_data[i + 1] == 0x89) syncs++;
    ASSERT(syncs >= 10);
    free(t.raw_data);
    if (t.weak_mask) free(t.weak_mask);
    uft_format_plugin_hfe.close(&disk);
}

TEST(no_weak_regions_in_v1) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_hfe.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_hfe.read_track(&disk, 5, 1, &t) == UFT_OK);
    char w[16] = {0};
    if (uft_format_plugin_hfe.read_metadata) {
        ASSERT(uft_format_plugin_hfe.read_metadata(&disk, "weak_regions", w, sizeof(w)) == UFT_OK);
        ASSERT(strcmp(w, "0") == 0);             /* v1 has no RAND opcodes */
    }
    free(t.raw_data);
    if (t.weak_mask) free(t.weak_mask);
    uft_format_plugin_hfe.close(&disk);
}

int main(void) {
    printf("=== T1b corpus: HFE by greaseweazle (MF-376) ===\n");
    RUN(open_and_header);
    RUN(track0_has_amiga_sync);
    RUN(no_weak_regions_in_v1);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
