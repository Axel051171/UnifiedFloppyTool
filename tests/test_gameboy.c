/**
 * @file test_gameboy.c
 * @brief Unit tests for Game Boy / GBA ROM Format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/nintendo/uft_gameboy.h"

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

/* Nintendo logo for test ROMs */
static const uint8_t NINTENDO_LOGO[48] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
    0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
};

/* Create test GB ROM */
static uint8_t *create_test_gb(size_t *size)
{
    size_t rom_size = 32768;  /* 32KB */
    uint8_t *data = calloc(1, rom_size);
    
    /* Nintendo logo at 0x104 */
    memcpy(data + 0x104, NINTENDO_LOGO, 48);
    
    /* Title at 0x134 */
    memcpy(data + 0x134, "TEST ROM", 8);
    
    /* Cartridge type at 0x147 */
    data[0x147] = GB_MBC_MBC1_RAM_BATT;
    
    /* ROM size at 0x148 */
    data[0x148] = 0x00;  /* 32KB */
    
    /* RAM size at 0x149 */
    data[0x149] = 0x02;  /* 8KB */
    
    /* Calculate header checksum */
    uint8_t checksum = 0;
    for (int i = 0x134; i <= 0x14C; i++) {
        checksum = checksum - data[i] - 1;
    }
    data[0x14D] = checksum;
    
    *size = rom_size;
    return data;
}

/* Create test GBA ROM */
static uint8_t *create_test_gba(size_t *size)
{
    size_t rom_size = 1024 * 1024;  /* 1MB */
    uint8_t *data = calloc(1, rom_size);
    
    /* ARM branch instruction at 0x00 */
    data[0] = 0x00;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 0xEA;  /* B instruction */
    
    /* Title at 0xA0 */
    memcpy(data + 0xA0, "TESTGAME", 8);
    
    /* Game code at 0xAC */
    memcpy(data + 0xAC, "TEST", 4);
    
    /* Maker code at 0xB0 */
    memcpy(data + 0xB0, "01", 2);
    
    /* Fixed value at 0xB2 */
    data[0xB2] = 0x96;
    
    /* Calculate complement */
    uint8_t checksum = 0;
    for (int i = 0xA0; i <= 0xBC; i++) {
        checksum += data[i];
    }
    data[0xBD] = -(0x19 + checksum);
    
    *size = rom_size;
    return data;
}

/* Tests */
TEST(detect_gb)
{
    size_t size;
    uint8_t *data = create_test_gb(&size);
    
    ASSERT_TRUE(gb_detect(data, size));
    ASSERT_FALSE(gba_detect(data, size));
    
    free(data);
}

TEST(detect_gba)
{
    size_t size;
    uint8_t *data = create_test_gba(&size);
    
    ASSERT_TRUE(gba_detect(data, size));
    ASSERT_FALSE(gb_detect(data, size));
    
    free(data);
}

TEST(validate_logo)
{
    size_t size;
    uint8_t *data = create_test_gb(&size);
    
    ASSERT_TRUE(gb_validate_logo(data, size));
    
    /* Corrupt logo */
    data[0x104] = 0x00;
    ASSERT_FALSE(gb_validate_logo(data, size));
    
    free(data);
}

TEST(mbc_name)
{
    ASSERT(strcmp(gb_mbc_name(GB_MBC_ROM_ONLY), "ROM ONLY") == 0);
    ASSERT(strcmp(gb_mbc_name(GB_MBC_MBC1), "MBC1") == 0);
    ASSERT(strcmp(gb_mbc_name(GB_MBC_MBC3_RAM_BATT), "MBC3+RAM+BATTERY") == 0);
}

TEST(compat_name)
{
    ASSERT(strcmp(gb_compat_name(GB_COMPAT_DMG), "Game Boy") == 0);
    ASSERT(strcmp(gb_compat_name(GB_COMPAT_CGB_ONLY), "Game Boy Color Only") == 0);
}

