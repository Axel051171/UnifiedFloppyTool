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

/* ── Klassifikation: was die Lesungen ZUEINANDER sagen (MF-473) ──────
 *
 * Nach a8rawconv `sift_sectors` (src/a8rawconv/disk.cpp:236-365). Die
 * Unterscheidung, die `has_weak_bits` allein nicht treffen kann:
 *
 *   ein Inhalt  + CRC ok      -> gesunder Sektor
 *   ein Inhalt  + CRC falsch  -> Kopierschutz, nicht weak, nicht rettbar
 *   mehrere     + CRC falsch  -> weak, ab gemeinsamem Praefix
 *   mehrere     + CRC ok      -> mehrdeutig (ungewoehnlich)
 */

TEST(stable_bad_crc_is_protection_not_weakness) {
    multiread_ctx_t *ctx = multiread_create(NULL);
    ASSERT(ctx != NULL);

    /* Drei identische Lesungen, keine mit gueltiger CRC. Genau das schreibt
     * ein Kopierschutz absichtlich aufs Medium: die CRC ist falsch, aber sie
     * ist JEDES MAL gleich falsch. Nochmal lesen bringt nichts Neues. */
    const uint8_t prot[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
    ASSERT(multiread_add_pass(ctx, prot, 8, 90, false) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, prot, 8, 90, false) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, prot, 8, 90, false) == MULTIREAD_OK);

    uint8_t out[8] = {0};
    multiread_sector_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(multiread_execute(ctx, out, 8, &res) == MULTIREAD_OK);

    ASSERT(res.class_ == MULTIREAD_CLASS_STABLE_BAD_CRC);
    ASSERT(res.distinct_contents == 1);
    ASSERT(res.weak_offset == -1);          /* nichts laeuft auseinander */
    ASSERT(res.has_weak_bits == false);     /* stabil ist nicht weak */
    ASSERT(res.recovered == false);         /* MF-466: nichts hat es geprueft */
    ASSERT(memcmp(out, prot, 8) == 0);      /* Daten trotzdem da */

    free(res.weak_mask);
    multiread_destroy(ctx);
}

TEST(weak_offset_is_the_common_prefix_of_all_reads) {
    multiread_ctx_t *ctx = multiread_create(NULL);
    ASSERT(ctx != NULL);

    /* Drei Lesungen ohne gueltige CRC, die ab Byte 4 auseinanderlaufen —
     * aber unterschiedlich weit: eine weicht erst ab 6 ab, eine ab 4. Der
     * Weak-Offset ist das LAENGSTE GEMEINSAME PRAEFIX ueber alle, also 4,
     * nicht der Abstand zum ersten Vergleichspartner (disk.cpp:331-343). */
    const uint8_t a[8] = { 1, 2, 3, 4, 0xAA, 0xAA, 0xAA, 0xAA };
    const uint8_t b[8] = { 1, 2, 3, 4, 0xAA, 0xAA, 0xBB, 0xBB };  /* ab 6 */
    const uint8_t c[8] = { 1, 2, 3, 4, 0xCC, 0xCC, 0xCC, 0xCC };  /* ab 4 */
    ASSERT(multiread_add_pass(ctx, a, 8, 50, false) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, b, 8, 50, false) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, c, 8, 50, false) == MULTIREAD_OK);

    uint8_t out[8] = {0};
    multiread_sector_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(multiread_execute(ctx, out, 8, &res) == MULTIREAD_OK);

    ASSERT(res.class_ == MULTIREAD_CLASS_WEAK);
    ASSERT(res.weak_offset == 4);
    ASSERT(res.distinct_contents >= 2);
    ASSERT(res.has_weak_bits == true);

    /* Der Offset hat dieselbe Bedeutung wie ein ATX-Weak-Chunk: alles davor
     * ist fest, alles danach unsicher. Die ersten vier Bytes muessen also
     * unveraendert durchkommen. */
    ASSERT(out[0] == 1 && out[1] == 2 && out[2] == 3 && out[3] == 4);

    free(res.weak_mask);
    multiread_destroy(ctx);
}

TEST(one_verified_read_makes_the_sector_stable_good) {
    multiread_ctx_t *ctx = multiread_create(NULL);
    ASSERT(ctx != NULL);

    /* Eine gepruefte Lesung, zwei ungepruefte mit ANDEREM Inhalt. Die
     * CRC-Vorauswahl wirft die beiden weg (disk.cpp:240-254), also bleibt
     * ein Inhalt uebrig — der Sektor ist stabil und gut, nicht weak.
     * Ohne die Vorauswahl waere er faelschlich "mehrdeutig". */
    const uint8_t good[8] = { 9, 9, 9, 9, 9, 9, 9, 9 };
    const uint8_t junk[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    ASSERT(multiread_add_pass(ctx, good, 8, 100, true)  == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, junk, 8,  40, false) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, junk, 8,  40, false) == MULTIREAD_OK);

    uint8_t out[8] = {0};
    multiread_sector_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(multiread_execute(ctx, out, 8, &res) == MULTIREAD_OK);

    ASSERT(res.class_ == MULTIREAD_CLASS_STABLE_GOOD);
    ASSERT(res.distinct_contents == 1);
    ASSERT(res.weak_offset == -1);
    ASSERT(res.recovered == true);
    ASSERT(memcmp(out, good, 8) == 0);      /* die gepruefte gewinnt */

    free(res.weak_mask);
    multiread_destroy(ctx);
}

