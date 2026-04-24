/**
 * @file test_interleave.c
 * @brief Unit tests for uft_compute_interleave (TA2 M2 port).
 *
 * Self-contained per template from test_atr_plugin.c.
 * References a8rawconv/interleave.cpp::compute_interleave as the
 * reference implementation. The formulae below match the reference
 * for the three common Atari sector shapes:
 *   128-byte SD (18 sectors per track) → interleave = 9
 *   128-byte ED (26 sectors per track) → interleave = 13
 *   256-byte DD (18 sectors per track) → interleave = 15
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "uft/core/uft_interleave.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* Returns the interleave factor the algorithm picked by observing
 * which slot was used for the second sector. */
static int infer_interleave(const float *t, size_t n, float spacing) {
    (void)n;
    /* Slot 0 is always sector 0 (timing = t0 + 0). Sector 1 at slot_idx
     * = interleave. Its fractional position is t0 + spacing * interleave
     * (mod 1). We can recover it by dividing the delta. */
    float d = t[1] - t[0];
    if (d < 0.0f) d += 1.0f;
    float raw = d / spacing;
    return (int)lroundf(raw);
}

TEST(rejects_null_out) {
    ASSERT(uft_compute_interleave(NULL, 18, 128, false, 0, 0,
                                    UFT_INTERLEAVE_AUTO) == UFT_ERR_INVALID_ARG);
}

TEST(rejects_zero_sectors) {
    float t[1];
    ASSERT(uft_compute_interleave(t, 0, 128, false, 0, 0,
                                    UFT_INTERLEAVE_AUTO) == UFT_ERR_INVALID_ARG);
}

TEST(rejects_too_many_sectors) {
    float t[1];
    ASSERT(uft_compute_interleave(t, 257, 128, false, 0, 0,
                                    UFT_INTERLEAVE_AUTO) == UFT_ERR_INVALID_ARG);
}

TEST(sd_auto_18_sectors_uses_9_interleave) {
    float t[18];
    ASSERT(uft_compute_interleave(t, 18, 128, false, 0, 0,
                                    UFT_INTERLEAVE_AUTO) == UFT_OK);
    const float spacing = 0.98f / 18.0f;
    ASSERT(infer_interleave(t, 18, spacing) == 9);
}

TEST(ed_auto_26_sectors_uses_13_interleave) {
    float t[26];
    ASSERT(uft_compute_interleave(t, 26, 128, false, 0, 0,
                                    UFT_INTERLEAVE_AUTO) == UFT_OK);
    const float spacing = 0.98f / 26.0f;
    ASSERT(infer_interleave(t, 26, spacing) == 13);
}

TEST(dd_auto_18_sectors_uses_15_interleave) {
    float t[18];
    ASSERT(uft_compute_interleave(t, 18, 256, false, 0, 0,
                                    UFT_INTERLEAVE_AUTO) == UFT_OK);
    const float spacing = 0.98f / 18.0f;
    ASSERT(infer_interleave(t, 18, spacing) == 15);
}

TEST(mode_none_is_sequential_1_to_1) {
    float t[18];
    ASSERT(uft_compute_interleave(t, 18, 128, false, 0, 0,
                                    UFT_INTERLEAVE_NONE) == UFT_OK);
    const float spacing = 0.98f / 18.0f;
    /* With interleave=1 and track=0 (so t0=0), positions are
     * spacing*0, spacing*1, spacing*2, ... */
    for (size_t i = 0; i < 18; i++) {
        float expected = spacing * (float)i;
        ASSERT(fabsf(t[i] - expected) < 1e-5f);
    }
}

TEST(xf551_dd_hs_matches_sd_9_to_1) {
    float t_xf[18], t_sd[18];
    ASSERT(uft_compute_interleave(t_xf, 18, 256, true, 0, 0,
                                    UFT_INTERLEAVE_XF551_DD_HS) == UFT_OK);
    ASSERT(uft_compute_interleave(t_sd, 18, 128, false, 0, 0,
                                    UFT_INTERLEAVE_AUTO) == UFT_OK);
    /* XF551-DD-HS overrides sector_size and uses 9:1 like SD.
     * The infer_interleave should read 9 in both. */
    const float spacing = 0.98f / 18.0f;
    ASSERT(infer_interleave(t_xf, 18, spacing) == 9);
    ASSERT(infer_interleave(t_sd, 18, spacing) == 9);
}

TEST(all_positions_in_unit_interval) {
    float t[26];
    ASSERT(uft_compute_interleave(t, 26, 128, false, 17, 0,
                                    UFT_INTERLEAVE_AUTO) == UFT_OK);
    /* Track 17 means t0 = 0.08 * 17 = 1.36 → fractionally 0.36.
     * All outputs must land in [0, 1). */
    for (size_t i = 0; i < 26; i++) {
        ASSERT(t[i] >= 0.0f);
        ASSERT(t[i] < 1.0f);
    }
}

TEST(all_slots_are_allocated_exactly_once) {
    /* Each sector gets a unique slot, and slot positions are
     * deterministic. The set of timings for interleave=9, n=18
     * should have no duplicates. */
    float t[18];
    ASSERT(uft_compute_interleave(t, 18, 128, false, 0, 0,
                                    UFT_INTERLEAVE_AUTO) == UFT_OK);
    for (size_t i = 0; i < 18; i++) {
        for (size_t j = i + 1; j < 18; j++) {
            ASSERT(fabsf(t[i] - t[j]) > 1e-5f);
        }
    }
}

TEST(invalid_mode_rejected) {
    float t[18];
    ASSERT(uft_compute_interleave(t, 18, 128, false, 0, 0,
                                    (uft_interleave_mode_t)99) == UFT_ERR_INVALID_ARG);
}

int main(void) {
    printf("=== uft_compute_interleave tests ===\n");
    RUN(rejects_null_out);
    RUN(rejects_zero_sectors);
    RUN(rejects_too_many_sectors);
    RUN(sd_auto_18_sectors_uses_9_interleave);
    RUN(ed_auto_26_sectors_uses_13_interleave);
    RUN(dd_auto_18_sectors_uses_15_interleave);
    RUN(mode_none_is_sequential_1_to_1);
    RUN(xf551_dd_hs_matches_sd_9_to_1);
    RUN(all_positions_in_unit_interval);
    RUN(all_slots_are_allocated_exactly_once);
    RUN(invalid_mode_rejected);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
