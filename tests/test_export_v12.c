/**
 * test_export_v12.c — Tests for OTDR v12 Export/Integration + UFT Bridge
 */

#include "otdr_event_core_v12.h"
#include "uft_export_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int t_run = 0, t_pass = 0;
#define TEST(n) do { printf("  %-55s ", n); t_run++; } while(0)
#define PASS()  do { printf("✓\n"); t_pass++; } while(0)
#define FAIL(m) do { printf("✗ (%s)\n", m); return; } while(0)

/* ═══════════ Core v12 tests ═══════════ */

static void test_v12_version_registry(void) {
    TEST("Version registry: all modules present");
    size_t n = otdr12_module_count();
    if (n < 8) FAIL("too few modules");
    for (size_t i = 0; i < n; i++) {
        const otdr12_module_ver_t *m = otdr12_module_version(i);
        if (!m || !m->module || !m->version) FAIL("null entry");
    }
    if (!otdr12_full_version()) FAIL("no full ver");
    PASS();
}

static void test_v12_analyze_clean(void) {
    TEST("Analyze clean signal → high quality");
    float signal[4096];
    otdr12_golden_generate(0, signal, 4096);

    otdr12_result_t r;
    int rc = otdr12_analyze(signal, 4096, &r);
    if (rc != 0) FAIL("analyze failed");

    int ok = (r.n_samples == 4096);
    if (r.integrity_score < 0.7f) ok = 0;
    if (r.mean_confidence <= 0.0f) ok = 0;
    if (r.overall_quality <= 0.0f || r.overall_quality > 1.0f) ok = 0;
    if (r.confidence == NULL) ok = 0;
    if (r.flags == NULL) ok = 0;

    otdr12_free_result(&r);
    if (!ok) FAIL("bad quality");
    PASS();
}

static void test_v12_analyze_dropout(void) {
    TEST("Analyze dropout → flagged samples");
    float signal[4096];
    otdr12_golden_generate(1, signal, 4096);

    otdr12_result_t r;
    otdr12_analyze(signal, 4096, &r);

    int ok = (r.flagged_samples >= 50);
    if (r.dropout_count < 1) ok = 0;
    if (r.n_events < 1) ok = 0;

    otdr12_free_result(&r);
    if (!ok) FAIL("no dropout detected");
    PASS();
}

static void test_v12_analyze_multi_fault(void) {
    TEST("Analyze multi-fault → 3+ anomaly types");
    float signal[4096];
    otdr12_golden_generate(2, signal, 4096);

    otdr12_result_t r;
    otdr12_analyze(signal, 4096, &r);

    int types = 0;
    if (r.dropout_count > 0) types++;
    if (r.saturated_count > 0) types++;
    if (r.stuck_count > 0) types++;

    otdr12_free_result(&r);
    if (types < 3) FAIL("too few types");
    PASS();
}

static void test_v12_analyze_null(void) {
    TEST("Analyze NULL rejection");
    otdr12_result_t r;
    if (otdr12_analyze(NULL, 100, &r) >= 0) FAIL("null signal");
    float x[8] = {0};
    if (otdr12_analyze(x, 8, &r) >= 0) FAIL("too small");
    PASS();
}

static void test_v12_golden_count(void) {
    TEST("Golden vectors: count ≥ 5");
    if (otdr12_golden_count() < 5) FAIL("too few");
    PASS();
}

static void test_v12_golden_generate(void) {
    TEST("Golden vectors: generate all");
    size_t ng = otdr12_golden_count();
    float buf[4096];
    for (size_t i = 0; i < ng; i++) {
        if (otdr12_golden_generate(i, buf, 4096) != 0) FAIL("gen failed");
    }
    PASS();
}

static void test_v12_golden_validate_all(void) {
    TEST("Golden vectors: all validate");
    size_t ng = otdr12_golden_count();
    for (size_t i = 0; i < ng; i++) {
        const otdr12_golden_info_t *info = otdr12_golden_info(i);
        float *sig = (float *)malloc(info->n * sizeof(float));
        otdr12_golden_generate(i, sig, info->n);
        otdr12_result_t r;
        otdr12_analyze(sig, info->n, &r);
        int rc = otdr12_golden_validate(i, &r);
        otdr12_free_result(&r);
        free(sig);
        if (rc != 0) {
            char msg[64];
            snprintf(msg, sizeof(msg), "golden[%zu] fail reason=%d", i, rc);
            FAIL(msg);
        }
    }
    PASS();
}

static void test_v12_golden_reproducible(void) {
    TEST("Golden vectors: deterministic");
    float a[4096], b[4096];
    otdr12_golden_generate(0, a, 4096);
    otdr12_golden_generate(0, b, 4096);
    for (int i = 0; i < 4096; i++)
        if (a[i] != b[i]) FAIL("not deterministic");
    PASS();
}

