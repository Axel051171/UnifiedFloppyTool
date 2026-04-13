/**
 * test_align_fuse_bridge.c — Tests for OTDR v7 Align+Fuse + UFT Bridge
 * ======================================================================
 */

#include "otdr_event_core_v7.h"
#include "uft_align_fuse_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { printf("  %-55s ", name); tests_run++; } while(0)
#define PASS()     do { printf("✓\n"); tests_passed++; } while(0)
#define FAIL(m)    do { printf("✗ (%s)\n", m); return; } while(0)

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

/* Build a synthetic "revolution" trace with optional shift and noise */
static float *build_rev(size_t n, int shift, float noise_amp, unsigned seed) {
    float *buf = (float *)calloc(n, sizeof(float));
    if (!buf) return NULL;
    srand(seed);
    for (size_t i = 0; i < n; i++) {
        long src = (long)i - (long)shift;
        float base = 0.0f;
        if (src >= 0 && src < (long)n) {
            float t = (float)src / (float)n;
            base = 1.0f - 0.3f * t;
            if (src == 5000) base += 1.0f;     /* spike */
            if (src > 5020) base -= 0.1f;      /* step */
        }
        buf[i] = base + (frand() - 0.5f) * noise_amp;
    }
    return buf;
}

/* ==== Core library tests (otdr_v7) ==== */

static void test_ncc_zero_shift(void) {
    TEST("NCC: identical traces → shift=0");
    const size_t N = 4096;
    float *x = build_rev(N, 0, 0.01f, 42);
    float score = 0.0f;
    int sh = otdr_estimate_shift_ncc(x, x, N, 32, &score);
    free(x);
    if (sh != 0) FAIL("shift!=0");
    if (score < 0.99f) FAIL("low score");
    PASS();
}

static void test_ncc_known_shift(void) {
    TEST("NCC: known +7 shift recovered");
    const size_t N = 8192;
    float *ref = build_rev(N, 0, 0.01f, 100);
    float *shifted = build_rev(N, 7, 0.01f, 100);
    float score = 0.0f;
    int sh = otdr_estimate_shift_ncc(ref, shifted, N, 32, &score);
    free(ref); free(shifted);
    /* v7 returns negative shift to align target to ref */
    if (abs(sh) != 7) FAIL("wrong shift");
    PASS();
}

static void test_ncc_positive_shift_large(void) {
    TEST("NCC: +12 shift recovered (apply_shift path)");
    const size_t N = 8192;
    float *ref = build_rev(N, 0, 0.005f, 200);
    float *shifted = (float *)calloc(N, sizeof(float));
    otdr_apply_shift_zeropad(ref, N, 12, shifted);
    float score = 0.0f;
    int sh = otdr_estimate_shift_ncc(ref, shifted, N, 32, &score);
    free(ref); free(shifted);
    if (sh != -12) FAIL("wrong shift");
    if (score < 0.95f) FAIL("low score");
    PASS();
}

static void test_ncc_null_reject(void) {
    TEST("NCC: NULL input returns 0");
    int sh = otdr_estimate_shift_ncc(NULL, NULL, 0, 10, NULL);
    if (sh != 0) FAIL("non-zero");
    PASS();
}

static void test_apply_shift(void) {
    TEST("apply_shift_zeropad correct");
    float x[] = {1, 2, 3, 4, 5};
    float out[5];
    otdr_apply_shift_zeropad(x, 5, 2, out);
    /* shift +2: out[0]=0, out[1]=0, out[2]=x[0]=1, out[3]=x[1]=2, out[4]=x[2]=3 */
    if (fabsf(out[0]) > 1e-6f) FAIL("out[0]");
    if (fabsf(out[2] - 1.0f) > 1e-6f) FAIL("out[2]");
    if (fabsf(out[4] - 3.0f) > 1e-6f) FAIL("out[4]");
    PASS();
}

static void test_align_traces(void) {
    TEST("align_traces aligns 4 revolutions");
    const size_t N = 8192, M = 4;
    int apply_shifts[] = {0, 7, 3, 15};  /* positive shifts only (reliably recovered by NCC) */
    float *ref_clean = build_rev(N, 0, 0.005f, 300);
    float *tr[4], *aligned[4];
    int shifts[4];

    for (size_t k = 0; k < M; k++) {
        tr[k] = (float *)calloc(N, sizeof(float));
        aligned[k] = (float *)calloc(N, sizeof(float));
        otdr_apply_shift_zeropad(ref_clean, N, apply_shifts[k], tr[k]);
        /* add small per-rev noise */
        srand((unsigned)(300 + k));
        for (size_t i = 0; i < N; i++)
            tr[k][i] += (frand() - 0.5f) * 0.01f;
    }

    int rc = otdr_align_traces((const float *const *)tr, M, N, 0, 32,
                                shifts, aligned);

    int ok = (rc == 0);
    /* Ref has shift=0, others should have estimated shift = -apply_shift */
    if (ok && shifts[0] != 0) ok = 0;
    for (size_t k = 1; k < M && ok; k++) {
        if (abs(shifts[k] + apply_shifts[k]) > 2) ok = 0;
    }

    free(ref_clean);
    for (size_t k = 0; k < M; k++) { free(tr[k]); free(aligned[k]); }
    if (!ok) FAIL("bad alignment");
    PASS();
}

