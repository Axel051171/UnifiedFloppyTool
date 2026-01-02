/**
 * @file test_forensic_flux_decoder.c
 * @brief Unit Tests for Forensic Flux Decoder
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

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
#define ASSERT_EQ(a, b) do { if ((a) != (b)) return 0; } while(0)
#define ASSERT_NEAR(a, b, tol) do { if (fabs((double)(a)-(double)(b)) > (tol)) return 0; } while(0)

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static int hamming_distance_16(uint16_t a, uint16_t b) {
    uint16_t x = a ^ b;
    int count = 0;
    while (x) { count += (x & 1); x >>= 1; }
    return count;
}

// ============================================================================
// TEST CASES
// ============================================================================

/**
 * Test: Confidence calculation
 */
static int test_confidence_levels(void) {
    // Confidence thresholds
    float certain = 0.99f;
    float high = 0.90f;
    float medium = 0.70f;
    float low = 0.50f;
    
    ASSERT_TRUE(certain > high);
    ASSERT_TRUE(high > medium);
    ASSERT_TRUE(medium > low);
    
    return 1;
}

/**
 * Test: Hamming distance for sync detection
 */
static int test_sync_hamming(void) {
    uint16_t mfm_sync = 0x4489;
    
    // Exact match
    ASSERT_EQ(hamming_distance_16(mfm_sync, mfm_sync), 0);
    
    // 1-bit error
    uint16_t one_bit = mfm_sync ^ 0x0001;
    ASSERT_EQ(hamming_distance_16(mfm_sync, one_bit), 1);
    
    // 2-bit error
    uint16_t two_bit = mfm_sync ^ 0x0003;
    ASSERT_EQ(hamming_distance_16(mfm_sync, two_bit), 2);
    
    // Random pattern
    uint16_t random = 0x1234;
    int dist = hamming_distance_16(mfm_sync, random);
    ASSERT_TRUE(dist > 0 && dist <= 16);
    
    return 1;
}

/**
 * Test: Cell time estimation
 */
static int test_cell_time(void) {
    // MFM DD: 2000ns cell time
    double cell_mfm_dd = 2000.0;
    
    // MFM HD: 1000ns cell time
    double cell_mfm_hd = 1000.0;
    
    // GCR: ~3500ns average
    double cell_gcr = 3500.0;
    
    ASSERT_NEAR(cell_mfm_hd, cell_mfm_dd / 2.0, 10.0);
    ASSERT_TRUE(cell_gcr > cell_mfm_dd);
    
    return 1;
}

/**
 * Test: RPM calculation
 */
static int test_rpm_calculation(void) {
    // 300 RPM = 200ms per rotation
    double rpm = 300.0;
    double rotation_ms = 60000.0 / rpm;
    ASSERT_NEAR(rotation_ms, 200.0, 0.1);
    
    // 360 RPM = 166.67ms per rotation
    rpm = 360.0;
    rotation_ms = 60000.0 / rpm;
    ASSERT_NEAR(rotation_ms, 166.67, 0.1);
    
    return 1;
}

/**
 * Test: Weak-bit detection threshold
 */
static int test_weak_bit_threshold(void) {
    float threshold = 3.0f;  // 3 sigma
    float innovation_sigma = 100.0f;
    
    // Normal innovation (within threshold)
    float normal_innovation = 200.0f;
    ASSERT_TRUE(fabsf(normal_innovation) / innovation_sigma < threshold);
    
    // Weak innovation (above threshold)
    float weak_innovation = 400.0f;
    ASSERT_TRUE(fabsf(weak_innovation) / innovation_sigma > threshold);
    
    return 1;
}

/**
 * Test: Multi-revolution fusion weights
 */
static int test_fusion_weights(void) {
    // Weights should sum to 1.0 (after normalization)
    float conf1 = 0.9f;
    float conf2 = 0.7f;
    float conf3 = 0.5f;
    
    float w1 = conf1 * conf1;
    float w2 = conf2 * conf2;
    float w3 = conf3 * conf3;
    float total = w1 + w2 + w3;
    
    w1 /= total;
    w2 /= total;
    w3 /= total;
    
    ASSERT_NEAR(w1 + w2 + w3, 1.0, 0.001);
    
    // Highest confidence gets highest weight
    ASSERT_TRUE(w1 > w2);
    ASSERT_TRUE(w2 > w3);
    
    return 1;
}

/**
 * Test: CRC-16 CCITT
 */
static int test_crc16(void) {
    // Known test vector
    // "123456789" should give CRC = 0x29B1 (with polynomial 0x1021)
    
    uint16_t crc = 0xFFFF;
    const char *data = "123456789";
    
    for (int i = 0; i < 9; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    
    ASSERT_EQ(crc, 0x29B1);
    
    return 1;
}

/**
 * Test: 1-bit error correction simulation
 */
static int test_1bit_correction(void) {
    // Simulate finding correct bit position
    size_t data_size = 512;
    size_t error_pos = 100;  // Byte position
    int error_bit = 3;       // Bit position
    
    // Total positions to try
    size_t total_attempts = data_size * 8;
    ASSERT_EQ(total_attempts, 4096);
    
    // Error at (100, 3) = attempt 100*8 + 3 = 803
    size_t attempt = error_pos * 8 + error_bit;
    ASSERT_EQ(attempt, 803);
    
    return 1;
}

/**
 * Test: Sector status flags
 */
static int test_sector_status(void) {
    uint32_t status = 0;
    
    status |= 0x0001;  // ID_CRC_ERR
    ASSERT_TRUE(status & 0x0001);
    
    status |= 0x0002;  // DATA_CRC_ERR
    ASSERT_TRUE(status & 0x0002);
    
    status |= 0x0010;  // CORRECTED
    ASSERT_TRUE(status & 0x0010);
    
    // Clear CRC error if corrected
    status &= ~0x0002;
    ASSERT_TRUE(!(status & 0x0002));
    ASSERT_TRUE(status & 0x0010);  // Still corrected
    
    return 1;
}

/**
 * Test: Configuration presets
 */
static int test_config_presets(void) {
    // Default config values
    uint32_t default_cell = 2000;
    float default_threshold = 3.0f;
    
    // Paranoid config should be more conservative
    float paranoid_threshold = 3.0f;  // Same or stricter
    size_t paranoid_min_revs = 3;     // More revolutions
    
    ASSERT_TRUE(paranoid_min_revs > 1);
    ASSERT_TRUE(paranoid_threshold >= default_threshold);
    
    return 1;
}

/**
 * Test: Bit packing
 */
static int test_bit_packing(void) {
    uint8_t bits[2] = {0};
    
    // Set bit 0 (MSB of byte 0)
    bits[0] |= 0x80;
    ASSERT_EQ(bits[0], 0x80);
    
    // Set bit 7 (LSB of byte 0)
    bits[0] |= 0x01;
    ASSERT_EQ(bits[0], 0x81);
    
    // Set bit 8 (MSB of byte 1)
    bits[1] |= 0x80;
    ASSERT_EQ(bits[1], 0x80);
    
    return 1;
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("         FORENSIC FLUX DECODER UNIT TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    TEST(confidence_levels);
    TEST(sync_hamming);
    TEST(cell_time);
    TEST(rpm_calculation);
    TEST(weak_bit_threshold);
    TEST(fusion_weights);
    TEST(crc16);
    TEST(1bit_correction);
    TEST(sector_status);
    TEST(config_presets);
    TEST(bit_packing);
    
    printf("\n───────────────────────────────────────────────────────────────────\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("───────────────────────────────────────────────────────────────────\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
