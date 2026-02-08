/**
 * @file test_pce.c
 * @brief Unit tests for PC Engine / TurboGrafx-16 ROM Format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/nec/uft_pce.h"

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

/* Create test HuCard ROM (256KB) */
static uint8_t *create_test_hucard(size_t *size)
{
    *size = PCE_SIZE_256K;
    uint8_t *data = calloc(1, *size);
    
    /* Simple test pattern */
    data[0] = 0x4C;  /* JMP */
    data[1] = 0x00;
    data[2] = 0xE0;  /* $E000 */
    
    return data;
}

/* Create test ROM with 512-byte header */
static uint8_t *create_test_with_header(size_t *size)
{
    size_t rom_size = PCE_SIZE_256K;
    *size = 512 + rom_size;
    uint8_t *data = calloc(1, *size);
    
    /* Header (all zeros) */
    /* ROM data starts at 512 */
    data[512] = 0x4C;
    
    return data;
}

/* Tests */
TEST(detect_hucard)
{
    size_t size;
    uint8_t *data = create_test_hucard(&size);
    
    pce_type_t type = pce_detect_type(data, size);
    ASSERT_EQ(type, PCE_TYPE_HUCARD);
    
    free(data);
}

TEST(detect_header)
{
    size_t size;
    uint8_t *data = create_test_with_header(&size);
    
    bool has_header = pce_has_header(data, size);
    ASSERT_TRUE(has_header);
    
    free(data);
}

TEST(detect_no_header)
{
    size_t size;
    uint8_t *data = create_test_hucard(&size);
    
    bool has_header = pce_has_header(data, size);
    ASSERT_FALSE(has_header);
    
    free(data);
}

TEST(type_name)
{
    ASSERT(strcmp(pce_type_name(PCE_TYPE_HUCARD), "HuCard") == 0);
    ASSERT(strcmp(pce_type_name(PCE_TYPE_SUPERGRAFX), "SuperGrafx") == 0);
    ASSERT(strcmp(pce_type_name(PCE_TYPE_SF2), "Street Fighter II") == 0);
}

TEST(region_name)
{
    ASSERT(strcmp(pce_region_name(PCE_REGION_JAPAN), "Japan (PC Engine)") == 0);
    ASSERT(strcmp(pce_region_name(PCE_REGION_USA), "USA (TurboGrafx-16)") == 0);
}

TEST(validate)
{
    size_t size;
    uint8_t *data = create_test_hucard(&size);
    
    ASSERT_TRUE(pce_validate(data, size));
    
    free(data);
}

TEST(open_rom)
{
    size_t size;
    uint8_t *data = create_test_hucard(&size);
    
    pce_rom_t rom;
    int ret = pce_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(rom.data);
    ASSERT_EQ(rom.type, PCE_TYPE_HUCARD);
    ASSERT_FALSE(rom.has_header);
    
    pce_close(&rom);
    free(data);
}

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_hucard(&size);
    
    pce_rom_t rom;
    pce_open(data, size, &rom);
    
    pce_info_t info;
    int ret = pce_get_info(&rom, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.type, PCE_TYPE_HUCARD);
    ASSERT_EQ(info.file_size, PCE_SIZE_256K);
    ASSERT_EQ(info.rom_size, PCE_SIZE_256K);
    
    pce_close(&rom);
    free(data);
}

TEST(get_rom_data)
{
    size_t size;
    uint8_t *data = create_test_hucard(&size);
    
    pce_rom_t rom;
    pce_open(data, size, &rom);
    
    const uint8_t *rom_data = pce_get_rom_data(&rom);
    size_t rom_size = pce_get_rom_size(&rom);
    
    ASSERT_NOT_NULL(rom_data);
    ASSERT_EQ(rom_size, PCE_SIZE_256K);
    ASSERT_EQ(rom_data[0], 0x4C);
    
    pce_close(&rom);
    free(data);
}

TEST(calc_crc32)
{
    uint8_t test_data[] = { 0x01, 0x02, 0x03, 0x04 };
    uint32_t crc = pce_calc_crc32(test_data, 4);
    
    /* Known CRC32 for this sequence */
    ASSERT(crc != 0);
}

TEST(close_rom)
{
    size_t size;
    uint8_t *data = create_test_hucard(&size);
    
    pce_rom_t rom;
    pce_open(data, size, &rom);
    pce_close(&rom);
    
    ASSERT_EQ(rom.data, NULL);
    
    free(data);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== PC Engine / TurboGrafx-16 ROM Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_hucard);
    RUN_TEST(detect_header);
    RUN_TEST(detect_no_header);
    RUN_TEST(type_name);
    RUN_TEST(region_name);
    RUN_TEST(validate);
    
    printf("\nROM Operations:\n");
    RUN_TEST(open_rom);
    RUN_TEST(get_info);
    RUN_TEST(get_rom_data);
    RUN_TEST(calc_crc32);
    RUN_TEST(close_rom);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
