/**
 * @file test_neogeo.c
 * @brief Unit tests for SNK Neo Geo ROM Format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/snk/uft_neogeo.h"

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

/* Create test .neo format ROM */
static uint8_t *create_test_neo(size_t *size)
{
    size_t p_size = 1024;
    size_t s_size = 512;
    size_t m_size = 256;
    size_t v_size = 1024;
    size_t c_size = 2048;
    
    *size = NEO_HEADER_SIZE + p_size + s_size + m_size + v_size + c_size;
    uint8_t *data = calloc(1, *size);
    
    /* NEO header */
    memcpy(data, "NEO\x01", 4);
    
    /* ROM sizes (little-endian) */
    data[4] = p_size & 0xFF;
    data[5] = (p_size >> 8) & 0xFF;
    
    data[8] = s_size & 0xFF;
    data[9] = (s_size >> 8) & 0xFF;
    
    data[12] = m_size & 0xFF;
    data[13] = (m_size >> 8) & 0xFF;
    
    data[16] = v_size & 0xFF;
    data[17] = (v_size >> 8) & 0xFF;
    
    data[20] = c_size & 0xFF;
    data[21] = (c_size >> 8) & 0xFF;
    
    /* Year */
    data[24] = 0xD0;
    data[25] = 0x07;  /* 2000 */
    
    /* NGH number */
    data[36] = 0x2A;  /* NGH 42 */
    
    /* Name */
    memcpy(data + 40, "TEST NEO GEO GAME", 17);
    
    /* Manufacturer */
    memcpy(data + 72, "SNK", 3);
    
    return data;
}

/* Create raw P-ROM */
static uint8_t *create_test_prom(size_t *size)
{
    *size = 1024 * 1024;  /* 1MB */
    uint8_t *data = calloc(1, *size);
    
    /* 68000 reset vector */
    data[0] = 0x00;
    data[1] = 0x10;  /* Initial SP */
    data[4] = 0x00;
    data[5] = 0xC0;  /* Reset PC */
    
    return data;
}

/* Tests */
TEST(is_neo_format)
{
    size_t size;
    uint8_t *data = create_test_neo(&size);
    
    ASSERT_TRUE(neogeo_is_neo_format(data, size));
    
    free(data);
}

TEST(is_not_neo_format)
{
    size_t size;
    uint8_t *data = create_test_prom(&size);
    
    ASSERT_FALSE(neogeo_is_neo_format(data, size));
    
    free(data);
}

TEST(detect_chip_type)
{
    ASSERT_EQ(neogeo_detect_chip_type("001-p1.bin"), NEO_ROM_P);
    ASSERT_EQ(neogeo_detect_chip_type("001-s1.bin"), NEO_ROM_S);
    ASSERT_EQ(neogeo_detect_chip_type("001-m1.bin"), NEO_ROM_M);
    ASSERT_EQ(neogeo_detect_chip_type("001-v1.bin"), NEO_ROM_V);
    ASSERT_EQ(neogeo_detect_chip_type("001-c1.bin"), NEO_ROM_C);
}

TEST(system_name)
{
    ASSERT(strcmp(neogeo_system_name(NEO_SYSTEM_MVS), "MVS (Arcade)") == 0);
    ASSERT(strcmp(neogeo_system_name(NEO_SYSTEM_AES), "AES (Home)") == 0);
    ASSERT(strcmp(neogeo_system_name(NEO_SYSTEM_CD), "Neo Geo CD") == 0);
}

TEST(rom_type_name)
{
    ASSERT(strcmp(neogeo_rom_type_name(NEO_ROM_P), "P-ROM (Program)") == 0);
    ASSERT(strcmp(neogeo_rom_type_name(NEO_ROM_C), "C-ROM (Character/Sprite)") == 0);
}

TEST(open_neo)
{
    size_t size;
    uint8_t *data = create_test_neo(&size);
    
    neogeo_rom_t rom;
    int ret = neogeo_open(data, size, &rom);
    
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(rom.is_neo_format);
    ASSERT_NOT_NULL(rom.data);
    
    neogeo_close(&rom);
    free(data);
}

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_neo(&size);
    
    neogeo_rom_t rom;
    neogeo_open(data, size, &rom);
    
    neogeo_info_t info;
    int ret = neogeo_get_info(&rom, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(info.is_neo_format);
    ASSERT(strncmp(info.name, "TEST NEO GEO GAME", 17) == 0);
    ASSERT_EQ(info.ngh, 42);
    
    neogeo_close(&rom);
    free(data);
}

TEST(get_prom)
{
    size_t size;
    uint8_t *data = create_test_neo(&size);
    
    neogeo_rom_t rom;
    neogeo_open(data, size, &rom);
    
    size_t p_size;
    const uint8_t *prom = neogeo_get_prom(&rom, &p_size);
    
    ASSERT_NOT_NULL(prom);
    ASSERT_EQ(p_size, 1024);
    
    neogeo_close(&rom);
    free(data);
}

TEST(close_rom)
{
    size_t size;
    uint8_t *data = create_test_neo(&size);
    
    neogeo_rom_t rom;
    neogeo_open(data, size, &rom);
    neogeo_close(&rom);
    
    ASSERT_EQ(rom.data, NULL);
    
    free(data);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== SNK Neo Geo ROM Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(is_neo_format);
    RUN_TEST(is_not_neo_format);
    RUN_TEST(detect_chip_type);
    RUN_TEST(system_name);
    RUN_TEST(rom_type_name);
    
    printf("\nROM Operations:\n");
    RUN_TEST(open_neo);
    RUN_TEST(get_info);
    RUN_TEST(get_prom);
    RUN_TEST(close_rom);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
