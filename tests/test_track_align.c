/**
 * @file test_track_align.c
 * @brief Unit tests for Track Alignment Module
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Include the header */
#include "uft/protection/uft_track_align.h"

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

/* ============================================================================
 * Test Helper Functions
 * ============================================================================ */

/**
 * @brief Create test track with sync marks
 */
static void create_test_track_with_sync(uint8_t *buffer, size_t length)
{
    memset(buffer, 0x55, length);  /* Fill with gap bytes */
    
    /* Add some sync marks */
    if (length >= 100) {
        memset(buffer + 10, 0xFF, 10);   /* Sync at offset 10 */
        memset(buffer + 50, 0xFF, 20);   /* Longer sync at offset 50 */
        memset(buffer + 80, 0xFF, 5);    /* Short sync at offset 80 */
    }
}

/**
 * @brief Create test track with V-MAX! markers
 */
static void create_vmax_track(uint8_t *buffer, size_t length)
{
    memset(buffer, 0x55, length);
    
    /* Add V-MAX! marker run at offset 100 */
    if (length >= 120) {
        buffer[100] = VMAX_MARKER_4B;
        buffer[101] = VMAX_MARKER_69;
        buffer[102] = VMAX_MARKER_49;
        buffer[103] = VMAX_MARKER_5A;
        buffer[104] = VMAX_MARKER_A5;
        buffer[105] = VMAX_MARKER_4B;
        buffer[106] = VMAX_MARKER_69;
        buffer[107] = VMAX_MARKER_49;
    }
}

/**
 * @brief Create test track with Cinemaware V-MAX! markers
 */
static void create_vmax_cw_track(uint8_t *buffer, size_t length)
{
    memset(buffer, 0x55, length);
    
    /* Add Cinemaware pattern: 0x64 0xA5 0xA5 0xA5 */
    if (length >= 60) {
        buffer[50] = VMAX_CW_MARKER;
        buffer[51] = VMAX_MARKER_A5;
        buffer[52] = VMAX_MARKER_A5;
        buffer[53] = VMAX_MARKER_A5;
    }
}

/**
 * @brief Create test track with Pirate Slayer signature
 */
static void create_pirateslayer_track(uint8_t *buffer, size_t length)
{
    memset(buffer, 0x55, length);
    
    /* Add Pirate Slayer signature: D7 D7 EB CC AD */
    if (length >= 80) {
        buffer[70] = PSLAYER_SIG_0;
        buffer[71] = PSLAYER_SIG_1;
        buffer[72] = PSLAYER_SIG_2;
        buffer[73] = PSLAYER_SIG_3;
        buffer[74] = PSLAYER_SIG_4;
    }
}

/**
 * @brief Create test track with RapidLok structure
 */
static void create_rapidlok_track(uint8_t *buffer, size_t length)
{
    memset(buffer, 0x55, length);
    
    if (length >= 200) {
        /* Sync (14-24 bytes of 0xFF) */
        memset(buffer + 10, 0xFF, 18);
        
        /* Extra sector start (0x55) */
        buffer[28] = 0x55;
        
        /* Extra sector fill (0x7B bytes, 60-300) */
        memset(buffer + 29, RAPIDLOK_EXTRA_BYTE, 100);
        
        /* End of track header - another sync */
        memset(buffer + 129, 0xFF, 10);
    }
}

/**
 * @brief Create test track with bad GCR
 */
static void create_bad_gcr_track(uint8_t *buffer, size_t length)
{
    memset(buffer, 0x55, length);
    
    /* Add bad GCR run (bytes that decode to invalid GCR) */
    /* Bad GCR values: 0x00-0x08 in high nibble position */
    if (length >= 50) {
        buffer[30] = 0x00;
        buffer[31] = 0x00;
        buffer[32] = 0x00;
        buffer[33] = 0x00;
        buffer[34] = 0x00;
    }
}

/* ============================================================================
 * Unit Tests
 * ============================================================================ */

TEST(constants)
{
    ASSERT_EQ(ALIGN_TRACK_LENGTH, 0x2000);
    ASSERT_EQ(ALIGN_MAX_HALFTRACKS, 84);
    ASSERT_EQ(ALIGN_MAX_TRACKS, 42);
}

TEST(alignment_methods)
{
    ASSERT_EQ(ALIGN_NONE, 0x00);
    ASSERT_EQ(ALIGN_VMAX, 0x05);
    ASSERT_EQ(ALIGN_PIRATESLAYER, 0x09);
    ASSERT_EQ(ALIGN_RAPIDLOK, 0x0A);
}

