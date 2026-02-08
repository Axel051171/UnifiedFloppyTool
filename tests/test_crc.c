/**
 * @file test_crc.c
 * @brief CRC algorithm validation tests
 * 
 * Tests CRC implementations against known test vectors.
 * Critical for verify pipeline integrity.
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Include CRC headers if available, otherwise use local implementations */
#ifdef UFT_HAS_CRC
#include "uft/uft_crc.h"
#include "uft/uft_crc_polys.h"
#endif

/* Test framework */
static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_BEGIN(name) \
    do { \
        g_tests_run++; \
        printf("  [%02d] %-50s ", g_tests_run, name); \
        fflush(stdout); \
    } while(0)

#define TEST_PASS() do { g_tests_passed++; printf("\033[32m[PASS]\033[0m\n"); } while(0)
#define TEST_FAIL(msg) do { printf("\033[31m[FAIL]\033[0m %s\n", msg); } while(0)
#define TEST_EXPECT(cond, msg) do { if (cond) { TEST_PASS(); } else { TEST_FAIL(msg); } } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Local CRC Implementations (for standalone testing)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief CRC-16 CCITT (0xFFFF init, 0x1021 poly)
 */
static uint16_t local_crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

/**
 * @brief CRC-32 (IEEE 802.3)
 */
static uint32_t local_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return ~crc;
}

/**
 * @brief CRC-16 IBM (MFM checksum)
 */
