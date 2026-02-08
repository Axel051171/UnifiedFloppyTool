/**
 * test_event_bridge.c — Tests for OTDR Event Core v2 + UFT Event Bridge
 * =======================================================================
 */

#include "otdr_event_core_v2.h"
#include "uft_event_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { printf("  %-55s ", name); tests_run++; } while(0)
#define PASS()     do { printf("✓\n"); tests_passed++; } while(0)
#define FAIL(m)    do { printf("✗ (%s)\n", m); return; } while(0)

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

/* ==== Build synthetic OTDR-like traces ==== */

/* Clean signal with a connector-like event: spike + step */
static float *build_connector_trace(size_t n, size_t spike_pos) {
    float *amp = (float *)malloc(n * sizeof(float));
    if (!amp) return NULL;
    srand(42);
    for (size_t i = 0; i < n; i++) {
        float base = 1.0f - 0.3f * (float)i / (float)n;
        float noise = (frand() - 0.5f) * 0.02f;
        if (i == spike_pos) base += 0.8f;         /* Fresnel spike */
        if (i > spike_pos + 10) base -= 0.12f;    /* insertion loss */
        amp[i] = base + noise;
    }
    return amp;
}

/* Clean signal with an attenuation step only (splice) */
static float *build_splice_trace(size_t n, size_t step_pos) {
    float *amp = (float *)malloc(n * sizeof(float));
    if (!amp) return NULL;
    srand(77);
    for (size_t i = 0; i < n; i++) {
        float base = 1.0f;
        float noise = (frand() - 0.5f) * 0.015f;
        if (i > step_pos) base -= 0.15f;
        amp[i] = base + noise;
    }
    return amp;
}

/* Pure noise (no events) */
static float *build_noise_trace(size_t n) {
    float *amp = (float *)malloc(n * sizeof(float));
    if (!amp) return NULL;
    srand(99);
    for (size_t i = 0; i < n; i++)
        amp[i] = 0.5f + (frand() - 0.5f) * 0.01f;
    return amp;
}

/* Multi-event trace */
static float *build_multi_event_trace(size_t n) {
    float *amp = (float *)malloc(n * sizeof(float));
    if (!amp) return NULL;
    srand(123);
    for (size_t i = 0; i < n; i++) {
        float base = 2.0f - 0.5f * (float)i / (float)n;
        float noise = (frand() - 0.5f) * 0.02f;

        /* Connector 1 at 10000 */
        if (i == 10000) base += 0.9f;
        if (i > 10015) base -= 0.1f;

        /* Splice at 30000 */
        if (i > 30000) base -= 0.08f;

        /* Connector 2 at 50000 */
        if (i == 50000) base += 0.7f;
        if (i > 50012) base -= 0.12f;

        /* Fiber break at 70000 */
        if (i == 70000) base += 1.5f;
        if (i > 70001) base -= 1.2f;

        amp[i] = base + noise;
    }
    return amp;
}

/* ==== Core library tests (otdr_*) ==== */

static void test_otdr_default_config(void) {
    TEST("otdr_default_config values");
    otdr_config c = otdr_default_config();
    if (c.window != 1025) FAIL("window");
    if (c.local_sigma_enable != 1) FAIL("local_sigma");
    if (fabsf(c.mad_scale - 1.4826f) > 0.001f) FAIL("mad_scale");
    PASS();
}

static void test_otdr_null_reject(void) {
    TEST("otdr_* NULL/zero rejection");
    otdr_config c = otdr_default_config();
    otdr_event_result_t r;
    if (otdr_detect_events_baseline(NULL, 0, &c, NULL, &r) >= 0) FAIL("null");
    float x = 1; 
    if (otdr_detect_events_baseline(&x, 0, &c, NULL, &r) >= 0) FAIL("n=0");
    PASS();
}

