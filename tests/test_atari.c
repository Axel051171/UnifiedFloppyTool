/**
 * @file test_atari.c
 * @brief Unit tests for Atari 2600/7800/Lynx ROM Format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/atari/uft_atari.h"

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

/* Create test 2600 ROM (4K) */
static uint8_t *create_test_2600_4k(size_t *size)
{
    *size = A26_SIZE_4K;
    uint8_t *data = calloc(1, *size);
    
    /* Simple 2600 reset vector pattern */
    data[0xFFC] = 0x00;
    data[0xFFD] = 0xF0;  /* Reset vector to $F000 */
    
    return data;
}

/* Create test 2600 ROM (8K) */
static uint8_t *create_test_2600_8k(size_t *size)
{
    *size = A26_SIZE_8K;
    uint8_t *data = calloc(1, *size);
    
    /* F8 bankswitch pattern */
    data[0x1FF8] = 0x00;
    data[0x1FF9] = 0x00;
    
    return data;
}

/* Create test 7800 ROM with header */
static uint8_t *create_test_7800(size_t *size)
{
    size_t rom_size = 32768;
    *size = A78_HEADER_SIZE + rom_size;
    uint8_t *data = calloc(1, *size);
    
    /* A78 header */
    data[0] = 1;  /* Version */
    memcpy(data + 1, "ATARI7800", 9);  /* Magic */
    memcpy(data + 17, "TEST 7800 GAME", 14);  /* Title */
    
    /* ROM size (big-endian) */
    data[49] = (rom_size >> 24) & 0xFF;
    data[50] = (rom_size >> 16) & 0xFF;
    data[51] = (rom_size >> 8) & 0xFF;
    data[52] = rom_size & 0xFF;
    
    /* Cart type */
    data[53] = 0x00;
    data[54] = 0x01;  /* POKEY */
    
    /* Controllers */
    data[55] = A78_CTRL_JOYSTICK;
    data[56] = A78_CTRL_JOYSTICK;
    
    /* TV type */
    data[57] = 0;  /* NTSC */
    
    return data;
}

/* Create test Lynx ROM with header */
static uint8_t *create_test_lynx(size_t *size)
{
    size_t rom_size = 65536;
    *size = LYNX_HEADER_SIZE + rom_size;
    uint8_t *data = calloc(1, *size);
    
    /* Lynx header */
    memcpy(data, "LYNX", 4);  /* Magic */
    
    /* Page sizes */
    data[4] = 0x00;
    data[5] = 0x01;  /* Bank 0: 256 bytes */
    data[6] = 0x00;
    data[7] = 0x01;  /* Bank 1: 256 bytes */
    
    /* Title */
    memcpy(data + 10, "TEST LYNX GAME", 14);
    
    /* Manufacturer */
    memcpy(data + 42, "ATARI", 5);
    
    /* Rotation */
    data[58] = 0;  /* No rotation */
    
    return data;
}

/* Tests */
TEST(detect_2600)
{
    size_t size;
    uint8_t *data = create_test_2600_4k(&size);
    
    atari_console_t console = atari_detect_console(data, size);
    ASSERT_EQ(console, ATARI_CONSOLE_2600);
    
    free(data);
}

TEST(detect_7800)
{
    size_t size;
    uint8_t *data = create_test_7800(&size);
    
    ASSERT_TRUE(atari_is_a78(data, size));
    
    atari_console_t console = atari_detect_console(data, size);
    ASSERT_EQ(console, ATARI_CONSOLE_7800);
    
    free(data);
}

TEST(detect_lynx)
{
    size_t size;
    uint8_t *data = create_test_lynx(&size);
    
    ASSERT_TRUE(atari_is_lynx(data, size));
    
    atari_console_t console = atari_detect_console(data, size);
    ASSERT_EQ(console, ATARI_CONSOLE_LYNX);
    
    free(data);
}

TEST(bankswitch_none)
{
    size_t size;
    uint8_t *data = create_test_2600_4k(&size);
    
    a26_bankswitch_t bs = a26_detect_bankswitch(data, size);
    ASSERT_EQ(bs, A26_BANK_NONE);
    
    free(data);
}

TEST(bankswitch_f8)
{
    size_t size;
    uint8_t *data = create_test_2600_8k(&size);
    
    a26_bankswitch_t bs = a26_detect_bankswitch(data, size);
    ASSERT_EQ(bs, A26_BANK_F8);
    
    free(data);
}

