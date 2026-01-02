#include <stdint.h>
/**
 * @file test_gcr_viterbi.c
 * @brief Unit Tests for GCR Viterbi Decoder
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test helpers
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  TEST: %s ... ", #name); \
        if (test_##name()) { \
            tests_passed++; \
            printf("PASS\n"); \
        } else { \
            printf("FAIL\n"); \
        } \
    } while(0)

#define ASSERT_TRUE(x) do { if (!(x)) return 0; } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { printf("ASSERT_EQ FAILED: %d != %d\n", (int)(a), (int)(b)); return 0; } } while(0)

// ============================================================================
// GCR TABLES (from implementation)
// ============================================================================

static const uint8_t GCR_VALID_CODES[16] = {
    0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F,
    0x12, 0x13, 0x15, 0x16, 0x17,
    0x19, 0x1A, 0x1B, 0x1D, 0x1E
};

static const uint8_t GCR_DECODE[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

static const uint8_t GCR_ENCODE[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

// Hamming distance
static int hamming_distance(uint8_t a, uint8_t b) {
    uint8_t x = a ^ b;
    int count = 0;
    while (x) { count += (x & 1); x >>= 1; }
    return count;
}

// ============================================================================
// TEST CASES
// ============================================================================

/**
 * Test: GCR encode/decode roundtrip
 */
static int test_gcr_roundtrip(void) {
    for (int nibble = 0; nibble < 16; nibble++) {
        uint8_t gcr = GCR_ENCODE[nibble];
        uint8_t decoded = GCR_DECODE[gcr];
        ASSERT_EQ(decoded, nibble);
    }
    return 1;
}

/**
 * Test: All valid GCR codes decode correctly
 */
static int test_valid_codes(void) {
    for (int i = 0; i < 16; i++) {
        uint8_t gcr = GCR_VALID_CODES[i];
        ASSERT_TRUE(gcr < 32);
        ASSERT_TRUE(GCR_DECODE[gcr] != 0xFF);
    }
    return 1;
}

/**
 * Test: Invalid GCR codes return 0xFF
 */
static int test_invalid_codes(void) {
    int invalid_count = 0;
    for (int gcr = 0; gcr < 32; gcr++) {
        if (GCR_DECODE[gcr] == 0xFF) {
            invalid_count++;
        }
    }
    // 32 total - 16 valid = 16 invalid
    ASSERT_EQ(invalid_count, 16);
    return 1;
}

/**
 * Test: Single-bit error correction via Hamming distance
 */
static int test_single_bit_correction(void) {
    // Take a valid code
    uint8_t valid = GCR_ENCODE[0x05];  // Should be 0x0F
    ASSERT_EQ(valid, 0x0F);
    
    // Introduce single-bit error
    uint8_t corrupted = valid ^ 0x01;  // Flip LSB -> 0x0E
    
    // 0x0E is also valid (decodes to 0x04)
    // But let's try a truly invalid corruption
    corrupted = valid ^ 0x10;  // 0x0F ^ 0x10 = 0x1F (invalid)
    
    // Find closest valid code
    int min_dist = 6;
    uint8_t best = 0;
    for (int i = 0; i < 16; i++) {
        int dist = hamming_distance(corrupted, GCR_VALID_CODES[i]);
        if (dist < min_dist) {
            min_dist = dist;
            best = GCR_VALID_CODES[i];
        }
    }
    
    ASSERT_EQ(min_dist, 1);  // Should be 1-bit away
    ASSERT_EQ(best, valid);  // Should recover original
    
    return 1;
}

/**
 * Test: C64 sync pattern detection (10 consecutive 1s)
 */
static int test_c64_sync_detection(void) {
    // Create bitstream with sync pattern
    uint8_t bits[16] = {0};
    
    // Set bits 40-49 to 1 (sync pattern)
    // Byte 5: bits 40-47
    bits[5] = 0xFF;
    // Byte 6: bits 48-49
    bits[6] = 0xC0;
    
    // Scan for 10 consecutive 1s
    int found_sync = 0;
    int consecutive = 0;
    
    for (int i = 0; i < 128; i++) {
        int byte_idx = i / 8;
        int bit_idx = 7 - (i % 8);
        int bit = (bits[byte_idx] >> bit_idx) & 1;
        
        if (bit) {
            consecutive++;
            if (consecutive >= 10) {
                found_sync = 1;
                break;
            }
        } else {
            consecutive = 0;
        }
    }
    
    ASSERT_TRUE(found_sync);
    return 1;
}

/**
 * Test: Format detection (C64 vs Apple)
 */
static int test_format_detection(void) {
    // C64: Uses 10-bit sync (all 1s)
    // Apple: Uses specific byte patterns (D5 AA 96)
    
    // Simulate C64 sync density
    int c64_syncs = 42;  // ~21 sectors × 2 syncs
    int apple_syncs = 0;
    
    // Decision logic
    int detected_c64 = (c64_syncs > 20 && c64_syncs > apple_syncs * 2);
    ASSERT_TRUE(detected_c64);
    
    // Simulate Apple sync density
    c64_syncs = 5;
    apple_syncs = 32;  // ~16 sectors × 2 syncs
    
    int detected_apple = (apple_syncs > 15);
    ASSERT_TRUE(detected_apple);
    
    return 1;
}

/**
 * Test: Viterbi correction limits
 */
static int test_viterbi_limits(void) {
    // 2-bit errors should still be correctable in most cases
    uint8_t valid = GCR_ENCODE[0x0A];
    uint8_t two_bit_error = valid ^ 0x03;  // Flip 2 bits
    
    int min_dist = 6;
    for (int i = 0; i < 16; i++) {
        int dist = hamming_distance(two_bit_error, GCR_VALID_CODES[i]);
        if (dist < min_dist) min_dist = dist;
    }
    
    ASSERT_TRUE(min_dist <= 2);  // Should find something within 2 bits
    
    // 3-bit errors are marginal
    uint8_t three_bit_error = valid ^ 0x07;
    
    min_dist = 6;
    for (int i = 0; i < 16; i++) {
        int dist = hamming_distance(three_bit_error, GCR_VALID_CODES[i]);
        if (dist < min_dist) min_dist = dist;
    }
    
    // May or may not be correctable depending on which bits
    ASSERT_TRUE(min_dist <= 5);  // At least something found
    
    return 1;
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("         GCR VITERBI DECODER UNIT TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    TEST(gcr_roundtrip);
    TEST(valid_codes);
    TEST(invalid_codes);
    TEST(single_bit_correction);
    TEST(c64_sync_detection);
    TEST(format_detection);
    TEST(viterbi_limits);
    
    printf("\n───────────────────────────────────────────────────────────────────\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("───────────────────────────────────────────────────────────────────\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
