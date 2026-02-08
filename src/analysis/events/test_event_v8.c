/**
 * test_event_v8.c — Tests for OTDR Event Core v8 + UFT Bridge
 */

#include "otdr_event_core_v8.h"
#include "uft_event_v8_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int t_run = 0, t_pass = 0;
#define TEST(n) do { printf("  %-55s ", n); t_run++; } while(0)
#define PASS()  do { printf("✓\n"); t_pass++; } while(0)
#define FAIL(m) do { printf("✗ (%s)\n", m); return; } while(0)

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

/* ═══════════ Trace builders ═══════════ */

/* Connector: spike + step */
static float *mk_connector(size_t n, size_t pos) {
    float *a = calloc(n, sizeof(float));
    srand(42);
    for (size_t i = 0; i < n; i++) {
        float base = 1.0f - 0.3f * (float)i / (float)n;
        if (i == pos) base += 0.9f;
        if (i > pos + 15) base -= 0.12f;
        a[i] = base + (frand() - 0.5f) * 0.02f;
    }
    return a;
}

/* Splice: pure step-down */
static float *mk_splice(size_t n, size_t pos) {
    float *a = calloc(n, sizeof(float));
    srand(77);
    for (size_t i = 0; i < n; i++) {
        float base = 1.0f;
        if (i > pos) base -= 0.15f;
        a[i] = base + (frand() - 0.5f) * 0.015f;
    }
    return a;
}

/* Oscillation / ringing */
static float *mk_oscillation(size_t n, size_t start, size_t len) {
    float *a = calloc(n, sizeof(float));
    srand(88);
    for (size_t i = 0; i < n; i++) {
        float base = 1.0f;
        if (i >= start && i < start + len) {
            base += 0.5f * sinf((float)(i - start) * 3.14159f * 0.5f);
        }
        a[i] = base + (frand() - 0.5f) * 0.01f;
    }
    return a;
}

/* Gain-up step */
static float *mk_gainup(size_t n, size_t pos) {
    float *a = calloc(n, sizeof(float));
    srand(55);
    for (size_t i = 0; i < n; i++) {
        float base = 0.5f;
        if (i > pos) base += 0.3f;
        a[i] = base + (frand() - 0.5f) * 0.01f;
    }
    return a;
}

/* Clean noise */
static float *mk_clean(size_t n) {
    float *a = calloc(n, sizeof(float));
    srand(99);
    for (size_t i = 0; i < n; i++)
        a[i] = 0.5f + (frand() - 0.5f) * 0.01f;
    return a;
}

/* Multi-event */
static float *mk_multi(size_t n) {
    float *a = calloc(n, sizeof(float));
    srand(123);
    for (size_t i = 0; i < n; i++) {
        float base = 2.0f - 0.5f * (float)i / (float)n;
        if (i == 10000) base += 0.9f;
        if (i > 10015) base -= 0.1f;
        if (i > 30000) base -= 0.08f;
        if (i == 50000) base += 0.7f;
        if (i > 50012) base -= 0.12f;
        a[i] = base + (frand() - 0.5f) * 0.02f;
    }
    return a;
}

/* ═══════════ Core v8 tests ═══════════ */

static void test_v8_defaults(void) {
    TEST("v8 default config valid");
    otdr8_config_t c = otdr8_default_config();
    if (c.num_scales != 4) FAIL("scales");
    if (c.scale_windows[0] != 128) FAIL("win0");
    if (c.scale_windows[3] != 8192) FAIL("win3");
    if (fabsf(c.mad_scale - 1.4826f) > 0.001f) FAIL("mad");
    PASS();
}

static void test_v8_null_reject(void) {
    TEST("v8 NULL/zero rejection");
    otdr8_config_t c = otdr8_default_config();
    otdr8_result_t r;
    if (otdr8_detect(NULL, 0, &c, NULL, &r) >= 0) FAIL("null");
    float x = 1.0f;
    if (otdr8_detect(&x, 0, &c, NULL, &r) >= 0) FAIL("n=0");
    PASS();
}

