/**
 * test_confidence_v10.c — Tests for OTDR v10 Confidence + UFT Bridge
 */

#include "otdr_event_core_v10.h"
#include "uft_confidence_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int t_run = 0, t_pass = 0;
#define TEST(n) do { printf("  %-55s ", n); t_run++; } while(0)
#define PASS()  do { printf("✓\n"); t_pass++; } while(0)
#define FAIL(m) do { printf("✗ (%s)\n", m); return; } while(0)

/* ═══════════ Input generators ═══════════ */

/* Perfect agreement, high SNR, clean flags → max confidence */
static void gen_perfect(size_t n, float *agree, float *snr, uint8_t *flags) {
    for (size_t i = 0; i < n; i++) {
        agree[i] = 1.0f;
        snr[i] = 30.0f;
        flags[i] = 0;
    }
}

/* Mixed: good + bad region */
static void gen_mixed(size_t n, float *agree, float *snr, uint8_t *flags,
                      size_t bad_start, size_t bad_len) {
    for (size_t i = 0; i < n; i++) {
        agree[i] = 0.9f;
        snr[i] = 25.0f;
        flags[i] = 0;
    }
    for (size_t i = bad_start; i < bad_start + bad_len && i < n; i++) {
        agree[i] = 0.2f;
        snr[i] = -5.0f;
        flags[i] = 0x01;  /* DROPOUT flag */
    }
}

/* Gradient: confidence decreases across trace */
static void gen_gradient(size_t n, float *agree, float *snr, uint8_t *flags) {
    for (size_t i = 0; i < n; i++) {
        float t = (float)i / (float)(n - 1);
        agree[i] = 1.0f - 0.8f * t;
        snr[i] = 35.0f - 50.0f * t;
        flags[i] = (t > 0.8f) ? 0x01 : 0;
    }
}

/* ═══════════ Core v10 tests ═══════════ */

static void test_v10_defaults(void) {
    TEST("v10 default config valid");
    otdr10_config_t c = otdr10_default_config();
    float wsum = c.w_agreement + c.w_snr + c.w_integrity;
    if (fabsf(wsum - 1.0f) > 0.01f) FAIL("weights");
    if (c.snr_floor_db >= c.snr_ceil_db) FAIL("snr range");
    PASS();
}

static void test_v10_null_reject(void) {
    TEST("v10 NULL/zero rejection");
    otdr10_sample_t s;
    if (otdr10_compute(NULL, NULL, NULL, 0, NULL, &s) >= 0) FAIL("n=0");
    if (otdr10_compute(NULL, NULL, NULL, 100, NULL, NULL) >= 0) FAIL("null out");
    PASS();
}

static void test_v10_perfect_high_conf(void) {
    TEST("Perfect inputs → confidence ≈ 1.0");
    const size_t N = 1000;
    float *agree = malloc(N * sizeof(float));
    float *snr = malloc(N * sizeof(float));
    uint8_t *flags = calloc(N, 1);
    gen_perfect(N, agree, snr, flags);

    otdr10_sample_t *out = calloc(N, sizeof(otdr10_sample_t));
    otdr10_compute(agree, snr, flags, N, NULL, out);

    int ok = (out[500].confidence > 0.85f);

    free(agree); free(snr); free(flags); free(out);
    if (!ok) FAIL("low conf");
    PASS();
}

static void test_v10_flagged_low_conf(void) {
    TEST("Flagged samples → low confidence");
    const size_t N = 500;
    float *agree = malloc(N * sizeof(float));
    float *snr = malloc(N * sizeof(float));
    uint8_t *flags = calloc(N, 1);

    for (size_t i = 0; i < N; i++) {
        agree[i] = 0.1f;
        snr[i] = -5.0f;
        flags[i] = 0x01;  /* dropout */
    }

    otdr10_sample_t *out = calloc(N, sizeof(otdr10_sample_t));
    otdr10_compute(agree, snr, flags, N, NULL, out);

    int ok = (out[250].confidence < 0.3f);

    free(agree); free(snr); free(flags); free(out);
    if (!ok) FAIL("not low");
    PASS();
}

static void test_v10_null_inputs_defaults(void) {
    TEST("NULL inputs use defaults");
    const size_t N = 200;
    otdr10_sample_t *out = calloc(N, sizeof(otdr10_sample_t));
    int rc = otdr10_compute(NULL, NULL, NULL, N, NULL, out);

    int ok = (rc == 0);
    /* With defaults: agree=0.5, snr=10dB (→normalized ~0.4), integ=1.0 */
    if (out[100].confidence < 0.3f || out[100].confidence > 0.8f) ok = 0;

    free(out);
    if (!ok) FAIL("bad defaults");
    PASS();
}

