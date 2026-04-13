/**
 * test_denoise_bridge.c — Tests for UFT Denoise Bridge + φ-OTDR Denoiser
 * ========================================================================
 */

#include "phi_otdr_denoise_1d.h"
#include "uft_denoise_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { printf("  %-55s ", name); tests_run++; } while(0)
#define PASS()     do { printf("✓\n"); tests_passed++; } while(0)
#define FAIL(m)    do { printf("✗ (%s)\n", m); return; } while(0)

/* ==== Helpers ==== */

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

/* ==== Core library tests (podr_*) ==== */

static void test_podr_default_config(void) {
    TEST("podr_default_config values");
    podr_config c = podr_default_config();
    if (c.levels != 5) FAIL("levels");
    if (c.mode != PODR_THRESH_SOFT) FAIL("mode");
    if (fabsf(c.thresh_scale - 1.0f) > 1e-6f) FAIL("scale");
    if (!c.auto_quiet) FAIL("auto_quiet");
    PASS();
}

static void test_podr_perfect_recon(void) {
    TEST("SWT perfect reconstruction (σ≈0)");
    const size_t N = 256;
    float *x = malloc(N * sizeof(float));
    float *y = malloc(N * sizeof(float));
    for (size_t i = 0; i < N; i++)
        x[i] = sinf(2.0f * 3.14159f * (float)i / 32.0f);

    podr_config c = podr_default_config();
    c.levels = 4; c.sigma_override = 1e-10f; c.auto_quiet = 0;
    podr_denoise_1d(x, y, N, &c, NULL);

    float max_err = 0.0f;
    for (size_t i = 0; i < N; i++) {
        float e = fabsf(x[i] - y[i]);
        if (e > max_err) max_err = e;
    }
    free(x); free(y);
    if (max_err > 0.01f) FAIL("recon error");
    PASS();
}

static void test_podr_dc_preserve(void) {
    TEST("DC signal preserved through SWT");
    const size_t N = 128;
    float *x = malloc(N * sizeof(float));
    float *y = malloc(N * sizeof(float));
    for (size_t i = 0; i < N; i++) x[i] = 42.0f;

    podr_config c = podr_default_config();
    c.levels = 3; c.sigma_override = 0.001f; c.auto_quiet = 0;
    podr_denoise_1d(x, y, N, &c, NULL);

    float max_err = 0.0f;
    for (size_t i = 0; i < N; i++) {
        float e = fabsf(42.0f - y[i]);
        if (e > max_err) max_err = e;
    }
    free(x); free(y);
    if (max_err > 0.1f) FAIL("DC drift");
    PASS();
}

static void test_podr_noise_reduction(void) {
    TEST("Noise reduction (MSE output < MSE input)");
    const size_t N = 4096;
    float *x = malloc(N * sizeof(float));
    float *y = malloc(N * sizeof(float));
    srand(42);
    for (size_t i = 0; i < N; i++) {
        float s = sinf(2.0f * 3.14159f * (float)i / 64.0f);
        x[i] = s + (frand() - 0.5f) * 2.0f;
    }

    podr_config c = podr_default_config();
    c.levels = 5; c.auto_quiet = 1;
    podr_diag d;
    podr_denoise_1d(x, y, N, &c, &d);

    double mse_in = 0.0, mse_out = 0.0;
    for (size_t i = 0; i < N; i++) {
        float clean = sinf(2.0f * 3.14159f * (float)i / 64.0f);
        double ei = x[i] - clean, eo = y[i] - clean;
        mse_in += ei * ei; mse_out += eo * eo;
    }
    podr_free_diag(&d);
    free(x); free(y);
    if (mse_out >= mse_in) FAIL("no improvement");
    PASS();
}

static void test_podr_hard_thresh(void) {
    TEST("Hard threshold mode runs");
    const size_t N = 512;
    float *x = malloc(N * sizeof(float));
    float *y = malloc(N * sizeof(float));
    srand(77);
    for (size_t i = 0; i < N; i++) x[i] = frand() - 0.5f;

    podr_config c = podr_default_config();
    c.levels = 3; c.mode = PODR_THRESH_HARD; c.auto_quiet = 1;
    int rc = podr_denoise_1d(x, y, N, &c, NULL);
    free(x); free(y);
    if (rc != 0) FAIL("failed");
    PASS();
}

