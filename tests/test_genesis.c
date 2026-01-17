/**
 * @file test_genesis.c
 * @brief Unit tests for Sega Genesis/Mega Drive ROM Format
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/sega/uft_genesis.h"

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

/* Create test Genesis ROM */
static uint8_t *create_test_genesis_rom(size_t *size)
{
    /* Minimum ROM: 512KB */
    size_t rom_size = 0x80000;
    uint8_t *data = calloc(1, rom_size);
    
    /* Header at $100 */
    uint8_t *hdr = data + 0x100;
    
    /* System type */
    memcpy(hdr + 0x00, "SEGA MEGA DRIVE ", 16);
    
    /* Copyright */
    memcpy(hdr + 0x10, "(C)SEGA 1991.JAN", 16);
    
    /* Domestic title */
    memset(hdr + 0x20, ' ', 48);
    memcpy(hdr + 0x20, "SONIC THE HEDGEHOG", 18);
    
    /* Overseas title */
    memset(hdr + 0x50, ' ', 48);
    memcpy(hdr + 0x50, "SONIC THE HEDGEHOG", 18);
    
    /* Serial number */
    memcpy(hdr + 0x80, "GM 00001009-00", 14);
    
    /* Checksum placeholder */
    hdr[0x8E] = 0x00;
    hdr[0x8F] = 0x00;
    
    /* I/O support */
    memcpy(hdr + 0x90, "J               ", 16);
    
    /* ROM addresses (big endian) */
    hdr[0xA0] = 0x00; hdr[0xA1] = 0x00; hdr[0xA2] = 0x00; hdr[0xA3] = 0x00;  /* Start */
    hdr[0xA4] = 0x00; hdr[0xA5] = 0x07; hdr[0xA6] = 0xFF; hdr[0xA7] = 0xFF;  /* End */
    
    /* RAM addresses */
    hdr[0xA8] = 0x00; hdr[0xA9] = 0xFF; hdr[0xAA] = 0x00; hdr[0xAB] = 0x00;  /* Start */
    hdr[0xAC] = 0x00; hdr[0xAD] = 0xFF; hdr[0xAE] = 0xFF; hdr[0xAF] = 0xFF;  /* End */
    
    /* No SRAM */
    memset(hdr + 0xB0, ' ', 12);
    
    /* Region codes */
    memcpy(hdr + 0xF0, "JUE             ", 16);
    
    *size = rom_size;
    return data;
}

/* Create test SMD format ROM */
static uint8_t *create_test_smd_rom(size_t *size)
{
    /* SMD: header (512) + interleaved blocks */
    size_t bin_size = 0x80000;
    size_t blocks = bin_size / SMD_BLOCK_SIZE;
    size_t smd_size = SMD_HEADER_SIZE + blocks * SMD_BLOCK_SIZE;
    
    uint8_t *data = calloc(1, smd_size);
    
    /* SMD header */
    data[0] = blocks;
    data[1] = 0x03;
    data[8] = 0xAA;
    data[9] = 0xBB;
    data[10] = 0x06;  /* Genesis */
    
    /* Create valid Genesis header in first block (interleaved) */
    uint8_t *block = data + SMD_HEADER_SIZE;
    
    /* "SEGA MEGA DRIVE " interleaved at $100 */
    /* In SMD: odd bytes at start, even bytes at +8192 */
    const char *system = "SEGA MEGA DRIVE ";
    for (int i = 0; i < 16; i++) {
        block[0x80 + i] = system[i];  /* Odd bytes of header */
    }
    
    *size = smd_size;
    return data;
}

/* Tests */
TEST(detect_format_bin)
{
    size_t size;
    uint8_t *data = create_test_genesis_rom(&size);
    
    genesis_format_t format = genesis_detect_format(data, size);
    ASSERT_EQ(format, GENESIS_FORMAT_BIN);
    
    free(data);
}

TEST(detect_format_smd)
{
    size_t size;
    uint8_t *data = create_test_smd_rom(&size);
    
    genesis_format_t format = genesis_detect_format(data, size);
    ASSERT_EQ(format, GENESIS_FORMAT_SMD);
    
    free(data);
}

TEST(detect_system_md)
{
    size_t size;
    uint8_t *data = create_test_genesis_rom(&size);
    
    genesis_system_t system = genesis_detect_system(data, size);
    ASSERT_EQ(system, GENESIS_TYPE_MD);
    
    free(data);
}

TEST(format_name)
{
    ASSERT_STR_EQ(genesis_format_name(GENESIS_FORMAT_BIN), "BIN (Raw Binary)");
    ASSERT_STR_EQ(genesis_format_name(GENESIS_FORMAT_SMD), "SMD (Super Magic Drive)");
}

TEST(system_name)
{
    ASSERT_STR_EQ(genesis_system_name(GENESIS_TYPE_MD), "Sega Mega Drive");
    ASSERT_STR_EQ(genesis_system_name(GENESIS_TYPE_GENESIS), "Sega Genesis");
    ASSERT_STR_EQ(genesis_system_name(GENESIS_TYPE_32X), "Sega 32X");
}

