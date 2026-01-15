/**
 * @file test_pll_unified.c
 * @brief Tests for Unified PLL Controller (W-P1-005)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "uft/uft_pll_unified.h"

/*============================================================================
 * TEST FRAMEWORK
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;
static int current_test_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s ... ", #name); \
    tests_run++; \
    current_test_failed = 0; \
    test_##name(); \
    if (!current_test_failed) { \
        tests_passed++; \
        printf("PASS\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        current_test_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)
#define ASSERT_NULL(x) ASSERT((x) == NULL)

/*============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/* Generate ideal MFM flux pattern for 0x4E (sync byte) */
#ifdef __GNUC__
__attribute__((unused))
#endif
static void generate_sync_flux(int *flux, int *count, int bitcell_ns) {
    /* 0x4E = 01001110 in MFM with clock bits becomes specific pattern */
    /* Simplified: alternating 1.5x and 2x bitcell times */
    int patterns[] = {4000, 4000, 6000, 4000, 4000, 6000, 6000, 4000};
    int n = sizeof(patterns) / sizeof(patterns[0]);
    
    for (int i = 0; i < n; i++) {
        flux[i] = (patterns[i] * bitcell_ns) / 4000;
    }
    *count = n;
}

/* Generate flux with jitter */
static void generate_flux_with_jitter(int *flux, int count, int bitcell_ns, int jitter_ns) {
    for (int i = 0; i < count; i++) {
        int jitter = (rand() % (2 * jitter_ns + 1)) - jitter_ns;
        flux[i] = bitcell_ns + jitter;
    }
}

/*============================================================================
 * TESTS: LIFECYCLE
 *============================================================================*/

TEST(pll_create_default) {
    uft_pll_context_t *ctx = uft_pll_create(NULL);
    ASSERT_NOT_NULL(ctx);
    uft_pll_destroy(ctx);
}

TEST(pll_create_with_config) {
    uft_pll_config_t config = UFT_PLL_CONFIG_DEFAULT;
    config.algorithm = UFT_PLL_ALGO_PI;
    
    uft_pll_context_t *ctx = uft_pll_create(&config);
    ASSERT_NOT_NULL(ctx);
    
    const uft_pll_config_t *cfg = uft_pll_get_config(ctx);
    ASSERT_EQ(cfg->algorithm, UFT_PLL_ALGO_PI);
    
    uft_pll_destroy(ctx);
}

TEST(pll_create_preset) {
    for (int p = 0; p < UFT_PLL_PRESET_COUNT; p++) {
        uft_pll_context_t *ctx = uft_pll_create_preset((uft_pll_preset_t)p);
        ASSERT_NOT_NULL(ctx);
        uft_pll_destroy(ctx);
    }
}

TEST(pll_destroy_null) {
    uft_pll_destroy(NULL);  /* Should not crash */
}

TEST(pll_reset) {
    uft_pll_context_t *ctx = uft_pll_create(NULL);
    ASSERT_NOT_NULL(ctx);
    
    /* Process some data */
    int bit;
    uft_pll_process(ctx, 4000, &bit);
    uft_pll_process(ctx, 4000, &bit);
    
    /* Reset */
    uft_pll_context_reset(ctx);
    
    const uft_pll_stats_t *stats = uft_pll_get_stats(ctx);
    ASSERT_EQ(stats->bits_decoded, 0);
    
    uft_pll_destroy(ctx);
}

/*============================================================================
 * TESTS: CONFIGURATION
 *============================================================================*/

TEST(pll_set_algorithm) {
    uft_pll_context_t *ctx = uft_pll_create(NULL);
    ASSERT_NOT_NULL(ctx);
    
    ASSERT_EQ(uft_pll_set_algorithm(ctx, UFT_PLL_ALGO_PI), 0);
    ASSERT_EQ(uft_pll_get_config(ctx)->algorithm, UFT_PLL_ALGO_PI);
    
    ASSERT_EQ(uft_pll_set_algorithm(ctx, UFT_PLL_ALGO_ADAPTIVE), 0);
    ASSERT_EQ(uft_pll_get_config(ctx)->algorithm, UFT_PLL_ALGO_ADAPTIVE);
    
    uft_pll_destroy(ctx);
}

TEST(pll_set_bitcell) {
    uft_pll_context_t *ctx = uft_pll_create(NULL);
    ASSERT_NOT_NULL(ctx);
    
    ASSERT_EQ(uft_pll_set_bitcell(ctx, 2000), 0);
    ASSERT_EQ(uft_pll_get_config(ctx)->base.bitcell_ns, 2000);
    
    uft_pll_destroy(ctx);
}

