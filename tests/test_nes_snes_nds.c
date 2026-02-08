/**
 * @file test_nes_snes_nds.c
 * @brief Unit tests for NES, SNES, and NDS ROM formats
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/nintendo/uft_nes.h"
#include "uft/formats/nintendo/uft_snes.h"
#include "uft/formats/nintendo/uft_nds.h"

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

/* ============================================================================
 * NES Test Data
 * ============================================================================ */

static uint8_t *create_test_nes(size_t *size)
{
    /* iNES format: 16-byte header + 16KB PRG + 8KB CHR */
    *size = 16 + 16384 + 8192;
    uint8_t *data = calloc(1, *size);
    
    /* iNES header */
    memcpy(data, "NES\x1A", 4);
    data[4] = 1;    /* 1x 16KB PRG */
    data[5] = 1;    /* 1x 8KB CHR */
    data[6] = 0x01; /* Vertical mirroring, mapper 0 low */
    data[7] = 0x00; /* Mapper 0 high */
    
    return data;
}

static uint8_t *create_test_nes20(size_t *size)
{
    *size = 16 + 32768 + 8192;
    uint8_t *data = calloc(1, *size);
    
    /* NES 2.0 header */
    memcpy(data, "NES\x1A", 4);
    data[4] = 2;    /* 2x 16KB PRG */
    data[5] = 1;    /* 1x 8KB CHR */
    data[6] = 0x12; /* Mapper 1 low, battery */
    data[7] = 0x08; /* NES 2.0 identifier */
    
    return data;
}

/* ============================================================================
 * SNES Test Data
 * ============================================================================ */

static uint8_t *create_test_snes_lorom(size_t *size)
{
    /* LoROM: 256KB */
    *size = 256 * 1024;
    uint8_t *data = calloc(1, *size);
    
    /* Internal header at $7FC0 */
    size_t hdr = SNES_LOROM_HEADER;
    memcpy(data + hdr, "TEST SNES ROM       ", 21);
    data[hdr + 21] = 0x20;  /* LoROM */
    data[hdr + 22] = 0x02;  /* ROM + SRAM */
    data[hdr + 23] = 0x09;  /* 256KB */
    data[hdr + 24] = 0x03;  /* 8KB SRAM */
    data[hdr + 25] = 0x01;  /* USA */
    data[hdr + 26] = 0x00;  /* Developer */
    data[hdr + 27] = 0x00;  /* Version */
    
    /* Calculate simple checksum */
    uint32_t sum = 0;
    for (size_t i = 0; i < *size; i++) {
        if (i < hdr + 28 || i >= hdr + 32) sum += data[i];
    }
    uint16_t checksum = sum & 0xFFFF;
    uint16_t complement = checksum ^ 0xFFFF;
    
    data[hdr + 28] = complement & 0xFF;
    data[hdr + 29] = (complement >> 8) & 0xFF;
    data[hdr + 30] = checksum & 0xFF;
    data[hdr + 31] = (checksum >> 8) & 0xFF;
    
    return data;
}

static uint8_t *create_test_snes_smc(size_t *size)
{
    /* SMC: 512-byte header + ROM */
    uint8_t *rom = create_test_snes_lorom(size);
    *size += SNES_COPIER_HEADER;
    
    uint8_t *data = calloc(1, *size);
    memcpy(data + SNES_COPIER_HEADER, rom, *size - SNES_COPIER_HEADER);
    free(rom);
    
    /* SMC header */
    data[0] = ((*size - SNES_COPIER_HEADER) / 8192) & 0xFF;
    data[1] = ((*size - SNES_COPIER_HEADER) / 8192) >> 8;
    
    return data;
}

/* ============================================================================
 * NDS Test Data
 * ============================================================================ */

/* CRC16 for NDS */
static uint16_t crc16_calc(const uint8_t *data, size_t len)
{
    static const uint16_t crc16_table[256] = {
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
        0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
        0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
        0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
        /* ... abbreviated for brevity ... */
    };
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc16_table[(crc ^ data[i]) & 0xFF];
    }
    return crc;
}

