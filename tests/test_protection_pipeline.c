/**
 * @file test_protection_pipeline.c
 * @brief Weak-bit detection, against the SHIPPED code (MF-400).
 *
 * This file used to consist entirely of mocks and tautologies. It defined its
 * own mock_detect_weak_bits() and asserted against that; the other cases
 * asserted that setting bit 0 sets bit 0, that 80 * 2 == 160, and that a local
 * `bool scp_weak = true` is true. No UFT code was called anywhere in it, so it
 * could not have failed for any change to the project.
 *
 * The real detector is uft_protection_detect_weak_bits() in
 * src/protection/uft_protection_extended.c. It had no test and, at the time of
 * writing, no caller either — see docs/KNOWN_ISSUES.md PROT-10.
 *
 * The expected bit positions below were computed independently before this
 * code was run, from the documented semantics (MSB first: the first bit on the
 * disk is the most significant bit of the byte), not read back out of the
 * implementation.
 *
 * Why the exact position matters: a weak-bit list is what a writer uses to
 * reproduce the protection. A position that is off is not a missing feature,
 * it is a wrong one — the mask lands on a bit that was never weak.
 */

#include "uft/protection/uft_protection_extended.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define MAXPOS 64

TEST(identical_revolutions_yield_no_weak_bits) {
    /* Two reads that agree carry no evidence of a weak bit. Reporting one
     * would be inventing a protection feature that is not on the disk. */
    const uint8_t rev[] = { 0xAA, 0x55, 0xFF, 0x00, 0x12, 0x34 };
    uint32_t pos[MAXPOS];

    ASSERT(uft_protection_detect_weak_bits(rev, rev, sizeof(rev), pos, MAXPOS) == 0);
}

TEST(a_single_differing_bit_is_located_exactly) {
    uint32_t pos[MAXPOS];

    /* MSB of byte 0 -> position 0 */
    const uint8_t a1[] = { 0x80 }, b1[] = { 0x00 };
    ASSERT(uft_protection_detect_weak_bits(a1, b1, 1, pos, MAXPOS) == 1);
    ASSERT(pos[0] == 0);

    /* LSB of byte 0 -> position 7 */
    const uint8_t a2[] = { 0x01 }, b2[] = { 0x00 };
    ASSERT(uft_protection_detect_weak_bits(a2, b2, 1, pos, MAXPOS) == 1);
    ASSERT(pos[0] == 7);

    /* MSB of byte 3 -> position 24 */
    const uint8_t a3[] = { 0, 0, 0, 0x80 }, b3[] = { 0, 0, 0, 0 };
    ASSERT(uft_protection_detect_weak_bits(a3, b3, 4, pos, MAXPOS) == 1);
    ASSERT(pos[0] == 24);
}

TEST(positions_are_msb_first_and_ascending) {
    uint32_t pos[MAXPOS];

    /* All eight bits of byte 1 differ -> 8..15 in order. Ascending order is
     * what makes the list usable as a track-order mask. */
    const uint8_t a[] = { 0x00, 0xFF }, b[] = { 0x00, 0x00 };
    ASSERT(uft_protection_detect_weak_bits(a, b, 2, pos, MAXPOS) == 8);
    for (uint32_t i = 0; i < 8; i++) ASSERT(pos[i] == 8 + i);

    /* 0xAA vs 0x55 differs in every bit of byte 0 */
    const uint8_t c[] = { 0xAA }, d[] = { 0x55 };
    ASSERT(uft_protection_detect_weak_bits(c, d, 1, pos, MAXPOS) == 8);
    for (uint32_t i = 0; i < 8; i++) ASSERT(pos[i] == i);
}