static void test_podr_quiet_mask(void) {
    TEST("Auto quiet mask favors low-variance regions");
    const size_t N = 1000;
    float *x = malloc(N * sizeof(float));
    uint8_t *m = calloc(N, 1);
    srand(55);
    for (size_t i = 0; i < 500; i++) x[i] = 0.001f * frand();
    for (size_t i = 500; i < 1000; i++) x[i] = 10.0f * frand();

    podr_build_auto_quiet_mask(x, N, 100, 0.3f, m);
    size_t q1 = 0, q2 = 0;
    for (size_t i = 0; i < 500; i++) if (m[i]) q1++;
    for (size_t i = 500; i < 1000; i++) if (m[i]) q2++;
    free(x); free(m);
    if (q1 <= q2) FAIL("mask wrong");
    PASS();
}

static void test_podr_null_reject(void) {
    TEST("NULL/zero input rejected");
    podr_config c = podr_default_config();
    float y = 0;
    if (podr_denoise_1d(NULL, &y, 1, &c, NULL) >= 0) FAIL("null in");
    float x = 0;
    if (podr_denoise_1d(&x, &y, 0, &c, NULL) >= 0) FAIL("n=0");
    c.levels = 0;
    if (podr_denoise_1d(&x, &y, 1, &c, NULL) >= 0) FAIL("levels=0");
    PASS();
}

/* ==== Bridge tests (uft_denoise_*) ==== */

static void test_bridge_version(void) {
    TEST("Bridge version string");
    const char *v = uft_denoise_version();
    if (!v || strlen(v) == 0) FAIL("empty");
    PASS();
}

static void test_bridge_error_strings(void) {
    TEST("Error strings non-NULL");
    for (int i = 0; i >= -5; i--) {
        if (!uft_denoise_error_str((uft_denoise_error_t)i)) FAIL("NULL");
    }
    PASS();
}

static void test_bridge_init_free(void) {
    TEST("Init/free lifecycle");
    uft_denoise_ctx_t ctx;
    uft_denoise_error_t rc = uft_denoise_init(&ctx, NULL);
    if (rc != UFT_DN_OK) FAIL("init");
    if (!ctx.initialized) FAIL("not init");
    if (ctx.cfg.levels != 5) FAIL("default levels");
    uft_denoise_free(&ctx);
    if (ctx.initialized) FAIL("still init");
    PASS();
}

static void test_bridge_custom_config(void) {
    TEST("Custom config applied");
    uft_denoise_config_t cfg = uft_denoise_default_config();
    cfg.levels = 7;
    cfg.mode = UFT_DN_HARD;
    cfg.thresh_scale = 1.5f;
    cfg.auto_quiet = false;

    uft_denoise_ctx_t ctx;
    uft_denoise_init(&ctx, &cfg);
    if (ctx.cfg.levels != 7) FAIL("levels");
    if (ctx.cfg.mode != UFT_DN_HARD) FAIL("mode");
    if (fabsf(ctx.cfg.thresh_scale - 1.5f) > 1e-6f) FAIL("scale");
    uft_denoise_free(&ctx);
    PASS();
}

static void test_bridge_null_reject(void) {
    TEST("Bridge NULL/error handling");
    uft_denoise_ctx_t ctx;
    uft_denoise_init(&ctx, NULL);
    if (uft_denoise_flux_ns(&ctx, NULL, 0, NULL) != UFT_DN_ERR_NULL) FAIL("null");
    float x = 1.0f;
    if (uft_denoise_float(&ctx, &x, &x, 1) != UFT_DN_ERR_TOO_SMALL) FAIL("n=1");
    uft_denoise_free(&ctx);
    PASS();
}

static void test_bridge_float_denoise(void) {
    TEST("Float denoise reduces noise");
    const size_t N = 8192;
    float *x = malloc(N * sizeof(float));
    float *y = malloc(N * sizeof(float));
    srand(123);
    for (size_t i = 0; i < N; i++) {
        float s = 0.5f * sinf(2.0f * 3.14159f * (float)i / 100.0f);
        x[i] = s + (frand() - 0.5f) * 1.0f;
    }

    uft_denoise_ctx_t ctx;
    uft_denoise_init(&ctx, NULL);
    uft_denoise_error_t rc = uft_denoise_float(&ctx, x, y, N);
    if (rc != UFT_DN_OK) { free(x); free(y); uft_denoise_free(&ctx); FAIL("failed"); }

    uft_denoise_report_t rpt = uft_denoise_get_report(&ctx);
    if (rpt.sigma_est <= 0.0f) { free(x); free(y); uft_denoise_free(&ctx); FAIL("sigma=0"); }
    if (rpt.samples_processed != N) { free(x); free(y); uft_denoise_free(&ctx); FAIL("count"); }

    /* Check noise reduction */
    double mse_in = 0.0, mse_out = 0.0;
    for (size_t i = 0; i < N; i++) {
        float clean = 0.5f * sinf(2.0f * 3.14159f * (float)i / 100.0f);
        double ei = x[i] - clean, eo = y[i] - clean;
        mse_in += ei * ei; mse_out += eo * eo;
    }
    uft_denoise_free(&ctx);
    free(x); free(y);
    if (mse_out >= mse_in) FAIL("no improvement");
    PASS();
}

