/**
 * @file test_scp_hardened.c
 * @brief Unit tests for hardened SCP parser
 */

#include "uft/formats/scp_hardened.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_PASS() printf("  ✓ PASS\n")
#define TEST_FAIL(msg) do { printf("  ✗ FAIL: %s\n", msg); return 1; } while(0)

// ============================================================================
// Mock Data Generation
// ============================================================================

static int create_mock_scp(const char *path, bool valid, bool overflow_offsets) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    // Create minimal valid SCP header
    uft_scp_header_t hdr = {0};
    memcpy(hdr.signature, "SCP", 3);
    hdr.version = 0x19;
    hdr.disk_type = 0x80;  // Other
    hdr.num_revs = 3;
    hdr.start_track = 0;
    hdr.end_track = 79;
    hdr.flags = 0;
    hdr.bit_cell_width = 0;
    hdr.heads = 0;
    hdr.resolution = 0;
    hdr.checksum = 0;  // Not checked yet
    
    // Set track offsets
    if (overflow_offsets) {
        // Create offsets that would overflow
        hdr.track_offsets[0] = 0xFFFFFF00;  // Will overflow when added
    } else if (valid) {
        hdr.track_offsets[0] = sizeof(hdr);  // Point right after header
    }
    
    fwrite(&hdr, sizeof(hdr), 1, f);
    
    if (valid && !overflow_offsets) {
        // Write a minimal track
        uft_scp_track_header_t trk = {0};
        memcpy(trk.signature, "TRK", 3);
        trk.track_number = 0;
        fwrite(&trk, sizeof(trk), 1, f);
        
        // Revolution entry
        uint8_t rev[12] = {0};
        // time_duration
        rev[0] = 0x00; rev[1] = 0x00; rev[2] = 0x10; rev[3] = 0x00;  // 0x100000
        // data_length
        rev[4] = 0x10; rev[5] = 0x00; rev[6] = 0x00; rev[7] = 0x00;  // 16 values
        // data_offset
        rev[8] = (uint8_t)(sizeof(trk) + 12); rev[9] = 0; rev[10] = 0; rev[11] = 0;
        fwrite(rev, 12, 1, f);
        
        // More revs
        fwrite(rev, 12, 1, f);
        fwrite(rev, 12, 1, f);
        
        // Flux data (16 BE values)
        for (int i = 0; i < 16; i++) {
            uint8_t flux[2] = {0x01, 0x00};  // 256 ticks
            fwrite(flux, 2, 1, f);
        }
    }
    
    fclose(f);
    return 0;
}

// ============================================================================
// Tests
// ============================================================================

static int test_open_valid(void) {
    printf("Test: Open valid SCP file\n");
    
    const char *path = "/tmp/test_valid.scp";
    if (create_mock_scp(path, true, false) != 0) {
        TEST_FAIL("Failed to create mock file");
    }
    
    uft_scp_image_hardened_t *img = NULL;
    uft_scp_error_t rc = uft_scp_open_safe(path, &img);
    
    if (rc != UFT_SCP_OK) {
        printf("  Error: %s\n", uft_scp_error_string(rc));
        TEST_FAIL("Open failed");
    }
    
    if (!uft_scp_is_valid(img)) {
        TEST_FAIL("Image not valid after open");
    }
    
    uft_scp_close_safe(&img);
    
    if (img != NULL) {
        TEST_FAIL("Handle not NULL after close");
    }
    
    remove(path);
    TEST_PASS();
    return 0;
}

static int test_open_null_args(void) {
    printf("Test: Open with NULL arguments\n");
    
    uft_scp_image_hardened_t *img = NULL;
    
    // NULL path
    uft_scp_error_t rc = uft_scp_open_safe(NULL, &img);
    if (rc != UFT_SCP_EINVAL) {
        TEST_FAIL("Expected EINVAL for NULL path");
    }
    
    // NULL img_out
    rc = uft_scp_open_safe("/tmp/test.scp", NULL);
    if (rc != UFT_SCP_EINVAL) {
        TEST_FAIL("Expected EINVAL for NULL img_out");
    }
    
    TEST_PASS();
    return 0;
}

static int test_open_nonexistent(void) {
    printf("Test: Open non-existent file\n");
    
    uft_scp_image_hardened_t *img = NULL;
    uft_scp_error_t rc = uft_scp_open_safe("/tmp/nonexistent_12345.scp", &img);
    
    if (rc != UFT_SCP_EIO) {
        TEST_FAIL("Expected EIO for non-existent file");
    }
    
    TEST_PASS();
    return 0;
}

static int test_open_invalid_magic(void) {
    printf("Test: Open file with invalid magic\n");
    
    const char *path = "/tmp/test_badmagic.scp";
    FILE *f = fopen(path, "wb");
    if (f) {
        uint8_t bad[256] = {0};
        memcpy(bad, "XXX", 3);  // Wrong magic
        fwrite(bad, sizeof(bad), 1, f);
        fclose(f);
    }
    
    uft_scp_image_hardened_t *img = NULL;
    uft_scp_error_t rc = uft_scp_open_safe(path, &img);
    
    if (rc != UFT_SCP_EFORMAT) {
        TEST_FAIL("Expected EFORMAT for invalid magic");
    }
    
    remove(path);
    TEST_PASS();
    return 0;
}

static int test_overflow_protection(void) {
    printf("Test: Integer overflow protection\n");
    
    const char *path = "/tmp/test_overflow.scp";
    if (create_mock_scp(path, true, true) != 0) {
        TEST_FAIL("Failed to create mock file");
    }
    
    uft_scp_image_hardened_t *img = NULL;
    uft_scp_error_t rc = uft_scp_open_safe(path, &img);
    
    // Should fail with EFORMAT due to offset validation
    if (rc != UFT_SCP_EFORMAT) {
        if (img) uft_scp_close_safe(&img);
        TEST_FAIL("Expected EFORMAT for overflow offsets");
    }
    
    remove(path);
    TEST_PASS();
    return 0;
}

static int test_close_null(void) {
    printf("Test: Close with NULL\n");
    
    // Should not crash
    uft_scp_close_safe(NULL);
    
    uft_scp_image_hardened_t *img = NULL;
    uft_scp_close_safe(&img);
    
    TEST_PASS();
    return 0;
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    printf("\n========================================\n");
    printf("  SCP HARDENED PARSER TESTS\n");
    printf("========================================\n\n");
    
    int failures = 0;
    
    failures += test_open_valid();
    failures += test_open_null_args();
    failures += test_open_nonexistent();
    failures += test_open_invalid_magic();
    failures += test_overflow_protection();
    failures += test_close_null();
    
    printf("\n========================================\n");
    if (failures == 0) {
        printf("  ALL TESTS PASSED\n");
    } else {
        printf("  %d TESTS FAILED\n", failures);
    }
    printf("========================================\n\n");
    
    return failures;
}
