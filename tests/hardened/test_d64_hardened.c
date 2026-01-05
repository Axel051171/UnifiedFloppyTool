/**
 * @file test_d64_hardened.c
 * @brief Unit tests for hardened D64 parser
 */

#include "uft/formats/d64_hardened.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PASS() printf("  ✓ PASS\n")
#define TEST_FAIL(msg) do { printf("  ✗ FAIL: %s\n", msg); return 1; } while(0)

// ============================================================================
// Mock Data
// ============================================================================

static int create_mock_d64(const char *path, int tracks, bool with_errors) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    // Calculate size
    uint16_t total_sectors = 0;
    static const uint8_t spt[] = {
        21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
        19,19,19,19,19,19,19,
        18,18,18,18,18,18,
        17,17,17,17,17,17,17,17,17,17,17,17
    };
    
    for (int t = 0; t < tracks && t < 42; t++) {
        total_sectors += spt[t];
    }
    
    // Write sectors
    uint8_t sector[256] = {0};
    for (uint16_t s = 0; s < total_sectors; s++) {
        // BAM at track 18, sector 0 (sector 357)
        if (s == 357) {
            memset(sector, 0, 256);
            sector[0] = 18;   // Directory track
            sector[1] = 1;    // Directory sector
            sector[2] = 0x41; // DOS version 'A'
            // Disk name at 144
            memcpy(&sector[144], "TEST DISK       ", 16);
            sector[162] = 'I'; // ID
            sector[163] = 'D';
            sector[164] = 0xA0;
            sector[165] = '2';
            sector[166] = 'A';
        } else {
            memset(sector, 0, 256);
        }
        fwrite(sector, 256, 1, f);
    }
    
    // Write error info if requested
    if (with_errors) {
        uint8_t *errors = malloc(total_sectors);
        memset(errors, 0x01, total_sectors);  // All OK
        errors[0] = 0x05;  // Checksum error on first sector
        fwrite(errors, total_sectors, 1, f);
        free(errors);
    }
    
    fclose(f);
    return 0;
}

// ============================================================================
// Tests
// ============================================================================

static int test_open_valid_35(void) {
    printf("Test: Open valid 35-track D64\n");
    
    const char *path = "/tmp/test_d64_35.d64";
    if (create_mock_d64(path, 35, false) != 0) {
        TEST_FAIL("Failed to create mock file");
    }
    
    uft_d64_image_hardened_t *img = NULL;
    uft_d64_error_t rc = uft_d64_open_safe(path, true, &img);
    
    if (rc != UFT_D64_OK) {
        printf("  Error: %s\n", uft_d64_error_string(rc));
        TEST_FAIL("Open failed");
    }
    
    uint8_t num_tracks;
    uint16_t total_sectors;
    bool has_errors;
    
    uft_d64_get_geometry(img, &num_tracks, &total_sectors, &has_errors);
    
    if (num_tracks != 35) {
        TEST_FAIL("Wrong track count");
    }
    
    if (total_sectors != 683) {
        TEST_FAIL("Wrong sector count");
    }
    
    if (has_errors) {
        TEST_FAIL("Should not have errors");
    }
    
    uft_d64_close_safe(&img);
    remove(path);
    TEST_PASS();
    return 0;
}

static int test_open_with_errors(void) {
    printf("Test: Open D64 with error info\n");
    
    const char *path = "/tmp/test_d64_err.d64";
    if (create_mock_d64(path, 35, true) != 0) {
        TEST_FAIL("Failed to create mock file");
    }
    
    uft_d64_image_hardened_t *img = NULL;
    uft_d64_error_t rc = uft_d64_open_safe(path, true, &img);
    
    if (rc != UFT_D64_OK) {
        TEST_FAIL("Open failed");
    }
    
    uint8_t num_tracks;
    uint16_t total_sectors;
    bool has_errors;
    
    uft_d64_get_geometry(img, &num_tracks, &total_sectors, &has_errors);
    
    if (!has_errors) {
        TEST_FAIL("Should have errors");
    }
    
    // Read sector with error
    uft_d64_sector_t sector;
    rc = uft_d64_read_sector(img, 1, 0, &sector);
    
    if (rc != UFT_D64_OK) {
        TEST_FAIL("Read sector failed");
    }
    
    if (sector.error_code != 0x05) {
        TEST_FAIL("Wrong error code");
    }
    
    uft_d64_close_safe(&img);
    remove(path);
    TEST_PASS();
    return 0;
}

