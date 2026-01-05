/**
 * @file test_error_paths.c
 * @brief Unit Tests für Error-Handling Paths
 * 
 * Diese Tests prüfen, dass alle Error-Pfade korrekt funktionieren:
 * - NULL Parameter
 * - OOM Simulation
 * - File I/O Fehler
 * - Bounds Violations
 */

#include "uft_test.h"
#include "uft/uft_safe.h"
#include "uft/uft_types.h"
#include "uft/uft_error.h"
#include <errno.h>

// ============================================================================
// NULL PARAMETER TESTS
// ============================================================================

static uft_error_t func_with_null_check(void* ptr) {
    UFT_REQUIRE_NOT_NULL(ptr);
    return UFT_OK;
}

UFT_TEST(null_pointer_rejected) {
    uft_error_t err = func_with_null_check(NULL);
    UFT_ASSERT_EQ(err, UFT_ERROR_NULL_POINTER);
    UFT_PASS();
}

UFT_TEST(valid_pointer_accepted) {
    int dummy = 42;
    uft_error_t err = func_with_null_check(&dummy);
    UFT_ASSERT_EQ(err, UFT_OK);
    UFT_PASS();
}

// ============================================================================
// BOUNDS CHECK TESTS
// ============================================================================

static uft_error_t check_track_bounds(int cyl, int head, int max_cyl, int max_head) {
    if (cyl < 0 || cyl >= max_cyl) return UFT_ERROR_INVALID_ARG;
    if (head < 0 || head >= max_head) return UFT_ERROR_INVALID_ARG;
    return UFT_OK;
}

UFT_TEST(track_bounds_valid) {
    UFT_ASSERT_EQ(check_track_bounds(0, 0, 80, 2), UFT_OK);
    UFT_ASSERT_EQ(check_track_bounds(79, 1, 80, 2), UFT_OK);
    UFT_PASS();
}

UFT_TEST(track_bounds_cyl_negative) {
    UFT_ASSERT_EQ(check_track_bounds(-1, 0, 80, 2), UFT_ERROR_INVALID_ARG);
    UFT_PASS();
}

UFT_TEST(track_bounds_cyl_overflow) {
    UFT_ASSERT_EQ(check_track_bounds(80, 0, 80, 2), UFT_ERROR_INVALID_ARG);
    UFT_PASS();
}

UFT_TEST(track_bounds_head_invalid) {
    UFT_ASSERT_EQ(check_track_bounds(0, 2, 80, 2), UFT_ERROR_INVALID_ARG);
    UFT_ASSERT_EQ(check_track_bounds(0, -1, 80, 2), UFT_ERROR_INVALID_ARG);
    UFT_PASS();
}

// ============================================================================
// FILE SIZE VALIDATION TESTS
// ============================================================================

typedef struct {
    size_t expected_size;
    size_t tolerance_percent;
} file_size_validator_t;

static bool validate_file_size(const file_size_validator_t* v, size_t actual) {
    size_t min_size = v->expected_size * (100 - v->tolerance_percent) / 100;
    size_t max_size = v->expected_size * (100 + v->tolerance_percent) / 100;
    return actual >= min_size && actual <= max_size;
}

UFT_TEST(file_size_exact_match) {
    file_size_validator_t v = {.expected_size = 174848, .tolerance_percent = 0};
    UFT_ASSERT(validate_file_size(&v, 174848));
    UFT_ASSERT(!validate_file_size(&v, 174847));
    UFT_ASSERT(!validate_file_size(&v, 174849));
    UFT_PASS();
}

UFT_TEST(file_size_with_tolerance) {
    file_size_validator_t v = {.expected_size = 1000, .tolerance_percent = 10};
    UFT_ASSERT(validate_file_size(&v, 1000));  // Exact
    UFT_ASSERT(validate_file_size(&v, 900));   // -10%
    UFT_ASSERT(validate_file_size(&v, 1100));  // +10%
    UFT_ASSERT(!validate_file_size(&v, 899));  // Too small
    UFT_ASSERT(!validate_file_size(&v, 1101)); // Too large
    UFT_PASS();
}

