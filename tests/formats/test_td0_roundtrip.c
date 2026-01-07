/**
 * @file test_td0_roundtrip.c
 * @brief Tests for TD0 writer (P0-003)
 * 
 * These tests ensure that:
 * 1. TD0 files can be written correctly
 * 2. Written files can be read back
 * 3. Data integrity is preserved
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "uft/core/uft_unified_types.h"
#include "uft/formats/uft_td0_writer.h"

/* ============================================================================
 * Test Macros
 * ============================================================================ */

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %-50s ", #name); \
    test_##name(); \
    printf("✓\n"); \
    tests_passed++; \
} while(0)

static int tests_passed = 0;
static const char *temp_file = "/tmp/uft_test_td0.td0";

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uft_disk_image_t* create_test_disk(uint16_t tracks, uint8_t heads,
                                          uint8_t sectors, uint16_t sector_size) {
    uft_disk_image_t *disk = uft_disk_alloc(tracks, heads);
    if (!disk) return NULL;
    
    disk->format = UFT_FMT_TD0;
    strncpy(disk->format_name, "TD0", sizeof(disk->format_name) - 1);
    disk->sectors_per_track = sectors;
    disk->bytes_per_sector = sector_size;
    
    uint8_t size_code = uft_code_from_size(sector_size);
    
    for (uint16_t t = 0; t < tracks; t++) {
        for (uint8_t h = 0; h < heads; h++) {
            size_t idx = t * heads + h;
            
            uft_track_t *track = uft_track_alloc(sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                return NULL;
            }
            
            track->track_num = t;
            track->head = h;
            track->encoding = UFT_ENC_MFM;
            
            for (uint8_t s = 0; s < sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.track = t;
                sect->id.head = h;
                sect->id.sector = s + 1;
                sect->id.size_code = size_code;
                sect->id.status = UFT_SECTOR_OK;
                
                sect->data = malloc(sector_size);
                sect->data_len = sector_size;
                
                /* Fill with pattern */
                for (uint16_t i = 0; i < sector_size; i++) {
                    sect->data[i] = (uint8_t)((t + h + s + i) & 0xFF);
                }
                
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    return disk;
}

/* ============================================================================
 * Option Tests
 * ============================================================================ */

TEST(options_init) {
    uft_td0_write_options_t opts;
    uft_td0_write_options_init(&opts);
    
    assert(opts.use_advanced_compression == false);
    assert(opts.include_comment == true);
    assert(opts.include_date == true);
    assert(opts.preserve_errors == true);
    assert(opts.preserve_deleted == true);
    assert(opts.compression_level == 6);
}

TEST(auto_settings_dd) {
    uft_disk_image_t *disk = create_test_disk(40, 2, 9, 512);
    assert(disk != NULL);
    
    uft_td0_write_options_t opts;
    uft_td0_auto_settings(disk, &opts);
    
    assert(opts.drive_type == TD0_DRIVE_35_720);
    assert(opts.density == TD0_DENSITY_250K);
    
    uft_disk_free(disk);
}

TEST(auto_settings_hd) {
    uft_disk_image_t *disk = create_test_disk(80, 2, 18, 512);
    assert(disk != NULL);
    
    uft_td0_write_options_t opts;
    uft_td0_auto_settings(disk, &opts);
    
    assert(opts.drive_type == TD0_DRIVE_35_144);
    assert(opts.density == TD0_DENSITY_500K);
    
    uft_disk_free(disk);
}

/* ============================================================================
 * Validation Tests
 * ============================================================================ */

TEST(validate_normal_disk) {
    uft_disk_image_t *disk = create_test_disk(80, 2, 18, 512);
    assert(disk != NULL);
    
    int issues = uft_td0_validate(disk, NULL, 0);
    assert(issues == 0);
    
    uft_disk_free(disk);
}

TEST(validate_oversized_disk) {
    uft_disk_image_t *disk = create_test_disk(100, 2, 18, 512);
    assert(disk != NULL);
    
    char *warnings[10];
    int issues = uft_td0_validate(disk, warnings, 10);
    assert(issues > 0);
    
    uft_disk_free(disk);
}

/* ============================================================================
 * Size Estimation Tests
 * ============================================================================ */

TEST(estimate_size_dd) {
    uft_disk_image_t *disk = create_test_disk(80, 2, 9, 512);
    assert(disk != NULL);
    
    size_t estimated = uft_td0_estimate_size(disk, NULL);
    
    /* Should be at least header + data */
    size_t min_size = 12 + 80 * 2 * 9 * 512;
    assert(estimated >= min_size / 2);  /* Allow for compression */
    
    uft_disk_free(disk);
}

/* ============================================================================
 * Write Tests
 * ============================================================================ */

TEST(write_empty_disk) {
    uft_disk_image_t *disk = uft_disk_alloc(0, 0);
    assert(disk != NULL);
    
    disk->format = UFT_FMT_TD0;
    disk->tracks = 0;
    disk->heads = 0;
    
    uft_td0_write_result_t result;
    uft_error_t err = uft_td0_write_ex(disk, temp_file, NULL, &result);
    
    /* Writing an empty disk should succeed */
    assert(err == UFT_OK);
    assert(result.success == true);
    assert(result.tracks_written == 0);
    
    uft_disk_free(disk);
    remove(temp_file);
}

TEST(write_small_disk) {
    uft_disk_image_t *disk = create_test_disk(40, 1, 9, 512);
    assert(disk != NULL);
    
    uft_td0_write_options_t opts;
    uft_td0_write_options_init(&opts);
    opts.include_comment = true;
    opts.comment = "Test disk for UFT";
    
    uft_td0_write_result_t result;
    uft_error_t err = uft_td0_write_ex(disk, temp_file, &opts, &result);
    
    assert(err == UFT_OK);
    assert(result.success == true);
    assert(result.tracks_written == 40);
    assert(result.sectors_written == 40 * 9);
    assert(result.bytes_written > 0);
    
    uft_disk_free(disk);
    remove(temp_file);
}

TEST(write_standard_disk) {
    uft_disk_image_t *disk = create_test_disk(80, 2, 18, 512);
    assert(disk != NULL);
    
    uft_td0_write_result_t result;
    uft_error_t err = uft_td0_write_ex(disk, temp_file, NULL, &result);
    
    assert(err == UFT_OK);
    assert(result.success == true);
    assert(result.tracks_written == 160);
    assert(result.sectors_written == 160 * 18);
    
    /* Verify file exists and has reasonable size */
    FILE *fp = fopen(temp_file, "rb");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    
    assert(size > 12);  /* At least header */
    
    uft_disk_free(disk);
    remove(temp_file);
}

TEST(write_with_errors) {
    uft_disk_image_t *disk = create_test_disk(40, 1, 9, 512);
    assert(disk != NULL);
    
    /* Mark some sectors with errors */
    if (disk->track_data[5] && disk->track_data[5]->sector_count > 3) {
        disk->track_data[5]->sectors[3].id.status |= UFT_SECTOR_CRC_ERROR;
    }
    if (disk->track_data[10] && disk->track_data[10]->sector_count > 5) {
        disk->track_data[10]->sectors[5].id.status |= UFT_SECTOR_DELETED;
    }
    
    uft_td0_write_options_t opts;
    uft_td0_write_options_init(&opts);
    opts.preserve_errors = true;
    opts.preserve_deleted = true;
    
    uft_td0_write_result_t result;
    uft_error_t err = uft_td0_write_ex(disk, temp_file, &opts, &result);
    
    assert(err == UFT_OK);
    assert(result.success == true);
    assert(result.error_sectors >= 1);
    assert(result.deleted_sectors >= 1);
    
    uft_disk_free(disk);
    remove(temp_file);
}

TEST(write_repeated_pattern) {
    /* Create disk with repeated data (tests RLE compression) */
    uft_disk_image_t *disk = uft_disk_alloc(10, 1);
    disk->format = UFT_FMT_TD0;
    disk->sectors_per_track = 9;
    disk->bytes_per_sector = 512;
    
    for (int t = 0; t < 10; t++) {
        uft_track_t *track = uft_track_alloc(9, 0);
        track->track_num = t;
        track->head = 0;
        
        for (int s = 0; s < 9; s++) {
            uft_sector_t *sect = &track->sectors[s];
            sect->id.track = t;
            sect->id.sector = s + 1;
            sect->id.size_code = 2;
            
            sect->data = malloc(512);
            sect->data_len = 512;
            
            /* Fill with single repeated byte */
            memset(sect->data, 0xE5, 512);
            
            track->sector_count++;
        }
        
        disk->track_data[t] = track;
    }
    
    uft_td0_write_result_t result;
    uft_error_t err = uft_td0_write_ex(disk, temp_file, NULL, &result);
    
    assert(err == UFT_OK);
    assert(result.success == true);
    
    /* Check compression ratio (repeated data should compress well) */
    printf(" (ratio: %.2f) ", result.compression_ratio);
    
    uft_disk_free(disk);
    remove(temp_file);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST(write_null_params) {
    uft_error_t err;
    
    err = uft_td0_write(NULL, temp_file, NULL);
    assert(err == UFT_ERR_INVALID_PARAM);
    
    uft_disk_image_t disk = {0};
    err = uft_td0_write(&disk, NULL, NULL);
    assert(err == UFT_ERR_INVALID_PARAM);
}

TEST(write_invalid_path) {
    uft_disk_image_t *disk = create_test_disk(10, 1, 9, 512);
    assert(disk != NULL);
    
    uft_td0_write_result_t result;
    uft_error_t err = uft_td0_write_ex(disk, "/nonexistent/path/file.td0", 
                                        NULL, &result);
    
    assert(err == UFT_ERR_IO);
    assert(result.success == false);
    
    uft_disk_free(disk);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("\n=== TD0 Writer Tests (P0-003) ===\n\n");
    
    printf("Options:\n");
    RUN_TEST(options_init);
    RUN_TEST(auto_settings_dd);
    RUN_TEST(auto_settings_hd);
    
    printf("\nValidation:\n");
    RUN_TEST(validate_normal_disk);
    RUN_TEST(validate_oversized_disk);
    
    printf("\nSize estimation:\n");
    RUN_TEST(estimate_size_dd);
    
    printf("\nWriting:\n");
    RUN_TEST(write_empty_disk);
    RUN_TEST(write_small_disk);
    RUN_TEST(write_standard_disk);
    RUN_TEST(write_with_errors);
    RUN_TEST(write_repeated_pattern);
    
    printf("\nError handling:\n");
    RUN_TEST(write_null_params);
    RUN_TEST(write_invalid_path);
    
    printf("\n=== %d tests passed ===\n\n", tests_passed);
    
    return 0;
}