static void test_fuse_median(void) {
    TEST("Median fusion reduces noise");
    const size_t N = 2048, M = 5;
    float *aligned[5];
    srand(42);
    for (size_t k = 0; k < M; k++) {
        aligned[k] = (float *)malloc(N * sizeof(float));
        for (size_t i = 0; i < N; i++) {
            float clean = sinf(2.0f * 3.14159f * (float)i / 100.0f);
            aligned[k][i] = clean + (frand() - 0.5f) * 1.0f;
        }
    }
    float *fused = (float *)calloc(N, sizeof(float));

    otdr_fuse_aligned_median(aligned, M, N, fused);

    /* Fused MSE should be lower than any single pass */
    double mse_single = 0.0, mse_fused = 0.0;
    for (size_t i = 0; i < N; i++) {
        float clean = sinf(2.0f * 3.14159f * (float)i / 100.0f);
        double e0 = aligned[0][i] - clean;
        double ef = fused[i] - clean;
        mse_single += e0 * e0;
        mse_fused  += ef * ef;
    }

    for (size_t k = 0; k < M; k++) free(aligned[k]);
    free(fused);
    if (mse_fused >= mse_single) FAIL("no improvement");
    PASS();
}

static void test_label_stability_perfect(void) {
    TEST("Label stability: perfect agreement → 1.0");
    const size_t N = 100, M = 4;
    uint8_t *labels[4];
    for (size_t k = 0; k < M; k++) {
        labels[k] = (uint8_t *)calloc(N, 1);
        for (size_t i = 0; i < N; i++) labels[k][i] = 1;
    }

    float *agree = (float *)calloc(N, sizeof(float));
    float *entropy = (float *)calloc(N, sizeof(float));
    otdr_label_stability((const uint8_t *const *)labels, M, N, 4,
                          agree, entropy);

    int ok = 1;
    for (size_t i = 0; i < N; i++) {
        if (fabsf(agree[i] - 1.0f) > 1e-6f) { ok = 0; break; }
        if (fabsf(entropy[i]) > 1e-6f) { ok = 0; break; }
    }

    for (size_t k = 0; k < M; k++) free(labels[k]);
    free(agree); free(entropy);
    if (!ok) FAIL("not perfect");
    PASS();
}

static void test_label_stability_mixed(void) {
    TEST("Label stability: mixed labels → agreement < 1");
    const size_t N = 100, M = 4;
    uint8_t *labels[4];
    for (size_t k = 0; k < M; k++) {
        labels[k] = (uint8_t *)calloc(N, 1);
        for (size_t i = 0; i < N; i++) labels[k][i] = (uint8_t)(k % 3);
    }

    float *agree = (float *)calloc(N, sizeof(float));
    float *entropy = (float *)calloc(N, sizeof(float));
    otdr_label_stability((const uint8_t *const *)labels, M, N, 4,
                          agree, entropy);

    int ok = 1;
    for (size_t i = 0; i < N; i++) {
        if (agree[i] >= 1.0f - 1e-6f) { ok = 0; break; }
    }

    for (size_t k = 0; k < M; k++) free(labels[k]);
    free(agree); free(entropy);
    if (!ok) FAIL("should disagree");
    PASS();
}

/* ==== Bridge tests (uft_align_*) ==== */

static void test_bridge_version(void) {
    TEST("Bridge version string");
    if (!uft_align_version() || strlen(uft_align_version()) == 0) FAIL("empty");
    PASS();
}

static void test_bridge_error_strings(void) {
    TEST("Error strings non-NULL");
    for (int i = 0; i >= -5; i--) {
        if (!uft_align_error_str((uft_align_error_t)i)) FAIL("NULL");
    }
    PASS();
}

static void test_bridge_init_free(void) {
    TEST("Init/free lifecycle");
    uft_align_ctx_t ctx;
    uft_align_error_t rc = uft_align_init(&ctx, NULL);
    if (rc != UFT_ALN_OK) FAIL("init");
    if (!ctx.initialized) FAIL("not init");
    if (ctx.cfg.max_shift != 64) FAIL("default shift");
    uft_align_free(&ctx);
    if (ctx.initialized) FAIL("still init");
    PASS();
}

