/**
 * @file test_g71_read.c
 * @brief G71 (Commodore 1571 GCR) per-track raw read (MF-355).
 *
 * The G71 plugin read_track used to return UFT_ERR_NOT_IMPLEMENTED (honest
 * stub). It now reads the raw GCR bitstream for each (cyl, head), mirroring the
 * proven file-level reader's offset-table layout: header (12) + 168 half-track
 * LE32 offsets + 168 LE32 speeds + track data, each track = LE16 size + GCR
 * bytes. Whole track lives at even half-track slot (side*42 + cyl)*2. Raw GCR is
 * preserved verbatim ("kein Bit verloren"); sector decode is out of scope.
 *
 * This test builds a synthetic double-sided G71 with a distinct GCR track on
 * each side plus an empty (offset 0) track, then verifies read_track returns the
 * exact bytes on side 0 and side 1, marks the empty track UNFORMATTED, and
 * rejects out-of-range coordinates.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_g71;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define HDR        12
#define HALF       168
#define OFF_TAB    HDR
#define SPD_TAB    (OFF_TAB + HALF * 4)
#define TRK_DATA   (SPD_TAB + HALF * 4)   /* 1356 */
#define SZ_A       16u
#define SZ_B       20u

static void wle16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
static void wle32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}

static void get_temp_path(char *path, size_t n) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, n, "%s/uft_g71_%d.g71", dir, rand() % 100000);
}

/* Build a synthetic G71: side0-track0 = A (0xAA*16), side1-track0 = B (0x55*20),
 * everything else offset 0. Returns the total file size, fills A/B references. */
static size_t build_g71(const char *path, uint8_t a_ref[SZ_A], uint8_t b_ref[SZ_B]) {
    size_t off_a = TRK_DATA;               /* 1356 */
    size_t off_b = off_a + 2 + SZ_A;       /* after track A */
    size_t total = off_b + 2 + SZ_B;
    uint8_t *file = calloc(1, total);
    if (!file) return 0;

    memcpy(file, "GCR-1571", 8);
    file[8] = 0;                            /* version */
    file[9] = 84;                           /* num_tracks */
    wle16(&file[10], 7928);                 /* max track size */

    /* half-track slot for side s, whole track t = (s*42 + t)*2 */
    wle32(&file[OFF_TAB + 0 * 4], (uint32_t)off_a);       /* side0 track0 */
    wle32(&file[OFF_TAB + 84 * 4], (uint32_t)off_b);      /* side1 track0 */

    for (unsigned i = 0; i < SZ_A; i++) a_ref[i] = 0xAA;
    for (unsigned i = 0; i < SZ_B; i++) b_ref[i] = 0x55;

    wle16(&file[off_a], (uint16_t)SZ_A); memcpy(&file[off_a + 2], a_ref, SZ_A);
    wle16(&file[off_b], (uint16_t)SZ_B); memcpy(&file[off_b + 2], b_ref, SZ_B);

    FILE *f = fopen(path, "wb");
    if (!f) { free(file); return 0; }
    size_t w = fwrite(file, 1, total, f);
    fclose(f); free(file);
    return w == total ? total : 0;
}

static int open_disk(const char *path, uft_disk_t *disk) {
    memset(disk, 0, sizeof(*disk));
    disk->read_only = true;
    return uft_format_plugin_g71.open(disk, path, true) == UFT_OK;
}

TEST(side0_track0_raw_matches) {
    char path[300]; get_temp_path(path, sizeof(path));
    uint8_t a[SZ_A], b[SZ_B];
    ASSERT(build_g71(path, a, b));
    uft_disk_t disk; ASSERT(open_disk(path, &disk));
    ASSERT(disk.geometry.cylinders == 42 && disk.geometry.heads == 2);
    uft_track_t t; memset(&t, 0, sizeof(t));
    /* NB: assert on raw bytes (the forensic core), not on duplicated status/
     * encoding enums — UFT_TRACK_* and UFT_ENC_* resolve to different numerics
     * across TUs (pre-existing single-source drift, out of scope for MF-355). */
    ASSERT(uft_format_plugin_g71.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.raw_size == SZ_A);
    ASSERT(t.raw_data && memcmp(t.raw_data, a, SZ_A) == 0);
    free(t.raw_data);
    uft_format_plugin_g71.close(&disk);
    remove(path);
}

TEST(side1_track0_raw_matches) {
    char path[300]; get_temp_path(path, sizeof(path));
    uint8_t a[SZ_A], b[SZ_B];
    ASSERT(build_g71(path, a, b));
    uft_disk_t disk; ASSERT(open_disk(path, &disk));
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_g71.read_track(&disk, 0, 1, &t) == UFT_OK);
    ASSERT(t.status == UFT_TRACK_OK);
    ASSERT(t.raw_size == SZ_B);
    ASSERT(t.raw_data && memcmp(t.raw_data, b, SZ_B) == 0);
    free(t.raw_data);
    uft_format_plugin_g71.close(&disk);
    remove(path);
}

TEST(empty_track_unformatted) {
    char path[300]; get_temp_path(path, sizeof(path));
    uint8_t a[SZ_A], b[SZ_B];
    ASSERT(build_g71(path, a, b));
    uft_disk_t disk; ASSERT(open_disk(path, &disk));
    uft_track_t t; memset(&t, 0, sizeof(t));
    /* Empty track: read succeeds, no raw data (offset 0 in table). The
     * UFT_TRACK_UNFORMATTED assert works again since MF-371 removed the
     * value-shadowing compat macros in uft_track.h (former cross-TU drift). */
    ASSERT(uft_format_plugin_g71.read_track(&disk, 5, 0, &t) == UFT_OK);
    ASSERT(t.status == UFT_TRACK_UNFORMATTED);
    ASSERT(t.raw_data == NULL);
    ASSERT(t.raw_size == 0);
    uft_format_plugin_g71.close(&disk);
    remove(path);
}

TEST(out_of_range_rejected) {
    char path[300]; get_temp_path(path, sizeof(path));
    uint8_t a[SZ_A], b[SZ_B];
    ASSERT(build_g71(path, a, b));
    uft_disk_t disk; ASSERT(open_disk(path, &disk));
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_g71.read_track(&disk, 0, 2, &t) != UFT_OK);   /* head 2 */
    ASSERT(uft_format_plugin_g71.read_track(&disk, 42, 0, &t) != UFT_OK);  /* cyl 42 */
    uft_format_plugin_g71.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== G71 1571 GCR per-track raw read (MF-355) ===\n");
    RUN(side0_track0_raw_matches);
    RUN(side1_track0_raw_matches);
    RUN(empty_track_unformatted);
    RUN(out_of_range_rejected);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
