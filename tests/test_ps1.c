/**
 * @file test_ps1.c
 * @brief Unit tests for PlayStation 1 Disc Image Format
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/sony/uft_ps1.h"

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

/* Create minimal BIN image with sync pattern */
static uint8_t *create_test_bin(size_t *size)
{
    /* Minimal: 20 sectors raw format */
    size_t num_sectors = 20;
    size_t total = num_sectors * PS1_SECTOR_RAW;
    uint8_t *data = calloc(1, total);
    
    /* Add sync pattern to each sector */
    for (size_t s = 0; s < num_sectors; s++) {
        uint8_t *sector = data + s * PS1_SECTOR_RAW;
        memcpy(sector, PS1_SYNC_PATTERN, 12);
        
        /* Add MSF header */
        ps1_msf_t msf;
        ps1_lba_to_msf(s, &msf);
        sector[12] = ((msf.minutes / 10) << 4) | (msf.minutes % 10);  /* BCD */
        sector[13] = ((msf.seconds / 10) << 4) | (msf.seconds % 10);
        sector[14] = ((msf.frames / 10) << 4) | (msf.frames % 10);
        sector[15] = 0x02;  /* Mode 2 */
    }
    
    *size = total;
    return data;
}

/* Create minimal ISO image */
static uint8_t *create_test_iso(size_t *size)
{
    /* Minimal: 20 sectors ISO format */
    size_t num_sectors = 20;
    size_t total = num_sectors * PS1_SECTOR_MODE1;
    uint8_t *data = calloc(1, total);
    
    /* Add ISO9660 signature at sector 16 */
    uint8_t *pvd = data + 16 * PS1_SECTOR_MODE1;
    pvd[0] = 0x01;  /* Type */
    memcpy(pvd + 1, "CD001", 5);  /* Standard identifier */
    pvd[6] = 0x01;  /* Version */
    
    *size = total;
    return data;
}

/* Tests */
TEST(detect_type_bin)
{
    size_t size;
    uint8_t *data = create_test_bin(&size);
    
    ps1_image_type_t type = ps1_detect_type(data, size);
    ASSERT_EQ(type, PS1_IMAGE_BIN);
    
    free(data);
}

TEST(detect_type_iso)
{
    size_t size;
    uint8_t *data = create_test_iso(&size);
    
    ps1_image_type_t type = ps1_detect_type(data, size);
    ASSERT_EQ(type, PS1_IMAGE_ISO);
    
    free(data);
}

TEST(detect_sector_size_raw)
{
    size_t size;
    uint8_t *data = create_test_bin(&size);
    
    int sector_size = ps1_detect_sector_size(data, size);
    ASSERT_EQ(sector_size, PS1_SECTOR_RAW);
    
    free(data);
}

TEST(detect_sector_size_iso)
{
    size_t size;
    uint8_t *data = create_test_iso(&size);
    
    int sector_size = ps1_detect_sector_size(data, size);
    ASSERT_EQ(sector_size, PS1_SECTOR_MODE1);
    
    free(data);
}

TEST(type_name)
{
    ASSERT_STR_EQ(ps1_type_name(PS1_IMAGE_BIN), "BIN (2352-byte raw sectors)");
    ASSERT_STR_EQ(ps1_type_name(PS1_IMAGE_ISO), "ISO (2048-byte sectors)");
}

TEST(region_name)
{
    ASSERT_STR_EQ(ps1_region_name(PS1_REGION_NTSC_J), "NTSC-J (Japan)");
    ASSERT_STR_EQ(ps1_region_name(PS1_REGION_NTSC_U), "NTSC-U (USA)");
    ASSERT_STR_EQ(ps1_region_name(PS1_REGION_PAL), "PAL (Europe)");
}

TEST(validate_bin)
{
    size_t size;
    uint8_t *data = create_test_bin(&size);
    
    ASSERT_TRUE(ps1_validate(data, size));
    
    free(data);
}

TEST(validate_iso)
{
    size_t size;
    uint8_t *data = create_test_iso(&size);
    
    ASSERT_TRUE(ps1_validate(data, size));
    
    free(data);
}