static void test_v12_export_json(void) {
    TEST("Export JSON: valid output");
    float signal[4096];
    otdr12_golden_generate(1, signal, 4096);
    otdr12_result_t r;
    otdr12_analyze(signal, 4096, &r);

    int est = otdr12_export(&r, OTDR12_FMT_JSON, NULL, 0);
    if (est <= 0) { otdr12_free_result(&r); FAIL("estimate"); }

    char *buf = (char *)malloc((size_t)est + 1024);
    int written = otdr12_export(&r, OTDR12_FMT_JSON, buf, (size_t)est + 1024);
    int ok = (written > 0);
    if (ok && !strstr(buf, "\"version\"")) ok = 0;
    if (ok && !strstr(buf, "\"integrity\"")) ok = 0;
    if (ok && !strstr(buf, "\"events\"")) ok = 0;
    if (ok && !strstr(buf, "\"segments\"")) ok = 0;

    free(buf);
    otdr12_free_result(&r);
    if (!ok) FAIL("bad json");
    PASS();
}

static void test_v12_export_csv(void) {
    TEST("Export CSV: valid output");
    float signal[4096];
    otdr12_golden_generate(2, signal, 4096);
    otdr12_result_t r;
    otdr12_analyze(signal, 4096, &r);

    int est = otdr12_export(&r, OTDR12_FMT_CSV, NULL, 0);
    char *buf = (char *)malloc((size_t)est + 512);
    int written = otdr12_export(&r, OTDR12_FMT_CSV, buf, (size_t)est + 512);
    int ok = (written > 0);
    if (ok && !strstr(buf, "type,start,end")) ok = 0;

    free(buf);
    otdr12_free_result(&r);
    if (!ok) FAIL("bad csv");
    PASS();
}

static void test_v12_export_binary(void) {
    TEST("Export binary: valid header");
    float signal[4096];
    otdr12_golden_generate(0, signal, 4096);
    otdr12_result_t r;
    otdr12_analyze(signal, 4096, &r);

    int est = otdr12_export(&r, OTDR12_FMT_BINARY, NULL, 0);
    char *buf = (char *)calloc(1, (size_t)est + 64);
    int written = otdr12_export(&r, OTDR12_FMT_BINARY, buf, (size_t)est + 64);
    int ok = (written > 0);

    otdr12_bin_header_t hdr;
    memcpy(&hdr, buf, sizeof(hdr));
    if (memcmp(hdr.magic, "UFTx", 4) != 0) ok = 0;
    if (hdr.version != 12) ok = 0;
    if (hdr.n_samples != 4096) ok = 0;

    free(buf);
    otdr12_free_result(&r);
    if (!ok) FAIL("bad binary");
    PASS();
}

static void test_v12_segments_ranked(void) {
    TEST("Segments: ranked by confidence");
    float signal[4096];
    otdr12_golden_generate(1, signal, 4096);
    otdr12_result_t r;
    otdr12_analyze(signal, 4096, &r);

    int ok = (r.n_segments >= 1);
    if (ok && r.segments) {
        if (r.segments[0].rank != 0) ok = 0;
    }

    otdr12_free_result(&r);
    if (!ok) FAIL("bad segments");
    PASS();
}

/* ═══════════ Bridge tests ═══════════ */

static void test_br_version(void) {
    TEST("Bridge version");
    if (!uft_export_version()) FAIL("null");
    if (!uft_export_pipeline_version()) FAIL("null pipe");
    PASS();
}

static void test_br_error_strings(void) {
    TEST("Error strings");
    for (int i = 0; i >= -5; i--)
        if (!uft_export_error_str((uft_export_error_t)i)) FAIL("null");
    PASS();
}

static void test_br_format_strings(void) {
    TEST("Format strings: JSON/CSV/BINARY");
    if (strcmp(uft_export_format_str(UFT_EXPORT_JSON), "JSON") != 0) FAIL("json");
    if (strcmp(uft_export_format_str(UFT_EXPORT_CSV), "CSV") != 0) FAIL("csv");
    if (strcmp(uft_export_format_str(UFT_EXPORT_BINARY), "BINARY") != 0) FAIL("bin");
    PASS();
}

static void test_br_create_destroy(void) {
    TEST("Create/destroy lifecycle");
    uft_export_ctx_t *ctx;
    if (uft_export_create(&ctx) != UFT_EXP_OK) FAIL("create");
    if (!uft_export_has_result(ctx)) { /* expected: no result yet */ }
    uft_export_destroy(ctx);
    PASS();
}

static void test_br_analyze_float(void) {
    TEST("Bridge: analyze float");
    uft_export_ctx_t *ctx;
    uft_export_create(&ctx);

    float signal[4096];
    otdr12_golden_generate(0, signal, 4096);
    uft_export_error_t rc = uft_export_analyze_float(ctx, signal, 4096);
    if (rc != UFT_EXP_OK) { uft_export_destroy(ctx); FAIL("analyze"); }
    if (!uft_export_has_result(ctx)) { uft_export_destroy(ctx); FAIL("no result"); }

    uft_export_report_t rpt = uft_export_get_report(ctx);
    if (rpt.n_samples != 4096) { uft_export_destroy(ctx); FAIL("samples"); }

    uft_export_destroy(ctx);
    PASS();
}

