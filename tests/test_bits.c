/**
 * @file test_bits.c
 * @brief Unit tests for bit manipulation helpers
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "uft/compat/uft_bits.h"

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Testing %s... ", #name); \
    test_##name(); \
    printf("OK\n"); \
} while(0)

TEST(set_get_bit) {
    uint8_t buf[2] = {0, 0};
    
    /* Set bit 0 (MSB of first byte) */
    uft_set_bit(buf, 0, true);
    assert(buf[0] == 0x80);
    assert(uft_get_bit(buf, 0) == true);
    
    /* Set bit 7 (LSB of first byte) */
    uft_set_bit(buf, 7, true);
    assert(buf[0] == 0x81);
    assert(uft_get_bit(buf, 7) == true);
    
    /* Clear bit 0 */
    uft_clear_bit(buf, 0);
    assert(buf[0] == 0x01);
    
    /* Set bit in second byte */
    uft_set_bit(buf, 8, true);
    assert(buf[1] == 0x80);
}

TEST(toggle_bit) {
    uint8_t buf[1] = {0x00};
    uft_toggle_bit(buf, 4);  /* Middle bit */
    assert(buf[0] == 0x08);
    uft_toggle_bit(buf, 4);
    assert(buf[0] == 0x00);
}

TEST(read_le16) {
    uint8_t data[] = {0x34, 0x12};
    assert(uft_read_le16(data) == 0x1234);
}

TEST(read_le32) {
    uint8_t data[] = {0x78, 0x56, 0x34, 0x12};
    assert(uft_read_le32(data) == 0x12345678);
}

TEST(read_be16) {
    uint8_t data[] = {0x12, 0x34};
    assert(uft_read_be16(data) == 0x1234);
}

TEST(read_be32) {
    uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
    assert(uft_read_be32(data) == 0x12345678);
}

TEST(write_le16) {
    uint8_t data[2] = {0};
    uft_write_le16(data, 0x1234);
    assert(data[0] == 0x34);
    assert(data[1] == 0x12);
}

TEST(write_be16) {
    uint8_t data[2] = {0};
    uft_write_be16(data, 0x1234);
    assert(data[0] == 0x12);
    assert(data[1] == 0x34);
}

TEST(clamp) {
    assert(uft_clamp_u32(50, 0, 100) == 50);
    assert(uft_clamp_u32(150, 0, 100) == 100);
    assert(uft_clamp_u32(0, 10, 100) == 10);
    
    assert(uft_clamp_i32(-50, -100, 100) == -50);
    assert(uft_clamp_i32(-150, -100, 100) == -100);
}

TEST(popcount) {
    assert(uft_popcount8(0x00) == 0);
    assert(uft_popcount8(0xFF) == 8);
    assert(uft_popcount8(0x55) == 4);
    assert(uft_popcount8(0xAA) == 4);
    
    assert(uft_popcount32(0x00000000) == 0);
    assert(uft_popcount32(0xFFFFFFFF) == 32);
}

TEST(reverse_bits) {
    assert(uft_reverse_bits8(0x80) == 0x01);
    assert(uft_reverse_bits8(0x01) == 0x80);
    assert(uft_reverse_bits8(0xF0) == 0x0F);
}

int main(void) {
    printf("=== Bit Manipulation Tests ===\n");
    
    RUN_TEST(set_get_bit);
    RUN_TEST(toggle_bit);
    RUN_TEST(read_le16);
    RUN_TEST(read_le32);
    RUN_TEST(read_be16);
    RUN_TEST(read_be32);
    RUN_TEST(write_le16);
    RUN_TEST(write_be16);
    RUN_TEST(clamp);
    RUN_TEST(popcount);
    RUN_TEST(reverse_bits);
    
    printf("\nAll tests passed!\n");
    return 0;
}
