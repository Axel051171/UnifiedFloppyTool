/**
 * test_integrity_v9.c — Tests for OTDR v9 Integrity + UFT Bridge
 */

#include "otdr_event_core_v9.h"
#include "uft_integrity_bridge.h"

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

/* Clean signal */
static float *mk_clean(size_t n, float base) {
    float *a = calloc(n, sizeof(float));
    srand(42);
    for (size_t i = 0; i < n; i++)
        a[i] = base + (frand() - 0.5f) * 0.01f;
    return a;
}

/* Signal with dropout region */
static float *mk_dropout(size_t n, size_t dstart, size_t dlen) {
    float *a = mk_clean(n, 0.5f);
    for (size_t i = dstart; i < dstart + dlen && i < n; i++)
        a[i] = 0.0f;
    return a;
}

/* Signal with saturation (clipping) */
static float *mk_saturated(size_t n, size_t cstart, size_t clen, float rail) {
    float *a = mk_clean(n, 0.5f);
    for (size_t i = cstart; i < cstart + clen && i < n; i++)
        a[i] = rail;
    return a;
}

/* Signal with stuck-at fault */
static float *mk_stuck(size_t n, size_t sstart, size_t slen, float val) {
    float *a = mk_clean(n, 0.5f);
    for (size_t i = sstart; i < sstart + slen && i < n; i++)
        a[i] = val;
    return a;
}

/* Signal with multiple anomalies */
static float *mk_multi_anom(size_t n) {
    float *a = mk_clean(n, 0.5f);
    /* Dropout at 2000 */
    for (size_t i = 2000; i < 2020 && i < n; i++) a[i] = 0.0f;
    /* Clipping at 5000 */
    for (size_t i = 5000; i < 5010 && i < n; i++) a[i] = 0.99f;
    /* Stuck at 8000 */
    for (size_t i = 8000; i < 8030 && i < n; i++) a[i] = 0.333f;
    return a;
}

/* ═══════════ Core v9 tests ═══════════ */

static void test_v9_defaults(void) {
    TEST("v9 default config valid");
    otdr9_config_t c = otdr9_default_config();
    if (c.dropout_min_run != 3) FAIL("dropout_min");
    if (c.clip_min_run != 2) FAIL("clip_min");
    if (c.stuck_min_run != 5) FAIL("stuck_min");
    if (c.deadzone_min_run != 64) FAIL("dz_min");
    if (fabsf(c.mad_scale - 1.4826f) > 0.001f) FAIL("mad");
    PASS();
}

static void test_v9_null_reject(void) {
    TEST("v9 NULL/zero rejection");
    uint8_t flags[8];
    otdr9_region_t reg[8];
    int rc = otdr9_scan(NULL, 0, NULL, flags, reg, 8, NULL);
    if (rc >= 0) FAIL("null accepted");
    PASS();
}

static void test_v9_clean_no_anom(void) {
    TEST("Clean signal → no anomalies");
    const size_t N = 5000;
    float *amp = mk_clean(N, 0.5f);
    uint8_t *flags = calloc(N, 1);
    otdr9_region_t reg[256];
    otdr9_summary_t summ;

    int nreg = otdr9_scan(amp, N, NULL, flags, reg, 256, &summ);

    int ok = (nreg >= 0);
    if (summ.dropout_count > 0) ok = 0;
    if (summ.saturated_count > 0) ok = 0;
    if (summ.stuck_count > 0) ok = 0;
    /* Dead zones may appear due to noise, that's OK */

    free(amp); free(flags);
    if (!ok) FAIL("found anomalies");
    PASS();
}

