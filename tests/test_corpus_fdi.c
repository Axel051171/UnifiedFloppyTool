/**
 * @file test_corpus_fdi.c
 * @brief T1 real-corpus test: ZX Spectrum FDI — Spectrofon #01 diskmag (MF-367).
 *
 * The image tests/corpus/zxart_spectrofon01.fdi is a REAL, historically
 * distributed FDI: issue #01 of the Spectrofon disk magazine (1994) as
 * preserved by the zxart.ee archive (diskmags are made for free distribution).
 * The file is local-only (tests/corpus/ is gitignored); its sha256 + provenance
 * live in tests/corpus_manifest/manifest.json so anyone can re-acquire and
 * verify it. When the file is absent (e.g. CI), this test SKIPS (exit 77).
 *
 * Ground truth pinned by independent python inspection of the file per the
 * Spectrum FDI spec (SAMdisk/WoS), BEFORE running it through UFT:
 *   header: "FDI", wp=0, cyls=83 (non-standard 83-track disk!), heads=2,
 *           data offset 0x4D24, extra=0
 *   track 0: 16 sectors, N=1 (256 bytes), first sector R=1 flags=0x02 (CRC ok)
 *   TR-DOS volume sector R=9: disk-type byte 0x16 at offset 227 (80T DS),
 *           TR-DOS signature 0x10 at offset 231
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_fdi;

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
    snprintf(p, sizeof(p), "%s/zxart_spectrofon01.fdi", UFT_CORPUS_RESTRICTED_DIR);
    return p;
}

static void free_ts(uft_track_t *t) {
    for (size_t i = 0; i < t->sector_count; i++) free(t->sectors[i].data);
    free(t->sectors); t->sectors = NULL; t->sector_count = 0;
}

TEST(open_and_geometry) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_fdi.open(&disk, img_path(), true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 83);   /* real-world non-standard size */
    ASSERT(disk.geometry.heads == 2);
    ASSERT(disk.geometry.sector_size == 256);
    uft_format_plugin_fdi.close(&disk);
}

TEST(track0_trdos_structure) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_fdi.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_fdi.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 16);            /* TR-DOS: 16 x 256 */
    bool have_r1_crc_ok = false, have_r9_volume = false;
    for (size_t s = 0; s < t.sector_count; s++) {
        const uft_sector_t *sec = &t.sectors[s];
        ASSERT(sec->data_len == 256);
        if (sec->id.sector == 1 && sec->crc_ok) have_r1_crc_ok = true;
        if (sec->id.sector == 9 && sec->data &&
            sec->data[227] == 0x16 && sec->data[231] == 0x10)
            have_r9_volume = true;           /* TR-DOS volume info sector */
    }
    ASSERT(have_r1_crc_ok);                  /* flags bit(N&3) honoured */
    ASSERT(have_r9_volume);                  /* data located via sector offsets */
    free_ts(&t);
    uft_format_plugin_fdi.close(&disk);
}

TEST(partially_formatted_last_cyl) {
    /* Pinned ground truth: tracks 0..163 have 16 sectors, but the final
     * cylinder is PARTIALLY formatted — track 164 (cyl 82, head 0) has 13
     * sectors and track 165 (cyl 82, head 1) has 15. A real-world
     * irregularity no synthetic test would produce; the reader must follow
     * the per-track headers instead of assuming a uniform geometry. */
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_fdi.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_fdi.read_track(&disk, 82, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 13);
    free_ts(&t);
    uft_track_t t2; memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_fdi.read_track(&disk, 82, 1, &t2) == UFT_OK);
    ASSERT(t2.sector_count == 15);
    free_ts(&t2);
    uft_format_plugin_fdi.close(&disk);
}

int main(void) {
    FILE *f = fopen(img_path(), "rb");
    if (!f) {
        printf("SKIP: restricted corpus image absent (%s)\n"
               "      acquire per tests/corpus_manifest/manifest.json\n",
               img_path());
        return SKIP_EXIT;
    }
    fclose(f);
    printf("=== T1 corpus: ZX FDI Spectrofon #01 (MF-367) ===\n");
    RUN(open_and_geometry);
    RUN(track0_trdos_structure);
    RUN(partially_formatted_last_cyl);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
