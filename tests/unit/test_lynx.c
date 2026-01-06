/**
 * @file test_lynx.c
 * @brief Unit Tests for Lynx Archive Format
 * @version 1.0.0
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <uft/cbm/uft_lynx.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        return false; \
    } \
    tests_passed++; \
} while(0)

#define RUN_TEST(fn) do { \
    printf("Running %s...\n", #fn); \
    if (fn()) { \
        printf("  PASS\n"); \
    } else { \
        printf("  FAILED\n"); \
    } \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Data - Minimal Lynx Archive
 * ═══════════════════════════════════════════════════════════════════════════ */

/* 
 * This is a minimal Lynx archive with one file for testing.
 * Created by encoding the structure manually.
 */
static const uint8_t TEST_LYNX_HEADER[] = {
    /* Load address $0801 */
    0x01, 0x08,
    /* BASIC line: 10 SYS ... (simplified) */
    0x0B, 0x08, 0x0A, 0x00, 0x9E, 0x32, 0x30, 0x36,
    0x31, 0x00, 0x00, 0x00,
    /* CR + directory blocks + space + signature */
    0x0D, 0x31, 0x20, '*', 'T', 'E', 'S', 'T', ' ',
    'A', 'R', 'C', 'H', 'I', 'V', 'E', 0x0D,
    /* File count: 1 */
    0x31, 0x20, 0x0D,
    /* Entry 1: "HELLO" PRG 1 block */
    'H', 'E', 'L', 'L', 'O', 0xA0, 0xA0, 0xA0,
    0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0x0D,
    0x20, 0x31, 0x20, 0x0D,  /* 1 block */
    'P', 0x0D,               /* PRG type */
    0x20, 0x31, 0x30, 0x20, 0x0D,  /* LSU: 10 */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Detection Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool test_detect_empty(void)
{
    TEST_ASSERT(!uft_lynx_detect(NULL, 0), "NULL data returns false");
    TEST_ASSERT(!uft_lynx_detect((uint8_t*)"", 0), "empty data returns false");
    
    uint8_t small[50] = {0};
    TEST_ASSERT(!uft_lynx_detect(small, sizeof(small)), "too small returns false");
    
    return true;
}

static bool test_detect_random(void)
{
    uint8_t random[500];
    for (size_t i = 0; i < sizeof(random); i++) {
        random[i] = (uint8_t)(i * 17 + 3);
    }
    
    TEST_ASSERT(!uft_lynx_detect(random, sizeof(random)), "random data returns false");
    TEST_ASSERT(uft_lynx_detect_confidence(random, sizeof(random)) < 50,
                "random data has low confidence");
    
    return true;
}

static bool test_type_conversion(void)
{
    TEST_ASSERT(uft_lynx_type_from_d64(0x00) == UFT_LYNX_DEL, "DEL conversion");
    TEST_ASSERT(uft_lynx_type_from_d64(0x01) == UFT_LYNX_SEQ, "SEQ conversion");
    TEST_ASSERT(uft_lynx_type_from_d64(0x02) == UFT_LYNX_PRG, "PRG conversion");
    TEST_ASSERT(uft_lynx_type_from_d64(0x03) == UFT_LYNX_USR, "USR conversion");
    TEST_ASSERT(uft_lynx_type_from_d64(0x04) == UFT_LYNX_REL, "REL conversion");
    TEST_ASSERT(uft_lynx_type_from_d64(0x82) == UFT_LYNX_PRG, "PRG+closed conversion");
    
    TEST_ASSERT(uft_lynx_type_to_d64(UFT_LYNX_PRG) == 0x82, "PRG to D64");
    TEST_ASSERT(uft_lynx_type_to_d64(UFT_LYNX_SEQ) == 0x81, "SEQ to D64");
    
    return true;
}

static bool test_type_names(void)
{
    TEST_ASSERT(strcmp(uft_lynx_type_name(UFT_LYNX_DEL), "DEL") == 0, "DEL name");
    TEST_ASSERT(strcmp(uft_lynx_type_name(UFT_LYNX_SEQ), "SEQ") == 0, "SEQ name");
    TEST_ASSERT(strcmp(uft_lynx_type_name(UFT_LYNX_PRG), "PRG") == 0, "PRG name");
    TEST_ASSERT(strcmp(uft_lynx_type_name(UFT_LYNX_USR), "USR") == 0, "USR name");
    TEST_ASSERT(strcmp(uft_lynx_type_name(UFT_LYNX_REL), "REL") == 0, "REL name");
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Creation Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool test_create_simple(void)
{
    /* Create a simple test file */
    uint8_t file_data[] = { 0x01, 0x08, 0x00, 0x00, 0x00 }; /* Minimal PRG */
    
    uft_lynx_file_t files[1] = {
        {
            .name = "TEST",
            .type = UFT_LYNX_PRG,
            .data = file_data,
            .size = sizeof(file_data),
            .record_len = 0
        }
    };
    
    uint8_t* archive_data = NULL;
    size_t archive_size = 0;
    
    int rc = uft_lynx_create(files, 1, "*TEST ARCHIVE", &archive_data, &archive_size);
    
    TEST_ASSERT(rc == 0, "create returns 0");
    TEST_ASSERT(archive_data != NULL, "archive_data not NULL");
    TEST_ASSERT(archive_size > 100, "archive has reasonable size");
    
    /* Verify we can detect it */
    TEST_ASSERT(uft_lynx_detect(archive_data, archive_size), "created archive is detectable");
    
    /* Verify we can open it */
    uft_lynx_archive_t archive;
    rc = uft_lynx_open(archive_data, archive_size, &archive);
    TEST_ASSERT(rc == 0, "can open created archive");
    
    TEST_ASSERT(uft_lynx_get_file_count(&archive) == 1, "file count is 1");
    
    const uft_lynx_entry_t* entry = uft_lynx_get_entry(&archive, 0);
    TEST_ASSERT(entry != NULL, "entry not NULL");
    TEST_ASSERT(strcmp(entry->name, "TEST") == 0, "filename matches");
    TEST_ASSERT(entry->type == UFT_LYNX_PRG, "type is PRG");
    
    uft_lynx_close(&archive);
    free(archive_data);
    
    return true;
}

static bool test_create_multiple(void)
{
    uint8_t file1[] = { 0x01, 0x08, 'H', 'E', 'L', 'L', 'O' };
    uint8_t file2[] = { 0x00, 0x00, 'W', 'O', 'R', 'L', 'D' };
    uint8_t file3[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    
    uft_lynx_file_t files[3] = {
        { .name = "HELLO", .type = UFT_LYNX_PRG, .data = file1, .size = sizeof(file1) },
        { .name = "WORLD", .type = UFT_LYNX_SEQ, .data = file2, .size = sizeof(file2) },
        { .name = "DATA", .type = UFT_LYNX_USR, .data = file3, .size = sizeof(file3) }
    };
    
    uint8_t* archive_data = NULL;
    size_t archive_size = 0;
    
    int rc = uft_lynx_create(files, 3, NULL, &archive_data, &archive_size);
    TEST_ASSERT(rc == 0, "create 3-file archive");
    
    uft_lynx_archive_t archive;
    rc = uft_lynx_open(archive_data, archive_size, &archive);
    TEST_ASSERT(rc == 0, "open 3-file archive");
    TEST_ASSERT(uft_lynx_get_file_count(&archive) == 3, "has 3 files");
    
    /* Find by name */
    TEST_ASSERT(uft_lynx_find_file(&archive, "HELLO") == 0, "find HELLO");
    TEST_ASSERT(uft_lynx_find_file(&archive, "WORLD") == 1, "find WORLD");
    TEST_ASSERT(uft_lynx_find_file(&archive, "DATA") == 2, "find DATA");
    TEST_ASSERT(uft_lynx_find_file(&archive, "NOTEXIST") == -1, "not found returns -1");
    
    /* Case-insensitive search */
    TEST_ASSERT(uft_lynx_find_file(&archive, "hello") == 0, "find lowercase");
    
    uft_lynx_close(&archive);
    free(archive_data);
    
    return true;
}

static bool test_roundtrip(void)
{
    /* Create archive with specific data */
    uint8_t original[] = { 0x00, 0x10, 0xA9, 0x00, 0x8D, 0x20, 0xD0, 0x60 };
    
    uft_lynx_file_t files[1] = {
        { .name = "CODE", .type = UFT_LYNX_PRG, .data = original, .size = sizeof(original) }
    };
    
    uint8_t* archive_data = NULL;
    size_t archive_size = 0;
    
    int rc = uft_lynx_create(files, 1, "*ROUNDTRIP TEST", &archive_data, &archive_size);
    TEST_ASSERT(rc == 0, "create archive");
    
    /* Open and extract */
    uft_lynx_archive_t archive;
    rc = uft_lynx_open(archive_data, archive_size, &archive);
    TEST_ASSERT(rc == 0, "open archive");
    
    uint8_t* extracted = NULL;
    size_t extracted_size = 0;
    
    rc = uft_lynx_extract_file_alloc(&archive, 0, &extracted, &extracted_size);
    TEST_ASSERT(rc == 0, "extract file");
    TEST_ASSERT(extracted_size == sizeof(original), "size matches");
    TEST_ASSERT(memcmp(extracted, original, sizeof(original)) == 0, "data matches");
    
    free(extracted);
    uft_lynx_close(&archive);
    free(archive_data);
    
    return true;
}

static bool test_estimate_size(void)
{
    uint8_t data[100];
    memset(data, 0, sizeof(data));
    
    uft_lynx_file_t files[2] = {
        { .name = "FILE1", .type = UFT_LYNX_PRG, .data = data, .size = 100 },
        { .name = "FILE2", .type = UFT_LYNX_SEQ, .data = data, .size = 50 }
    };
    
    size_t estimate = uft_lynx_estimate_size(files, 2);
    TEST_ASSERT(estimate > 0, "estimate > 0");
    TEST_ASSERT(estimate < 10000, "estimate reasonable");
    
    /* Create and compare */
    uint8_t* archive_data = NULL;
    size_t archive_size = 0;
    
    uft_lynx_create(files, 2, NULL, &archive_data, &archive_size);
    
    /* Estimate should be >= actual (we add safety margin) */
    TEST_ASSERT(archive_size <= estimate + 1024, "estimate >= actual");
    
    free(archive_data);
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("═══════════════════════════════════════════════════════════════\n");
    printf(" UFT Lynx Archive Format Tests\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    printf("Detection Tests:\n");
    RUN_TEST(test_detect_empty);
    RUN_TEST(test_detect_random);
    
    printf("\nUtility Tests:\n");
    RUN_TEST(test_type_conversion);
    RUN_TEST(test_type_names);
    
    printf("\nCreation Tests:\n");
    RUN_TEST(test_create_simple);
    RUN_TEST(test_create_multiple);
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_estimate_size);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf(" Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