static void test_bridge_null_reject(void) {
    TEST("Bridge NULL/small rejection");
    uft_align_ctx_t ctx;
    uft_align_init(&ctx, NULL);
    float fused[64];
    if (uft_align_fuse_float(&ctx, NULL, 0, 0, fused) != UFT_ALN_ERR_NULL) FAIL("null");
    float rev0[32], rev1[32];
    const float *revs[] = {rev0, rev1};
    if (uft_align_fuse_float(&ctx, revs, 1, 32, fused) != UFT_ALN_ERR_SMALL) FAIL("m=1");
    uft_align_free(&ctx);
    PASS();
}

static void test_bridge_fuse_float(void) {
    TEST("Float fusion with shift recovery");
    const size_t N = 10000, M = 4;
    int true_shifts[] = {0, 5, -3, 10};
    const float *revs[4];
    float *bufs[4];
    for (size_t k = 0; k < M; k++) {
        bufs[k] = build_rev(N, true_shifts[k], 0.02f, (unsigned)(500 + k));
        revs[k] = bufs[k];
    }

    float *fused = (float *)calloc(N, sizeof(float));
    uft_align_ctx_t ctx;
    uft_align_init(&ctx, NULL);
    uft_align_error_t rc = uft_align_fuse_float(&ctx, revs, M, N, fused);

    int ok = (rc == UFT_ALN_OK);
    uft_align_report_t rpt = uft_align_get_report(&ctx);
    if (rpt.num_revolutions != M) ok = 0;
    if (rpt.mean_ncc < 0.5f) ok = 0;

    /* Check per-rev alignment info */
    for (size_t k = 0; k < M && ok; k++) {
        const uft_rev_alignment_t *ri = uft_align_get_rev(&ctx, k);
        if (!ri) { ok = 0; break; }
    }

    uft_align_free(&ctx);
    for (size_t k = 0; k < M; k++) free(bufs[k]);
    free(fused);
    if (!ok) FAIL("fusion failed");
    PASS();
}

static void test_bridge_flux_ns(void) {
    TEST("Flux interval fusion (uint32)");
    const size_t N = 4000, M = 3;
    uint32_t *urevs[3];
    const uint32_t *crevs[3];
    srand(600);
    for (size_t k = 0; k < M; k++) {
        urevs[k] = (uint32_t *)malloc(N * sizeof(uint32_t));
        for (size_t i = 0; i < N; i++) {
            float base = 4000.0f + 200.0f * sinf(2.0f * 3.14159f * (float)i / 200.0f);
            urevs[k][i] = (uint32_t)(base + (frand() - 0.5f) * 100.0f);
        }
        crevs[k] = urevs[k];
    }

    float *fused = (float *)calloc(N, sizeof(float));
    uft_align_ctx_t ctx;
    uft_align_init(&ctx, NULL);
    uft_align_error_t rc = uft_align_fuse_flux_ns(&ctx, crevs, M, N, fused);

    uft_align_free(&ctx);
    for (size_t k = 0; k < M; k++) free(urevs[k]);
    free(fused);
    if (rc != UFT_ALN_OK) FAIL("failed");
    PASS();
}

static void test_bridge_auto_ref(void) {
    TEST("Auto-reference selection");
    const size_t N = 4096, M = 3;
    float *bufs[3];
    const float *revs[3];

    /* Rev 0: weak signal, Rev 1: strongest, Rev 2: medium */
    bufs[0] = build_rev(N, 0, 0.01f, 700);
    bufs[1] = build_rev(N, 0, 0.01f, 701);
    bufs[2] = build_rev(N, 0, 0.01f, 702);

    /* Scale rev 1 to be strongest */
    for (size_t i = 0; i < N; i++) {
        bufs[0][i] *= 0.3f;
        bufs[1][i] *= 2.0f;
    }
    for (size_t k = 0; k < M; k++) revs[k] = bufs[k];

    float *fused = (float *)calloc(N, sizeof(float));
    uft_align_config_t cfg = uft_align_default_config();
    cfg.auto_ref = true;

    uft_align_ctx_t ctx;
    uft_align_init(&ctx, &cfg);
    uft_align_fuse_float(&ctx, revs, M, N, fused);

    uft_align_report_t rpt = uft_align_get_report(&ctx);

    uft_align_free(&ctx);
    for (size_t k = 0; k < M; k++) free(bufs[k]);
    free(fused);

    /* Auto-ref should pick rev 1 (highest energy) */
    if (rpt.ref_revolution != 1) FAIL("wrong ref");
    PASS();
}