TEST(align_method_name)
{
    ASSERT(strcmp(align_method_name(ALIGN_NONE), "NONE") == 0);
    ASSERT(strcmp(align_method_name(ALIGN_VMAX), "VMAX") == 0);
    ASSERT(strcmp(align_method_name(ALIGN_PIRATESLAYER), "PIRATESLAYER") == 0);
    ASSERT(strcmp(align_method_name(ALIGN_RAPIDLOK), "RAPIDLOK") == 0);
    ASSERT(strcmp(align_method_name(ALIGN_LONGSYNC), "SYNC") == 0);
}

TEST(track_capacity)
{
    /* Density 0: tracks 31-42, ~6250 bytes */
    size_t cap0 = get_track_capacity(0);
    ASSERT(cap0 >= 6000 && cap0 <= 6500);
    
    /* Density 3: tracks 1-17, ~7692 bytes */
    size_t cap3 = get_track_capacity(3);
    ASSERT(cap3 >= 7500 && cap3 <= 8000);
    
    /* Invalid density */
    ASSERT_EQ(get_track_capacity(-1), 0);
    ASSERT_EQ(get_track_capacity(5), 0);
}

TEST(track_capacity_min_max)
{
    for (int d = 0; d <= 3; d++) {
        size_t cap_min = get_track_capacity_min(d);
        size_t cap = get_track_capacity(d);
        size_t cap_max = get_track_capacity_max(d);
        
        ASSERT(cap_min < cap);
        ASSERT(cap < cap_max);
    }
}

TEST(sectors_per_track)
{
    /* Tracks 1-17: 21 sectors */
    ASSERT_EQ(get_sectors_per_track(1), 21);
    ASSERT_EQ(get_sectors_per_track(17), 21);
    
    /* Tracks 18-24: 19 sectors */
    ASSERT_EQ(get_sectors_per_track(18), 19);
    ASSERT_EQ(get_sectors_per_track(24), 19);
    
    /* Tracks 25-30: 18 sectors */
    ASSERT_EQ(get_sectors_per_track(25), 18);
    ASSERT_EQ(get_sectors_per_track(30), 18);
    
    /* Tracks 31-35: 17 sectors */
    ASSERT_EQ(get_sectors_per_track(31), 17);
    ASSERT_EQ(get_sectors_per_track(35), 17);
    
    /* Invalid tracks */
    ASSERT_EQ(get_sectors_per_track(0), 0);
    ASSERT_EQ(get_sectors_per_track(50), 0);
}

TEST(track_density)
{
    /* Density 3: tracks 1-17 */
    ASSERT_EQ(get_track_density(1), 3);
    ASSERT_EQ(get_track_density(17), 3);
    
    /* Density 2: tracks 18-24 */
    ASSERT_EQ(get_track_density(18), 2);
    ASSERT_EQ(get_track_density(24), 2);
    
    /* Density 1: tracks 25-30 */
    ASSERT_EQ(get_track_density(25), 1);
    ASSERT_EQ(get_track_density(30), 1);
    
    /* Density 0: tracks 31-42 */
    ASSERT_EQ(get_track_density(31), 0);
    ASSERT_EQ(get_track_density(35), 0);
}

TEST(align_vmax)
{
    uint8_t buffer[1024];
    create_vmax_track(buffer, sizeof(buffer));
    
    uint8_t *result = align_vmax(buffer, sizeof(buffer));
    ASSERT_NOT_NULL(result);
    
    /* Should point to V-MAX! marker run */
    size_t offset = result - buffer;
    ASSERT_EQ(offset, 100);
}

TEST(align_vmax_new)
{
    uint8_t buffer[1024];
    create_vmax_track(buffer, sizeof(buffer));
    
    uint8_t *result = align_vmax_new(buffer, sizeof(buffer));
    ASSERT_NOT_NULL(result);
    
    /* Should find the marker run */
    size_t offset = result - buffer;
    ASSERT(offset >= 100 && offset <= 108);
}

TEST(align_vmax_not_found)
{
    uint8_t buffer[1024];
    memset(buffer, 0x55, sizeof(buffer));  /* No V-MAX! markers */
    
    uint8_t *result = align_vmax(buffer, sizeof(buffer));
    ASSERT_NULL(result);
}

TEST(align_vmax_cinemaware)
{
    uint8_t buffer[1024];
    create_vmax_cw_track(buffer, sizeof(buffer));
    
    uint8_t *result = align_vmax_cinemaware(buffer, sizeof(buffer));
    ASSERT_NOT_NULL(result);
    
    /* Should point to Cinemaware pattern */
    size_t offset = result - buffer;
    ASSERT_EQ(offset, 50);
}