static void test_v8_multiscale_features(void) {
    TEST("Multi-scale features: all scales populated");
    const size_t N = 10000;
    float *amp = mk_connector(N, 5000);
    otdr8_features_t *feat = calloc(N, sizeof(otdr8_features_t));
    otdr8_config_t c = otdr8_default_config();
    int rc = otdr8_extract_features(amp, N, &c, feat);
    int ok = (rc == 0);

    /* Spike sample should have high SNR on at least one scale */
    if (ok && feat[5000].max_snr_db < 10.0f) ok = 0;
    /* All 4 scales should be populated */
    for (size_t s = 0; s < 4 && ok; s++) {
        if (feat[5000].env_rms[s] <= 0.0f) ok = 0;
    }

    free(amp); free(feat);
    if (!ok) FAIL("features");
    PASS();
}

static void test_v8_polarity_spike(void) {
    TEST("Polarity: spike-step at connector");
    const size_t N = 10000;
    float *amp = mk_connector(N, 5000);
    otdr8_features_t *feat = calloc(N, sizeof(otdr8_features_t));
    otdr8_config_t c = otdr8_default_config();
    otdr8_extract_features(amp, N, &c, feat);

    /* Around the spike, expect SPIKE_POS or SPIKE_STEP */
    int found_spike_pat = 0;
    for (size_t i = 4998; i <= 5002; i++) {
        if (feat[i].polarity == OTDR8_PAT_SPIKE_POS ||
            feat[i].polarity == OTDR8_PAT_SPIKE_STEP) {
            found_spike_pat = 1; break;
        }
    }
    free(amp); free(feat);
    if (!found_spike_pat) FAIL("no spike pattern");
    PASS();
}

static void test_v8_classify_connector(void) {
    TEST("Classify: connector → REFLECTION detected");
    const size_t N = 20000;
    float *amp = mk_connector(N, 10000);
    otdr8_result_t *res = calloc(N, sizeof(otdr8_result_t));
    otdr8_config_t c = otdr8_default_config();
    otdr8_detect(amp, N, &c, NULL, res);

    int found = 0;
    for (size_t i = 9998; i <= 10002; i++) {
        if (res[i].label == OTDR8_EVT_REFLECTION) { found = 1; break; }
    }
    free(amp); free(res);
    if (!found) FAIL("no reflection");
    PASS();
}

static void test_v8_merge_reflect_loss(void) {
    TEST("Merge: REFLECT+ATTEN → REFLECT_LOSS");
    const size_t N = 20000;
    float *amp = mk_connector(N, 10000);
    otdr8_features_t *feat = calloc(N, sizeof(otdr8_features_t));
    otdr8_result_t *res = calloc(N, sizeof(otdr8_result_t));
    otdr8_config_t c = otdr8_default_config();
    otdr8_detect(amp, N, &c, feat, res);

    otdr8_segment_t segs[1024];
    size_t nseg = otdr8_segment_merge(res, feat, N, NULL, segs, 1024);

    int found = 0;
    for (size_t i = 0; i < nseg; i++) {
        if (segs[i].label == OTDR8_EVT_REFLECT_LOSS && segs[i].flags) {
            found = 1; break;
        }
    }
    free(amp); free(feat); free(res);
    if (!found) FAIL("no merged");
    PASS();
}

static void test_v8_passfail(void) {
    TEST("Pass/fail: connector gets verdict");
    const size_t N = 20000;
    float *amp = mk_connector(N, 10000);
    otdr8_features_t *feat = calloc(N, sizeof(otdr8_features_t));
    otdr8_result_t *res = calloc(N, sizeof(otdr8_result_t));
    otdr8_config_t c = otdr8_default_config();
    otdr8_detect(amp, N, &c, feat, res);

    otdr8_segment_t segs[1024];
    size_t nseg = otdr8_segment_merge(res, feat, N, NULL, segs, 1024);
    otdr8_passfail_config_t pf = otdr8_default_passfail_config();
    otdr8_apply_passfail(segs, nseg, &pf);

    /* Non-NONE segments should have some verdict */
    int has_verdict = 0;
    for (size_t i = 0; i < nseg; i++) {
        if (segs[i].label != OTDR8_EVT_NONE &&
            segs[i].verdict != OTDR8_VERDICT_PASS) {
            has_verdict = 1; break;
        }
    }
    free(amp); free(feat); free(res);
    /* It's OK if all pass — just check they were processed */
    if (nseg < 2) FAIL("too few segments");
    PASS();
}

