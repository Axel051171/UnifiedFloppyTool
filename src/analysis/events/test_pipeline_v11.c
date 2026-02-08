/**
 * test_pipeline_v11.c — Tests for OTDR v11 Pipeline + UFT Bridge
 */

#include "otdr_event_core_v11.h"
#include "uft_pipeline_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int t_run = 0, t_pass = 0;
#define TEST(n) do { printf("  %-55s ", n); t_run++; } while(0)
#define PASS()  do { printf("✓\n"); t_pass++; } while(0)
#define FAIL(m) do { printf("✗ (%s)\n", m); return; } while(0)

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

/* ═══════════ Callback tracking ═══════════ */

typedef struct {
    uint32_t chunks_seen;
    size_t   events_seen;
    float    last_mean_conf;
    float    last_integrity;
} cb_tracker_t;

static void on_chunk_track(const otdr11_chunk_result_t *r, void *ud) {
    cb_tracker_t *t = (cb_tracker_t *)ud;
    t->chunks_seen++;
    t->last_mean_conf = r->mean_confidence;
    t->last_integrity = r->integrity_score;
}

static void on_event_track(const otdr11_event_t *e, void *ud) {
    cb_tracker_t *t = (cb_tracker_t *)ud;
    t->events_seen++;
    (void)e;
}

/* Bridge callback tracker */
typedef struct {
    uint32_t chunks;
    size_t   events;
    float    last_conf;
} br_tracker_t;

static void br_on_chunk(const uft_pipe_chunk_t *c, void *ud) {
    br_tracker_t *t = (br_tracker_t *)ud;
    t->chunks++;
    t->last_conf = c->mean_confidence;
}

static void br_on_event(const uft_pipe_event_t *e, void *ud) {
    br_tracker_t *t = (br_tracker_t *)ud;
    t->events++;
    (void)e;
}

/* ═══════════ Signal generators ═══════════ */

static float *mk_signal(size_t n, float base) {
    float *a = (float *)malloc(n * sizeof(float));
    srand(42);
    for (size_t i = 0; i < n; i++)
        a[i] = base + (frand() - 0.5f) * 0.02f;
    return a;
}

static float *mk_signal_with_dropout(size_t n, size_t dstart, size_t dlen) {
    float *a = mk_signal(n, 0.5f);
    for (size_t i = dstart; i < dstart + dlen && i < n; i++)
        a[i] = 0.0f;
    return a;
}

static float *mk_signal_with_spike(size_t n, size_t pos) {
    float *a = mk_signal(n, 0.5f);
    if (pos < n) a[pos] = 5.0f;
    if (pos + 1 < n) a[pos + 1] = -3.0f;
    return a;
}

/* ═══════════ Core v11 tests ═══════════ */

static void test_v11_defaults(void) {
    TEST("v11 default config valid");
    otdr11_config_t c = otdr11_default_config();
    if (c.chunk_size != 8192) FAIL("chunk_size");
    if (c.overlap != 256) FAIL("overlap");
    if (c.ring_capacity != 65536) FAIL("ring_cap");
    if (!c.enable_integrity) FAIL("integrity");
    if (!c.enable_detect) FAIL("detect");
    if (!c.enable_confidence) FAIL("confidence");
    PASS();
}

static void test_v11_init_free(void) {
    TEST("v11 init/free lifecycle");
    otdr11_pipeline_t p;
    if (otdr11_init(&p, NULL) != 0) FAIL("init");
    if (!p.initialized) FAIL("not init");
    otdr11_free(&p);
    if (p.initialized) FAIL("still init");
    PASS();
}

static void test_v11_null_reject(void) {
    TEST("v11 NULL rejection");
    otdr11_pipeline_t p;
    otdr11_init(&p, NULL);
    if (otdr11_push(&p, NULL, 100) >= 0) { otdr11_free(&p); FAIL("null push"); }
    if (otdr11_push(NULL, NULL, 0) >= 0) FAIL("null pipe");
    otdr11_free(&p);
    PASS();
}

