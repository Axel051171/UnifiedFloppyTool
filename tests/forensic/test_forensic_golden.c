/**
 * @file test_forensic_golden.c
 * @brief Golden Tests for Forensic APIs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/forensic/uft_protection.h"
#include "uft/forensic/uft_recovery.h"
#include "uft/forensic/uft_xcopy.h"
#include "uft/forensic/uft_forensic_params.h"

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
// PROTECTION GOLDEN TESTS
// ============================================================================

TEST(protection_context_init) {
    uft_protection_context_t ctx;
    uft_protection_context_init(&ctx);
    
    ASSERT(ctx.detect_weak_bits == true, "weak_bits should default true");
    ASSERT(ctx.detect_sync == true, "sync should default true");
    ASSERT(ctx.detect_halftrack == false, "halftrack should default false");
    ASSERT(ctx.max_revolutions == 5, "max_revs should default 5");
    PASS();
}

TEST(protection_scheme_name) {
    ASSERT(strcmp(uft_protection_scheme_name(UFT_PROT_CBM_RAPIDLOK), "RapidLok") == 0,
           "RapidLok name");
    ASSERT(strcmp(uft_protection_scheme_name(UFT_PROT_CBM_VMAX), "V-MAX") == 0,
           "V-MAX name");
    ASSERT(strcmp(uft_protection_scheme_name(UFT_PROT_AMI_COPYLOCK), "CopyLock") == 0,
           "CopyLock name");
    PASS();
}

TEST(protection_technique_name) {
    ASSERT(strcmp(uft_protection_technique_name(UFT_PROT_TECH_WEAK_BITS), "Weak Bits") == 0,
           "Weak bits name");
    ASSERT(strcmp(uft_protection_technique_name(UFT_PROT_TECH_HALF_TRACK), "Half Track") == 0,
           "Half track name");
    PASS();
}

TEST(protection_weak_bit_analysis) {
    // Create test data with weak bits
    uint8_t rev1[100], rev2[100], rev3[100];
    memset(rev1, 0x55, 100);
    memset(rev2, 0x55, 100);
    memset(rev3, 0x55, 100);
    
    // Introduce weak bits at position 50
    rev2[50] = 0x54;  // Bit 0 differs
    rev3[50] = 0x57;  // Bits 0,1 differ
    
    const uint8_t* revs[] = {rev1, rev2, rev3};
    size_t sizes[] = {100, 100, 100};
    
    uint8_t weak_map[100];
    size_t weak_count = 0;
    
    int result = uft_protection_analyze_weak_bits(revs, sizes, 3, weak_map, &weak_count);
    
    ASSERT(result == 0, "Analysis should succeed");
    ASSERT(weak_count > 0, "Should detect weak bits");
    ASSERT(weak_map[50] != 0, "Position 50 should be weak");
    PASS();
}

TEST(protection_detect_no_protection) {
    uint8_t clean_data[1000];
    memset(clean_data, 0, 1000);
    
    uft_protection_context_t ctx;
    uft_protection_context_init(&ctx);
    ctx.data = clean_data;
    ctx.data_size = 1000;
    ctx.hint_platform = UFT_PLATFORM_PC;
    
    uft_protection_result_t result;
    int ret = uft_protection_detect(&ctx, &result);
    
    ASSERT(ret == 0, "Detection should succeed");
    ASSERT(result.scheme == UFT_PROT_SCHEME_NONE, "Should detect no protection");
    
    uft_protection_result_free(&result);
    PASS();
}

// ============================================================================
// RECOVERY GOLDEN TESTS
// ============================================================================

TEST(recovery_config_default) {
    uft_recovery_config_t config;
    uft_recovery_config_default(&config);
    
    ASSERT(config.max_retries == 5, "Default retries should be 5");
    ASSERT(config.min_confidence == 0.90, "Default confidence should be 0.90");
    ASSERT(config.enable_crc_correction == true, "CRC correction should be enabled");
    PASS();
}

TEST(recovery_bam_analyze_clean) {
    // Create minimal D64 with valid BAM
    uint8_t d64[174848];
    memset(d64, 0, sizeof(d64));
    
    // Set up BAM at track 18, sector 0
    size_t bam_offset = 17 * 21 * 256;  // Track 18 (0-indexed 17)
    
    // Initialize BAM entries
    for (int track = 1; track <= 35; track++) {
        int sectors = track <= 17 ? 21 : (track <= 24 ? 19 : (track <= 30 ? 18 : 17));
        int entry = bam_offset + 4 * track;
        
        d64[entry] = sectors;  // Free count
        d64[entry + 1] = 0xFF;
        d64[entry + 2] = 0xFF;
        d64[entry + 3] = 0xFF;
    }
    
    uft_bam_analysis_t analysis;
    int result = uft_recovery_bam_analyze(d64, sizeof(d64), 0x0100, &analysis);
    
    ASSERT(result == 0, "Analysis should succeed");
    ASSERT(analysis.track == 18, "BAM track should be 18");
    ASSERT(analysis.total_blocks > 0, "Should have blocks");
    
    uft_recovery_bam_analysis_free(&analysis);
    PASS();
}

// ============================================================================
// XCOPY GOLDEN TESTS
// ============================================================================

TEST(xcopy_profile_init) {
    uft_copy_profile_t profile;
    uft_xcopy_profile_init(&profile);
    
    ASSERT(profile.mode == UFT_COPY_MODE_NORMAL, "Default mode should be normal");
    ASSERT(profile.start_track == 0, "Start track should be 0");
    ASSERT(profile.end_track == 79, "End track should be 79");
    ASSERT(profile.default_retries == 3, "Default retries should be 3");
    PASS();
}

TEST(xcopy_profile_for_mode) {
    uft_copy_profile_t profile;
    
    uft_xcopy_profile_for_mode(&profile, UFT_COPY_MODE_FORENSIC);
    
    ASSERT(profile.mode == UFT_COPY_MODE_FORENSIC, "Mode should be forensic");
    ASSERT(profile.verify == UFT_VERIFY_HASH, "Verify should be hash");
    ASSERT(profile.default_retries == 10, "Retries should be 10");
    ASSERT(profile.revolutions == 7, "Revolutions should be 7");
    ASSERT(profile.copy_halftracks == true, "Halftracks should be true");
    PASS();
}

TEST(xcopy_profile_parse) {
    uft_copy_profile_t profile;
    
    int result = uft_xcopy_profile_parse("tracks:1-40,sides:0-0,mode:raw,retries:5", &profile);
    
    ASSERT(result == 0, "Parse should succeed");
    ASSERT(profile.start_track == 1, "Start track should be 1");
    ASSERT(profile.end_track == 40, "End track should be 40");
    ASSERT(profile.mode == UFT_COPY_MODE_RAW, "Mode should be raw");
    ASSERT(profile.default_retries == 5, "Retries should be 5");
    
    uft_xcopy_profile_free(&profile);
    PASS();
}

TEST(xcopy_profile_export) {
    uft_copy_profile_t profile;
    uft_xcopy_profile_init(&profile);
    profile.start_track = 1;
    profile.end_track = 35;
    profile.mode = UFT_COPY_MODE_RAW;
    
    char buffer[256];
    int len = uft_xcopy_profile_export(&profile, buffer, sizeof(buffer));
    
    ASSERT(len > 0, "Export should return length");
    ASSERT(strstr(buffer, "tracks:1-35") != NULL, "Should contain track range");
    ASSERT(strstr(buffer, "mode:raw") != NULL, "Should contain mode");
    PASS();
}

TEST(xcopy_mode_names) {
    ASSERT(strcmp(uft_xcopy_mode_name(UFT_COPY_MODE_NORMAL), "normal") == 0, "Normal");
    ASSERT(strcmp(uft_xcopy_mode_name(UFT_COPY_MODE_RAW), "raw") == 0, "Raw");
    ASSERT(strcmp(uft_xcopy_mode_name(UFT_COPY_MODE_FLUX), "flux") == 0, "Flux");
    ASSERT(strcmp(uft_xcopy_mode_name(UFT_COPY_MODE_FORENSIC), "forensic") == 0, "Forensic");
    PASS();
}

// ============================================================================
// PARAMS GOLDEN TESTS
// ============================================================================

TEST(params_get_definition) {
    const uft_param_def_t* def = uft_param_get_def("xcopy.retries");
    
    ASSERT(def != NULL, "Should find xcopy.retries");
    ASSERT(def->type == UFT_PARAM_INT, "Type should be INT");
    ASSERT(def->default_value.int_val == 3, "Default should be 3");
    ASSERT(def->constraint.int_range.min == 0, "Min should be 0");
    ASSERT(def->constraint.int_range.max == 20, "Max should be 20");
    PASS();
}

TEST(params_set_operations) {
    uft_param_set_t* set = uft_param_set_create();
    ASSERT(set != NULL, "Should create set");
    
    int result = uft_param_set_int(set, "xcopy.retries", 5);
    ASSERT(result == 0, "Should set int");
    
    result = uft_param_set_bool(set, "recovery.crc_correction", true);
    ASSERT(result == 0, "Should set bool");
    
    int value;
    bool found = uft_param_get_int(set, "xcopy.retries", &value);
    ASSERT(found, "Should find value");
    ASSERT(value == 5, "Value should be 5");
    
    uft_param_set_destroy(set);
    PASS();
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         FORENSIC API GOLDEN TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("Protection API:\n");
    RUN(protection_context_init);
    RUN(protection_scheme_name);
    RUN(protection_technique_name);
    RUN(protection_weak_bit_analysis);
    RUN(protection_detect_no_protection);
    
    printf("\nRecovery API:\n");
    RUN(recovery_config_default);
    RUN(recovery_bam_analyze_clean);
    
    printf("\nXCopy API:\n");
    RUN(xcopy_profile_init);
    RUN(xcopy_profile_for_mode);
    RUN(xcopy_profile_parse);
    RUN(xcopy_profile_export);
    RUN(xcopy_mode_names);
    
    printf("\nParameter API:\n");
    RUN(params_get_definition);
    RUN(params_set_operations);
    
    printf("\n═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         RESULTS: %d/%d passed, %d failed\n", tests_passed, tests_run, tests_failed);
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    return tests_failed > 0 ? 1 : 0;
}