TEST(open_gb)
{
    size_t size;
    uint8_t *data = create_test_gb(&size);
    
    gb_rom_t rom;
    int ret = gb_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(rom.is_gba);
    ASSERT_NOT_NULL(rom.data);
    
    gb_close(&rom);
    free(data);
}

TEST(open_gba)
{
    size_t size;
    uint8_t *data = create_test_gba(&size);
    
    gb_rom_t rom;
    int ret = gb_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(rom.is_gba);
    
    gb_close(&rom);
    free(data);
}

TEST(get_gb_info)
{
    size_t size;
    uint8_t *data = create_test_gb(&size);
    
    gb_rom_t rom;
    gb_open(data, size, &rom);
    
    gb_info_t info;
    int ret = gb_get_info(&rom, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT(strncmp(info.title, "TEST ROM", 8) == 0);
    ASSERT_EQ(info.mbc_type, GB_MBC_MBC1_RAM_BATT);
    ASSERT_TRUE(info.has_battery);
    ASSERT_TRUE(info.header_valid);
    
    gb_close(&rom);
    free(data);
}

TEST(get_gba_info)
{
    size_t size;
    uint8_t *data = create_test_gba(&size);
    
    gb_rom_t rom;
    gb_open(data, size, &rom);
    
    gba_info_t info;
    int ret = gba_get_info(&rom, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT(strncmp(info.title, "TESTGAME", 8) == 0);
    ASSERT(strncmp(info.game_code, "TEST", 4) == 0);
    
    gb_close(&rom);
    free(data);
}

TEST(rom_size_bytes)
{
    ASSERT_EQ(gb_rom_size_bytes(0x00), 32768);
    ASSERT_EQ(gb_rom_size_bytes(0x01), 65536);
    ASSERT_EQ(gb_rom_size_bytes(0x05), 1048576);
}

TEST(ram_size_bytes)
{
    ASSERT_EQ(gb_ram_size_bytes(0x00), 0);
    ASSERT_EQ(gb_ram_size_bytes(0x02), 8192);
    ASSERT_EQ(gb_ram_size_bytes(0x03), 32768);
}

TEST(has_battery)
{
    ASSERT_TRUE(gb_has_battery(GB_MBC_MBC1_RAM_BATT));
    ASSERT_TRUE(gb_has_battery(GB_MBC_MBC3_RAM_BATT));
    ASSERT_FALSE(gb_has_battery(GB_MBC_ROM_ONLY));
    ASSERT_FALSE(gb_has_battery(GB_MBC_MBC1));
}

TEST(has_timer)
{
    ASSERT_TRUE(gb_has_timer(GB_MBC_MBC3_TIMER_BATT));
    ASSERT_TRUE(gb_has_timer(GB_MBC_MBC3_TIMER_RAM_BATT));
    ASSERT_FALSE(gb_has_timer(GB_MBC_MBC3_RAM_BATT));
}

TEST(close_rom)
{
    size_t size;
    uint8_t *data = create_test_gb(&size);
    
    gb_rom_t rom;
    gb_open(data, size, &rom);
    gb_close(&rom);
    
    ASSERT_EQ(rom.data, NULL);
    
    free(data);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== Game Boy / GBA ROM Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_gb);
    RUN_TEST(detect_gba);
    RUN_TEST(validate_logo);
    RUN_TEST(mbc_name);
    RUN_TEST(compat_name);
    
    printf("\nROM Operations:\n");
    RUN_TEST(open_gb);
    RUN_TEST(open_gba);
    RUN_TEST(get_gb_info);
    RUN_TEST(get_gba_info);
    RUN_TEST(close_rom);
    
    printf("\nSize Conversion:\n");
    RUN_TEST(rom_size_bytes);
    RUN_TEST(ram_size_bytes);
    
    printf("\nFeature Detection:\n");
    RUN_TEST(has_battery);
    RUN_TEST(has_timer);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
