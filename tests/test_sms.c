/**
 * @file test_sms.c
 * @brief Unit tests for Sega Master System / Game Gear ROM Format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/sega/uft_sms.h"

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

/* Actual ROM sizes in bytes */
#define ROM_SIZE_32K    (32 * 1024)

/* Create test SMS ROM with header */
static uint8_t *create_test_sms(size_t *size)
{
    *size = ROM_SIZE_32K;
    uint8_t *data = calloc(1, *size);
    
    /* TMR SEGA header at 0x7FF0 */
    memcpy(data + SMS_HEADER_OFFSET_7FF0, "TMR SEGA", 8);
    
    /* Checksum placeholder */
    data[SMS_HEADER_OFFSET_7FF0 + 10] = 0x00;
    data[SMS_HEADER_OFFSET_7FF0 + 11] = 0x00;
    
    /* Product code (BCD): 12345 */
    data[SMS_HEADER_OFFSET_7FF0 + 12] = 0x45;
    data[SMS_HEADER_OFFSET_7FF0 + 13] = 0x23;
    data[SMS_HEADER_OFFSET_7FF0 + 14] = 0x01;
    
    /* Version (1) + Region (SMS Export = 4) */
    data[SMS_HEADER_OFFSET_7FF0 + 14] |= (1 << 4);  /* Version in high nibble of last product byte */
    data[SMS_HEADER_OFFSET_7FF0 + 15] = (4 << 4) | 0x0C;  /* Region + ROM size */
    
    return data;
}

/* Create test Game Gear ROM */
static uint8_t *create_test_gg(size_t *size)
{
    *size = ROM_SIZE_32K;
    uint8_t *data = calloc(1, *size);
    
    /* TMR SEGA header at 0x7FF0 */
    memcpy(data + SMS_HEADER_OFFSET_7FF0, "TMR SEGA", 8);
    
    /* Region: Game Gear Export = 6 */
    data[SMS_HEADER_OFFSET_7FF0 + 15] = (6 << 4) | 0x0C;
    
    return data;
}

/* Create test SMS ROM without header */
static uint8_t *create_test_sms_no_header(size_t *size)
{
    *size = ROM_SIZE_32K;
    uint8_t *data = calloc(1, *size);
    
    /* Just ROM data, no TMR SEGA */
    data[0] = 0xF3;  /* DI instruction (common start) */
    data[1] = 0xED;
    data[2] = 0x56;  /* IM 1 */
    
    return data;
}

/* Tests */
TEST(find_header_present)
{
    size_t size;
    uint8_t *data = create_test_sms(&size);
    
    uint32_t offset;
    ASSERT_TRUE(sms_find_header(data, size, &offset));
    ASSERT_EQ(offset, SMS_HEADER_OFFSET_7FF0);
    
    free(data);
}

TEST(find_header_absent)
{
    size_t size;
    uint8_t *data = create_test_sms_no_header(&size);
    
    uint32_t offset;
    ASSERT_FALSE(sms_find_header(data, size, &offset));
    
    free(data);
}

TEST(detect_console_sms)
{
    size_t size;
    uint8_t *data = create_test_sms(&size);
    
    sms_console_t console = sms_detect_console(data, size);
    ASSERT_EQ(console, SMS_CONSOLE_SMS);
    
    free(data);
}

TEST(detect_console_gg)
{
    size_t size;
    uint8_t *data = create_test_gg(&size);
    
    sms_console_t console = sms_detect_console(data, size);
    ASSERT_EQ(console, SMS_CONSOLE_GG);
    
    free(data);
}

TEST(console_name)
{
    ASSERT(strcmp(sms_console_name(SMS_CONSOLE_SMS), "Master System") == 0);
    ASSERT(strcmp(sms_console_name(SMS_CONSOLE_GG), "Game Gear") == 0);
    ASSERT(strcmp(sms_console_name(SMS_CONSOLE_SG1000), "SG-1000") == 0);
}

TEST(region_name)
{
    ASSERT(strcmp(sms_region_name(SMS_REGION_SMS_JAPAN), "SMS Japan") == 0);
    ASSERT(strcmp(sms_region_name(SMS_REGION_SMS_EXPORT), "SMS Export") == 0);
    ASSERT(strcmp(sms_region_name(SMS_REGION_GG_JAPAN), "GG Japan") == 0);
}

TEST(mapper_name)
{
    ASSERT(strcmp(sms_mapper_name(SMS_MAPPER_NONE), "None") == 0);
    ASSERT(strcmp(sms_mapper_name(SMS_MAPPER_SEGA), "Sega") == 0);
    ASSERT(strcmp(sms_mapper_name(SMS_MAPPER_CODEMASTERS), "Codemasters") == 0);
}

TEST(validate_with_header)
{
    size_t size;
    uint8_t *data = create_test_sms(&size);
    
    ASSERT_TRUE(sms_validate(data, size));
    
    free(data);
}

TEST(validate_no_header)
{
    size_t size;
    uint8_t *data = create_test_sms_no_header(&size);
    
    /* Should still be valid based on size */
    ASSERT_TRUE(sms_validate(data, size));
    
    free(data);
}

TEST(open_sms)
{
    size_t size;
    uint8_t *data = create_test_sms(&size);
    
    sms_rom_t rom;
    int ret = sms_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(rom.data);
    ASSERT_TRUE(rom.has_header);
    ASSERT_EQ(rom.console, SMS_CONSOLE_SMS);
    
    sms_close(&rom);
    free(data);
}

TEST(open_gg)
{
    size_t size;
    uint8_t *data = create_test_gg(&size);
    
    sms_rom_t rom;
    int ret = sms_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(rom.console, SMS_CONSOLE_GG);
    
    sms_close(&rom);
    free(data);
}

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_sms(&size);
    
    sms_rom_t rom;
    sms_open(data, size, &rom);
    
    sms_info_t info;
    int ret = sms_get_info(&rom, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.console, SMS_CONSOLE_SMS);
    ASSERT_TRUE(info.has_header);
    ASSERT_EQ(info.rom_size, ROM_SIZE_32K);
    
    sms_close(&rom);
    free(data);
}

TEST(detect_mapper_none)
{
    size_t size;
    uint8_t *data = create_test_sms(&size);
    
    sms_mapper_t mapper = sms_detect_mapper(data, size);
    ASSERT_EQ(mapper, SMS_MAPPER_NONE);
    
    free(data);
}

TEST(close_rom)
{
    size_t size;
    uint8_t *data = create_test_sms(&size);
    
    sms_rom_t rom;
    sms_open(data, size, &rom);
    sms_close(&rom);
    
    ASSERT_EQ(rom.data, NULL);
    
    free(data);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== Sega Master System / Game Gear ROM Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(find_header_present);
    RUN_TEST(find_header_absent);
    RUN_TEST(detect_console_sms);
    RUN_TEST(detect_console_gg);
    RUN_TEST(console_name);
    RUN_TEST(region_name);
    RUN_TEST(mapper_name);
    
    printf("\nValidation:\n");
    RUN_TEST(validate_with_header);
    RUN_TEST(validate_no_header);
    
    printf("\nROM Operations:\n");
    RUN_TEST(open_sms);
    RUN_TEST(open_gg);
    RUN_TEST(get_info);
    RUN_TEST(detect_mapper_none);
    RUN_TEST(close_rom);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
