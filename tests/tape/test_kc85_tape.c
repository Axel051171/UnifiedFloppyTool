/**
 * @file test_kc85_tape.c
 * @brief Tests for KC85/Z1013 Tape Formats
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/tape/uft_kc85_tape.h"

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
 * Structure Size Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_struct_sizes(void) {
    return sizeof(uft_kc85_tape_packet_t) == 130 &&
           sizeof(uft_kc85_kcc_header_t) == 128 &&
           sizeof(uft_kc85_tape_header_t) == 13;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Checksum Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_checksum_calc(void) {
    uint8_t data[128];
    memset(data, 0x01, sizeof(data));  /* 128 bytes of 0x01 */
    
    uint8_t sum = uft_kc85_calc_checksum(data, sizeof(data));
    return sum == 0x80;  /* 128 * 1 = 128 = 0x80 */
}

static int test_checksum_verify(void) {
    uft_kc85_tape_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    
    pkt.packet_id = 0x01;
    memset(pkt.data, 0x55, 128);
    pkt.checksum = uft_kc85_calc_checksum(pkt.data, 128);
    
    return uft_kc85_verify_packet(&pkt);
}

static int test_checksum_verify_bad(void) {
    uft_kc85_tape_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    
    pkt.packet_id = 0x01;
    memset(pkt.data, 0x55, 128);
    pkt.checksum = 0x00;  /* Wrong checksum */
    
    return !uft_kc85_verify_packet(&pkt);  /* Should fail */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Packet ID Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_packet_id_first(void) {
    return uft_kc85_next_packet_id(0) == UFT_KC85_PACKET_FIRST;
}

static int test_packet_id_increment(void) {
    return uft_kc85_next_packet_id(0x01) == 0x02 &&
           uft_kc85_next_packet_id(0x10) == 0x11;
}

static int test_packet_id_wrap(void) {
    return uft_kc85_next_packet_id(0xFE) == UFT_KC85_PACKET_FIRST;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * File Type Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_file_type_names(void) {
    return strcmp(uft_kc85_file_type_name(UFT_KC85_FILE_KCC), "KCC (Machine Code)") == 0 &&
           strcmp(uft_kc85_file_type_name(UFT_KC85_FILE_KCB), "KCB (HC-BASIC)") == 0;
}

static int test_file_type_ext(void) {
    return strcmp(uft_kc85_file_type_ext(UFT_KC85_FILE_KCC), "KCC") == 0 &&
           strcmp(uft_kc85_file_type_ext(UFT_KC85_FILE_SSS), "SSS") == 0;
}

static int test_detect_type_ext(void) {
    return uft_kc85_detect_type_ext("KCC") == UFT_KC85_FILE_KCC &&
           uft_kc85_detect_type_ext("kcc") == UFT_KC85_FILE_KCC &&
           uft_kc85_detect_type_ext("COM") == UFT_KC85_FILE_KCC &&
           uft_kc85_detect_type_ext("SSS") == UFT_KC85_FILE_SSS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * KCC Header Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_kcc_filename(void) {
    uft_kc85_kcc_header_t hdr;
    memset(&hdr, ' ', sizeof(hdr));  /* Space-fill */
    memcpy(hdr.filename, "TEST    ", 8);
    memcpy(hdr.extension, "KCC", 3);
    
    char filename[16];
    uft_kc85_get_kcc_filename(&hdr, filename, sizeof(filename));
    
    return strcmp(filename, "TEST.KCC") == 0;
}

static int test_kcc_parse(void) {
    uft_kc85_kcc_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    
    memcpy(hdr.filename, "HELLO   ", 8);
    memcpy(hdr.extension, "COM", 3);
    hdr.num_args = 3;
    hdr.start_addr = 0x0300;
    hdr.end_addr = 0x0500;
    hdr.exec_addr = 0x0300;
    
    uint8_t buffer[256];
    memcpy(buffer, &hdr, sizeof(hdr));
    
    uft_kc85_file_info_t info;
    bool ok = uft_kc85_parse_kcc(buffer, 256, &info);
    
    return ok &&
           info.start_addr == 0x0300 &&
           info.end_addr == 0x0500 &&
           info.exec_addr == 0x0300 &&
           info.has_autorun;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tape Header Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tape_filename(void) {
    uft_kc85_tape_header_t hdr;
    
    /* Extension with high bit set */
    hdr.extension[0] = 'S' | 0x80;
    hdr.extension[1] = 'S' | 0x80;
    hdr.extension[2] = 'S' | 0x80;
    memcpy(hdr.filename, "PROGRAM ", 8);
    hdr.length = 1024;
    
    char filename[16];
    uft_kc85_get_tape_filename(&hdr, filename, sizeof(filename));
    
    return strcmp(filename, "PROGRAM.SSS") == 0;
}

static int test_tape_probe_high_bit(void) {
    uint8_t data[32];
    memset(data, 0, sizeof(data));
    
    /* Create tape header with high-bit extension */
    data[0] = 'S' | 0x80;
    data[1] = 'S' | 0x80;
    data[2] = 'S' | 0x80;
    memcpy(data + 3, "TEST    ", 8);
    
    int score = uft_kc85_tape_probe(data, sizeof(data));
    return score >= 30;  /* Should detect tape format */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Packet Calculation Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_calc_packets(void) {
    return uft_kc85_calc_packets(128) == 1 &&
           uft_kc85_calc_packets(129) == 2 &&
           uft_kc85_calc_packets(256) == 2 &&
           uft_kc85_calc_packets(1024) == 8;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Timing Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_timing_44100(void) {
    uft_kc85_tape_timing_t timing;
    uft_kc85_init_timing(&timing, 44100);
    
    /* At 44100 Hz:
     * 2400 Hz -> ~18 samples per wave
     * 1200 Hz -> ~36 samples per wave
     * 600 Hz  -> ~73 samples per wave
     */
    return timing.sample_rate == 44100 &&
           timing.samples_per_bit0 == 18 &&
           timing.samples_per_bit1 == 36 &&
           timing.samples_per_stop == 73;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== KC85 Tape Format Tests ===\n\n");
    
    printf("[Structure Sizes]\n");
    TEST(struct_sizes);
    
    printf("\n[Checksum]\n");
    TEST(checksum_calc);
    TEST(checksum_verify);
    TEST(checksum_verify_bad);
    
    printf("\n[Packet IDs]\n");
    TEST(packet_id_first);
    TEST(packet_id_increment);
    TEST(packet_id_wrap);
    
    printf("\n[File Types]\n");
    TEST(file_type_names);
    TEST(file_type_ext);
    TEST(detect_type_ext);
    
    printf("\n[KCC Header]\n");
    TEST(kcc_filename);
    TEST(kcc_parse);
    
    printf("\n[Tape Header]\n");
    TEST(tape_filename);
    TEST(tape_probe_high_bit);
    
    printf("\n[Packet Calculation]\n");
    TEST(calc_packets);
    
    printf("\n[Timing]\n");
    TEST(timing_44100);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