static int test_read_sector_bounds(void) {
    printf("Test: Read sector bounds checking\n");
    
    const char *path = "/tmp/test_d64_bounds.d64";
    if (create_mock_d64(path, 35, false) != 0) {
        TEST_FAIL("Failed to create mock file");
    }
    
    uft_d64_image_hardened_t *img = NULL;
    uft_d64_open_safe(path, true, &img);
    
    uft_d64_sector_t sector;
    
    // Track 0 (invalid - 1-based)
    uft_d64_error_t rc = uft_d64_read_sector(img, 0, 0, &sector);
    if (rc != UFT_D64_EBOUNDS) {
        TEST_FAIL("Track 0 should fail");
    }
    
    // Track 36 (beyond 35-track image)
    rc = uft_d64_read_sector(img, 36, 0, &sector);
    if (rc != UFT_D64_EBOUNDS) {
        TEST_FAIL("Track 36 should fail for 35-track image");
    }
    
    // Sector 21 on track 1 (only 0-20 valid)
    rc = uft_d64_read_sector(img, 1, 21, &sector);
    if (rc != UFT_D64_EBOUNDS) {
        TEST_FAIL("Sector 21 should fail on track 1");
    }
    
    // Sector 19 on track 18 (only 0-18 valid)
    rc = uft_d64_read_sector(img, 18, 19, &sector);
    if (rc != UFT_D64_EBOUNDS) {
        TEST_FAIL("Sector 19 should fail on track 18");
    }
    
    uft_d64_close_safe(&img);
    remove(path);
    TEST_PASS();
    return 0;
}

static int test_disk_info(void) {
    printf("Test: Get disk info from BAM\n");
    
    const char *path = "/tmp/test_d64_info.d64";
    if (create_mock_d64(path, 35, false) != 0) {
        TEST_FAIL("Failed to create mock file");
    }
    
    uft_d64_image_hardened_t *img = NULL;
    uft_d64_open_safe(path, true, &img);
    
    uft_d64_disk_info_t info;
    uft_d64_error_t rc = uft_d64_get_info(img, &info);
    
    if (rc != UFT_D64_OK) {
        TEST_FAIL("Get info failed");
    }
    
    if (strcmp(info.name, "TEST DISK") != 0) {
        printf("  Got name: '%s'\n", info.name);
        TEST_FAIL("Wrong disk name");
    }
    
    if (strcmp(info.id, "ID") != 0) {
        TEST_FAIL("Wrong disk ID");
    }
    
    if (info.dos_version != 0x41) {
        TEST_FAIL("Wrong DOS version");
    }
    
    uft_d64_close_safe(&img);
    remove(path);
    TEST_PASS();
    return 0;
}

static int test_null_handling(void) {
    printf("Test: NULL argument handling\n");
    
    uft_d64_image_hardened_t *img = NULL;
    
    // NULL path
    if (uft_d64_open_safe(NULL, true, &img) != UFT_D64_EINVAL) {
        TEST_FAIL("NULL path should fail");
    }
    
    // NULL img_out
    if (uft_d64_open_safe("/tmp/x.d64", true, NULL) != UFT_D64_EINVAL) {
        TEST_FAIL("NULL img_out should fail");
    }
    
    // Close NULL should not crash
    uft_d64_close_safe(NULL);
    uft_d64_close_safe(&img);
    
    TEST_PASS();
    return 0;
}

static int test_sectors_per_track(void) {
    printf("Test: Sectors per track function\n");
    
    // Zone 0
    if (uft_d64_sectors_per_track(1) != 21) TEST_FAIL("Track 1");
    if (uft_d64_sectors_per_track(17) != 21) TEST_FAIL("Track 17");
    
    // Zone 1
    if (uft_d64_sectors_per_track(18) != 19) TEST_FAIL("Track 18");
    if (uft_d64_sectors_per_track(24) != 19) TEST_FAIL("Track 24");
    
    // Zone 2
    if (uft_d64_sectors_per_track(25) != 18) TEST_FAIL("Track 25");
    if (uft_d64_sectors_per_track(30) != 18) TEST_FAIL("Track 30");
    
    // Zone 3
    if (uft_d64_sectors_per_track(31) != 17) TEST_FAIL("Track 31");
    if (uft_d64_sectors_per_track(35) != 17) TEST_FAIL("Track 35");
    
    // Invalid
    if (uft_d64_sectors_per_track(0) != 0) TEST_FAIL("Track 0");
    if (uft_d64_sectors_per_track(43) != 0) TEST_FAIL("Track 43");
    
    TEST_PASS();
    return 0;
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    printf("\n========================================\n");
    printf("  D64 HARDENED PARSER TESTS\n");
    printf("========================================\n\n");
    
    int failures = 0;
    
    failures += test_open_valid_35();
    failures += test_open_with_errors();
    failures += test_read_sector_bounds();
    failures += test_disk_info();
    failures += test_null_handling();
    failures += test_sectors_per_track();
    
    printf("\n========================================\n");
    if (failures == 0) {
        printf("  ALL TESTS PASSED\n");
    } else {
        printf("  %d TESTS FAILED\n", failures);
    }
    printf("========================================\n\n");
    
    return failures;
}
