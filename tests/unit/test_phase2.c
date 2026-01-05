/**
 * @file test_phase2.c
 * @brief Unit Tests for Roadmap Phase 2
 * 
 * F2.1: Kalman PLL
 * F2.2: Viterbi GCR
 * F2.3: Multi-Rev Fusion
 * F2.4: Bayesian Detection
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "uft/decoder/uft_pll.h"
#include "uft/decoder/uft_gcr.h"
#include "uft/decoder/uft_fusion.h"
#include "uft/formats/uft_detect.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN(name) do { \
    printf("  TEST: %s... ", #name); \
    tests_run++; \
    test_##name(); \
} while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; return; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; return; } while(0)
#define ASSERT(c, m) if (!(c)) FAIL(m)

// ============================================================================
// F2.1 PLL Tests
// ============================================================================

TEST(pll_create) {
    uft_pll_config_t config;
    uft_pll_config_default(&config, UFT_ENC_MFM_DD);
    
    ASSERT(config.initial_frequency == 250000.0, "Wrong default freq");
    
    uft_pll_t* pll = uft_pll_create(&config);
    ASSERT(pll != NULL, "PLL create failed");
    
    uft_pll_destroy(pll);
    PASS();
}

TEST(pll_lock) {
    uft_pll_t* pll = uft_pll_create(NULL);
    ASSERT(pll != NULL, "PLL create failed");
    
    // Feed clean MFM signal (4000ns bit cells)
    uint8_t bits[16];
    for (int i = 0; i < 20; i++) {
        uft_pll_process(pll, 4000, bits, 16);
    }
    
    ASSERT(uft_pll_is_locked(pll), "PLL should be locked");
    
    uft_pll_destroy(pll);
    PASS();
}

TEST(pll_encoding_name) {
    ASSERT(strcmp(uft_pll_encoding_name(UFT_ENC_MFM_DD), "MFM DD") == 0, "MFM DD name");
    ASSERT(strcmp(uft_pll_encoding_name(UFT_ENC_GCR_C64), "GCR C64") == 0, "GCR C64 name");
    PASS();
}

// ============================================================================
// F2.2 GCR Tests
// ============================================================================

TEST(gcr_create) {
    uft_gcr_config_t config;
    uft_gcr_config_default(&config, GCR_MODE_C64);
    
    ASSERT(config.mode == GCR_MODE_C64, "Wrong mode");
    ASSERT(config.allow_bitslip == true, "Bitslip should be enabled");
    
    uft_gcr_decoder_t* dec = uft_gcr_create(&config);
    ASSERT(dec != NULL, "GCR create failed");
    
    uft_gcr_destroy(dec);
    PASS();
}

TEST(gcr_decode_nibble) {
    // C64 GCR: 0x0A = 0, 0x0B = 1, etc.
    ASSERT(uft_gcr_decode_nibble(0x0A, GCR_MODE_C64) == 0, "0x0A should decode to 0");
    ASSERT(uft_gcr_decode_nibble(0x0B, GCR_MODE_C64) == 1, "0x0B should decode to 1");
    ASSERT(uft_gcr_decode_nibble(0x12, GCR_MODE_C64) == 2, "0x12 should decode to 2");
    ASSERT(uft_gcr_decode_nibble(0x00, GCR_MODE_C64) == -1, "Invalid should return -1");
    PASS();
}

TEST(gcr_encode_nibble) {
    ASSERT(uft_gcr_encode_nibble(0, GCR_MODE_C64) == 0x0A, "0 should encode to 0x0A");
    ASSERT(uft_gcr_encode_nibble(15, GCR_MODE_C64) == 0x15, "15 should encode to 0x15");
    PASS();
}

TEST(gcr_sectors_per_track) {
    ASSERT(uft_gcr_sectors_in_track(1, GCR_MODE_C64) == 21, "Track 1 = 21");
    ASSERT(uft_gcr_sectors_in_track(18, GCR_MODE_C64) == 19, "Track 18 = 19");
    ASSERT(uft_gcr_sectors_in_track(31, GCR_MODE_C64) == 17, "Track 31 = 17");
    PASS();
}

// ============================================================================
// F2.3 Fusion Tests
// ============================================================================

TEST(fusion_create) {
    uft_fusion_config_t config;
    uft_fusion_config_default(&config);
    
    ASSERT(config.min_revolutions == 2, "Min revs should be 2");
    ASSERT(config.consensus_threshold >= 0.5, "Consensus >= 0.5");
    
    uft_fusion_t* fusion = uft_fusion_create(&config);
    ASSERT(fusion != NULL, "Fusion create failed");
    
    uft_fusion_destroy(fusion);
    PASS();
}

TEST(fusion_add_revolution) {
    uft_fusion_t* fusion = uft_fusion_create(NULL);
    ASSERT(fusion != NULL, "Fusion create failed");
    
    uint8_t data1[100] = {0xFF};
    uint8_t data2[100] = {0xFF};
    
    int idx1 = uft_fusion_add_revolution(fusion, data1, 100, 0.9);
    int idx2 = uft_fusion_add_revolution(fusion, data2, 100, 0.8);
    
    ASSERT(idx1 == 0, "First index should be 0");
    ASSERT(idx2 == 1, "Second index should be 1");
    
    uft_fusion_destroy(fusion);
    PASS();
}

TEST(fusion_quality) {
    uint8_t good[100];
    uint8_t bad[100];
    
    // Good: alternating bits (MFM-like)
    for (int i = 0; i < 100; i++) good[i] = 0x55;
    
    // Bad: all zeros
    memset(bad, 0, 100);
    
    double good_q = uft_fusion_analyze_quality(good, 100);
    double bad_q = uft_fusion_analyze_quality(bad, 100);
    
    ASSERT(good_q > bad_q, "Good quality should be higher");
    PASS();
}

// ============================================================================
// F2.4 Detection Tests
// ============================================================================

TEST(detect_d64) {
    uint8_t data[174848] = {0};
    uft_detect_result_t result;
    
    int ret = uft_detect_format(data, 174848, "test.d64", NULL, &result);
    ASSERT(ret == 0, "Detection should succeed");
    ASSERT(result.candidate_count > 0, "Should have candidates");
    
    const uft_detect_candidate_t* best = uft_detect_best(&result);
    ASSERT(best != NULL, "Should have best match");
    ASSERT(best->format_id == UFT_FMT_D64, "Should detect D64");
    
    uft_detect_result_free(&result);
    PASS();
}

TEST(detect_adf) {
    uint8_t data[901120] = {0};
    memcpy(data, "DOS\x01", 4);  // FFS signature
    
    uft_detect_result_t result;
    int ret = uft_detect_format(data, 901120, "test.adf", NULL, &result);
    ASSERT(ret == 0, "Detection should succeed");
    
    const uft_detect_candidate_t* best = uft_detect_best(&result);
    ASSERT(best != NULL, "Should have best match");
    ASSERT(best->format_id == UFT_FMT_ADF, "Should detect ADF");
    
    uft_detect_result_free(&result);
    PASS();
}

TEST(detect_format_name) {
    ASSERT(strcmp(uft_format_name(UFT_FMT_D64), "D64") == 0, "D64 name");
    ASSERT(strcmp(uft_format_name(UFT_FMT_ADF), "ADF") == 0, "ADF name");
    ASSERT(strcmp(uft_format_name(UFT_FMT_SCP), "SCP") == 0, "SCP name");
    PASS();
}

TEST(detect_is_flux) {
    ASSERT(uft_format_is_flux(UFT_FMT_SCP) == true, "SCP is flux");
    ASSERT(uft_format_is_flux(UFT_FMT_HFE) == true, "HFE is flux");
    ASSERT(uft_format_is_flux(UFT_FMT_D64) == false, "D64 is not flux");
    PASS();
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         PHASE 2 UNIT TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("F2.1: Kalman PLL\n");
    RUN(pll_create);
    RUN(pll_lock);
    RUN(pll_encoding_name);
    
    printf("\nF2.2: Viterbi GCR\n");
    RUN(gcr_create);
    RUN(gcr_decode_nibble);
    RUN(gcr_encode_nibble);
    RUN(gcr_sectors_per_track);
    
    printf("\nF2.3: Multi-Rev Fusion\n");
    RUN(fusion_create);
    RUN(fusion_add_revolution);
    RUN(fusion_quality);
    
    printf("\nF2.4: Bayesian Detection\n");
    RUN(detect_d64);
    RUN(detect_adf);
    RUN(detect_format_name);
    RUN(detect_is_flux);
    
    printf("\n═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         RESULTS: %d/%d passed, %d failed\n", tests_passed, tests_run, tests_failed);
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    return tests_failed > 0 ? 1 : 0;
}
