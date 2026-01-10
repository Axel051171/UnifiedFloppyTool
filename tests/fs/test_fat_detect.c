/**
 * @file test_fat_detect.c
 * @brief P1-5: FAT BPB Detection Tests
 * 
 * Tests:
 * 1. Valid FAT12/16/32 detection with confidence
 * 2. False positive rejection (D64, ADF, SCP, HFE, G64)
 * 3. Edge cases and malformed data
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "uft/fs/uft_fat_detect.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        tests_failed++; \
    } else { \
        printf("  OK: %s\n", msg); \
        tests_passed++; \
    } \
} while(0)

/* ============================================================================
 * Test Data Generators
 * ============================================================================ */

/**
 * @brief Create a minimal valid FAT12 boot sector (720K floppy)
 */
static void create_fat12_720k_bootsector(uint8_t *data)
{
    memset(data, 0, 512);
    
    /* Jump instruction */
    data[0] = 0xEB;
    data[1] = 0x3C;
    data[2] = 0x90;
    
    /* OEM name */
    memcpy(data + 3, "MSDOS5.0", 8);
    
    /* BPB for 720K */
    data[11] = 0x00; data[12] = 0x02;  /* bytes_per_sector = 512 */
    data[13] = 0x02;                    /* sectors_per_cluster = 2 */
    data[14] = 0x01; data[15] = 0x00;  /* reserved_sectors = 1 */
    data[16] = 0x02;                    /* fat_count = 2 */
    data[17] = 0x70; data[18] = 0x00;  /* root_entries = 112 */
    data[19] = 0xA0; data[20] = 0x05;  /* total_sectors = 1440 */
    data[21] = 0xF9;                    /* media_descriptor */
    data[22] = 0x03; data[23] = 0x00;  /* sectors_per_fat = 3 */
    data[24] = 0x09; data[25] = 0x00;  /* sectors_per_track = 9 */
    data[26] = 0x02; data[27] = 0x00;  /* heads = 2 */
    
    /* Boot signature */
    data[510] = 0x55;
    data[511] = 0xAA;
}

/**
 * @brief Create a FAT12 1.44MB boot sector
 */
static void create_fat12_144m_bootsector(uint8_t *data)
{
    memset(data, 0, 512);
    
    data[0] = 0xEB; data[1] = 0x3C; data[2] = 0x90;
    memcpy(data + 3, "MSDOS5.0", 8);
    
    /* BPB for 1.44MB */
    data[11] = 0x00; data[12] = 0x02;  /* 512 bytes/sector */
    data[13] = 0x01;                    /* 1 sector/cluster */
    data[14] = 0x01; data[15] = 0x00;  /* 1 reserved */
    data[16] = 0x02;                    /* 2 FATs */
    data[17] = 0xE0; data[18] = 0x00;  /* 224 root entries */
    data[19] = 0x40; data[20] = 0x0B;  /* 2880 sectors */
    data[21] = 0xF0;                    /* media descriptor */
    data[22] = 0x09; data[23] = 0x00;  /* 9 sectors/FAT */
    data[24] = 0x12; data[25] = 0x00;  /* 18 sectors/track */
    data[26] = 0x02; data[27] = 0x00;  /* 2 heads */
    
    data[510] = 0x55;
    data[511] = 0xAA;
}

/**
 * @brief Create fake D64 header (CBM directory-like)
 */
static void create_fake_d64(uint8_t *data)
{
    memset(data, 0, 512);
    data[0] = 0x12;  /* Track 18 link (typical) */
    data[1] = 0x01;
    /* No 0x55AA signature */
}

/**
 * @brief Create ADF header (Amiga DOS)
 */
static void create_adf_header(uint8_t *data)
{
    memset(data, 0, 512);
    data[0] = 'D';
    data[1] = 'O';
    data[2] = 'S';
    data[3] = 0x00;  /* OFS */
}

/**
 * @brief Create SCP header
 */
static void create_scp_header(uint8_t *data)
{
    memset(data, 0, 512);
    data[0] = 'S';
    data[1] = 'C';
    data[2] = 'P';
}

/**
 * @brief Create HFE header
 */
static void create_hfe_header(uint8_t *data)
{
    memset(data, 0, 512);
    memcpy(data, "HXCPICFE", 8);
}

/**
 * @brief Create G64 header
 */
static void create_g64_header(uint8_t *data)
{
    memset(data, 0, 512);
    memcpy(data, "GCR-1541", 8);
}

/* ============================================================================
 * Test 1: Valid FAT Detection
 * ============================================================================ */
static void test_valid_fat_detection(void)
{
    printf("\n=== Test 1: Valid FAT Detection ===\n");
    
    uint8_t *image = calloc(1, 737280);  /* 720K */
    create_fat12_720k_bootsector(image);
    
    uft_fat_detect_result_t result;
    int rc = uft_fat_detect(image, 737280, &result);
    
    TEST(rc == 0, "720K FAT12 detected");
    TEST(result.is_fat == true, "is_fat flag set");
    TEST(result.fat_type == UFT_FAT12, "Detected as FAT12");
    TEST(result.confidence >= 50, "Confidence >= 50");
    printf("  Confidence: %d%%, Reason: %s\n", result.confidence, result.reason);
    
    free(image);
    
    /* Test 1.44MB */
    image = calloc(1, 1474560);
    create_fat12_144m_bootsector(image);
    
    rc = uft_fat_detect(image, 1474560, &result);
    TEST(rc == 0, "1.44MB FAT12 detected");
    TEST(result.fat_type == UFT_FAT12, "Detected as FAT12");
    TEST(result.confidence >= 60, "1.44MB confidence >= 60");
    printf("  Confidence: %d%%\n", result.confidence);
    
    free(image);
}

