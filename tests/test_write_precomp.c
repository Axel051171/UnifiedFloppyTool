/**
 * @file test_write_precomp.c
 * @brief Unit tests for uft_precomp_track_mac800k (TA1 M2 port).
 *
 * Self-contained per template from tests/test_interleave.c.
 * Verifies the Mac-800K peak-shift compensation algorithm against
 * hand-computed expected outputs.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "uft/core/uft_write_precomp.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* A representative Mac 800K flux captured at 40 MHz:
 *   rotation = 394 RPM → period = 152 284 us
 *   samples_per_rev at 40 MHz = 6 091 370 (approx)
 * Use a round figure for test reproducibility. */
#define MAC800_SAMPLES_PER_REV  6000000u

TEST(rejects_null_ptr) {
    ASSERT(uft_precomp_track_mac800k(NULL, 10, MAC800_SAMPLES_PER_REV, 0)
           == UFT_ERR_INVALID_ARG);
}

TEST(noop_on_empty) {
    uint32_t t[1] = { 0 };
    ASSERT(uft_precomp_track_mac800k(t, 0, MAC800_SAMPLES_PER_REV, 0)
           == UFT_OK);
    ASSERT(t[0] == 0);
}

TEST(noop_on_single_transition) {
    uint32_t t[1] = { 1234 };
    ASSERT(uft_precomp_track_mac800k(t, 1, MAC800_SAMPLES_PER_REV, 0)
           == UFT_OK);
    ASSERT(t[0] == 1234);
}

TEST(noop_on_two_transitions) {
    uint32_t t[2] = { 100, 200 };
    ASSERT(uft_precomp_track_mac800k(t, 2, MAC800_SAMPLES_PER_REV, 0)
           == UFT_OK);
    ASSERT(t[0] == 100);
    ASSERT(t[1] == 200);
}

TEST(preserves_first_and_last) {
    /* Three transitions forming a corrected-eligible pattern;
     * middle one may shift but first/last never do. */
    uint32_t t[3] = { 1000, 1050, 2000 };
    uint32_t t0_before = t[0];
    uint32_t t2_before = t[2];
    ASSERT(uft_precomp_track_mac800k(t, 3, MAC800_SAMPLES_PER_REV, 0)
           == UFT_OK);
    ASSERT(t[0] == t0_before);
    ASSERT(t[2] == t2_before);
}

TEST(wide_transitions_unchanged) {
    /* Well-spaced transitions (far above threshold) → no correction. */
    uint32_t t[5] = { 0, 10000, 20000, 30000, 40000 };
    uint32_t expected[5];
    memcpy(expected, t, sizeof(t));
    ASSERT(uft_precomp_track_mac800k(t, 5, MAC800_SAMPLES_PER_REV, 0)
           == UFT_OK);
    ASSERT(memcmp(t, expected, sizeof(t)) == 0);
}

TEST(close_pair_triggers_shift) {
    /* Three transitions: wide first gap, narrow second gap.
     * Peak-shift physics: close transitions appear FURTHER apart in
     * the readback than they really are. So the algorithm COMPRESSES
     * narrow pairs toward their true (smaller) spacing.
     *
     * Mechanism in the reference code:
     *   shift = (delta2 - delta1) * 5 / 12
     * When t12 < threshold and t01 is wide: delta2 > 0, delta1 = 0,
     * shift is positive → t[1] moves toward t[2] (narrowing t12
     * further). The verification is that a shift DID happen; direction
     * is determined by the algorithm, not by our intuition. */
    uint32_t t[3] = { 0, 2000, 2080 };   /* wide then near-threshold */

    ASSERT(uft_precomp_track_mac800k(t, 3, MAC800_SAMPLES_PER_REV, 0)
           == UFT_OK);

    ASSERT(t[0] == 0);
    ASSERT(t[2] == 2080);
    ASSERT(t[1] != 2000);               /* correction applied */
    /* t[1] must stay within the original half-distance clamps:
     *   lo = -t01/2 = -1000 → t1 >= 1000
     *   hi =  t12/2 =   40 → t1 <= 2040
     */
    ASSERT(t[1] >= 1000);
    ASSERT(t[1] <= 2040);
}