static void test_otdr_features_computed(void) {
    TEST("Feature extraction produces valid fields");
    const size_t N = 1024;
    float *amp = build_connector_trace(N, 500);
    if (!amp) FAIL("alloc");

    otdr_features_t *feat = calloc(N, sizeof(otdr_features_t));
    otdr_config c = otdr_default_config();
    int rc = otdr_extract_features(amp, N, &c, feat);

    int ok = (rc == 0);
    if (ok && feat[500].delta < 0.5f) ok = 0;     /* spike should have large delta */
    if (ok && feat[500].snr_db < 10.0f) ok = 0;   /* should have high SNR */
    if (ok && feat[500].noise_sigma <= 0.0f) ok = 0;

    free(amp); free(feat);
    if (!ok) FAIL("features");
    PASS();
}

static void test_otdr_spike_detected(void) {
    TEST("Connector spike detected");
    const size_t N = 4096;
    float *amp = build_connector_trace(N, 2000);
    if (!amp) FAIL("alloc");

    otdr_event_result_t *res = calloc(N, sizeof(otdr_event_result_t));
    otdr_config c = otdr_default_config();
    c.thr_reflect_snr_db = 14.0f;
    otdr_detect_events_baseline(amp, N, &c, NULL, res);

    int found = 0;
    for (size_t i = 1995; i < 2005; i++) {
        if (res[i].label == OTDR_EVT_REFLECTION) { found = 1; break; }
    }
    free(amp); free(res);
    if (!found) FAIL("not detected");
    PASS();
}

static void test_otdr_rle_segments(void) {
    TEST("RLE segmentation produces segments");
    const size_t N = 2048;
    float *amp = build_connector_trace(N, 1000);
    otdr_event_result_t *res = calloc(N, sizeof(otdr_event_result_t));
    otdr_config c = otdr_default_config();
    otdr_detect_events_baseline(amp, N, &c, NULL, res);

    otdr_segment_t segs[256];
    size_t nseg = otdr_rle_segments(res, N, segs, 256);

    free(amp); free(res);
    if (nseg < 1) FAIL("no segments");
    PASS();
}

static void test_otdr_merge(void) {
    TEST("Merge logic creates REFLECT_LOSS");
    const size_t N = 8192;
    float *amp = build_connector_trace(N, 4000);
    otdr_event_result_t *res = calloc(N, sizeof(otdr_event_result_t));
    otdr_config c = otdr_default_config();
    otdr_detect_events_baseline(amp, N, &c, NULL, res);

    otdr_merge_config mc = otdr_default_merge_config();
    mc.merge_gap_max = 100;
    otdr_segment_t segs[512];
    size_t nseg = otdr_rle_segments_merged(res, N, &mc, segs, 512);

    int found_merged = 0;
    for (size_t i = 0; i < nseg; i++) {
        if (segs[i].label == OTDR_EVT_REFLECT_LOSS &&
            (segs[i].flags & OTDR_SEG_FLAG_MERGED)) {
            found_merged = 1; break;
        }
    }
    free(amp); free(res);
    if (!found_merged) FAIL("no merged");
    PASS();
}

static void test_otdr_clean_signal(void) {
    TEST("Clean signal → few/no events");
    const size_t N = 4096;
    float *amp = build_noise_trace(N);
    otdr_event_result_t *res = calloc(N, sizeof(otdr_event_result_t));
    otdr_config c = otdr_default_config();
    c.thr_reflect_snr_db = 16.0f;
    c.thr_atten_snr_db = 14.0f;
    otdr_detect_events_baseline(amp, N, &c, NULL, res);

    size_t event_count = 0;
    for (size_t i = 0; i < N; i++)
        if (res[i].label != OTDR_EVT_NONE) event_count++;

    free(amp); free(res);
    /* With tight thresholds, clean signal should have <10% events */
    if ((float)event_count / (float)N > 0.10f) FAIL("too many events");
    PASS();
}

/* ==== Bridge tests (uft_event_*) ==== */

static void test_bridge_version(void) {
    TEST("Bridge version string");
    const char *v = uft_event_version();
    if (!v || strlen(v) == 0) FAIL("empty");
    PASS();
}

static void test_bridge_error_strings(void) {
    TEST("Error strings non-NULL");
    for (int i = 0; i >= -5; i--) {
        if (!uft_event_error_str((uft_event_error_t)i)) FAIL("NULL");
    }
    PASS();
}