TEST(differing_content_despite_good_crc_is_its_own_class) {
    multiread_ctx_t *ctx = multiread_create(NULL);
    ASSERT(ctx != NULL);

    /* Zwei Lesungen, beide CRC-geprueft, aber mit verschiedenem Inhalt.
     * Physikalisch sehr ungewoehnlich; a8rawconv warnt und behaelt eine
     * (disk.cpp:320-328). Es unter "weak" zu verbuchen waere falsch — die
     * Ursache ist eine andere, und wer die Klassen fuer eine Diagnose
     * benutzt, braucht sie getrennt. */
    const uint8_t x[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    const uint8_t y[4] = { 0xDE, 0xAD, 0xC0, 0xDE };
    /* Drei Lesungen, weil die Voreinstellung min_passes = 3 verlangt —
     * multiread_execute() lehnt weniger mit INSUFFICIENT_PASSES ab. */
    ASSERT(multiread_add_pass(ctx, x, 4, 100, true) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, y, 4, 100, true) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, y, 4, 100, true) == MULTIREAD_OK);

    uint8_t out[4] = {0};
    multiread_sector_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(multiread_execute(ctx, out, 4, &res) == MULTIREAD_OK);

    ASSERT(res.class_ == MULTIREAD_CLASS_AMBIGUOUS_GOOD);
    ASSERT(res.distinct_contents >= 2);
    ASSERT(res.weak_offset == -1);          /* nur bei WEAK gesetzt */

    free(res.weak_mask);
    multiread_destroy(ctx);
}

TEST(class_names_are_all_present) {
    /* Ein Name je Klasse, keiner NULL, keiner doppelt — die Klasse wird in
     * Berichten ausgegeben, und "unbestimmt" fuer eine bestimmte Klasse
     * waere eine stille Falschaussage. */
    const multiread_class_t all[] = {
        MULTIREAD_CLASS_UNKNOWN, MULTIREAD_CLASS_STABLE_GOOD,
        MULTIREAD_CLASS_STABLE_BAD_CRC, MULTIREAD_CLASS_WEAK,
        MULTIREAD_CLASS_AMBIGUOUS_GOOD
    };
    for (size_t i = 0; i < sizeof(all)/sizeof(all[0]); i++) {
        const char *n = multiread_class_name(all[i]);
        ASSERT(n != NULL && n[0] != '\0');
        for (size_t j = 0; j < i; j++)
            ASSERT(strcmp(n, multiread_class_name(all[j])) != 0);
    }
}

/* ── stable but never verified: data yes, claim no ── */

TEST(stable_reads_without_a_verified_crc_are_not_recovered) {
    multiread_ctx_t *ctx = multiread_create(NULL);
    ASSERT(ctx != NULL);

    /* Three reads that agree byte for byte, and not one of them verified.
     * That is exactly what a deliberately CRC-broken protection sector looks
     * like: perfectly stable, and unreadable by design. Agreement drives the
     * confidence to 100, so before MF-466 the sector came back as
     * `recovered = true` with `good_reads == 0` — a confident claim about
     * data nothing had checked.
     *
     * The data must still be handed over. Only the claim is withheld. */
    const uint8_t stable[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    ASSERT(multiread_add_pass(ctx, stable, 4, 90, /*crc_ok=*/false) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, stable, 4, 90, /*crc_ok=*/false) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, stable, 4, 90, /*crc_ok=*/false) == MULTIREAD_OK);

    uint8_t out[4] = {0};
    multiread_sector_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(multiread_execute(ctx, out, 4, &res) == MULTIREAD_OK);

    ASSERT(memcmp(out, stable, 4) == 0);   /* nothing was thrown away */
    ASSERT(res.good_reads == 0);           /* nothing verified it either */
    ASSERT(res.has_weak_bits == false);    /* it is stable, not weak */
    ASSERT(res.confidence >= 90);          /* the reads DO agree */
    ASSERT(res.recovered == false);        /* ... and that is not recovery */

    free(res.weak_mask);
    multiread_destroy(ctx);
}

TEST(one_verified_read_is_enough_to_claim_recovery) {
    multiread_ctx_t *ctx = multiread_create(NULL);
    ASSERT(ctx != NULL);

    /* The other side of the same rule: one pass verified, so the claim holds. */
    const uint8_t good[4] = { 0x01, 0x02, 0x03, 0x04 };
    ASSERT(multiread_add_pass(ctx, good, 4, 100, /*crc_ok=*/true)  == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, good, 4,  90, /*crc_ok=*/false) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, good, 4,  90, /*crc_ok=*/false) == MULTIREAD_OK);

    uint8_t out[4] = {0};
    multiread_sector_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(multiread_execute(ctx, out, 4, &res) == MULTIREAD_OK);

    ASSERT(memcmp(out, good, 4) == 0);
    ASSERT(res.good_reads == 1);
    ASSERT(res.recovered == true);

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
    RUN(stable_reads_without_a_verified_crc_are_not_recovered);
    RUN(one_verified_read_is_enough_to_claim_recovery);
    RUN(stable_bad_crc_is_protection_not_weakness);
    RUN(weak_offset_is_the_common_prefix_of_all_reads);
    RUN(one_verified_read_makes_the_sector_stable_good);
    RUN(differing_content_despite_good_crc_is_its_own_class);
    RUN(class_names_are_all_present);
    RUN(vote_buffers_confidence_drops_on_divergence);
    RUN(guards_null_and_insufficient_passes);

    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
