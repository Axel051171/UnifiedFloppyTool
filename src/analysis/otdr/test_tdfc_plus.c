/*
 * test_tdfc_plus.c — Tests for TDFC+ extensions
 * Robust stats, envelope dropout, amplitude dropout, segmentation
 */
#include "tdfc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0, tests_pass = 0;
#define T(name) do { \
    tests_run++; \
    printf("  [%2d] %-52s ", tests_run, name); \
} while(0)
#define PASS() do { tests_pass++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)
#define NEAR(a,b,eps) (fabsf((a)-(b)) < (eps))

/* Generate test signal: clean sine with optional dropouts */
static float *gen_signal(size_t n, float amp, int dropout_start, int dropout_len, float drop_amp) {
    float *x = (float*)calloc(n, sizeof(float));
    for (size_t i = 0; i < n; i++) {
        x[i] = amp * sinf(2.0f * 3.14159f * (float)i / 200.0f);
        if (dropout_start >= 0 && (int)i >= dropout_start && (int)i < dropout_start + dropout_len)
            x[i] = drop_amp * sinf(2.0f * 3.14159f * (float)i / 200.0f);
    }
    return x;
}

/* ---- Robust Stats Tests ---- */
static void test_robust_stats_basic(void) {
    T("Robust stats: median, MAD, trimmed mean");
    float data[] = {1,2,3,4,5,6,7,8,9,10};
    tdfc_robust_stats_t rs;
    int rc = tdfc_compute_robust_stats(data, 10, 0.10f, &rs);
    if (rc != 0) { FAIL("rc != 0"); return; }
    if (!NEAR(rs.median, 5.5f, 0.01f)) { FAIL("median"); return; }
    if (rs.mad < 1.0f || rs.mad > 3.0f) { FAIL("MAD range"); return; }
    if (rs.sigma_mad < 1.0f) { FAIL("sigma_mad"); return; }
    if (rs.trimmed_mean < 4.0f || rs.trimmed_mean > 7.0f) { FAIL("trimmed_mean"); return; }
    PASS();
}

static void test_robust_stats_outlier(void) {
    T("Robust stats: outlier resistance");
    float data[] = {1,2,3,4,5,6,7,8,9,1000};
    tdfc_robust_stats_t rs;
    tdfc_compute_robust_stats(data, 10, 0.20f, &rs);
    /* Median should be ~5.5, not pulled by outlier */
    if (!NEAR(rs.median, 5.5f, 0.01f)) { FAIL("median with outlier"); return; }
    /* Trimmed mean (20%) should exclude outlier */
    if (rs.trimmed_mean > 10.0f) { FAIL("trimmed_mean should exclude outlier"); return; }
    PASS();
}

static void test_robust_stats_null(void) {
    T("Robust stats: NULL safety");
    tdfc_robust_stats_t rs;
    if (tdfc_compute_robust_stats(NULL, 10, 0.1f, &rs) != -2) { FAIL("NULL input"); return; }
    if (tdfc_compute_robust_stats(NULL, 0, 0.1f, &rs) != -2) { FAIL("zero length"); return; }
    if (tdfc_compute_robust_stats(NULL, 10, 0.1f, NULL) != -1) { FAIL("NULL out"); return; }
    PASS();
}

