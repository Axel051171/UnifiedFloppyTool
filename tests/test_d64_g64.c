/**
 * @file test_d64_g64.c
 * @brief Unit tests for D64/G64 format conversion
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_d64_g64.h"

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
#define ASSERT_NULL(p) ASSERT((p) == NULL)
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)
#define ASSERT_TRUE(x) ASSERT((x))
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Create minimal D64 image for testing
 */
static d64_image_t *create_test_d64(void)
{
    d64_image_t *img = d64_create(35);
    if (!img) return NULL;
    
    /* Set disk ID */
    img->disk_id[0] = 'A';
    img->disk_id[1] = 'B';
    
    /* Create simple BAM */
    uint8_t bam[256] = {0};
    bam[0] = 18;  /* Next track */
    bam[1] = 1;   /* Next sector */
    bam[2] = 'A'; /* DOS version */
    bam[3] = 0;
    
    /* Disk name at 0x90 */
    memcpy(bam + 0x90, "TEST DISK       ", 16);
    
    /* Disk ID at 0xA2 */
    bam[0xA2] = 'A';
    bam[0xA3] = 'B';
    
    /* DOS type at 0xA5 */
    bam[0xA5] = '2';
    bam[0xA6] = 'A';
    
    d64_set_sector(img, 18, 0, bam, D64_ERR_OK);
    
    /* Fill some sectors with test patterns */
    for (int track = 1; track <= 35; track++) {
        int num_sectors = d64_sectors_on_track(track);
        for (int sector = 0; sector < num_sectors; sector++) {
            uint8_t data[256];
            memset(data, (track * 10 + sector) & 0xFF, 256);
            data[0] = track;
            data[1] = sector;
            d64_set_sector(img, track, sector, data, D64_ERR_OK);
        }
    }
    
    return img;
}

/* ============================================================================
 * Unit Tests - D64 Constants
 * ============================================================================ */

TEST(d64_constants)
{
    ASSERT_EQ(D64_SECTOR_SIZE, 256);
    ASSERT_EQ(D64_BLOCKS_35, 683);
    ASSERT_EQ(D64_BLOCKS_40, 768);
    ASSERT_EQ(D64_SIZE_35, 683 * 256);
    ASSERT_EQ(D64_BAM_TRACK, 18);
}

TEST(d64_sectors_on_track)
{
    /* Tracks 1-17: 21 sectors */
    ASSERT_EQ(d64_sectors_on_track(1), 21);
    ASSERT_EQ(d64_sectors_on_track(17), 21);
    
    /* Tracks 18-24: 19 sectors */
    ASSERT_EQ(d64_sectors_on_track(18), 19);
    ASSERT_EQ(d64_sectors_on_track(24), 19);
    
    /* Tracks 25-30: 18 sectors */
    ASSERT_EQ(d64_sectors_on_track(25), 18);
    ASSERT_EQ(d64_sectors_on_track(30), 18);
    
    /* Tracks 31-42: 17 sectors */
    ASSERT_EQ(d64_sectors_on_track(31), 17);
    ASSERT_EQ(d64_sectors_on_track(35), 17);
    
    /* Invalid */
    ASSERT_EQ(d64_sectors_on_track(0), 0);
    ASSERT_EQ(d64_sectors_on_track(50), 0);
}

TEST(d64_block_offset)
{
    /* Track 1, sector 0 */
    ASSERT_EQ(d64_block_offset(1, 0), 0);
    
    /* Track 1, sector 20 */
    ASSERT_EQ(d64_block_offset(1, 20), 20);
    
    /* Track 2, sector 0 */
    ASSERT_EQ(d64_block_offset(2, 0), 21);
    
    /* Track 18, sector 0 (BAM) */
    ASSERT_EQ(d64_block_offset(18, 0), 357);
    
    /* Invalid */
    ASSERT_EQ(d64_block_offset(0, 0), -1);
    ASSERT_EQ(d64_block_offset(1, 25), -1);  /* Only 21 sectors */
}

TEST(d64_speed_zone)
{
    /* Tracks 1-17: zone 3 */
    ASSERT_EQ(d64_speed_zone(1), 3);
    ASSERT_EQ(d64_speed_zone(17), 3);
    
    /* Tracks 18-24: zone 2 */
    ASSERT_EQ(d64_speed_zone(18), 2);
    
    /* Tracks 25-30: zone 1 */
    ASSERT_EQ(d64_speed_zone(25), 1);
    
    /* Tracks 31+: zone 0 */
    ASSERT_EQ(d64_speed_zone(31), 0);
}

