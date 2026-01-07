/**
 * @file test_unified_types.c
 * @brief Tests for unified data types (P0-001, P0-002)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "uft/core/uft_unified_types.h"

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

/* ============================================================================
 * Error Code Tests
 * ============================================================================ */

TEST(error_str) {
    assert(strcmp(uft_error_str(UFT_OK), "OK") == 0);
    assert(strcmp(uft_error_str(UFT_ERR_CRC), "CRC error") == 0);
    assert(strcmp(uft_error_str(UFT_ERR_MEMORY), "Memory error") == 0);
    assert(strcmp(uft_error_str(0xFE), "Unknown error") == 0);
}

TEST(error_recoverable) {
    assert(uft_error_recoverable(UFT_ERR_CRC) == true);
    assert(uft_error_recoverable(UFT_ERR_TIMEOUT) == true);
    assert(uft_error_recoverable(UFT_ERR_WRITE_PROTECT) == false);
    assert(uft_error_recoverable(UFT_ERR_COPY_DENIED) == false);
}

/* ============================================================================
 * Sector ID Tests
 * ============================================================================ */

TEST(sector_id_basic) {
    uft_sector_id_t id = {0};
    
    id.track = 17;
    id.head = 0;
    id.sector = 1;
    id.size_code = 2;  /* 512 bytes */
    id.status = UFT_SECTOR_OK;
    
    assert(id.track == 17);
    assert(id.head == 0);
    assert(id.sector == 1);
    assert(uft_size_from_code(id.size_code) == 512);
}

TEST(sector_id_status_flags) {
    uft_sector_id_t id = {0};
    
    id.status = UFT_SECTOR_CRC_ERROR | UFT_SECTOR_WEAK;
    
    assert((id.status & UFT_SECTOR_CRC_ERROR) != 0);
    assert((id.status & UFT_SECTOR_WEAK) != 0);
    assert((id.status & UFT_SECTOR_DELETED) == 0);
}

TEST(sector_size_conversion) {
    assert(uft_size_from_code(0) == 128);
    assert(uft_size_from_code(1) == 256);
    assert(uft_size_from_code(2) == 512);
    assert(uft_size_from_code(3) == 1024);
    assert(uft_size_from_code(4) == 2048);
    assert(uft_size_from_code(5) == 4096);
    assert(uft_size_from_code(6) == 8192);
    assert(uft_size_from_code(7) == 16384);
    assert(uft_size_from_code(8) == 0);  /* Invalid */
    
    assert(uft_code_from_size(128) == 0);
    assert(uft_code_from_size(256) == 1);
    assert(uft_code_from_size(512) == 2);
    assert(uft_code_from_size(1024) == 3);
    assert(uft_code_from_size(999) == 2);  /* Default to 512 */
}

/* ============================================================================
 * Sector Memory Tests
 * ============================================================================ */

TEST(sector_alloc_free) {
    uft_sector_t *sector = uft_sector_alloc(512);
    assert(sector != NULL);
    assert(sector->data != NULL);
    assert(sector->data_len == 512);
    
    /* Initialize data */
    memset(sector->data, 0xAA, 512);
    
    uft_sector_free(sector);
    /* No crash = success */
}

TEST(sector_alloc_zero_size) {
    uft_sector_t *sector = uft_sector_alloc(0);
    assert(sector != NULL);
    assert(sector->data == NULL);
    assert(sector->data_len == 0);
    
    uft_sector_free(sector);
}

TEST(sector_copy) {
    uft_sector_t src = {0};
    src.id.track = 5;
    src.id.sector = 10;
    src.id.size_code = 2;
    src.data_len = 512;
    src.data = malloc(512);
    memset(src.data, 0x55, 512);
    src.crc_stored = 0x1234;
    src.crc_calculated = 0x1234;
    src.crc_valid = true;
    
    uft_sector_t dest = {0};
    int ret = uft_sector_copy(&dest, &src);
    assert(ret == 0);
    
    assert(dest.id.track == src.id.track);
    assert(dest.id.sector == src.id.sector);
    assert(dest.data_len == src.data_len);
    assert(dest.data != src.data);  /* Deep copy */
    assert(memcmp(dest.data, src.data, 512) == 0);
    assert(dest.crc_valid == true);
    
    free(src.data);
    free(dest.data);
}

/* ============================================================================
 * Track Memory Tests
 * ============================================================================ */

TEST(track_alloc_free) {
    uft_track_t *track = uft_track_alloc(18, 50000);
    assert(track != NULL);
    assert(track->sectors != NULL);
    assert(track->sector_capacity == 18);
    assert(track->raw_data != NULL);
    assert(track->raw_capacity >= 50000 / 8);
    assert(track->owns_data == true);
    
    uft_track_free(track);
}

TEST(track_add_sectors) {
    uft_track_t *track = uft_track_alloc(18, 100000);
    
    track->track_num = 0;
    track->head = 0;
    track->encoding = UFT_ENC_MFM;
    
    /* Add sectors */
    for (int i = 0; i < 18; i++) {
        track->sectors[i].id.track = 0;
        track->sectors[i].id.head = 0;
        track->sectors[i].id.sector = i + 1;
        track->sectors[i].id.size_code = 2;
        track->sectors[i].data = malloc(512);
        track->sectors[i].data_len = 512;
        track->sector_count++;
    }
    
    assert(track->sector_count == 18);
    
    uft_track_free(track);
}

