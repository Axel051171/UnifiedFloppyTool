/**
 * @file test_corpus_scp.c
 * @brief T1b cross-tool corpus test: SCP flux produced by greaseweazle (MF-376).
 *
 * tests/corpus/gw_amigados.scp (LOCAL-ONLY, 32 MB — too large to track; fully
 * reproducible via the manifest command) was created by greaseweazle 1.23
 * (`gw convert --format=amiga.amigados <xdftool-ADF> out.scp`). Ground truth
 * pinned by independent python inspection BEFORE any UFT run: magic "SCP",
 * disk type 0x80, 2 revolutions, tracks 0..159, flags 0x23. SKIPS (exit 77)
 * when the local image is absent (e.g. CI).
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_scp;

#ifndef UFT_CORPUS_RESTRICTED_DIR
#error "UFT_CORPUS_RESTRICTED_DIR must be defined by the build"
#endif

#define SKIP_EXIT 77

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static const char *img_path(void) {
    static char p[512];
    snprintf(p, sizeof(p), "%s/gw_amigados.scp", UFT_CORPUS_RESTRICTED_DIR);
    return p;
}

TEST(open_ok) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_scp.open(&disk, img_path(), true) == UFT_OK);
    uft_format_plugin_scp.close(&disk);
}

TEST(track0_flux_and_rpm) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_scp.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_scp.read_track(&disk, 0, 0, &t) == UFT_OK);
    /* Amiga DD @300rpm: tens of thousands of transitions per revolution */
    ASSERT(t.flux_count > 10000);
    ASSERT(t.metrics.rpm > 250.0 && t.metrics.rpm < 350.0);
    free(t.flux);
    for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
    free(t.sectors);
    uft_format_plugin_scp.close(&disk);
}

TEST(last_track_readable) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_scp.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    /* SCP track index 159 = cyl 79, head 1 */
    ASSERT(uft_format_plugin_scp.read_track(&disk, 79, 1, &t) == UFT_OK);
    ASSERT(t.flux_count > 10000);
    free(t.flux);
    for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
    free(t.sectors);
    uft_format_plugin_scp.close(&disk);
}

int main(void) {
    FILE *f = fopen(img_path(), "rb");
    if (!f) {
        printf("SKIP: local corpus image absent (%s)\n"
               "      regenerate per tests/corpus_manifest/manifest.json\n",
               img_path());
        return SKIP_EXIT;
    }
    fclose(f);
    printf("=== T1b corpus: SCP flux by greaseweazle (MF-376) ===\n");
    RUN(open_ok);
    RUN(track0_flux_and_rpm);
    RUN(last_track_readable);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