static void test_v10_segment_ranking(void) {
    TEST("Segments ranked: good > bad");
    const size_t N = 2000;
    float *agree = malloc(N * sizeof(float));
    float *snr = malloc(N * sizeof(float));
    uint8_t *flags = calloc(N, 1);
    gen_mixed(N, agree, snr, flags, 800, 400);

    otdr10_sample_t *out = calloc(N, sizeof(otdr10_sample_t));
    otdr10_compute(agree, snr, flags, N, NULL, out);

    otdr10_segment_t segs[256];
    size_t nseg = otdr10_segment_rank(out, N, NULL, segs, 256);

    int ok = (nseg >= 2);
    /* Rank 0 = best segment, should have high confidence */
    if (ok && segs[0].mean_confidence < segs[nseg - 1].mean_confidence)
        ok = 0;

    free(agree); free(snr); free(flags); free(out);
    if (!ok) FAIL("bad ranking");
    PASS();
}

static void test_v10_summary(void) {
    TEST("Summary statistics computed");
    const size_t N = 1000;
    float *agree = malloc(N * sizeof(float));
    float *snr = malloc(N * sizeof(float));
    uint8_t *flags = calloc(N, 1);
    gen_gradient(N, agree, snr, flags);

    otdr10_sample_t *out = calloc(N, sizeof(otdr10_sample_t));
    otdr10_compute(agree, snr, flags, N, NULL, out);

    otdr10_segment_t segs[256];
    size_t nseg = otdr10_segment_rank(out, N, NULL, segs, 256);

    otdr10_summary_t summ;
    otdr10_summarize(out, N, segs, nseg, &summ);

    int ok = (summ.n == N);
    if (summ.mean_confidence <= 0.0f || summ.mean_confidence >= 1.0f) ok = 0;
    if (summ.min_confidence > summ.max_confidence) ok = 0;
    if (summ.high_conf_count + summ.mid_conf_count + summ.low_conf_count != N) ok = 0;

    free(agree); free(snr); free(flags); free(out);
    if (!ok) FAIL("bad summary");
    PASS();
}

static void test_v10_custom_weights(void) {
    TEST("Custom weights: SNR-only mode");
    const size_t N = 200;
    float agree[200], snr[200];
    uint8_t flags[200];
    for (size_t i = 0; i < N; i++) {
        agree[i] = 0.0f;   /* zero agreement */
        snr[i] = 30.0f;    /* high SNR */
        flags[i] = 0x01;   /* flagged */
    }

    otdr10_config_t cfg = otdr10_default_config();
    cfg.w_agreement = 0.0f;
    cfg.w_snr       = 1.0f;
    cfg.w_integrity = 0.0f;

    otdr10_sample_t *out = calloc(N, sizeof(otdr10_sample_t));
    otdr10_compute(agree, snr, flags, N, &cfg, out);

    /* Should be high because only SNR matters */
    int ok = (out[100].confidence > 0.7f);
    /* agree_comp should be 0 */
    if (out[100].agree_comp > 0.01f) ok = 0;

    free(out);
    if (!ok) FAIL("weights wrong");
    PASS();
}

/* ═══════════ Bridge tests ═══════════ */

static void test_br_version(void) {
    TEST("Bridge version");
    if (!uft_conf_version() || strlen(uft_conf_version()) == 0) FAIL("empty");
    PASS();
}

static void test_br_error_strings(void) {
    TEST("Error strings");
    for (int i = 0; i >= -4; i--)
        if (!uft_conf_error_str((uft_conf_error_t)i)) FAIL("null");
    PASS();
}

static void test_br_band_strings(void) {
    TEST("Band strings: HIGH/MID/LOW");
    if (strcmp(uft_conf_band_str(UFT_CONF_HIGH), "HIGH") != 0) FAIL("high");
    if (strcmp(uft_conf_band_str(UFT_CONF_MID), "MID") != 0) FAIL("mid");
    if (strcmp(uft_conf_band_str(UFT_CONF_LOW), "LOW") != 0) FAIL("low");
    PASS();
}

static void test_br_init_free(void) {
    TEST("Init/free lifecycle");
    uft_conf_ctx_t ctx;
    if (uft_conf_init(&ctx, NULL) != UFT_CONF_OK) FAIL("init");
    if (!ctx.initialized) FAIL("not init");
    uft_conf_free(&ctx);
    if (ctx.initialized) FAIL("still init");
    PASS();
}

