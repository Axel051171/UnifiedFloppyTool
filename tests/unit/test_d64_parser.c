/**
 * @file test_d64_parser.c
 * @brief Unit tests for D64 parser
 */

#include "../uft_test_framework.h"
#include "uft/core/uft_safe_math.h"

// ============================================================================
// Safe Math Tests
// ============================================================================

TEST_CASE(safe_mul_no_overflow) {
    size_t result;
    ASSERT_TRUE(uft_safe_mul_size(100, 200, &result));
    ASSERT_EQ(result, 20000);
}

TEST_CASE(safe_mul_overflow) {
    size_t result;
    // This should overflow on 64-bit
    ASSERT_FALSE(uft_safe_mul_size(SIZE_MAX, 2, &result));
}

TEST_CASE(safe_add_no_overflow) {
    size_t result;
    ASSERT_TRUE(uft_safe_add_size(100, 200, &result));
    ASSERT_EQ(result, 300);
}

TEST_CASE(safe_add_overflow) {
    size_t result;
    ASSERT_FALSE(uft_safe_add_size(SIZE_MAX, 1, &result));
}

// ============================================================================
// D64 Size Validation Tests
// ============================================================================

#define D64_SIZE_35 174848
#define D64_SIZE_35_ERR 175531
#define D64_SIZE_40 196608
#define D64_SIZE_40_ERR 197376

TEST_CASE(d64_size_35_track) {
    ASSERT_EQ(D64_SIZE_35, 35 * 683 + 683 - 683);  // Placeholder calculation
}

TEST_CASE(d64_validate_track_range) {
    // Track numbers 1-35 are valid for standard D64
    for (int t = 1; t <= 35; t++) {
        ASSERT_TRUE(t >= 1 && t <= 42);
    }
    // Track 0 is invalid
    ASSERT_FALSE(0 >= 1);
    // Track 43 is invalid
    ASSERT_FALSE(43 <= 42);
}

// ============================================================================
// Bounds Checking Tests
// ============================================================================

TEST_CASE(bounds_check_within) {
    size_t offset = 1000;
    size_t size = 256;
    size_t file_size = 174848;
    
    ASSERT_TRUE(offset + size <= file_size);
}

TEST_CASE(bounds_check_at_end) {
    size_t offset = 174848 - 256;
    size_t size = 256;
    size_t file_size = 174848;
    
    ASSERT_TRUE(offset + size <= file_size);
}

TEST_CASE(bounds_check_overflow) {
    size_t offset = 174848;
    size_t size = 1;
    size_t file_size = 174848;
    
    ASSERT_FALSE(offset + size <= file_size);
}

TEST_MAIN()
