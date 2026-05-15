/**
 * @file test_marginal_data_preserved.c
 * @brief Improvement test — UFT preserves marginal/divergent reads.
 *
 * P3.3 / task #110, forensic category. Makes the DESIGN_PRINCIPLES
 * "Kein Bit verloren" property executable: when multiple reads of the
 * same data disagree, UFT's multi-read voting emits the majority value
 * BUT preserves the fact of the disagreement as forensic metadata
 * (weak_mask, has_weak_bits, a sub-100 confidence). The divergence
 * survives to the output — it is never silently flattened away.
 *
 * Why gw cannot pass this: Greaseweazle (and classic imagers) majority-
 * vote marginal reads and emit a clean buffer with NO record that the
 * reads disagreed. The disagreement — often the signature of a weak-bit
 * copy protection or of media degradation — is lost. UFT keeps it.
 *
 * Also covers the related forensic rule "verifizierbare Information
 * dominiert": a read that FAILED its CRC can never outvote a
 * CRC-verified read, no matter how numerous the failed reads are.
 *
 * Tests OBSERVED behaviour of the public multiread_* API
 * (include/uft/recovery/uft_multiread_pipeline.h, MF-215). Real
 * CHECK-style macros, not assert() — the suite builds with -DNDEBUG.
 */
#include "uft/recovery/uft_multiread_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* ── unanimous reads: nothing flagged weak ───────────────────────── */
TEST(unanimous_reads_have_no_weak_bits) {
    multiread_ctx_t *ctx = multiread_create(NULL);   /* default config */
    ASSERT(ctx != NULL);

    const uint8_t pass[4] = { 0x11, 0x22, 0x33, 0x44 };
    ASSERT(multiread_add_pass(ctx, pass, 4, 100, true) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, pass, 4, 100, true) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, pass, 4, 100, true) == MULTIREAD_OK);

    uint8_t out[4] = {0};
    multiread_sector_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(multiread_execute(ctx, out, 4, &res) == MULTIREAD_OK);

    ASSERT(memcmp(out, pass, 4) == 0);          /* output == the agreed data */
    ASSERT(res.has_weak_bits == false);         /* nothing divergent */
    ASSERT(res.confidence == 100);              /* full agreement */
    if (res.weak_mask) {
        for (int i = 0; i < 4; i++)
            ASSERT(res.weak_mask[i] == 0);
    }

    free(res.weak_mask);
    multiread_destroy(ctx);
}

/* ── THE forensic property: a divergent byte is PRESERVED ─────────── */
TEST(divergent_byte_is_preserved_not_collapsed) {
    multiread_ctx_t *ctx = multiread_create(NULL);
    ASSERT(ctx != NULL);

    /* Three reads; byte 2 disagrees (0xAA, 0xAA, 0xBB). A naive imager
     * would emit {.., 0xAA, ..} and throw the disagreement away. */
    const uint8_t p1[4] = { 0x11, 0x22, 0xAA, 0x44 };
    const uint8_t p2[4] = { 0x11, 0x22, 0xAA, 0x44 };
    const uint8_t p3[4] = { 0x11, 0x22, 0xBB, 0x44 };
    ASSERT(multiread_add_pass(ctx, p1, 4, 100, true) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, p2, 4, 100, true) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, p3, 4, 100, true) == MULTIREAD_OK);

    uint8_t out[4] = {0};
    multiread_sector_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(multiread_execute(ctx, out, 4, &res) == MULTIREAD_OK);

    /* Majority value IS emitted... */
    ASSERT(out[2] == 0xAA);
    /* ...but the disagreement is PRESERVED, not silently dropped: */
    ASSERT(res.has_weak_bits == true);
    ASSERT(res.weak_mask != NULL);
    ASSERT(res.weak_mask[2] == 1);              /* byte 2 flagged divergent */
    ASSERT(res.weak_mask[0] == 0);              /* the agreed bytes are not */
    ASSERT(res.weak_mask[1] == 0);
    ASSERT(res.weak_mask[3] == 0);
    /* confidence reflects the divergence — strictly below unanimous */
    ASSERT(res.confidence < 100);

    free(res.weak_mask);
    multiread_destroy(ctx);
}