/* ---- Envelope Dropout Tests ---- */
static void test_dropout_env_none(void) {
    T("Dropout ENV: no dropouts in clean signal");
    float env[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    uint8_t flags[8];
    float ratio = -1.0f;
    int rc = tdfc_detect_dropouts_env(env, 8, 0.1f, 3, flags, &ratio);
    if (rc != 0) { FAIL("rc"); return; }
    if (ratio > 0.001f) { FAIL("ratio should be 0"); return; }
    for (int i = 0; i < 8; i++) if (flags[i]) { FAIL("no flags expected"); return; }
    PASS();
}

static void test_dropout_env_detect(void) {
    T("Dropout ENV: detect low-energy run");
    float env[] = {0.5f, 0.5f, 0.01f, 0.01f, 0.01f, 0.01f, 0.5f, 0.5f};
    uint8_t flags[8];
    float ratio = -1.0f;
    tdfc_detect_dropouts_env(env, 8, 0.1f, 3, flags, &ratio);
    /* Points 2-5 should be flagged */
    if (!flags[2] || !flags[3] || !flags[4] || !flags[5]) { FAIL("dropout not flagged"); return; }
    if (flags[0] || flags[1] || flags[6] || flags[7]) { FAIL("clean points flagged"); return; }
    if (!NEAR(ratio, 0.5f, 0.01f)) { FAIL("ratio"); return; }
    PASS();
}

static void test_dropout_env_minrun(void) {
    T("Dropout ENV: min_run filtering");
    float env[] = {0.5f, 0.01f, 0.01f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    uint8_t flags[8];
    float ratio;
    tdfc_detect_dropouts_env(env, 8, 0.1f, 3, flags, &ratio);
    /* Run of 2 < min_run=3, should NOT be flagged */
    if (flags[1] || flags[2]) { FAIL("short run should not be flagged"); return; }
    PASS();
}

/* ---- Amplitude Dropout Tests ---- */
static void test_dropout_amp_basic(void) {
    T("Dropout AMP: detect raw amplitude dropout");
    size_t N = 10000;
    float *sig = gen_signal(N, 0.8f, 4000, 600, 0.01f);
    size_t step = 64;
    size_t n_points = (N - 1) / step + 1;
    uint8_t *pts = (uint8_t*)calloc(n_points, 1);
    uint8_t *samps = (uint8_t*)calloc(N, 1);
    float ratio = 0;

    int rc = tdfc_detect_dropouts_amp(sig, N, step, 0.05f, 400, samps, pts, &ratio);
    if (rc != 0) { FAIL("rc"); free(sig); free(pts); free(samps); return; }

    /* Check sample-level flags around the dropout zone */
    int found_sample_flag = 0;
    for (size_t i = 4100; i < 4500; i++) if (samps[i]) found_sample_flag = 1;
    if (!found_sample_flag) { FAIL("no sample flags in dropout zone"); free(sig); free(pts); free(samps); return; }

    /* Check point-level flags */
    int found_point_flag = 0;
    size_t p_start = 4000 / step, p_end = 4600 / step;
    for (size_t p = p_start; p <= p_end && p < n_points; p++)
        if (pts[p]) found_point_flag = 1;
    if (!found_point_flag) { FAIL("no point flags in dropout zone"); free(sig); free(pts); free(samps); return; }

    if (ratio < 0.001f) { FAIL("ratio too low"); free(sig); free(pts); free(samps); return; }

    free(sig); free(pts); free(samps);
    PASS();
}

static void test_dropout_amp_clean(void) {
    T("Dropout AMP: no false positives on clean signal");
    size_t N = 10000;
    float *sig = gen_signal(N, 0.8f, -1, 0, 0);
    size_t step = 64;
    size_t n_points = (N - 1) / step + 1;
    uint8_t *pts = (uint8_t*)calloc(n_points, 1);
    float ratio = 1.0f;

    tdfc_detect_dropouts_amp(sig, N, step, 0.05f, 400, NULL, pts, &ratio);
    if (ratio > 0.001f) { FAIL("false positives"); free(sig); free(pts); return; }
    free(sig); free(pts);
    PASS();
}

static void test_dropout_amp_short_run(void) {
    T("Dropout AMP: short run below min_run_samples filtered");
    size_t N = 10000;
    float *sig = gen_signal(N, 0.8f, 5000, 100, 0.01f); /* 100 < min_run=400 */
    size_t step = 64;
    size_t n_points = (N - 1) / step + 1;
    uint8_t *pts = (uint8_t*)calloc(n_points, 1);
    float ratio = 1.0f;

    tdfc_detect_dropouts_amp(sig, N, step, 0.05f, 400, NULL, pts, &ratio);
    if (ratio > 0.001f) { FAIL("short run should be filtered"); free(sig); free(pts); return; }
    free(sig); free(pts);
    PASS();
}

static void test_dropout_amp_null(void) {
    T("Dropout AMP: NULL safety");
    uint8_t pts[10];
    if (tdfc_detect_dropouts_amp(NULL, 100, 64, 0.1f, 10, NULL, pts, NULL) != -1) { FAIL("null sig"); return; }
    float sig[10] = {0};
    if (tdfc_detect_dropouts_amp(sig, 10, 0, 0.1f, 10, NULL, pts, NULL) != -1) { FAIL("step=0"); return; }
    if (tdfc_detect_dropouts_amp(sig, 10, 64, 0.1f, 10, NULL, NULL, NULL) != -1) { FAIL("null pts"); return; }
    PASS();
}

/* ---- Segmentation Tests ---- */
static void test_segmentation_basic(void) {
    T("Segmentation: basic split at change points");
    size_t N = 50000;
    float *sig = gen_signal(N, 0.8f, -1, 0, 0);
    tdfc_config cfg = tdfc_default_config();
    tdfc_result r;
    tdfc_analyze(sig, N, &cfg, &r);

    uint8_t *drop = (uint8_t*)calloc(r.n_points, 1);
    tdfc_segmentation segs;
    int rc = tdfc_segment_from_changepoints(&r, drop, 10, &segs);
    if (rc != 0) { FAIL("rc"); goto done_basic; }
    if (segs.n_seg == 0) { FAIL("no segments"); goto done_basic; }

    /* All segments should cover entire range */
    size_t first = segs.seg[0].start_point;
    size_t last = segs.seg[segs.n_seg-1].end_point;
    if (first != 0) { FAIL("doesn't start at 0"); goto done_basic; }
    if (last != r.n_points - 1) { FAIL("doesn't end at last point"); goto done_basic; }
    PASS();
done_basic:
    tdfc_free_segmentation(&segs);
    free(drop);
    tdfc_free_result(&r);
    free(sig);
}

static void test_segmentation_scores(void) {
    T("Segmentation: clean signal has high scores");
    size_t N = 50000;
    float *sig = gen_signal(N, 0.8f, -1, 0, 0);
    tdfc_config cfg = tdfc_default_config();
    tdfc_result r;
    tdfc_analyze(sig, N, &cfg, &r);

    uint8_t *drop = (uint8_t*)calloc(r.n_points, 1);
    tdfc_segmentation segs;
    tdfc_segment_from_changepoints(&r, drop, 10, &segs);

    int all_good = 1;
    for (size_t i = 0; i < segs.n_seg; i++) {
        if (segs.seg[i].score < 30.0f) { all_good = 0; break; }
    }
    if (!all_good) { FAIL("clean signal should have decent scores"); }
    else PASS();

    tdfc_free_segmentation(&segs);
    free(drop); tdfc_free_result(&r); free(sig);
}

static void test_segmentation_degraded(void) {
    T("Segmentation: degraded segment flagged");
    size_t N = 50000;
    float *sig = gen_signal(N, 0.8f, 20000, 10000, 0.02f);
    tdfc_config cfg = tdfc_default_config();
    cfg.cusum_h = 3.0f; /* more sensitive */
    tdfc_result r;
    tdfc_analyze(sig, N, &cfg, &r);

    /* Compute amplitude-based dropouts */
    uint8_t *drop = (uint8_t*)calloc(r.n_points, 1);
    tdfc_detect_dropouts_amp(sig, N, r.step, 0.05f, 400, NULL, drop, NULL);

    tdfc_segmentation segs;
    tdfc_segment_from_changepoints(&r, drop, 10, &segs);

    int found_degraded = 0;
    for (size_t i = 0; i < segs.n_seg; i++) {
        if (segs.seg[i].flags & TDFC_SEG_FLAG_DEGRADED) found_degraded = 1;
    }
    if (!found_degraded) { FAIL("should find degraded segment"); }
    else PASS();

    tdfc_free_segmentation(&segs);
    free(drop); tdfc_free_result(&r); free(sig);
}

static void test_segmentation_merge(void) {
    T("Segmentation: min_seg_len merge works");
    size_t N = 50000;
    float *sig = gen_signal(N, 0.8f, -1, 0, 0);
    tdfc_config cfg = tdfc_default_config();
    cfg.cusum_h = 2.0f; /* very sensitive -> many change points */
    tdfc_result r;
    tdfc_analyze(sig, N, &cfg, &r);

    uint8_t *drop = (uint8_t*)calloc(r.n_points, 1);
    tdfc_segmentation segs_small, segs_big;
    tdfc_segment_from_changepoints(&r, drop, 1, &segs_small);
    tdfc_segment_from_changepoints(&r, drop, 50, &segs_big);

    /* Large min_seg_len should produce fewer or equal segments */
    if (segs_big.n_seg > segs_small.n_seg) { FAIL("merge should reduce count"); }
    else PASS();

    /* Check all big segments meet min length */
    for (size_t i = 0; i < segs_big.n_seg; i++) {
        size_t len = segs_big.seg[i].end_point - segs_big.seg[i].start_point + 1;
        if (segs_big.n_seg > 1 && len < 50) {
            printf("    WARNING: seg %zu len=%zu < 50\n", i, len);
        }
    }

    tdfc_free_segmentation(&segs_small);
    tdfc_free_segmentation(&segs_big);
    free(drop); tdfc_free_result(&r); free(sig);
}

/* ---- Integration: full pipeline ---- */
static void test_full_pipeline(void) {
    T("Full pipeline: analyze + robust + dropout + segment");
    size_t N = 100000;
    float *sig = gen_signal(N, 0.7f, 60000, 800, 0.02f);

    /* Core analysis */
    tdfc_config cfg = tdfc_default_config();
    tdfc_result r;
    int rc = tdfc_analyze(sig, N, &cfg, &r);
    if (rc != 0) { FAIL("analyze"); free(sig); return; }

    /* Robust stats */
    tdfc_robust_stats_t rs;
    tdfc_compute_robust_stats(r.envelope_rms, r.n_points, 0.10f, &rs);
    if (rs.median <= 0) { FAIL("robust median"); tdfc_free_result(&r); free(sig); return; }

    /* Envelope dropout */
    uint8_t *drop_env = (uint8_t*)calloc(r.n_points, 1);
    float ratio_env = 0;
    tdfc_detect_dropouts_env(r.envelope_rms, r.n_points, rs.median * 0.35f, 8, drop_env, &ratio_env);

    /* Amplitude dropout */
    uint8_t *drop_amp = (uint8_t*)calloc(r.n_points, 1);
    float ratio_amp = 0;
    tdfc_detect_dropouts_amp(sig, N, r.step, rs.median * 0.10f, 400, NULL, drop_amp, &ratio_amp);

    /* Segmentation using amplitude dropouts */
    tdfc_segmentation segs;
    rc = tdfc_segment_from_changepoints(&r, drop_amp, 12, &segs);
    if (rc != 0) { FAIL("segment"); goto done_pipe; }

    int score = tdfc_health_score(&r);
    printf("\n       Health=%d segs=%zu env_drop=%.2f%% amp_drop=%.2f%% robust_med=%.4f ... ",
           score, segs.n_seg, ratio_env*100, ratio_amp*100, rs.median);

    if (segs.n_seg > 0 && score >= 0) PASS();
    else FAIL("bad pipeline output");

    tdfc_free_segmentation(&segs);
done_pipe:
    free(drop_env); free(drop_amp);
    tdfc_free_result(&r);
    free(sig);
}

int main(void) {
    printf("\n  TDFC+ Test Suite\n");
    printf("  ================\n\n");

    /* Robust stats */
    test_robust_stats_basic();
    test_robust_stats_outlier();
    test_robust_stats_null();

    /* Envelope dropout */
    test_dropout_env_none();
    test_dropout_env_detect();
    test_dropout_env_minrun();

    /* Amplitude dropout (new in plus2) */
    test_dropout_amp_basic();
    test_dropout_amp_clean();
    test_dropout_amp_short_run();
    test_dropout_amp_null();

    /* Segmentation */
    test_segmentation_basic();
    test_segmentation_scores();
    test_segmentation_degraded();
    test_segmentation_merge();

    /* Full pipeline */
    test_full_pipeline();

    printf("\n  Results: %d/%d passed", tests_pass, tests_run);
    if (tests_pass == tests_run) printf("  ✓ ALL PASSED\n\n");
    else printf("  ✗ FAILURES\n\n");

    return (tests_pass == tests_run) ? 0 : 1;
}