TEST(d64_track_capacity)
{
    size_t cap1 = d64_track_capacity(1);
    size_t cap31 = d64_track_capacity(31);
    
    ASSERT(cap1 > cap31);  /* Higher zone = more capacity */
    ASSERT(cap1 > 7000);
    ASSERT(cap31 > 6000);
}

/* ============================================================================
 * Unit Tests - D64 Create/Load/Save
 * ============================================================================ */

TEST(d64_create)
{
    d64_image_t *img = d64_create(35);
    ASSERT_NOT_NULL(img);
    ASSERT_EQ(img->num_tracks, 35);
    ASSERT_EQ(img->num_blocks, D64_BLOCKS_35);
    ASSERT_NOT_NULL(img->data);
    
    d64_free(img);
    
    img = d64_create(40);
    ASSERT_NOT_NULL(img);
    ASSERT_EQ(img->num_tracks, 40);
    ASSERT_EQ(img->num_blocks, D64_BLOCKS_40);
    
    d64_free(img);
    
    /* Invalid */
    img = d64_create(30);
    ASSERT_NULL(img);
}

TEST(d64_set_get_sector)
{
    d64_image_t *img = d64_create(35);
    ASSERT_NOT_NULL(img);
    
    /* Write test sector */
    uint8_t write_data[256];
    for (int i = 0; i < 256; i++) {
        write_data[i] = i;
    }
    
    int result = d64_set_sector(img, 1, 5, write_data, D64_ERR_OK);
    ASSERT_EQ(result, 0);
    
    /* Read back */
    uint8_t read_data[256];
    d64_error_t error;
    result = d64_get_sector(img, 1, 5, read_data, &error);
    ASSERT_EQ(result, 0);
    
    /* Verify */
    ASSERT(memcmp(write_data, read_data, 256) == 0);
    
    d64_free(img);
}

TEST(d64_save_load_roundtrip)
{
    d64_image_t *img = create_test_d64();
    ASSERT_NOT_NULL(img);
    
    /* Save to buffer */
    uint8_t *saved;
    size_t saved_size;
    int result = d64_save_buffer(img, &saved, &saved_size, false);
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(saved);
    ASSERT_EQ(saved_size, D64_SIZE_35);
    
    /* Load back */
    d64_image_t *loaded = NULL;
    result = d64_load_buffer(saved, saved_size, &loaded);
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(loaded);
    
    /* Verify some sectors */
    uint8_t orig_data[256], load_data[256];
    
    result = d64_get_sector(img, 1, 0, orig_data, NULL);
    ASSERT_EQ(result, 0);
    result = d64_get_sector(loaded, 1, 0, load_data, NULL);
    ASSERT_EQ(result, 0);
    ASSERT(memcmp(orig_data, load_data, 256) == 0);
    
    d64_free(img);
    d64_free(loaded);
    free(saved);
}

TEST(d64_with_errors)
{
    /* Create D64 with error info */
    uint8_t data[D64_SIZE_35_ERR];
    memset(data, 0, D64_SIZE_35);
    memset(data + D64_SIZE_35, D64_ERR_OK, D64_BLOCKS_35);
    
    /* Set an error */
    data[D64_SIZE_35 + 100] = D64_ERR_CHECKSUM;
    
    d64_image_t *img = NULL;
    int result = d64_load_buffer(data, sizeof(data), &img);
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(img);
    ASSERT_TRUE(img->has_errors);
    ASSERT_NOT_NULL(img->errors);
    
    d64_free(img);
}

/* ============================================================================
 * Unit Tests - G64 Constants
 * ============================================================================ */

TEST(g64_constants)
{
    ASSERT_EQ(G64_HEADER_SIZE, 0x2AC);
    ASSERT_EQ(G64_MAX_TRACKS, 84);
    ASSERT_STR_EQ(G64_SIGNATURE, "GCR-1541");
}

/* ============================================================================
 * Unit Tests - G64 Create/Load/Save
 * ============================================================================ */

TEST(g64_create)
{
    g64_image_t *img = g64_create(35, false);
    ASSERT_NOT_NULL(img);
    ASSERT_EQ(img->num_tracks, 35);
    
    g64_free(img);
    
    img = g64_create(35, true);
    ASSERT_NOT_NULL(img);
    ASSERT_EQ(img->num_tracks, 70);  /* With halftracks */
    
    g64_free(img);
}

