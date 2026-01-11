/**
 * @file test_pzx_csw.c
 * @brief Tests for PZX and CSW Tape Formats
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/tape/uft_pzx_format.h"
#include "uft/tape/uft_csw_format.h"

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
 * PZX Structure Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_pzx_header_size(void) {
    return sizeof(uft_pzx_header_t) == 8;
}

static int test_pzx_block_header_size(void) {
    return sizeof(uft_pzx_block_header_t) == 8;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PZX Signature Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_pzx_verify_signature(void) {
    uint8_t valid[8] = { 'P', 'Z', 'X', 'T', 1, 0, 0, 0 };
    uint8_t invalid[8] = { 'B', 'A', 'D', '!', 0, 0, 0, 0 };
    
    return uft_pzx_verify_signature(valid, 8) &&
           !uft_pzx_verify_signature(invalid, 8);
}

static int test_pzx_probe_valid(void) {
    uint8_t data[24];
    memset(data, 0, sizeof(data));
    
    /* Header */
    memcpy(data, "PZXT", 4);
    data[4] = 1;  /* version 1.0 */
    
    /* First block: PULS */
    data[8] = 'P'; data[9] = 'U'; data[10] = 'L'; data[11] = 'S';
    data[12] = 4; /* length = 4 */
    
    int score = uft_pzx_probe(data, sizeof(data));
    return score >= 80;
}

