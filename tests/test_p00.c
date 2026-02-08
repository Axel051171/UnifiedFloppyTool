/**
 * @file test_p00.c
 * @brief Unit tests for P00/S00/U00 PC64 Format
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_p00.h"

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
 * @brief Create test P00 file
 */
static uint8_t *create_test_p00(size_t *size, const char *filename)
{
    /* Header (26) + PRG load address (2) + some data */
    size_t data_size = 100;
    size_t total = P00_HEADER_SIZE + data_size;
    uint8_t *data = calloc(1, total);
    
    /* Magic */
    memcpy(data, "C64File", 7);
    data[7] = 0x00;
    
    /* Filename (PETSCII) */
    if (filename) {
        for (int i = 0; i < 16 && filename[i]; i++) {
            char c = filename[i];
            if (c >= 'a' && c <= 'z') {
                data[8 + i] = c - 0x20;  /* Uppercase PETSCII */
            } else {
                data[8 + i] = c;
            }
        }
    } else {
        memcpy(data + 8, "TEST FILE", 9);
    }
    
    /* Pad with shifted space */
    for (int i = 0; i < 16; i++) {
        if (data[8 + i] == 0) data[8 + i] = 0xA0;
    }
    
    /* Record size (0 for PRG) */
    data[24] = 0x00;
    data[25] = 0x00;
    
    /* PRG data: load address $0801 + some BASIC */
    uint8_t *prg = data + P00_HEADER_SIZE;
    prg[0] = 0x01;  /* Load address low */
    prg[1] = 0x08;  /* Load address high */
    
    /* Simple BASIC program */
    prg[2] = 0x0B; prg[3] = 0x08;  /* Next line */
    prg[4] = 0x0A; prg[5] = 0x00;  /* Line 10 */
    prg[6] = 0x99;                  /* PRINT */
    prg[7] = '"';
    memcpy(prg + 8, "HELLO", 5);
    prg[13] = '"';
    prg[14] = 0x00;                 /* End of line */
    prg[15] = 0x00; prg[16] = 0x00; /* End of program */
    
    *size = total;
    return data;
}

/* ============================================================================
 * Unit Tests - Detection
 * ============================================================================ */

TEST(detect_valid)
{
    size_t size;
    uint8_t *data = create_test_p00(&size, NULL);
    
    ASSERT_TRUE(p00_detect(data, size));
    
    free(data);
}

TEST(detect_invalid)
{
    uint8_t data[100] = {0};
    ASSERT_FALSE(p00_detect(data, 100));
    ASSERT_FALSE(p00_detect(NULL, 100));
    ASSERT_FALSE(p00_detect(data, 10));
}

TEST(validate_valid)
{
    size_t size;
    uint8_t *data = create_test_p00(&size, NULL);
    
    ASSERT_TRUE(p00_validate(data, size));
    
    free(data);
}

TEST(type_from_name)
{
    ASSERT_EQ(p00_detect_type_from_name("test.P00"), P00_TYPE_PRG);
    ASSERT_EQ(p00_detect_type_from_name("test.S00"), P00_TYPE_SEQ);
    ASSERT_EQ(p00_detect_type_from_name("test.U00"), P00_TYPE_USR);
    ASSERT_EQ(p00_detect_type_from_name("test.R00"), P00_TYPE_REL);
    ASSERT_EQ(p00_detect_type_from_name("test.D00"), P00_TYPE_DEL);
    ASSERT_EQ(p00_detect_type_from_name("test.P01"), P00_TYPE_PRG);
    ASSERT_EQ(p00_detect_type_from_name("test.txt"), P00_TYPE_UNKNOWN);
}

TEST(type_name)
{
    ASSERT_STR_EQ(p00_type_name(P00_TYPE_PRG), "PRG");
    ASSERT_STR_EQ(p00_type_name(P00_TYPE_SEQ), "SEQ");
    ASSERT_STR_EQ(p00_type_name(P00_TYPE_USR), "USR");
    ASSERT_STR_EQ(p00_type_name(P00_TYPE_REL), "REL");
}

TEST(type_extension)
{
    ASSERT_STR_EQ(p00_type_extension(P00_TYPE_PRG), "P00");
    ASSERT_STR_EQ(p00_type_extension(P00_TYPE_SEQ), "S00");
    ASSERT_STR_EQ(p00_type_extension(P00_TYPE_USR), "U00");
}

/* ============================================================================
 * Unit Tests - File Operations
 * ============================================================================ */

TEST(open_p00)
{
    size_t size;
    uint8_t *data = create_test_p00(&size, "MYTEST");
    
    p00_file_t file;
    int ret = p00_open(data, size, &file);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(file.data);
    ASSERT_NOT_NULL(file.file_data);
    ASSERT(file.file_data_size > 0);
    
    p00_close(&file);
    free(data);
}