TEST(pll_apply_preset) {
    uft_pll_context_t *ctx = uft_pll_create(NULL);
    ASSERT_NOT_NULL(ctx);
    
    ASSERT_EQ(uft_pll_apply_preset(ctx, UFT_PLL_PRESET_C64), 0);
    ASSERT_TRUE(uft_pll_get_config(ctx)->base.use_gcr);
    
    uft_pll_destroy(ctx);
}

/*============================================================================
 * TESTS: DECODING
 *============================================================================*/

TEST(pll_process_single) {
    uft_pll_context_t *ctx = uft_pll_create_preset(UFT_PLL_PRESET_IBM_DD);
    ASSERT_NOT_NULL(ctx);
    
    int bit;
    int rc = uft_pll_process(ctx, 4000, &bit);
    ASSERT_EQ(rc, 0);
    
    uft_pll_destroy(ctx);
}

TEST(pll_process_noise_filter) {
    uft_pll_context_t *ctx = uft_pll_create(NULL);
    ASSERT_NOT_NULL(ctx);
    
    int bit;
    /* Very short flux should be filtered */
    uft_pll_process(ctx, 50, &bit);
    ASSERT_EQ(bit, -1);
    
    uft_pll_destroy(ctx);
}

TEST(pll_decode_flux_array) {
    uft_pll_context_t *ctx = uft_pll_create_preset(UFT_PLL_PRESET_IBM_DD);
    ASSERT_NOT_NULL(ctx);
    
    /* Generate test flux */
    int flux[100];
    generate_flux_with_jitter(flux, 100, 4000, 200);
    
    uint8_t bits[20];
    int decoded = uft_pll_decode_flux(ctx, flux, 100, bits, sizeof(bits));
    
    ASSERT_TRUE(decoded > 0);
    
    uft_pll_destroy(ctx);
}

TEST(pll_sync_establishment) {
    uft_pll_context_t *ctx = uft_pll_create_preset(UFT_PLL_PRESET_IBM_DD);
    ASSERT_NOT_NULL(ctx);
    
    ASSERT_FALSE(uft_pll_is_synced(ctx));
    
    /* Feed many consistent transitions */
    int bit;
    for (int i = 0; i < 300; i++) {
        uft_pll_process(ctx, 4000, &bit);
    }
    
    ASSERT_TRUE(uft_pll_is_synced(ctx));
    
    uft_pll_destroy(ctx);
}

/*============================================================================
 * TESTS: ALGORITHMS
 *============================================================================*/

TEST(pll_algo_dpll) {
    uft_pll_context_t *ctx = uft_pll_create(NULL);
    uft_pll_set_algorithm(ctx, UFT_PLL_ALGO_DPLL);
    
    int flux[100];
    generate_flux_with_jitter(flux, 100, 4000, 100);
    
    uint8_t bits[20];
    int decoded = uft_pll_decode_flux(ctx, flux, 100, bits, sizeof(bits));
    ASSERT_TRUE(decoded > 50);
    
    uft_pll_destroy(ctx);
}

TEST(pll_algo_pi) {
    uft_pll_context_t *ctx = uft_pll_create(NULL);
    uft_pll_set_algorithm(ctx, UFT_PLL_ALGO_PI);
    
    int flux[100];
    generate_flux_with_jitter(flux, 100, 4000, 100);
    
    uint8_t bits[20];
    int decoded = uft_pll_decode_flux(ctx, flux, 100, bits, sizeof(bits));
    ASSERT_TRUE(decoded > 50);
    
    uft_pll_destroy(ctx);
}

TEST(pll_algo_adaptive) {
    uft_pll_context_t *ctx = uft_pll_create(NULL);
    uft_pll_set_algorithm(ctx, UFT_PLL_ALGO_ADAPTIVE);
    
    int flux[100];
    generate_flux_with_jitter(flux, 100, 4000, 100);
    
    uint8_t bits[20];
    int decoded = uft_pll_decode_flux(ctx, flux, 100, bits, sizeof(bits));
    ASSERT_TRUE(decoded > 50);
    
    uft_pll_destroy(ctx);
}

/*============================================================================
 * TESTS: STATISTICS
 *============================================================================*/

TEST(pll_stats_basic) {
    uft_pll_context_t *ctx = uft_pll_create(NULL);
    ASSERT_NOT_NULL(ctx);
    
    int bit;
    for (int i = 0; i < 100; i++) {
        uft_pll_process(ctx, 4000, &bit);
    }
    
    const uft_pll_stats_t *stats = uft_pll_get_stats(ctx);
    ASSERT_NOT_NULL(stats);
    ASSERT_TRUE(stats->bits_decoded > 0);
    
    uft_pll_destroy(ctx);
}