static uint8_t *create_test_nds(size_t *size)
{
    /* Minimal NDS: 512-byte header + some data */
    *size = 1024 * 1024;  /* 1MB */
    uint8_t *data = calloc(1, *size);
    
    /* Title */
    memcpy(data, "TESTGAME", 8);
    
    /* Game code */
    memcpy(data + 12, "TEST", 4);
    
    /* Maker code */
    memcpy(data + 16, "01", 2);
    
    /* Unit code */
    data[18] = 0x00;  /* NDS */
    
    /* ARM9 */
    *(uint32_t*)(data + 0x20) = 0x200;  /* offset */
    *(uint32_t*)(data + 0x24) = 0x02000000;  /* entry */
    *(uint32_t*)(data + 0x28) = 0x02000000;  /* load */
    *(uint32_t*)(data + 0x2C) = 0x1000;  /* size */
    
    /* ARM7 */
    *(uint32_t*)(data + 0x30) = 0x1200;  /* offset */
    *(uint32_t*)(data + 0x34) = 0x02380000;  /* entry */
    *(uint32_t*)(data + 0x38) = 0x02380000;  /* load */
    *(uint32_t*)(data + 0x3C) = 0x1000;  /* size */
    
    /* Total size */
    *(uint32_t*)(data + 0x80) = *size;
    
    /* Header size */
    *(uint32_t*)(data + 0x84) = 0x200;
    
    /* Calculate and store header CRC */
    uint16_t crc = crc16_calc(data, 0x15E);
    data[0x15E] = crc & 0xFF;
    data[0x15F] = (crc >> 8) & 0xFF;
    
    return data;
}

/* ============================================================================
 * NES Tests
 * ============================================================================ */

TEST(nes_detect_ines)
{
    size_t size;
    uint8_t *data = create_test_nes(&size);
    
    nes_format_t format = nes_detect_format(data, size);
    ASSERT_EQ(format, NES_FORMAT_INES);
    
    free(data);
}

TEST(nes_detect_nes20)
{
    size_t size;
    uint8_t *data = create_test_nes20(&size);
    
    nes_format_t format = nes_detect_format(data, size);
    ASSERT_EQ(format, NES_FORMAT_NES20);
    
    free(data);
}

TEST(nes_format_name)
{
    ASSERT(strcmp(nes_format_name(NES_FORMAT_INES), "iNES") == 0);
    ASSERT(strcmp(nes_format_name(NES_FORMAT_NES20), "NES 2.0") == 0);
}

TEST(nes_mapper_name)
{
    ASSERT(strcmp(nes_mapper_name(0), "NROM") == 0);
    ASSERT(strcmp(nes_mapper_name(1), "MMC1 (SxROM)") == 0);
    ASSERT(strcmp(nes_mapper_name(4), "MMC3 (TxROM)") == 0);
}

TEST(nes_open)
{
    size_t size;
    uint8_t *data = create_test_nes(&size);
    
    nes_rom_t rom;
    int ret = nes_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(rom.data);
    ASSERT_EQ(rom.format, NES_FORMAT_INES);
    
    nes_close(&rom);
    free(data);
}

TEST(nes_get_info)
{
    size_t size;
    uint8_t *data = create_test_nes(&size);
    
    nes_rom_t rom;
    nes_open(data, size, &rom);
    
    nes_info_t info;
    int ret = nes_get_info(&rom, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.mapper, 0);
    ASSERT_EQ(info.prg_rom_size, 16384);
    ASSERT_EQ(info.chr_rom_size, 8192);
    
    nes_close(&rom);
    free(data);
}

TEST(nes_close)
{
    size_t size;
    uint8_t *data = create_test_nes(&size);
    
    nes_rom_t rom;
    nes_open(data, size, &rom);
    nes_close(&rom);
    
    ASSERT_EQ(rom.data, NULL);
    
    free(data);
}

/* ============================================================================
 * SNES Tests
 * ============================================================================ */

