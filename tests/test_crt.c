/**
 * @file test_crt.c
 * @brief Unit tests for CRT Cartridge Format
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_crt.h"

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
 * @brief Create minimal CRT image
 */
static uint8_t *create_test_crt(size_t *size)
{
    /* Header (64) + CHIP header (16) + 8K ROM */
    size_t total = CRT_HEADER_SIZE + CRT_CHIP_HEADER_SIZE + 8192;
    uint8_t *data = calloc(1, total);
    
    /* CRT header */
    memcpy(data, "C64 CARTRIDGE   ", 16);
    /* Header length (big endian) */
    data[16] = 0x00; data[17] = 0x00; data[18] = 0x00; data[19] = 0x40;
    /* Version 1.0 */
    data[20] = 0x01; data[21] = 0x00;
    /* Hardware type 0 (normal) */
    data[22] = 0x00; data[23] = 0x00;
    /* EXROM=0, GAME=1 (8K mode) */
    data[24] = 0x00;
    data[25] = 0x01;
    /* Name */
    memcpy(data + 32, "TEST CARTRIDGE", 14);
    
    /* CHIP packet */
    uint8_t *chip = data + CRT_HEADER_SIZE;
    memcpy(chip, "CHIP", 4);
    /* Packet length (big endian) */
    chip[4] = 0x00; chip[5] = 0x00; chip[6] = 0x20; chip[7] = 0x10; /* 8208 */
    /* Chip type = ROM */
    chip[8] = 0x00; chip[9] = 0x00;
    /* Bank 0 */
    chip[10] = 0x00; chip[11] = 0x00;
    /* Load address $8000 */
    chip[12] = 0x80; chip[13] = 0x00;
    /* ROM size 8192 */
    chip[14] = 0x20; chip[15] = 0x00;
    
    /* ROM data (simple pattern) */
    uint8_t *rom = chip + CRT_CHIP_HEADER_SIZE;
    for (int i = 0; i < 8192; i++) {
        rom[i] = i & 0xFF;
    }
    /* CBM80 signature at $8004 */
    rom[4] = 0xC3; rom[5] = 0xC2; rom[6] = 0xCD;
    rom[7] = 0x38; rom[8] = 0x30;
    
    *size = total;
    return data;
}

/* ============================================================================
 * Unit Tests - Detection
 * ============================================================================ */

TEST(detect_valid)
{
    size_t size;
    uint8_t *data = create_test_crt(&size);
    
    ASSERT_TRUE(crt_detect(data, size));
    
    free(data);
}

TEST(detect_invalid)
{
    uint8_t data[100] = {0};
    ASSERT_FALSE(crt_detect(data, 100));
    ASSERT_FALSE(crt_detect(NULL, 100));
    ASSERT_FALSE(crt_detect(data, 10));
}

TEST(validate_valid)
{
    size_t size;
    uint8_t *data = create_test_crt(&size);
    
    ASSERT_TRUE(crt_validate(data, size));
    
    free(data);
}

/* ============================================================================
 * Unit Tests - Image Management
 * ============================================================================ */

TEST(open_crt)
{
    size_t size;
    uint8_t *data = create_test_crt(&size);
    
    crt_image_t image;
    int ret = crt_open(data, size, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(image.data);
    ASSERT_EQ(image.header.hw_type, CRT_TYPE_NORMAL);
    ASSERT_EQ(image.header.exrom, 0);
    ASSERT_EQ(image.header.game, 1);
    ASSERT_EQ(image.num_chips, 1);
    
    crt_close(&image);
    free(data);
}

TEST(close_crt)
{
    size_t size;
    uint8_t *data = create_test_crt(&size);
    
    crt_image_t image;
    crt_open(data, size, &image);
    crt_close(&image);
    
    ASSERT_EQ(image.data, NULL);
    ASSERT_EQ(image.chips, NULL);
    
    free(data);
}

/* ============================================================================
 * Unit Tests - Cartridge Info
 * ============================================================================ */

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_crt(&size);
    
    crt_image_t image;
    crt_open(data, size, &image);
    
    crt_info_t info;
    int ret = crt_get_info(&image, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.type, CRT_TYPE_NORMAL);
    ASSERT_EQ(info.num_chips, 1);
    ASSERT_EQ(info.total_rom_size, 8192);
    ASSERT_EQ(info.exrom, 0);
    ASSERT_EQ(info.game, 1);
    
    crt_close(&image);
    free(data);
}

TEST(get_name)
{
    size_t size;
    uint8_t *data = create_test_crt(&size);
    
    crt_image_t image;
    crt_open(data, size, &image);
    
    char name[33];
    crt_get_name(&image, name);
    
    ASSERT_STR_EQ(name, "TEST CARTRIDGE");
    
    crt_close(&image);
    free(data);
}