TEST(open_bin)
{
    size_t size;
    uint8_t *data = create_test_bin(&size);
    
    ps1_image_t image;
    int ret = ps1_open(data, size, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(image.data);
    ASSERT_EQ(image.type, PS1_IMAGE_BIN);
    ASSERT_EQ(image.sector_size, PS1_SECTOR_RAW);
    ASSERT_EQ(image.num_sectors, 20);
    
    ps1_close(&image);
    free(data);
}

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_bin(&size);
    
    ps1_image_t image;
    ps1_open(data, size, &image);
    
    ps1_info_t info;
    int ret = ps1_get_info(&image, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.type, PS1_IMAGE_BIN);
    ASSERT_EQ(info.num_sectors, 20);
    ASSERT_EQ(info.sector_size, PS1_SECTOR_RAW);
    
    ps1_close(&image);
    free(data);
}

TEST(read_sector)
{
    size_t size;
    uint8_t *data = create_test_bin(&size);
    
    ps1_image_t image;
    ps1_open(data, size, &image);
    
    uint8_t buffer[PS1_SECTOR_RAW];
    int ret = ps1_read_sector(&image, 0, buffer, false);
    
    ASSERT_EQ(ret, PS1_SECTOR_RAW);
    ASSERT(memcmp(buffer, PS1_SYNC_PATTERN, 12) == 0);
    
    ps1_close(&image);
    free(data);
}

TEST(lba_to_msf)
{
    ps1_msf_t msf;
    
    ps1_lba_to_msf(0, &msf);
    ASSERT_EQ(msf.minutes, 0);
    ASSERT_EQ(msf.seconds, 2);
    ASSERT_EQ(msf.frames, 0);
    
    ps1_lba_to_msf(75, &msf);  /* 1 second */
    ASSERT_EQ(msf.minutes, 0);
    ASSERT_EQ(msf.seconds, 3);
    ASSERT_EQ(msf.frames, 0);
}

TEST(msf_to_lba)
{
    ps1_msf_t msf = {0, 2, 0};
    uint32_t lba = ps1_msf_to_lba(&msf);
    ASSERT_EQ(lba, 0);
    
    msf.seconds = 3;
    lba = ps1_msf_to_lba(&msf);
    ASSERT_EQ(lba, 75);
}

TEST(detect_region)
{
    ASSERT_EQ(ps1_detect_region("SCUS-12345"), PS1_REGION_NTSC_U);
    ASSERT_EQ(ps1_detect_region("SLUS-00001"), PS1_REGION_NTSC_U);
    ASSERT_EQ(ps1_detect_region("SCPS-10001"), PS1_REGION_NTSC_J);
    ASSERT_EQ(ps1_detect_region("SLPS-00001"), PS1_REGION_NTSC_J);
    ASSERT_EQ(ps1_detect_region("SCES-00001"), PS1_REGION_PAL);
    ASSERT_EQ(ps1_detect_region("SLES-00001"), PS1_REGION_PAL);
}

TEST(get_track)
{
    size_t size;
    uint8_t *data = create_test_bin(&size);
    
    ps1_image_t image;
    ps1_open(data, size, &image);
    
    ps1_track_t track;
    int ret = ps1_get_track(&image, 1, &track);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(track.number, 1);
    ASSERT_EQ(track.start_lba, 0);
    ASSERT_EQ(track.length, 20);
    
    ps1_close(&image);
    free(data);
}

TEST(close_image)
{
    size_t size;
    uint8_t *data = create_test_bin(&size);
    
    ps1_image_t image;
    ps1_open(data, size, &image);
    ps1_close(&image);
    
    ASSERT_EQ(image.data, NULL);
    
    free(data);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== PlayStation 1 Disc Image Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_type_bin);
    RUN_TEST(detect_type_iso);
    RUN_TEST(detect_sector_size_raw);
    RUN_TEST(detect_sector_size_iso);
    RUN_TEST(type_name);
    RUN_TEST(region_name);
    
    printf("\nValidation:\n");
    RUN_TEST(validate_bin);
    RUN_TEST(validate_iso);
    
    printf("\nImage Operations:\n");
    RUN_TEST(open_bin);
    RUN_TEST(get_info);
    RUN_TEST(read_sector);
    RUN_TEST(close_image);
    
    printf("\nTime Conversion:\n");
    RUN_TEST(lba_to_msf);
    RUN_TEST(msf_to_lba);
    
    printf("\nGame Info:\n");
    RUN_TEST(detect_region);
    RUN_TEST(get_track);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