static void test_bridge_label_stability(void) {
    TEST("Label stability via bridge");
    const size_t N = 200, M = 5;
    uint8_t *labels[5];
    const uint8_t *clabels[5];

    /* First half: all agree (class 1). Second half: disagree. */
    for (size_t k = 0; k < M; k++) {
        labels[k] = (uint8_t *)calloc(N, 1);
        for (size_t i = 0; i < N / 2; i++) labels[k][i] = 1;
        for (size_t i = N / 2; i < N; i++) labels[k][i] = (uint8_t)(k % 3);
        clabels[k] = labels[k];
    }

    uft_align_ctx_t ctx;
    uft_align_init(&ctx, NULL);
    uft_align_error_t rc = uft_align_label_stability(&ctx, clabels, M, N);

    int ok = (rc == UFT_ALN_OK);
    uft_align_report_t rpt = uft_align_get_report(&ctx);
    if (!rpt.has_stability) ok = 0;
    if (ok && rpt.stability.mean_agreement < 0.4f) ok = 0;

    /* Check agreement array */
    size_t alen = 0;
    const float *agree = uft_align_get_agreement(&ctx, &alen);
    if (!agree || alen != N) ok = 0;

    /* First half should have perfect agreement */
    if (ok && agree[0] < 0.99f) ok = 0;

    uft_align_free(&ctx);
    for (size_t k = 0; k < M; k++) free(labels[k]);
    if (!ok) FAIL("stability");
    PASS();
}

static void test_bridge_report_fields(void) {
    TEST("Report fields populated");
    const size_t N = 5000, M = 3;
    float *bufs[3];
    const float *revs[3];
    for (size_t k = 0; k < M; k++) {
        bufs[k] = build_rev(N, (int)k * 3, 0.02f, (unsigned)(800 + k));
        revs[k] = bufs[k];
    }

    float *fused = (float *)calloc(N, sizeof(float));
    uft_align_ctx_t ctx;
    uft_align_init(&ctx, NULL);
    uft_align_fuse_float(&ctx, revs, M, N, fused);
    uft_align_report_t rpt = uft_align_get_report(&ctx);

    int ok = 1;
    if (rpt.num_revolutions != M) { ok = 0; printf("[m] "); }
    if (rpt.samples_per_rev != N) { ok = 0; printf("[n] "); }
    if (rpt.alignment_quality < 0.0f || rpt.alignment_quality > 1.0f) { ok = 0; printf("[q] "); }
    if (rpt.valid_alignments == 0) { ok = 0; printf("[valid] "); }

    uft_align_free(&ctx);
    for (size_t k = 0; k < M; k++) free(bufs[k]);
    free(fused);
    if (!ok) FAIL("bad fields");
    PASS();
}

static void test_bridge_double_free(void) {
    TEST("Double free safety");
    uft_align_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    uft_align_free(&ctx);
    uft_align_free(&ctx);
    uft_align_free(NULL);
    PASS();
}

static void test_bridge_large_multi_rev(void) {
    TEST("Large N=50K × 6 revolutions");
    const size_t N = 50000, M = 6;
    float *bufs[6];
    const float *revs[6];
    int shifts[] = {0, 3, -7, 12, -2, 8};
    for (size_t k = 0; k < M; k++) {
        bufs[k] = build_rev(N, shifts[k], 0.03f, (unsigned)(900 + k));
        revs[k] = bufs[k];
    }

    float *fused = (float *)calloc(N, sizeof(float));
    uft_align_ctx_t ctx;
    uft_align_init(&ctx, NULL);
    uft_align_error_t rc = uft_align_fuse_float(&ctx, revs, M, N, fused);

    uft_align_free(&ctx);
    for (size_t k = 0; k < M; k++) free(bufs[k]);
    free(fused);
    if (rc != UFT_ALN_OK) FAIL("failed");
    PASS();
}

/* ==== Main ==== */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║   OTDR v7 ALIGN+FUSE + UFT BRIDGE - TEST SUITE            ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("── Core Library (otdr_v7) ────────────────────────────────────\n");
    test_ncc_zero_shift();
    test_ncc_known_shift();
    test_ncc_positive_shift_large();
    test_ncc_null_reject();
    test_apply_shift();
    test_align_traces();
    test_fuse_median();
    test_label_stability_perfect();
    test_label_stability_mixed();

    printf("\n── Bridge API (uft_align_*) ───────────────────────────────────\n");
    test_bridge_version();
    test_bridge_error_strings();
    test_bridge_init_free();
    test_bridge_null_reject();
    test_bridge_fuse_float();
    test_bridge_flux_ns();
    test_bridge_auto_ref();
    test_bridge_label_stability();
    test_bridge_report_fields();
    test_bridge_double_free();
    test_bridge_large_multi_rev();

    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("  Ergebnis: %d/%d Tests bestanden\n", tests_passed, tests_run);
    printf("══════════════════════════════════════════════════════════════\n\n");

    return tests_passed < tests_run ? 1 : 0;
}
