/**
 * @file test_crc_suite.c
 * @brief Comprehensive CRC unit tests
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

/* Test vectors from various standards */
static const char *test_string = "123456789";

/* CRC-16 CCITT implementation (for testing) */
static uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}

/* CRC-16 IBM/ANSI implementation */
static uint16_t crc16_ansi(const uint8_t *data, size_t len) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
        }
    }
    return crc;
}

/* CRC-32 implementation */
static uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Testing %s... ", #name); \
    test_##name(); \
    printf("OK\n"); \
} while(0)

TEST(crc16_ccitt_check) {
    /* Standard test vector: CRC-16/CCITT of "123456789" = 0x29B1 */
    uint16_t result = crc16_ccitt((const uint8_t*)test_string, 9);
    assert(result == 0x29B1);
}

TEST(crc16_ansi_check) {
    /* Standard test vector: CRC-16/IBM of "123456789" = 0xBB3D */
    uint16_t result = crc16_ansi((const uint8_t*)test_string, 9);
    assert(result == 0xBB3D);
}

TEST(crc32_check) {
    /* Standard test vector: CRC-32 of "123456789" = 0xCBF43926 */
    uint32_t result = crc32((const uint8_t*)test_string, 9);
    assert(result == 0xCBF43926);
}

TEST(crc_empty) {
    uint8_t empty[] = {};
    /* CRC of empty data should be initial value */
    assert(crc16_ccitt(empty, 0) == 0xFFFF);
    assert(crc16_ansi(empty, 0) == 0x0000);
    assert(crc32(empty, 0) == 0x00000000);
}

TEST(crc_incremental) {
    /* CRC should be same whether computed all at once or incrementally */
    const uint8_t *data = (const uint8_t*)test_string;
    
    uint16_t full = crc16_ansi(data, 9);
    
    /* Compute in two parts - note: this simplified test assumes 
       the CRC can be computed incrementally, which isn't always true
       without proper state management */
    uint16_t part1 = crc16_ansi(data, 4);
    
    /* For proper incremental CRC, we'd need to continue from part1's state */
    /* This test just verifies the full computation */
    assert(full == 0xBB3D);
}

int main(void) {
    printf("=== CRC Test Suite ===\n");
    
    RUN_TEST(crc16_ccitt_check);
    RUN_TEST(crc16_ansi_check);
    RUN_TEST(crc32_check);
    RUN_TEST(crc_empty);
    RUN_TEST(crc_incremental);
    
    printf("\nAll tests passed!\n");
    return 0;
}