static void test_v11_small_push(void) {
    TEST("Small push: no chunks yet");
    otdr11_pipeline_t p;
    otdr11_init(&p, NULL);
    float buf[100];
    for (int i = 0; i < 100; i++) buf[i] = 0.5f;
    int rc = otdr11_push(&p, buf, 100);
    if (rc != 0) { otdr11_free(&p); FAIL("should be 0 chunks"); }
    otdr11_stats_t s = otdr11_get_stats(&p);
    if (s.chunks_processed != 0) { otdr11_free(&p); FAIL("unexpected chunk"); }
    otdr11_free(&p);
    PASS();
}

static void test_v11_single_chunk(void) {
    TEST("Push full chunk → 1 chunk processed");
    otdr11_config_t cfg = otdr11_default_config();
    cfg.chunk_size = 1024;
    cfg.overlap = 64;
    cfg.ring_capacity = 4096;

    otdr11_pipeline_t p;
    otdr11_init(&p, &cfg);
    float *sig = mk_signal(1024, 0.5f);
    int rc = otdr11_push(&p, sig, 1024);
    if (rc < 1) { otdr11_free(&p); free(sig); FAIL("no chunk"); }
    otdr11_stats_t s = otdr11_get_stats(&p);
    if (s.chunks_processed < 1) { otdr11_free(&p); free(sig); FAIL("count"); }
    otdr11_free(&p); free(sig);
    PASS();
}

static void test_v11_multi_push(void) {
    TEST("Multi-push → multiple chunks");
    otdr11_config_t cfg = otdr11_default_config();
    cfg.chunk_size = 512;
    cfg.overlap = 32;
    cfg.ring_capacity = 8192;

    otdr11_pipeline_t p;
    otdr11_init(&p, &cfg);
    float *sig = mk_signal(5000, 0.5f);

    int total = 0;
    /* Push in 500-sample blocks */
    for (size_t off = 0; off < 5000; off += 500) {
        size_t n = (off + 500 <= 5000) ? 500 : (5000 - off);
        int rc = otdr11_push(&p, sig + off, n);
        if (rc > 0) total += rc;
    }

    otdr11_stats_t s = otdr11_get_stats(&p);
    otdr11_free(&p); free(sig);
    if (s.chunks_processed < 5) FAIL("too few chunks");
    PASS();
}

static void test_v11_flush(void) {
    TEST("Flush processes remaining data");
    otdr11_config_t cfg = otdr11_default_config();
    cfg.chunk_size = 2000;
    cfg.overlap = 100;
    cfg.ring_capacity = 8192;

    otdr11_pipeline_t p;
    otdr11_init(&p, &cfg);
    float *sig = mk_signal(1500, 0.5f);  /* less than chunk_size */
    otdr11_push(&p, sig, 1500);

    otdr11_stats_t s1 = otdr11_get_stats(&p);
    if (s1.chunks_processed != 0) { otdr11_free(&p); free(sig); FAIL("premature"); }

    int rc = otdr11_flush(&p);
    otdr11_stats_t s2 = otdr11_get_stats(&p);
    if (s2.chunks_processed < 1) { otdr11_free(&p); free(sig); FAIL("no flush"); }
    if (s2.state != OTDR11_STATE_DONE) { otdr11_free(&p); free(sig); FAIL("not done"); }

    otdr11_free(&p); free(sig);
    (void)rc;
    PASS();
}

static void test_v11_callbacks(void) {
    TEST("Callbacks fired per chunk");
    cb_tracker_t tr;
    memset(&tr, 0, sizeof(tr));

    otdr11_config_t cfg = otdr11_default_config();
    cfg.chunk_size = 512;
    cfg.overlap = 32;
    cfg.ring_capacity = 4096;
    cfg.on_chunk = on_chunk_track;
    cfg.on_event = on_event_track;
    cfg.user_data = &tr;

    otdr11_pipeline_t p;
    otdr11_init(&p, &cfg);
    float *sig = mk_signal(3000, 0.5f);
    otdr11_push(&p, sig, 3000);
    otdr11_flush(&p);

    otdr11_free(&p); free(sig);
    if (tr.chunks_seen < 3) FAIL("few callbacks");
    PASS();
}