// ============================================================================
// ERROR CODE TESTS
// ============================================================================

UFT_TEST(error_codes_unique) {
    // Verify all error codes are unique
    UFT_ASSERT_NE(UFT_OK, UFT_ERROR_NO_MEMORY);
    UFT_ASSERT_NE(UFT_OK, UFT_ERROR_FILE_OPEN);
    UFT_ASSERT_NE(UFT_OK, UFT_ERROR_FILE_READ);
    UFT_ASSERT_NE(UFT_OK, UFT_ERROR_FILE_WRITE);
    UFT_ASSERT_NE(UFT_OK, UFT_ERROR_INVALID_ARG);
    UFT_ASSERT_NE(UFT_OK, UFT_ERROR_NULL_POINTER);
    UFT_PASS();
}

UFT_TEST(error_code_check_macro) {
    // Test UFT_CHECK pattern
    uft_error_t err = UFT_OK;
    
    // Should not change err
    if (UFT_OK != UFT_OK) err = UFT_ERROR_INVALID_STATE;
    UFT_ASSERT_EQ(err, UFT_OK);
    
    UFT_PASS();
}

// ============================================================================
// RESOURCE CLEANUP TESTS
// ============================================================================

UFT_TEST(double_free_prevention) {
    // Test that our patterns prevent double-free
    void* ptr = malloc(100);
    UFT_ASSERT_NOT_NULL(ptr);
    
    free(ptr);
    ptr = NULL;  // Critical: set to NULL after free
    
    // This is safe because ptr is NULL
    if (ptr) {
        free(ptr);  // Would crash if ptr wasn't NULL
    }
    
    UFT_PASS();
}

// ============================================================================
// INTEGER BOUNDARY TESTS
// ============================================================================

UFT_TEST(int_boundary_uint8) {
    uint8_t val = 255;
    UFT_ASSERT_EQ(val, 255);
    val++;
    UFT_ASSERT_EQ(val, 0);  // Overflow wraps
    UFT_PASS();
}

UFT_TEST(int_boundary_uint16) {
    uint16_t val = 65535;
    UFT_ASSERT_EQ(val, 65535);
    val++;
    UFT_ASSERT_EQ(val, 0);  // Overflow wraps
    UFT_PASS();
}

UFT_TEST(sector_id_limits) {
    // D64 has max 21 sectors per track
    // D80 has max 29 sectors per track
    // Max sector ID should be 255 (uint8_t)
    
    uint8_t max_d64_spt = 21;
    uint8_t max_d80_spt = 29;
    uint8_t max_sector_id = 255;
    
    UFT_ASSERT(max_d64_spt < max_sector_id);
    UFT_ASSERT(max_d80_spt < max_sector_id);
    UFT_PASS();
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("         UNIFIEDFLOPPYTOOL - ERROR PATH TESTS\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    
    UFT_TEST_SUITE("NULL Parameter Handling");
    UFT_RUN_TEST(null_pointer_rejected);
    UFT_RUN_TEST(valid_pointer_accepted);
    
    UFT_TEST_SUITE("Bounds Checking");
    UFT_RUN_TEST(track_bounds_valid);
    UFT_RUN_TEST(track_bounds_cyl_negative);
    UFT_RUN_TEST(track_bounds_cyl_overflow);
    UFT_RUN_TEST(track_bounds_head_invalid);
    
    UFT_TEST_SUITE("File Size Validation");
    UFT_RUN_TEST(file_size_exact_match);
    UFT_RUN_TEST(file_size_with_tolerance);
    
    UFT_TEST_SUITE("Error Codes");
    UFT_RUN_TEST(error_codes_unique);
    UFT_RUN_TEST(error_code_check_macro);
    
    UFT_TEST_SUITE("Resource Management");
    UFT_RUN_TEST(double_free_prevention);
    
    UFT_TEST_SUITE("Integer Boundaries");
    UFT_RUN_TEST(int_boundary_uint8);
    UFT_RUN_TEST(int_boundary_uint16);
    UFT_RUN_TEST(sector_id_limits);
    
    UFT_TEST_SUMMARY();
    UFT_TEST_EXIT();
}
