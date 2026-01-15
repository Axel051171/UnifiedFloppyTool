/**
 * @file test_msa.c
 * @brief MSA format test utilities
 * @version 3.8.0
 */
/*
 * test_msa.c - Unit Tests for MSA Format Parser
 */

#include "uft/uft_msa.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Test data: MSA header for 720K disk */
static const uint8_t TEST_MSA_HEADER[] = {
    0x0E, 0x0F,  /* Signature */
    0x00, 0x09,  /* 9 sectors per track */
    0x00, 0x01,  /* 2 sides (1+1) */
    0x00, 0x00,  /* Start track 0 */
    0x00, 0x4F,  /* End track 79 */
};

/* Test RLE: "AAAAAABBBBCCCCCC" */
static const uint8_t TEST_RLE_UNCOMPRESSED[] = {
    'A', 'A', 'A', 'A', 'A', 'A',
    'B', 'B', 'B', 'B',
    'C', 'C', 'C', 'C', 'C', 'C',
};

/* Expected RLE compressed: $E5 'A' 00 06, $E5 'B' 00 04, $E5 'C' 00 06 */
static const uint8_t TEST_RLE_COMPRESSED[] = {
    0xE5, 'A', 0x00, 0x06,
    0xE5, 'B', 0x00, 0x04,
    0xE5, 'C', 0x00, 0x06,
};

static int test_count = 0;
static int pass_count = 0;

#define TEST(name) do { \
    test_count++; \
    printf("  TEST: %s ... ", name); \
} while(0)

#define PASS() do { \
    pass_count++; \
    printf("PASS\n"); \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
} while(0)

void test_header_parse(void) {
    TEST("Header Parse");
    
    uft_msa_header_t header;
    uft_msa_error_t err = uft_msa_parse_header(TEST_MSA_HEADER, 
                                                sizeof(TEST_MSA_HEADER), 
                                                &header);
    
    if (err != UFT_MSA_OK) { FAIL("parse failed"); return; }
    if (header.signature != 0x0E0F) { FAIL("bad signature"); return; }
    if (header.sectors_per_track != 9) { FAIL("bad sectors"); return; }
    if (header.sides != 1) { FAIL("bad sides"); return; }
    if (header.start_track != 0) { FAIL("bad start"); return; }
    if (header.end_track != 79) { FAIL("bad end"); return; }
    
    PASS();
}

void test_header_validate(void) {
    TEST("Header Validate");
    
    uft_msa_header_t header;
    uft_msa_info_t info;
    
    uft_msa_parse_header(TEST_MSA_HEADER, sizeof(TEST_MSA_HEADER), &header);
    uft_msa_error_t err = uft_msa_validate_header(&header, &info);
    
    if (err != UFT_MSA_OK) { FAIL("validate failed"); return; }
    if (info.sectors_per_track != 9) { FAIL("bad sectors"); return; }
    if (info.side_count != 2) { FAIL("bad side_count"); return; }
    if (info.track_count != 80) { FAIL("bad track_count"); return; }
    if (info.raw_size != 80 * 2 * 9 * 512) { FAIL("bad raw_size"); return; }
    
    PASS();
}

void test_probe(void) {
    TEST("Probe Valid");
    
    if (!uft_msa_probe(TEST_MSA_HEADER, sizeof(TEST_MSA_HEADER))) {
        FAIL("probe returned false");
        return;
    }
    PASS();
    
    TEST("Probe Invalid");
    uint8_t bad_header[] = {0x00, 0x00, 0x00, 0x00, 0x00};
    if (uft_msa_probe(bad_header, sizeof(bad_header))) {
        FAIL("probe returned true for bad header");
        return;
    }
    PASS();
}

void test_rle_decode(void) {
    TEST("RLE Decode");
    
    uint8_t output[256];
    size_t written;
    
    uft_msa_error_t err = uft_msa_rle_decode(TEST_RLE_COMPRESSED,
                                              sizeof(TEST_RLE_COMPRESSED),
                                              output, sizeof(output),
                                              &written);
    
    if (err != UFT_MSA_OK) { FAIL(uft_msa_strerror(err)); return; }
    if (written != sizeof(TEST_RLE_UNCOMPRESSED)) { FAIL("wrong length"); return; }
    if (memcmp(output, TEST_RLE_UNCOMPRESSED, written) != 0) { FAIL("data mismatch"); return; }
    
    PASS();
}

void test_rle_encode(void) {
    TEST("RLE Encode");
    
    uint8_t output[256];
    size_t written;
    
    uft_msa_error_t err = uft_msa_rle_encode(TEST_RLE_UNCOMPRESSED,
                                              sizeof(TEST_RLE_UNCOMPRESSED),
                                              output, sizeof(output),
                                              &written);
    
    if (err != UFT_MSA_OK) { FAIL(uft_msa_strerror(err)); return; }
    if (written != sizeof(TEST_RLE_COMPRESSED)) { FAIL("wrong length"); return; }
    if (memcmp(output, TEST_RLE_COMPRESSED, written) != 0) { FAIL("data mismatch"); return; }
    
    PASS();
}

void test_rle_roundtrip(void) {
    TEST("RLE Roundtrip");
    
    /* Create test data with various patterns */
    uint8_t original[1024];
    for (int i = 0; i < 256; i++) original[i] = 'X';       /* Run of X */
    for (int i = 256; i < 260; i++) original[i] = 0xE5;    /* E5 bytes */
    for (int i = 260; i < 512; i++) original[i] = (uint8_t)i; /* Mixed */
    for (int i = 512; i < 1024; i++) original[i] = 0x00;   /* Zeros */
    
    uint8_t compressed[2048];
    uint8_t decompressed[1024];
    size_t comp_len, decomp_len;
    
    uft_msa_error_t err = uft_msa_rle_encode(original, sizeof(original),
                                              compressed, sizeof(compressed),
                                              &comp_len);
    if (err != UFT_MSA_OK) { FAIL("encode failed"); return; }
    
    err = uft_msa_rle_decode(compressed, comp_len,
                              decompressed, sizeof(decompressed),
                              &decomp_len);
    if (err != UFT_MSA_OK) { FAIL("decode failed"); return; }
    
    if (decomp_len != sizeof(original)) { FAIL("wrong length"); return; }
    if (memcmp(original, decompressed, sizeof(original)) != 0) { FAIL("data mismatch"); return; }
    
    printf("(compressed %zu -> %zu) ", sizeof(original), comp_len);
    PASS();
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("MSA FORMAT UNIT TESTS\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    test_header_parse();
    test_header_validate();
    test_probe();
    test_rle_decode();
    test_rle_encode();
    test_rle_roundtrip();
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("RESULTS: %d/%d tests passed\n", pass_count, test_count);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return (pass_count == test_count) ? 0 : 1;
}