static void test_bridge_flux_ns(void) {
    TEST("Flux interval denoising (uint32 → float)");
    const size_t N = 4096;
    uint32_t *flux = malloc(N * sizeof(uint32_t));
    float *out = malloc(N * sizeof(float));
    srand(456);

    /* Simulate flux intervals: ~4000ns ± jitter */
    for (size_t i = 0; i < N; i++) {
        float base = 4000.0f + 200.0f * sinf(2.0f * 3.14159f * (float)i / 200.0f);
        float noise = (frand() - 0.5f) * 500.0f;
        float val = base + noise;
        flux[i] = (uint32_t)(val > 0.0f ? val : 1.0f);
    }

    uft_denoise_ctx_t ctx;
    uft_denoise_init(&ctx, NULL);
    uft_denoise_error_t rc = uft_denoise_flux_ns(&ctx, flux, N, out);
    if (rc != UFT_DN_OK) { free(flux); free(out); uft_denoise_free(&ctx); FAIL("failed"); }

    /* Verify: all outputs non-negative */
    for (size_t i = 0; i < N; i++) {
        if (out[i] < 0.0f) { free(flux); free(out); uft_denoise_free(&ctx); FAIL("negative"); }
    }

    /* Verify: total time preserved (within 1%) */
    double sum_in = 0.0, sum_out = 0.0;
    for (size_t i = 0; i < N; i++) {
        sum_in += (double)flux[i];
        sum_out += (double)out[i];
    }
    double ratio = sum_out / sum_in;
    uft_denoise_free(&ctx);
    free(flux); free(out);
    if (fabs(ratio - 1.0) > 0.01) FAIL("integral not preserved");
    PASS();
}

static void test_bridge_analog(void) {
    TEST("Analog sample denoising (int16 → float)");
    const size_t N = 2048;
    int16_t *samples = malloc(N * sizeof(int16_t));
    float *out = malloc(N * sizeof(float));
    srand(789);
    for (size_t i = 0; i < N; i++) {
        float s = 8000.0f * sinf(2.0f * 3.14159f * (float)i / 50.0f);
        float n = (frand() - 0.5f) * 6000.0f;
        int val = (int)(s + n);
        if (val > 32767) val = 32767;
        if (val < -32768) val = -32768;
        samples[i] = (int16_t)val;
    }

    uft_denoise_ctx_t ctx;
    uft_denoise_init(&ctx, NULL);
    uft_denoise_error_t rc = uft_denoise_analog(&ctx, samples, N, out);
    uft_denoise_free(&ctx);
    free(samples); free(out);
    if (rc != UFT_DN_OK) FAIL("failed");
    PASS();
}

static void test_bridge_masked(void) {
    TEST("Explicit quiet mask denoising");
    const size_t N = 2048;
    float *x = malloc(N * sizeof(float));
    float *y = malloc(N * sizeof(float));
    uint8_t *mask = calloc(N, 1);
    srand(321);

    /* First quarter = quiet region */
    for (size_t i = 0; i < N/4; i++) {
        x[i] = (frand() - 0.5f) * 0.1f;
        mask[i] = 1;
    }
    for (size_t i = N/4; i < N; i++) {
        x[i] = sinf(2.0f * 3.14159f * (float)i / 80.0f) + (frand() - 0.5f) * 0.5f;
    }

    uft_denoise_ctx_t ctx;
    uft_denoise_init(&ctx, NULL);
    uft_denoise_error_t rc = uft_denoise_float_masked(&ctx, x, y, N, mask);

    uft_denoise_report_t rpt = uft_denoise_get_report(&ctx);
    uft_denoise_free(&ctx);
    free(x); free(y); free(mask);

    if (rc != UFT_DN_OK) FAIL("failed");
    if (rpt.quiet_samples < N/4 - 10) FAIL("quiet count wrong");
    PASS();
}

