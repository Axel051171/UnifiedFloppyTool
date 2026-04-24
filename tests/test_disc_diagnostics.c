/**
 * @file test_disc_diagnostics.c
 * @brief Tests for disc diagnostics (restored from v3.7.0 EXT3-017).
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "uft/diag/uft_disc_diagnostics.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* Mock read function — always succeeds with a sector of zeros. */
static int mock_good_read(int t, int s, int sec, uint8_t *buf,
                            size_t sz, void *ud) {
    (void)t; (void)s; (void)sec; (void)ud;
    memset(buf, 0, sz);
    return 0;
}

/* Mock write function — always succeeds. */
static int mock_write(int t, int s, int sec, const uint8_t *buf,
                        size_t sz, void *ud) {
    (void)t; (void)s; (void)sec; (void)buf; (void)sz; (void)ud;
    return 0;
}

/* Mock read that fails on track 10. */
static int mock_bad_track10_read(int t, int s, int sec, uint8_t *buf,
                                   size_t sz, void *ud) {
    (void)s; (void)sec; (void)ud;
    if (t == 10) return -1;
    memset(buf, 0, sz);
    return 0;
}

TEST(init_with_null_config_installs_defaults) {
    uft_diag_ctx_t ctx;
    ASSERT(uft_diag_init(&ctx, NULL) == 0);
    ASSERT(ctx.config.tracks == 80);
    ASSERT(ctx.config.sides == 2);
    ASSERT(ctx.config.sectors == 18);
    ASSERT(ctx.config.sector_size == 512);
    ASSERT(ctx.config.retries == 3);
    uft_diag_free(&ctx);
}

TEST(init_copies_provided_config) {
    uft_diag_config_t cfg = {
        .tracks = 40, .sides = 1, .sectors = 10,
        .sector_size = 256, .retries = 1, .verbose = false
    };
    uft_diag_ctx_t ctx;
    ASSERT(uft_diag_init(&ctx, &cfg) == 0);
    ASSERT(ctx.config.tracks == 40);
    ASSERT(ctx.config.sectors == 10);
    ASSERT(ctx.config.sector_size == 256);
    uft_diag_free(&ctx);
}

TEST(init_rejects_null_ctx) {
    ASSERT(uft_diag_init(NULL, NULL) == -1);
}

TEST(surface_scan_all_good) {
    uft_diag_config_t cfg = { .tracks = 5, .sides = 1, .sectors = 9,
                               .sector_size = 512, .retries = 1 };
    uft_diag_ctx_t ctx;
    ASSERT(uft_diag_init(&ctx, &cfg) == 0);
    ASSERT(uft_diag_surface_scan(&ctx, mock_good_read, NULL) == 0);
    ASSERT(ctx.total_sectors == 5 * 1 * 9);
    ASSERT(ctx.good_sectors == ctx.total_sectors);
    ASSERT(ctx.bad_sectors == 0);
    ASSERT(ctx.completed == true);
    uft_diag_free(&ctx);
}

TEST(surface_scan_with_bad_track_reports_bad) {
    uft_diag_config_t cfg = { .tracks = 20, .sides = 1, .sectors = 9,
                               .sector_size = 512, .retries = 1 };
    uft_diag_ctx_t ctx;
    ASSERT(uft_diag_init(&ctx, &cfg) == 0);
    ASSERT(uft_diag_surface_scan(&ctx, mock_bad_track10_read, NULL) == 0);
    ASSERT(ctx.bad_sectors == 9);        /* only track 10 fails, 9 sectors */
    ASSERT(ctx.good_sectors == ctx.total_sectors - 9);
    uft_diag_free(&ctx);
}

TEST(surface_scan_rejects_null) {
    uft_diag_ctx_t ctx;
    uft_diag_init(&ctx, NULL);
    ASSERT(uft_diag_surface_scan(&ctx, NULL, NULL) == -1);
    ASSERT(uft_diag_surface_scan(NULL, mock_good_read, NULL) == -1);
    uft_diag_free(&ctx);
}

TEST(bad_sectors_list_all_good_scan_is_empty) {
    uft_diag_config_t cfg = { .tracks = 5, .sides = 1, .sectors = 9,
                               .sector_size = 512, .retries = 1 };
    uft_diag_ctx_t ctx;
    uft_diag_init(&ctx, &cfg);
    uft_diag_surface_scan(&ctx, mock_good_read, NULL);

    uft_bad_sector_t list[256];
    size_t count = 256;
    ASSERT(uft_diag_get_bad_sectors(&ctx, list, &count) == 0);
    ASSERT(count == 0);
    uft_diag_free(&ctx);
}

TEST(report_text_produces_output) {
    uft_diag_config_t cfg = { .tracks = 5, .sides = 1, .sectors = 9,
                               .sector_size = 512, .retries = 1 };
    uft_diag_ctx_t ctx;
    uft_diag_init(&ctx, &cfg);
    uft_diag_surface_scan(&ctx, mock_good_read, NULL);

    char buf[4096];
    ASSERT(uft_diag_report_text(&ctx, buf, sizeof(buf)) == 0);
    ASSERT(strstr(buf, "Diagnostics") != NULL ||
           strstr(buf, "EXCELLENT") != NULL ||
           strstr(buf, "Quality") != NULL);
    uft_diag_free(&ctx);
}

TEST(report_json_is_valid_shape) {
    uft_diag_config_t cfg = { .tracks = 5, .sides = 1, .sectors = 9,
                               .sector_size = 512, .retries = 1 };
    uft_diag_ctx_t ctx;
    uft_diag_init(&ctx, &cfg);
    uft_diag_surface_scan(&ctx, mock_good_read, NULL);

    char buf[4096];
    ASSERT(uft_diag_report_json(&ctx, buf, sizeof(buf)) == 0);
    ASSERT(strchr(buf, '{') != NULL);
    ASSERT(strchr(buf, '}') != NULL);
    uft_diag_free(&ctx);
}

TEST(write_verify_rejects_null_callbacks) {
    uft_diag_ctx_t ctx;
    uft_diag_init(&ctx, NULL);
    ASSERT(uft_diag_write_verify(&ctx, NULL, mock_write, NULL, 40, 0)
           == -1);
    ASSERT(uft_diag_write_verify(&ctx, mock_good_read, NULL, NULL, 40, 0)
           == -1);
    uft_diag_free(&ctx);
}

TEST(free_null_is_safe) {
    uft_diag_free(NULL);
}

int main(void) {
    printf("=== uft_disc_diagnostics tests ===\n");
    RUN(init_with_null_config_installs_defaults);
    RUN(init_copies_provided_config);
    RUN(init_rejects_null_ctx);
    RUN(surface_scan_all_good);
    RUN(surface_scan_with_bad_track_reports_bad);
    RUN(surface_scan_rejects_null);
    RUN(bad_sectors_list_all_good_scan_is_empty);
    RUN(report_text_produces_output);
    RUN(report_json_is_valid_shape);
    RUN(write_verify_rejects_null_callbacks);
    RUN(free_null_is_safe);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