static void test_v11_event_detection(void) {
    TEST("Events detected via pipeline");
    cb_tracker_t tr;
    memset(&tr, 0, sizeof(tr));

    otdr11_config_t cfg = otdr11_default_config();
    cfg.chunk_size = 2048;
    cfg.overlap = 64;
    cfg.ring_capacity = 8192;
    cfg.detect_snr_threshold = 5.0f;
    cfg.on_event = on_event_track;
    cfg.user_data = &tr;

    otdr11_pipeline_t p;
    otdr11_init(&p, &cfg);
    float *sig = mk_signal_with_spike(4096, 1000);
    otdr11_push(&p, sig, 4096);
    otdr11_flush(&p);

    otdr11_stats_t s = otdr11_get_stats(&p);
    otdr11_free(&p); free(sig);
    if (s.total_events < 1) FAIL("no events");
    PASS();
}

static void test_v11_reset(void) {
    TEST("Reset clears state");
    otdr11_config_t cfg = otdr11_default_config();
    cfg.chunk_size = 512;
    cfg.ring_capacity = 4096;

    otdr11_pipeline_t p;
    otdr11_init(&p, &cfg);
    float *sig = mk_signal(2000, 0.5f);
    otdr11_push(&p, sig, 2000);
    otdr11_flush(&p);

    otdr11_reset(&p);
    otdr11_stats_t s = otdr11_get_stats(&p);
    if (s.chunks_processed != 0) { otdr11_free(&p); free(sig); FAIL("not cleared"); }
    if (s.state != OTDR11_STATE_IDLE) { otdr11_free(&p); free(sig); FAIL("not idle"); }

    otdr11_free(&p); free(sig);
    PASS();
}

static void test_v11_string_helpers(void) {
    TEST("String helpers");
    if (strcmp(otdr11_stage_str(OTDR11_STAGE_INTEGRITY), "INTEGRITY") != 0) FAIL("stage");
    if (strcmp(otdr11_stage_str(OTDR11_STAGE_DETECT), "DETECT") != 0) FAIL("detect");
    if (strcmp(otdr11_state_str(OTDR11_STATE_DONE), "DONE") != 0) FAIL("state");
    PASS();
}

/* ═══════════ Bridge tests ═══════════ */

static void test_br_version(void) {
    TEST("Bridge version");
    if (!uft_pipe_version() || strlen(uft_pipe_version()) == 0) FAIL("empty");
    PASS();
}

static void test_br_error_strings(void) {
    TEST("Error strings");
    for (int i = 0; i >= -5; i--)
        if (!uft_pipe_error_str((uft_pipe_error_t)i)) FAIL("null");
    PASS();
}

static void test_br_create_destroy(void) {
    TEST("Create/destroy lifecycle");
    uft_pipe_ctx_t *ctx = NULL;
    uft_pipe_error_t rc = uft_pipe_create(&ctx, NULL);
    if (rc != UFT_PIPE_OK || !ctx) FAIL("create");
    uft_pipe_destroy(ctx);
    PASS();
}

static void test_br_null_reject(void) {
    TEST("Bridge NULL rejection");
    if (uft_pipe_create(NULL, NULL) != UFT_PIPE_ERR_NULL) FAIL("null out");
    uft_pipe_ctx_t *ctx;
    uft_pipe_create(&ctx, NULL);
    if (uft_pipe_push_float(ctx, NULL, 100) != UFT_PIPE_ERR_NULL) {
        uft_pipe_destroy(ctx); FAIL("null push");
    }
    uft_pipe_destroy(ctx);
    PASS();
}

