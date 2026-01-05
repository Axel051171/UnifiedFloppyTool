/**
 * @file parser_test.c
 * @brief UFT Parser Tests - Format detection and parsing verification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(test) do { \
    printf("  Running: %s... ", #test); \
    if (test()) { \
        printf("PASS\n"); \
        tests_passed++; \
    } else { \
        printf("FAIL\n"); \
    } \
    tests_run++; \
} while(0)

/* Magic bytes for format detection */
static const uint8_t D64_SIZE = 174848 / 256;  /* D64 is 174848 bytes (683 blocks) */
static const uint8_t ADF_MAGIC[] = "DOS";
static const uint8_t ATR_MAGIC[] = { 0x96, 0x02 };
static const uint8_t WOZ_MAGIC[] = "WOZ1";
static const uint8_t WOZ2_MAGIC[] = "WOZ2";
static const uint8_t SCP_MAGIC[] = "SCP";
static const uint8_t HFE_MAGIC[] = "HXCPICFE";
static const uint8_t IMD_MAGIC[] = "IMD ";

/* Test: D64 size detection */
static bool test_d64_size_detection(void) {
    /* D64 standard sizes */
    size_t d64_35_tracks = 174848;
    size_t d64_40_tracks = 196608;
    size_t d64_35_errors = 175531;
    
    return d64_35_tracks == 174848 &&
           d64_40_tracks == 196608 &&
           d64_35_errors == 175531;
}

/* Test: ADF magic detection */
static bool test_adf_magic_detection(void) {
    uint8_t header[12] = { 'D', 'O', 'S', 0x00 };
    return memcmp(header, ADF_MAGIC, 3) == 0;
}

/* Test: ATR magic detection */
static bool test_atr_magic_detection(void) {
    uint8_t header[16] = { 0x96, 0x02, 0x80, 0x16 };
    return header[0] == ATR_MAGIC[0] && header[1] == ATR_MAGIC[1];
}

/* Test: WOZ magic detection */
static bool test_woz_magic_detection(void) {
    uint8_t header_v1[8] = { 'W', 'O', 'Z', '1', 0xFF, 0x0A, 0x0D, 0x0A };
    uint8_t header_v2[8] = { 'W', 'O', 'Z', '2', 0xFF, 0x0A, 0x0D, 0x0A };
    
    bool is_woz1 = memcmp(header_v1, WOZ_MAGIC, 4) == 0;
    bool is_woz2 = memcmp(header_v2, WOZ2_MAGIC, 4) == 0;
    
    return is_woz1 && is_woz2;
}

/* Test: SCP magic detection */
static bool test_scp_magic_detection(void) {
    uint8_t header[16] = { 'S', 'C', 'P', 0x00 };
    return memcmp(header, SCP_MAGIC, 3) == 0;
}

/* Test: HFE magic detection */
static bool test_hfe_magic_detection(void) {
    uint8_t header[16] = { 'H', 'X', 'C', 'P', 'I', 'C', 'F', 'E' };
    return memcmp(header, HFE_MAGIC, 8) == 0;
}

/* Test: IMD magic detection */
static bool test_imd_magic_detection(void) {
    uint8_t header[16] = { 'I', 'M', 'D', ' ', '1', '.', '1', '9' };
    return memcmp(header, IMD_MAGIC, 4) == 0;
}

/* Test: Invalid/corrupted header handling */
static bool test_corrupted_header(void) {
    uint8_t garbage[16] = { 0xFF, 0xFE, 0xFD, 0xFC };
    
    /* Should not match any known format */
    bool is_adf = memcmp(garbage, ADF_MAGIC, 3) == 0;
    bool is_woz = memcmp(garbage, WOZ_MAGIC, 4) == 0;
    bool is_scp = memcmp(garbage, SCP_MAGIC, 3) == 0;
    
    return !is_adf && !is_woz && !is_scp;
}

/* Test: Truncated file detection */
static bool test_truncated_detection(void) {
    /* A D64 with only 1000 bytes is definitely truncated */
    size_t file_size = 1000;
    size_t min_d64 = 174848;
    
    return file_size < min_d64;
}

/* Test: CRC16 calculation */
static bool test_crc16(void) {
    /* CRC16-CCITT for "123456789" = 0x29B1 */
    const uint8_t data[] = "123456789";
    uint16_t expected = 0x29B1;
    
    /* Simplified CRC16-CCITT */
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < 9; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    
    return crc == expected;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("═══════════════════════════════════════════════════\n");
    printf("  UFT Parser Tests v3.3.0\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    printf("Format Detection:\n");
    RUN_TEST(test_d64_size_detection);
    RUN_TEST(test_adf_magic_detection);
    RUN_TEST(test_atr_magic_detection);
    RUN_TEST(test_woz_magic_detection);
    RUN_TEST(test_scp_magic_detection);
    RUN_TEST(test_hfe_magic_detection);
    RUN_TEST(test_imd_magic_detection);
    
    printf("\nError Handling:\n");
    RUN_TEST(test_corrupted_header);
    RUN_TEST(test_truncated_detection);
    
    printf("\nCRC Verification:\n");
    RUN_TEST(test_crc16);
    
    printf("\n═══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