static void test_bridge_type_strings(void) {
    TEST("Event type strings");
    if (strcmp(uft_event_type_str(UFT_EVT_NORMAL), "NORMAL") != 0) FAIL("normal");
    if (strcmp(uft_event_type_str(UFT_EVT_SPIKE), "SPIKE") != 0) FAIL("spike");
    if (strcmp(uft_event_type_str(UFT_EVT_COMPOUND), "COMPOUND") != 0) FAIL("compound");
    PASS();
}

static void test_bridge_init_free(void) {
    TEST("Init/free lifecycle");
    uft_event_ctx_t ctx;
    uft_event_error_t rc = uft_event_init(&ctx, NULL);
    if (rc != UFT_EVT_OK) FAIL("init");
    if (!ctx.initialized) FAIL("not init");
    uft_event_free(&ctx);
    if (ctx.initialized) FAIL("still init");
    PASS();
}

static void test_bridge_null_reject(void) {
    TEST("Bridge NULL/error handling");
    uft_event_ctx_t ctx;
    uft_event_init(&ctx, NULL);
    if (uft_event_detect_float(&ctx, NULL, 100) != UFT_EVT_ERR_NULL) FAIL("null");
    float x[] = {1,2,3};
    if (uft_event_detect_float(&ctx, x, 3) != UFT_EVT_ERR_SMALL) FAIL("small");
    uft_event_free(&ctx);
    PASS();
}

static void test_bridge_connector_detection(void) {
    TEST("Connector event → SPIKE/COMPOUND detected");
    const size_t N = 20000;
    float *amp = build_connector_trace(N, 10000);
    if (!amp) FAIL("alloc");

    uft_event_ctx_t ctx;
    uft_event_init(&ctx, NULL);
    uft_event_error_t rc = uft_event_detect_float(&ctx, amp, N);
    if (rc != UFT_EVT_OK) { free(amp); uft_event_free(&ctx); FAIL("detect"); }

    uft_event_report_t rpt = uft_event_get_report(&ctx);
    int found_spike_or_compound = (rpt.spike_count > 0 || rpt.compound_count > 0);

    /* Check event location */
    int near_target = 0;
    for (size_t i = 0; i < uft_event_count(&ctx); i++) {
        const uft_event_info_t *e = uft_event_get(&ctx, i);
        if (e && e->start >= 9990 && e->start <= 10020 &&
            (e->type == UFT_EVT_SPIKE || e->type == UFT_EVT_COMPOUND)) {
            near_target = 1; break;
        }
    }

    uft_event_free(&ctx);
    free(amp);
    if (!found_spike_or_compound) FAIL("no spike/compound");
    if (!near_target) FAIL("wrong position");
    PASS();
}

static void test_bridge_splice_detection(void) {
    TEST("Splice event → DEGRADATION detected");
    const size_t N = 20000;
    float *amp = build_splice_trace(N, 10000);
    if (!amp) FAIL("alloc");

    uft_event_ctx_t ctx;
    uft_event_init(&ctx, NULL);
    uft_event_detect_float(&ctx, amp, N);

    uft_event_report_t rpt = uft_event_get_report(&ctx);
    int found = (rpt.degradation_count > 0 || rpt.compound_count > 0);

    uft_event_free(&ctx);
    free(amp);
    if (!found) FAIL("no degradation");
    PASS();
}

static void test_bridge_clean_quality(void) {
    TEST("Clean signal → high quality score");
    const size_t N = 10000;
    float *amp = build_noise_trace(N);
    if (!amp) FAIL("alloc");

    uft_event_config_t cfg = uft_event_default_config();
    cfg.spike_snr_db  = 18.0f;
    cfg.degrad_snr_db = 16.0f;

    uft_event_ctx_t ctx;
    uft_event_init(&ctx, &cfg);
    uft_event_detect_float(&ctx, amp, N);

    uft_event_report_t rpt = uft_event_get_report(&ctx);
    uft_event_free(&ctx);
    free(amp);

    if (rpt.quality_score < 0.5f) FAIL("low quality");
    PASS();
}