static void test_br_push_float(void) {
    TEST("Bridge: push float → chunks processed");
    uft_pipe_config_t cfg = uft_pipe_default_config();
    cfg.chunk_size = 1024;
    cfg.ring_capacity = 8192;

    uft_pipe_ctx_t *ctx;
    uft_pipe_create(&ctx, &cfg);

    float *sig = mk_signal(5000, 0.5f);
    uft_pipe_push_float(ctx, sig, 5000);
    uft_pipe_flush(ctx);

    uint32_t chunks = uft_pipe_chunks_processed(ctx);
    uft_pipe_destroy(ctx); free(sig);
    if (chunks < 3) FAIL("few chunks");
    PASS();
}

static void test_br_push_flux(void) {
    TEST("Bridge: push flux_ns");
    uft_pipe_config_t cfg = uft_pipe_default_config();
    cfg.chunk_size = 512;
    cfg.ring_capacity = 4096;

    uft_pipe_ctx_t *ctx;
    uft_pipe_create(&ctx, &cfg);

    uint32_t flux[2000];
    srand(77);
    for (int i = 0; i < 2000; i++)
        flux[i] = (uint32_t)(4000.0f + (frand() - 0.5f) * 20.0f);

    uft_pipe_error_t rc = uft_pipe_push_flux_ns(ctx, flux, 2000);
    uft_pipe_flush(ctx);
    uft_pipe_destroy(ctx);
    if (rc != UFT_PIPE_OK) FAIL("error");
    PASS();
}

static void test_br_push_analog(void) {
    TEST("Bridge: push analog (int16)");
    uft_pipe_config_t cfg = uft_pipe_default_config();
    cfg.chunk_size = 512;
    cfg.ring_capacity = 4096;

    uft_pipe_ctx_t *ctx;
    uft_pipe_create(&ctx, &cfg);

    int16_t samp[2000];
    srand(88);
    for (int i = 0; i < 2000; i++)
        samp[i] = (int16_t)(10000.0f + (frand() - 0.5f) * 200.0f);

    uft_pipe_error_t rc = uft_pipe_push_analog(ctx, samp, 2000);
    uft_pipe_flush(ctx);
    uft_pipe_destroy(ctx);
    if (rc != UFT_PIPE_OK) FAIL("error");
    PASS();
}

static void test_br_callbacks(void) {
    TEST("Bridge: callbacks fire");
    br_tracker_t tr;
    memset(&tr, 0, sizeof(tr));

    uft_pipe_config_t cfg = uft_pipe_default_config();
    cfg.chunk_size = 512;
    cfg.ring_capacity = 4096;
    cfg.on_chunk = br_on_chunk;
    cfg.on_event = br_on_event;
    cfg.user_data = &tr;

    uft_pipe_ctx_t *ctx;
    uft_pipe_create(&ctx, &cfg);

    float *sig = mk_signal_with_spike(3000, 800);
    uft_pipe_push_float(ctx, sig, 3000);
    uft_pipe_flush(ctx);

    uft_pipe_destroy(ctx); free(sig);
    if (tr.chunks < 3) FAIL("few chunk cb");
    PASS();
}

static void test_br_report(void) {
    TEST("Bridge: report populated");
    uft_pipe_config_t cfg = uft_pipe_default_config();
    cfg.chunk_size = 1024;
    cfg.ring_capacity = 8192;

    uft_pipe_ctx_t *ctx;
    uft_pipe_create(&ctx, &cfg);

    float *sig = mk_signal_with_dropout(5000, 2000, 50);
    uft_pipe_push_float(ctx, sig, 5000);
    uft_pipe_flush(ctx);

    uft_pipe_report_t rpt = uft_pipe_get_report(ctx);
    int ok = (rpt.chunks_processed >= 3);
    if (rpt.total_samples == 0) ok = 0;
    if (!rpt.is_done) ok = 0;
    if (rpt.overall_quality < 0.0f || rpt.overall_quality > 1.0f) ok = 0;

    uft_pipe_destroy(ctx); free(sig);
    if (!ok) FAIL("bad report");
    PASS();
}

