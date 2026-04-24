/**
 * @file test_d64_plugin.c
 * @brief D64 (Commodore 1541) Plugin — probe tests.
 *
 * Self-contained per template from test_crt_plugin.c.
 * Mirrors src/formats/commodore/d64.c probe logic (size-based match).
 *
 * D64 has no magic; probe is exact file-size match against the four
 * canonical shapes:
 *   174848 bytes = 35 tracks, no error info
 *   175531 bytes = 35 tracks, 683 error bytes appended
 *   196608 bytes = 40 tracks, no error info
 *   197376 bytes = 40 tracks, 768 error bytes appended
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-28s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define D64_35_TRACKS_NOERR   174848u
#define D64_35_TRACKS_WITHERR 175531u
#define D64_40_TRACKS_NOERR   196608u
#define D64_40_TRACKS_WITHERR 197376u

static bool d64_probe_replica(size_t file_size,
                               int *out_tracks, bool *out_has_errors) {
    if (out_tracks) *out_tracks = 0;
    if (out_has_errors) *out_has_errors = false;

    if (file_size == D64_35_TRACKS_NOERR) {
        if (out_tracks) *out_tracks = 35;
        return true;
    }
    if (file_size == D64_35_TRACKS_WITHERR) {
        if (out_tracks) *out_tracks = 35;
        if (out_has_errors) *out_has_errors = true;
        return true;
    }
    if (file_size == D64_40_TRACKS_NOERR) {
        if (out_tracks) *out_tracks = 40;
        return true;
    }
    if (file_size == D64_40_TRACKS_WITHERR) {
        if (out_tracks) *out_tracks = 40;
        if (out_has_errors) *out_has_errors = true;
        return true;
    }
    return false;
}

TEST(probe_35_tracks_no_error) {
    int tracks = 0;
    bool errs = false;
    ASSERT(d64_probe_replica(D64_35_TRACKS_NOERR, &tracks, &errs));
    ASSERT(tracks == 35);
    ASSERT(errs == false);
}

TEST(probe_35_tracks_with_error) {
    int tracks = 0;
    bool errs = false;
    ASSERT(d64_probe_replica(D64_35_TRACKS_WITHERR, &tracks, &errs));
    ASSERT(tracks == 35);
    ASSERT(errs == true);
}

TEST(probe_40_tracks_no_error) {
    int tracks = 0;
    bool errs = false;
    ASSERT(d64_probe_replica(D64_40_TRACKS_NOERR, &tracks, &errs));
    ASSERT(tracks == 40);
    ASSERT(errs == false);
}

TEST(probe_40_tracks_with_error) {
    int tracks = 0;
    bool errs = false;
    ASSERT(d64_probe_replica(D64_40_TRACKS_WITHERR, &tracks, &errs));
    ASSERT(tracks == 40);
    ASSERT(errs == true);
}

TEST(probe_reject_nearby_wrong_size) {
    /* Off-by-one in each direction must be rejected. */
    ASSERT(!d64_probe_replica(D64_35_TRACKS_NOERR - 1, NULL, NULL));
    ASSERT(!d64_probe_replica(D64_35_TRACKS_NOERR + 1, NULL, NULL));
    ASSERT(!d64_probe_replica(D64_40_TRACKS_WITHERR + 1, NULL, NULL));
}

TEST(probe_reject_zero_size) {
    ASSERT(!d64_probe_replica(0, NULL, NULL));
}

TEST(probe_reject_d71_size) {
    /* D71 is 349696 bytes — must be rejected by d64 probe even though
     * it's "a Commodore disk image". */
    ASSERT(!d64_probe_replica(349696u, NULL, NULL));
}

TEST(probe_reject_d81_size) {
    /* D81 is 819200 bytes — must be rejected. */
    ASSERT(!d64_probe_replica(819200u, NULL, NULL));
}

int main(void) {
    printf("=== D64 plugin probe tests ===\n");
    RUN(probe_35_tracks_no_error);
    RUN(probe_35_tracks_with_error);
    RUN(probe_40_tracks_no_error);
    RUN(probe_40_tracks_with_error);
    RUN(probe_reject_nearby_wrong_size);
    RUN(probe_reject_zero_size);
    RUN(probe_reject_d71_size);
    RUN(probe_reject_d81_size);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