static void test_bridge_multi_event(void) {
    TEST("Multi-event trace → multiple events found");
    const size_t N = 80000;
    float *amp = build_multi_event_trace(N);
    if (!amp) FAIL("alloc");

    uft_event_ctx_t ctx;
    uft_event_init(&ctx, NULL);
    uft_event_detect_float(&ctx, amp, N);

    uft_event_report_t rpt = uft_event_get_report(&ctx);
    size_t n_events = uft_event_count(&ctx);

    uft_event_free(&ctx);
    free(amp);

    /* Should find at least 3 distinct event regions */
    if (n_events < 3) FAIL("too few events");
    if (rpt.total_events != n_events) FAIL("count mismatch");
    PASS();
}

static void test_bridge_flux_ns(void) {
    TEST("Flux interval detection (uint32)");
    const size_t N = 8000;
    uint32_t *flux = malloc(N * sizeof(uint32_t));
    srand(456);
    for (size_t i = 0; i < N; i++) {
        float base = 4000.0f;
        float noise = (frand() - 0.5f) * 20.0f;
        if (i == 4000) base += 2000.0f;  /* big spike */
        if (i > 4010) base -= 200.0f;    /* step */
        flux[i] = (uint32_t)(base + noise);
    }

    uft_event_ctx_t ctx;
    uft_event_init(&ctx, NULL);
    uft_event_error_t rc = uft_event_detect_flux_ns(&ctx, flux, N);
    size_t n_events = uft_event_count(&ctx);

    uft_event_free(&ctx);
    free(flux);
    if (rc != UFT_EVT_OK) FAIL("failed");
    if (n_events < 1) FAIL("no events");
    PASS();
}

static void test_bridge_analog(void) {
    TEST("Analog sample detection (int16)");
    const size_t N = 4000;
    int16_t *samples = malloc(N * sizeof(int16_t));
    srand(789);
    for (size_t i = 0; i < N; i++) {
        float s = 10000.0f;
        if (i == 2000) s += 15000.0f;  /* spike */
        if (i > 2010) s -= 2000.0f;    /* step */
        s += (frand() - 0.5f) * 200.0f;
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        samples[i] = (int16_t)s;
    }

    uft_event_ctx_t ctx;
    uft_event_init(&ctx, NULL);
    uft_event_error_t rc = uft_event_detect_analog(&ctx, samples, N);

    uft_event_free(&ctx);
    free(samples);
    if (rc != UFT_EVT_OK) FAIL("failed");
    PASS();
}

static void test_bridge_report_fields(void) {
    TEST("Report fields populated correctly");
    const size_t N = 20000;
    float *amp = build_connector_trace(N, 10000);

    uft_event_ctx_t ctx;
    uft_event_init(&ctx, NULL);
    uft_event_detect_float(&ctx, amp, N);
    uft_event_report_t rpt = uft_event_get_report(&ctx);

    int ok = 1;
    if (rpt.samples_analyzed != N) { ok = 0; printf("[n] "); }
    if (rpt.sigma_mean <= 0.0f) { ok = 0; printf("[σ=0] "); }
    if (rpt.quality_score < 0.0f || rpt.quality_score > 1.0f) { ok = 0; printf("[q] "); }
    if (rpt.event_density < 0.0f) { ok = 0; printf("[d] "); }

    uft_event_free(&ctx);
    free(amp);
    if (!ok) FAIL("bad fields");
    PASS();
}

static void test_bridge_event_access(void) {
    TEST("Event access by index");
    const size_t N = 20000;
    float *amp = build_connector_trace(N, 10000);

    uft_event_ctx_t ctx;
    uft_event_init(&ctx, NULL);
    uft_event_detect_float(&ctx, amp, N);

    size_t count = uft_event_count(&ctx);
    int ok = 1;
    if (count == 0) { ok = 0; printf("[count=0] "); }

    /* First event should have valid fields */
    if (count > 0) {
        const uft_event_info_t *e = uft_event_get(&ctx, 0);
        if (!e) { ok = 0; printf("[null] "); }
        else {
            if (e->length == 0) { ok = 0; printf("[len=0] "); }
            if (e->end < e->start) { ok = 0; printf("[end<start] "); }
            if (e->confidence < 0.0f || e->confidence > 1.0f) { ok = 0; printf("[conf] "); }
        }
    }

    /* Out of range returns NULL */
    if (uft_event_get(&ctx, 999999) != NULL) { ok = 0; printf("[oob] "); }

    uft_event_free(&ctx);
    free(amp);
    if (!ok) FAIL("access");
    PASS();
}

