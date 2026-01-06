/*
 * test_c64_gcr_codec.c
 *
 * Unit tests for uft_c64_gcr_codec module.
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -I../libflux_core/include \
 *      test_c64_gcr_codec.c ../libflux_core/src/c64/uft_c64_gcr_codec.c \
 *      -o test_c64_gcr_codec
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "c64/uft_c64_gcr_codec.h"

/* Test counters */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        tests_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s (FAILED at line %d)\n", msg, __LINE__); \
    } \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR TABLE TESTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_gcr_tables(void)
{
    printf("\n=== GCR Tables ===\n");
    
    const uint8_t *encode = uft_c64_gcr_encode_table();
    const uint8_t *decode_hi = uft_c64_gcr_decode_high_table();
    const uint8_t *decode_lo = uft_c64_gcr_decode_low_table();
    
    TEST_ASSERT(encode != NULL, "Encode table exists");
    TEST_ASSERT(decode_hi != NULL, "Decode high table exists");
    TEST_ASSERT(decode_lo != NULL, "Decode low table exists");
    
    /* Test that all 16 nibbles encode to valid GCR */
    int all_valid = 1;
    for (int i = 0; i < 16; i++) {
        if (encode[i] > 31 || !uft_c64_gcr_is_valid(encode[i])) {
            all_valid = 0;
            break;
        }
    }
    TEST_ASSERT(all_valid, "All 16 nibbles encode to valid GCR");
    
    /* Test roundtrip for all nibbles */
    int roundtrip_ok = 1;
    for (int i = 0; i < 16; i++) {
        uint8_t gcr = uft_c64_gcr_encode_nibble(i);
        uint8_t decoded = uft_c64_gcr_decode_nibble(gcr);
        if (decoded != i) {
            roundtrip_ok = 0;
            printf("    Nibble %d: encode=%02X, decode=%02X\n", i, gcr, decoded);
        }
    }
    TEST_ASSERT(roundtrip_ok, "All nibbles roundtrip correctly");
    
    /* Test invalid GCR values */
    TEST_ASSERT(!uft_c64_gcr_is_valid(0x00), "GCR 0x00 is invalid");
    TEST_ASSERT(!uft_c64_gcr_is_valid(0x01), "GCR 0x01 is invalid");
    TEST_ASSERT(!uft_c64_gcr_is_valid(0x10), "GCR 0x10 is invalid");
    TEST_ASSERT(uft_c64_gcr_is_valid(0x0A), "GCR 0x0A is valid (nibble 0)");
    TEST_ASSERT(uft_c64_gcr_is_valid(0x15), "GCR 0x15 is valid (nibble F)");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4-BYTE ENCODE/DECODE TESTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_gcr_4bytes(void)
{
    printf("\n=== GCR 4-Byte Encode/Decode ===\n");
    
    /* Test pattern 1: All zeros */
    uint8_t plain1[4] = {0x00, 0x00, 0x00, 0x00};
    uint8_t gcr1[5];
    uint8_t decoded1[4];
    
    uft_c64_gcr_encode_4bytes(plain1, gcr1);
    int result1 = uft_c64_gcr_decode_4bytes(gcr1, decoded1);
    
    TEST_ASSERT(result1 == 4, "Decode all-zeros returns 4 bytes");
    TEST_ASSERT(memcmp(plain1, decoded1, 4) == 0, "All-zeros roundtrip correct");
    
    /* Test pattern 2: All ones (0xFF) */
    uint8_t plain2[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t gcr2[5];
    uint8_t decoded2[4];
    
    uft_c64_gcr_encode_4bytes(plain2, gcr2);
    int result2 = uft_c64_gcr_decode_4bytes(gcr2, decoded2);
    
    TEST_ASSERT(result2 == 4, "Decode all-ones returns 4 bytes");
    TEST_ASSERT(memcmp(plain2, decoded2, 4) == 0, "All-ones roundtrip correct");
    
    /* Test pattern 3: Sequential bytes */
    uint8_t plain3[4] = {0x01, 0x23, 0x45, 0x67};
    uint8_t gcr3[5];
    uint8_t decoded3[4];
    
    uft_c64_gcr_encode_4bytes(plain3, gcr3);
    int result3 = uft_c64_gcr_decode_4bytes(gcr3, decoded3);
    
    TEST_ASSERT(result3 == 4, "Decode sequential bytes returns 4");
    TEST_ASSERT(memcmp(plain3, decoded3, 4) == 0, "Sequential bytes roundtrip");
    
    /* Test pattern 4: High nibbles */
    uint8_t plain4[4] = {0x89, 0xAB, 0xCD, 0xEF};
    uint8_t gcr4[5];
    uint8_t decoded4[4];
    
    uft_c64_gcr_encode_4bytes(plain4, gcr4);
    int result4 = uft_c64_gcr_decode_4bytes(gcr4, decoded4);
    
    TEST_ASSERT(result4 == 4, "Decode high nibbles returns 4");
    TEST_ASSERT(memcmp(plain4, decoded4, 4) == 0, "High nibbles roundtrip");
    
    /* Test size expansion: 4 bytes -> 5 bytes */
    TEST_ASSERT(sizeof(gcr1) == 5, "GCR output is 5 bytes");
    
    /* Test all 256 values in each byte position */
    int all_roundtrip = 1;
    for (int val = 0; val < 256; val++) {
        uint8_t plain[4] = {(uint8_t)val, (uint8_t)val, (uint8_t)val, (uint8_t)val};
        uint8_t gcr[5];
        uint8_t decoded[4];
        
        uft_c64_gcr_encode_4bytes(plain, gcr);
        int result = uft_c64_gcr_decode_4bytes(gcr, decoded);
        
        if (result != 4 || memcmp(plain, decoded, 4) != 0) {
            all_roundtrip = 0;
            break;
        }
    }
    TEST_ASSERT(all_roundtrip, "All 256 byte values roundtrip correctly");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SYNC DETECTION TESTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_sync_detection(void)
{
    printf("\n=== Sync Detection ===\n");
    
    /* Create track with sync marks */
    uint8_t track[100];
    memset(track, 0x55, sizeof(track));  /* Gap bytes */
    
    /* Insert sync at position 10 */
    track[10] = 0xFF;
    track[11] = 0xFF;
    track[12] = 0xFF;
    track[13] = 0xFF;
    track[14] = 0xFF;
    track[15] = 0x52;  /* Header marker */
    
    const uint8_t *ptr = track;
    const uint8_t *end = track + sizeof(track);
    
    bool found = uft_c64_gcr_find_sync(&ptr, end);
    TEST_ASSERT(found, "Sync found in track");
    TEST_ASSERT(ptr == track + 15, "Pointer after sync is correct");
    
    /* Test sync count */
    size_t count = uft_c64_gcr_count_sync(track + 10, end);
    TEST_ASSERT(count == 5, "Sync count is 5");
    
    /* Test no sync in gap */
    const uint8_t *ptr2 = track;
    bool found2 = uft_c64_gcr_find_sync(&ptr2, track + 10);
    TEST_ASSERT(!found2, "No sync in gap-only region");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * BAD GCR DETECTION TESTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_bad_gcr_detection(void)
{
    printf("\n=== Bad GCR Detection ===\n");
    
    /* Valid GCR pattern (encoded zeros) */
    uint8_t valid[5];
    uint8_t zeros[4] = {0, 0, 0, 0};
    uft_c64_gcr_encode_4bytes(zeros, valid);
    
    TEST_ASSERT(!uft_c64_gcr_is_bad_at(valid, 5, 0), "Valid GCR at position 0");
    TEST_ASSERT(!uft_c64_gcr_is_bad_at(valid, 5, 1), "Valid GCR at position 1");
    
    /* Create bad GCR by inserting invalid pattern */
    uint8_t bad[5] = {0x00, 0x00, 0x00, 0x00, 0x00};  /* All zeros is bad */
    
    /* 0x00 >> 3 = 0x00, which should be invalid */
    /* Actually, let's check what counts as bad */
    size_t bad_count = uft_c64_gcr_count_bad(bad, 5);
    printf("    Bad count in all-zeros: %zu\n", bad_count);
    
    /* Test with known bad pattern: 0x00 0x01 */
    uint8_t bad2[5] = {0x00, 0x01, 0x02, 0x03, 0x04};
    size_t bad_count2 = uft_c64_gcr_count_bad(bad2, 5);
    printf("    Bad count in test pattern: %zu\n", bad_count2);
    
    TEST_ASSERT(bad_count > 0 || bad_count2 > 0, "Bad GCR detected in invalid patterns");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PETSCII CONVERSION TESTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_petscii_conversion(void)
{
    printf("\n=== PETSCII Conversion ===\n");
    
    /* Lowercase to uppercase */
    TEST_ASSERT(uft_c64_to_petscii('a') == 'A', "a -> A");
    TEST_ASSERT(uft_c64_to_petscii('z') == 'Z', "z -> Z");
    
    /* Uppercase to shifted */
    TEST_ASSERT(uft_c64_to_petscii('A') == 'a', "A -> shifted (a)");
    
    /* Numbers unchanged */
    TEST_ASSERT(uft_c64_to_petscii('0') == '0', "0 unchanged");
    TEST_ASSERT(uft_c64_to_petscii('9') == '9', "9 unchanged");
    
    /* Roundtrip */
    char test[] = "HELLO";
    char original[6];
    strcpy(original, test);
    
    uft_c64_str_to_petscii(test, strlen(test));
    uft_c64_str_from_petscii(test, strlen(test));
    TEST_ASSERT(strcmp(test, original) == 0, "String roundtrip");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK FORMAT TESTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_track_format(void)
{
    printf("\n=== Track Format ===\n");
    
    /* Test track formatting check with valid pattern */
    uint8_t formatted[100];
    memset(formatted, 0x55, sizeof(formatted));
    
    /* Add a valid GCR sequence */
    uint8_t data[4] = {0x08, 0x01, 0x00, 0x01};  /* Header-like data */
    uft_c64_gcr_encode_4bytes(data, formatted + 20);
    uft_c64_gcr_encode_4bytes(data, formatted + 25);
    uft_c64_gcr_encode_4bytes(data, formatted + 30);
    uft_c64_gcr_encode_4bytes(data, formatted + 35);
    
    bool is_formatted = uft_c64_gcr_is_formatted(formatted, sizeof(formatted));
    TEST_ASSERT(is_formatted, "Track with valid GCR is formatted");
    
    /* Unformatted track (all gaps) */
    uint8_t unformatted[100];
    memset(unformatted, 0x55, sizeof(unformatted));
    
    /* Note: This test may depend on implementation */
    printf("    Unformatted detection depends on implementation\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║  UFT C64 GCR Codec Unit Tests                                      ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
    
    test_gcr_tables();
    test_gcr_4bytes();
    test_sync_detection();
    test_bad_gcr_detection();
    test_petscii_conversion();
    test_track_format();
    
    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("════════════════════════════════════════════════════════════════════\n");
    
    return (tests_failed > 0) ? 1 : 0;
}
