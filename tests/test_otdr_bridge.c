/**
 * @file test_otdr_bridge.c
 * @brief Integration tests for UFT ↔ OTDR Bridge
 */
#include "uft/analysis/uft_otdr_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(n) do { printf("  [%02d] %-50s ", ++tests_run, n); fflush(stdout); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(m) do { printf("FAIL: %s\n", m); } while(0)
#define ASSERT_TRUE(c,m) do { if (!(c)) { FAIL(m); return; } } while(0)
#define ASSERT_EQ(a,b,m) ASSERT_TRUE((a)==(b),m)

/* Generate synthetic MFM DD flux (reused from OTDR tests) */
static uint32_t gen_mfm(uint32_t *f, uint32_t max, float jitter_pct) {
    uint32_t pat[] = {4000,4000,4000,6000,6000,8000};
    uint32_t total = 0, i = 0, seed = 42;
    while (total < 200000000 && i < max) {
        f[i] = pat[i % 6];
        if (jitter_pct > 0) {
            seed = seed * 1103515245 + 12345;
            float n = ((float)(seed >> 16) / 32768.0f - 1.0f) * jitter_pct / 100.0f;
            f[i] = (uint32_t)((int32_t)f[i] + (int32_t)(f[i] * n));
            if (f[i] < 500) f[i] = 500;
        }
        total += f[i]; i++;
    }
    return i;
}

/* Build a fake KryoFlux stream from flux intervals */
static uint32_t build_kf_stream(const uint32_t *flux_ns, uint32_t n_flux,
                                uint8_t *out, uint32_t max_out) {
    uint32_t pos = 0;

    /* OOB StreamInfo */
    if (pos + 12 < max_out) {
        out[pos++] = 0x0D; /* OOB */
        out[pos++] = 0x01; /* StreamInfo */
        out[pos++] = 8; out[pos++] = 0; /* length=8 */
        memset(&out[pos], 0, 8); pos += 8;
    }

    /* Index mark at start */
    if (pos + 16 < max_out) {
        out[pos++] = 0x0D; out[pos++] = 0x02; /* OOB Index */
        out[pos++] = 12; out[pos++] = 0;
        memset(&out[pos], 0, 12); pos += 12;
    }

    /* Encode flux as KryoFlux Flux1 or Flux2 */
    for (uint32_t i = 0; i < n_flux && pos + 3 < max_out; i++) {
        uint32_t ticks = (uint32_t)(flux_ns[i] / 20.810 + 0.5);
        if (ticks >= 0x0E && ticks <= 0xFF) {
            /* Flux1 */
            out[pos++] = (uint8_t)ticks;
        } else if (ticks <= 0x7FF) {
            /* Flux2 */
            out[pos++] = (uint8_t)((ticks >> 8) & 0x07);
            out[pos++] = (uint8_t)(ticks & 0xFF);
        } else {
            /* Flux3 */
            out[pos++] = 0x0C;
            out[pos++] = (uint8_t)((ticks >> 8) & 0xFF);
            out[pos++] = (uint8_t)(ticks & 0xFF);
        }
    }

    /* Index mark at end (revolution boundary) */
    if (pos + 16 < max_out) {
        out[pos++] = 0x0D; out[pos++] = 0x02;
        out[pos++] = 12; out[pos++] = 0;
        memset(&out[pos], 0, 12); pos += 12;
    }

    /* OOB EOF */
    if (pos + 4 < max_out) {
        out[pos++] = 0x0D; out[pos++] = 0x0D;
        out[pos++] = 0; out[pos++] = 0;
    }

    return pos;
}

/* Build fake Greaseweazle data from flux intervals */
static uint32_t build_gw_data(const uint32_t *flux_ns, uint32_t n_flux,
                              uint8_t *out, uint32_t max_out) {
    uint32_t pos = 0;
    for (uint32_t i = 0; i < n_flux && pos + 4 < max_out; i++) {
        uint32_t ticks = (uint32_t)(flux_ns[i] / 13.889 + 0.5);
        if (ticks >= 1 && ticks <= 248) {
            out[pos++] = (uint8_t)(ticks - 1);
        } else {
            out[pos++] = 0xFF;
            out[pos++] = 0xFE;
            out[pos++] = (uint8_t)(ticks & 0xFF);
            out[pos++] = (uint8_t)((ticks >> 8) & 0xFF);
        }
    }
    return pos;
}

/* === Tests === */

static void test_init_default(void) {
    TEST("Bridge init (default)");
    uft_otdr_context_t ctx;
    ASSERT_EQ(uft_otdr_init(&ctx, NULL), 0, "init");
    ASSERT_TRUE(ctx.disk != NULL, "disk");
    ASSERT_EQ(ctx.config.encoding, OTDR_ENC_AUTO, "auto");
    uft_otdr_free(&ctx);
    PASS();
}