static void test_v9_dropout_detect(void) {
    TEST("Dropout detection: 20 zero samples");
    const size_t N = 5000;
    float *amp = mk_dropout(N, 2000, 20);
    uint8_t *flags = calloc(N, 1);
    otdr9_region_t reg[256];
    otdr9_summary_t summ;

    otdr9_scan(amp, N, NULL, flags, reg, 256, &summ);

    int ok = (summ.dropout_count >= 1);
    /* Check flags at dropout location */
    if (!(flags[2010] & OTDR9_FLAG_DROPOUT)) ok = 0;
    /* Clean area should be unflagged */
    if (flags[100] & OTDR9_FLAG_DROPOUT) ok = 0;

    free(amp); free(flags);
    if (!ok) FAIL("dropout missed");
    PASS();
}

static void test_v9_clip_high(void) {
    TEST("Clipping detection: high rail");
    const size_t N = 3000;
    float *amp = mk_saturated(N, 1000, 10, 0.995f);
    uint8_t *flags = calloc(N, 1);
    otdr9_region_t reg[256];
    otdr9_summary_t summ;

    otdr9_scan(amp, N, NULL, flags, reg, 256, &summ);

    int ok = (summ.saturated_count >= 1);
    if (!(flags[1005] & OTDR9_FLAG_CLIPPED_HIGH)) ok = 0;

    free(amp); free(flags);
    if (!ok) FAIL("clip missed");
    PASS();
}

static void test_v9_clip_low(void) {
    TEST("Clipping detection: low rail");
    const size_t N = 3000;
    float *amp = mk_saturated(N, 1000, 10, -0.995f);
    uint8_t *flags = calloc(N, 1);
    otdr9_region_t reg[256];
    otdr9_summary_t summ;

    otdr9_scan(amp, N, NULL, flags, reg, 256, &summ);

    int ok = (summ.saturated_count >= 1);
    if (!(flags[1005] & OTDR9_FLAG_CLIPPED_LOW)) ok = 0;

    free(amp); free(flags);
    if (!ok) FAIL("low clip missed");
    PASS();
}

static void test_v9_stuck_at(void) {
    TEST("Stuck-at detection: 30 constant samples");
    const size_t N = 5000;
    float *amp = mk_stuck(N, 2000, 30, 0.333f);
    uint8_t *flags = calloc(N, 1);
    otdr9_region_t reg[256];
    otdr9_summary_t summ;

    otdr9_scan(amp, N, NULL, flags, reg, 256, &summ);

    int ok = (summ.stuck_count >= 1);
    if (!(flags[2015] & OTDR9_FLAG_STUCK)) ok = 0;

    free(amp); free(flags);
    if (!ok) FAIL("stuck missed");
    PASS();
}

static void test_v9_multi_anomalies(void) {
    TEST("Multiple anomalies: dropout + clip + stuck");
    const size_t N = 10000;
    float *amp = mk_multi_anom(N);
    uint8_t *flags = calloc(N, 1);
    otdr9_region_t reg[256];
    otdr9_summary_t summ;

    otdr9_scan(amp, N, NULL, flags, reg, 256, &summ);

    int ok = (summ.dropout_count >= 1);
    if (summ.saturated_count < 1) ok = 0;
    if (summ.stuck_count < 1) ok = 0;

    free(amp); free(flags);
    if (!ok) FAIL("missing types");
    PASS();
}

static void test_v9_exclude_flag(void) {
    TEST("EXCLUDE flag set on anomalies");
    const size_t N = 3000;
    float *amp = mk_dropout(N, 1000, 10);
    uint8_t *flags = calloc(N, 1);
    otdr9_region_t reg[64];

    otdr9_scan(amp, N, NULL, flags, reg, 64, NULL);

    int ok = !!(flags[1005] & OTDR9_FLAG_EXCLUDE);

    free(amp); free(flags);
    if (!ok) FAIL("no exclude");
    PASS();
}

