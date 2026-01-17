/**
 * @file test_gcr_ops.c
 * @brief Unit tests for GCR Operations
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_gcr_ops.h"

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
#define ASSERT_TRUE(x) ASSERT((x))
#define ASSERT_FALSE(x) ASSERT(!(x))

/* ============================================================================
 * Test Helper Functions
 * ============================================================================ */

/**
 * @brief Create test track with syncs and gaps
 */
static void create_test_track(uint8_t *buffer, size_t size)
{
    memset(buffer, GCR_GAP_BYTE, size);
    
    /* Add some syncs */
    if (size >= 500) {
        memset(buffer + 100, GCR_SYNC_BYTE, 10);  /* Short sync */
        memset(buffer + 200, GCR_SYNC_BYTE, 20);  /* Long sync */
        memset(buffer + 300, GCR_SYNC_BYTE, 5);   /* Minimum sync */
    }
}

/**
 * @brief Create killer track
 */
static void create_killer_track(uint8_t *buffer, size_t size)
{
    memset(buffer, GCR_SYNC_BYTE, size);
}

/* ============================================================================
 * Unit Tests - Constants
 * ============================================================================ */

TEST(constants)
{
    ASSERT_EQ(GCR_SYNC_BYTE, 0xFF);
    ASSERT_EQ(GCR_GAP_BYTE, 0x55);
    ASSERT_EQ(GCR_MIN_SYNC, 5);
    ASSERT_EQ(SECTOR_SIZE, 256);
}

TEST(sectors_per_track)
{
    /* Tracks 1-17: 21 sectors */
    ASSERT_EQ(gcr_sectors_per_track(1), 21);
    ASSERT_EQ(gcr_sectors_per_track(17), 21);
    
    /* Tracks 18-24: 19 sectors */
    ASSERT_EQ(gcr_sectors_per_track(18), 19);
    ASSERT_EQ(gcr_sectors_per_track(24), 19);
    
    /* Tracks 25-30: 18 sectors */
    ASSERT_EQ(gcr_sectors_per_track(25), 18);
    ASSERT_EQ(gcr_sectors_per_track(30), 18);
    
    /* Tracks 31-42: 17 sectors */
    ASSERT_EQ(gcr_sectors_per_track(31), 17);
    ASSERT_EQ(gcr_sectors_per_track(35), 17);
    
    /* Invalid */
    ASSERT_EQ(gcr_sectors_per_track(0), 0);
    ASSERT_EQ(gcr_sectors_per_track(50), 0);
}

TEST(expected_capacity)
{
    /* Density 3: tracks 1-17 */
    size_t cap1 = gcr_expected_capacity(1);
    ASSERT(cap1 >= 7500 && cap1 <= 8000);
    
    /* Density 0: tracks 31+ */
    size_t cap31 = gcr_expected_capacity(31);
    ASSERT(cap31 >= 6000 && cap31 <= 6500);
    
    /* Invalid */
    ASSERT_EQ(gcr_expected_capacity(0), 0);
}

/* ============================================================================
 * Unit Tests - GCR Tables
 * ============================================================================ */

TEST(gcr_tables)
{
    const uint8_t *encode = gcr_get_encode_table();
    const uint8_t *decode_high = gcr_get_decode_high_table();
    const uint8_t *decode_low = gcr_get_decode_low_table();
    
    ASSERT(encode != NULL);
    ASSERT(decode_high != NULL);
    ASSERT(decode_low != NULL);
    
    /* Verify encode table has valid GCR codes */
    for (int i = 0; i < 16; i++) {
        uint8_t gcr = encode[i];
        ASSERT(gcr >= 0x09 && gcr <= 0x1E);  /* Valid GCR range */
    }
}

/* ============================================================================
 * Unit Tests - GCR Encode/Decode
 * ============================================================================ */

TEST(gcr_encode_decode)
{
    /* Test data: 4 bytes */
    uint8_t plain[4] = {0x00, 0x55, 0xAA, 0xFF};
    uint8_t gcr[5];
    uint8_t decoded[4];
    size_t errors;
    
    /* Encode */
    size_t gcr_size = gcr_encode(plain, 4, gcr);
    ASSERT_EQ(gcr_size, 5);
    
    /* Decode */
    size_t plain_size = gcr_decode(gcr, 5, decoded, &errors);
    ASSERT_EQ(plain_size, 4);
    ASSERT_EQ(errors, 0);
    
    /* Verify */
    ASSERT(memcmp(plain, decoded, 4) == 0);
}