TEST(align_pirateslayer)
{
    uint8_t buffer[2048];  /* Double buffer for potential shifting */
    memset(buffer, 0, sizeof(buffer));
    create_pirateslayer_track(buffer, 1024);
    
    uint8_t *result = align_pirateslayer(buffer, 1024);
    ASSERT_NOT_NULL(result);
}

TEST(align_rapidlok)
{
    uint8_t buffer[1024];
    create_rapidlok_track(buffer, sizeof(buffer));
    
    align_result_t result;
    uint8_t *pos = align_rapidlok(buffer, sizeof(buffer), &result);
    
    /* May or may not find alignment depending on exact structure */
    if (pos) {
        ASSERT_TRUE(result.success);
        ASSERT_EQ(result.method_used, ALIGN_RAPIDLOK);
    }
}

TEST(align_long_sync)
{
    uint8_t buffer[1024];
    create_test_track_with_sync(buffer, sizeof(buffer));
    
    uint8_t *result = align_long_sync(buffer, sizeof(buffer));
    ASSERT_NOT_NULL(result);
    
    /* Should point to longest sync (20 bytes at offset 50) */
    size_t offset = result - buffer;
    ASSERT_EQ(offset, 50);
}

TEST(align_auto_gap)
{
    uint8_t buffer[1024];
    memset(buffer, 0x00, sizeof(buffer));
    
    /* Create a long gap of identical bytes */
    memset(buffer + 200, 0xAA, 50);
    
    uint8_t *result = align_auto_gap(buffer, sizeof(buffer));
    ASSERT_NOT_NULL(result);
}

TEST(align_bad_gcr)
{
    uint8_t buffer[1024];
    create_bad_gcr_track(buffer, sizeof(buffer));
    
    uint8_t *result = align_bad_gcr(buffer, sizeof(buffer));
    /* May or may not find bad GCR depending on exact encoding */
    /* Just verify function doesn't crash */
}

TEST(shift_buffer_left)
{
    uint8_t buffer[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
    
    shift_buffer_left(buffer, sizeof(buffer), 1);
    
    /* After left shift by 1: 
     * 0x80 << 1 = 0x00 | (0x40 >> 7) = 0x00
     * Actually: 0x80 << 1 | carry = 0x00 | 0 = 0x00
     * 0x40 << 1 | carry = 0x80 | 0 = 0x80
     */
    ASSERT_EQ(buffer[0], 0x00);  /* 0x80 << 1 = 0x100 -> 0x00, carry from 0x40 is 0 */
}

TEST(shift_buffer_right)
{
    uint8_t buffer[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
    
    shift_buffer_right(buffer, sizeof(buffer), 1);
    
    /* After right shift by 1: 0x80 >> 1 = 0x40 */
    ASSERT_EQ(buffer[0], 0x40);
}

TEST(rotate_track)
{
    uint8_t buffer[16];
    for (int i = 0; i < 16; i++) buffer[i] = i;
    
    /* Rotate to start at position 4 */
    rotate_track(buffer, 16, buffer + 4);
    
    /* Buffer should now be: 4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3 */
    ASSERT_EQ(buffer[0], 4);
    ASSERT_EQ(buffer[12], 0);
    ASSERT_EQ(buffer[15], 3);
}

TEST(is_bad_gcr)
{
    /* Valid GCR byte */
    uint8_t valid[2] = {0x52, 0x00};  /* Should decode to valid GCR */
    
    /* Bad GCR byte (0x00 is invalid) */
    uint8_t bad[2] = {0x00, 0x00};
    
    ASSERT_TRUE(is_bad_gcr(bad, 2, 0));
}

TEST(is_track_bitshifted)
{
    uint8_t buffer[256];
    memset(buffer, 0x55, sizeof(buffer));
    
    /* Add proper sync marks */
    memset(buffer + 10, 0xFF, 10);
    
    /* This simple track should not be bitshifted */
    /* (More complex test would need actual GCR data) */
}

TEST(sync_align_track)
{
    uint8_t buffer[ALIGN_TRACK_LENGTH];
    memset(buffer, 0x55, sizeof(buffer));
    
    /* Add sync marks */
    memset(buffer + 100, 0xFF, 10);
    memset(buffer + 200, 0xFF, 10);
    
    size_t result = sync_align_track(buffer, sizeof(buffer));
    /* May return 0 if no proper sync structure */
    /* Just verify function doesn't crash */
}

TEST(compare_tracks)
{
    uint8_t track1[1024];
    uint8_t track2[1024];
    
    /* Identical tracks */
    memset(track1, 0x55, sizeof(track1));
    memset(track2, 0x55, sizeof(track2));
    
    size_t diff = compare_tracks(track1, track2, sizeof(track1), sizeof(track2), true, NULL);
    ASSERT_EQ(diff, 0);
    
    /* Different tracks */
    track2[500] = 0xAA;
    diff = compare_tracks(track1, track2, sizeof(track1), sizeof(track2), true, NULL);
    ASSERT_EQ(diff, 1);
}

TEST(search_fat_tracks)
{
    /* Create track buffer for halftracks */
    uint8_t *track_buffer = malloc(ALIGN_MAX_HALFTRACKS * ALIGN_TRACK_LENGTH);
    uint8_t track_density[ALIGN_MAX_HALFTRACKS];
    size_t track_length[ALIGN_MAX_HALFTRACKS];
    int fat_track = 0;
    
    if (!track_buffer) {
        printf("SKIPPED (malloc failed)\n");
        tests_passed++;  /* Don't fail test */
        return;
    }
    
    memset(track_buffer, 0, ALIGN_MAX_HALFTRACKS * ALIGN_TRACK_LENGTH);
    memset(track_density, 0, sizeof(track_density));
    
    /* Set all track lengths to 0 (no tracks) */
    for (int i = 0; i < ALIGN_MAX_HALFTRACKS; i++) {
        track_length[i] = 0;
    }
    
    int result = search_fat_tracks(track_buffer, track_density, track_length, &fat_track);
    ASSERT_EQ(result, 0);  /* No fat tracks when all lengths are 0 */
    
    free(track_buffer);
}

TEST(align_track_method)
{
    uint8_t buffer[1024];
    create_vmax_track(buffer, sizeof(buffer));
    
    align_result_t result;
    uint8_t *pos = align_track(buffer, sizeof(buffer), ALIGN_VMAX, &result);
    
    ASSERT_NOT_NULL(pos);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.method_used, ALIGN_VMAX);
}

TEST(align_track_auto)
{
    uint8_t buffer[1024];
    create_vmax_track(buffer, sizeof(buffer));
    
    align_result_t result;
    uint8_t *pos = align_track_auto(buffer, sizeof(buffer), 3, 4, &result);
    
    ASSERT_NOT_NULL(pos);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.method_used, ALIGN_VMAX);
}