TEST(the_comparison_is_symmetric) {
    /* Which read is called "rev1" is arbitrary, so the answer must not depend
     * on it. */
    const uint8_t a[] = { 0x0F, 0xC3, 0x00 }, b[] = { 0xF0, 0xC0, 0x00 };
    uint32_t p1[MAXPOS], p2[MAXPOS];

    size_t n1 = uft_protection_detect_weak_bits(a, b, 3, p1, MAXPOS);
    size_t n2 = uft_protection_detect_weak_bits(b, a, 3, p2, MAXPOS);
    ASSERT(n1 == n2);
    ASSERT(n1 > 0);
    ASSERT(memcmp(p1, p2, n1 * sizeof(p1[0])) == 0);
}

TEST(weak_bits_beyond_byte_8191_keep_their_true_position) {
    /* Regression guard for the defect this rewrite uncovered (KNOWN_ISSUES
     * PROT-9). The position was a uint16_t, which holds bit positions only up
     * to byte 8191. Real raw tracks are larger than that — Amiga DD is ~12798
     * bytes, PC HD ~12500 — so a weak bit in the second half of a track was
     * not dropped, it was reported at (position mod 65536): a bit near the
     * track start that was never weak. Wrong data, not missing data. */
    const size_t size = 9000;
    uint8_t *a = calloc(size, 1), *b = calloc(size, 1);
    uint32_t pos[MAXPOS];
    ASSERT(a && b);

    a[8192] = 0x80;                         /* true position 65536 */
    a[8600] = 0x01;                         /* true position 68807 */

    size_t n = uft_protection_detect_weak_bits(a, b, size, pos, MAXPOS);
    ASSERT(n == 2);
    ASSERT(pos[0] == 8192u * 8u);
    ASSERT(pos[1] == 8600u * 8u + 7u);

    free(a); free(b);
}

TEST(a_full_output_buffer_is_reported_as_truncated) {
    /* More weak bits than the caller's array can hold. "Kein Bit verloren"
     * means the caller has to be able to tell that the list is incomplete,
     * so the return value must exceed max_positions rather than stop at it. */
    uint8_t a[16], b[16];
    memset(a, 0xFF, sizeof(a));
    memset(b, 0x00, sizeof(b));            /* 128 differing bits */

    uint32_t pos[8];
    size_t n = uft_protection_detect_weak_bits(a, b, sizeof(a), pos, 8);

    ASSERT(n == 128);                      /* the true count, not the capacity */
    for (uint32_t i = 0; i < 8; i++) ASSERT(pos[i] == i);   /* first 8 stored */
}

TEST(rejects_degenerate_arguments) {
    const uint8_t a[] = { 0xFF }, b[] = { 0x00 };
    uint32_t pos[MAXPOS];

    ASSERT(uft_protection_detect_weak_bits(NULL, b, 1, pos, MAXPOS) == 0);
    ASSERT(uft_protection_detect_weak_bits(a, NULL, 1, pos, MAXPOS) == 0);
    ASSERT(uft_protection_detect_weak_bits(a, b, 1, NULL, MAXPOS) == 0);
    ASSERT(uft_protection_detect_weak_bits(a, b, 0, pos, MAXPOS) == 0);

    /* Zero capacity must not write through the pointer. A canary catches a
     * store that the return value alone would not reveal. */
    uint32_t canary[2] = { 0xDEADBEEFu, 0xDEADBEEFu };
    (void)uft_protection_detect_weak_bits(a, b, 1, canary, 0);
    ASSERT(canary[0] == 0xDEADBEEFu && canary[1] == 0xDEADBEEFu);
}

int main(void)
{
    printf("=== Weak-bit detection, real code (MF-400) ===\n");
    RUN(identical_revolutions_yield_no_weak_bits);
    RUN(a_single_differing_bit_is_located_exactly);
    RUN(positions_are_msb_first_and_ascending);
    RUN(the_comparison_is_symmetric);
    RUN(weak_bits_beyond_byte_8191_keep_their_true_position);
    RUN(a_full_output_buffer_is_reported_as_truncated);
    RUN(rejects_degenerate_arguments);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