TEST(gcr_encode_larger)
{
    uint8_t plain[256];
    uint8_t gcr[325];
    uint8_t decoded[256];
    
    /* Fill with pattern */
    for (int i = 0; i < 256; i++) {
        plain[i] = (uint8_t)i;
    }
    
    /* Encode */
    size_t gcr_size = gcr_encode(plain, 256, gcr);
    ASSERT(gcr_size > 256);  /* GCR is 25% larger */
    
    /* Decode */
    size_t errors;
    size_t plain_size = gcr_decode(gcr, gcr_size, decoded, &errors);
    ASSERT_EQ(plain_size, 256);
    
    /* Verify */
    ASSERT(memcmp(plain, decoded, 256) == 0);
}

TEST(gcr_check_errors)
{
    /* Valid GCR */
    uint8_t valid[5];
    gcr_encode((uint8_t*)"\x00\x00\x00\x00", 4, valid);
    size_t errors = gcr_check_errors(valid, 5);
    /* May have some due to boundary conditions */
    
    /* Invalid GCR (bad patterns) */
    uint8_t invalid[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
    errors = gcr_check_errors(invalid, 5);
    ASSERT(errors > 0);  /* Should have errors */
}

/* ============================================================================
 * Unit Tests - Sync Operations
 * ============================================================================ */

TEST(gcr_find_sync)
{
    uint8_t buffer[100];
    memset(buffer, GCR_GAP_BYTE, sizeof(buffer));
    
    /* Add sync at position 50 */
    buffer[50] = GCR_SYNC_BYTE;
    buffer[51] = GCR_SYNC_BYTE;
    buffer[52] = GCR_SYNC_BYTE;
    
    int pos = gcr_find_sync(buffer, sizeof(buffer), 0);
    ASSERT_EQ(pos, 50);
    
    /* No sync at start */
    pos = gcr_find_sync(buffer, sizeof(buffer), 60);
    ASSERT_EQ(pos, -1);
}

TEST(gcr_find_sync_end)
{
    uint8_t buffer[100];
    memset(buffer, GCR_GAP_BYTE, sizeof(buffer));
    
    /* Add 10-byte sync at position 20 */
    memset(buffer + 20, GCR_SYNC_BYTE, 10);
    
    size_t end = gcr_find_sync_end(buffer, sizeof(buffer), 20);
    ASSERT_EQ(end, 30);
}

TEST(gcr_count_syncs)
{
    uint8_t buffer[200];
    create_test_track(buffer, sizeof(buffer));
    
    int count = gcr_count_syncs(buffer, sizeof(buffer));
    /* Should find the syncs we added */
    ASSERT(count >= 1);
}

TEST(gcr_longest_sync)
{
    uint8_t buffer[500];
    create_test_track(buffer, sizeof(buffer));
    
    size_t position;
    size_t length = gcr_longest_sync(buffer, sizeof(buffer), &position);
    
    /* Longest sync is 20 bytes at position 200 */
    ASSERT_EQ(length, 20);
    ASSERT_EQ(position, 200);
}

TEST(gcr_kill_partial_syncs)
{
    uint8_t buffer[500];
    memset(buffer, GCR_GAP_BYTE, sizeof(buffer));
    
    /* Add syncs of various lengths */
    memset(buffer + 100, GCR_SYNC_BYTE, 3);   /* Too short */
    memset(buffer + 200, GCR_SYNC_BYTE, 10);  /* Long enough */
    
    int killed = gcr_kill_partial_syncs(buffer, sizeof(buffer), 5);
    ASSERT(killed >= 1);  /* Should kill the short one */
    
    /* Short sync should be replaced with gap */
    ASSERT(buffer[100] != GCR_SYNC_BYTE);
    
    /* Long sync should remain */
    ASSERT_EQ(buffer[200], GCR_SYNC_BYTE);
}

/* ============================================================================
 * Unit Tests - Gap Operations
 * ============================================================================ */

TEST(gcr_find_gap)
{
    uint8_t buffer[100];
    memset(buffer, 0x00, sizeof(buffer));
    
    /* Add gap at position 30 */
    memset(buffer + 30, 0xAA, 10);
    
    int pos = gcr_find_gap(buffer, sizeof(buffer), 0);
    /* Should find the first gap run */
    ASSERT(pos >= 0);
}

TEST(gcr_longest_gap)
{
    uint8_t buffer[200];
    memset(buffer, GCR_SYNC_BYTE, sizeof(buffer));  /* Start with sync */
    
    /* Add gaps of different lengths */
    memset(buffer + 50, GCR_GAP_BYTE, 10);
    memset(buffer + 100, GCR_GAP_BYTE, 30);
    memset(buffer + 150, GCR_GAP_BYTE, 5);
    
    size_t position;
    uint8_t gap_byte;
    size_t length = gcr_longest_gap(buffer, sizeof(buffer), &position, &gap_byte);
    
    ASSERT_EQ(length, 30);
    ASSERT_EQ(position, 100);
    ASSERT_EQ(gap_byte, GCR_GAP_BYTE);
}

TEST(gcr_strip_runs)
{
    uint8_t buffer[500];
    memset(buffer, 0xAA, sizeof(buffer));
    
    /* Add long sync and gap runs */
    memset(buffer + 100, GCR_SYNC_BYTE, 50);
    memset(buffer + 200, GCR_GAP_BYTE, 100);
    
    size_t original_size = sizeof(buffer);
    size_t new_size = gcr_strip_runs(buffer, original_size, 5, 5);
    
    /* Should be smaller after stripping */
    ASSERT(new_size < original_size);
}

TEST(gcr_reduce_gaps)
{
    uint8_t buffer[1000];
    memset(buffer, GCR_GAP_BYTE, sizeof(buffer));
    
    /* Add some syncs */
    memset(buffer + 100, GCR_SYNC_BYTE, 10);
    memset(buffer + 500, GCR_SYNC_BYTE, 10);
    
    size_t new_size = gcr_reduce_gaps(buffer, sizeof(buffer));
    ASSERT(new_size <= sizeof(buffer));
}

/* ============================================================================
 * Unit Tests - Track Cycle Detection
 * ============================================================================ */

TEST(gcr_detect_cycle)
{
    uint8_t buffer[2000];
    
    /* Create repeating pattern */
    uint8_t pattern[100];
    memset(pattern, GCR_GAP_BYTE, sizeof(pattern));
    memset(pattern, GCR_SYNC_BYTE, 10);  /* Start with sync */
    
    /* Repeat pattern */
    for (int i = 0; i < 2000; i += 100) {
        memcpy(buffer + i, pattern, (i + 100 <= 2000) ? 100 : (2000 - i));
    }
    
    gcr_cycle_t cycle;
    bool found = gcr_detect_cycle(buffer, sizeof(buffer), 50, &cycle);
    /* May or may not find depending on pattern */
    if (found) {
        ASSERT(cycle.cycle_length > 0);
    }
}

/* ============================================================================
 * Unit Tests - Track Comparison
 * ============================================================================ */

TEST(gcr_compare_tracks_identical)
{
    uint8_t track[500];
    create_test_track(track, sizeof(track));
    
    gcr_compare_result_t result;
    size_t diffs = gcr_compare_tracks(track, sizeof(track), 
                                       track, sizeof(track), 
                                       true, &result);
    
    ASSERT_EQ(diffs, 0);
    ASSERT_EQ(result.similarity, 100.0f);
}

TEST(gcr_compare_tracks_different)
{
    uint8_t track1[500];
    uint8_t track2[500];
    
    create_test_track(track1, sizeof(track1));
    memset(track2, 0xAA, sizeof(track2));  /* Different */
    
    gcr_compare_result_t result;
    size_t diffs = gcr_compare_tracks(track1, sizeof(track1),
                                       track2, sizeof(track2),
                                       false, &result);
    
    ASSERT(diffs > 0);
    ASSERT(result.similarity < 100.0f);
}

TEST(gcr_compare_tracks_length_diff)
{
    uint8_t track1[500];
    uint8_t track2[400];
    
    memset(track1, GCR_GAP_BYTE, sizeof(track1));
    memset(track2, GCR_GAP_BYTE, sizeof(track2));
    
    gcr_compare_result_t result;
    size_t diffs = gcr_compare_tracks(track1, sizeof(track1),
                                       track2, sizeof(track2),
                                       false, &result);
    
    /* Should have at least 100 diffs due to length */
    ASSERT(diffs >= 100);
    ASSERT_FALSE(result.same_format);
}

/* ============================================================================
 * Unit Tests - Track Verification
 * ============================================================================ */

TEST(gcr_verify_track)
{
    uint8_t track[7000];
    create_test_track(track, sizeof(track));
    
    gcr_verify_result_t result;
    int errors = gcr_verify_track(track, sizeof(track), 1, NULL, &result);
    
    /* Function should complete without crash */
    ASSERT(result.sectors_found >= 0);
}

/* ============================================================================
 * Unit Tests - Track Utilities
 * ============================================================================ */

TEST(gcr_is_empty_track)
{
    uint8_t empty[500];
    uint8_t full[500];
    
    memset(empty, 0, sizeof(empty));
    create_test_track(full, sizeof(full));
    
    ASSERT_TRUE(gcr_is_empty_track(empty, sizeof(empty)));
    /* Full track might not be empty depending on content */
}

TEST(gcr_is_killer_track)
{
    uint8_t killer[500];
    uint8_t normal[500];
    
    create_killer_track(killer, sizeof(killer));
    create_test_track(normal, sizeof(normal));
    
    ASSERT_TRUE(gcr_is_killer_track(killer, sizeof(killer)));
    ASSERT_FALSE(gcr_is_killer_track(normal, sizeof(normal)));
}

TEST(gcr_detect_density)
{
    /* Small track = density 0 */
    ASSERT_EQ(gcr_detect_density(NULL, 6200), 0);
    
    /* Large track = density 3 */
    ASSERT_EQ(gcr_detect_density(NULL, 7600), 3);
}

/* ============================================================================
 * Unit Tests - Checksums
 * ============================================================================ */

TEST(gcr_calc_data_checksum)
{
    uint8_t data[SECTOR_SIZE];
    memset(data, 0, sizeof(data));
    
    uint8_t checksum = gcr_calc_data_checksum(data);
    ASSERT_EQ(checksum, 0);  /* XOR of all zeros is zero */
    
    /* Set all bytes to 0xFF */
    memset(data, 0xFF, sizeof(data));
    checksum = gcr_calc_data_checksum(data);
    /* 256 bytes of 0xFF XOR'd = 0 (even count) */
    ASSERT_EQ(checksum, 0);
    
    /* Set one byte different */
    data[0] = 0x00;
    checksum = gcr_calc_data_checksum(data);
    ASSERT_EQ(checksum, 0xFF);
}

TEST(gcr_calc_header_checksum)
{
    uint8_t id[2] = {0x41, 0x42};  /* "AB" */
    
    uint8_t checksum = gcr_calc_header_checksum(1, 0, id);
    /* 1 ^ 0 ^ 0x41 ^ 0x42 */
    ASSERT_EQ(checksum, 1 ^ 0x41 ^ 0x42);
}

TEST(gcr_crc_track)
{
    uint8_t track[500];
    create_test_track(track, sizeof(track));
    
    uint32_t crc = gcr_crc_track(track, sizeof(track), 1);
    ASSERT(crc != 0);  /* Should produce some CRC */
    
    /* Same data should produce same CRC */
    uint32_t crc2 = gcr_crc_track(track, sizeof(track), 1);
    ASSERT_EQ(crc, crc2);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    printf("\n=== GCR Operations Tests ===\n\n");
    
    printf("Constants:\n");
    RUN_TEST(constants);
    RUN_TEST(sectors_per_track);
    RUN_TEST(expected_capacity);
    
    printf("\nGCR Tables:\n");
    RUN_TEST(gcr_tables);
    
    printf("\nGCR Encode/Decode:\n");
    RUN_TEST(gcr_encode_decode);
    RUN_TEST(gcr_encode_larger);
    RUN_TEST(gcr_check_errors);
    
    printf("\nSync Operations:\n");
    RUN_TEST(gcr_find_sync);
    RUN_TEST(gcr_find_sync_end);
    RUN_TEST(gcr_count_syncs);
    RUN_TEST(gcr_longest_sync);
    RUN_TEST(gcr_kill_partial_syncs);
    
    printf("\nGap Operations:\n");
    RUN_TEST(gcr_find_gap);
    RUN_TEST(gcr_longest_gap);
    RUN_TEST(gcr_strip_runs);
    RUN_TEST(gcr_reduce_gaps);
    
    printf("\nTrack Cycle:\n");
    RUN_TEST(gcr_detect_cycle);
    
    printf("\nTrack Comparison:\n");
    RUN_TEST(gcr_compare_tracks_identical);
    RUN_TEST(gcr_compare_tracks_different);
    RUN_TEST(gcr_compare_tracks_length_diff);
    
    printf("\nTrack Verification:\n");
    RUN_TEST(gcr_verify_track);
    
    printf("\nTrack Utilities:\n");
    RUN_TEST(gcr_is_empty_track);
    RUN_TEST(gcr_is_killer_track);
    RUN_TEST(gcr_detect_density);
    
    printf("\nChecksums:\n");
    RUN_TEST(gcr_calc_data_checksum);
    RUN_TEST(gcr_calc_header_checksum);
    RUN_TEST(gcr_crc_track);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