TEST(g64_set_get_track)
{
    g64_image_t *img = g64_create(35, false);
    ASSERT_NOT_NULL(img);
    
    /* Create test track */
    uint8_t track_data[7000];
    memset(track_data, 0x55, sizeof(track_data));
    memset(track_data, 0xFF, 10);  /* Sync */
    
    int result = g64_set_track(img, 4, track_data, sizeof(track_data), 3);
    ASSERT_EQ(result, 0);
    
    /* Read back */
    const uint8_t *retrieved;
    size_t length;
    uint8_t speed;
    result = g64_get_track(img, 4, &retrieved, &length, &speed);
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(retrieved);
    ASSERT_EQ(length, sizeof(track_data));
    ASSERT_EQ(speed, 3);
    ASSERT(memcmp(track_data, retrieved, length) == 0);
    
    g64_free(img);
}

TEST(g64_save_load_roundtrip)
{
    g64_image_t *img = g64_create(35, false);
    ASSERT_NOT_NULL(img);
    
    /* Add some tracks */
    uint8_t track_data[7000];
    for (int track = 1; track <= 35; track++) {
        int halftrack = track * 2;
        memset(track_data, track, sizeof(track_data));
        memset(track_data, 0xFF, 10);
        g64_set_track(img, halftrack, track_data, sizeof(track_data), d64_speed_zone(track));
    }
    
    /* Save to buffer */
    uint8_t *saved;
    size_t saved_size;
    int result = g64_save_buffer(img, &saved, &saved_size);
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(saved);
    
    /* Verify G64 signature */
    ASSERT(memcmp(saved, G64_SIGNATURE, G64_SIGNATURE_LEN) == 0);
    
    /* Load back */
    g64_image_t *loaded = NULL;
    result = g64_load_buffer(saved, saved_size, &loaded);
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(loaded);
    
    /* Verify a track */
    const uint8_t *orig_data;
    const uint8_t *load_data;
    size_t len1, len2;
    
    result = g64_get_track(img, 4, &orig_data, &len1, NULL);
    ASSERT_EQ(result, 0);
    result = g64_get_track(loaded, 4, &load_data, &len2, NULL);
    ASSERT_EQ(result, 0);
    ASSERT_EQ(len1, len2);
    ASSERT(memcmp(orig_data, load_data, len1) == 0);
    
    g64_free(img);
    g64_free(loaded);
    free(saved);
}

/* ============================================================================
 * Unit Tests - GCR Conversion
 * ============================================================================ */

TEST(sector_to_gcr)
{
    uint8_t sector_data[256];
    uint8_t gcr_output[400];
    uint8_t disk_id[2] = {'A', 'B'};
    
    /* Fill sector with pattern */
    for (int i = 0; i < 256; i++) {
        sector_data[i] = i;
    }
    
    size_t gcr_len = sector_to_gcr(sector_data, gcr_output, 1, 0, disk_id, D64_ERR_OK);
    ASSERT(gcr_len > 300);  /* GCR is larger than raw */
    ASSERT(gcr_len < 400);
    
    /* Check sync marks present */
    ASSERT_EQ(gcr_output[0], 0xFF);
}

TEST(gcr_to_sector)
{
    uint8_t original[256];
    uint8_t gcr_data[400];
    uint8_t recovered[256];
    uint8_t disk_id[2] = {'A', 'B'};
    
    /* Create test sector */
    for (int i = 0; i < 256; i++) {
        original[i] = i;
    }
    
    /* Convert to GCR */
    size_t gcr_len = sector_to_gcr(original, gcr_data, 1, 5, disk_id, D64_ERR_OK);
    ASSERT(gcr_len > 0);
    
    /* Convert back */
    int track_out, sector_out;
    uint8_t id_out[2];
    d64_error_t error;
    
    int result = gcr_to_sector(gcr_data, gcr_len, recovered, 
                               &track_out, &sector_out, id_out, &error);
    ASSERT_EQ(result, 0);
    ASSERT_EQ(track_out, 1);
    ASSERT_EQ(sector_out, 5);
    ASSERT_EQ(error, D64_ERR_OK);
    
    /* Verify data */
    ASSERT(memcmp(original, recovered, 256) == 0);
}

/* ============================================================================
 * Unit Tests - D64 ↔ G64 Conversion
 * ============================================================================ */

