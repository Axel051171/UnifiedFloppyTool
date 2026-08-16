/**
 * @file test_corpus_adf.c
 * @brief T1b cross-tool corpus test: ADF produced by amitools xdftool (MF-365).
 *
 * tests/corpus_free/xdftool_dd_ofs.adf was created by amitools xdftool
 * (`create + format "UFTCORPUS" ofs + write marker.txt`) — a canonical
 * third-party Amiga implementation, NOT UFT's own writer (tier T1b,
 * docs/VERIFICATION_PLAN.md).
 *
 * Ground truth pinned independently of UFT: bootblock begins "DOS\0" (OFS);
 * root block (block 880 = cyl 40, head 0) has primary type 0x00000002 and the
 * BCPL volume name "UFTCORPUS"; geometry 80x2x11x512 (901120 bytes).
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_adf;

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
    snprintf(p, sizeof(p), "%s/xdftool_dd_ofs.adf", UFT_CORPUS_DIR);
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
    ASSERT(uft_format_plugin_adf.open(&disk, img_path(), true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 80);
    ASSERT(disk.geometry.heads == 2);
    ASSERT(disk.geometry.sectors == 11);
    ASSERT(disk.geometry.sector_size == 512);
    uft_format_plugin_adf.close(&disk);
}

TEST(bootblock_is_ofs_dos) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_adf.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_adf.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 11);
    /* bootblock: 'D','O','S',0x00 written by xdftool at block 0 */
    ASSERT(t.sectors[0].data_len == 512);
    bool found = false;
    for (size_t s = 0; s < t.sector_count && !found; s++)
        found = (t.sectors[s].data[0] == 'D' && t.sectors[s].data[1] == 'O' &&
                 t.sectors[s].data[2] == 'S' && t.sectors[s].data[3] == 0x00);
    ASSERT(found);
    free_ts(&t);
    uft_format_plugin_adf.close(&disk);
}

TEST(rootblock_volume_name) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_adf.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    /* root block 880 -> track 80 -> cylinder 40, head 0 */
    ASSERT(uft_format_plugin_adf.read_track(&disk, 40, 0, &t) == UFT_OK);
    /* BCPL name: length byte 9 followed by "UFTCORPUS" */
    const uint8_t volname[] = { 9, 'U','F','T','C','O','R','P','U','S' };
    ASSERT(track_contains(&t, volname, sizeof(volname)));
    free_ts(&t);
    uft_format_plugin_adf.close(&disk);
}

int main(void) {
    printf("=== T1b corpus: ADF by amitools xdftool (MF-365) ===\n");
    RUN(open_and_geometry);
    RUN(bootblock_is_ofs_dos);
    RUN(rootblock_volume_name);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