/* ============================================================================
 * Disk Image Tests
 * ============================================================================ */

TEST(disk_alloc_free) {
    uft_disk_image_t *disk = uft_disk_alloc(80, 2);
    assert(disk != NULL);
    assert(disk->tracks == 80);
    assert(disk->heads == 2);
    assert(disk->track_count == 160);
    assert(disk->track_data != NULL);
    assert(disk->owns_data == true);
    
    uft_disk_free(disk);
}

TEST(disk_add_tracks) {
    uft_disk_image_t *disk = uft_disk_alloc(40, 1);
    
    disk->format = UFT_FMT_D64;
    strncpy(disk->format_name, "D64", sizeof(disk->format_name) - 1);
    disk->sectors_per_track = 21;  /* Average for D64 */
    disk->bytes_per_sector = 256;
    
    /* Add a track */
    disk->track_data[0] = uft_track_alloc(21, 50000);
    disk->track_data[0]->track_num = 0;
    disk->track_data[0]->head = 0;
    disk->track_data[0]->encoding = UFT_ENC_GCR_C64;
    
    assert(disk->track_data[0] != NULL);
    
    uft_disk_free(disk);
}

TEST(disk_compare_identical) {
    uft_disk_image_t *a = uft_disk_alloc(40, 1);
    uft_disk_image_t *b = uft_disk_alloc(40, 1);
    
    a->format = UFT_FMT_D64;
    b->format = UFT_FMT_D64;
    a->sectors_per_track = 21;
    b->sectors_per_track = 21;
    a->bytes_per_sector = 256;
    b->bytes_per_sector = 256;
    
    uft_compare_result_t result;
    int ret = uft_disk_compare(a, b, &result);
    
    assert(ret == 0);
    assert(result == UFT_CMP_IDENTICAL);
    
    uft_disk_free(a);
    uft_disk_free(b);
}

TEST(disk_compare_geometry_differs) {
    uft_disk_image_t *a = uft_disk_alloc(40, 1);
    uft_disk_image_t *b = uft_disk_alloc(80, 2);
    
    uft_compare_result_t result;
    int ret = uft_disk_compare(a, b, &result);
    
    assert(ret == 0);
    assert((result & UFT_CMP_GEOMETRY_DIFFERS) != 0);
    
    uft_disk_free(a);
    uft_disk_free(b);
}

/* ============================================================================
 * Format Name Tests
 * ============================================================================ */

TEST(format_names) {
    assert(strcmp(uft_format_name(UFT_FMT_D64), "D64") == 0);
    assert(strcmp(uft_format_name(UFT_FMT_ADF), "ADF") == 0);
    assert(strcmp(uft_format_name(UFT_FMT_SCP), "SCP") == 0);
    assert(strcmp(uft_format_name(UFT_FMT_HFE), "HFE") == 0);
    assert(strcmp(uft_format_name(UFT_FMT_UNKNOWN), "Unknown") == 0);
}

TEST(encoding_names) {
    assert(strcmp(uft_encoding_name(UFT_ENC_MFM), "MFM") == 0);
    assert(strcmp(uft_encoding_name(UFT_ENC_FM), "FM") == 0);
    assert(strcmp(uft_encoding_name(UFT_ENC_GCR_C64), "GCR (C64)") == 0);
}

/* ============================================================================
 * Compatibility Macro Tests
 * ============================================================================ */

#ifdef UFT_COMPAT_LEGACY_TYPES
TEST(compat_macros) {
    uft_sector_id_t id = {0};
    id.track = 17;
    id.head = 0;
    id.sector = 1;
    id.size_code = 2;
    
    assert(UFT_SECTOR_CYLINDER(&id) == 17);
    assert(UFT_SECTOR_SIDE(&id) == 0);
    assert(UFT_SECTOR_NUM(&id) == 1);
    assert(UFT_SECTOR_SIZE(&id) == 512);
}
#endif

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("\n=== Unified Types Tests ===\n\n");
    
    printf("Error handling:\n");
    RUN_TEST(error_str);
    RUN_TEST(error_recoverable);
    
    printf("\nSector ID:\n");
    RUN_TEST(sector_id_basic);
    RUN_TEST(sector_id_status_flags);
    RUN_TEST(sector_size_conversion);
    
    printf("\nSector memory:\n");
    RUN_TEST(sector_alloc_free);
    RUN_TEST(sector_alloc_zero_size);
    RUN_TEST(sector_copy);
    
    printf("\nTrack memory:\n");
    RUN_TEST(track_alloc_free);
    RUN_TEST(track_add_sectors);
    
    printf("\nDisk image:\n");
    RUN_TEST(disk_alloc_free);
    RUN_TEST(disk_add_tracks);
    RUN_TEST(disk_compare_identical);
    RUN_TEST(disk_compare_geometry_differs);
    
    printf("\nFormat names:\n");
    RUN_TEST(format_names);
    RUN_TEST(encoding_names);
    
    #ifdef UFT_COMPAT_LEGACY_TYPES
    printf("\nCompatibility macros:\n");
    RUN_TEST(compat_macros);
    #endif
    
    printf("\n=== %d tests passed ===\n\n", tests_passed);
    
    return 0;
}