TEST(type_name)
{
    ASSERT_STR_EQ(crt_type_name(CRT_TYPE_NORMAL), "Normal cartridge");
    ASSERT_STR_EQ(crt_type_name(CRT_TYPE_ACTION_REPLAY), "Action Replay");
    ASSERT_STR_EQ(crt_type_name(CRT_TYPE_OCEAN_1), "Ocean type 1");
    ASSERT_STR_EQ(crt_type_name(CRT_TYPE_EASYFLASH), "EasyFlash");
}

/* ============================================================================
 * Unit Tests - CHIP Operations
 * ============================================================================ */

TEST(get_chip_count)
{
    size_t size;
    uint8_t *data = create_test_crt(&size);
    
    crt_image_t image;
    crt_open(data, size, &image);
    
    ASSERT_EQ(crt_get_chip_count(&image), 1);
    
    crt_close(&image);
    free(data);
}

TEST(get_chip)
{
    size_t size;
    uint8_t *data = create_test_crt(&size);
    
    crt_image_t image;
    crt_open(data, size, &image);
    
    crt_chip_t chip;
    int ret = crt_get_chip(&image, 0, &chip);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(chip.header.bank, 0);
    ASSERT_EQ(chip.header.load_address, 0x8000);
    ASSERT_EQ(chip.header.rom_size, 8192);
    ASSERT_EQ(chip.header.chip_type, CRT_ROM_TYPE_ROM);
    
    crt_close(&image);
    free(data);
}

TEST(extract_rom)
{
    size_t size;
    uint8_t *data = create_test_crt(&size);
    
    crt_image_t image;
    crt_open(data, size, &image);
    
    uint8_t rom[8192];
    size_t extracted;
    int ret = crt_extract_rom(&image, rom, sizeof(rom), &extracted);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(extracted, 8192);
    /* Check CBM80 signature */
    ASSERT_EQ(rom[4], 0xC3);
    ASSERT_EQ(rom[5], 0xC2);
    ASSERT_EQ(rom[6], 0xCD);
    
    crt_close(&image);
    free(data);
}

/* ============================================================================
 * Unit Tests - CRT Creation
 * ============================================================================ */

TEST(create_crt)
{
    crt_image_t image;
    int ret = crt_create("MY CART", CRT_TYPE_NORMAL, 0, 1, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(image.data);
    ASSERT_EQ(image.header.hw_type, CRT_TYPE_NORMAL);
    ASSERT_EQ(image.header.exrom, 0);
    ASSERT_EQ(image.header.game, 1);
    
    crt_close(&image);
}

TEST(add_chip)
{
    crt_image_t image;
    crt_create("ADD CHIP", CRT_TYPE_NORMAL, 0, 1, &image);
    
    uint8_t rom[8192];
    memset(rom, 0xAA, sizeof(rom));
    
    int ret = crt_add_chip(&image, 0, 0x8000, rom, sizeof(rom), CRT_ROM_TYPE_ROM);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(image.num_chips, 1);
    
    crt_close(&image);
}

TEST(create_8k)
{
    uint8_t rom[8192];
    for (int i = 0; i < 8192; i++) rom[i] = i;
    
    crt_image_t image;
    int ret = crt_create_8k("8K CART", rom, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(image.num_chips, 1);
    ASSERT_EQ(image.header.exrom, 0);
    ASSERT_EQ(image.header.game, 1);
    
    crt_close(&image);
}

TEST(create_16k)
{
    uint8_t rom[16384];
    for (int i = 0; i < 16384; i++) rom[i] = i;
    
    crt_image_t image;
    int ret = crt_create_16k("16K CART", rom, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(image.num_chips, 2);  /* ROML + ROMH */
    ASSERT_EQ(image.header.exrom, 0);
    ASSERT_EQ(image.header.game, 0);
    
    /* Verify ROML at $8000 */
    ASSERT_EQ(image.chips[0].header.load_address, 0x8000);
    /* Verify ROMH at $A000 */
    ASSERT_EQ(image.chips[1].header.load_address, 0xA000);
    
    crt_close(&image);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== CRT Cartridge Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_valid);
    RUN_TEST(detect_invalid);
    RUN_TEST(validate_valid);
    
    printf("\nImage Management:\n");
    RUN_TEST(open_crt);
    RUN_TEST(close_crt);
    
    printf("\nCartridge Info:\n");
    RUN_TEST(get_info);
    RUN_TEST(get_name);
    RUN_TEST(type_name);
    
    printf("\nCHIP Operations:\n");
    RUN_TEST(get_chip_count);
    RUN_TEST(get_chip);
    RUN_TEST(extract_rom);
    
    printf("\nCRT Creation:\n");
    RUN_TEST(create_crt);
    RUN_TEST(add_chip);
    RUN_TEST(create_8k);
    RUN_TEST(create_16k);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
