/**
 * @file test_nibbel_api.c
 * @brief Unit tests for NIBBEL API
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Note: In real build, include via proper path
// #include "uft/nibbel/uft_nibbel.h"

// Forward declarations for testing without full header
typedef struct uft_nibbel_ctx_s uft_nibbel_ctx_t;
typedef struct {
    int start_track;
    int end_track;
    int retries;
    int recovery_level;
    int max_correction_bits;
    int skip_errors;
    int verify_checksums;
    int verify_output;
    int attempt_correction;
    double bitcell_ns;
    double pll_bandwidth;
} uft_nibbel_config_t;

extern uft_nibbel_ctx_t* uft_nibbel_create(void);
extern void uft_nibbel_destroy(uft_nibbel_ctx_t* ctx);
extern void uft_nibbel_config_defaults(uft_nibbel_config_t* cfg);
extern const char* uft_nibbel_config_validate(const uft_nibbel_config_t* cfg);
extern const char* uft_nibbel_version(void);
extern uint32_t uft_nibbel_gcr_table_checksum(void);

// Test macros
#define TEST(name) static int test_##name(void)
#define RUN(name) do { \
    printf("  TEST: %-40s ", #name); \
    if (test_##name() == 0) { printf("[PASS]\n"); passed++; } \
    else { printf("[FAIL]\n"); failed++; } \
    total++; \
} while(0)

static int passed = 0, failed = 0, total = 0;

// ===========================================================================
// CONTEXT TESTS
// ===========================================================================

TEST(create_destroy) {
    uft_nibbel_ctx_t* ctx = uft_nibbel_create();
    if (!ctx) return -1;
    uft_nibbel_destroy(ctx);
    return 0;
}

TEST(create_null_safe) {
    uft_nibbel_destroy(NULL);  // Should not crash
    return 0;
}

TEST(version_not_null) {
    const char* ver = uft_nibbel_version();
    return (ver && strlen(ver) > 0) ? 0 : -1;
}

// ===========================================================================
// CONFIG TESTS
// ===========================================================================

TEST(config_defaults) {
    uft_nibbel_config_t cfg;
    memset(&cfg, 0xFF, sizeof(cfg));  // Fill with garbage
    
    uft_nibbel_config_defaults(&cfg);
    
    if (cfg.start_track != 0) return -1;
    if (cfg.end_track != 0) return -1;
    if (cfg.retries != 3) return -1;
    if (cfg.recovery_level != 1) return -1;
    
    return 0;
}

TEST(config_validate_valid) {
    uft_nibbel_config_t cfg;
    uft_nibbel_config_defaults(&cfg);
    
    const char* err = uft_nibbel_config_validate(&cfg);
    return (err == NULL) ? 0 : -1;
}

TEST(config_validate_bad_track) {
    uft_nibbel_config_t cfg;
    uft_nibbel_config_defaults(&cfg);
    cfg.start_track = 100;  // Invalid
    
    const char* err = uft_nibbel_config_validate(&cfg);
    return (err != NULL) ? 0 : -1;  // Should fail
}

TEST(config_validate_bad_retries) {
    uft_nibbel_config_t cfg;
    uft_nibbel_config_defaults(&cfg);
    cfg.retries = 20;  // Invalid
    
    const char* err = uft_nibbel_config_validate(&cfg);
    return (err != NULL) ? 0 : -1;  // Should fail
}

TEST(config_validate_track_order) {
    uft_nibbel_config_t cfg;
    uft_nibbel_config_defaults(&cfg);
    cfg.start_track = 20;
    cfg.end_track = 10;  // Invalid: end < start
    
    const char* err = uft_nibbel_config_validate(&cfg);
    return (err != NULL) ? 0 : -1;  // Should fail
}

TEST(config_validate_recovery_conflict) {
    uft_nibbel_config_t cfg;
    uft_nibbel_config_defaults(&cfg);
    cfg.recovery_level = 2;
    cfg.attempt_correction = 0;  // Conflict
    
    const char* err = uft_nibbel_config_validate(&cfg);
    return (err != NULL) ? 0 : -1;  // Should fail
}

// ===========================================================================
// GCR TABLE TESTS
// ===========================================================================

TEST(gcr_table_checksum) {
    uint32_t checksum = uft_nibbel_gcr_table_checksum();
    // Just verify it returns something non-zero
    return (checksum != 0) ? 0 : -1;
}

TEST(gcr_table_checksum_consistent) {
    uint32_t checksum1 = uft_nibbel_gcr_table_checksum();
    uint32_t checksum2 = uft_nibbel_gcr_table_checksum();
    return (checksum1 == checksum2) ? 0 : -1;
}

// ===========================================================================
// MAIN
// ===========================================================================

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         NIBBEL API UNIT TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("Context Management:\n");
    RUN(create_destroy);
    RUN(create_null_safe);
    RUN(version_not_null);
    
    printf("\nConfiguration:\n");
    RUN(config_defaults);
    RUN(config_validate_valid);
    RUN(config_validate_bad_track);
    RUN(config_validate_bad_retries);
    RUN(config_validate_track_order);
    RUN(config_validate_recovery_conflict);
    
    printf("\nGCR Tables:\n");
    RUN(gcr_table_checksum);
    RUN(gcr_table_checksum_consistent);
    
    printf("\n═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         RESULTS: %d/%d passed, %d failed\n", passed, total, failed);
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    return (failed == 0) ? 0 : 1;
}