/* ── verifiable information dominates: a failed-CRC read cannot win ─ */
TEST(failed_crc_read_cannot_outvote_a_verified_one) {
    multiread_ctx_t *ctx = multiread_create(NULL);
    ASSERT(ctx != NULL);

    /* ONE CRC-verified read says 0xAA; TWO CRC-failed reads say 0xBB.
     * A blind majority would pick 0xBB (2 vs 1). The forensic rule:
     * a read of unknown integrity must not sway the result, so the
     * single verified read wins. */
    const uint8_t verified[4] = { 0xAA, 0xAA, 0xAA, 0xAA };
    const uint8_t corrupt [4] = { 0xBB, 0xBB, 0xBB, 0xBB };
    ASSERT(multiread_add_pass(ctx, verified, 4, 100, /*crc_ok=*/true)  == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, corrupt,  4,  50, /*crc_ok=*/false) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, corrupt,  4,  50, /*crc_ok=*/false) == MULTIREAD_OK);

    uint8_t out[4] = {0};
    multiread_sector_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(multiread_execute(ctx, out, 4, &res) == MULTIREAD_OK);

    for (int i = 0; i < 4; i++)
        ASSERT(out[i] == 0xAA);                 /* verified read wins */
    ASSERT(res.good_reads == 1);                /* exactly one CRC-OK read */
    ASSERT(res.total_reads == 3);

    free(res.weak_mask);
    multiread_destroy(ctx);
}

/* ── no verified read at all: still answers, still flags uncertainty ─ */
TEST(no_verified_read_still_records_divergence) {
    multiread_ctx_t *ctx = multiread_create(NULL);
    ASSERT(ctx != NULL);

    /* All three reads failed CRC and they disagree at byte 0. With no
     * authoritative pass the voter falls back to the full pool — it
     * still produces an answer, but the divergence must still surface
     * (honest uncertainty, never a confident lie). */
    const uint8_t a[2] = { 0xAA, 0x00 };
    const uint8_t b[2] = { 0xBB, 0x00 };
    ASSERT(multiread_add_pass(ctx, a, 2, 50, false) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, a, 2, 50, false) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, b, 2, 50, false) == MULTIREAD_OK);

    uint8_t out[2] = {0};
    multiread_sector_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(multiread_execute(ctx, out, 2, &res) == MULTIREAD_OK);

    ASSERT(out[0] == 0xAA);                     /* majority of the pool */
    ASSERT(res.has_weak_bits == true);          /* divergence still recorded */
    ASSERT(res.weak_mask && res.weak_mask[0] == 1);
    ASSERT(res.weak_mask[1] == 0);

    free(res.weak_mask);
    multiread_destroy(ctx);
}

/* ── multiread_vote_buffers: divergence lowers reported confidence ── */
TEST(vote_buffers_confidence_drops_on_divergence) {
    const uint8_t b1[4] = { 0x10, 0x20, 0x30, 0x40 };
    const uint8_t b2[4] = { 0x10, 0x20, 0x30, 0x40 };
    const uint8_t b3[4] = { 0x10, 0x20, 0x99, 0x40 };   /* byte 2 diverges */
    const uint8_t *bufs[3]  = { b1, b2, b3 };
    const size_t   lens[3]  = { 4, 4, 4 };

    uint8_t out[4] = {0};
    uint8_t conf = 0;
    ASSERT(multiread_vote_buffers(bufs, lens, 3, out, 4, &conf) == MULTIREAD_OK);
    ASSERT(out[2] == 0x30);                     /* majority emitted */
    ASSERT(conf < 100);                         /* divergence pulls confidence down */

    /* Unanimous control: confidence is full. */
    const uint8_t *same[3] = { b1, b2, b1 };
    uint8_t conf2 = 0;
    ASSERT(multiread_vote_buffers(same, lens, 3, out, 4, &conf2) == MULTIREAD_OK);
    ASSERT(conf2 == 100);
}

/* ── guards: NULL params + insufficient passes ───────────────────── */
TEST(guards_null_and_insufficient_passes) {
    uint8_t out[4] = {0};
    multiread_sector_t res;
    memset(&res, 0, sizeof(res));

    ASSERT(multiread_execute(NULL, out, 4, &res) == MULTIREAD_ERR_NULL_PARAM);
    ASSERT(multiread_add_pass(NULL, out, 4, 100, true) == MULTIREAD_ERR_NULL_PARAM);

    /* default config: min_passes = 3; two passes is not enough */
    multiread_ctx_t *ctx = multiread_create(NULL);
    ASSERT(ctx != NULL);
    const uint8_t d[4] = { 1, 2, 3, 4 };
    ASSERT(multiread_add_pass(ctx, d, 4, 100, true) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, d, 4, 100, true) == MULTIREAD_OK);
    ASSERT(multiread_execute(ctx, out, 4, &res) ==
           MULTIREAD_ERR_INSUFFICIENT_PASSES);

    multiread_destroy(ctx);
}

int main(void) {
    printf("=== Improvement: marginal/divergent reads preserved "
           "(P3.3 / #110) ===\n");
    RUN(unanimous_reads_have_no_weak_bits);
    RUN(divergent_byte_is_preserved_not_collapsed);
    RUN(failed_crc_read_cannot_outvote_a_verified_one);
    RUN(no_verified_read_still_records_divergence);
    RUN(vote_buffers_confidence_drops_on_divergence);
    RUN(guards_null_and_insufficient_passes);

    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