static void test_br_null_reject(void) {
    TEST("Bridge NULL/small rejection");
    uft_conf_ctx_t ctx;
    uft_conf_init(&ctx, NULL);
    if (uft_conf_compute(&ctx, NULL, NULL, NULL, 1) != UFT_CONF_ERR_SMALL) FAIL("small");
    uft_conf_free(&ctx);
    PASS();
}

static void test_br_perfect(void) {
    TEST("Bridge: perfect → HIGH band");
    const size_t N = 1000;
    float *agree = malloc(N * sizeof(float));
    float *snr = malloc(N * sizeof(float));
    uint8_t *flags = calloc(N, 1);
    gen_perfect(N, agree, snr, flags);

    uft_conf_ctx_t ctx;
    uft_conf_init(&ctx, NULL);
    uft_conf_compute(&ctx, agree, snr, flags, N);

    const uft_conf_sample_t *s = uft_conf_get_sample(&ctx, 500);
    int ok = (s && s->band == UFT_CONF_HIGH && s->confidence > 0.85f);

    uft_conf_free(&ctx);
    free(agree); free(snr); free(flags);
    if (!ok) FAIL("not high");
    PASS();
}

static void test_br_mixed_bands(void) {
    TEST("Bridge: mixed → both HIGH and LOW bands");
    const size_t N = 2000;
    float *agree = malloc(N * sizeof(float));
    float *snr = malloc(N * sizeof(float));
    uint8_t *flags = calloc(N, 1);
    gen_mixed(N, agree, snr, flags, 800, 400);

    uft_conf_ctx_t ctx;
    uft_conf_init(&ctx, NULL);
    uft_conf_compute(&ctx, agree, snr, flags, N);

    size_t high = uft_conf_count_band(&ctx, UFT_CONF_HIGH);
    size_t low = uft_conf_count_band(&ctx, UFT_CONF_LOW);

    uft_conf_free(&ctx);
    free(agree); free(snr); free(flags);
    if (high == 0) FAIL("no high");
    if (low == 0) FAIL("no low");
    PASS();
}

static void test_br_segments_ranked(void) {
    TEST("Bridge: segments ranked best→worst");
    const size_t N = 4000;
    float *agree = malloc(N * sizeof(float));
    float *snr = malloc(N * sizeof(float));
    uint8_t *flags = calloc(N, 1);
    gen_mixed(N, agree, snr, flags, 1500, 1000);

    uft_conf_ctx_t ctx;
    uft_conf_init(&ctx, NULL);
    uft_conf_compute(&ctx, agree, snr, flags, N);

    size_t nseg = uft_conf_segment_count(&ctx);
    int ok = (nseg >= 2);

    if (ok) {
        const uft_conf_segment_t *s0 = uft_conf_get_segment(&ctx, 0);
        const uft_conf_segment_t *sn = uft_conf_get_segment(&ctx, nseg - 1);
        if (s0->mean_confidence < sn->mean_confidence) ok = 0;
        if (s0->rank != 0) ok = 0;
    }

    uft_conf_free(&ctx);
    free(agree); free(snr); free(flags);
    if (!ok) FAIL("bad ranking");
    PASS();
}

static void test_br_null_inputs(void) {
    TEST("Bridge: NULL inputs use defaults");
    const size_t N = 500;
    uft_conf_ctx_t ctx;
    uft_conf_init(&ctx, NULL);
    uft_conf_error_t rc = uft_conf_compute(&ctx, NULL, NULL, NULL, N);

    int ok = (rc == UFT_CONF_OK);
    if (ok && uft_conf_sample_count(&ctx) != N) ok = 0;
    const uft_conf_sample_t *s = uft_conf_get_sample(&ctx, 250);
    if (ok && (!s || s->confidence <= 0.0f)) ok = 0;

    uft_conf_free(&ctx);
    if (!ok) FAIL("failed");
    PASS();
}

static void test_br_report_fields(void) {
    TEST("Report fields populated");
    const size_t N = 2000;
    float *agree = malloc(N * sizeof(float));
    float *snr = malloc(N * sizeof(float));
    uint8_t *flags = calloc(N, 1);
    gen_gradient(N, agree, snr, flags);

    uft_conf_ctx_t ctx;
    uft_conf_init(&ctx, NULL);
    uft_conf_compute(&ctx, agree, snr, flags, N);

    uft_conf_report_t rpt = uft_conf_get_report(&ctx);

    int ok = (rpt.samples_analyzed == N);
    if (rpt.mean_confidence <= 0.0f) ok = 0;
    if (rpt.min_confidence > rpt.max_confidence) ok = 0;
    if (rpt.high_count + rpt.mid_count + rpt.low_count != N) ok = 0;
    if (rpt.overall_quality < 0.0f || rpt.overall_quality > 1.0f) ok = 0;

    uft_conf_free(&ctx);
    free(agree); free(snr); free(flags);
    if (!ok) FAIL("bad fields");
    PASS();
}