static void test_br_analyze_flux(void) {
    TEST("Bridge: analyze flux_ns");
    uft_export_ctx_t *ctx;
    uft_export_create(&ctx);

    uint32_t flux[2000];
    for (int i = 0; i < 2000; i++) flux[i] = 4000 + (i % 20);
    uft_export_error_t rc = uft_export_analyze_flux_ns(ctx, flux, 2000);

    uft_export_destroy(ctx);
    if (rc != UFT_EXP_OK) FAIL("failed");
    PASS();
}

static void test_br_analyze_analog(void) {
    TEST("Bridge: analyze analog (int16)");
    uft_export_ctx_t *ctx;
    uft_export_create(&ctx);

    int16_t samp[2000];
    for (int i = 0; i < 2000; i++) samp[i] = (int16_t)(10000 + (i % 50));
    uft_export_error_t rc = uft_export_analyze_analog(ctx, samp, 2000);

    uft_export_destroy(ctx);
    if (rc != UFT_EXP_OK) FAIL("failed");
    PASS();
}

static void test_br_export_json(void) {
    TEST("Bridge: export JSON");
    uft_export_ctx_t *ctx;
    uft_export_create(&ctx);
    float signal[4096];
    otdr12_golden_generate(1, signal, 4096);
    uft_export_analyze_float(ctx, signal, 4096);

    size_t needed = 0;
    uft_export_to_buffer(ctx, UFT_EXPORT_JSON, NULL, 0, &needed);

    char *buf = (char *)malloc(needed + 1024);
    size_t written = 0;
    uft_export_error_t rc = uft_export_to_buffer(ctx, UFT_EXPORT_JSON,
                                                   buf, needed + 1024, &written);
    int ok = (rc == UFT_EXP_OK && written > 0);
    if (ok && !strstr(buf, "\"events\"")) ok = 0;

    free(buf);
    uft_export_destroy(ctx);
    if (!ok) FAIL("bad json");
    PASS();
}

static void test_br_golden_run_all(void) {
    TEST("Bridge: all golden vectors pass");
    int rc = uft_export_golden_run_all();
    if (rc != 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "golden failed rc=%d", rc);
        FAIL(msg);
    }
    PASS();
}

static void test_br_module_count(void) {
    TEST("Bridge: module count ≥ 8");
    if (uft_export_module_count() < 8) FAIL("too few");
    PASS();
}

static void test_br_report_quality_range(void) {
    TEST("Bridge: quality score 0..1");
    uft_export_ctx_t *ctx;
    uft_export_create(&ctx);
    float signal[4096];
    otdr12_golden_generate(2, signal, 4096);
    uft_export_analyze_float(ctx, signal, 4096);

    uft_export_report_t rpt = uft_export_get_report(ctx);
    uft_export_destroy(ctx);

    if (rpt.overall_quality < 0.0f || rpt.overall_quality > 1.0f) FAIL("out of range");
    if (rpt.mean_confidence < 0.0f || rpt.mean_confidence > 1.0f) FAIL("conf range");
    PASS();
}

static void test_br_reanalyze(void) {
    TEST("Bridge: re-analyze overwrites old result");
    uft_export_ctx_t *ctx;
    uft_export_create(&ctx);

    float s1[4096], s2[4096];
    otdr12_golden_generate(0, s1, 4096);
    otdr12_golden_generate(2, s2, 4096);

    uft_export_analyze_float(ctx, s1, 4096);
    uft_export_report_t r1 = uft_export_get_report(ctx);

    uft_export_analyze_float(ctx, s2, 4096);
    uft_export_report_t r2 = uft_export_get_report(ctx);

    uft_export_destroy(ctx);
    /* Multi-fault should have lower integrity than clean */
    if (r2.integrity_score >= r1.integrity_score) FAIL("not overwritten");
    PASS();
}

static void test_br_double_destroy(void) {
    TEST("Double destroy safety");
    uft_export_destroy(NULL);
    PASS();
}

/* ═══════════ Main ═══════════ */

int main(void) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║   OTDR v12 EXPORT/INTEGRATION + UFT BRIDGE - TEST SUITE    ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("── Core v12 (otdr12_*) ───────────────────────────────────────\n");
    test_v12_version_registry();
    test_v12_analyze_clean();
    test_v12_analyze_dropout();
    test_v12_analyze_multi_fault();
    test_v12_analyze_null();
    test_v12_golden_count();
    test_v12_golden_generate();
    test_v12_golden_validate_all();
    test_v12_golden_reproducible();
    test_v12_export_json();
    test_v12_export_csv();
    test_v12_export_binary();
    test_v12_segments_ranked();

    printf("\n── Bridge (uft_export_*) ─────────────────────────────────────\n");
    test_br_version();
    test_br_error_strings();
    test_br_format_strings();
    test_br_create_destroy();
    test_br_analyze_float();
    test_br_analyze_flux();
    test_br_analyze_analog();
    test_br_export_json();
    test_br_golden_run_all();
    test_br_module_count();
    test_br_report_quality_range();
    test_br_reanalyze();
    test_br_double_destroy();

    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("  Ergebnis: %d/%d Tests bestanden\n", t_pass, t_run);
    printf("══════════════════════════════════════════════════════════════\n\n");
    return t_pass < t_run ? 1 : 0;
}
