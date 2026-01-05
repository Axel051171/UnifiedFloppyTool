/**
 * @file test_god_mode.c
 * @brief Unit tests for GOD MODE algorithms
 */

#include "../uft_test_framework.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// Test: Kalman PLL
// ============================================================================

typedef struct {
    double cell_time;
    double variance;
} pll_state_t;

void pll_init(pll_state_t* s) {
    s->cell_time = 2000.0;
    s->variance = 200.0;
}

int pll_update(pll_state_t* s, double flux) {
    int bits = (int)(flux / s->cell_time + 0.5);
    if (bits < 1) bits = 1;
    if (bits > 5) bits = 5;
    
    double K = 0.1;
    double residual = flux - s->cell_time * bits;
    s->cell_time += K * (residual / bits);
    
    if (s->cell_time < 1500) s->cell_time = 1500;
    if (s->cell_time > 3000) s->cell_time = 3000;
    
    return bits;
}

TEST_CASE(kalman_pll_single_bit) {
    pll_state_t pll;
    pll_init(&pll);
    
    // Single bit cell should return 1
    int bits = pll_update(&pll, 2000.0);
    ASSERT_EQ(bits, 1);
}

TEST_CASE(kalman_pll_double_bit) {
    pll_state_t pll;
    pll_init(&pll);
    
    // Double bit cell should return 2
    int bits = pll_update(&pll, 4000.0);
    ASSERT_EQ(bits, 2);
}

TEST_CASE(kalman_pll_adapts) {
    pll_state_t pll;
    pll_init(&pll);
    
    double initial = pll.cell_time;
    
    // Feed consistently longer cells
    for (int i = 0; i < 20; i++) {
        pll_update(&pll, 2200.0);  // 10% longer
    }
    
    // PLL should have adapted upward
    ASSERT_TRUE(pll.cell_time > initial);
}

TEST_CASE(kalman_pll_bounds) {
    pll_state_t pll;
    pll_init(&pll);
    
    // Try to push below minimum
    for (int i = 0; i < 100; i++) {
        pll_update(&pll, 500.0);
    }
    ASSERT_GE(pll.cell_time, 1500.0);
    
    // Try to push above maximum
    pll_init(&pll);
    for (int i = 0; i < 100; i++) {
        pll_update(&pll, 5000.0);
    }
    ASSERT_LE(pll.cell_time, 3000.0);
}

// ============================================================================
// Test: Hamming Distance
// ============================================================================

static int hamming16(uint16_t a, uint16_t b) {
    uint16_t diff = a ^ b;
    int count = 0;
    while (diff) {
        count += diff & 1;
        diff >>= 1;
    }
    return count;
}

TEST_CASE(hamming_zero_distance) {
    ASSERT_EQ(hamming16(0x4489, 0x4489), 0);
}

TEST_CASE(hamming_one_bit) {
    ASSERT_EQ(hamming16(0x4489, 0x4488), 1);
    ASSERT_EQ(hamming16(0x4489, 0x448B), 1);
}

TEST_CASE(hamming_multiple_bits) {
    ASSERT_EQ(hamming16(0x0000, 0xFFFF), 16);
    ASSERT_EQ(hamming16(0xAAAA, 0x5555), 16);
}

// ============================================================================
// Test: CRC-16 CCITT
// ============================================================================

uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

TEST_CASE(crc16_empty) {
    uint8_t data[1] = {0};
    uint16_t crc = crc16(data, 0);
    ASSERT_EQ(crc, 0xFFFF);  // Initial value
}

TEST_CASE(crc16_known_value) {
    uint8_t data[] = "123456789";
    uint16_t crc = crc16(data, 9);
    ASSERT_EQ(crc, 0x29B1);  // Known CRC-16 CCITT for this string
}

TEST_CASE(crc16_single_byte) {
    uint8_t data[1] = {0x00};
    uint16_t crc = crc16(data, 1);
    ASSERT_TRUE(crc != 0xFFFF);  // Should have changed
}

// ============================================================================
// Test: 1-Bit CRC Correction
// ============================================================================

bool try_1bit_fix(uint8_t* data, size_t len, uint16_t expected, size_t* flip_pos) {
    uint16_t original = crc16(data, len);
    if (original == expected) {
        return true;  // Already correct
    }
    
    for (size_t byte = 0; byte < len; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            data[byte] ^= (1 << bit);
            if (crc16(data, len) == expected) {
                if (flip_pos) *flip_pos = byte * 8 + bit;
                return true;
            }
            data[byte] ^= (1 << bit);
        }
    }
    return false;
}