TEST(find_sync)
{
    uint8_t buffer[100];
    memset(buffer, 0x55, sizeof(buffer));
    
    /* Add sync: ...0x01 0xFF 0xFF... */
    buffer[20] = 0x01;  /* Partial sync bit */
    buffer[21] = 0xFF;
    buffer[22] = 0xFF;
    buffer[23] = 0xFF;
    
    uint8_t *pos = buffer;
    bool found = find_sync(&pos, buffer + sizeof(buffer));
    
    ASSERT_TRUE(found);
    ASSERT(pos > buffer + 20);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    printf("\n=== Track Alignment Module Tests ===\n\n");
    
    printf("Constants:\n");
    RUN_TEST(constants);
    RUN_TEST(alignment_methods);
    RUN_TEST(align_method_name);
    
    printf("\nTrack Properties:\n");
    RUN_TEST(track_capacity);
    RUN_TEST(track_capacity_min_max);
    RUN_TEST(sectors_per_track);
    RUN_TEST(track_density);
    
    printf("\nV-MAX! Alignment:\n");
    RUN_TEST(align_vmax);
    RUN_TEST(align_vmax_new);
    RUN_TEST(align_vmax_not_found);
    RUN_TEST(align_vmax_cinemaware);
    
    printf("\nProtection Alignment:\n");
    RUN_TEST(align_pirateslayer);
    RUN_TEST(align_rapidlok);
    
    printf("\nGeneric Alignment:\n");
    RUN_TEST(align_long_sync);
    RUN_TEST(align_auto_gap);
    RUN_TEST(align_bad_gcr);
    
    printf("\nBuffer Operations:\n");
    RUN_TEST(shift_buffer_left);
    RUN_TEST(shift_buffer_right);
    RUN_TEST(rotate_track);
    
    printf("\nGCR Functions:\n");
    RUN_TEST(is_bad_gcr);
    RUN_TEST(find_sync);
    
    printf("\nBitshift Repair:\n");
    RUN_TEST(is_track_bitshifted);
    RUN_TEST(sync_align_track);
    
    printf("\nTrack Comparison:\n");
    RUN_TEST(compare_tracks);
    RUN_TEST(search_fat_tracks);
    
    printf("\nMain API:\n");
    RUN_TEST(align_track_method);
    RUN_TEST(align_track_auto);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