static void test_v9_repair(void) {
    TEST("Repair: interpolates dropout");
    const size_t N = 1000;
    float *amp = mk_dropout(N, 400, 10);
    uint8_t *flags = calloc(N, 1);
    otdr9_region_t reg[64];

    otdr9_scan(amp, N, NULL, flags, reg, 64, NULL);

    /* Before repair: dropout samples ≈ 0 */
    float before = amp[405];

    size_t repaired = otdr9_repair(amp, N, flags);

    /* After repair: should be interpolated ≈ 0.5 */
    float after = amp[405];
    int ok = (repaired >= 10);
    if (fabsf(after) < 0.1f) ok = 0;  /* should be non-zero now */
    if (!(flags[405] & OTDR9_FLAG_REPAIRED)) ok = 0;

    free(amp); free(flags);
    if (!ok) FAIL("bad repair");
    (void)before;
    PASS();
}

static void test_v9_integrity_score(void) {
    TEST("Integrity score: clean > damaged");
    const size_t N = 5000;

    float *clean = mk_clean(N, 0.5f);
    uint8_t *fl1 = calloc(N, 1);
    otdr9_region_t r1[64];
    otdr9_summary_t s1;
    otdr9_scan(clean, N, NULL, fl1, r1, 64, &s1);
    free(clean); free(fl1);

    float *damaged = mk_multi_anom(N);
    uint8_t *fl2 = calloc(N, 1);
    otdr9_region_t r2[64];
    otdr9_summary_t s2;
    otdr9_scan(damaged, N, NULL, fl2, r2, 64, &s2);
    free(damaged); free(fl2);

    if (s1.integrity_score <= s2.integrity_score) FAIL("clean not better");
    PASS();
}

static void test_v9_string_helpers(void) {
    TEST("String helpers");
    if (strcmp(otdr9_anomaly_str(OTDR9_ANOM_DROPOUT), "DROPOUT") != 0) FAIL("anom");
    if (strcmp(otdr9_anomaly_str(OTDR9_ANOM_SATURATED), "SATURATED") != 0) FAIL("sat");
    if (strcmp(otdr9_flag_str(OTDR9_FLAG_STUCK), "STUCK") != 0) FAIL("flag");
    if (strcmp(otdr9_flag_str(OTDR9_FLAG_OK), "OK") != 0) FAIL("ok");
    PASS();
}

/* ═══════════ Bridge tests ═══════════ */

static void test_br_version(void) {
    TEST("Bridge version");
    if (!uft_integrity_version()) FAIL("null");
    PASS();
}

static void test_br_error_strings(void) {
    TEST("Error strings");
    for (int i = 0; i >= -4; i--)
        if (!uft_integrity_error_str((uft_integrity_error_t)i)) FAIL("null");
    PASS();
}

static void test_br_type_strings(void) {
    TEST("Type strings: all 5 types");
    const char *names[] = {"NORMAL","DROPOUT","SATURATED","STUCK","DEADZONE"};
    for (int i = 0; i < 5; i++)
        if (strcmp(uft_integrity_type_str((uft_integrity_type_t)i), names[i]) != 0) FAIL("mismatch");
    PASS();
}

static void test_br_init_free(void) {
    TEST("Init/free lifecycle");
    uft_integrity_ctx_t ctx;
    if (uft_integrity_init(&ctx, NULL) != UFT_INT_OK) FAIL("init");
    if (!ctx.initialized) FAIL("not init");
    uft_integrity_free(&ctx);
    if (ctx.initialized) FAIL("still init");
    PASS();
}

static void test_br_null_reject(void) {
    TEST("Bridge NULL/small rejection");
    uft_integrity_ctx_t ctx;
    uft_integrity_init(&ctx, NULL);
    if (uft_integrity_scan_float(&ctx, NULL, 100) != UFT_INT_ERR_NULL) FAIL("null");
    float x[] = {1,2,3};
    if (uft_integrity_scan_float(&ctx, x, 3) != UFT_INT_ERR_SMALL) FAIL("small");
    uft_integrity_free(&ctx);
    PASS();
}