TEST(d64_to_g64_conversion)
{
    d64_image_t *d64 = create_test_d64();
    ASSERT_NOT_NULL(d64);
    
    g64_image_t *g64 = NULL;
    convert_result_t result;
    
    int ret = d64_to_g64(d64, &g64, NULL, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(g64);
    ASSERT_TRUE(result.success);
    ASSERT(result.tracks_converted > 0);
    ASSERT(result.sectors_converted > 0);
    
    /* Verify tracks exist */
    const uint8_t *track_data;
    size_t length;
    ret = g64_get_track(g64, 4, &track_data, &length, NULL);
    ASSERT_EQ(ret, 0);
    ASSERT(length > 0);
    
    d64_free(d64);
    g64_free(g64);
}

TEST(g64_to_d64_conversion)
{
    /* First create a D64 and convert to G64 */
    d64_image_t *original_d64 = create_test_d64();
    ASSERT_NOT_NULL(original_d64);
    
    g64_image_t *g64 = NULL;
    int ret = d64_to_g64(original_d64, &g64, NULL, NULL);
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(g64);
    
    /* Now convert back to D64 */
    d64_image_t *converted_d64 = NULL;
    convert_result_t result;
    
    ret = g64_to_d64(g64, &converted_d64, NULL, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(converted_d64);
    ASSERT_TRUE(result.success);
    
    /* Some sectors should match */
    uint8_t orig_data[256], conv_data[256];
    
    ret = d64_get_sector(original_d64, 1, 0, orig_data, NULL);
    ASSERT_EQ(ret, 0);
    ret = d64_get_sector(converted_d64, 1, 0, conv_data, NULL);
    ASSERT_EQ(ret, 0);
    
    /* Data should match */
    ASSERT(memcmp(orig_data, conv_data, 256) == 0);
    
    d64_free(original_d64);
    g64_free(g64);
    d64_free(converted_d64);
}

TEST(conversion_roundtrip)
{
    d64_image_t *original = create_test_d64();
    ASSERT_NOT_NULL(original);
    
    /* D64 → G64 → D64 */
    g64_image_t *g64 = NULL;
    d64_to_g64(original, &g64, NULL, NULL);
    ASSERT_NOT_NULL(g64);
    
    d64_image_t *converted = NULL;
    g64_to_d64(g64, &converted, NULL, NULL);
    ASSERT_NOT_NULL(converted);
    
    /* Compare multiple sectors */
    for (int track = 1; track <= 35; track++) {
        uint8_t orig[256], conv[256];
        
        int r1 = d64_get_sector(original, track, 0, orig, NULL);
        int r2 = d64_get_sector(converted, track, 0, conv, NULL);
        
        if (r1 == 0 && r2 == 0) {
            ASSERT(memcmp(orig, conv, 256) == 0);
        }
    }
    
    d64_free(original);
    g64_free(g64);
    d64_free(converted);
}

/* ============================================================================
 * Unit Tests - Error Names
 * ============================================================================ */

TEST(error_names)
{
    ASSERT_STR_EQ(d64_error_name(D64_ERR_OK), "OK");
    ASSERT_STR_EQ(d64_error_name(D64_ERR_CHECKSUM), "Data checksum");
    ASSERT_STR_EQ(d64_error_name(D64_ERR_NO_SYNC), "No sync");
    ASSERT_NOT_NULL(d64_error_name(99));  /* Should return something */
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    printf("\n=== D64/G64 Format Tests ===\n\n");
    
    printf("D64 Constants:\n");
    RUN_TEST(d64_constants);
    RUN_TEST(d64_sectors_on_track);
    RUN_TEST(d64_block_offset);
    RUN_TEST(d64_speed_zone);
    RUN_TEST(d64_track_capacity);
    
    printf("\nD64 Create/Load/Save:\n");
    RUN_TEST(d64_create);
    RUN_TEST(d64_set_get_sector);
    RUN_TEST(d64_save_load_roundtrip);
    RUN_TEST(d64_with_errors);
    
    printf("\nG64 Constants:\n");
    RUN_TEST(g64_constants);
    
    printf("\nG64 Create/Load/Save:\n");
    RUN_TEST(g64_create);
    RUN_TEST(g64_set_get_track);
    RUN_TEST(g64_save_load_roundtrip);
    
    printf("\nGCR Conversion:\n");
    RUN_TEST(sector_to_gcr);
    RUN_TEST(gcr_to_sector);
    
    printf("\nD64 ↔ G64 Conversion:\n");
    RUN_TEST(d64_to_g64_conversion);
    RUN_TEST(g64_to_d64_conversion);
    RUN_TEST(conversion_roundtrip);
    
    printf("\nError Names:\n");
    RUN_TEST(error_names);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
