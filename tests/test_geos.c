/**
 * @file test_geos.c
 * @brief Unit tests for GEOS Filesystem Support
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_geos.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(condition) do { \
    if (!(condition)) { \
        printf("FAILED at line %d: %s\n", __LINE__, #condition); \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT((x))
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/**
 * @brief Create test info block
 */
static void create_test_info_block(uint8_t *data)
{
    memset(data, 0, 256);
    
    /* Info block ID */
    data[0] = 0x00;
    data[1] = 0xFF;
    data[2] = 0x00;
    
    /* Icon (24x21) */
    data[3] = 3;   /* Width in bytes */
    data[4] = 21;  /* Height */
    /* Simple icon pattern */
    for (int i = 0; i < 63; i++) {
        data[5 + i] = (i % 3 == 1) ? 0xFF : 0x00;
    }
    
    /* File type info */
    data[68] = 0x83;                /* USR */
    data[69] = GEOS_TYPE_APPLICATION;
    data[70] = GEOS_STRUCT_SEQ;
    data[71] = 0x00; data[72] = 0x08;  /* Load $0800 */
    data[73] = 0xFF; data[74] = 0x9F;  /* End $9FFF */
    data[75] = 0x00; data[76] = 0x08;  /* Exec $0800 */
    
    /* Class name */
    memcpy(data + 77, "Test App", 8);
    
    /* Author */
    memcpy(data + 97, "Test Author", 11);
    
    /* Timestamp */
    data[161] = 126;  /* Year 2026 */
    data[162] = 1;    /* January */
    data[163] = 17;   /* 17th */
    data[164] = 12;   /* 12:00 */
    data[165] = 0;
}

/* ============================================================================
 * Unit Tests - Detection
 * ============================================================================ */

TEST(type_name)
{
    ASSERT_STR_EQ(geos_type_name(GEOS_TYPE_NON_GEOS), "Non-GEOS");
    ASSERT_STR_EQ(geos_type_name(GEOS_TYPE_APPLICATION), "Application");
    ASSERT_STR_EQ(geos_type_name(GEOS_TYPE_DESK_ACC), "Desk Accessory");
    ASSERT_STR_EQ(geos_type_name(GEOS_TYPE_FONT), "Font");
}

TEST(structure_name)
{
    ASSERT_STR_EQ(geos_structure_name(GEOS_STRUCT_SEQ), "Sequential");
    ASSERT_STR_EQ(geos_structure_name(GEOS_STRUCT_VLIR), "VLIR");
}

/* ============================================================================
 * Unit Tests - Info Block
 * ============================================================================ */

TEST(parse_info)
{
    uint8_t block[256];
    create_test_info_block(block);
    
    geos_info_t info;
    int ret = geos_parse_info(block, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.geos_type, GEOS_TYPE_APPLICATION);
    ASSERT_EQ(info.structure, GEOS_STRUCT_SEQ);
    ASSERT_EQ(info.load_address, 0x0800);
    ASSERT_STR_EQ(info.class_name, "Test App");
    ASSERT_STR_EQ(info.author, "Test Author");
}

TEST(write_info)
{
    geos_info_t info;
    memset(&info, 0, sizeof(info));
    
    info.geos_type = GEOS_TYPE_DATA;
    info.structure = GEOS_STRUCT_VLIR;
    info.load_address = 0x1000;
    strcpy(info.class_name, "My Class");
    strcpy(info.author, "Me");
    
    uint8_t block[256];
    int ret = geos_write_info(&info, block);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(block[0], 0x00);
    ASSERT_EQ(block[1], 0xFF);
    ASSERT_EQ(block[2], 0x00);
    ASSERT_EQ(block[69], GEOS_TYPE_DATA);
    ASSERT_EQ(block[70], GEOS_STRUCT_VLIR);
}

TEST(format_timestamp)
{
    geos_timestamp_t ts = {126, 1, 17, 14, 30};  /* 2026-01-17 14:30 */
    
    char buffer[32];
    geos_format_timestamp(&ts, buffer);
    
    ASSERT_STR_EQ(buffer, "2026-01-17 14:30");
}

/* ============================================================================
 * Unit Tests - VLIR
 * ============================================================================ */

TEST(parse_vlir_index)
{
    uint8_t index[254];
    memset(index, 0, 254);
    
    /* Record 0: Track 5, Sector 10 */
    index[0] = 5; index[1] = 10;
    /* Record 1: Track 5, Sector 11 */
    index[2] = 5; index[3] = 11;
    /* Record 2: Empty */
    index[4] = 0; index[5] = 0;
    /* Record 3: Deleted */
    index[6] = 0xFF; index[7] = 0x00;
    
    geos_vlir_record_t records[127];
    int num_records;
    int ret = geos_parse_vlir_index(index, records, &num_records);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(records[0].track, 5);
    ASSERT_EQ(records[0].sector, 10);
    ASSERT_TRUE(geos_vlir_record_empty(&records[2]));
    ASSERT_TRUE(geos_vlir_record_deleted(&records[3]));
}

TEST(vlir_record_empty)
{
    geos_vlir_record_t empty = {0, 0, 0, NULL};
    geos_vlir_record_t valid = {5, 10, 256, NULL};
    
    ASSERT_TRUE(geos_vlir_record_empty(&empty));
    ASSERT_FALSE(geos_vlir_record_empty(&valid));
}

TEST(vlir_record_deleted)
{
    geos_vlir_record_t deleted = {0xFF, 0, 0, NULL};
    geos_vlir_record_t valid = {5, 10, 256, NULL};
    
    ASSERT_TRUE(geos_vlir_record_deleted(&deleted));
    ASSERT_FALSE(geos_vlir_record_deleted(&valid));
}