static void test_init_platform(void) {
    TEST("Bridge init (atari_st)");
    uft_otdr_context_t ctx;
    ASSERT_EQ(uft_otdr_init(&ctx, "atari_st"), 0, "init");
    ASSERT_EQ(ctx.config.encoding, OTDR_ENC_MFM_DD, "DD");
    ASSERT_EQ(ctx.config.expected_sectors, 9u, "9 sec");
    uft_otdr_free(&ctx);
    PASS();
}

static void test_feed_flux_ns(void) {
    TEST("Feed raw flux_ns");
    uft_otdr_context_t ctx;
    uft_otdr_init(&ctx, "atari_st");
    uint32_t flux[40000];
    uint32_t n = gen_mfm(flux, 40000, 5.0f);
    ASSERT_EQ(uft_otdr_feed_flux_ns(&ctx, flux, n, 0, 0, 0), 0, "feed");
    uft_otdr_free(&ctx);
    PASS();
}

static void test_feed_kryoflux(void) {
    TEST("Feed KryoFlux stream");
    uft_otdr_context_t ctx;
    uft_otdr_init(&ctx, "atari_st");

    /* Generate flux and encode as KryoFlux */
    uint32_t flux[10000];
    uint32_t n = gen_mfm(flux, 10000, 3.0f);
    uint8_t *stream = (uint8_t *)malloc(n * 4);
    uint32_t slen = build_kf_stream(flux, n, stream, n * 4);
    ASSERT_TRUE(slen > 0, "encoded");

    ASSERT_EQ(uft_otdr_feed_kryoflux(&ctx, stream, slen, 0, 0), 0, "feed");

    /* Track should have flux loaded */
    const otdr_track_t *t = uft_otdr_get_track(&ctx, 0, 0);
    ASSERT_TRUE(t != NULL, "track");
    ASSERT_TRUE(t->flux_count > 100, "flux loaded");

    free(stream);
    uft_otdr_free(&ctx);
    PASS();
}

static void test_feed_greaseweazle(void) {
    TEST("Feed Greaseweazle data");
    uft_otdr_context_t ctx;
    uft_otdr_init(&ctx, "pc_dd");

    uint32_t flux[10000];
    uint32_t n = gen_mfm(flux, 10000, 3.0f);
    uint8_t *gw = (uint8_t *)malloc(n * 4);
    uint32_t glen = build_gw_data(flux, n, gw, n * 4);
    ASSERT_TRUE(glen > 0, "encoded");

    ASSERT_EQ(uft_otdr_feed_greaseweazle(&ctx, gw, glen, 0, 0), 0, "feed");
    const otdr_track_t *t = uft_otdr_get_track(&ctx, 0, 0);
    ASSERT_TRUE(t != NULL && t->flux_count > 100, "flux loaded");

    free(gw);
    uft_otdr_free(&ctx);
    PASS();
}

static void test_feed_analog(void) {
    TEST("Feed analog samples");
    uft_otdr_context_t ctx;
    uft_otdr_init(&ctx, "atari_st");

    /* Generate a simple square wave at ~250kHz (4µs period) */
    float sample_rate = 10e6; /* 10 MHz */
    uint32_t n_samples = 200000; /* 20ms */
    int16_t *samples = (int16_t *)malloc(n_samples * sizeof(int16_t));
    for (uint32_t i = 0; i < n_samples; i++) {
        /* ~4µs period = 40 samples at 10 MHz */
        samples[i] = ((i % 40) < 20) ? 16000 : -16000;
    }

    ASSERT_EQ(uft_otdr_feed_analog(&ctx, samples, n_samples, sample_rate, 0, 0), 0, "feed");
    const otdr_track_t *t = uft_otdr_get_track(&ctx, 0, 0);
    ASSERT_TRUE(t != NULL && t->flux_count > 100, "flux from analog");

    free(samples);
    uft_otdr_free(&ctx);
    PASS();
}

static void test_full_pipeline(void) {
    TEST("Full pipeline: feed → analyze → report");
    uft_otdr_context_t ctx;
    uft_otdr_init(&ctx, "atari_st");

    /* Load 4 tracks */
    uint32_t flux[40000];
    float jitters[] = {3.0f, 5.0f, 10.0f, 20.0f};
    for (int i = 0; i < 4; i++) {
        uint8_t cyl = (uint8_t)(i / 2);
        uint8_t head = (uint8_t)(i % 2);
        uint32_t n = gen_mfm(flux, 40000, jitters[i]);
        uft_otdr_feed_flux_ns(&ctx, flux, n, cyl, head, 0);
    }

    ASSERT_EQ(uft_otdr_analyze(&ctx), 0, "analyze");

    uft_otdr_report_t report = uft_otdr_get_report(&ctx);
    ASSERT_EQ(report.analyzed_tracks, 4u, "4 analyzed");
    ASSERT_TRUE(report.track_count == 4, "4 summaries");
    ASSERT_TRUE(report.overall_jitter_pct > 0, "jitter>0");
    ASSERT_TRUE(report.worst_track_jitter > 10.0f, "worst>10%");

    /* Track summaries */
    for (uint32_t i = 0; i < report.track_count; i++) {
        ASSERT_TRUE(report.tracks[i].jitter_rms_pct > 0, "track jitter");
    }

    uft_otdr_report_free(&report);
    uft_otdr_free(&ctx);
    PASS();
}

