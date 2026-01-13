/**
 * @file test_recovery.c
 * @brief Recovery Algorithm Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

static int _pass = 0, _fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name) do { printf("  [TEST] %s... ", #name); test_##name(); printf("OK\n"); _pass++; } while(0)
#define ASSERT(c) do { if(!(c)) { printf("FAIL @ %d\n", __LINE__); _fail++; return; } } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Inline implementations for testing (matching uft_recovery.h/c)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_REC_OK = 0, UFT_REC_PARTIAL = 1, UFT_REC_CRC_ERROR = 2,
    UFT_REC_WEAK = 3, UFT_REC_UNREADABLE = 4, UFT_REC_NO_SYNC = 5,
    UFT_REC_NO_HEADER = 6, UFT_REC_NO_DATA = 7, UFT_REC_TIMEOUT = 8,
    UFT_REC_IO_ERROR = 9
} uft_recovery_status_t;

static uint8_t vote_bit(const uint8_t *bits, size_t count, uint8_t *conf) {
    size_t ones = 0;
    for (size_t i = 0; i < count; i++) if (bits[i]) ones++;
    *conf = (ones > count - ones) ? ones : (count - ones);
    return (ones > count / 2) ? 1 : 0;
}

static uint32_t analyze_revolutions(
    const uint8_t **revolutions, size_t rev_count, size_t bit_count,
    uint8_t *consensus, uint8_t *weak_mask, uint8_t *confidence)
{
    if (!revolutions || rev_count < 2 || !consensus) return 0;
    uint32_t weak_count = 0;
    size_t byte_count = (bit_count + 7) / 8;
    memset(consensus, 0, byte_count);
    if (weak_mask) memset(weak_mask, 0, byte_count);
    if (confidence) memset(confidence, 0xFF, byte_count);
    
    for (size_t bit = 0; bit < bit_count; bit++) {
        size_t byte_idx = bit / 8;
        uint8_t bit_mask = 0x80 >> (bit % 8);
        size_t ones = 0;
        for (size_t rev = 0; rev < rev_count; rev++) {
            if (revolutions[rev][byte_idx] & bit_mask) ones++;
        }
        bool is_one = (ones > rev_count / 2);
        if (is_one) consensus[byte_idx] |= bit_mask;
        bool is_weak = (ones > 0 && ones < rev_count);
        if (is_weak) {
            weak_count++;
            if (weak_mask) weak_mask[byte_idx] |= bit_mask;
        }
        if (confidence) {
            size_t agreement = is_one ? ones : (rev_count - ones);
            confidence[byte_idx] = (uint8_t)((agreement * 255) / rev_count);
        }
    }
    return weak_count;
}

static uint16_t calc_crc16(const uint8_t *data, size_t size) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < size; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}

static bool fix_crc_single_bit(uint8_t *data, size_t size, uint16_t expected, int *fixed) {
    for (size_t i = 0; i < size; i++) {
        for (int bit = 0; bit < 8; bit++) {
            data[i] ^= (1 << bit);
            if (calc_crc16(data, size) == expected) {
                if (fixed) *fixed = (int)(i * 8 + (7 - bit));
                return true;
            }
            data[i] ^= (1 << bit);
        }
    }
    if (fixed) *fixed = -1;
    return false;
}

static uint8_t interpolate_sector(const uint8_t *prev, const uint8_t *next, size_t size, uint8_t *out) {
    if (!out || size == 0) return 0;
    if (!prev && !next) { memset(out, 0xE5, size); return 0; }
    if (prev && !next) { memcpy(out, prev, size); return 30; }
    if (!prev && next) { memcpy(out, next, size); return 30; }
    for (size_t i = 0; i < size; i++) out[i] = (prev[i] + next[i]) / 2;
    return 50;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

TEST(status_codes) {
    ASSERT(UFT_REC_OK == 0);
    ASSERT(UFT_REC_PARTIAL == 1);
    ASSERT(UFT_REC_UNREADABLE == 4);
    ASSERT(UFT_REC_IO_ERROR == 9);
}

TEST(vote_bit_unanimous_zero) {
    uint8_t bits[] = {0, 0, 0, 0, 0};
    uint8_t conf;
    uint8_t result = vote_bit(bits, 5, &conf);
    ASSERT(result == 0);
    ASSERT(conf == 5);  /* All agree */
}

TEST(vote_bit_unanimous_one) {
    uint8_t bits[] = {1, 1, 1, 1, 1};
    uint8_t conf;
    uint8_t result = vote_bit(bits, 5, &conf);
    ASSERT(result == 1);
    ASSERT(conf == 5);
}

TEST(vote_bit_majority_one) {
    uint8_t bits[] = {1, 1, 1, 0, 0};
    uint8_t conf;
    uint8_t result = vote_bit(bits, 5, &conf);
    ASSERT(result == 1);
    ASSERT(conf == 3);  /* 3 agree */
}