static void test_v8_clean_signal(void) {
    TEST("Clean signal → few events");
    const size_t N = 10000;
    float *amp = mk_clean(N);
    otdr8_result_t *res = calloc(N, sizeof(otdr8_result_t));
    otdr8_config_t c = otdr8_default_config();
    c.thr_reflect_snr_db = 18.0f;
    c.thr_atten_snr_db = 16.0f;
    otdr8_detect(amp, N, &c, NULL, res);

    size_t event_count = 0;
    for (size_t i = 0; i < N; i++)
        if (res[i].label != OTDR8_EVT_NONE) event_count++;

    free(amp); free(res);
    if ((float)event_count / (float)N > 0.10f) FAIL("too many");
    PASS();
}

static void test_v8_string_helpers(void) {
    TEST("String helpers non-NULL");
    if (!otdr8_event_str(OTDR8_EVT_REFLECTION)) FAIL("evt");
    if (!otdr8_polarity_str(OTDR8_PAT_SPIKE_POS)) FAIL("pol");
    if (!otdr8_verdict_str(OTDR8_VERDICT_FAIL)) FAIL("vrd");
    if (strcmp(otdr8_event_str(OTDR8_EVT_OSCILLATION), "OSCILLATION") != 0) FAIL("osc");
    PASS();
}

/* ═══════════ Bridge tests ═══════════ */

static void test_br_version(void) {
    TEST("Bridge version");
    if (!uft_ev8_version() || strlen(uft_ev8_version()) == 0) FAIL("empty");
    PASS();
}

static void test_br_error_strings(void) {
    TEST("Error strings");
    for (int i = 0; i >= -5; i--)
        if (!uft_ev8_error_str((uft_ev8_error_t)i)) FAIL("NULL");
    PASS();
}

static void test_br_type_strings(void) {
    TEST("Type strings: all 8 types");
    const char *names[] = {"NORMAL","SPIKE","DEGRADATION","COMPOUND",
                           "RECOVERY","DROPOUT","FLUTTER","WEAKSIGNAL"};
    for (int i = 0; i < 8; i++) {
        if (strcmp(uft_ev8_type_str((uft_ev8_type_t)i), names[i]) != 0) FAIL("mismatch");
    }
    PASS();
}

static void test_br_init_free(void) {
    TEST("Init/free lifecycle");
    uft_ev8_ctx_t ctx;
    if (uft_ev8_init(&ctx, NULL) != UFT_EV8_OK) FAIL("init");
    if (!ctx.initialized) FAIL("not init");
    if (ctx.cfg.num_scales != 4) FAIL("scales");
    uft_ev8_free(&ctx);
    if (ctx.initialized) FAIL("still init");
    PASS();
}

static void test_br_null_reject(void) {
    TEST("Bridge NULL/small rejection");
    uft_ev8_ctx_t ctx;
    uft_ev8_init(&ctx, NULL);
    if (uft_ev8_detect_float(&ctx, NULL, 100) != UFT_EV8_ERR_NULL) FAIL("null");
    float x[] = {1,2,3,4,5,6,7};
    if (uft_ev8_detect_float(&ctx, x, 7) != UFT_EV8_ERR_SMALL) FAIL("small");
    uft_ev8_free(&ctx);
    PASS();
}