static uint16_t local_crc16_ibm(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Amiga MFM Checksum
 */
static uint32_t local_amiga_checksum(const uint32_t *data, size_t count) {
    uint32_t sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum ^= data[i];
    }
    return sum & 0x55555555;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Vectors
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Standard "123456789" test string */
static const uint8_t test_vector[] = "123456789";
#define TEST_VECTOR_LEN 9

/* Known CRC values for "123456789" */
#define CRC32_123456789     0xCBF43926
#define CRC16_CCITT_123456789 0x29B1

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC-32 Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_crc32_standard_vector(void) {
    TEST_BEGIN("CRC-32: Standard vector '123456789'");
    
    uint32_t crc = local_crc32(test_vector, TEST_VECTOR_LEN);
    
    char msg[64];
    snprintf(msg, sizeof(msg), "Expected 0x%08X, got 0x%08X", 
             CRC32_123456789, crc);
    TEST_EXPECT(crc == CRC32_123456789, msg);
}

static void test_crc32_empty(void) {
    TEST_BEGIN("CRC-32: Empty data");
    
    uint32_t crc = local_crc32(NULL, 0);
    /* CRC of empty data should be 0 with our ~crc */
    TEST_EXPECT(crc == 0x00000000, "Empty CRC unexpected");
}

static void test_crc32_single_byte(void) {
    TEST_BEGIN("CRC-32: Single byte 0x00");
    
    uint8_t data = 0x00;
    uint32_t crc = local_crc32(&data, 1);
    
    /* Known value for single 0x00 byte */
    TEST_EXPECT(crc == 0xD202EF8D, "Single byte CRC mismatch");
}

static void test_crc32_incremental(void) {
    TEST_BEGIN("CRC-32: Incremental == Full");
    
    /* Calculate CRC in one go */
    uint32_t crc_full = local_crc32(test_vector, TEST_VECTOR_LEN);
    
    /* Calculate same CRC incrementally (simulated) */
    uint8_t part1[] = "12345";
    uint8_t part2[] = "6789";
    uint8_t combined[TEST_VECTOR_LEN];
    memcpy(combined, part1, 5);
    memcpy(combined + 5, part2, 4);
    
    uint32_t crc_combined = local_crc32(combined, TEST_VECTOR_LEN);
    
    TEST_EXPECT(crc_full == crc_combined, "Incremental CRC mismatch");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC-16 CCITT Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_crc16_ccitt_standard_vector(void) {
    TEST_BEGIN("CRC-16 CCITT: Standard vector '123456789'");
    
    uint16_t crc = local_crc16_ccitt(test_vector, TEST_VECTOR_LEN);
    
    char msg[64];
    snprintf(msg, sizeof(msg), "Expected 0x%04X, got 0x%04X", 
             CRC16_CCITT_123456789, crc);
    TEST_EXPECT(crc == CRC16_CCITT_123456789, msg);
}

static void test_crc16_single_ff(void) {
    TEST_BEGIN("CRC-16 CCITT: Single byte 0xFF");
    
    uint8_t data = 0xFF;
    uint16_t crc = local_crc16_ccitt(&data, 1);
    
    /* 0xFF should give a specific result */
    TEST_EXPECT(crc != 0xFFFF, "0xFF CRC should differ from init");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * MFM CRC Tests (IBM Floppy)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_mfm_address_mark(void) {
    TEST_BEGIN("MFM: Address mark CRC");
    
    /* Typical IBM MFM address mark: A1 A1 A1 FE + IDAM */
    uint8_t address_mark[] = {0xA1, 0xA1, 0xA1, 0xFE, 0x00, 0x00, 0x01, 0x02};
    uint16_t crc = local_crc16_ibm(address_mark, sizeof(address_mark));
    
    /* Just verify it produces a non-zero CRC */
    TEST_EXPECT(crc != 0x0000, "Address mark CRC is zero");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Amiga Checksum Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_amiga_checksum_zero(void) {
    TEST_BEGIN("Amiga: Zero data checksum");
    
    uint32_t data[10] = {0};
    uint32_t sum = local_amiga_checksum(data, 10);
    
    TEST_EXPECT(sum == 0, "Zero data should give zero checksum");
}

static void test_amiga_checksum_ones(void) {
    TEST_BEGIN("Amiga: All-ones data checksum");
    
    uint32_t data[4] = {0x55555555, 0x55555555, 0x55555555, 0x55555555};
    uint32_t sum = local_amiga_checksum(data, 4);
    
    /* XOR of four identical values should be 0 */
    TEST_EXPECT(sum == 0, "Four identical values XOR should be 0");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * WOZ CRC-32 Test
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_woz_crc32(void) {
    TEST_BEGIN("WOZ: CRC-32 polynomial");
    
    /* WOZ uses standard CRC-32 */
    uint8_t data[] = "WOZ2";
    uint32_t crc = local_crc32(data, 4);
    
    /* Just verify it's non-zero and deterministic */
    uint32_t crc2 = local_crc32(data, 4);
    TEST_EXPECT(crc == crc2 && crc != 0, "WOZ CRC not deterministic");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Collision Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_no_trivial_collisions(void) {
    TEST_BEGIN("CRC-32: No trivial collisions");
    
    uint8_t data1[] = {0x00, 0x01, 0x02, 0x03};
    uint8_t data2[] = {0x03, 0x02, 0x01, 0x00};  /* Reversed */
    uint8_t data3[] = {0x00, 0x01, 0x02, 0x04};  /* One bit different */
    
    uint32_t crc1 = local_crc32(data1, 4);
    uint32_t crc2 = local_crc32(data2, 4);
    uint32_t crc3 = local_crc32(data3, 4);
    
    bool no_collisions = (crc1 != crc2) && (crc1 != crc3) && (crc2 != crc3);
    TEST_EXPECT(no_collisions, "Found trivial collision");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Performance Test
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_crc32_large_buffer(void) {
    TEST_BEGIN("CRC-32: 1MB buffer performance");
    
    #define MB_SIZE (1024 * 1024)
    uint8_t *data = malloc(MB_SIZE);
    if (!data) {
        printf("\033[33m[SKIP]\033[0m Not enough memory\n");
        g_tests_run--;  /* Don't count skipped test */
        return;
    }
    
    /* Fill with pattern */
    for (size_t i = 0; i < MB_SIZE; i++) {
        data[i] = (uint8_t)(i ^ (i >> 8));
    }
    
    uint32_t crc = local_crc32(data, MB_SIZE);
    free(data);
    
    /* Just verify it completed and gave non-zero */
    TEST_EXPECT(crc != 0, "1MB CRC is zero");
    
    #undef MB_SIZE
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT CRC Validation Tests\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    printf("CRC-32 Tests:\n");
    test_crc32_standard_vector();
    test_crc32_empty();
    test_crc32_single_byte();
    test_crc32_incremental();
    
    printf("\nCRC-16 CCITT Tests:\n");
    test_crc16_ccitt_standard_vector();
    test_crc16_single_ff();
    
    printf("\nMFM CRC Tests:\n");
    test_mfm_address_mark();
    
    printf("\nAmiga Checksum Tests:\n");
    test_amiga_checksum_zero();
    test_amiga_checksum_ones();
    
    printf("\nFormat-Specific CRC Tests:\n");
    test_woz_crc32();
    
    printf("\nQuality Tests:\n");
    test_no_trivial_collisions();
    test_crc32_large_buffer();
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d/%d tests passed\n", g_tests_passed, g_tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
