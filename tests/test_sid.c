/**
 * @file test_sid.c
 * @brief Unit tests for SID Music Format
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_sid.h"

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
 * @brief Create minimal PSID v2 file
 */
static uint8_t *create_test_sid(size_t *size)
{
    /* Header (124 bytes) + small program */
    size_t total = SID_HEADER_V2 + 256;
    uint8_t *data = calloc(1, total);
    
    /* Magic */
    memcpy(data, "PSID", 4);
    
    /* Version (big endian) */
    data[4] = 0x00; data[5] = 0x02;  /* Version 2 */
    
    /* Data offset */
    data[6] = 0x00; data[7] = 0x7C;  /* 124 bytes */
    
    /* Load address ($1000) */
    data[8] = 0x10; data[9] = 0x00;
    
    /* Init address ($1000) */
    data[10] = 0x10; data[11] = 0x00;
    
    /* Play address ($1003) */
    data[12] = 0x10; data[13] = 0x03;
    
    /* Songs */
    data[14] = 0x00; data[15] = 0x01;  /* 1 song */
    
    /* Start song */
    data[16] = 0x00; data[17] = 0x01;  /* Song 1 */
    
    /* Speed (all VBI) */
    data[18] = 0x00; data[19] = 0x00;
    data[20] = 0x00; data[21] = 0x00;
    
    /* Name */
    memcpy(data + 22, "Test Tune", 9);
    
    /* Author */
    memcpy(data + 54, "Test Author", 11);
    
    /* Released */
    memcpy(data + 86, "2026 Test", 9);
    
    /* Flags (PAL, 6581) */
    data[118] = 0x00; data[119] = 0x14;  /* PAL + 6581 */
    
    /* C64 program data */
    uint8_t *prg = data + 124;
    prg[0] = 0x78;  /* SEI */
    prg[1] = 0x4C; prg[2] = 0x03; prg[3] = 0x10;  /* JMP $1003 */
    /* Play routine - just RTS */
    prg[3] = 0x60;  /* RTS */
    
    *size = total;
    return data;
}

/* ============================================================================
 * Unit Tests - Detection
 * ============================================================================ */

TEST(detect_psid)
{
    size_t size;
    uint8_t *data = create_test_sid(&size);
    
    ASSERT_TRUE(sid_detect(data, size));
    
    free(data);
}

TEST(detect_rsid)
{
    size_t size;
    uint8_t *data = create_test_sid(&size);
    memcpy(data, "RSID", 4);
    
    ASSERT_TRUE(sid_detect(data, size));
    
    free(data);
}

TEST(detect_invalid)
{
    uint8_t data[200] = {0};
    ASSERT_FALSE(sid_detect(data, 200));
    ASSERT_FALSE(sid_detect(NULL, 200));
    ASSERT_FALSE(sid_detect(data, 3));
}

TEST(validate_valid)
{
    size_t size;
    uint8_t *data = create_test_sid(&size);
    
    ASSERT_TRUE(sid_validate(data, size));
    
    free(data);
}

/* ============================================================================
 * Unit Tests - Image Management
 * ============================================================================ */

TEST(open_sid)
{
    size_t size;
    uint8_t *data = create_test_sid(&size);
    
    sid_image_t image;
    int ret = sid_open(data, size, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(image.data);
    ASSERT_EQ(image.header.version, 2);
    ASSERT_EQ(image.header.songs, 1);
    ASSERT_EQ(image.actual_load_addr, 0x1000);
    
    sid_close(&image);
    free(data);
}

TEST(close_sid)
{
    size_t size;
    uint8_t *data = create_test_sid(&size);
    
    sid_image_t image;
    sid_open(data, size, &image);
    sid_close(&image);
    
    ASSERT_EQ(image.data, NULL);
    
    free(data);
}

/* ============================================================================
 * Unit Tests - SID Info
 * ============================================================================ */

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_sid(&size);
    
    sid_image_t image;
    sid_open(data, size, &image);
    
    sid_info_t info;
    int ret = sid_get_info(&image, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.type, SID_TYPE_PSID);
    ASSERT_EQ(info.version, 2);
    ASSERT_STR_EQ(info.name, "Test Tune");
    ASSERT_STR_EQ(info.author, "Test Author");
    ASSERT_EQ(info.load_address, 0x1000);
    ASSERT_EQ(info.init_address, 0x1000);
    ASSERT_EQ(info.play_address, 0x1003);
    ASSERT_EQ(info.songs, 1);
    
    sid_close(&image);
    free(data);
}

TEST(get_name)
{
    size_t size;
    uint8_t *data = create_test_sid(&size);
    
    sid_image_t image;
    sid_open(data, size, &image);
    
    char name[33];
    sid_get_name(&image, name);
    
    ASSERT_STR_EQ(name, "Test Tune");
    
    sid_close(&image);
    free(data);
}

TEST(get_author)
{
    size_t size;
    uint8_t *data = create_test_sid(&size);
    
    sid_image_t image;
    sid_open(data, size, &image);
    
    char author[33];
    sid_get_author(&image, author);
    
    ASSERT_STR_EQ(author, "Test Author");
    
    sid_close(&image);
    free(data);
}

