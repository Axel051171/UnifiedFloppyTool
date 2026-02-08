/**
 * @file test_reu.c
 * @brief Unit tests for REU/GeoRAM Support
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_reu.h"

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

/* ============================================================================
 * Unit Tests - Constants
 * ============================================================================ */

TEST(reu_sizes)
{
    ASSERT_EQ(REU_SIZE_1700, 128 * 1024);
    ASSERT_EQ(REU_SIZE_1764, 256 * 1024);
    ASSERT_EQ(REU_SIZE_1750, 512 * 1024);
    ASSERT_EQ(REU_SIZE_1MB, 1024 * 1024);
    ASSERT_EQ(GEORAM_SIZE_512K, 512 * 1024);
}

TEST(detect_type)
{
    ASSERT_EQ(reu_detect_type(REU_SIZE_1700), REU_TYPE_1700);
    ASSERT_EQ(reu_detect_type(REU_SIZE_1764), REU_TYPE_1764);
    ASSERT_EQ(reu_detect_type(REU_SIZE_1750), REU_TYPE_1750);
    ASSERT_EQ(reu_detect_type(REU_SIZE_1MB), REU_TYPE_1MB);
    ASSERT_EQ(reu_detect_type(REU_SIZE_2MB), REU_TYPE_2MB);
}

TEST(type_size)
{
    ASSERT_EQ(reu_type_size(REU_TYPE_1700), REU_SIZE_1700);
    ASSERT_EQ(reu_type_size(REU_TYPE_1764), REU_SIZE_1764);
    ASSERT_EQ(reu_type_size(REU_TYPE_1750), REU_SIZE_1750);
    ASSERT_EQ(reu_type_size(REU_TYPE_1MB), REU_SIZE_1MB);
}

TEST(type_name)
{
    ASSERT(strstr(reu_type_name(REU_TYPE_1700), "1700") != NULL);
    ASSERT(strstr(reu_type_name(REU_TYPE_1764), "1764") != NULL);
    ASSERT(strstr(reu_type_name(REU_TYPE_1750), "1750") != NULL);
    ASSERT(strstr(reu_type_name(REU_TYPE_GEORAM), "GeoRAM") != NULL);
}

/* ============================================================================
 * Unit Tests - Image Creation
 * ============================================================================ */