static void test_br_dropout(void) {
    TEST("Bridge: dropout at target position");
    const size_t N = 5000;
    float *amp = mk_dropout(N, 2000, 15);
    uft_integrity_ctx_t ctx;
    uft_integrity_init(&ctx, NULL);
    uft_integrity_scan_float(&ctx, amp, N);

    uft_integrity_report_t rpt = uft_integrity_get_report(&ctx);
    int ok = (rpt.dropout_count >= 1);

    /* Check region position */
    int near = 0;
    for (size_t i = 0; i < uft_integrity_count(&ctx); i++) {
        const uft_integrity_region_t *r = uft_integrity_get(&ctx, i);
        if (r && r->type == UFT_INT_DROPOUT && r->start >= 1995 && r->start <= 2005)
            near = 1;
    }
    if (!near) ok = 0;

    uft_integrity_free(&ctx); free(amp);
    if (!ok) FAIL("dropout missed");
    PASS();
}

static void test_br_saturated(void) {
    TEST("Bridge: saturation detected");
    const size_t N = 3000;
    float *amp = mk_saturated(N, 1000, 10, 0.995f);
    uft_integrity_ctx_t ctx;
    uft_integrity_init(&ctx, NULL);
    uft_integrity_scan_float(&ctx, amp, N);
    uft_integrity_report_t rpt = uft_integrity_get_report(&ctx);
    uft_integrity_free(&ctx); free(amp);
    if (rpt.saturated_count < 1) FAIL("no saturation");
    PASS();
}

static void test_br_stuck(void) {
    TEST("Bridge: stuck-at detected");
    const size_t N = 5000;
    float *amp = mk_stuck(N, 2000, 25, 0.777f);
    uft_integrity_ctx_t ctx;
    uft_integrity_init(&ctx, NULL);
    uft_integrity_scan_float(&ctx, amp, N);
    uft_integrity_report_t rpt = uft_integrity_get_report(&ctx);
    uft_integrity_free(&ctx); free(amp);
    if (rpt.stuck_count < 1) FAIL("no stuck");
    PASS();
}

static void test_br_flags_array(void) {
    TEST("Flags array accessible + correct length");
    const size_t N = 3000;
    float *amp = mk_dropout(N, 1000, 10);
    uft_integrity_ctx_t ctx;
    uft_integrity_init(&ctx, NULL);
    uft_integrity_scan_float(&ctx, amp, N);

    size_t flen = 0;
    const uint8_t *fl = uft_integrity_flags(&ctx, &flen);
    int ok = (fl != NULL && flen == N);
    if (ok && !(fl[1005] & UFT_INT_FLAG_DROPOUT)) ok = 0;

    uft_integrity_free(&ctx); free(amp);
    if (!ok) FAIL("bad flags");
    PASS();
}

static void test_br_repair(void) {
    TEST("Bridge repair interpolates dropouts");
    const size_t N = 2000;
    float *amp = mk_dropout(N, 800, 12);
    uft_integrity_ctx_t ctx;
    uft_integrity_init(&ctx, NULL);
    uft_integrity_scan_float(&ctx, amp, N);

    size_t repaired = uft_integrity_repair(&ctx, amp, N);
    int ok = (repaired >= 12);
    if (fabsf(amp[806]) < 0.1f) ok = 0;  /* should be interpolated */

    uft_integrity_report_t rpt = uft_integrity_get_report(&ctx);
    if (rpt.repaired_samples < 12) ok = 0;

    uft_integrity_free(&ctx); free(amp);
    if (!ok) FAIL("repair failed");
    PASS();
}

static void test_br_flux_ns(void) {
    TEST("Flux interval scan (uint32)");
    const size_t N = 2000;
    uint32_t *flux = malloc(N * sizeof(uint32_t));
    srand(333);
    for (size_t i = 0; i < N; i++) {
        flux[i] = (uint32_t)(4000.0f + (frand() - 0.5f) * 20.0f);
        if (i >= 800 && i < 810) flux[i] = 0;  /* dropout */
    }
    uft_integrity_ctx_t ctx;
    uft_integrity_init(&ctx, NULL);
    uft_integrity_error_t rc = uft_integrity_scan_flux_ns(&ctx, flux, N);
    uft_integrity_free(&ctx); free(flux);
    if (rc != UFT_INT_OK) FAIL("failed");
    PASS();
}

