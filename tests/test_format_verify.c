/**
 * @file test_format_verify.c
 * @brief Tests for WOZ, A2R, TD0 format verifiers
 * 
 * Part of INDUSTRIAL_UPGRADE_PLAN - Parser Matrix Verify Gap
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include the header directly since we might not have full build */
#include "uft/uft_format_verify.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  TEST: %-40s ", #name); \
        fflush(stdout); \
    } while(0)

#define PASS() \
    do { \
        tests_passed++; \
        printf("[PASS]\n"); \
    } while(0)

#define FAIL(msg) \
    do { \
        printf("[FAIL] %s\n", msg); \
    } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * WOZ Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_woz_null_data(void) {
    TEST(woz_null_data);
    
    uft_verify_status_t status = uft_verify_woz(NULL, 0, NULL);
    if (status == UFT_VERIFY_SIZE_MISMATCH) {
        PASS();
    } else {
        FAIL("Expected FORMAT_ERROR for NULL data");
    }
}

static void test_woz_small_data(void) {
    TEST(woz_small_data);
    
    uint8_t data[4] = {0};
    uft_verify_status_t status = uft_verify_woz(data, sizeof(data), NULL);
    if (status == UFT_VERIFY_SIZE_MISMATCH) {
        PASS();
    } else {
        FAIL("Expected FORMAT_ERROR for small data");
    }
}

static void test_woz_bad_magic(void) {
    TEST(woz_bad_magic);
    
    uint8_t data[16] = {'B', 'A', 'D', '!', 0xFF, 0x0A, 0x0D, 0x0A};
    uft_verify_status_t status = uft_verify_woz(data, sizeof(data), NULL);
    if (status == UFT_VERIFY_FORMAT_ERROR) {
        PASS();
    } else {
        FAIL("Expected FORMAT_ERROR for bad magic");
    }
}