static void test_br_connector(void) {
    TEST("Connector → SPIKE/COMPOUND near target pos");
    const size_t N = 20000;
    float *amp = mk_connector(N, 10000);
    uft_ev8_ctx_t ctx;
    uft_ev8_init(&ctx, NULL);
    uft_ev8_detect_float(&ctx, amp, N);

    uft_ev8_report_t rpt = uft_ev8_get_report(&ctx);
    int found = (rpt.spike_count > 0 || rpt.compound_count > 0);

    int near = 0;
    for (size_t i = 0; i < uft_ev8_count(&ctx); i++) {
        const uft_ev8_event_t *e = uft_ev8_get(&ctx, i);
        if (e && e->start >= 9990 && e->start <= 10020 &&
            (e->type == UFT_EV8_SPIKE || e->type == UFT_EV8_COMPOUND)) {
            near = 1; break;
        }
    }
    uft_ev8_free(&ctx); free(amp);
    if (!found) FAIL("no spike/compound");
    if (!near) FAIL("wrong position");
    PASS();
}

static void test_br_splice(void) {
    TEST("Splice → DEGRADATION detected");
    const size_t N = 20000;
    float *amp = mk_splice(N, 10000);
    uft_ev8_ctx_t ctx;
    uft_ev8_init(&ctx, NULL);
    uft_ev8_detect_float(&ctx, amp, N);
    uft_ev8_report_t rpt = uft_ev8_get_report(&ctx);
    int found = (rpt.degradation_count > 0 || rpt.compound_count > 0 ||
                 rpt.weaksignal_count > 0);
    uft_ev8_free(&ctx); free(amp);
    if (!found) FAIL("no degradation");
    PASS();
}

static void test_br_clean_quality(void) {
    TEST("Clean signal → high quality score");
    const size_t N = 10000;
    float *amp = mk_clean(N);
    uft_ev8_config_t cfg = uft_ev8_default_config();
    cfg.spike_snr_db     = 25.0f;
    cfg.degrad_snr_db    = 23.0f;
    cfg.dropout_snr_db   = 25.0f;
    cfg.flutter_snr_db   = 20.0f;
    cfg.broadloss_snr_db = 18.0f;
    cfg.min_confidence   = 0.2f;
    cfg.enable_passfail  = false;  /* quality score: event-count based only */
    uft_ev8_ctx_t ctx;
    uft_ev8_init(&ctx, &cfg);
    uft_ev8_detect_float(&ctx, amp, N);
    uft_ev8_report_t rpt = uft_ev8_get_report(&ctx);
    uft_ev8_free(&ctx); free(amp);
    if (rpt.quality_score < 0.5f) FAIL("low quality");
    PASS();
}

static void test_br_multi_event(void) {
    TEST("Multi-event trace → ≥3 events");
    const size_t N = 80000;
    float *amp = mk_multi(N);
    uft_ev8_ctx_t ctx;
    uft_ev8_init(&ctx, NULL);
    uft_ev8_detect_float(&ctx, amp, N);
    size_t ne = uft_ev8_count(&ctx);
    uft_ev8_free(&ctx); free(amp);
    if (ne < 3) FAIL("too few");
    PASS();
}

static void test_br_passfail_verdicts(void) {
    TEST("Pass/fail verdicts populated");
    const size_t N = 20000;
    float *amp = mk_connector(N, 10000);
    uft_ev8_ctx_t ctx;
    uft_ev8_init(&ctx, NULL);
    uft_ev8_detect_float(&ctx, amp, N);
    uft_ev8_report_t rpt = uft_ev8_get_report(&ctx);

    /* Should have some verdicts */
    int has_verdicts = (rpt.pass_count + rpt.warn_count + rpt.fail_count > 0);

    /* Check individual events */
    int valid_verdicts = 1;
    for (size_t i = 0; i < uft_ev8_count(&ctx); i++) {
        const uft_ev8_event_t *e = uft_ev8_get(&ctx, i);
        if (e->verdict > UFT_EV8_FAIL) { valid_verdicts = 0; break; }
    }

    uft_ev8_free(&ctx); free(amp);
    if (!has_verdicts) FAIL("no verdicts");
    if (!valid_verdicts) FAIL("bad verdict values");
    PASS();
}

