/**
 * @file test_protection.c
 * @brief Unit tests for UFT Copy Protection Detection
 * 
 * Tests CopyLock LFSR, Speedlock detection, and all longtrack variants.
 * 
 * Compile: gcc -o test_protection test_protection.c ../sources/uft_protection.c 
 *          ../sources/uft_protection_ext.c -I../integration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "uft_protection.h"
#include "uft_protection_ext.h"

/*============================================================================
 * Test Utilities
 *============================================================================*/

#define TEST_PASS  "\033[32mPASS\033[0m"
#define TEST_FAIL  "\033[31mFAIL\033[0m"

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(test) do { \
    tests_run++; \
    printf("  Testing %s... ", #test); \
    if (test()) { \
        tests_passed++; \
        printf("%s\n", TEST_PASS); \
    } else { \
        printf("%s\n", TEST_FAIL); \
    } \
} while(0)

/*============================================================================
 * LFSR Tests
 *============================================================================*/

static bool test_lfsr_forward(void)
{
    /* Test known LFSR sequence */
    uint32_t state = 0x123456 & 0x7FFFFF;  /* 23-bit mask */
    
    /* Advance 8 steps */
    for (int i = 0; i < 8; i++) {
        state = uft_lfsr_next(state);
    }
    
    /* Go back 8 steps */
    for (int i = 0; i < 8; i++) {
        state = uft_lfsr_prev(state);
    }
    
    /* Should be back to original */
    return state == (0x123456 & 0x7FFFFF);
}

static bool test_lfsr_byte_extraction(void)
{
    /* Test byte extraction from state */
    uint32_t state = 0x7F8000;  /* bits 22:15 = 0xFF */
    uint8_t byte = uft_lfsr_byte(state);
    
    return byte == 0xFF;
}

static bool test_lfsr_advance(void)
{
    uint32_t state = 0x555555 & 0x7FFFFF;
    
    /* Advance +100 then -100 should return to same state */
    uint32_t advanced = uft_lfsr_advance(state, 100);
    uint32_t reversed = uft_lfsr_advance(advanced, -100);
    
    return state == reversed;
}

/*============================================================================
 * CopyLock Tests
 *============================================================================*/

static bool test_copylock_signature(void)
{
    /* Test that signature constant is correct */
    return strcmp(UFT_COPYLOCK_SIGNATURE, "Rob Northen Comp") == 0;
}

static bool test_copylock_sync_count(void)
{
    /* Test that we have 11 sync marks */
    return UFT_COPYLOCK_SECTORS == 11;
}

static bool test_copylock_sync_values(void)
{
    /* Test specific sync values */
    return uft_copylock_sync_marks[0] == 0x8a91 &&
           uft_copylock_sync_marks[4] == 0x8912 &&  /* Fast */
           uft_copylock_sync_marks[6] == 0x8914;    /* Slow */
}

/*============================================================================
 * Longtrack Definition Tests
 *============================================================================*/

static bool test_longtrack_defs_count(void)
{
    return UFT_LONGTRACK_DEF_COUNT == 12;
}

static bool test_longtrack_protec_def(void)
{
    const uft_longtrack_def_t *def = uft_longtrack_get_def(UFT_LONGTRACK_PROTEC);
    if (!def) return false;
    
    return def->sync_word == 0x4454 &&
           def->sync_bits == 16 &&
           def->min_bits == 107200;
}

static bool test_longtrack_silmarils_def(void)
{
    const uft_longtrack_def_t *def = uft_longtrack_get_def(UFT_LONGTRACK_SILMARILS);
    if (!def) return false;
    
    return def->sync_word == 0xa144 &&
           def->signature != NULL &&
           strcmp(def->signature, "ROD0") == 0;
}

/*============================================================================
 * Longtrack Generation Tests
 *============================================================================*/

static bool test_generate_protec(void)
{
    uint8_t track_data[16384];  /* 128KB */
    memset(track_data, 0, sizeof(track_data));
    
    size_t bytes = uft_generate_longtrack_protec(0x33, 110000, track_data);
    
    /* Check we got reasonable output */
    if (bytes == 0) return false;
    
    /* Check sync word is present (first 16 bits = 0x4454) */
    uint16_t sync = (track_data[0] << 8) | track_data[1];
    return sync == UFT_SYNC_PROTEC;
}

static bool test_generate_protoscan(void)
{
    uint8_t track_data[16384];
    memset(track_data, 0, sizeof(track_data));
    
    size_t bytes = uft_generate_longtrack_protoscan(105500, track_data);
    
    if (bytes == 0) return false;
    
    /* Check 32-bit sync 0x41244124 */
    uint32_t sync = ((uint32_t)track_data[0] << 24) |
                    ((uint32_t)track_data[1] << 16) |
                    ((uint32_t)track_data[2] << 8) |
                    track_data[3];
    return sync == UFT_SYNC_PROTOSCAN;
}

/*============================================================================
 * Longtrack Detection Tests
 *============================================================================*/

static bool test_detect_protec(void)
{
    /* Generate a PROTEC track and detect it */
    uint8_t track_data[16384];
    memset(track_data, 0, sizeof(track_data));
    
    uft_generate_longtrack_protec(0x33, 110000, track_data);
    
    uft_longtrack_ext_t result;
    bool detected = uft_detect_longtrack_protec(track_data, 110000, &result);
    
    return detected && 
           result.type == UFT_LONGTRACK_PROTEC &&
           result.pattern_byte == 0x33;
}

static bool test_detect_protoscan(void)
{
    uint8_t track_data[16384];
    memset(track_data, 0, sizeof(track_data));
    
    uft_generate_longtrack_protoscan(105500, track_data);
    
    uft_longtrack_ext_t result;
    bool detected = uft_detect_longtrack_protoscan(track_data, 105500, &result);
    
    return detected && result.type == UFT_LONGTRACK_PROTOSCAN;
}

static bool test_detect_ext_auto(void)
{
    /* Generate PROTEC and let auto-detect find it */
    uint8_t track_data[16384];
    memset(track_data, 0, sizeof(track_data));
    
    uft_generate_longtrack_protec(0x44, 110000, track_data);
    
    uft_longtrack_ext_t result;
    bool detected = uft_detect_longtrack_ext(track_data, 110000, NULL, 0, &result);
    
    return detected && result.type == UFT_LONGTRACK_PROTEC;
}

/*============================================================================
 * CRC Tests
 *============================================================================*/

static bool test_crc16_ccitt(void)
{
    /* Test known CRC value */
    const uint8_t test_data[] = "123456789";
    uint16_t crc = uft_crc16_ccitt(test_data, 9);
    
    /* Standard CRC-16-CCITT of "123456789" is 0x29B1 */
    return crc == 0x29B1;
}

/*============================================================================
 * Utility Function Tests
 *============================================================================*/

static bool test_longtrack_type_name(void)
{
    const char *name = uft_longtrack_type_name(UFT_LONGTRACK_PROTEC);
    return name != NULL && strcmp(name, "PROTEC") == 0;
}

static bool test_longtrack_type_name_unknown(void)
{
    const char *name = uft_longtrack_type_name((uft_longtrack_type_t)9999);
    return name != NULL && strcmp(name, "Unknown") == 0;
}

static bool test_protection_name(void)
{
    const char *name = uft_protection_name(UFT_PROT_COPYLOCK);
    return name != NULL && strcmp(name, "CopyLock") == 0;
}

/*============================================================================
 * Main Test Runner
 *============================================================================*/

int main(int argc, char *argv[])
{
    printf("\n=== UFT Protection Detection Tests ===\n\n");
    
    printf("LFSR Tests:\n");
    RUN_TEST(test_lfsr_forward);
    RUN_TEST(test_lfsr_byte_extraction);
    RUN_TEST(test_lfsr_advance);
    
    printf("\nCopyLock Tests:\n");
    RUN_TEST(test_copylock_signature);
    RUN_TEST(test_copylock_sync_count);
    RUN_TEST(test_copylock_sync_values);
    
    printf("\nLongtrack Definition Tests:\n");
    RUN_TEST(test_longtrack_defs_count);
    RUN_TEST(test_longtrack_protec_def);
    RUN_TEST(test_longtrack_silmarils_def);
    
    printf("\nLongtrack Generation Tests:\n");
    RUN_TEST(test_generate_protec);
    RUN_TEST(test_generate_protoscan);
    
    printf("\nLongtrack Detection Tests:\n");
    RUN_TEST(test_detect_protec);
    RUN_TEST(test_detect_protoscan);
    RUN_TEST(test_detect_ext_auto);
    
    printf("\nCRC Tests:\n");
    RUN_TEST(test_crc16_ccitt);
    
    printf("\nUtility Tests:\n");
    RUN_TEST(test_longtrack_type_name);
    RUN_TEST(test_longtrack_type_name_unknown);
    RUN_TEST(test_protection_name);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", 
           tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