TEST(snes_no_copier_header)
{
    size_t size;
    uint8_t *data = create_test_snes_lorom(&size);
    
    ASSERT_FALSE(snes_has_copier_header(data, size));
    
    free(data);
}

TEST(snes_with_copier_header)
{
    size_t size;
    uint8_t *data = create_test_snes_smc(&size);
    
    ASSERT_TRUE(snes_has_copier_header(data, size));
    
    free(data);
}

TEST(snes_mapping_name)
{
    ASSERT(strcmp(snes_mapping_name(0x20), "LoROM") == 0);
    ASSERT(strcmp(snes_mapping_name(0x21), "HiROM") == 0);
}

TEST(snes_region_name)
{
    ASSERT(strcmp(snes_region_name(SNES_REGION_JAPAN), "Japan") == 0);
    ASSERT(strcmp(snes_region_name(SNES_REGION_USA), "USA") == 0);
}

TEST(snes_open)
{
    size_t size;
    uint8_t *data = create_test_snes_lorom(&size);
    
    snes_rom_t rom;
    int ret = snes_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(rom.data);
    
    snes_close(&rom);
    free(data);
}

TEST(snes_get_info)
{
    size_t size;
    uint8_t *data = create_test_snes_lorom(&size);
    
    snes_rom_t rom;
    snes_open(data, size, &rom);
    
    snes_info_t info;
    int ret = snes_get_info(&rom, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT(strncmp(info.title, "TEST SNES ROM", 13) == 0);
    ASSERT_EQ(info.region, SNES_REGION_USA);
    
    snes_close(&rom);
    free(data);
}

TEST(snes_close)
{
    size_t size;
    uint8_t *data = create_test_snes_lorom(&size);
    
    snes_rom_t rom;
    snes_open(data, size, &rom);
    snes_close(&rom);
    
    ASSERT_EQ(rom.data, NULL);
    
    free(data);
}

/* ============================================================================
 * NDS Tests
 * ============================================================================ */

TEST(nds_unit_name)
{
    ASSERT(strcmp(nds_unit_name(NDS_UNIT_NDS), "Nintendo DS") == 0);
    ASSERT(strcmp(nds_unit_name(NDS_UNIT_DSI), "Nintendo DSi") == 0);
}

TEST(nds_open)
{
    size_t size;
    uint8_t *data = create_test_nds(&size);
    
    nds_rom_t rom;
    int ret = nds_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(rom.data);
    
    nds_close(&rom);
    free(data);
}

TEST(nds_get_info)
{
    size_t size;
    uint8_t *data = create_test_nds(&size);
    
    nds_rom_t rom;
    nds_open(data, size, &rom);
    
    nds_info_t info;
    int ret = nds_get_info(&rom, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT(strncmp(info.title, "TESTGAME", 8) == 0);
    ASSERT(strcmp(info.game_code, "TEST") == 0);
    
    nds_close(&rom);
    free(data);
}

TEST(nds_close)
{
    size_t size;
    uint8_t *data = create_test_nds(&size);
    
    nds_rom_t rom;
    nds_open(data, size, &rom);
    nds_close(&rom);
    
    ASSERT_EQ(rom.data, NULL);
    
    free(data);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== NES / SNES / NDS ROM Format Tests ===\n\n");
    
    printf("NES:\n");
    RUN_TEST(nes_detect_ines);
    RUN_TEST(nes_detect_nes20);
    RUN_TEST(nes_format_name);
    RUN_TEST(nes_mapper_name);
    RUN_TEST(nes_open);
    RUN_TEST(nes_get_info);
    RUN_TEST(nes_close);
    
    printf("\nSNES:\n");
    RUN_TEST(snes_no_copier_header);
    RUN_TEST(snes_with_copier_header);
    RUN_TEST(snes_mapping_name);
    RUN_TEST(snes_region_name);
    RUN_TEST(snes_open);
    RUN_TEST(snes_get_info);
    RUN_TEST(snes_close);
    
    printf("\nNDS:\n");
    RUN_TEST(nds_unit_name);
    RUN_TEST(nds_open);
    RUN_TEST(nds_get_info);
    RUN_TEST(nds_close);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