/* ============================================================================
 * Unit Tests - Data Extraction
 * ============================================================================ */

TEST(get_c64_data)
{
    size_t size;
    uint8_t *data = create_test_sid(&size);
    
    sid_image_t image;
    sid_open(data, size, &image);
    
    const uint8_t *c64_data;
    size_t c64_size;
    int ret = sid_get_c64_data(&image, &c64_data, &c64_size);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(c64_data);
    ASSERT(c64_size > 0);
    
    sid_close(&image);
    free(data);
}

TEST(extract_prg)
{
    size_t size;
    uint8_t *data = create_test_sid(&size);
    
    sid_image_t image;
    sid_open(data, size, &image);
    
    uint8_t prg[1024];
    size_t extracted;
    int ret = sid_extract_prg(&image, prg, sizeof(prg), &extracted);
    
    ASSERT_EQ(ret, 0);
    ASSERT(extracted >= 2);
    /* Check load address */
    ASSERT_EQ(prg[0], 0x00);
    ASSERT_EQ(prg[1], 0x10);
    
    sid_close(&image);
    free(data);
}

/* ============================================================================
 * Unit Tests - SID Creation
 * ============================================================================ */

TEST(create_sid)
{
    sid_image_t image;
    int ret = sid_create(SID_TYPE_PSID, 2, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(image.data);
    ASSERT_EQ(image.header.version, 2);
    
    sid_close(&image);
}

TEST(set_metadata)
{
    sid_image_t image;
    sid_create(SID_TYPE_PSID, 2, &image);
    
    sid_set_metadata(&image, "My Song", "My Author", "2026");
    
    char name[33], author[33];
    sid_get_name(&image, name);
    sid_get_author(&image, author);
    
    ASSERT_STR_EQ(name, "My Song");
    ASSERT_STR_EQ(author, "My Author");
    
    sid_close(&image);
}

TEST(set_addresses)
{
    sid_image_t image;
    sid_create(SID_TYPE_PSID, 2, &image);
    
    sid_set_addresses(&image, 0x0800, 0x0800, 0x0803);
    
    ASSERT_EQ(image.header.load_address, 0x0800);
    ASSERT_EQ(image.header.init_address, 0x0800);
    ASSERT_EQ(image.header.play_address, 0x0803);
    
    sid_close(&image);
}

TEST(from_prg)
{
    /* Create simple PRG */
    uint8_t prg[10] = {
        0x00, 0x10,  /* Load address $1000 */
        0x78,        /* SEI */
        0xA9, 0x00,  /* LDA #$00 */
        0x60,        /* RTS */
        0x60,        /* RTS (play) */
    };
    
    sid_image_t image;
    int ret = sid_from_prg(prg, sizeof(prg), "PRG Tune", "PRG Author",
                           0, 0x1006, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(image.actual_load_addr, 0x1000);
    ASSERT_EQ(image.header.play_address, 0x1006);
    
    sid_close(&image);
}

/* ============================================================================
 * Unit Tests - Utilities
 * ============================================================================ */

TEST(clock_name)
{
    ASSERT_STR_EQ(sid_clock_name(SID_CLOCK_PAL), "PAL");
    ASSERT_STR_EQ(sid_clock_name(SID_CLOCK_NTSC), "NTSC");
    ASSERT_STR_EQ(sid_clock_name(SID_CLOCK_ANY), "PAL/NTSC");
}

TEST(model_name)
{
    ASSERT_STR_EQ(sid_model_name(SID_MODEL_6581), "6581");
    ASSERT_STR_EQ(sid_model_name(SID_MODEL_8580), "8580");
    ASSERT_STR_EQ(sid_model_name(SID_MODEL_ANY), "6581/8580");
}

TEST(decode_address)
{
    ASSERT_EQ(sid_decode_address(SID_ADDR_NONE), 0);
    ASSERT_EQ(sid_decode_address(SID_ADDR_D420), 0xD420);
    ASSERT_EQ(sid_decode_address(SID_ADDR_DE00), 0xDE00);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== SID Music Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_psid);
    RUN_TEST(detect_rsid);
    RUN_TEST(detect_invalid);
    RUN_TEST(validate_valid);
    
    printf("\nImage Management:\n");
    RUN_TEST(open_sid);
    RUN_TEST(close_sid);
    
    printf("\nSID Info:\n");
    RUN_TEST(get_info);
    RUN_TEST(get_name);
    RUN_TEST(get_author);
    
    printf("\nData Extraction:\n");
    RUN_TEST(get_c64_data);
    RUN_TEST(extract_prg);
    
    printf("\nSID Creation:\n");
    RUN_TEST(create_sid);
    RUN_TEST(set_metadata);
    RUN_TEST(set_addresses);
    RUN_TEST(from_prg);
    
    printf("\nUtilities:\n");
    RUN_TEST(clock_name);
    RUN_TEST(model_name);
    RUN_TEST(decode_address);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
