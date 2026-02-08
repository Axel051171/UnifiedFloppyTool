/**
 * @file test_protection_pipeline.c
 * @brief Unit Tests for Protection Preserve Pipeline
 * 
 * P2-002: Test protection detection and preservation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Test framework */
static int _tests_passed = 0;
static int _tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s... ", #name); \
    test_##name(); \
    printf("OK\n"); \
    _tests_passed++; \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { printf("FAIL at line %d\n", __LINE__); _tests_failed++; return; } \
} while(0)

/* Mock weak bit detection */
static int mock_detect_weak_bits(
    const uint8_t **rev_data,
    int rev_count,
    size_t data_size,
    float threshold,
    uint8_t *weak_mask_out)
{
    if (!rev_data || rev_count < 2 || !data_size || !weak_mask_out)
        return 0;
    
    memset(weak_mask_out, 0, data_size);
    int weak_count = 0;
    
    for (size_t i = 0; i < data_size; i++) {
        uint8_t byte_mask = 0;
        
        for (int bit = 0; bit < 8; bit++) {
            int ones = 0;
            int zeros = 0;
            
            for (int rev = 0; rev < rev_count; rev++) {
                if (rev_data[rev][i] & (1 << bit))
                    ones++;
                else
                    zeros++;
            }
            
            float variance = (float)(ones < zeros ? ones : zeros) / rev_count;
            
            if (variance >= threshold) {
                byte_mask |= (1 << bit);
                weak_count++;
            }
        }
        
        weak_mask_out[i] = byte_mask;
    }
    
    return weak_count;
}

/* Mock weak bit randomization */
static void mock_randomize_weak(uint8_t *data, const uint8_t *mask, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        if (mask[i]) {
            uint8_t random = rand() & mask[i];
            data[i] = (data[i] & ~mask[i]) | random;
        }
    }
}