static int test_pzx_probe_invalid(void) {
    uint8_t data[16];
    memset(data, 0xFF, sizeof(data));
    
    int score = uft_pzx_probe(data, sizeof(data));
    return score == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PZX Tag Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_pzx_tag_to_str(void) {
    char str[5];
    uft_pzx_tag_to_str(UFT_PZX_TAG_PULS, str);
    return strcmp(str, "PULS") == 0;
}

static int test_pzx_str_to_tag(void) {
    uint32_t tag = uft_pzx_str_to_tag("DATA");
    return tag == UFT_PZX_TAG_DATA;
}

static int test_pzx_block_names(void) {
    return strstr(uft_pzx_block_name(UFT_PZX_TAG_PULS), "Pulse") != NULL &&
           strstr(uft_pzx_block_name(UFT_PZX_TAG_DATA), "Data") != NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PZX PULS Decode Tests (Full Spec)
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_pzx_decode_puls_simple(void) {
    /* Simple durations: 100, 200, 300 */
    uint8_t data[] = {
        0x64, 0x00,  /* 100 */
        0xC8, 0x00,  /* 200 */
        0x2C, 0x01   /* 300 */
    };
    
    uft_pzx_pulse_t* pulses = NULL;
    size_t count = 0;
    
    bool ok = uft_pzx_decode_puls(data, sizeof(data), &pulses, &count);
    
    if (!ok || count != 3) {
        free(pulses);
        return 0;
    }
    
    int result = (pulses[0].duration == 100 &&
                  pulses[1].duration == 200 &&
                  pulses[2].duration == 300);
    
    free(pulses);
    return result;
}

static int test_pzx_decode_puls_extended(void) {
    /* Extended 32-bit: 0x0000 followed by 32-bit value */
    uint8_t data[] = {
        0x00, 0x00,              /* Extended marker */
        0x40, 0x42, 0x0F, 0x00   /* 1000000 (0x000F4240) */
    };
    
    uft_pzx_pulse_t* pulses = NULL;
    size_t count = 0;
    
    bool ok = uft_pzx_decode_puls(data, sizeof(data), &pulses, &count);
    
    if (!ok || count != 1) {
        free(pulses);
        return 0;
    }
    
    int result = (pulses[0].duration == 1000000);
    
    free(pulses);
    return result;
}

static int test_pzx_decode_puls_repeat(void) {
    /* Repeat: 0x8003 (repeat 3x), then duration 500 */
    uint8_t data[] = {
        0x03, 0x80,  /* Repeat 3 times */
        0xF4, 0x01   /* Duration 500 */
    };
    
    uft_pzx_pulse_t* pulses = NULL;
    size_t count = 0;
    
    bool ok = uft_pzx_decode_puls(data, sizeof(data), &pulses, &count);
    
    if (!ok || count != 3) {
        free(pulses);
        return 0;
    }
    
    /* All 3 pulses should be 500 */
    int result = (pulses[0].duration == 500 &&
                  pulses[1].duration == 500 &&
                  pulses[2].duration == 500);
    
    free(pulses);
    return result;
}

static int test_pzx_calc_tstates(void) {
    uft_pzx_pulse_t pulses[] = {
        { 1000, 1 },
        { 2000, 1 },
        { 3000, 1 }
    };
    
    uint64_t total = uft_pzx_calc_tstates(pulses, 3);
    return total == 6000;
}

static int test_pzx_tstates_to_sec(void) {
    /* 3500000 T-states = 1 second @ 3.5 MHz */
    float sec = uft_pzx_tstates_to_sec(3500000);
    return sec > 0.99f && sec < 1.01f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CSW Structure Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_csw_v1_header_size(void) {
    return sizeof(uft_csw_v1_header_t) == 32;
}

static int test_csw_v2_header_size(void) {
    return sizeof(uft_csw_v2_header_t) == 52;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CSW Signature Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_csw_verify_signature(void) {
    uint8_t valid[32];
    memset(valid, 0, sizeof(valid));
    memcpy(valid, "Compressed Square Wave", 22);
    valid[22] = 0x1A;
    
    uint8_t invalid[32];
    memset(invalid, 0, sizeof(invalid));
    memcpy(invalid, "Not A CSW File!", 15);
    
    return uft_csw_verify_signature(valid, 32) &&
           !uft_csw_verify_signature(invalid, 32);
}

static int test_csw_probe_v1(void) {
    uint8_t data[32];
    memset(data, 0, sizeof(data));
    
    memcpy(data, "Compressed Square Wave", 22);
    data[22] = 0x1A;
    data[23] = 1;  /* major */
    data[24] = 1;  /* minor */
    data[27] = 1;  /* compression = RLE */
    
    int score = uft_csw_probe(data, sizeof(data));
    return score >= 80;
}

static int test_csw_probe_v2(void) {
    uint8_t data[52];
    memset(data, 0, sizeof(data));
    
    memcpy(data, "Compressed Square Wave", 22);
    data[22] = 0x1A;
    data[23] = 2;  /* major */
    data[24] = 0;  /* minor */
    data[33] = 1;  /* compression = RLE (v2 offset) */
    
    int score = uft_csw_probe(data, sizeof(data));
    return score >= 80;
}

static int test_csw_probe_invalid(void) {
    uint8_t data[32];
    memset(data, 0xFF, sizeof(data));
    
    int score = uft_csw_probe(data, sizeof(data));
    return score == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CSW Parse Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_csw_parse_v1_header(void) {
    uint8_t data[64];
    memset(data, 0, sizeof(data));
    
    memcpy(data, "Compressed Square Wave", 22);
    data[22] = 0x1A;
    data[23] = 1;
    data[24] = 1;
    data[25] = 0x44; data[26] = 0xAC;  /* 44100 Hz */
    data[27] = 1;     /* RLE */
    data[28] = 1;     /* Polarity high */
    
    uft_csw_file_info_t info;
    bool ok = uft_csw_parse_header(data, sizeof(data), &info);
    
    return ok &&
           info.version_major == 1 &&
           info.version_minor == 1 &&
           info.sample_rate == 44100 &&
           info.compression == 1 &&
           info.initial_polarity == true &&
           info.data_offset == 32;
}

static int test_csw_parse_v2_header(void) {
    uint8_t data[64];
    memset(data, 0, sizeof(data));
    
    memcpy(data, "Compressed Square Wave", 22);
    data[22] = 0x1A;
    data[23] = 2;
    data[24] = 0;
    /* Sample rate at offset 25-28 */
    data[25] = 0x44; data[26] = 0xAC; data[27] = 0x00; data[28] = 0x00;  /* 44100 */
    /* Total samples at offset 29-32 */
    data[29] = 0xE8; data[30] = 0x03; data[31] = 0x00; data[32] = 0x00;  /* 1000 */
    data[33] = 1;     /* RLE */
    data[34] = 0;     /* Polarity low */
    data[35] = 0;     /* No extension */
    memcpy(data + 36, "UFT Test", 8);
    
    uft_csw_file_info_t info;
    bool ok = uft_csw_parse_header(data, sizeof(data), &info);
    
    return ok &&
           info.version_major == 2 &&
           info.sample_rate == 44100 &&
           info.total_samples == 1000 &&
           info.data_offset == 52 &&
           strstr(info.encoding_app, "UFT") != NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CSW RLE Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_csw_decode_rle_simple(void) {
    uint8_t data[] = { 10, 20, 30, 40, 50 };
    
    uint32_t* samples = NULL;
    size_t count = 0;
    
    bool ok = uft_csw_decode_rle(data, sizeof(data), &samples, &count);
    
    if (!ok || count != 5) {
        free(samples);
        return 0;
    }
    
    int result = (samples[0] == 10 && samples[1] == 20 &&
                  samples[2] == 30 && samples[3] == 40 &&
                  samples[4] == 50);
    
    free(samples);
    return result;
}

static int test_csw_decode_rle_extended(void) {
    uint8_t data[] = {
        100,                         /* 100 */
        0, 0x10, 0x27, 0x00, 0x00,   /* 0 + 10000 (extended) */
        200                          /* 200 */
    };
    
    uint32_t* samples = NULL;
    size_t count = 0;
    
    bool ok = uft_csw_decode_rle(data, sizeof(data), &samples, &count);
    
    if (!ok || count != 3) {
        free(samples);
        return 0;
    }
    
    int result = (samples[0] == 100 &&
                  samples[1] == 10000 &&
                  samples[2] == 200);
    
    free(samples);
    return result;
}

static int test_csw_encode_rle_simple(void) {
    uint32_t samples[] = { 10, 20, 30 };
    uint8_t out[16];
    
    size_t len = uft_csw_encode_rle(samples, 3, out, sizeof(out));
    
    return len == 3 && out[0] == 10 && out[1] == 20 && out[2] == 30;
}

static int test_csw_encode_rle_extended(void) {
    uint32_t samples[] = { 100, 1000 };  /* 1000 > 255, needs extended */
    uint8_t out[16];
    
    size_t len = uft_csw_encode_rle(samples, 2, out, sizeof(out));
    
    /* 100 = 1 byte, 1000 = 5 bytes (0 + u32) */
    return len == 6 && out[0] == 100 && out[1] == 0;
}

static int test_csw_calc_duration(void) {
    uint32_t samples[] = { 44100, 44100, 44100 };  /* 3 seconds @ 44100 Hz */
    
    float dur = uft_csw_calc_duration(samples, 3, 44100);
    
    return dur > 2.9f && dur < 3.1f;
}

static int test_csw_compression_names(void) {
    return strcmp(uft_csw_compression_name(UFT_CSW_COMP_RLE), "RLE") == 0 &&
           strcmp(uft_csw_compression_name(UFT_CSW_COMP_ZRLE), "Z-RLE") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== PZX/CSW Tape Format Tests ===\n\n");
    
    printf("[PZX Structure]\n");
    TEST(pzx_header_size);
    TEST(pzx_block_header_size);
    
    printf("\n[PZX Signature]\n");
    TEST(pzx_verify_signature);
    TEST(pzx_probe_valid);
    TEST(pzx_probe_invalid);
    
    printf("\n[PZX Tags]\n");
    TEST(pzx_tag_to_str);
    TEST(pzx_str_to_tag);
    TEST(pzx_block_names);
    
    printf("\n[PZX PULS Decode (Full Spec)]\n");
    TEST(pzx_decode_puls_simple);
    TEST(pzx_decode_puls_extended);
    TEST(pzx_decode_puls_repeat);
    TEST(pzx_calc_tstates);
    TEST(pzx_tstates_to_sec);
    
    printf("\n[CSW Structure]\n");
    TEST(csw_v1_header_size);
    TEST(csw_v2_header_size);
    
    printf("\n[CSW Signature]\n");
    TEST(csw_verify_signature);
    TEST(csw_probe_v1);
    TEST(csw_probe_v2);
    TEST(csw_probe_invalid);
    
    printf("\n[CSW Parse]\n");
    TEST(csw_parse_v1_header);
    TEST(csw_parse_v2_header);
    
    printf("\n[CSW RLE]\n");
    TEST(csw_decode_rle_simple);
    TEST(csw_decode_rle_extended);
    TEST(csw_encode_rle_simple);
    TEST(csw_encode_rle_extended);
    TEST(csw_calc_duration);
    TEST(csw_compression_names);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
