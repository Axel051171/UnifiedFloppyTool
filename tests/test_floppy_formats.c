/**
 * @file test_floppy_formats.c
 * @brief Tests for Atari ST, ZX Spectrum, and Sega CD formats
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/atari/uft_atari_st.h"
#include "uft/formats/sinclair/uft_spectrum.h"
#include "uft/formats/sega/uft_sega_cd.h"

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
 * Atari ST Test Data
 * ============================================================================ */

static uint8_t *create_test_st_disk(size_t *size)
{
    /* 720KB DS/DD disk */
    *size = ST_DS_DD_SIZE;
    uint8_t *data = calloc(1, *size);
    
    /* Boot sector with valid geometry */
    data[11] = 0x00; data[12] = 0x02;  /* BPS = 512 */
    data[13] = 2;                       /* Sectors per cluster */
    data[14] = 1; data[15] = 0;        /* Reserved sectors */
    data[16] = 2;                       /* Number of FATs */
    data[17] = 0x70; data[18] = 0;     /* Root dir entries = 112 */
    data[19] = 0xA0; data[20] = 0x05;  /* Total sectors = 1440 */
    data[21] = 0xF9;                    /* Media descriptor */
    data[22] = 3; data[23] = 0;        /* Sectors per FAT */
    data[24] = 9; data[25] = 0;        /* Sectors per track */
    data[26] = 2; data[27] = 0;        /* Number of heads */
    
    return data;
}

/* ============================================================================
 * ZX Spectrum Test Data
 * ============================================================================ */

static uint8_t *create_test_tap(size_t *size)
{
    /* TAP with header block + data block */
    *size = 2 + 19 + 2 + 10;  /* header block + small data block */
    uint8_t *data = calloc(1, *size);
    
    /* First block: header (19 bytes + flag) */
    data[0] = 19;  /* Block length LSB */
    data[1] = 0;   /* Block length MSB */
    data[2] = 0x00; /* Flag: header */
    data[3] = 3;   /* Type: Code */
    memcpy(data + 4, "TEST      ", 10);  /* Filename */
    data[14] = 10; data[15] = 0;  /* Length */
    data[16] = 0; data[17] = 0x80;  /* Start address */
    data[18] = 0; data[19] = 0x80;  /* Param 2 */
    data[20] = 0x00;  /* Checksum placeholder */
    
    /* Second block: data */
    data[21] = 12;  /* Block length */
    data[22] = 0;
    data[23] = 0xFF;  /* Flag: data */
    /* Data bytes... */
    
    return data;
}

static uint8_t *create_test_tzx(size_t *size)
{
    /* TZX with magic header */
    *size = 64;
    uint8_t *data = calloc(1, *size);
    
    memcpy(data, "ZXTape!\x1A", 8);
    data[8] = 1;   /* Major version */
    data[9] = 20;  /* Minor version */
    
    return data;
}

static uint8_t *create_test_sna_48k(size_t *size)
{
    /* 48K SNA snapshot */
    *size = 49179;  /* 27 byte header + 48KB RAM */
    uint8_t *data = calloc(1, *size);
    
    /* Header */
    data[0] = 0x3F;  /* I register */
    /* Registers... */
    data[25] = 1;    /* Interrupt mode */
    data[26] = 7;    /* Border color */
    
    return data;
}

/* ============================================================================
 * Sega Saturn/Dreamcast Test Data
 * ============================================================================ */

static uint8_t *create_test_saturn_iso(size_t *size)
{
    *size = 32768;  /* Minimal ISO with header */
    uint8_t *data = calloc(1, *size);
    
    /* Saturn IP.BIN header */
    memcpy(data, "SEGA SEGASATURN ", 16);
    memcpy(data + 16, "SEGA            ", 16);  /* Maker */
    memcpy(data + 32, "T-00000   ", 10);        /* Product */
    memcpy(data + 42, "V1.000", 6);             /* Version */
    memcpy(data + 48, "19960101", 8);           /* Date */
    memcpy(data + 64, "JUE       ", 10);        /* Region */
    memcpy(data + 96, "TEST SATURN GAME", 16);  /* Title */
    
    return data;
}

static uint8_t *create_test_dreamcast_iso(size_t *size)
{
    *size = 32768;
    uint8_t *data = calloc(1, *size);
    
    /* Dreamcast IP.BIN header */
    memcpy(data, "SEGA SEGAKATANA ", 16);
    memcpy(data + 16, "SEGA            ", 16);  /* Maker */
    memcpy(data + 32, "GD-ROM          ", 16);  /* Device */
    memcpy(data + 48, "JUE     ", 8);           /* Region */
    memcpy(data + 64, "T-00000   ", 10);        /* Product */
    memcpy(data + 128, "TEST DREAMCAST GAME", 19);
    
    return data;
}

/* ============================================================================
 * Atari ST Tests
 * ============================================================================ */

TEST(st_format_name)
{
    ASSERT(strcmp(st_format_name(ST_FORMAT_ST), "ST (Raw)") == 0);
    ASSERT(strcmp(st_format_name(ST_FORMAT_MSA), "MSA (Magic Shadow Archiver)") == 0);
}

TEST(st_disk_type_name)
{
    ASSERT(strcmp(st_disk_type_name(ST_DISK_DS_DD), "Double-sided DD (720KB)") == 0);
}

TEST(st_detect_format)
{
    size_t size;
    uint8_t *data = create_test_st_disk(&size);
    
    st_format_t format = st_detect_format(data, size);
    ASSERT_EQ(format, ST_FORMAT_ST);
    
    free(data);
}