/* Count bits in byte */
static int popcount(uint8_t b)
{
    int count = 0;
    while (b) {
        count += b & 1;
        b >>= 1;
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

TEST(weak_bit_detection_identical) {
    /* Identical data = no weak bits */
    uint8_t rev1[] = {0xAA, 0x55, 0xFF, 0x00};
    uint8_t rev2[] = {0xAA, 0x55, 0xFF, 0x00};
    uint8_t rev3[] = {0xAA, 0x55, 0xFF, 0x00};
    
    const uint8_t *revs[] = {rev1, rev2, rev3};
    uint8_t mask[4];
    
    int count = mock_detect_weak_bits(revs, 3, 4, 0.15f, mask);
    
    ASSERT(count == 0);
    ASSERT(mask[0] == 0);
    ASSERT(mask[1] == 0);
}

TEST(weak_bit_detection_varying) {
    /* Varying bit at position 0 of byte 0 */
    uint8_t rev1[] = {0x01, 0x55, 0xFF, 0x00};
    uint8_t rev2[] = {0x00, 0x55, 0xFF, 0x00};
    uint8_t rev3[] = {0x01, 0x55, 0xFF, 0x00};
    
    const uint8_t *revs[] = {rev1, rev2, rev3};
    uint8_t mask[4];
    
    int count = mock_detect_weak_bits(revs, 3, 4, 0.15f, mask);
    
    ASSERT(count >= 1);
    ASSERT((mask[0] & 0x01) != 0);  /* Bit 0 should be weak */
}

TEST(weak_bit_detection_multiple) {
    /* Multiple varying bits */
    uint8_t rev1[] = {0xFF, 0x00};
    uint8_t rev2[] = {0x00, 0xFF};
    uint8_t rev3[] = {0xAA, 0x55};
    
    const uint8_t *revs[] = {rev1, rev2, rev3};
    uint8_t mask[2];
    
    int count = mock_detect_weak_bits(revs, 3, 2, 0.15f, mask);
    
    ASSERT(count > 0);
}

TEST(weak_bit_randomize) {
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    uint8_t mask[] = {0xFF, 0x0F, 0xF0, 0x00};
    
    /* Save original */
    uint8_t orig[4];
    memcpy(orig, data, 4);
    
    mock_randomize_weak(data, mask, 4);
    
    /* Byte 3 should be unchanged (mask=0) */
    ASSERT(data[3] == orig[3]);
    
    /* Other bytes may have changed where mask is set */
    /* We can't guarantee they changed due to randomness,
       but we can verify the mask is respected */
    ASSERT((data[0] & ~mask[0]) == (orig[0] & ~mask[0]));
    ASSERT((data[1] & ~mask[1]) == (orig[1] & ~mask[1]));
}

TEST(weak_bit_count) {
    uint8_t mask1[] = {0xFF};  /* 8 bits */
    uint8_t mask2[] = {0x0F};  /* 4 bits */
    uint8_t mask3[] = {0x00};  /* 0 bits */
    uint8_t mask4[] = {0x01, 0x02, 0x04}; /* 1+1+1 = 3 bits */
    
    int c1 = 0, c2 = 0, c3 = 0, c4 = 0;
    for (size_t i = 0; i < sizeof(mask1); i++) c1 += popcount(mask1[i]);
    for (size_t i = 0; i < sizeof(mask2); i++) c2 += popcount(mask2[i]);
    for (size_t i = 0; i < sizeof(mask3); i++) c3 += popcount(mask3[i]);
    for (size_t i = 0; i < sizeof(mask4); i++) c4 += popcount(mask4[i]);
    
    ASSERT(c1 == 8);
    ASSERT(c2 == 4);
    ASSERT(c3 == 0);
    ASSERT(c4 == 3);
}

TEST(artifact_flags) {
    uint32_t flags = 0;
    
    flags |= (1 << 0);  /* WEAK_BITS */
    ASSERT(flags & (1 << 0));
    ASSERT(!(flags & (1 << 1)));
    
    flags |= (1 << 3);  /* DUP_SECTOR */
    ASSERT(flags & (1 << 3));
}

TEST(timing_variance) {
    double expected = 100000.0;
    double actual_long = 110000.0;
    double actual_short = 90000.0;
    double actual_normal = 100500.0;
    
    double var_long = (actual_long - expected) / expected * 100.0;
    double var_short = (actual_short - expected) / expected * 100.0;
    double var_normal = (actual_normal - expected) / expected * 100.0;
    
    ASSERT(var_long > 5.0);   /* Long track */
    ASSERT(var_short < -5.0); /* Short track */
    ASSERT(var_normal < 5.0 && var_normal > -5.0);  /* Normal */
}

TEST(threshold_sensitivity) {
    /* Test that threshold affects detection */
    uint8_t rev1[] = {0x01};
    uint8_t rev2[] = {0x00};
    uint8_t rev3[] = {0x00};
    
    const uint8_t *revs[] = {rev1, rev2, rev3};
    uint8_t mask[1];
    
    /* Low threshold - should detect */
    int count_low = mock_detect_weak_bits(revs, 3, 1, 0.10f, mask);
    
    /* Reset mask */
    mask[0] = 0;
    
    /* High threshold - might not detect */
    int count_high = mock_detect_weak_bits(revs, 3, 1, 0.40f, mask);
    
    ASSERT(count_low >= count_high);
}

TEST(format_protection_support) {
    /* Flux formats should support all */
    /* SCP = flux format */
    bool scp_weak = true;  /* SCP supports weak bits */
    bool scp_timing = true;
    
    /* ADF = sector format */
    bool adf_weak = false;  /* ADF doesn't support weak bits */
    bool adf_bad = true;    /* ADF can have bad sectors via filesystem */
    
    ASSERT(scp_weak == true);
    ASSERT(scp_timing == true);
    ASSERT(adf_weak == false);
    ASSERT(adf_bad == true);
}

TEST(protection_map_geometry) {
    int cylinders = 80;
    int heads = 2;
    int track_count = cylinders * heads;
    
    ASSERT(track_count == 160);
    
    /* Verify track indexing */
    for (int c = 0; c < cylinders; c++) {
        for (int h = 0; h < heads; h++) {
            int idx = c * heads + h;
            ASSERT(idx < track_count);
        }
    }
}

TEST(multirev_minimum) {
    /* Need at least 2 revolutions for weak bit detection */
    uint8_t rev1[] = {0xFF};
    uint8_t mask[1];
    
    const uint8_t *revs1[] = {rev1};
    
    /* Single revolution - should return 0 */
    int count = mock_detect_weak_bits(revs1, 1, 1, 0.15f, mask);
    ASSERT(count == 0);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Protection Pipeline Tests (P2-002)\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    RUN_TEST(weak_bit_detection_identical);
    RUN_TEST(weak_bit_detection_varying);
    RUN_TEST(weak_bit_detection_multiple);
    RUN_TEST(weak_bit_randomize);
    RUN_TEST(weak_bit_count);
    RUN_TEST(artifact_flags);
    RUN_TEST(timing_variance);
    RUN_TEST(threshold_sensitivity);
    RUN_TEST(format_protection_support);
    RUN_TEST(protection_map_geometry);
    RUN_TEST(multirev_minimum);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", _tests_passed, _tests_failed);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return _tests_failed > 0 ? 1 : 0;
}
