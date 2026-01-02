/**
 * @file test_forensic_negative.c
 * @brief Negative Tests - Error Handling & Edge Cases
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
// NULL POINTER TESTS
// ============================================================================

TEST(protection_null_context) {
    uft_protection_result_t result;
    int ret = uft_protection_detect(NULL, &result);
    ASSERT(ret == -1, "Should reject NULL context");
    PASS();
}

TEST(protection_null_result) {
    uft_protection_context_t ctx;
    uft_protection_context_init(&ctx);
    uint8_t data[100] = {0};
    ctx.data = data;
    ctx.data_size = 100;
    
    int ret = uft_protection_detect(&ctx, NULL);
    ASSERT(ret == -1, "Should reject NULL result");
    PASS();
}

TEST(protection_null_data) {
    uft_protection_context_t ctx;
    uft_protection_context_init(&ctx);
    ctx.data = NULL;
    ctx.data_size = 100;
    
    uft_protection_result_t result;
    int ret = uft_protection_detect(&ctx, &result);
    ASSERT(ret == -1, "Should reject NULL data");
    PASS();
}

TEST(weak_bits_null_revolutions) {
    size_t sizes[] = {100, 100};
    uint8_t weak_map[100];
    size_t weak_count;
    
    int ret = uft_protection_analyze_weak_bits(NULL, sizes, 2, weak_map, &weak_count);
    ASSERT(ret == -1, "Should reject NULL revolutions");
    PASS();
}

TEST(weak_bits_single_revolution) {
    uint8_t rev1[100] = {0};
    const uint8_t* revs[] = {rev1};
    size_t sizes[] = {100};
    uint8_t weak_map[100];
    size_t weak_count;
    
    int ret = uft_protection_analyze_weak_bits(revs, sizes, 1, weak_map, &weak_count);
    ASSERT(ret == -1, "Should reject single revolution");
    PASS();
}

TEST(recovery_bam_null_data) {
    uft_bam_analysis_t analysis;
    int ret = uft_recovery_bam_analyze(NULL, 174848, 0x0100, &analysis);
    ASSERT(ret == -1, "Should reject NULL data");
    PASS();
}

TEST(recovery_bam_null_analysis) {
    uint8_t data[174848] = {0};
    int ret = uft_recovery_bam_analyze(data, sizeof(data), 0x0100, NULL);
    ASSERT(ret == -1, "Should reject NULL analysis");
    PASS();
}

TEST(recovery_bam_unsupported_format) {
    uint8_t data[1000] = {0};
    uft_bam_analysis_t analysis;
    int ret = uft_recovery_bam_analyze(data, 1000, 0x9999, &analysis);
    ASSERT(ret == -1, "Should reject unsupported format");
    PASS();
}

TEST(xcopy_profile_null) {
    int ret = uft_xcopy_profile_set_range(NULL, 0, 40, 0, 1);
    ASSERT(ret == -1, "Should reject NULL profile");
    PASS();
}

TEST(xcopy_profile_invalid_range) {
    uft_copy_profile_t profile;
    uft_xcopy_profile_init(&profile);
    
    // Start > End
    int ret = uft_xcopy_profile_set_range(&profile, 40, 10, 0, 1);
    ASSERT(ret == -1, "Should reject invalid track range");
    
    // Track > 84
    ret = uft_xcopy_profile_set_range(&profile, 0, 100, 0, 1);
    ASSERT(ret == -1, "Should reject track > 84");
    
    // Invalid side
    ret = uft_xcopy_profile_set_range(&profile, 0, 40, 0, 5);
    ASSERT(ret == -1, "Should reject invalid side");
    
    PASS();
}

TEST(xcopy_parse_null) {
    uft_copy_profile_t profile;
    int ret = uft_xcopy_profile_parse(NULL, &profile);
    ASSERT(ret == -1, "Should reject NULL string");
    
    ret = uft_xcopy_profile_parse("tracks:1-40", NULL);
    ASSERT(ret == -1, "Should reject NULL profile");
    PASS();
}

TEST(xcopy_export_null) {
    char buffer[256];
    int ret = uft_xcopy_profile_export(NULL, buffer, 256);
    ASSERT(ret == -1, "Should reject NULL profile");
    
    uft_copy_profile_t profile;
    uft_xcopy_profile_init(&profile);
    
    ret = uft_xcopy_profile_export(&profile, NULL, 256);
    ASSERT(ret == -1, "Should reject NULL buffer");
    
    ret = uft_xcopy_profile_export(&profile, buffer, 0);
    ASSERT(ret == -1, "Should reject zero size");
    PASS();
}

TEST(xcopy_session_null) {
    uft_copy_session_t* session = uft_xcopy_session_create(NULL);
    ASSERT(session != NULL, "Should create with NULL profile (uses defaults)");
    
    int ret = uft_xcopy_session_start(NULL, "src", "dst");
    ASSERT(ret == -1, "Should reject NULL session");
    
    ret = uft_xcopy_session_start(session, NULL, "dst");
    ASSERT(ret == -1, "Should reject NULL source");
    
    ret = uft_xcopy_session_start(session, "src", NULL);
    ASSERT(ret == -1, "Should reject NULL destination");
    
    uft_xcopy_session_destroy(session);
    PASS();
}

TEST(params_get_unknown) {
    const uft_param_def_t* def = uft_param_get_def("unknown.param");
    ASSERT(def == NULL, "Should return NULL for unknown param");
    PASS();
}

TEST(params_set_null) {
    int ret = uft_param_set_int(NULL, "xcopy.retries", 5);
    ASSERT(ret == -1, "Should reject NULL set");
    
    uft_param_set_t* set = uft_param_set_create();
    ret = uft_param_set_int(set, NULL, 5);
    ASSERT(ret == -1, "Should reject NULL id");
    
    uft_param_set_destroy(set);
    PASS();
}

// ============================================================================
// BOUNDARY TESTS
// ============================================================================

TEST(protection_empty_data) {
    uft_protection_context_t ctx;
    uft_protection_context_init(&ctx);
    uint8_t data[1] = {0};
    ctx.data = data;
    ctx.data_size = 0;
    
    uft_protection_result_t result;
    int ret = uft_protection_detect(&ctx, &result);
    // Should handle gracefully
    ASSERT(ret == 0 || ret == -1, "Should handle empty data");
    PASS();
}

TEST(recovery_undersized_d64) {
    // D64 smaller than expected
    uint8_t small_d64[1000] = {0};
    uft_bam_analysis_t analysis;
    
    int ret = uft_recovery_bam_analyze(small_d64, 1000, 0x0100, &analysis);
    ASSERT(ret == -1, "Should reject undersized D64");
    PASS();
}

TEST(xcopy_profile_max_values) {
    uft_copy_profile_t profile;
    uft_xcopy_profile_init(&profile);
    
    // Set maximum valid values
    int ret = uft_xcopy_profile_set_range(&profile, 0, 84, 0, 1);
    ASSERT(ret == 0, "Should accept max track 84");
    ASSERT(profile.end_track == 84, "End track should be 84");
    PASS();
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         FORENSIC API NEGATIVE TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("NULL Pointer Tests:\n");
    RUN(protection_null_context);
    RUN(protection_null_result);
    RUN(protection_null_data);
    RUN(weak_bits_null_revolutions);
    RUN(weak_bits_single_revolution);
    RUN(recovery_bam_null_data);
    RUN(recovery_bam_null_analysis);
    RUN(recovery_bam_unsupported_format);
    RUN(xcopy_profile_null);
    RUN(xcopy_profile_invalid_range);
    RUN(xcopy_parse_null);
    RUN(xcopy_export_null);
    RUN(xcopy_session_null);
    RUN(params_get_unknown);
    RUN(params_set_null);
    
    printf("\nBoundary Tests:\n");
    RUN(protection_empty_data);
    RUN(recovery_undersized_d64);
    RUN(xcopy_profile_max_values);
    
    printf("\n═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         RESULTS: %d/%d passed, %d failed\n", tests_passed, tests_run, tests_failed);
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    return tests_failed > 0 ? 1 : 0;
}