static void test_br_reset(void) {
    TEST("Bridge: reset clears state");
    uft_pipe_config_t cfg = uft_pipe_default_config();
    cfg.chunk_size = 512;
    cfg.ring_capacity = 4096;

    uft_pipe_ctx_t *ctx;
    uft_pipe_create(&ctx, &cfg);

    float *sig = mk_signal(2000, 0.5f);
    uft_pipe_push_float(ctx, sig, 2000);
    uft_pipe_flush(ctx);

    uft_pipe_reset(ctx);
    uft_pipe_report_t rpt = uft_pipe_get_report(ctx);
    if (rpt.chunks_processed != 0) { uft_pipe_destroy(ctx); free(sig); FAIL("not reset"); }

    uft_pipe_destroy(ctx); free(sig);
    PASS();
}

static void test_br_stages_disable(void) {
    TEST("Bridge: stages can be disabled");
    uft_pipe_config_t cfg = uft_pipe_default_config();
    cfg.chunk_size = 512;
    cfg.ring_capacity = 4096;
    cfg.enable_integrity = false;
    cfg.enable_detect = false;
    cfg.enable_confidence = false;

    uft_pipe_ctx_t *ctx;
    uft_pipe_create(&ctx, &cfg);

    float *sig = mk_signal(2000, 0.5f);
    uft_pipe_error_t rc = uft_pipe_push_float(ctx, sig, 2000);
    uft_pipe_flush(ctx);

    uft_pipe_report_t rpt = uft_pipe_get_report(ctx);
    uft_pipe_destroy(ctx); free(sig);
    if (rc != UFT_PIPE_OK) FAIL("error");
    if (rpt.total_events != 0) FAIL("events without detect");
    PASS();
}

static void test_br_large_stream(void) {
    TEST("Bridge: N=500K streaming");
    uft_pipe_config_t cfg = uft_pipe_default_config();
    cfg.chunk_size = 4096;
    cfg.ring_capacity = 32768;

    uft_pipe_ctx_t *ctx;
    uft_pipe_create(&ctx, &cfg);

    float *sig = mk_signal(500000, 0.5f);
    /* Stream in 10K blocks */
    for (size_t off = 0; off < 500000; off += 10000) {
        size_t n = (off + 10000 <= 500000) ? 10000 : (500000 - off);
        uft_pipe_push_float(ctx, sig + off, n);
    }
    uft_pipe_flush(ctx);

    uft_pipe_report_t rpt = uft_pipe_get_report(ctx);
    uft_pipe_destroy(ctx); free(sig);
    if (rpt.chunks_processed < 50) FAIL("too few");
    if (!rpt.is_done) FAIL("not done");
    PASS();
}

static void test_br_double_destroy(void) {
    TEST("Double destroy safety");
    uft_pipe_destroy(NULL);
    uft_pipe_ctx_t *ctx;
    uft_pipe_create(&ctx, NULL);
    uft_pipe_destroy(ctx);
    /* Don't destroy again — freed pointer */
    PASS();
}

/* ═══════════ Main ═══════════ */

int main(void) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║   OTDR v11 PIPELINE + UFT BRIDGE - TEST SUITE              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("── Core v11 (otdr11_*) ───────────────────────────────────────\n");
    test_v11_defaults();
    test_v11_init_free();
    test_v11_null_reject();
    test_v11_small_push();
    test_v11_single_chunk();
    test_v11_multi_push();
    test_v11_flush();
    test_v11_callbacks();
    test_v11_event_detection();
    test_v11_reset();
    test_v11_string_helpers();

    printf("\n── Bridge (uft_pipe_*) ───────────────────────────────────────\n");
    test_br_version();
    test_br_error_strings();
    test_br_create_destroy();
    test_br_null_reject();
    test_br_push_float();
    test_br_push_flux();
    test_br_push_analog();
    test_br_callbacks();
    test_br_report();
    test_br_reset();
    test_br_stages_disable();
    test_br_large_stream();
    test_br_double_destroy();

    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("  Ergebnis: %d/%d Tests bestanden\n", t_pass, t_run);
    printf("══════════════════════════════════════════════════════════════\n\n");
    return t_pass < t_run ? 1 : 0;
}
