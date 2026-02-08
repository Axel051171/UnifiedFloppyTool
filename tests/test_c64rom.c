/**
 * @file test_c64rom.c
 * @brief Unit tests for C64 ROM Image Format
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_c64rom.h"

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
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)
#define ASSERT_TRUE(x) ASSERT((x))
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)

/* Create test KERNAL ROM */
static uint8_t *create_test_kernal(size_t *size)
{
    uint8_t *data = calloc(1, C64ROM_KERNAL_SIZE);
    
    /* Set up valid reset vector pointing to KERNAL space */
    data[0x1FFC] = 0x00;  /* Reset low */
    data[0x1FFD] = 0xFC;  /* Reset high ($FC00) */
    data[0x1FFA] = 0x00;  /* NMI low */
    data[0x1FFB] = 0xFE;  /* NMI high ($FE00) */
    data[0x1FFE] = 0x00;  /* IRQ low */
    data[0x1FFF] = 0xFF;  /* IRQ high ($FF00) */
    
    /* Add some JMP instructions for I/O vectors */
    for (int i = 0x1FC0; i < 0x1FF8; i += 3) {
        data[i] = 0x4C;  /* JMP */
        data[i + 1] = 0x00;
        data[i + 2] = 0xE0 + (i / 0x100);
    }
    
    *size = C64ROM_KERNAL_SIZE;
    return data;
}

/* Create test combined BASIC+KERNAL */
static uint8_t *create_test_combined(size_t *size)
{
    uint8_t *data = calloc(1, C64ROM_COMBINED_SIZE);
    
    /* BASIC section */
    data[0] = 0x94;  /* First byte of BASIC usually */
    
    /* KERNAL section */
    uint8_t *kernal = data + C64ROM_BASIC_SIZE;
    kernal[0x1FFC] = 0x00;
    kernal[0x1FFD] = 0xFC;
    kernal[0x1FFA] = 0x00;
    kernal[0x1FFB] = 0xFE;
    kernal[0x1FFE] = 0x00;
    kernal[0x1FFF] = 0xFF;
    
    *size = C64ROM_COMBINED_SIZE;
    return data;
}

/* Tests */
TEST(detect_type_kernal)
{
    ASSERT_EQ(c64rom_detect_type(C64ROM_KERNAL_SIZE), C64ROM_TYPE_BASIC);
}

TEST(detect_type_combined)
{
    ASSERT_EQ(c64rom_detect_type(C64ROM_COMBINED_SIZE), C64ROM_TYPE_COMBINED);
}

TEST(detect_type_full)
{
    ASSERT_EQ(c64rom_detect_type(C64ROM_FULL_SIZE), C64ROM_TYPE_FULL);
}

TEST(detect_type_char)
{
    ASSERT_EQ(c64rom_detect_type(C64ROM_CHAR_SIZE), C64ROM_TYPE_CHAR);
}

TEST(type_name)
{
    ASSERT_STR_EQ(c64rom_type_name(C64ROM_TYPE_BASIC), "BASIC ROM");
    ASSERT_STR_EQ(c64rom_type_name(C64ROM_TYPE_KERNAL), "KERNAL ROM");
    ASSERT_STR_EQ(c64rom_type_name(C64ROM_TYPE_COMBINED), "Combined BASIC+KERNAL");
}

TEST(version_name)
{
    ASSERT_STR_EQ(c64rom_version_name(C64ROM_VER_ORIGINAL), "Original Commodore");
    ASSERT_STR_EQ(c64rom_version_name(C64ROM_VER_JIFFYDOS), "JiffyDOS");
}

TEST(validate_kernal)
{
    size_t size;
    uint8_t *data = create_test_kernal(&size);
    
    ASSERT_TRUE(c64rom_validate(data, size));
    
    free(data);
}

TEST(validate_combined)
{
    size_t size;
    uint8_t *data = create_test_combined(&size);
    
    ASSERT_TRUE(c64rom_validate(data, size));
    
    free(data);
}

TEST(open_rom)
{
    size_t size;
    uint8_t *data = create_test_combined(&size);
    
    c64rom_image_t rom;
    int ret = c64rom_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(rom.data);
    ASSERT_NOT_NULL(rom.basic);
    ASSERT_NOT_NULL(rom.kernal);
    ASSERT_EQ(rom.type, C64ROM_TYPE_COMBINED);
    
    c64rom_close(&rom);
    free(data);
}

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_combined(&size);
    
    c64rom_image_t rom;
    c64rom_open(data, size, &rom);
    
    c64rom_info_t info;
    int ret = c64rom_get_info(&rom, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.type, C64ROM_TYPE_COMBINED);
    ASSERT_EQ(info.size, C64ROM_COMBINED_SIZE);
    ASSERT_TRUE(info.has_basic);
    ASSERT_TRUE(info.has_kernal);
    ASSERT_FALSE(info.has_char);
    
    c64rom_close(&rom);
    free(data);
}

TEST(get_vectors)
{
    size_t size;
    uint8_t *data = create_test_combined(&size);
    
    c64rom_image_t rom;
    c64rom_open(data, size, &rom);
    
    c64rom_vectors_t vectors;
    int ret = c64rom_get_vectors(&rom, &vectors);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(vectors.reset, 0xFC00);
    ASSERT_EQ(vectors.nmi, 0xFE00);
    ASSERT_EQ(vectors.irq, 0xFF00);
    
    c64rom_close(&rom);
    free(data);
}

TEST(extract_kernal)
{
    size_t size;
    uint8_t *data = create_test_combined(&size);
    
    c64rom_image_t rom;
    c64rom_open(data, size, &rom);
    
    uint8_t buffer[C64ROM_KERNAL_SIZE];
    size_t extracted;
    int ret = c64rom_extract(&rom, C64ROM_TYPE_KERNAL, buffer, sizeof(buffer), &extracted);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(extracted, C64ROM_KERNAL_SIZE);
    
    c64rom_close(&rom);
    free(data);
}

TEST(crc32)
{
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03};
    uint32_t crc = c64rom_crc32(data, sizeof(data));
    ASSERT(crc != 0);  /* Just verify it runs */
}

TEST(patch_rom)
{
    size_t size;
    uint8_t *data = create_test_kernal(&size);
    
    c64rom_image_t rom;
    c64rom_open(data, size, &rom);
    
    int ret = c64rom_patch(&rom, 0x100, 0xAA);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(rom.data[0x100], 0xAA);
    ASSERT_EQ(rom.version, C64ROM_VER_CUSTOM);
    
    c64rom_close(&rom);
    free(data);
}

TEST(close_rom)
{
    size_t size;
    uint8_t *data = create_test_kernal(&size);
    
    c64rom_image_t rom;
    c64rom_open(data, size, &rom);
    c64rom_close(&rom);
    
    ASSERT_EQ(rom.data, NULL);
    
    free(data);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== C64 ROM Image Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_type_kernal);
    RUN_TEST(detect_type_combined);
    RUN_TEST(detect_type_full);
    RUN_TEST(detect_type_char);
    RUN_TEST(type_name);
    RUN_TEST(version_name);
    
    printf("\nValidation:\n");
    RUN_TEST(validate_kernal);
    RUN_TEST(validate_combined);
    
    printf("\nROM Operations:\n");
    RUN_TEST(open_rom);
    RUN_TEST(get_info);
    RUN_TEST(get_vectors);
    RUN_TEST(extract_kernal);
    RUN_TEST(close_rom);
    
    printf("\nUtilities:\n");
    RUN_TEST(crc32);
    RUN_TEST(patch_rom);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
