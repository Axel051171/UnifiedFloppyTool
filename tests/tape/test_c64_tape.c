/**
 * @file test_c64_tape.c
 * @brief Tests for C64 TAP and T64 Tape Formats
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "uft/tape/uft_c64_tap.h"
#include "uft/tape/uft_c64_t64.h"

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
 * C64 TAP Structure Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tap_header_size(void) {
    return sizeof(uft_c64_tap_header_t) == 20;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * C64 TAP Signature Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tap_verify_signature(void) {
    uint8_t valid[20];
    memcpy(valid, "C64-TAPE-RAW", 12);
    valid[12] = 1;  /* version */
    valid[13] = 0;  /* machine */
    valid[14] = 0;  /* video */
    valid[15] = 0;  /* reserved */
    /* data_size = 0 */
    valid[16] = valid[17] = valid[18] = valid[19] = 0;
    
    uint8_t invalid[20] = {0};
    memcpy(invalid, "NOT-A-TAP", 9);
    
    return uft_c64_tap_verify_signature(valid, 20) &&
           !uft_c64_tap_verify_signature(invalid, 20);
}

static int test_tap_probe_valid(void) {
    uint8_t data[32];
    memset(data, 0, sizeof(data));
    
    memcpy(data, "C64-TAPE-RAW", 12);
    data[12] = 1;   /* version 1 */
    data[13] = 0;   /* C64 */
    data[14] = 0;   /* PAL */
    
    int score = uft_c64_tap_probe(data, sizeof(data));
    return score >= 80;
}

