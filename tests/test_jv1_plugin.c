/**
 * @file test_jv1_plugin.c
 * @brief JV1 (Jeff Vavasour TRS-80 single-density) Plugin — probe tests.
 *
 * Self-contained per template from test_d64_plugin.c.
 * Mirrors src/formats/jv1/uft_jv1.c::jv1_probe and jv1_detect_geometry.
 *
 * Format: Headerless raw, 10 sectors × 256 bytes per track. Tracks
 * are variable (1..40 for single-sided, up to 80 for double-sided).
 * Probe returns true + confidence 35 for any file size that is a
 * non-zero multiple of 2560 (= 10 × 256) with a plausible track count.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-28s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define JV1_SECTOR_SIZE   256u
#define JV1_SPT           10u
#define JV1_TRACK_SIZE    (JV1_SPT * JV1_SECTOR_SIZE)   /* 2560 */
#define JV1_CONF          35

static bool jv1_detect_geometry(size_t file_size,
                                 uint8_t *cyl, uint8_t *heads) {
    if (file_size == 0 || file_size % JV1_TRACK_SIZE != 0) return false;
    uint32_t total_tracks = (uint32_t)(file_size / JV1_TRACK_SIZE);
    if (total_tracks <= 40) {
        if (cyl) *cyl = (uint8_t)total_tracks;
        if (heads) *heads = 1;
        return true;
    }
    /* Upper-bound reject; real JV1 rarely goes beyond 80 tracks. */
    if (total_tracks > 80) return false;
    if (cyl) *cyl = (uint8_t)(total_tracks / 2);
    if (heads) *heads = 2;
    return true;
}

static int jv1_probe_replica(size_t file_size) {
    uint8_t c = 0, h = 0;
    if (!jv1_detect_geometry(file_size, &c, &h)) return 0;
    return JV1_CONF;
}

TEST(probe_35_track_single_sided) {
    ASSERT(jv1_probe_replica(35u * JV1_TRACK_SIZE) == JV1_CONF);
}

TEST(probe_40_track_single_sided) {
    ASSERT(jv1_probe_replica(40u * JV1_TRACK_SIZE) == JV1_CONF);
}

TEST(probe_80_track_double_sided) {
    ASSERT(jv1_probe_replica(80u * JV1_TRACK_SIZE) == JV1_CONF);
}

TEST(probe_geometry_35_ss) {
    uint8_t c = 0, h = 0;
    ASSERT(jv1_detect_geometry(35u * JV1_TRACK_SIZE, &c, &h));
    ASSERT(c == 35 && h == 1);
}

TEST(probe_geometry_80_ds) {
    uint8_t c = 0, h = 0;
    ASSERT(jv1_detect_geometry(80u * JV1_TRACK_SIZE, &c, &h));
    ASSERT(c == 40 && h == 2);
}

TEST(probe_rejects_zero) {
    ASSERT(jv1_probe_replica(0) == 0);
}

TEST(probe_rejects_off_by_track) {
    /* 35 × 2560 + 128 = off a half-sector */
    ASSERT(jv1_probe_replica(35u * JV1_TRACK_SIZE + 128u) == 0);
}

TEST(probe_rejects_too_many_tracks) {
    /* 200 × 2560 = 512000 — way beyond any real TRS-80 disk. */
    ASSERT(jv1_probe_replica(200u * JV1_TRACK_SIZE) == 0);
}

TEST(probe_rejects_d64_size) {
    /* 174848 is not a multiple of 2560 — must reject. */
    ASSERT(jv1_probe_replica(174848u) == 0);
}

TEST(probe_boundary_41_tracks_treated_as_ds) {
    /* 41 tracks: > 40 → treated as double-sided (41/2=20.5 → 20). */
    uint8_t c = 0, h = 0;
    ASSERT(jv1_detect_geometry(41u * JV1_TRACK_SIZE, &c, &h));
    ASSERT(h == 2);
    ASSERT(c == 20);
}

int main(void) {
    printf("=== JV1 plugin probe tests ===\n");
    RUN(probe_35_track_single_sided);
    RUN(probe_40_track_single_sided);
    RUN(probe_80_track_double_sided);
    RUN(probe_geometry_35_ss);
    RUN(probe_geometry_80_ds);
    RUN(probe_rejects_zero);
    RUN(probe_rejects_off_by_track);
    RUN(probe_rejects_too_many_tracks);
    RUN(probe_rejects_d64_size);
    RUN(probe_boundary_41_tracks_treated_as_ds);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