static void test_woz_good_magic_woz1(void) {
    TEST(woz_good_magic_woz1);
    
    /* Minimal WOZ1 header - will fail CRC but magic is OK */
    uint8_t data[64] = {
        'W', 'O', 'Z', '1',  /* Magic */
        0xFF, 0x0A, 0x0D, 0x0A,  /* Header bytes */
        0x00, 0x00, 0x00, 0x00,  /* CRC (will be wrong) */
        /* INFO chunk */
        'I', 'N', 'F', 'O',
        0x3C, 0x00, 0x00, 0x00,  /* Size: 60 */
    };
    
    uft_verify_result_t result = {0};
    uft_verify_status_t status = uft_verify_woz(data, sizeof(data), &result);
    
    /* Should fail CRC check since CRC is 0 */
    if (status == UFT_VERIFY_CRC_ERROR) {
        PASS();
    } else {
        FAIL("Expected CRC_ERROR for zero CRC");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * A2R Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_a2r_null_data(void) {
    TEST(a2r_null_data);
    
    uft_verify_status_t status = uft_verify_a2r(NULL, 0, NULL);
    if (status == UFT_VERIFY_SIZE_MISMATCH) {
        PASS();
    } else {
        FAIL("Expected FORMAT_ERROR for NULL data");
    }
}

static void test_a2r_bad_magic(void) {
    TEST(a2r_bad_magic);
    
    uint8_t data[16] = {'X', 'Y', 'Z', '2', 0xFF, 0x0A, 0x0D, 0x0A};
    uft_verify_status_t status = uft_verify_a2r(data, sizeof(data), NULL);
    if (status == UFT_VERIFY_FORMAT_ERROR) {
        PASS();
    } else {
        FAIL("Expected FORMAT_ERROR for bad magic");
    }
}

static void test_a2r_good_magic_no_info(void) {
    TEST(a2r_good_magic_no_info);
    
    /* A2R2 header without INFO chunk */
    uint8_t data[16] = {
        'A', '2', 'R', '2',
        0xFF, 0x0A, 0x0D, 0x0A
    };
    
    uft_verify_result_t result = {0};
    uft_verify_status_t status = uft_verify_a2r(data, sizeof(data), &result);
    
    /* Should fail - no INFO chunk */
    if (status == UFT_VERIFY_FORMAT_ERROR) {
        PASS();
    } else {
        FAIL("Expected FORMAT_ERROR for missing INFO");
    }
}

static void test_a2r_with_info_chunk(void) {
    TEST(a2r_with_info_chunk);
    
    /* A2R2 header with INFO chunk */
    uint8_t data[32] = {
        'A', '2', 'R', '2',
        0xFF, 0x0A, 0x0D, 0x0A,
        /* INFO chunk */
        'I', 'N', 'F', 'O',
        0x08, 0x00, 0x00, 0x00,  /* Size: 8 */
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    
    uft_verify_result_t result = {0};
    uft_verify_status_t status = uft_verify_a2r(data, sizeof(data), &result);
    
    if (status == UFT_VERIFY_OK) {
        PASS();
    } else {
        FAIL("Expected OK for valid A2R with INFO");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * TD0 Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_td0_null_data(void) {
    TEST(td0_null_data);
    
    uft_verify_status_t status = uft_verify_td0(NULL, 0, NULL);
    if (status == UFT_VERIFY_SIZE_MISMATCH) {
        PASS();
    } else {
        FAIL("Expected FORMAT_ERROR for NULL data");
    }
}

static void test_td0_bad_magic(void) {
    TEST(td0_bad_magic);
    
    uint8_t data[16] = {'X', 'X', 0, 0};
    uft_verify_status_t status = uft_verify_td0(data, sizeof(data), NULL);
    if (status == UFT_VERIFY_FORMAT_ERROR) {
        PASS();
    } else {
        FAIL("Expected FORMAT_ERROR for bad magic");
    }
}

static void test_td0_good_magic_bad_version(void) {
    TEST(td0_good_magic_bad_version);
    
    /* TD header with invalid version */
    uint8_t data[16] = {
        'T', 'D',  /* Magic */
        0x00,      /* Volume sequence */
        0x00,      /* Check signature */
        0x05,      /* Version (5 - too old) */
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00 /* CRC */
    };
    
    uft_verify_result_t result = {0};
    uft_verify_status_t status = uft_verify_td0(data, sizeof(data), &result);
    
    if (status == UFT_VERIFY_FORMAT_ERROR) {
        PASS();
    } else {
        FAIL("Expected FORMAT_ERROR for bad version");
    }
}

static void test_td0_compressed_magic(void) {
    TEST(td0_compressed_magic);
    
    /* td (lowercase) = compressed */
    uint8_t data[16] = {
        't', 'd',  /* Compressed magic */
        0x00,      /* Volume sequence */
        0x00,      /* Check signature */
        0x15,      /* Version 21 (2.1) */
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00 /* CRC (will be wrong) */
    };
    
    uft_verify_result_t result = {0};
    uft_verify_status_t status = uft_verify_td0(data, sizeof(data), &result);
    
    /* Should fail CRC, not format */
    if (status == UFT_VERIFY_CRC_ERROR) {
        PASS();
    } else {
        FAIL("Expected CRC_ERROR (compressed magic accepted)");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  UFT Format Verify Tests\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    printf("WOZ Format Tests:\n");
    test_woz_null_data();
    test_woz_small_data();
    test_woz_bad_magic();
    test_woz_good_magic_woz1();
    
    printf("\nA2R Format Tests:\n");
    test_a2r_null_data();
    test_a2r_bad_magic();
    test_a2r_good_magic_no_info();
    test_a2r_with_info_chunk();
    
    printf("\nTD0 Format Tests:\n");
    test_td0_null_data();
    test_td0_bad_magic();
    test_td0_good_magic_bad_version();
    test_td0_compressed_magic();
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