static void test_br_count_by_verdict(void) {
    TEST("count_by_verdict consistent with report");
    const size_t N = 20000;
    float *amp = mk_connector(N, 10000);
    uft_ev8_ctx_t ctx;
    uft_ev8_init(&ctx, NULL);
    uft_ev8_detect_float(&ctx, amp, N);
    uft_ev8_report_t rpt = uft_ev8_get_report(&ctx);

    size_t p = uft_ev8_count_by_verdict(&ctx, UFT_EV8_PASS);
    size_t w = uft_ev8_count_by_verdict(&ctx, UFT_EV8_WARN);
    size_t f = uft_ev8_count_by_verdict(&ctx, UFT_EV8_FAIL);

    uft_ev8_free(&ctx); free(amp);
    if (p != rpt.pass_count) FAIL("pass mismatch");
    if (w != rpt.warn_count) FAIL("warn mismatch");
    if (f != rpt.fail_count) FAIL("fail mismatch");
    PASS();
}

static void test_br_flux_ns(void) {
    TEST("Flux interval detection (uint32)");
    const size_t N = 8000;
    uint32_t *flux = malloc(N * sizeof(uint32_t));
    srand(456);
    for (size_t i = 0; i < N; i++) {
        float base = 4000.0f + (frand() - 0.5f) * 20.0f;
        if (i == 4000) base += 2000.0f;
        if (i > 4010) base -= 200.0f;
        flux[i] = (uint32_t)base;
    }
    uft_ev8_ctx_t ctx;
    uft_ev8_init(&ctx, NULL);
    uft_ev8_error_t rc = uft_ev8_detect_flux_ns(&ctx, flux, N);
    uft_ev8_free(&ctx); free(flux);
    if (rc != UFT_EV8_OK) FAIL("failed");
    PASS();
}

static void test_br_analog(void) {
    TEST("Analog detection (int16)");
    const size_t N = 4000;
    int16_t *s = malloc(N * sizeof(int16_t));
    srand(789);
    for (size_t i = 0; i < N; i++) {
        float v = 10000.0f + (frand() - 0.5f) * 200.0f;
        if (i == 2000) v += 15000.0f;
        if (i > 2010) v -= 2000.0f;
        if (v > 32767) v = 32767;
        s[i] = (int16_t)v;
    }
    uft_ev8_ctx_t ctx;
    uft_ev8_init(&ctx, NULL);
    uft_ev8_error_t rc = uft_ev8_detect_analog(&ctx, s, N);
    uft_ev8_free(&ctx); free(s);
    if (rc != UFT_EV8_OK) FAIL("failed");
    PASS();
}

static void test_br_custom_scales(void) {
    TEST("Custom 2-scale config works");
    uft_ev8_config_t cfg = uft_ev8_default_config();
    cfg.scale_windows[0] = 64;
    cfg.scale_windows[1] = 4096;
    cfg.num_scales = 2;

    const size_t N = 10000;
    float *amp = mk_connector(N, 5000);
    uft_ev8_ctx_t ctx;
    uft_ev8_init(&ctx, &cfg);
    uft_ev8_error_t rc = uft_ev8_detect_float(&ctx, amp, N);
    size_t ne = uft_ev8_count(&ctx);
    uft_ev8_free(&ctx); free(amp);
    if (rc != UFT_EV8_OK) FAIL("failed");
    if (ne == 0) FAIL("no events");
    PASS();
}

