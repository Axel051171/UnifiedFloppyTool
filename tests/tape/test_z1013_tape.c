/**
 * @file test_z1013_tape.c
 * @brief Tests for Z1013 Tape Format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/tape/uft_z1013_tape.h"

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
 * Structure Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_header_size(void) {
    return sizeof(uft_z1013_header_t) == 32;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Checksum Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_checksum_xor(void) {
    uint8_t data[] = {0x01, 0x02, 0x04, 0x08};
    uint8_t sum = uft_z1013_calc_checksum(data, 4);
    return sum == 0x0F;  /* XOR of all */
}

static int test_checksum_verify(void) {
    uft_z1013_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    
    hdr.type = UFT_Z1013_TYPE_HEADERSAVE;
    hdr.start_addr = 0x0100;
    hdr.end_addr = 0x1000;
    hdr.exec_addr = 0x0100;
    memcpy(hdr.filename, "TEST            ", 16);
    
    /* Calculate correct checksum */
    hdr.checksum = uft_z1013_calc_checksum((uint8_t*)&hdr, 31);
    
    return uft_z1013_verify_header(&hdr);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Filename Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_filename_extract(void) {
    uft_z1013_header_t hdr;
    memset(&hdr, ' ', sizeof(hdr));
    memcpy(hdr.filename, "HELLO           ", 16);
    
    char filename[17];
    uft_z1013_get_filename(&hdr, filename, sizeof(filename));
    
    return strcmp(filename, "HELLO") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Headersave Detection Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_is_headersave(void) {
    uint8_t data[64];
    memset(data, 0, sizeof(data));
    
    uft_z1013_header_t* hdr = (uft_z1013_header_t*)data;
    hdr->type = UFT_Z1013_TYPE_HEADERSAVE;
    hdr->start_addr = 0x0100;
    hdr->end_addr = 0x2000;
    
    return uft_z1013_is_headersave(data, sizeof(data));
}

static int test_is_headersave_compressed(void) {
    uint8_t data[64];
    memset(data, 0, sizeof(data));
    
    uft_z1013_header_t* hdr = (uft_z1013_header_t*)data;
    hdr->type = UFT_Z1013_TYPE_HEADERSAVEZ;
    hdr->start_addr = 0x0100;
    hdr->end_addr = 0x2000;
    
    return uft_z1013_is_headersave(data, sizeof(data));
}

static int test_not_headersave(void) {
    uint8_t data[64];
    memset(data, 0xFF, sizeof(data));
    
    return !uft_z1013_is_headersave(data, sizeof(data));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * File Type Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_file_type_names(void) {
    return strcmp(uft_z1013_file_type_name(UFT_Z1013_FILE_Z13), "Z13 (Z1013 Generic)") == 0 &&
           strcmp(uft_z1013_file_type_name(UFT_Z1013_FILE_BAS), "BAS (BASIC)") == 0;
}

static int test_detect_type_ext(void) {
    return uft_z1013_detect_type_ext("Z13") == UFT_Z1013_FILE_Z13 &&
           uft_z1013_detect_type_ext("z80") == UFT_Z1013_FILE_Z80 &&
           uft_z1013_detect_type_ext("BAS") == UFT_Z1013_FILE_BAS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Parse Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_parse_header(void) {
    uint8_t data[64];
    memset(data, 0, sizeof(data));
    
    uft_z1013_header_t* hdr = (uft_z1013_header_t*)data;
    hdr->type = UFT_Z1013_TYPE_HEADERSAVE;
    hdr->start_addr = 0x0100;
    hdr->end_addr = 0x1FFF;
    hdr->exec_addr = 0x0100;
    memcpy(hdr->filename, "GAME            ", 16);
    
    uft_z1013_file_info_t info;
    bool ok = uft_z1013_parse_header(data, sizeof(data), &info);
    
    return ok &&
           info.start_addr == 0x0100 &&
           info.end_addr == 0x1FFF &&
           info.data_size == 0x1F00 &&
           strcmp(info.filename, "GAME") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Probe Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_probe_headersave(void) {
    uint8_t data[64];
    memset(data, 0, sizeof(data));
    
    uft_z1013_header_t* hdr = (uft_z1013_header_t*)data;
    hdr->type = UFT_Z1013_TYPE_HEADERSAVE;
    hdr->start_addr = 0x0100;
    hdr->end_addr = 0x2000;
    memcpy(hdr->filename, "TEST            ", 16);
    hdr->checksum = uft_z1013_calc_checksum(data, 31);
    
    int score = uft_z1013_tape_probe(data, sizeof(data));
    return score >= 70;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Timing Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_timing_init(void) {
    uft_z1013_tape_timing_t timing;
    uft_z1013_init_timing(&timing, 44100, UFT_Z1013_BAUD_STANDARD);
    
    return timing.sample_rate == 44100 &&
           timing.baud_rate == 1000 &&
           timing.samples_per_bit0 == 36 &&  /* 44100 / 1200 */
           timing.samples_per_bit1 == 18;    /* 44100 / 2400 */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== Z1013 Tape Format Tests ===\n\n");
    
    printf("[Structure]\n");
    TEST(header_size);
    
    printf("\n[Checksum]\n");
    TEST(checksum_xor);
    TEST(checksum_verify);
    
    printf("\n[Filename]\n");
    TEST(filename_extract);
    
    printf("\n[Headersave Detection]\n");
    TEST(is_headersave);
    TEST(is_headersave_compressed);
    TEST(not_headersave);
    
    printf("\n[File Types]\n");
    TEST(file_type_names);
    TEST(detect_type_ext);
    
    printf("\n[Parse]\n");
    TEST(parse_header);
    
    printf("\n[Probe]\n");
    TEST(probe_headersave);
    
    printf("\n[Timing]\n");
    TEST(timing_init);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