TEST_CASE(crc_correction_no_error) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint16_t crc = crc16(data, 4);
    
    size_t pos;
    ASSERT_TRUE(try_1bit_fix(data, 4, crc, &pos));
}

TEST_CASE(crc_correction_single_error) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint16_t correct_crc = crc16(data, 4);
    
    // Introduce error
    data[2] ^= 0x10;  // Flip bit 4 of byte 2
    
    size_t pos;
    ASSERT_TRUE(try_1bit_fix(data, 4, correct_crc, &pos));
    ASSERT_EQ(data[2], 0x03);  // Should be restored
}

// ============================================================================
// Test: Multi-Rev Fusion
// ============================================================================

typedef struct {
    uint8_t value;
    float confidence;
    bool weak;
} fused_t;

void fuse(const uint8_t** revs, int num_revs, size_t bits, fused_t* out) {
    for (size_t i = 0; i < bits; i++) {
        int ones = 0;
        for (int r = 0; r < num_revs; r++) {
            if (revs[r][i / 8] & (1 << (7 - i % 8))) ones++;
        }
        out[i].value = (ones > num_revs / 2) ? 1 : 0;
        out[i].confidence = (float)(ones > num_revs - ones ? ones : num_revs - ones) / num_revs;
        out[i].weak = (out[i].confidence < 0.8f);
    }
}

TEST_CASE(fusion_unanimous_ones) {
    uint8_t rev1[] = {0xFF};
    uint8_t rev2[] = {0xFF};
    uint8_t rev3[] = {0xFF};
    const uint8_t* revs[] = {rev1, rev2, rev3};
    
    fused_t result[8];
    fuse(revs, 3, 8, result);
    
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(result[i].value, 1);
        ASSERT_TRUE(result[i].confidence >= 0.99f);
        ASSERT_FALSE(result[i].weak);
    }
}

TEST_CASE(fusion_unanimous_zeros) {
    uint8_t rev1[] = {0x00};
    uint8_t rev2[] = {0x00};
    uint8_t rev3[] = {0x00};
    const uint8_t* revs[] = {rev1, rev2, rev3};
    
    fused_t result[8];
    fuse(revs, 3, 8, result);
    
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(result[i].value, 0);
        ASSERT_FALSE(result[i].weak);
    }
}

TEST_CASE(fusion_majority_vote) {
    uint8_t rev1[] = {0xFF};  // 11111111
    uint8_t rev2[] = {0xFF};  // 11111111
    uint8_t rev3[] = {0x00};  // 00000000
    const uint8_t* revs[] = {rev1, rev2, rev3};
    
    fused_t result[8];
    fuse(revs, 3, 8, result);
    
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(result[i].value, 1);  // Majority is 1
    }
}

TEST_CASE(fusion_weak_bit_detection) {
    uint8_t rev1[] = {0xFF};
    uint8_t rev2[] = {0x00};
    uint8_t rev3[] = {0x00};
    uint8_t rev4[] = {0xFF};
    uint8_t rev5[] = {0x00};
    const uint8_t* revs[] = {rev1, rev2, rev3, rev4, rev5};
    
    fused_t result[8];
    fuse(revs, 5, 8, result);
    
    // 2 ones, 3 zeros -> 60% confidence -> should be weak
    for (int i = 0; i < 8; i++) {
        ASSERT_TRUE(result[i].weak);
    }
}

// ============================================================================
// Test: GCR Decode
// ============================================================================

static const uint8_t gcr_decode_table[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

TEST_CASE(gcr_decode_valid) {
    // Test all valid GCR codes
    for (int code = 0; code < 32; code++) {
        uint8_t decoded = gcr_decode_table[code];
        if (decoded != 0xFF) {
            ASSERT_LT(decoded, 16);
        }
    }
}

TEST_CASE(gcr_decode_invalid) {
    // Some codes are invalid
    ASSERT_EQ(gcr_decode_table[0], 0xFF);
    ASSERT_EQ(gcr_decode_table[1], 0xFF);
}

// ============================================================================
// Main
// ============================================================================

TEST_MAIN()