TEST(st_open)
{
    size_t size;
    uint8_t *data = create_test_st_disk(&size);
    
    st_disk_t disk;
    int ret = st_open(data, size, &disk);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(disk.data);
    
    st_close(&disk);
    free(data);
}

TEST(st_get_info)
{
    size_t size;
    uint8_t *data = create_test_st_disk(&size);
    
    st_disk_t disk;
    st_open(data, size, &disk);
    
    st_info_t info;
    int ret = st_get_info(&disk, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.disk_size, ST_DS_DD_SIZE);
    
    st_close(&disk);
    free(data);
}

/* ============================================================================
 * ZX Spectrum Tests
 * ============================================================================ */

TEST(spec_format_name)
{
    ASSERT(strcmp(spec_format_name(SPEC_FORMAT_TAP), "TAP (Raw tape)") == 0);
    ASSERT(strcmp(spec_format_name(SPEC_FORMAT_TZX), "TZX (Extended tape)") == 0);
    ASSERT(strcmp(spec_format_name(SPEC_FORMAT_Z80), "Z80 (Snapshot)") == 0);
}

TEST(spec_model_name)
{
    ASSERT(strcmp(spec_model_name(SPEC_MODEL_48K), "ZX Spectrum 48K") == 0);
    ASSERT(strcmp(spec_model_name(SPEC_MODEL_128K), "ZX Spectrum 128K") == 0);
}

TEST(spec_detect_tap)
{
    size_t size;
    uint8_t *data = create_test_tap(&size);
    
    spec_format_t format = spec_detect_format(data, size);
    ASSERT_EQ(format, SPEC_FORMAT_TAP);
    
    free(data);
}

TEST(spec_detect_tzx)
{
    size_t size;
    uint8_t *data = create_test_tzx(&size);
    
    spec_format_t format = spec_detect_format(data, size);
    ASSERT_EQ(format, SPEC_FORMAT_TZX);
    
    free(data);
}

TEST(spec_detect_sna)
{
    size_t size;
    uint8_t *data = create_test_sna_48k(&size);
    
    spec_format_t format = spec_detect_format(data, size);
    ASSERT_EQ(format, SPEC_FORMAT_SNA);
    
    free(data);
}

TEST(spec_open)
{
    size_t size;
    uint8_t *data = create_test_tzx(&size);
    
    spec_file_t file;
    int ret = spec_open(data, size, &file);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(file.data);
    ASSERT_EQ(file.format, SPEC_FORMAT_TZX);
    
    spec_close(&file);
    free(data);
}

TEST(spec_get_info)
{
    size_t size;
    uint8_t *data = create_test_sna_48k(&size);
    
    spec_file_t file;
    spec_open(data, size, &file);
    
    spec_info_t info;
    int ret = spec_get_info(&file, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.model, SPEC_MODEL_48K);
    
    spec_close(&file);
    free(data);
}

/* ============================================================================
 * Sega Saturn/Dreamcast Tests
 * ============================================================================ */

TEST(sega_cd_platform_name)
{
    ASSERT(strcmp(sega_cd_platform_name(SEGA_CD_SATURN), "Sega Saturn") == 0);
    ASSERT(strcmp(sega_cd_platform_name(SEGA_CD_DREAMCAST), "Sega Dreamcast") == 0);
}

TEST(sega_cd_detect_saturn)
{
    size_t size;
    uint8_t *data = create_test_saturn_iso(&size);
    
    sega_cd_platform_t platform = sega_cd_detect_platform(data, size);
    ASSERT_EQ(platform, SEGA_CD_SATURN);
    
    free(data);
}

TEST(sega_cd_detect_dreamcast)
{
    size_t size;
    uint8_t *data = create_test_dreamcast_iso(&size);
    
    sega_cd_platform_t platform = sega_cd_detect_platform(data, size);
    ASSERT_EQ(platform, SEGA_CD_DREAMCAST);
    
    free(data);
}

TEST(sega_cd_open_saturn)
{
    size_t size;
    uint8_t *data = create_test_saturn_iso(&size);
    
    sega_cd_t cd;
    int ret = sega_cd_open(data, size, &cd);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(cd.platform, SEGA_CD_SATURN);
    
    sega_cd_close(&cd);
    free(data);
}

TEST(sega_cd_get_info)
{
    size_t size;
    uint8_t *data = create_test_saturn_iso(&size);
    
    sega_cd_t cd;
    sega_cd_open(data, size, &cd);
    
    sega_cd_info_t info;
    int ret = sega_cd_get_info(&cd, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT(strncmp(info.title, "TEST SATURN", 11) == 0);
    ASSERT_TRUE(info.region_japan);
    ASSERT_TRUE(info.region_usa);
    ASSERT_TRUE(info.region_europe);
    
    sega_cd_close(&cd);
    free(data);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== Floppy & CD Format Tests ===\n\n");
    
    printf("Atari ST:\n");
    RUN_TEST(st_format_name);
    RUN_TEST(st_disk_type_name);
    RUN_TEST(st_detect_format);
    RUN_TEST(st_open);
    RUN_TEST(st_get_info);
    
    printf("\nZX Spectrum:\n");
    RUN_TEST(spec_format_name);
    RUN_TEST(spec_model_name);
    RUN_TEST(spec_detect_tap);
    RUN_TEST(spec_detect_tzx);
    RUN_TEST(spec_detect_sna);
    RUN_TEST(spec_open);
    RUN_TEST(spec_get_info);
    
    printf("\nSega Saturn/Dreamcast:\n");
    RUN_TEST(sega_cd_platform_name);
    RUN_TEST(sega_cd_detect_saturn);
    RUN_TEST(sega_cd_detect_dreamcast);
    RUN_TEST(sega_cd_open_saturn);
    RUN_TEST(sega_cd_get_info);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