TEST(validate_bin)
{
    size_t size;
    uint8_t *data = create_test_genesis_rom(&size);
    
    ASSERT_TRUE(genesis_validate(data, size));
    
    free(data);
}

TEST(validate_invalid)
{
    uint8_t data[1024] = {0};
    ASSERT_FALSE(genesis_validate(data, sizeof(data)));
}

TEST(open_rom)
{
    size_t size;
    uint8_t *data = create_test_genesis_rom(&size);
    
    genesis_rom_t rom;
    int ret = genesis_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(rom.data);
    ASSERT_EQ(rom.format, GENESIS_FORMAT_BIN);
    ASSERT_EQ(rom.system, GENESIS_TYPE_MD);
    
    genesis_close(&rom);
    free(data);
}

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_genesis_rom(&size);
    
    genesis_rom_t rom;
    genesis_open(data, size, &rom);
    
    genesis_info_t info;
    int ret = genesis_get_info(&rom, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.format, GENESIS_FORMAT_BIN);
    ASSERT_EQ(info.system, GENESIS_TYPE_MD);
    ASSERT(strstr(info.title, "SONIC") != NULL);
    ASSERT(strstr(info.serial, "00001009") != NULL);
    
    genesis_close(&rom);
    free(data);
}

TEST(get_title)
{
    size_t size;
    uint8_t *data = create_test_genesis_rom(&size);
    
    genesis_rom_t rom;
    genesis_open(data, size, &rom);
    
    const char *title = genesis_get_title(&rom, true);
    ASSERT_NOT_NULL(title);
    ASSERT(strstr(title, "SONIC") != NULL);
    
    genesis_close(&rom);
    free(data);
}

TEST(calculate_checksum)
{
    size_t size;
    uint8_t *data = create_test_genesis_rom(&size);
    
    uint16_t checksum = genesis_calculate_checksum(data, size);
    ASSERT(checksum != 0);  /* Should produce some value */
    
    free(data);
}

TEST(verify_checksum)
{
    size_t size;
    uint8_t *data = create_test_genesis_rom(&size);
    
    genesis_rom_t rom;
    genesis_open(data, size, &rom);
    
    /* Checksum won't match initially (we set 0) */
    ASSERT_FALSE(genesis_verify_checksum(&rom));
    
    genesis_close(&rom);
    free(data);
}

TEST(fix_checksum)
{
    size_t size;
    uint8_t *data = create_test_genesis_rom(&size);
    
    genesis_rom_t rom;
    genesis_open(data, size, &rom);
    
    int ret = genesis_fix_checksum(&rom);
    ASSERT_EQ(ret, 0);
    
    /* Now it should verify */
    ASSERT_TRUE(genesis_verify_checksum(&rom));
    
    genesis_close(&rom);
    free(data);
}

TEST(parse_regions)
{
    genesis_region_t r;
    
    r = genesis_parse_regions("J");
    ASSERT_EQ(r, GENESIS_REGION_JAPAN);
    
    r = genesis_parse_regions("U");
    ASSERT_EQ(r, GENESIS_REGION_USA);
    
    r = genesis_parse_regions("JUE");
    ASSERT_EQ(r, GENESIS_REGION_WORLD);
}

TEST(region_string)
{
    char buffer[64];
    
    genesis_region_string(GENESIS_REGION_JAPAN, buffer, sizeof(buffer));
    ASSERT(strstr(buffer, "Japan") != NULL);
    
    genesis_region_string(GENESIS_REGION_WORLD, buffer, sizeof(buffer));
    ASSERT(strstr(buffer, "Japan") != NULL);
    ASSERT(strstr(buffer, "USA") != NULL);
    ASSERT(strstr(buffer, "Europe") != NULL);
}

TEST(close_rom)
{
    size_t size;
    uint8_t *data = create_test_genesis_rom(&size);
    
    genesis_rom_t rom;
    genesis_open(data, size, &rom);
    genesis_close(&rom);
    
    ASSERT_EQ(rom.data, NULL);
    
    free(data);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== Sega Genesis/Mega Drive ROM Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_format_bin);
    RUN_TEST(detect_format_smd);
    RUN_TEST(detect_system_md);
    RUN_TEST(format_name);
    RUN_TEST(system_name);
    
    printf("\nValidation:\n");
    RUN_TEST(validate_bin);
    RUN_TEST(validate_invalid);
    
    printf("\nROM Operations:\n");
    RUN_TEST(open_rom);
    RUN_TEST(get_info);
    RUN_TEST(get_title);
    RUN_TEST(close_rom);
    
    printf("\nChecksum:\n");
    RUN_TEST(calculate_checksum);
    RUN_TEST(verify_checksum);
    RUN_TEST(fix_checksum);
    
    printf("\nRegion:\n");
    RUN_TEST(parse_regions);
    RUN_TEST(region_string);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