TEST(vote_bit_majority_zero) {
    uint8_t bits[] = {0, 0, 0, 1, 1};
    uint8_t conf;
    uint8_t result = vote_bit(bits, 5, &conf);
    ASSERT(result == 0);
    ASSERT(conf == 3);
}

TEST(analyze_revolutions_no_weak) {
    /* All revolutions identical */
    uint8_t rev1[] = {0xFF, 0x00};
    uint8_t rev2[] = {0xFF, 0x00};
    uint8_t rev3[] = {0xFF, 0x00};
    const uint8_t *revs[] = {rev1, rev2, rev3};
    
    uint8_t consensus[2], weak[2];
    uint32_t weak_count = analyze_revolutions(revs, 3, 16, consensus, weak, NULL);
    
    ASSERT(weak_count == 0);
    ASSERT(consensus[0] == 0xFF);
    ASSERT(consensus[1] == 0x00);
    ASSERT(weak[0] == 0x00);
    ASSERT(weak[1] == 0x00);
}

TEST(analyze_revolutions_with_weak) {
    /* One bit differs */
    uint8_t rev1[] = {0xFF};
    uint8_t rev2[] = {0xFE};  /* LSB different */
    uint8_t rev3[] = {0xFF};
    const uint8_t *revs[] = {rev1, rev2, rev3};
    
    uint8_t consensus[1], weak[1];
    uint32_t weak_count = analyze_revolutions(revs, 3, 8, consensus, weak, NULL);
    
    ASSERT(weak_count == 1);  /* One weak bit */
    ASSERT(consensus[0] == 0xFF);  /* Majority wins */
    ASSERT(weak[0] == 0x01);  /* LSB is weak */
}

TEST(crc16_known_value) {
    uint8_t data[] = "123456789";
    uint16_t crc = calc_crc16(data, 9);
    /* CRC-16-CCITT of "123456789" = 0x29B1 */
    ASSERT(crc == 0x29B1);
}

TEST(crc16_empty) {
    uint8_t data[] = {0};
    uint16_t crc = calc_crc16(data, 1);
    ASSERT(crc != 0);  /* Not zero */
}

TEST(fix_crc_single_bit_correctable) {
    uint8_t original[] = {0x31, 0x32, 0x33};  /* "123" */
    uint16_t good_crc = calc_crc16(original, 3);
    
    /* Flip one bit */
    uint8_t corrupted[] = {0x31, 0x32, 0x33};
    corrupted[1] ^= 0x01;  /* Flip LSB of middle byte */
    
    int fixed_bit = -1;
    bool result = fix_crc_single_bit(corrupted, 3, good_crc, &fixed_bit);
    
    ASSERT(result == true);
    ASSERT(corrupted[0] == original[0]);
    ASSERT(corrupted[1] == original[1]);
    ASSERT(corrupted[2] == original[2]);
}

TEST(interpolate_no_neighbors) {
    uint8_t out[4];
    uint8_t conf = interpolate_sector(NULL, NULL, 4, out);
    ASSERT(conf == 0);
    ASSERT(out[0] == 0xE5);  /* Uninitialized pattern */
}

TEST(interpolate_prev_only) {
    uint8_t prev[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t out[4];
    uint8_t conf = interpolate_sector(prev, NULL, 4, out);
    ASSERT(conf == 30);
    ASSERT(out[0] == 0x11);
}

TEST(interpolate_next_only) {
    uint8_t next[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t out[4];
    uint8_t conf = interpolate_sector(NULL, next, 4, out);
    ASSERT(conf == 30);
    ASSERT(out[0] == 0xAA);
}

TEST(interpolate_both_neighbors) {
    uint8_t prev[] = {0x00, 0x10, 0x20, 0x30};
    uint8_t next[] = {0x10, 0x20, 0x30, 0x40};
    uint8_t out[4];
    uint8_t conf = interpolate_sector(prev, next, 4, out);
    ASSERT(conf == 50);
    ASSERT(out[0] == 0x08);  /* (0x00 + 0x10) / 2 */
    ASSERT(out[1] == 0x18);  /* (0x10 + 0x20) / 2 */
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Recovery Algorithm Tests\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    RUN(status_codes);
    RUN(vote_bit_unanimous_zero);
    RUN(vote_bit_unanimous_one);
    RUN(vote_bit_majority_one);
    RUN(vote_bit_majority_zero);
    RUN(analyze_revolutions_no_weak);
    RUN(analyze_revolutions_with_weak);
    RUN(crc16_known_value);
    RUN(crc16_empty);
    RUN(fix_crc_single_bit_correctable);
    RUN(interpolate_no_neighbors);
    RUN(interpolate_prev_only);
    RUN(interpolate_next_only);
    RUN(interpolate_both_neighbors);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", _pass, _fail);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return _fail > 0 ? 1 : 0;
}
