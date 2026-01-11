/**
 * @file test_zx_tape.c
 * @brief Tests for TZX and TAP Tape Formats (ZX Spectrum)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/tape/uft_tzx_format.h"
#include "uft/tape/uft_tap_format.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  Testing: %s... ", #name); \
    tests_run++; \
    if (test_##name()) { \
        printf("PASS\n"); \
        tests_passed++; \
    } else { \
        printf("FAIL\n"); \
    } \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Structure Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tzx_header_size(void) {
    return sizeof(uft_tzx_header_t) == 10;
}

static int test_tzx_spectrum_header_size(void) {
    return sizeof(uft_tzx_spectrum_header_t) == 19;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Signature Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tzx_verify_signature(void) {
    uint8_t valid[] = { 'Z', 'X', 'T', 'a', 'p', 'e', '!', 0x1A, 1, 20 };
    uint8_t invalid[] = { 'B', 'A', 'D', 'H', 'E', 'A', 'D', 0x00, 0, 0 };
    
    return uft_tzx_verify_signature(valid, sizeof(valid)) &&
           !uft_tzx_verify_signature(invalid, sizeof(invalid));
}

static int test_tzx_probe_valid(void) {
    uint8_t data[32];
    memset(data, 0, sizeof(data));
    
    /* TZX header */
    memcpy(data, "ZXTape!", 7);
    data[7] = 0x1A;
    data[8] = 1;
    data[9] = 20;
    
    /* First block: 0x10 Standard Speed */
    data[10] = 0x10;
    
    int score = uft_tzx_probe(data, sizeof(data));
    return score >= 80;
}

