/**
 * @file test_corpus_d64.c
 * @brief T1b cross-tool corpus test: D64 produced by VICE c1541 (MF-365).
 *
 * The image tests/corpus_free/vice_c1541_35trk.d64 was created by VICE 3.10
 * c1541 (`-format "uftcorpus,42" d64 ... -write marker "uft marker"`) — a
 * canonical third-party implementation, NOT UFT's own writer. Reading it
 * correctly is real-world ground truth the synthetic round-trip tests cannot
 * provide (see docs/VERIFICATION_PLAN.md, tier T1b).
 *
 * Ground-truth bytes pinned independently of UFT (python inspection of the
 * file): BAM (track 18, sector 0) begins 0x12 0x01 0x41 and carries the disk
 * name "UFTCORPUS" (PETSCII, 0xA0-padded); the directory (track 18) contains
 * the PETSCII entry "UFT MARKER"; track 1 has 21 sectors of 256 bytes.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_d64;

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
    snprintf(p, sizeof(p), "%s/vice_c1541_35trk.d64", UFT_CORPUS_DIR);
    return p;
}

static void free_ts(uft_track_t *t) {
    for (size_t i = 0; i < t->sector_count; i++) free(t->sectors[i].data);
    free(t->sectors); t->sectors = NULL; t->sector_count = 0;
}

/* Search every sector of a track for a byte sequence. */
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
    ASSERT(uft_format_plugin_d64.open(&disk, img_path(), true) == UFT_OK);
    ASSERT(disk.geometry.cylinders >= 35);
    ASSERT(disk.geometry.sector_size == 256);
    uft_format_plugin_d64.close(&disk);
}

TEST(track1_has_21_sectors) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_d64.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_d64.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 21);            /* zone 1: 21 sectors/track */
    ASSERT(t.sectors[0].data_len == 256);
    free_ts(&t);
    uft_format_plugin_d64.close(&disk);
}

TEST(bam_and_directory_content) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_d64.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    /* directory track 18 = cylinder index 17 */
    ASSERT(uft_format_plugin_d64.read_track(&disk, 17, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 19);            /* zone 2: 19 sectors/track */
    /* BAM signature written by c1541: dir t/s pointer 18/1 + DOS version 'A' */
    const uint8_t bam_sig[] = { 0x12, 0x01, 0x41, 0x00 };
    ASSERT(track_contains(&t, bam_sig, sizeof(bam_sig)));
    /* disk name (PETSCII, 0xA0 padded) */
    const uint8_t diskname[] = { 'U','F','T','C','O','R','P','U','S', 0xA0 };
    ASSERT(track_contains(&t, diskname, sizeof(diskname)));
    /* directory entry "UFT MARKER" written by c1541 */
    const uint8_t entry[] = { 'U','F','T',' ','M','A','R','K','E','R', 0xA0 };
    ASSERT(track_contains(&t, entry, sizeof(entry)));
    free_ts(&t);
    uft_format_plugin_d64.close(&disk);
}

int main(void) {
    printf("=== T1b corpus: D64 by VICE c1541 (MF-365) ===\n");
    RUN(open_and_geometry);
    RUN(track1_has_21_sectors);
    RUN(bam_and_directory_content);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