TEST(console_name)
{
    ASSERT(strcmp(atari_console_name(ATARI_CONSOLE_2600), "Atari 2600 (VCS)") == 0);
    ASSERT(strcmp(atari_console_name(ATARI_CONSOLE_7800), "Atari 7800 (ProSystem)") == 0);
    ASSERT(strcmp(atari_console_name(ATARI_CONSOLE_LYNX), "Atari Lynx") == 0);
}

TEST(bankswitch_name)
{
    ASSERT(strcmp(a26_bankswitch_name(A26_BANK_NONE), "None (2K/4K)") == 0);
    ASSERT(strcmp(a26_bankswitch_name(A26_BANK_F8), "F8 (8K Atari)") == 0);
    ASSERT(strcmp(a26_bankswitch_name(A26_BANK_F6), "F6 (16K Atari)") == 0);
}

TEST(controller_name)
{
    ASSERT(strcmp(a78_controller_name(A78_CTRL_JOYSTICK), "7800 Joystick") == 0);
    ASSERT(strcmp(a78_controller_name(A78_CTRL_LIGHTGUN), "Light Gun") == 0);
}

TEST(open_2600)
{
    size_t size;
    uint8_t *data = create_test_2600_4k(&size);
    
    atari_rom_t rom;
    int ret = atari_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(rom.console, ATARI_CONSOLE_2600);
    ASSERT_FALSE(rom.has_header);
    
    atari_close(&rom);
    free(data);
}

TEST(open_7800)
{
    size_t size;
    uint8_t *data = create_test_7800(&size);
    
    atari_rom_t rom;
    int ret = atari_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(rom.console, ATARI_CONSOLE_7800);
    ASSERT_TRUE(rom.has_header);
    ASSERT_EQ(rom.header_size, A78_HEADER_SIZE);
    
    atari_close(&rom);
    free(data);
}

TEST(get_info_2600)
{
    size_t size;
    uint8_t *data = create_test_2600_4k(&size);
    
    atari_rom_t rom;
    atari_open(data, size, &rom);
    
    atari_info_t info;
    int ret = atari_get_info(&rom, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.console, ATARI_CONSOLE_2600);
    ASSERT_EQ(info.rom_size, A26_SIZE_4K);
    ASSERT_EQ(info.bankswitch, A26_BANK_NONE);
    
    atari_close(&rom);
    free(data);
}

TEST(get_info_7800)
{
    size_t size;
    uint8_t *data = create_test_7800(&size);
    
    atari_rom_t rom;
    atari_open(data, size, &rom);
    
    atari_info_t info;
    int ret = atari_get_info(&rom, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.console, ATARI_CONSOLE_7800);
    ASSERT(strncmp(info.title, "TEST 7800 GAME", 14) == 0);
    ASSERT_TRUE(info.has_pokey);
    ASSERT_FALSE(info.is_pal);
    
    atari_close(&rom);
    free(data);
}

TEST(get_rom_data)
{
    size_t size;
    uint8_t *data = create_test_7800(&size);
    
    atari_rom_t rom;
    atari_open(data, size, &rom);
    
    const uint8_t *rom_data = atari_get_rom_data(&rom);
    size_t rom_size = atari_get_rom_size(&rom);
    
    ASSERT_NOT_NULL(rom_data);
    ASSERT_EQ(rom_size, size - A78_HEADER_SIZE);
    
    atari_close(&rom);
    free(data);
}

TEST(close_rom)
{
    size_t size;
    uint8_t *data = create_test_2600_4k(&size);
    
    atari_rom_t rom;
    atari_open(data, size, &rom);
    atari_close(&rom);
    
    ASSERT_EQ(rom.data, NULL);
    
    free(data);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== Atari 2600/7800/Lynx ROM Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_2600);
    RUN_TEST(detect_7800);
    RUN_TEST(detect_lynx);
    RUN_TEST(bankswitch_none);
    RUN_TEST(bankswitch_f8);
    RUN_TEST(console_name);
    RUN_TEST(bankswitch_name);
    RUN_TEST(controller_name);
    
    printf("\nROM Operations:\n");
    RUN_TEST(open_2600);
    RUN_TEST(open_7800);
    RUN_TEST(get_info_2600);
    RUN_TEST(get_info_7800);
    RUN_TEST(get_rom_data);
    RUN_TEST(close_rom);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