TEST(pll_stats_reset) {
    uft_pll_context_t *ctx = uft_pll_create(NULL);
    
    int bit;
    for (int i = 0; i < 50; i++) {
        uft_pll_process(ctx, 4000, &bit);
    }
    
    uft_pll_reset_stats(ctx);
    
    const uft_pll_stats_t *stats = uft_pll_get_stats(ctx);
    ASSERT_EQ(stats->bits_decoded, 0);
    
    uft_pll_destroy(ctx);
}

TEST(pll_quality_score) {
    uft_pll_context_t *ctx = uft_pll_create(NULL);
    
    /* Feed good data */
    int bit;
    for (int i = 0; i < 300; i++) {
        uft_pll_process(ctx, 4000, &bit);
    }
    
    int quality = uft_pll_get_quality(ctx);
    ASSERT_TRUE(quality >= 50);
    ASSERT_TRUE(quality <= 100);
    
    uft_pll_destroy(ctx);
}

/*============================================================================
 * TESTS: UTILITIES
 *============================================================================*/

TEST(pll_preset_config) {
    for (int p = 0; p < UFT_PLL_PRESET_COUNT; p++) {
        const uft_pll_config_t *cfg = uft_pll_get_preset_config((uft_pll_preset_t)p);
        ASSERT_NOT_NULL(cfg);
        ASSERT_TRUE(cfg->base.bitcell_ns > 0);
    }
}

TEST(pll_detect_preset) {
    /* HD flux (~2000ns) */
    int hd_flux[100];
    for (int i = 0; i < 100; i++) hd_flux[i] = 2000;
    ASSERT_EQ(uft_pll_detect_preset(hd_flux, 100), UFT_PLL_PRESET_IBM_HD);
    
    /* DD flux (~4000ns) */
    int dd_flux[100];
    for (int i = 0; i < 100; i++) dd_flux[i] = 4000;
    ASSERT_EQ(uft_pll_detect_preset(dd_flux, 100), UFT_PLL_PRESET_IBM_DD);
}

TEST(pll_algo_names) {
    for (int a = 0; a < UFT_PLL_ALGO_COUNT; a++) {
        const char *name = uft_pll_algo_name((uft_pll_algo_t)a);
        ASSERT_NOT_NULL(name);
        ASSERT_TRUE(strlen(name) > 0);
    }
}

TEST(pll_preset_names) {
    for (int p = 0; p < UFT_PLL_PRESET_COUNT; p++) {
        const char *name = uft_pll_preset_name((uft_pll_preset_t)p);
        ASSERT_NOT_NULL(name);
        ASSERT_TRUE(strlen(name) > 0);
    }
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT Unified PLL Tests (W-P1-005)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    /* Lifecycle */
    printf("[SUITE] Lifecycle\n");
    RUN_TEST(pll_create_default);
    RUN_TEST(pll_create_with_config);
    RUN_TEST(pll_create_preset);
    RUN_TEST(pll_destroy_null);
    RUN_TEST(pll_reset);
    
    /* Configuration */
    printf("\n[SUITE] Configuration\n");
    RUN_TEST(pll_set_algorithm);
    RUN_TEST(pll_set_bitcell);
    RUN_TEST(pll_apply_preset);
    
    /* Decoding */
    printf("\n[SUITE] Decoding\n");
    RUN_TEST(pll_process_single);
    RUN_TEST(pll_process_noise_filter);
    RUN_TEST(pll_decode_flux_array);
    RUN_TEST(pll_sync_establishment);
    
    /* Algorithms */
    printf("\n[SUITE] Algorithms\n");
    RUN_TEST(pll_algo_dpll);
    RUN_TEST(pll_algo_pi);
    RUN_TEST(pll_algo_adaptive);
    
    /* Statistics */
    printf("\n[SUITE] Statistics\n");
    RUN_TEST(pll_stats_basic);
    RUN_TEST(pll_stats_reset);
    RUN_TEST(pll_quality_score);
    
    /* Utilities */
    printf("\n[SUITE] Utilities\n");
    RUN_TEST(pll_preset_config);
    RUN_TEST(pll_detect_preset);
    RUN_TEST(pll_algo_names);
    RUN_TEST(pll_preset_names);
    
    /* Summary */
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n", 
           tests_passed, tests_run - tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