static void test_br_count_band(void) {
    TEST("count_band consistent with report");
    const size_t N = 1000;
    float *agree = malloc(N * sizeof(float));
    float *snr = malloc(N * sizeof(float));
    uint8_t *flags = calloc(N, 1);
    gen_gradient(N, agree, snr, flags);

    uft_conf_ctx_t ctx;
    uft_conf_init(&ctx, NULL);
    uft_conf_compute(&ctx, agree, snr, flags, N);

    uft_conf_report_t rpt = uft_conf_get_report(&ctx);
    size_t h = uft_conf_count_band(&ctx, UFT_CONF_HIGH);
    size_t m = uft_conf_count_band(&ctx, UFT_CONF_MID);
    size_t l = uft_conf_count_band(&ctx, UFT_CONF_LOW);

    uft_conf_free(&ctx);
    free(agree); free(snr); free(flags);
    if (h != rpt.high_count) FAIL("high mismatch");
    if (m != rpt.mid_count) FAIL("mid mismatch");
    if (l != rpt.low_count) FAIL("low mismatch");
    PASS();
}

static void test_br_custom_weights(void) {
    TEST("Bridge: custom weights applied");
    const size_t N = 300;
    float agree[300], snr[300];
    uint8_t flags[300];
    for (size_t i = 0; i < N; i++) {
        agree[i] = 1.0f; snr[i] = -20.0f; flags[i] = 0x01;
    }

    /* Agreement-only mode */
    uft_conf_config_t cfg = uft_conf_default_config();
    cfg.w_agreement = 1.0f;
    cfg.w_snr = 0.0f;
    cfg.w_integrity = 0.0f;

    uft_conf_ctx_t ctx;
    uft_conf_init(&ctx, &cfg);
    uft_conf_compute(&ctx, agree, snr, flags, N);

    const uft_conf_sample_t *s = uft_conf_get_sample(&ctx, 150);
    int ok = (s && s->confidence > 0.9f);  /* agree=1.0 → high */

    uft_conf_free(&ctx);
    if (!ok) FAIL("weights not applied");
    PASS();
}

static void test_br_double_free(void) {
    TEST("Double free safety");
    uft_conf_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    uft_conf_free(&ctx);
    uft_conf_free(&ctx);
    uft_conf_free(NULL);
    PASS();
}

static void test_br_large_n(void) {
    TEST("N=500K performance");
    const size_t N = 500000;
    float *agree = malloc(N * sizeof(float));
    float *snr = malloc(N * sizeof(float));
    uint8_t *flags = calloc(N, 1);
    gen_gradient(N, agree, snr, flags);

    uft_conf_ctx_t ctx;
    uft_conf_init(&ctx, NULL);
    uft_conf_error_t rc = uft_conf_compute(&ctx, agree, snr, flags, N);

    uft_conf_free(&ctx);
    free(agree); free(snr); free(flags);
    if (rc != UFT_CONF_OK) FAIL("failed");
    PASS();
}

/* ═══════════ Main ═══════════ */

int main(void) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║   OTDR v10 CONFIDENCE MAP + UFT BRIDGE - TEST SUITE        ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("── Core v10 (otdr10_*) ───────────────────────────────────────\n");
    test_v10_defaults();
    test_v10_null_reject();
    test_v10_perfect_high_conf();
    test_v10_flagged_low_conf();
    test_v10_null_inputs_defaults();
    test_v10_segment_ranking();
    test_v10_summary();
    test_v10_custom_weights();

    printf("\n── Bridge (uft_conf_*) ───────────────────────────────────────\n");
    test_br_version();
    test_br_error_strings();
    test_br_band_strings();
    test_br_init_free();
    test_br_null_reject();
    test_br_perfect();
    test_br_mixed_bands();
    test_br_segments_ranked();
    test_br_null_inputs();
    test_br_report_fields();
    test_br_count_band();
    test_br_custom_weights();
    test_br_double_free();
    test_br_large_n();

    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("  Ergebnis: %d/%d Tests bestanden\n", t_pass, t_run);
    printf("══════════════════════════════════════════════════════════════\n\n");
    return t_pass < t_run ? 1 : 0;
}