static int test_tzx_probe_invalid(void) {
    uint8_t data[32];
    memset(data, 0xFF, sizeof(data));
    
    int score = uft_tzx_probe(data, sizeof(data));
    return score == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Block Name Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tzx_block_names(void) {
    return strcmp(uft_tzx_block_name(UFT_TZX_BLOCK_STD_SPEED), "Standard Speed Data") == 0 &&
           strcmp(uft_tzx_block_name(UFT_TZX_BLOCK_TURBO_SPEED), "Turbo Speed Data") == 0 &&
           strcmp(uft_tzx_block_name(UFT_TZX_BLOCK_PAUSE), "Pause/Stop") == 0;
}

static int test_tzx_header_type_names(void) {
    return strcmp(uft_tzx_header_type_name(UFT_TZX_HDR_PROGRAM), "Program") == 0 &&
           strcmp(uft_tzx_header_type_name(UFT_TZX_HDR_CODE), "Bytes") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Block Size Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tzx_block_size_std(void) {
    /* 0x10 block: 1 + 2 + 2 + data_len */
    uint8_t block[] = { 0x10, 0xE8, 0x03, 0x13, 0x00 };  /* pause=1000, len=19 */
    
    int32_t size = uft_tzx_block_size(block);
    return size == 24;  /* 5 header + 19 data */
}

static int test_tzx_block_size_pause(void) {
    uint8_t block[] = { 0x20, 0xE8, 0x03 };  /* pause=1000ms */
    
    int32_t size = uft_tzx_block_size(block);
    return size == 3;
}

static int test_tzx_block_size_tone(void) {
    uint8_t block[] = { 0x12, 0x78, 0x08, 0x87, 0x1F };  /* len=2168, count=8071 */
    
    int32_t size = uft_tzx_block_size(block);
    return size == 5;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Timing Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tzx_tstates_to_us(void) {
    /* 3500 T-states = 1000 microseconds (at 3.5 MHz) */
    float us = uft_tzx_tstates_to_us(3500);
    return us > 999.0f && us < 1001.0f;
}

static int test_tzx_tstates_to_samples(void) {
    /* At 44100 Hz, 3500 T-states = 44.1 samples */
    uint32_t samples = uft_tzx_tstates_to_samples(3500, 44100);
    return samples == 44;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Checksum Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tzx_checksum(void) {
    uint8_t data[] = { 0x00, 0x03, 'A', 'B', 'C' };  /* flag + type + ABC */
    uint8_t sum = uft_tzx_calc_checksum(data, 5);
    
    /* XOR: 0x00 ^ 0x03 ^ 'A' ^ 'B' ^ 'C' = 0x03 ^ 0x41 ^ 0x42 ^ 0x43 */
    return sum == (0x03 ^ 0x41 ^ 0x42 ^ 0x43);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TAP Structure Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tap_spectrum_header_size(void) {
    return sizeof(uft_tap_spectrum_header_t) == 17;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TAP Probe Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tap_probe_valid(void) {
    /* Create valid TAP header block */
    uint8_t data[32];
    memset(data, 0, sizeof(data));
    
    /* Length = 19 */
    data[0] = 19;
    data[1] = 0;
    
    /* Flag = header */
    data[2] = 0x00;
    
    /* Type = Program */
    data[3] = 0x00;
    
    /* Filename (spaces) */
    memset(data + 4, ' ', 10);
    memcpy(data + 4, "TEST", 4);
    
    /* Data length */
    data[14] = 0x10; data[15] = 0x00;  /* 16 bytes */
    
    /* Params */
    data[16] = 0x0A; data[17] = 0x00;  /* Line 10 */
    data[18] = 0x10; data[19] = 0x00;
    
    /* Checksum */
    data[20] = uft_tap_calc_checksum(data + 2, 18);
    
    int score = uft_tap_probe(data, 21);
    return score >= 80;
}

static int test_tap_probe_invalid(void) {
    uint8_t data[32];
    memset(data, 0xFF, sizeof(data));
    
    int score = uft_tap_probe(data, sizeof(data));
    return score < 50;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TAP Checksum Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tap_checksum(void) {
    uint8_t data[] = { 0xFF, 0x01, 0x02, 0x03 };
    uint8_t sum = uft_tap_calc_checksum(data, 4);
    return sum == (0xFF ^ 0x01 ^ 0x02 ^ 0x03);
}

static int test_tap_verify_block(void) {
    uint8_t data[6];
    data[0] = 0xFF;  /* Flag */
    data[1] = 0x01;
    data[2] = 0x02;
    data[3] = 0x03;
    data[4] = uft_tap_calc_checksum(data, 4);  /* Correct checksum */
    
    return uft_tap_verify_block(data, 5);
}

static int test_tap_verify_block_bad(void) {
    uint8_t data[6];
    data[0] = 0xFF;
    data[1] = 0x01;
    data[2] = 0x02;
    data[3] = 0x03;
    data[4] = 0x00;  /* Wrong checksum */
    
    return !uft_tap_verify_block(data, 5);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TAP Create Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tap_create_header(void) {
    uint8_t buffer[32];
    
    size_t len = uft_tap_create_header(buffer, sizeof(buffer),
                                        UFT_TAP_HDR_CODE, "SCREEN",
                                        6912, 16384, 0);
    
    if (len != 21) return 0;
    
    /* Verify length field */
    if (buffer[0] != 19 || buffer[1] != 0) return 0;
    
    /* Verify flag */
    if (buffer[2] != 0x00) return 0;
    
    /* Verify type */
    if (buffer[3] != UFT_TAP_HDR_CODE) return 0;
    
    /* Verify checksum */
    return uft_tap_verify_block(buffer + 2, 19);
}

static int test_tap_create_data(void) {
    uint8_t buffer[32];
    uint8_t data[] = { 0x01, 0x02, 0x03, 0x04 };
    
    size_t len = uft_tap_create_data(buffer, sizeof(buffer), data, 4);
    
    if (len != 8) return 0;  /* 2 len + 1 flag + 4 data + 1 checksum */
    
    /* Verify length field */
    if (buffer[0] != 6 || buffer[1] != 0) return 0;  /* flag + data + checksum */
    
    /* Verify flag */
    if (buffer[2] != 0xFF) return 0;
    
    /* Verify checksum */
    return uft_tap_verify_block(buffer + 2, 6);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TAP Parse Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tap_parse_file(void) {
    /* Create a minimal TAP file with header + data */
    uint8_t buffer[64];
    size_t offset = 0;
    
    /* Header block */
    offset += uft_tap_create_header(buffer + offset, sizeof(buffer) - offset,
                                     UFT_TAP_HDR_CODE, "TEST", 4, 32768, 0);
    
    /* Data block */
    uint8_t data[] = { 0x01, 0x02, 0x03, 0x04 };
    offset += uft_tap_create_data(buffer + offset, sizeof(buffer) - offset, data, 4);
    
    uft_tap_file_info_t info;
    bool ok = uft_tap_parse_file(buffer, offset, &info);
    
    return ok &&
           info.block_count == 2 &&
           info.header_count == 1 &&
           info.data_count == 1 &&
           info.all_checksums_ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== ZX Spectrum Tape Format Tests ===\n\n");
    
    printf("[TZX Structure]\n");
    TEST(tzx_header_size);
    TEST(tzx_spectrum_header_size);
    
    printf("\n[TZX Signature]\n");
    TEST(tzx_verify_signature);
    TEST(tzx_probe_valid);
    TEST(tzx_probe_invalid);
    
    printf("\n[TZX Block Names]\n");
    TEST(tzx_block_names);
    TEST(tzx_header_type_names);
    
    printf("\n[TZX Block Size]\n");
    TEST(tzx_block_size_std);
    TEST(tzx_block_size_pause);
    TEST(tzx_block_size_tone);
    
    printf("\n[TZX Timing]\n");
    TEST(tzx_tstates_to_us);
    TEST(tzx_tstates_to_samples);
    
    printf("\n[TZX Checksum]\n");
    TEST(tzx_checksum);
    
    printf("\n[TAP Structure]\n");
    TEST(tap_spectrum_header_size);
    
    printf("\n[TAP Probe]\n");
    TEST(tap_probe_valid);
    TEST(tap_probe_invalid);
    
    printf("\n[TAP Checksum]\n");
    TEST(tap_checksum);
    TEST(tap_verify_block);
    TEST(tap_verify_block_bad);
    
    printf("\n[TAP Create]\n");
    TEST(tap_create_header);
    TEST(tap_create_data);
    
    printf("\n[TAP Parse]\n");
    TEST(tap_parse_file);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