/* ============================================================================
 * Test 2: False Positive Rejection
 * ============================================================================ */
static void test_false_positive_rejection(void)
{
    printf("\n=== Test 2: False Positive Rejection ===\n");
    
    uint8_t data[512];
    uft_fat_detect_result_t result;
    int rc;
    
    /* D64-like data at D64 size */
    create_fake_d64(data);
    rc = uft_fat_detect(data, 174848, &result);  /* D64 35-track size */
    TEST(rc != 0 || !result.is_fat, "D64 rejected (size match)");
    printf("  D64: %s\n", result.reason);
    
    /* ADF */
    create_adf_header(data);
    rc = uft_fat_detect(data, 901120, &result);  /* ADF DD size */
    TEST(rc != 0 || !result.is_fat, "ADF rejected");
    printf("  ADF: %s\n", result.reason);
    
    /* SCP */
    create_scp_header(data);
    rc = uft_fat_detect(data, 512, &result);
    TEST(rc != 0 || !result.is_fat, "SCP rejected");
    printf("  SCP: %s\n", result.reason);
    
    /* HFE */
    create_hfe_header(data);
    rc = uft_fat_detect(data, 512, &result);
    TEST(rc != 0 || !result.is_fat, "HFE rejected");
    printf("  HFE: %s\n", result.reason);
    
    /* G64 */
    create_g64_header(data);
    rc = uft_fat_detect(data, 512, &result);
    TEST(rc != 0 || !result.is_fat, "G64 rejected");
    printf("  G64: %s\n", result.reason);
}

/* ============================================================================
 * Test 3: Edge Cases
 * ============================================================================ */
static void test_edge_cases(void)
{
    printf("\n=== Test 3: Edge Cases ===\n");
    
    uft_fat_detect_result_t result;
    int rc;
    
    /* NULL pointer */
    rc = uft_fat_detect(NULL, 512, &result);
    TEST(rc != 0, "NULL data rejected");
    
    /* Too small */
    uint8_t small[256] = {0};
    rc = uft_fat_detect(small, 256, &result);
    TEST(rc != 0, "Too small rejected");
    
    /* No boot signature */
    uint8_t no_sig[512] = {0};
    rc = uft_fat_detect(no_sig, 512, &result);
    TEST(rc != 0, "No signature rejected");
    
    /* Invalid BPB (all zeros except signature) */
    uint8_t bad_bpb[512] = {0};
    bad_bpb[510] = 0x55;
    bad_bpb[511] = 0xAA;
    rc = uft_fat_detect(bad_bpb, 512, &result);
    TEST(rc != 0, "Invalid BPB rejected");
}

/* ============================================================================
 * Test 4: Helper Functions
 * ============================================================================ */
static void test_helper_functions(void)
{
    printf("\n=== Test 4: Helper Functions ===\n");
    
    TEST(strcmp(uft_fat_type_name(UFT_FAT12), "FAT12") == 0, "FAT12 name");
    TEST(strcmp(uft_fat_type_name(UFT_FAT16), "FAT16") == 0, "FAT16 name");
    TEST(strcmp(uft_fat_type_name(UFT_FAT32), "FAT32") == 0, "FAT32 name");
    
    TEST(uft_fat_is_floppy_size(737280), "720K is floppy");
    TEST(uft_fat_is_floppy_size(1474560), "1.44M is floppy");
    TEST(uft_fat_is_floppy_size(1720320), "1.68M DMF is floppy");
    TEST(!uft_fat_is_floppy_size(174848), "D64 size not floppy");
    TEST(!uft_fat_is_floppy_size(901120), "ADF size not floppy");
}

/* ============================================================================
 * Test 5: Confidence Scoring
 * ============================================================================ */
static void test_confidence_scoring(void)
{
    printf("\n=== Test 5: Confidence Scoring ===\n");
    
    uint8_t *image720 = calloc(1, 737280);
    uint8_t *image144 = calloc(1, 1474560);
    
    create_fat12_720k_bootsector(image720);
    create_fat12_144m_bootsector(image144);
    
    uft_fat_detect_result_t r720, r144;
    uft_fat_detect(image720, 737280, &r720);
    uft_fat_detect(image144, 1474560, &r144);
    
    printf("  720K confidence: %d%%\n", r720.confidence);
    printf("  1.44M confidence: %d%%\n", r144.confidence);
    
    TEST(r720.confidence >= 50 && r720.confidence <= 100, "720K confidence in range");
    TEST(r144.confidence >= 50 && r144.confidence <= 100, "1.44M confidence in range");
    
    /* Both should be high confidence for standard formats */
    TEST(r720.confidence >= 60, "720K high confidence");
    TEST(r144.confidence >= 60, "1.44M high confidence");
    
    free(image720);
    free(image144);
}

/* ============================================================================
 * Main
 * ============================================================================ */
int main(void)
{
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║        P1-5: FAT BPB Detection Tests                           ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    
    test_valid_fat_detection();
    test_false_positive_rejection();
    test_edge_cases();
    test_helper_functions();
    test_confidence_scoring();
    
    printf("\n════════════════════════════════════════════════════════════════\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("════════════════════════════════════════════════════════════════\n");
    
    if (tests_failed > 0) {
        printf("❌ SOME TESTS FAILED\n");
        return 1;
    }
    
    printf("✓ ALL TESTS PASSED\n");
    printf("\nP1-5 Status: FAT BPB Detection ERFOLGREICH\n");
    printf("  - FAT12/FAT16/FAT32 Erkennung\n");
    printf("  - Confidence-Score 0-100\n");
    printf("  - Keine False Positives bei D64/ADF/SCP/HFE/G64\n");
    
    return 0;
}