static void test_bridge_double_free(void) {
    TEST("Double free safety");
    uft_event_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    uft_event_free(&ctx);
    uft_event_free(&ctx);
    uft_event_free(NULL);
    PASS();
}

static void test_bridge_large_n(void) {
    TEST("N=200K performance");
    const size_t N = 200000;
    float *amp = malloc(N * sizeof(float));
    srand(1);
    for (size_t i = 0; i < N; i++) {
        float base = 1.0f - 0.25f * (float)i / (float)N;
        amp[i] = base + (frand() - 0.5f) * 0.03f;
        if (i == 60000) amp[i] += 0.9f;
        if (i > 60020) amp[i] -= 0.1f;
        if (i > 140000) amp[i] -= 0.15f;
    }

    uft_event_ctx_t ctx;
    uft_event_init(&ctx, NULL);
    uft_event_error_t rc = uft_event_detect_float(&ctx, amp, N);
    uft_event_free(&ctx);
    free(amp);
    if (rc != UFT_EVT_OK) FAIL("failed");
    PASS();
}

static void test_bridge_custom_config(void) {
    TEST("Custom config changes detection sensitivity");
    const size_t N = 10000;
    float *amp = build_connector_trace(N, 5000);

    /* Tight thresholds → fewer events */
    uft_event_config_t cfg_tight = uft_event_default_config();
    cfg_tight.spike_snr_db  = 25.0f;
    cfg_tight.degrad_snr_db = 22.0f;

    uft_event_ctx_t ctx1;
    uft_event_init(&ctx1, &cfg_tight);
    uft_event_detect_float(&ctx1, amp, N);
    size_t tight_count = uft_event_count(&ctx1);
    uft_event_free(&ctx1);

    /* Loose thresholds → more events */
    uft_event_config_t cfg_loose = uft_event_default_config();
    cfg_loose.spike_snr_db  = 5.0f;
    cfg_loose.degrad_snr_db = 4.0f;

    uft_event_ctx_t ctx2;
    uft_event_init(&ctx2, &cfg_loose);
    uft_event_detect_float(&ctx2, amp, N);
    size_t loose_count = uft_event_count(&ctx2);
    uft_event_free(&ctx2);

    free(amp);
    if (loose_count <= tight_count) FAIL("sensitivity unchanged");
    PASS();
}

/* ==== Main ==== */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║   OTDR EVENT CORE v2 + UFT EVENT BRIDGE - TEST SUITE       ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("── Core Library (otdr_*) ─────────────────────────────────────\n");
    test_otdr_default_config();
    test_otdr_null_reject();
    test_otdr_features_computed();
    test_otdr_spike_detected();
    test_otdr_rle_segments();
    test_otdr_merge();
    test_otdr_clean_signal();

    printf("\n── Bridge API (uft_event_*) ───────────────────────────────────\n");
    test_bridge_version();
    test_bridge_error_strings();
    test_bridge_type_strings();
    test_bridge_init_free();
    test_bridge_null_reject();
    test_bridge_connector_detection();
    test_bridge_splice_detection();
    test_bridge_clean_quality();
    test_bridge_multi_event();
    test_bridge_flux_ns();
    test_bridge_analog();
    test_bridge_report_fields();
    test_bridge_event_access();
    test_bridge_double_free();
    test_bridge_large_n();
    test_bridge_custom_config();

    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("  Ergebnis: %d/%d Tests bestanden\n", tests_passed, tests_run);
    printf("══════════════════════════════════════════════════════════════\n\n");

    return tests_passed < tests_run ? 1 : 0;
}