static int test_tap_probe_invalid(void) {
    uint8_t data[32];
    memset(data, 0xFF, sizeof(data));
    
    int score = uft_c64_tap_probe(data, sizeof(data));
    return score == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * C64 TAP Timing Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tap_to_us(void) {
    /* TAP value 0x2B (43) @ PAL: 43 * 8 / 985248 * 1e6 ≈ 349 µs */
    float us = uft_c64_tap_to_us(0x2B, UFT_C64_CLOCK_PAL);
    return us > 340.0f && us < 360.0f;
}

static int test_us_to_tap(void) {
    /* 350 µs @ PAL -> TAP value ~43 */
    uint32_t tap = uft_c64_us_to_tap(350.0f, UFT_C64_CLOCK_PAL);
    return tap >= 42 && tap <= 44;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * C64 TAP Pulse Classification Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_classify_pulse_short(void) {
    return uft_c64_classify_pulse(0x2B) == UFT_C64_PULSE_TYPE_SHORT;
}

static int test_classify_pulse_medium(void) {
    return uft_c64_classify_pulse(0x3F) == UFT_C64_PULSE_TYPE_MEDIUM;
}

static int test_classify_pulse_long(void) {
    return uft_c64_classify_pulse(0x53) == UFT_C64_PULSE_TYPE_LONG;
}

static int test_classify_pulse_unknown(void) {
    return uft_c64_classify_pulse(0x10) == UFT_C64_PULSE_TYPE_UNKNOWN &&
           uft_c64_classify_pulse(0x80) == UFT_C64_PULSE_TYPE_UNKNOWN;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * C64 TAP Name Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tap_machine_names(void) {
    return strcmp(uft_c64_tap_machine_name(UFT_C64_MACHINE_C64), "C64") == 0 &&
           strcmp(uft_c64_tap_machine_name(UFT_C64_MACHINE_VIC20), "VIC-20") == 0;
}

static int test_tap_video_names(void) {
    return strcmp(uft_c64_tap_video_name(UFT_C64_VIDEO_PAL), "PAL") == 0 &&
           strcmp(uft_c64_tap_video_name(UFT_C64_VIDEO_NTSC), "NTSC") == 0;
}

static int test_tap_pulse_type_names(void) {
    return strcmp(uft_c64_pulse_type_name(UFT_C64_PULSE_TYPE_SHORT), "Short") == 0 &&
           strcmp(uft_c64_pulse_type_name(UFT_C64_PULSE_TYPE_MEDIUM), "Medium") == 0 &&
           strcmp(uft_c64_pulse_type_name(UFT_C64_PULSE_TYPE_LONG), "Long") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * C64 TAP Clock Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tap_get_clock(void) {
    return uft_c64_tap_get_clock(UFT_C64_MACHINE_C64, UFT_C64_VIDEO_PAL) == UFT_C64_CLOCK_PAL &&
           uft_c64_tap_get_clock(UFT_C64_MACHINE_C64, UFT_C64_VIDEO_NTSC) == UFT_C64_CLOCK_NTSC;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * C64 TAP Parse Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tap_parse_header(void) {
    uint8_t data[32];
    memset(data, 0, sizeof(data));
    
    memcpy(data, "C64-TAPE-RAW", 12);
    data[12] = 1;   /* version */
    data[13] = 0;   /* machine = C64 */
    data[14] = 0;   /* video = PAL */
    data[16] = 100; /* data_size = 100 */
    
    uft_c64_tap_info_t info;
    bool ok = uft_c64_tap_parse_header(data, sizeof(data), &info);
    
    return ok &&
           info.version == 1 &&
           info.machine == UFT_C64_MACHINE_C64 &&
           info.video == UFT_C64_VIDEO_PAL &&
           info.data_size == 100 &&
           info.clock_hz == UFT_C64_CLOCK_PAL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * C64 TAP Create Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_tap_create_header(void) {
    uft_c64_tap_header_t hdr;
    
    uft_c64_tap_create_header(&hdr, 1, UFT_C64_MACHINE_C64, UFT_C64_VIDEO_PAL, 1000);
    
    return memcmp(hdr.signature, "C64-TAPE-RAW", 12) == 0 &&
           hdr.version == 1 &&
           hdr.machine == UFT_C64_MACHINE_C64 &&
           hdr.video == UFT_C64_VIDEO_PAL &&
           hdr.data_size == 1000;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * T64 Structure Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_t64_header_size(void) {
    return sizeof(uft_t64_header_t) == 64;
}

static int test_t64_entry_size(void) {
    return sizeof(uft_t64_entry_t) == 32;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * T64 Signature Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_t64_verify_signature(void) {
    uint8_t valid1[64];
    memset(valid1, 0, sizeof(valid1));
    memcpy(valid1, "C64 tape image file", 19);
    
    uint8_t valid2[64];
    memset(valid2, 0, sizeof(valid2));
    memcpy(valid2, "C64S tape image file", 20);
    
    uint8_t invalid[64];
    memset(invalid, 0, sizeof(invalid));
    memcpy(invalid, "NOT A T64", 9);
    
    return uft_t64_verify_signature(valid1, 64) &&
           uft_t64_verify_signature(valid2, 64) &&
           !uft_t64_verify_signature(invalid, 64);
}

static int test_t64_probe_valid(void) {
    uint8_t data[128];
    memset(data, 0, sizeof(data));
    
    /* Header */
    memcpy(data, "C64 tape image file", 19);
    
    /* version = 0x0100 */
    data[32] = 0x00;
    data[33] = 0x01;
    
    /* max_entries = 10 */
    data[34] = 10;
    data[35] = 0;
    
    /* used_entries = 1 */
    data[36] = 1;
    data[37] = 0;
    
    /* First entry at offset 64 */
    data[64] = UFT_T64_TYPE_NORMAL;  /* entry_type */
    data[65] = UFT_T64_FTYPE_PRG;    /* file_type */
    data[66] = 0x01; data[67] = 0x08; /* start_addr = 0x0801 */
    data[68] = 0x00; data[69] = 0x10; /* end_addr = 0x1000 */
    
    int score = uft_t64_probe(data, sizeof(data));
    return score >= 80;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * T64 Type Name Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_t64_entry_type_names(void) {
    return strcmp(uft_t64_entry_type_name(UFT_T64_TYPE_NORMAL), "Normal") == 0 &&
           strcmp(uft_t64_entry_type_name(UFT_T64_TYPE_SNAPSHOT), "Snapshot") == 0;
}

static int test_t64_file_type_names(void) {
    return strcmp(uft_t64_file_type_name(UFT_T64_FTYPE_PRG), "PRG") == 0 &&
           strcmp(uft_t64_file_type_name(UFT_T64_FTYPE_SEQ), "SEQ") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * T64 PETSCII Conversion Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_petscii_to_ascii(void) {
    char petscii[] = "HELLO   ";  /* PETSCII uppercase */
    char ascii[16];
    
    uft_t64_petscii_to_ascii(petscii, 8, ascii, sizeof(ascii));
    
    return strcmp(ascii, "HELLO") == 0;
}

static int test_petscii_trim_spaces(void) {
    char petscii[] = "TEST            ";
    char ascii[20];
    
    uft_t64_petscii_to_ascii(petscii, 16, ascii, sizeof(ascii));
    
    return strcmp(ascii, "TEST") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * T64 Parse Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_t64_parse_header(void) {
    uint8_t data[128];
    memset(data, 0, sizeof(data));
    
    memcpy(data, "C64 tape image file", 19);
    data[32] = 0x01; data[33] = 0x01;  /* version 0x0101 */
    data[34] = 5; data[35] = 0;        /* max_entries = 5 */
    data[36] = 2; data[37] = 0;        /* used_entries = 2 */
    memcpy(data + 40, "MY TAPE", 7);   /* tape_name (rest is zeros) */
    
    uft_t64_info_t info;
    bool ok = uft_t64_parse_header(data, sizeof(data), &info);
    
    return ok &&
           info.version == 0x0101 &&
           info.max_entries == 5 &&
           info.used_entries == 2 &&
           strcmp(info.tape_name, "MY TAPE") == 0;
}

static int test_t64_parse_entry(void) {
    uint8_t data[128];
    memset(data, 0, sizeof(data));
    
    /* Header */
    memcpy(data, "C64 tape image file", 19);
    data[34] = 1; /* max_entries */
    
    /* Entry at offset 64 */
    data[64] = UFT_T64_TYPE_NORMAL;
    data[65] = UFT_T64_FTYPE_PRG;
    data[66] = 0x01; data[67] = 0x08;  /* start = 0x0801 */
    data[68] = 0x01; data[69] = 0x10;  /* end = 0x1001 */
    /* data_offset at bytes 72-75 */
    data[72] = 0x60; /* offset = 96 */
    memcpy(data + 80, "PROGRAM         ", 16);
    
    uft_t64_file_info_t file;
    bool ok = uft_t64_parse_entry(data, sizeof(data), 0, &file);
    
    return ok &&
           file.entry_type == UFT_T64_TYPE_NORMAL &&
           file.file_type == UFT_T64_FTYPE_PRG &&
           file.start_addr == 0x0801 &&
           file.end_addr == 0x1001 &&
           file.data_size == 0x0800 &&
           strcmp(file.filename, "PROGRAM") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== C64 Tape Format Tests ===\n\n");
    
    printf("[C64 TAP Structure]\n");
    TEST(tap_header_size);
    
    printf("\n[C64 TAP Signature]\n");
    TEST(tap_verify_signature);
    TEST(tap_probe_valid);
    TEST(tap_probe_invalid);
    
    printf("\n[C64 TAP Timing]\n");
    TEST(tap_to_us);
    TEST(us_to_tap);
    
    printf("\n[C64 TAP Pulse Classification]\n");
    TEST(classify_pulse_short);
    TEST(classify_pulse_medium);
    TEST(classify_pulse_long);
    TEST(classify_pulse_unknown);
    
    printf("\n[C64 TAP Names]\n");
    TEST(tap_machine_names);
    TEST(tap_video_names);
    TEST(tap_pulse_type_names);
    
    printf("\n[C64 TAP Clock]\n");
    TEST(tap_get_clock);
    
    printf("\n[C64 TAP Parse]\n");
    TEST(tap_parse_header);
    
    printf("\n[C64 TAP Create]\n");
    TEST(tap_create_header);
    
    printf("\n[T64 Structure]\n");
    TEST(t64_header_size);
    TEST(t64_entry_size);
    
    printf("\n[T64 Signature]\n");
    TEST(t64_verify_signature);
    TEST(t64_probe_valid);
    
    printf("\n[T64 Type Names]\n");
    TEST(t64_entry_type_names);
    TEST(t64_file_type_names);
    
    printf("\n[T64 PETSCII]\n");
    TEST(petscii_to_ascii);
    TEST(petscii_trim_spaces);
    
    printf("\n[T64 Parse]\n");
    TEST(t64_parse_header);
    TEST(t64_parse_entry);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