TEST(create_1700)
{
    reu_image_t image;
    int ret = reu_create(REU_TYPE_1700, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(image.data);
    ASSERT_EQ(image.size, REU_SIZE_1700);
    ASSERT_EQ(image.type, REU_TYPE_1700);
    
    reu_close(&image);
}

TEST(create_1750)
{
    reu_image_t image;
    int ret = reu_create(REU_TYPE_1750, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(image.data);
    ASSERT_EQ(image.size, REU_SIZE_1750);
    ASSERT_EQ(image.type, REU_TYPE_1750);
    
    reu_close(&image);
}

TEST(create_sized)
{
    reu_image_t image;
    int ret = reu_create_sized(REU_SIZE_1MB, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(image.size, REU_SIZE_1MB);
    ASSERT_EQ(image.type, REU_TYPE_1MB);
    
    reu_close(&image);
}

TEST(close_reu)
{
    reu_image_t image;
    reu_create(REU_TYPE_1700, &image);
    reu_close(&image);
    
    ASSERT_EQ(image.data, NULL);
    ASSERT_EQ(image.size, 0);
}

/* ============================================================================
 * Unit Tests - REU Info
 * ============================================================================ */

TEST(get_info)
{
    reu_image_t image;
    reu_create(REU_TYPE_1750, &image);
    
    reu_info_t info;
    int ret = reu_get_info(&image, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.type, REU_TYPE_1750);
    ASSERT_EQ(info.size, REU_SIZE_1750);
    ASSERT_EQ(info.num_pages, REU_SIZE_1750 / 256);
    ASSERT_EQ(info.num_banks, REU_SIZE_1750 / 65536);
    ASSERT_FALSE(info.is_georam);
    
    reu_close(&image);
}

/* ============================================================================
 * Unit Tests - Memory Access
 * ============================================================================ */

TEST(read_write_byte)
{
    reu_image_t image;
    reu_create(REU_TYPE_1700, &image);
    
    reu_write_byte(&image, 0, 0xAA);
    reu_write_byte(&image, 1000, 0xBB);
    reu_write_byte(&image, image.size - 1, 0xCC);
    
    ASSERT_EQ(reu_read_byte(&image, 0), 0xAA);
    ASSERT_EQ(reu_read_byte(&image, 1000), 0xBB);
    ASSERT_EQ(reu_read_byte(&image, image.size - 1), 0xCC);
    
    /* Out of bounds read */
    ASSERT_EQ(reu_read_byte(&image, image.size), 0xFF);
    
    reu_close(&image);
}

TEST(read_write_block)
{
    reu_image_t image;
    reu_create(REU_TYPE_1700, &image);
    
    uint8_t write_buf[256];
    for (int i = 0; i < 256; i++) write_buf[i] = i;
    
    size_t written = reu_write_block(&image, 0, write_buf, sizeof(write_buf));
    ASSERT_EQ(written, 256);
    
    uint8_t read_buf[256];
    size_t read = reu_read_block(&image, 0, read_buf, sizeof(read_buf));
    ASSERT_EQ(read, 256);
    
    ASSERT(memcmp(write_buf, read_buf, 256) == 0);
    
    reu_close(&image);
}

TEST(read_write_page)
{
    reu_image_t image;
    reu_create(REU_TYPE_1764, &image);
    
    uint8_t page[256];
    for (int i = 0; i < 256; i++) page[i] = 255 - i;
    
    /* Write to bank 1, page 5 */
    int ret = reu_write_page(&image, 1, 5, page);
    ASSERT_EQ(ret, 0);
    
    /* Read back */
    uint8_t read_page[256];
    ret = reu_read_page(&image, 1, 5, read_page);
    ASSERT_EQ(ret, 0);
    
    ASSERT(memcmp(page, read_page, 256) == 0);
    
    reu_close(&image);
}

/* ============================================================================
 * Unit Tests - GeoRAM
 * ============================================================================ */

TEST(georam_create)
{
    reu_image_t image;
    int ret = georam_create(GEORAM_SIZE_512K, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(image.size, GEORAM_SIZE_512K);
    ASSERT_EQ(image.type, REU_TYPE_GEORAM);
    
    reu_close(&image);
}

TEST(georam_read_write)
{
    reu_image_t image;
    georam_create(GEORAM_SIZE_512K, &image);
    
    georam_state_t state = {0, 0};
    
    /* Write to block 0, page 0, offset 0 */
    georam_write(&image, &state, 0, 0x12);
    georam_write(&image, &state, 255, 0x34);
    
    ASSERT_EQ(georam_read(&image, &state, 0), 0x12);
    ASSERT_EQ(georam_read(&image, &state, 255), 0x34);
    
    reu_close(&image);
}

TEST(georam_block_page)
{
    reu_image_t image;
    georam_create(GEORAM_SIZE_1MB, &image);
    
    georam_state_t state = {0, 0};
    
    /* Write to block 0, page 0 */
    georam_set_block(&state, 0);
    georam_set_page(&state, 0);
    georam_write(&image, &state, 0, 0xAA);
    
    /* Write to block 1, page 10 */
    georam_set_block(&state, 1);
    georam_set_page(&state, 10);
    georam_write(&image, &state, 0, 0xBB);
    
    /* Read back */
    georam_set_block(&state, 0);
    georam_set_page(&state, 0);
    ASSERT_EQ(georam_read(&image, &state, 0), 0xAA);
    
    georam_set_block(&state, 1);
    georam_set_page(&state, 10);
    ASSERT_EQ(georam_read(&image, &state, 0), 0xBB);
    
    reu_close(&image);
}

/* ============================================================================
 * Unit Tests - Utilities
 * ============================================================================ */

TEST(fill_clear)
{
    reu_image_t image;
    reu_create(REU_TYPE_1700, &image);
    
    reu_fill(&image, 0xAA);
    ASSERT_EQ(image.data[0], 0xAA);
    ASSERT_EQ(image.data[1000], 0xAA);
    ASSERT_EQ(image.data[image.size - 1], 0xAA);
    
    reu_clear(&image);
    ASSERT_EQ(image.data[0], 0);
    ASSERT_EQ(image.data[1000], 0);
    ASSERT_EQ(image.data[image.size - 1], 0);
    
    reu_close(&image);
}

TEST(compare)
{
    reu_image_t image1, image2;
    reu_create(REU_TYPE_1700, &image1);
    reu_create(REU_TYPE_1700, &image2);
    
    /* Both empty, should be equal */
    ASSERT_EQ(reu_compare(&image1, &image2), 0);
    
    /* Modify one */
    reu_write_byte(&image1, 0, 0xFF);
    ASSERT_NE(reu_compare(&image1, &image2), 0);
    
    reu_close(&image1);
    reu_close(&image2);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== REU/GeoRAM Support Tests ===\n\n");
    
    printf("Constants:\n");
    RUN_TEST(reu_sizes);
    RUN_TEST(detect_type);
    RUN_TEST(type_size);
    RUN_TEST(type_name);
    
    printf("\nImage Creation:\n");
    RUN_TEST(create_1700);
    RUN_TEST(create_1750);
    RUN_TEST(create_sized);
    RUN_TEST(close_reu);
    
    printf("\nREU Info:\n");
    RUN_TEST(get_info);
    
    printf("\nMemory Access:\n");
    RUN_TEST(read_write_byte);
    RUN_TEST(read_write_block);
    RUN_TEST(read_write_page);
    
    printf("\nGeoRAM:\n");
    RUN_TEST(georam_create);
    RUN_TEST(georam_read_write);
    RUN_TEST(georam_block_page);
    
    printf("\nUtilities:\n");
    RUN_TEST(fill_clear);
    RUN_TEST(compare);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