static void test_br_analog(void) {
    TEST("Analog scan (int16)");
    const size_t N = 2000;
    int16_t *s = malloc(N * sizeof(int16_t));
    srand(444);
    for (size_t i = 0; i < N; i++) {
        s[i] = (int16_t)(10000.0f + (frand() - 0.5f) * 200.0f);
        if (i >= 500 && i < 512) s[i] = 32767;  /* clipping */
    }
    uft_integrity_ctx_t ctx;
    uft_integrity_init(&ctx, NULL);
    uft_integrity_error_t rc = uft_integrity_scan_analog(&ctx, s, N);
    uft_integrity_free(&ctx); free(s);
    if (rc != UFT_INT_OK) FAIL("failed");
    PASS();
}

static void test_br_report_fields(void) {
    TEST("Report fields populated");
    const size_t N = 10000;
    float *amp = mk_multi_anom(N);
    uft_integrity_ctx_t ctx;
    uft_integrity_init(&ctx, NULL);
    uft_integrity_scan_float(&ctx, amp, N);
    uft_integrity_report_t rpt = uft_integrity_get_report(&ctx);

    int ok = (rpt.samples_analyzed == N);
    if (rpt.total_regions < 3) ok = 0;  /* dropout+clip+stuck */
    if (rpt.flagged_samples == 0) ok = 0;
    if (rpt.integrity_score < 0.0f || rpt.integrity_score > 1.0f) ok = 0;

    uft_integrity_free(&ctx); free(amp);
    if (!ok) FAIL("bad fields");
    PASS();
}

static void test_br_double_free(void) {
    TEST("Double free safety");
    uft_integrity_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    uft_integrity_free(&ctx);
    uft_integrity_free(&ctx);
    uft_integrity_free(NULL);
    PASS();
}

static void test_br_large_n(void) {
    TEST("N=200K performance");
    const size_t N = 200000;
    float *amp = malloc(N * sizeof(float));
    srand(1);
    for (size_t i = 0; i < N; i++) {
        amp[i] = 0.5f + (frand() - 0.5f) * 0.02f;
        if (i >= 60000 && i < 60050) amp[i] = 0.0f;
        if (i >= 120000 && i < 120020) amp[i] = 0.5f;
    }
    uft_integrity_ctx_t ctx;
    uft_integrity_init(&ctx, NULL);
    uft_integrity_error_t rc = uft_integrity_scan_float(&ctx, amp, N);
    uft_integrity_free(&ctx); free(amp);
    if (rc != UFT_INT_OK) FAIL("failed");
    PASS();
}

/* ═══════════ Main ═══════════ */

int main(void) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║   OTDR v9 INTEGRITY + UFT BRIDGE - TEST SUITE              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("── Core v9 (otdr9_*) ─────────────────────────────────────────\n");
    test_v9_defaults();
    test_v9_null_reject();
    test_v9_clean_no_anom();
    test_v9_dropout_detect();
    test_v9_clip_high();
    test_v9_clip_low();
    test_v9_stuck_at();
    test_v9_multi_anomalies();
    test_v9_exclude_flag();
    test_v9_repair();
    test_v9_integrity_score();
    test_v9_string_helpers();

    printf("\n── Bridge (uft_integrity_*) ──────────────────────────────────\n");
    test_br_version();
    test_br_error_strings();
    test_br_type_strings();
    test_br_init_free();
    test_br_null_reject();
    test_br_dropout();
    test_br_saturated();
    test_br_stuck();
    test_br_flags_array();
    test_br_repair();
    test_br_flux_ns();
    test_br_analog();
    test_br_report_fields();
    test_br_double_free();
    test_br_large_n();

    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("  Ergebnis: %d/%d Tests bestanden\n", t_pass, t_run);
    printf("══════════════════════════════════════════════════════════════\n\n");
    return t_pass < t_run ? 1 : 0;
}