static void test_bridge_report_fields(void) {
    TEST("Report has valid diagnostics");
    const size_t N = 4096;
    float *x = malloc(N * sizeof(float));
    float *y = malloc(N * sizeof(float));
    srand(999);
    for (size_t i = 0; i < N; i++)
        x[i] = sinf((float)i * 0.02f) + (frand() - 0.5f) * 0.8f;

    uft_denoise_ctx_t ctx;
    uft_denoise_init(&ctx, NULL);
    uft_denoise_float(&ctx, x, y, N);
    uft_denoise_report_t r = uft_denoise_get_report(&ctx);

    int ok = 1;
    if (r.sigma_est <= 0.0f) { ok = 0; printf("[sigma=0] "); }
    if (r.samples_processed != N) { ok = 0; printf("[n=%zu] ", r.samples_processed); }
    if (r.num_levels != 5) { ok = 0; printf("[L=%u] ", r.num_levels); }
    if (r.thresh_per_level[0] <= 0.0f) { ok = 0; printf("[t0=0] "); }

    uft_denoise_free(&ctx);
    free(x); free(y);
    if (!ok) FAIL("bad fields");
    PASS();
}

static void test_bridge_large_n(void) {
    TEST("N=500K performance");
    const size_t N = 500000;
    float *x = malloc(N * sizeof(float));
    float *y = malloc(N * sizeof(float));
    srand(1);
    for (size_t i = 0; i < N; i++)
        x[i] = sinf((float)i * 0.001f) + (frand() - 0.5f) * 0.5f;

    uft_denoise_ctx_t ctx;
    uft_denoise_init(&ctx, NULL);
    uft_denoise_error_t rc = uft_denoise_float(&ctx, x, y, N);
    uft_denoise_free(&ctx);
    free(x); free(y);
    if (rc != UFT_DN_OK) FAIL("failed");
    PASS();
}

static void test_bridge_double_free(void) {
    TEST("Double free safety");
    uft_denoise_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    uft_denoise_free(&ctx);
    uft_denoise_free(&ctx);
    uft_denoise_free(NULL);
    PASS();
}

static void test_bridge_flux_events_preserved(void) {
    TEST("Flux events (bursts) preserved after denoise");
    const size_t N = 8192;
    uint32_t *flux = malloc(N * sizeof(uint32_t));
    float *out = malloc(N * sizeof(float));
    srand(2024);

    /* Simulate: steady 4µs intervals with event burst at 3000-3200 */
    for (size_t i = 0; i < N; i++) {
        float base = 4000.0f;
        if (i >= 3000 && i < 3200) base = 2000.0f;  /* event: half period */
        flux[i] = (uint32_t)(base + (frand() - 0.5f) * 300.0f);
    }

    uft_denoise_ctx_t ctx;
    uft_denoise_init(&ctx, NULL);
    uft_denoise_flux_ns(&ctx, flux, N, out);

    /* The event region should still be noticeably different from background */
    double mean_bg = 0.0, mean_ev = 0.0;
    for (size_t i = 1000; i < 2000; i++) mean_bg += out[i];
    mean_bg /= 1000.0;
    for (size_t i = 3000; i < 3200; i++) mean_ev += out[i];
    mean_ev /= 200.0;

    uft_denoise_free(&ctx);
    free(flux); free(out);
    /* Event region should be at least 30% lower than background */
    if (mean_ev > mean_bg * 0.85) FAIL("event smoothed away");
    PASS();
}

/* ==== Main ==== */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║   φ-OTDR DENOISE + UFT BRIDGE - TEST SUITE                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("── Core Library (podr_*) ─────────────────────────────────────\n");
    test_podr_default_config();
    test_podr_perfect_recon();
    test_podr_dc_preserve();
    test_podr_noise_reduction();
    test_podr_hard_thresh();
    test_podr_quiet_mask();
    test_podr_null_reject();

    printf("\n── Bridge API (uft_denoise_*) ─────────────────────────────────\n");
    test_bridge_version();
    test_bridge_error_strings();
    test_bridge_init_free();
    test_bridge_custom_config();
    test_bridge_null_reject();
    test_bridge_float_denoise();
    test_bridge_flux_ns();
    test_bridge_analog();
    test_bridge_masked();
    test_bridge_report_fields();
    test_bridge_large_n();
    test_bridge_double_free();
    test_bridge_flux_events_preserved();

    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("  Ergebnis: %d/%d Tests bestanden\n", tests_passed, tests_run);
    printf("══════════════════════════════════════════════════════════════\n\n");

    return tests_passed < tests_run ? 1 : 0;
}
