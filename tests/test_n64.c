/**
 * @file test_n64.c
 * @brief Unit tests for Nintendo 64 ROM Format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/nintendo/uft_n64.h"

static int tests_run = 0;
static int tests_passed = 0;

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
#define ASSERT_TRUE(x) ASSERT((x))
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)

/* Create test Z64 ROM */
static uint8_t *create_test_z64(size_t *size)
{
    size_t rom_size = 0x101000;  /* Minimum for CRC calculation */
    uint8_t *data = calloc(1, rom_size);
    
    /* Z64 magic */
    data[0] = 0x80;
    data[1] = 0x37;
    data[2] = 0x12;
    data[3] = 0x40;
    
    /* Title at 0x20 */
    memcpy(data + 0x20, "TEST N64 ROM        ", 20);
    
    /* Game ID at 0x3C-0x3D */
    data[0x3C] = 'T';
    data[0x3D] = 'E';
    
    /* Region at 0x3E */
    data[0x3E] = 'N';  /* NTSC */
    
    /* Version at 0x3F */
    data[0x3F] = 0x00;
    
    *size = rom_size;
    return data;
}

/* Create test V64 ROM */
static uint8_t *create_test_v64(size_t *size)
{
    uint8_t *data = create_test_z64(size);
    
    /* Convert to V64 (byte-swap) */
    for (size_t i = 0; i < *size; i += 2) {
        uint8_t tmp = data[i];
        data[i] = data[i + 1];
        data[i + 1] = tmp;
    }
    
    return data;
}

/* Create test N64 (little-endian) ROM */
static uint8_t *create_test_n64_le(size_t *size)
{
    uint8_t *data = create_test_z64(size);
    
    /* Convert to N64 (word-swap) */
    for (size_t i = 0; i < *size; i += 4) {
        uint8_t tmp0 = data[i];
        uint8_t tmp1 = data[i + 1];
        data[i] = data[i + 3];
        data[i + 1] = data[i + 2];
        data[i + 2] = tmp1;
        data[i + 3] = tmp0;
    }
    
    return data;
}

/* Tests */
TEST(detect_format_z64)
{
    size_t size;
    uint8_t *data = create_test_z64(&size);
    
    n64_format_t format = n64_detect_format(data, size);
    ASSERT_EQ(format, N64_FORMAT_Z64);
    
    free(data);
}

TEST(detect_format_v64)
{
    size_t size;
    uint8_t *data = create_test_v64(&size);
    
    n64_format_t format = n64_detect_format(data, size);
    ASSERT_EQ(format, N64_FORMAT_V64);
    
    free(data);
}

TEST(detect_format_n64)
{
    size_t size;
    uint8_t *data = create_test_n64_le(&size);
    
    n64_format_t format = n64_detect_format(data, size);
    ASSERT_EQ(format, N64_FORMAT_N64);
    
    free(data);
}

TEST(format_name)
{
    ASSERT(strcmp(n64_format_name(N64_FORMAT_Z64), "z64 (Big-endian)") == 0);
    ASSERT(strcmp(n64_format_name(N64_FORMAT_V64), "v64 (Byte-swapped)") == 0);
    ASSERT(strcmp(n64_format_name(N64_FORMAT_N64), "n64 (Little-endian)") == 0);
}

TEST(region_name)
{
    ASSERT(strcmp(n64_region_name(N64_REGION_NTSC), "USA (NTSC)") == 0);
    ASSERT(strcmp(n64_region_name(N64_REGION_PAL), "Europe (PAL)") == 0);
    ASSERT(strcmp(n64_region_name(N64_REGION_JAPAN), "Japan") == 0);
}

TEST(cic_name)
{
    ASSERT(strcmp(n64_cic_name(N64_CIC_6102), "CIC-6102") == 0);
    ASSERT(strcmp(n64_cic_name(N64_CIC_6105), "CIC-6105") == 0);
}

TEST(validate)
{
    size_t size;
    uint8_t *data = create_test_z64(&size);
    
    ASSERT_TRUE(n64_validate(data, size));
    
    /* Invalid magic */
    data[0] = 0x00;
    ASSERT_FALSE(n64_validate(data, size));
    
    free(data);
}

TEST(open_z64)
{
    size_t size;
    uint8_t *data = create_test_z64(&size);
    
    n64_rom_t rom;
    int ret = n64_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(rom.data);
    ASSERT_EQ(rom.original_format, N64_FORMAT_Z64);
    
    n64_close(&rom);
    free(data);
}

TEST(open_v64_converts)
{
    size_t size;
    uint8_t *data = create_test_v64(&size);
    
    n64_rom_t rom;
    int ret = n64_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(rom.original_format, N64_FORMAT_V64);
    
    /* Should be converted to z64 internally */
    ASSERT_EQ(rom.data[0], 0x80);
    ASSERT_EQ(rom.data[1], 0x37);
    
    n64_close(&rom);
    free(data);
}

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_z64(&size);
    
    n64_rom_t rom;
    n64_open(data, size, &rom);
    
    n64_info_t info;
    int ret = n64_get_info(&rom, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT(strncmp(info.title, "TEST N64 ROM", 12) == 0);
    ASSERT_EQ(info.region, 'N');
    ASSERT(strncmp(info.full_code, "NTEN", 4) == 0);
    
    n64_close(&rom);
    free(data);
}

TEST(close_rom)
{
    size_t size;
    uint8_t *data = create_test_z64(&size);
    
    n64_rom_t rom;
    n64_open(data, size, &rom);
    n64_close(&rom);
    
    ASSERT_EQ(rom.data, NULL);
    
    free(data);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== Nintendo 64 ROM Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_format_z64);
    RUN_TEST(detect_format_v64);
    RUN_TEST(detect_format_n64);
    RUN_TEST(format_name);
    RUN_TEST(region_name);
    RUN_TEST(cic_name);
    RUN_TEST(validate);
    
    printf("\nROM Operations:\n");
    RUN_TEST(open_z64);
    RUN_TEST(open_v64_converts);
    RUN_TEST(get_info);
    RUN_TEST(close_rom);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