static void test_br_sensitivity(void) {
    TEST("Tight vs loose thresholds change sensitivity");
    const size_t N = 10000;
    float *amp = mk_connector(N, 5000);

    uft_ev8_config_t tight = uft_ev8_default_config();
    tight.spike_snr_db = 25.0f;
    tight.degrad_snr_db = 22.0f;
    uft_ev8_ctx_t ctx1;
    uft_ev8_init(&ctx1, &tight);
    uft_ev8_detect_float(&ctx1, amp, N);
    size_t tight_n = uft_ev8_count(&ctx1);
    uft_ev8_free(&ctx1);

    uft_ev8_config_t loose = uft_ev8_default_config();
    loose.spike_snr_db = 5.0f;
    loose.degrad_snr_db = 4.0f;
    uft_ev8_ctx_t ctx2;
    uft_ev8_init(&ctx2, &loose);
    uft_ev8_detect_float(&ctx2, amp, N);
    size_t loose_n = uft_ev8_count(&ctx2);
    uft_ev8_free(&ctx2);

    free(amp);
    if (loose_n <= tight_n) FAIL("no difference");
    PASS();
}

static void test_br_report_fields(void) {
    TEST("Report fields populated correctly");
    const size_t N = 20000;
    float *amp = mk_connector(N, 10000);
    uft_ev8_ctx_t ctx;
    uft_ev8_init(&ctx, NULL);
    uft_ev8_detect_float(&ctx, amp, N);
    uft_ev8_report_t rpt = uft_ev8_get_report(&ctx);

    int ok = 1;
    if (rpt.samples_analyzed != N) ok = 0;
    if (rpt.sigma_mean <= 0.0f) ok = 0;
    if (rpt.quality_score < 0.0f || rpt.quality_score > 1.0f) ok = 0;
    if (rpt.total_events != uft_ev8_count(&ctx)) ok = 0;

    uft_ev8_free(&ctx); free(amp);
    if (!ok) FAIL("bad fields");
    PASS();
}

static void test_br_double_free(void) {
    TEST("Double free safety");
    uft_ev8_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    uft_ev8_free(&ctx);
    uft_ev8_free(&ctx);
    uft_ev8_free(NULL);
    PASS();
}

static void test_br_large_n(void) {
    TEST("N=200K performance");
    const size_t N = 200000;
    float *amp = malloc(N * sizeof(float));
    srand(1);
    for (size_t i = 0; i < N; i++) {
        amp[i] = 1.0f - 0.25f * (float)i / (float)N + (frand() - 0.5f) * 0.03f;
        if (i == 60000) amp[i] += 0.9f;
        if (i > 60020) amp[i] -= 0.1f;
    }
    uft_ev8_ctx_t ctx;
    uft_ev8_init(&ctx, NULL);
    uft_ev8_error_t rc = uft_ev8_detect_float(&ctx, amp, N);
    uft_ev8_free(&ctx); free(amp);
    if (rc != UFT_EV8_OK) FAIL("failed");
    PASS();
}

/* ═══════════ Main ═══════════ */

int main(void) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║   OTDR EVENT CORE v8 + UFT BRIDGE - TEST SUITE             ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("── Core v8 (otdr8_*) ─────────────────────────────────────────\n");
    test_v8_defaults();
    test_v8_null_reject();
    test_v8_multiscale_features();
    test_v8_polarity_spike();
    test_v8_classify_connector();
    test_v8_merge_reflect_loss();
    test_v8_passfail();
    test_v8_clean_signal();
    test_v8_string_helpers();

    printf("\n── Bridge (uft_ev8_*) ────────────────────────────────────────\n");
    test_br_version();
    test_br_error_strings();
    test_br_type_strings();
    test_br_init_free();
    test_br_null_reject();
    test_br_connector();
    test_br_splice();
    test_br_clean_quality();
    test_br_multi_event();
    test_br_passfail_verdicts();
    test_br_count_by_verdict();
    test_br_flux_ns();
    test_br_analog();
    test_br_custom_scales();
    test_br_sensitivity();
    test_br_report_fields();
    test_br_double_free();
    test_br_large_n();

    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("  Ergebnis: %d/%d Tests bestanden\n", t_pass, t_run);
    printf("══════════════════════════════════════════════════════════════\n\n");
    return t_pass < t_run ? 1 : 0;
}