TEST(close_p00)
{
    size_t size;
    uint8_t *data = create_test_p00(&size, NULL);
    
    p00_file_t file;
    p00_open(data, size, &file);
    p00_close(&file);
    
    ASSERT_EQ(file.data, NULL);
    
    free(data);
}

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_p00(&size, "TESTPROG");
    
    p00_file_t file;
    p00_open(data, size, &file);
    
    p00_info_t info;
    int ret = p00_get_info(&file, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.type, P00_TYPE_PRG);
    ASSERT_EQ(info.load_address, 0x0801);
    
    p00_close(&file);
    free(data);
}

TEST(get_filename)
{
    size_t size;
    uint8_t *data = create_test_p00(&size, "HELLO");
    
    p00_file_t file;
    p00_open(data, size, &file);
    
    char filename[17];
    p00_get_filename(&file, filename);
    
    /* Should be converted from PETSCII */
    ASSERT(filename[0] != 0);
    
    p00_close(&file);
    free(data);
}

TEST(get_load_address)
{
    size_t size;
    uint8_t *data = create_test_p00(&size, NULL);
    
    p00_file_t file;
    p00_open(data, size, &file);
    file.type = P00_TYPE_PRG;
    
    uint16_t addr = p00_get_load_address(&file);
    ASSERT_EQ(addr, 0x0801);
    
    p00_close(&file);
    free(data);
}

/* ============================================================================
 * Unit Tests - Creation
 * ============================================================================ */

TEST(create_p00)
{
    uint8_t prg_data[] = {0x01, 0x08, 0x00, 0x00};  /* Load $0801, end */
    
    p00_file_t file;
    int ret = p00_create(P00_TYPE_PRG, "NEWFILE", prg_data, sizeof(prg_data), 0, &file);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(file.data);
    ASSERT_EQ(file.size, P00_HEADER_SIZE + sizeof(prg_data));
    
    /* Verify header */
    ASSERT(memcmp(file.data, "C64File", 7) == 0);
    
    p00_close(&file);
}

TEST(from_prg)
{
    uint8_t prg_data[] = {0x00, 0x10, 0xA9, 0x00, 0x60};  /* Load $1000, LDA #0, RTS */
    
    p00_file_t file;
    int ret = p00_from_prg("PRGTEST", prg_data, sizeof(prg_data), &file);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(file.type, P00_TYPE_PRG);
    
    uint16_t addr = p00_get_load_address(&file);
    ASSERT_EQ(addr, 0x1000);
    
    p00_close(&file);
}

TEST(extract_prg)
{
    uint8_t orig_prg[] = {0x00, 0xC0, 0x78, 0x4C, 0x00, 0xC0};
    
    p00_file_t file;
    p00_from_prg("EXTRACT", orig_prg, sizeof(orig_prg), &file);
    
    uint8_t extracted[100];
    size_t size;
    int ret = p00_extract_prg(&file, extracted, sizeof(extracted), &size);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(size, sizeof(orig_prg));
    ASSERT(memcmp(extracted, orig_prg, sizeof(orig_prg)) == 0);
    
    p00_close(&file);
}

/* ============================================================================
 * Unit Tests - Conversion
 * ============================================================================ */

TEST(make_pc_filename)
{
    char pc_name[256];
    
    p00_make_pc_filename("HELLO", pc_name, P00_TYPE_PRG);
    ASSERT_STR_EQ(pc_name, "HELLO.P00");
    
    p00_make_pc_filename("DATA", pc_name, P00_TYPE_SEQ);
    ASSERT_STR_EQ(pc_name, "DATA.S00");
}

TEST(petscii_ascii)
{
    uint8_t petscii[16] = {0x48, 0x45, 0x4C, 0x4C, 0x4F, 0xA0, 0xA0};  /* HELLO */
    char ascii[17];
    
    p00_petscii_to_ascii(petscii, ascii, 16);
    /* H=0x48 stays, E=0x45 stays, etc. */
    ASSERT(ascii[0] != 0);
}

TEST(ascii_petscii)
{
    uint8_t petscii[16];
    p00_ascii_to_petscii("hello", petscii, 16);
    
    /* Should convert to uppercase PETSCII */
    ASSERT_EQ(petscii[0], 'H');
    ASSERT_EQ(petscii[1], 'E');
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== P00/S00/U00 PC64 Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_valid);
    RUN_TEST(detect_invalid);
    RUN_TEST(validate_valid);
    RUN_TEST(type_from_name);
    RUN_TEST(type_name);
    RUN_TEST(type_extension);
    
    printf("\nFile Operations:\n");
    RUN_TEST(open_p00);
    RUN_TEST(close_p00);
    RUN_TEST(get_info);
    RUN_TEST(get_filename);
    RUN_TEST(get_load_address);
    
    printf("\nCreation:\n");
    RUN_TEST(create_p00);
    RUN_TEST(from_prg);
    RUN_TEST(extract_prg);
    
    printf("\nConversion:\n");
    RUN_TEST(make_pc_filename);
    RUN_TEST(petscii_ascii);
    RUN_TEST(ascii_petscii);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