/* ============================================================================
 * Unit Tests - File Operations
 * ============================================================================ */

TEST(file_create_seq)
{
    geos_file_t file;
    int ret = geos_file_create("TESTFILE", GEOS_TYPE_DATA, false, &file);
    
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(file.filename, "TESTFILE");
    ASSERT_EQ(file.info.geos_type, GEOS_TYPE_DATA);
    ASSERT_EQ(file.info.structure, GEOS_STRUCT_SEQ);
    ASSERT_FALSE(file.is_vlir);
    
    geos_file_free(&file);
}

TEST(file_create_vlir)
{
    geos_file_t file;
    int ret = geos_file_create("VLIRFILE", GEOS_TYPE_APPLICATION, true, &file);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(file.info.structure, GEOS_STRUCT_VLIR);
    ASSERT_TRUE(file.is_vlir);
    
    geos_file_free(&file);
}

TEST(file_set_icon)
{
    geos_file_t file;
    geos_file_create("ICONTEST", GEOS_TYPE_APPLICATION, false, &file);
    
    uint8_t icon[63];
    memset(icon, 0xAA, 63);
    
    geos_file_set_icon(&file, icon);
    
    ASSERT_EQ(file.info.icon.width, 3);
    ASSERT_EQ(file.info.icon.height, 21);
    ASSERT_EQ(file.info.icon.data[0], 0xAA);
    
    geos_file_free(&file);
}

TEST(file_set_description)
{
    geos_file_t file;
    geos_file_create("DESCTEST", GEOS_TYPE_APPLICATION, false, &file);
    
    geos_file_set_description(&file, "MyClass", "MyAuthor", "A test file");
    
    ASSERT_STR_EQ(file.info.class_name, "MyClass");
    ASSERT_STR_EQ(file.info.author, "MyAuthor");
    
    geos_file_free(&file);
}

/* ============================================================================
 * Unit Tests - CVT Format
 * ============================================================================ */

TEST(cvt_detect)
{
    uint8_t valid[400];
    memset(valid, 0, sizeof(valid));
    memcpy(valid, CVT_MAGIC, CVT_MAGIC_LEN);
    
    ASSERT_TRUE(geos_cvt_detect(valid, sizeof(valid)));
    
    uint8_t invalid[400] = {0};
    ASSERT_FALSE(geos_cvt_detect(invalid, sizeof(invalid)));
}

TEST(cvt_create)
{
    geos_file_t file;
    geos_file_create("CVTTEST", GEOS_TYPE_DATA, false, &file);
    geos_file_set_description(&file, "TestClass", "TestAuthor", "Test desc");
    
    /* Add some data */
    file.seq_data = malloc(100);
    file.seq_size = 100;
    memset(file.seq_data, 0x42, 100);
    
    uint8_t cvt[1024];
    size_t cvt_size;
    int ret = geos_cvt_create(&file, cvt, sizeof(cvt), &cvt_size);
    
    ASSERT_EQ(ret, 0);
    ASSERT(cvt_size > 316);
    ASSERT(memcmp(cvt, CVT_MAGIC, CVT_MAGIC_LEN) == 0);
    
    geos_file_free(&file);
}

TEST(cvt_roundtrip)
{
    /* Create file */
    geos_file_t file1;
    geos_file_create("ROUNDTRIP", GEOS_TYPE_APPLICATION, false, &file1);
    geos_file_set_description(&file1, "RoundTrip", "Tester", "Roundtrip test");
    
    file1.seq_data = malloc(50);
    file1.seq_size = 50;
    for (int i = 0; i < 50; i++) file1.seq_data[i] = i;
    
    /* Convert to CVT */
    uint8_t cvt[1024];
    size_t cvt_size;
    geos_cvt_create(&file1, cvt, sizeof(cvt), &cvt_size);
    
    /* Parse back */
    geos_file_t file2;
    int ret = geos_cvt_parse(cvt, cvt_size, &file2);
    
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(file2.filename, "ROUNDTRIP");
    ASSERT_EQ(file2.info.geos_type, GEOS_TYPE_APPLICATION);
    
    geos_file_free(&file1);
    geos_file_free(&file2);
}

/* ============================================================================
 * Unit Tests - Utilities
 * ============================================================================ */

TEST(get_default_icon)
{
    geos_icon_t icon;
    geos_get_default_icon(GEOS_TYPE_APPLICATION, &icon);
    
    ASSERT_EQ(icon.width, 3);
    ASSERT_EQ(icon.height, 21);
    /* Should have some non-zero data */
    ASSERT_NE(icon.data[0], 0);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== GEOS Filesystem Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(type_name);
    RUN_TEST(structure_name);
    
    printf("\nInfo Block:\n");
    RUN_TEST(parse_info);
    RUN_TEST(write_info);
    RUN_TEST(format_timestamp);
    
    printf("\nVLIR:\n");
    RUN_TEST(parse_vlir_index);
    RUN_TEST(vlir_record_empty);
    RUN_TEST(vlir_record_deleted);
    
    printf("\nFile Operations:\n");
    RUN_TEST(file_create_seq);
    RUN_TEST(file_create_vlir);
    RUN_TEST(file_set_icon);
    RUN_TEST(file_set_description);
    
    printf("\nCVT Format:\n");
    RUN_TEST(cvt_detect);
    RUN_TEST(cvt_create);
    RUN_TEST(cvt_roundtrip);
    
    printf("\nUtilities:\n");
    RUN_TEST(get_default_icon);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