static void test_snr_weights(void) {
    TEST("SNR-weighted consensus");
    uft_otdr_context_t ctx;
    uft_otdr_init(&ctx, "atari_st");

    /* Load 2 revolutions */
    uint32_t flux[40000];
    uint32_t n = gen_mfm(flux, 40000, 5.0f);
    uft_otdr_feed_flux_ns(&ctx, flux, n, 0, 0, 0);
    n = gen_mfm(flux, 40000, 8.0f);
    uft_otdr_feed_flux_ns(&ctx, flux, n, 0, 0, 1);

    uft_otdr_analyze(&ctx);

    float weights[8];
    uint8_t nw = 0;
    ASSERT_EQ(uft_otdr_snr_weights(&ctx, 0, 0, weights, &nw), 0, "weights");
    ASSERT_EQ(nw, 2, "2 weights");

    /* Weights should sum to ~1 */
    float sum = weights[0] + weights[1];
    ASSERT_TRUE(fabsf(sum - 1.0f) < 0.01f, "sum≈1");

    uft_otdr_free(&ctx);
    PASS();
}

static void test_region_snr(void) {
    TEST("Per-region SNR profile");
    uft_otdr_context_t ctx;
    uft_otdr_init(&ctx, "atari_st");

    uint32_t flux[40000];
    uint32_t n = gen_mfm(flux, 40000, 5.0f);
    uft_otdr_feed_flux_ns(&ctx, flux, n, 0, 0, 0);
    uft_otdr_analyze(&ctx);

    float snr[16];
    ASSERT_EQ(uft_otdr_region_snr(&ctx, 0, 0, 16, snr), 0, "region_snr");
    /* All regions should have measurable SNR */
    for (int i = 0; i < 16; i++) {
        ASSERT_TRUE(snr[i] > -50.0f && snr[i] <= 0.0f, "snr range");
    }

    uft_otdr_free(&ctx);
    PASS();
}

static void test_export_wrappers(void) {
    TEST("Export wrappers (report/heatmap/csv)");
    uft_otdr_context_t ctx;
    uft_otdr_init(&ctx, "atari_st");
    uft_otdr_set_geometry(&ctx, 2, 2);

    uint32_t flux[40000];
    for (int i = 0; i < 4; i++) {
        uint32_t n = gen_mfm(flux, 40000, 5.0f + i * 3.0f);
        uft_otdr_feed_flux_ns(&ctx, flux, n, (uint8_t)(i/2), (uint8_t)(i%2), 0);
    }
    uft_otdr_analyze(&ctx);

    ASSERT_EQ(uft_otdr_export_report(&ctx, "/tmp/bridge_report.txt"), 0, "report");
    ASSERT_EQ(uft_otdr_export_heatmap(&ctx, "/tmp/bridge_heat.pgm"), 0, "heatmap");
    ASSERT_EQ(uft_otdr_export_track_csv(&ctx, 0, 0, "/tmp/bridge_t0.csv"), 0, "csv");

    uft_otdr_free(&ctx);
    PASS();
}

static void test_null_safety(void) {
    TEST("NULL safety");
    ASSERT_EQ(uft_otdr_init(NULL, NULL), -1, "init(NULL)");
    uft_otdr_free(NULL); /* should not crash */
    uft_otdr_report_free(NULL);
    ASSERT_EQ(uft_otdr_analyze(NULL), -1, "analyze(NULL)");
    ASSERT_EQ(uft_otdr_feed_flux_ns(NULL, NULL, 0, 0, 0, 0), -1, "feed(NULL)");
    ASSERT_TRUE(uft_otdr_get_disk(NULL) == NULL, "disk(NULL)");
    ASSERT_TRUE(uft_otdr_get_track(NULL, 0, 0) == NULL, "track(NULL)");
    PASS();
}

int main(void) {
    printf("\n========== UFT OTDR Bridge — Test Suite ==========\n\n");
    test_init_default();
    test_init_platform();
    test_feed_flux_ns();
    test_feed_kryoflux();
    test_feed_greaseweazle();
    test_feed_analog();
    test_full_pipeline();
    test_snr_weights();
    test_region_snr();
    test_export_wrappers();
    test_null_safety();

    printf("\n  Results: %d/%d passed", tests_passed, tests_run);
    if (tests_passed == tests_run) printf("  ✓ ALL PASSED\n\n");
    else printf("  ✗ %d FAILED\n\n", tests_run - tests_passed);
    return (tests_passed == tests_run) ? 0 : 1;
}