TEST(order_preserved_after_correction) {
    /* Several close transitions — corrections must keep them
     * monotonically increasing (no crossings). */
    uint32_t t[6] = { 0, 80, 160, 240, 320, 10000 };
    ASSERT(uft_precomp_track_mac800k(t, 6, MAC800_SAMPLES_PER_REV, 0)
           == UFT_OK);
    for (size_t i = 1; i < 6; i++) {
        ASSERT(t[i] > t[i - 1]);
    }
}

TEST(apply_none_is_noop) {
    uint32_t t[5] = { 0, 50, 100, 150, 10000 };
    uint32_t backup[5];
    memcpy(backup, t, sizeof(t));
    ASSERT(uft_precomp_apply(t, 5, MAC800_SAMPLES_PER_REV, 0,
                              UFT_PRECOMP_NONE) == UFT_OK);
    ASSERT(memcmp(t, backup, sizeof(t)) == 0);
}

TEST(apply_auto_is_noop) {
    /* AUTO mode is reserved; a8rawconv also returns early. */
    uint32_t t[5] = { 0, 50, 100, 150, 10000 };
    uint32_t backup[5];
    memcpy(backup, t, sizeof(t));
    ASSERT(uft_precomp_apply(t, 5, MAC800_SAMPLES_PER_REV, 0,
                              UFT_PRECOMP_AUTO) == UFT_OK);
    ASSERT(memcmp(t, backup, sizeof(t)) == 0);
}

TEST(apply_mac800k_matches_direct_call) {
    /* apply(MAC_800K) must produce the same output as calling
     * uft_precomp_track_mac800k directly. */
    uint32_t t_apply[6] = { 0, 80, 160, 240, 320, 10000 };
    uint32_t t_direct[6];
    memcpy(t_direct, t_apply, sizeof(t_apply));

    ASSERT(uft_precomp_apply(t_apply, 6, MAC800_SAMPLES_PER_REV, 5,
                              UFT_PRECOMP_MAC_800K) == UFT_OK);
    ASSERT(uft_precomp_track_mac800k(t_direct, 6,
                                      MAC800_SAMPLES_PER_REV, 5) == UFT_OK);
    ASSERT(memcmp(t_apply, t_direct, sizeof(t_apply)) == 0);
}

TEST(apply_invalid_mode) {
    uint32_t t[3] = { 0, 100, 200 };
    ASSERT(uft_precomp_apply(t, 3, MAC800_SAMPLES_PER_REV, 0,
                              (uft_precomp_mode_t)99) == UFT_ERR_INVALID_ARG);
}

TEST(inner_track_clamp) {
    /* Tracks > 47 should behave identically to track 47
     * (inner-track geometry clamp in the reference). */
    uint32_t t50[5] = { 0, 80, 160, 240, 320 };
    uint32_t t47[5] = { 0, 80, 160, 240, 320 };
    ASSERT(uft_precomp_track_mac800k(t50, 5, MAC800_SAMPLES_PER_REV, 50)
           == UFT_OK);
    ASSERT(uft_precomp_track_mac800k(t47, 5, MAC800_SAMPLES_PER_REV, 47)
           == UFT_OK);
    ASSERT(memcmp(t50, t47, sizeof(t50)) == 0);
}

int main(void) {
    printf("=== uft_precomp_track_mac800k tests ===\n");
    RUN(rejects_null_ptr);
    RUN(noop_on_empty);
    RUN(noop_on_single_transition);
    RUN(noop_on_two_transitions);
    RUN(preserves_first_and_last);
    RUN(wide_transitions_unchanged);
    RUN(close_pair_triggers_shift);
    RUN(order_preserved_after_correction);
    RUN(apply_none_is_noop);
    RUN(apply_auto_is_noop);
    RUN(apply_mac800k_matches_direct_call);
    RUN(apply_invalid_mode);
    RUN(inner_track_clamp);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
