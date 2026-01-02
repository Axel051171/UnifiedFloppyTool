/**
 * @file test_phase1.c
 * @brief Unit Tests for Roadmap Phase 1
 * 
 * F1.1: D64 Extended Variant Support
 * F1.2: ADF DirCache Full Support
 * F1.3: WOZ v2.1 Flux Timing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Include headers (would normally be from install path)
#include "uft/formats/uft_d64.h"
#include "uft/formats/uft_adf.h"
#include "uft/formats/uft_woz.h"

// ============================================================================
// Test Framework
// ============================================================================

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    static void test_##name(void); \
    static void run_##name(void) { \
        printf("  TEST: %s... ", #name); \
        tests_run++; \
        test_##name(); \
    } \
    static void test_##name(void)

#define PASS() do { printf("PASS\n"); tests_passed++; return; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; return; } while(0)
#define ASSERT(cond, msg) if (!(cond)) FAIL(msg)

// ============================================================================
// F1.1 D64 Tests
// ============================================================================

TEST(d64_size_detection) {
    d64_detect_result_t result;
    
    // Create minimal D64 data (just size check)
    uint8_t data_35[174848] = {0};
    uint8_t data_40[196608] = {0};
    
    // Initialize minimal BAM
    data_35[0x16500] = 18;  // Track 18, sector 0
    data_35[0x16501] = 1;
    data_35[0x16502] = 0x41;
    
    ASSERT(d64_detect_variant(data_35, 174848, &result) == 0, "35-track not detected");
    ASSERT(result.tracks == 35, "Wrong track count for 35-track");
    ASSERT(result.confidence >= 90, "Low confidence for 35-track");
    
    data_40[0x16500] = 18;
    data_40[0x16501] = 1;
    data_40[0x16502] = 0x41;
    
    ASSERT(d64_detect_variant(data_40, 196608, &result) == 0, "40-track not detected");
    ASSERT(result.tracks == 40, "Wrong track count for 40-track");
    
    PASS();
}

TEST(d64_error_info_detection) {
    d64_detect_result_t result;
    
    uint8_t data[175531] = {0};
    data[0x16500] = 18;
    data[0x16501] = 1;
    data[0x16502] = 0x41;
    
    ASSERT(d64_detect_variant(data, 175531, &result) == 0, "35+errors not detected");
    ASSERT(result.has_errors == true, "Error info not detected");
    ASSERT((result.variant & D64_VAR_ERROR_INFO) != 0, "Error flag not set");
    
    PASS();
}

TEST(d64_sector_offset) {
    // Track 1, Sector 0 = offset 0
    ASSERT(d64_get_sector_offset(1, 0) == 0, "Track 1 sector 0 offset wrong");
    
    // Track 1, Sector 1 = offset 256
    ASSERT(d64_get_sector_offset(1, 1) == 256, "Track 1 sector 1 offset wrong");
    
    // Track 18, Sector 0 = BAM
    int bam_offset = d64_get_sector_offset(18, 0);
    ASSERT(bam_offset > 0, "BAM offset invalid");
    
    // Invalid track
    ASSERT(d64_get_sector_offset(0, 0) == -1, "Track 0 should fail");
    ASSERT(d64_get_sector_offset(43, 0) == -1, "Track 43 should fail");
    
    PASS();
}

TEST(d64_sectors_per_track) {
    ASSERT(d64_sectors_in_track(1) == 21, "Track 1 should have 21 sectors");
    ASSERT(d64_sectors_in_track(18) == 19, "Track 18 should have 19 sectors");
    ASSERT(d64_sectors_in_track(25) == 18, "Track 25 should have 18 sectors");
    ASSERT(d64_sectors_in_track(31) == 17, "Track 31 should have 17 sectors");
    ASSERT(d64_sectors_in_track(0) == -1, "Track 0 should fail");
    
    PASS();
}

TEST(d64_create) {
    d64_image_t* img = d64_create(35, false);
    ASSERT(img != NULL, "d64_create failed");
    ASSERT(img->num_tracks == 35, "Wrong track count");
    ASSERT(img->is_valid == true, "Image not valid");
    ASSERT(img->data_size == 174848, "Wrong data size");
    
    d64_close(img);
    PASS();
}

// ============================================================================
// F1.2 ADF Tests
// ============================================================================

TEST(adf_size_detection) {
    adf_detect_result_t result;
    
    uint8_t data_dd[901120] = {0};
    uint8_t data_hd[1802240] = {0};
    
    // Set DOS signature
    memcpy(data_dd, "DOS\x01", 4);  // FFS
    memcpy(data_hd, "DOS\x03", 4);  // FFS-INTL
    
    ASSERT(adf_detect_variant(data_dd, 901120, &result) == 0, "DD not detected");
    ASSERT(result.is_hd == false, "DD detected as HD");
    ASSERT(result.fs_type == ADF_FS_FFS, "Wrong FS type for DD");
    
    ASSERT(adf_detect_variant(data_hd, 1802240, &result) == 0, "HD not detected");
    ASSERT(result.is_hd == true, "HD not detected");
    ASSERT(result.fs_type == ADF_FS_FFS_INTL, "Wrong FS type for HD");
    
    PASS();
}

TEST(adf_dircache_detection) {
    adf_detect_result_t result;
    
    uint8_t data[901120] = {0};
    memcpy(data, "DOS\x05", 4);  // FFS-DC
    
    ASSERT(adf_detect_variant(data, 901120, &result) == 0, "FFS-DC not detected");
    ASSERT(result.has_dircache == true, "DirCache not detected");
    ASSERT(result.fs_type == ADF_FS_FFS_DC, "Wrong FS type");
    
    PASS();
}

TEST(adf_pc_fat_detection) {
    adf_detect_result_t result;
    
    uint8_t data[901120] = {0};
    data[510] = 0x55;
    data[511] = 0xAA;
    
    ASSERT(adf_detect_variant(data, 901120, &result) == 0, "PC-FAT not detected");
    ASSERT((result.variant & ADF_VAR_PC_FAT) != 0, "PC-FAT flag not set");
    
    PASS();
}

TEST(adf_checksum) {
    uint8_t block[512] = {0};
    
    // Set some values
    block[0] = 0x00;
    block[1] = 0x00;
    block[2] = 0x00;
    block[3] = 0x02;  // Type = 2
    
    uint32_t checksum = adf_checksum(block);
    ASSERT(checksum != 0, "Checksum should be non-zero");
    
    // Write checksum and verify
    block[20] = (checksum >> 24) & 0xFF;
    block[21] = (checksum >> 16) & 0xFF;
    block[22] = (checksum >> 8) & 0xFF;
    block[23] = checksum & 0xFF;
    
    ASSERT(adf_verify_checksum(block) == true, "Checksum verification failed");
    
    PASS();
}

TEST(adf_fs_type_str) {
    ASSERT(strcmp(adf_fs_type_str(ADF_FS_OFS), "OFS") == 0, "OFS string wrong");
    ASSERT(strcmp(adf_fs_type_str(ADF_FS_FFS), "FFS") == 0, "FFS string wrong");
    ASSERT(strcmp(adf_fs_type_str(ADF_FS_FFS_DC), "FFS-DC") == 0, "FFS-DC string wrong");
    
    PASS();
}

// ============================================================================
// F1.3 WOZ Tests
// ============================================================================

TEST(woz_magic_detection) {
    woz_detect_result_t result;
    
    uint8_t woz1[32] = {0};
    uint8_t woz2[32] = {0};
    
    // WOZ1 magic
    woz1[0] = 'W'; woz1[1] = 'O'; woz1[2] = 'Z'; woz1[3] = '1';
    woz1[4] = 0xFF; woz1[5] = 0x0A; woz1[6] = 0x0D; woz1[7] = 0x0A;
    
    ASSERT(woz_detect_variant(woz1, 32, &result) == 0, "WOZ1 not detected");
    ASSERT(result.woz_version == 10, "Wrong WOZ1 version");
    
    // WOZ2 magic
    woz2[0] = 'W'; woz2[1] = 'O'; woz2[2] = 'Z'; woz2[3] = '2';
    woz2[4] = 0xFF; woz2[5] = 0x0A; woz2[6] = 0x0D; woz2[7] = 0x0A;
    
    ASSERT(woz_detect_variant(woz2, 32, &result) == 0, "WOZ2 not detected");
    ASSERT(result.woz_version == 20, "Wrong WOZ2 version");
    
    PASS();
}

TEST(woz_invalid_magic) {
    woz_detect_result_t result;
    
    uint8_t invalid[32] = {0};
    invalid[0] = 'W'; invalid[1] = 'O'; invalid[2] = 'Z'; invalid[3] = '3';
    
    ASSERT(woz_detect_variant(invalid, 32, &result) != 0, "Invalid magic should fail");
    
    PASS();
}

TEST(woz_timing_defaults) {
    ASSERT(WOZ_TIMING_525 == 32, "5.25\" timing should be 32");
    ASSERT(WOZ_TIMING_35 == 16, "3.5\" timing should be 16");
    
    PASS();
}

TEST(woz_disk_type_str) {
    ASSERT(strcmp(woz_disk_type_str(WOZ_DISK_525), "5.25\"") == 0, "5.25\" string wrong");
    ASSERT(strcmp(woz_disk_type_str(WOZ_DISK_35), "3.5\"") == 0, "3.5\" string wrong");
    
    PASS();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         PHASE 1 UNIT TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("F1.1: D64 Extended Variant Support\n");
    run_d64_size_detection();
    run_d64_error_info_detection();
    run_d64_sector_offset();
    run_d64_sectors_per_track();
    run_d64_create();
    
    printf("\nF1.2: ADF DirCache Full Support\n");
    run_adf_size_detection();
    run_adf_dircache_detection();
    run_adf_pc_fat_detection();
    run_adf_checksum();
    run_adf_fs_type_str();
    
    printf("\nF1.3: WOZ v2.1 Flux Timing\n");
    run_woz_magic_detection();
    run_woz_invalid_magic();
    run_woz_timing_defaults();
    run_woz_disk_type_str();
    
    printf("\n═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         RESULTS: %d/%d passed, %d failed\n", tests_passed, tests_run, tests_failed);
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    return tests_failed > 0 ? 1 : 0;
}
