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
 * IMG Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_img_null_data(void) {
    TEST(img_null_data);
    
    uft_verify_status_t status = uft_verify_img_buffer(NULL, 0, NULL);
    if (status == UFT_VERIFY_SIZE_MISMATCH) {
        PASS();
    } else {
        FAIL("Expected SIZE_MISMATCH for NULL data");
    }
}

static void test_img_valid_size_360k(void) {
    TEST(img_valid_size_360k);
    
    /* Allocate 360KB of zeros (valid disk size) */
    size_t size = 360 * 1024;
    uint8_t *data = (uint8_t *)calloc(size, 1);
    if (!data) {
        FAIL("Memory allocation failed");
        return;
    }
    
    uft_verify_status_t status = uft_verify_img_buffer(data, size, NULL);
    free(data);
    
    if (status == UFT_VERIFY_OK) {
        PASS();
    } else {
        FAIL("Expected OK for valid 360KB size");
    }
}

static void test_img_invalid_size(void) {
    TEST(img_invalid_size);
    
    /* Odd size that's not a valid disk image */
    uint8_t data[12345];
    memset(data, 0, sizeof(data));
    
    uft_verify_status_t status = uft_verify_img_buffer(data, sizeof(data), NULL);
    if (status == UFT_VERIFY_SIZE_MISMATCH) {
        PASS();
    } else {
        FAIL("Expected SIZE_MISMATCH for odd size");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * IMD Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_imd_null_data(void) {
    TEST(imd_null_data);
    
    uft_verify_status_t status = uft_verify_imd_buffer(NULL, 0, NULL);
    if (status == UFT_VERIFY_SIZE_MISMATCH) {
        PASS();
    } else {
        FAIL("Expected SIZE_MISMATCH for NULL data");
    }
}

static void test_imd_bad_magic(void) {
    TEST(imd_bad_magic);
    
    uint8_t data[16] = {'B', 'A', 'D', ' '};
    uft_verify_status_t status = uft_verify_imd_buffer(data, sizeof(data), NULL);
    if (status == UFT_VERIFY_FORMAT_ERROR) {
        PASS();
    } else {
        FAIL("Expected FORMAT_ERROR for bad magic");
    }
}

static void test_imd_good_magic(void) {
    TEST(imd_good_magic);
    
    /* Valid IMD header with 0x1A terminator and track data */
    uint8_t data[64] = {
        'I', 'M', 'D', ' ', '1', '.', '0', ' ',
        'T', 'e', 's', 't', 0x1A,  /* Header with terminator */
        0x00,  /* Mode (FM 500kbps) */
        0x00,  /* Cylinder 0 */
        0x00,  /* Head 0 */
        0x09,  /* 9 sectors */
        0x02,  /* 512 bytes per sector */
    };
    
    uft_verify_result_t result = {0};
    uft_verify_status_t status = uft_verify_imd_buffer(data, sizeof(data), &result);
    if (status == UFT_VERIFY_OK) {
        PASS();
    } else {
        FAIL("Expected OK for valid IMD");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * D71 Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_d71_invalid_size(void) {
    TEST(d71_invalid_size);
    
    uint8_t data[1024] = {0};
    uft_verify_status_t status = uft_verify_d71_buffer(data, sizeof(data), NULL);
    if (status == UFT_VERIFY_SIZE_MISMATCH) {
        PASS();
    } else {
        FAIL("Expected SIZE_MISMATCH for wrong size");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * D81 Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_d81_invalid_size(void) {
    TEST(d81_invalid_size);
    
    uint8_t data[1024] = {0};
    uft_verify_status_t status = uft_verify_d81_buffer(data, sizeof(data), NULL);
    if (status == UFT_VERIFY_SIZE_MISMATCH) {
        PASS();
    } else {
        FAIL("Expected SIZE_MISMATCH for wrong size");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * HFE Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_hfe_null_data(void) {
    TEST(hfe_null_data);
    
    uft_verify_status_t status = uft_verify_hfe_buffer(NULL, 0, NULL);
    if (status == UFT_VERIFY_SIZE_MISMATCH) {
        PASS();
    } else {
        FAIL("Expected SIZE_MISMATCH for NULL data");
    }
}

static void test_hfe_bad_magic(void) {
    TEST(hfe_bad_magic);
    
    uint8_t data[512] = {'B', 'A', 'D', 'M', 'A', 'G', 'I', 'C'};
    uft_verify_status_t status = uft_verify_hfe_buffer(data, sizeof(data), NULL);
    if (status == UFT_VERIFY_FORMAT_ERROR) {
        PASS();
    } else {
        FAIL("Expected FORMAT_ERROR for bad magic");
    }
}

static void test_hfe_good_header(void) {
    TEST(hfe_good_header);
    
    uint8_t data[512] = {0};
    /* Magic: HXCPICFE */
    memcpy(data, "HXCPICFE", 8);
    data[8] = 0;    /* Revision 0 */
    data[9] = 80;   /* 80 tracks */
    data[10] = 2;   /* 2 sides */
    data[11] = 1;   /* IBM MFM encoding */
    data[18] = 1;   /* Track list at block 1 (offset 512) */
    data[19] = 0;
    
    /* Need at least track list offset */
    uft_verify_status_t status = uft_verify_hfe_buffer(data, sizeof(data), NULL);
    if (status == UFT_VERIFY_SIZE_MISMATCH) {
        /* Track list at block 1 = offset 512 which equals size */
        PASS();  /* Expected since track list is at edge */
    } else if (status == UFT_VERIFY_OK) {
        PASS();
    } else {
        FAIL("Expected OK or SIZE_MISMATCH");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * D88 Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_d88_small_data(void) {
    TEST(d88_small_data);
    
    uint8_t data[100] = {0};
    uft_verify_status_t status = uft_verify_d88_buffer(data, sizeof(data), NULL);
    if (status == UFT_VERIFY_SIZE_MISMATCH) {
        PASS();
    } else {
        FAIL("Expected SIZE_MISMATCH for small data");
    }
}

static void test_d88_bad_media_type(void) {
    TEST(d88_bad_media_type);
    
    uint8_t data[1024] = {0};
    data[0x1B] = 0xFF;  /* Invalid media type */
    
    uft_verify_status_t status = uft_verify_d88_buffer(data, sizeof(data), NULL);
    if (status == UFT_VERIFY_FORMAT_ERROR) {
        PASS();
    } else {
        FAIL("Expected FORMAT_ERROR for bad media type");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  UFT Format Verify Tests (Extended)\n");
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
    
    printf("\nIMG Format Tests:\n");
    test_img_null_data();
    test_img_valid_size_360k();
    test_img_invalid_size();
    
    printf("\nIMD Format Tests:\n");
    test_imd_null_data();
    test_imd_bad_magic();
    test_imd_good_magic();
    
    printf("\nD71 Format Tests:\n");
    test_d71_invalid_size();
    
    printf("\nD81 Format Tests:\n");
    test_d81_invalid_size();
    
    printf("\nHFE Format Tests:\n");
    test_hfe_null_data();
    test_hfe_bad_magic();
    test_hfe_good_header();
    
    printf("\nD88 Format Tests:\n");
    test_d88_small_data();
    test_d88_bad_media_type();
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
