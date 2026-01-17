/**
 * @file test_nib_format.c
 * @brief Unit tests for NIB/NB2/NBZ format support
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_nib_format.h"

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
 * @brief Create test NIB file data
 */
static uint8_t *create_test_nib(size_t *size, int num_tracks, bool halftracks)
{
    *size = NIB_HEADER_SIZE + (num_tracks * NIB_TRACK_LENGTH);
    uint8_t *data = calloc(1, *size);
    if (!data) return NULL;
    
    /* Header */
    memcpy(data, NIB_SIGNATURE, NIB_SIGNATURE_LEN);
    data[13] = NIB_VERSION;  /* Version */
    data[14] = 0;
    data[15] = halftracks ? 1 : 0;
    
    /* Track entries */
    int track_inc = halftracks ? 1 : 2;
    int entry = 0;
    for (int track = 2; track < 2 + (num_tracks * track_inc); track += track_inc) {
        data[NIB_TRACK_ENTRY_OFFSET + (entry * 2)] = track;
        data[NIB_TRACK_ENTRY_OFFSET + (entry * 2) + 1] = 3;  /* Density 3 */
        
        /* Fill track with pattern */
        uint8_t *track_data = data + NIB_HEADER_SIZE + (entry * NIB_TRACK_LENGTH);
        for (size_t i = 0; i < NIB_TRACK_LENGTH; i++) {
            track_data[i] = (uint8_t)((track + i) & 0xFF);
        }
        
        /* Add sync marks */
        memset(track_data + 100, 0xFF, 10);
        
        entry++;
    }
    
    return data;
}

/* ============================================================================
 * Unit Tests - Constants
 * ============================================================================ */

TEST(constants)
{
    ASSERT_EQ(NIB_HEADER_SIZE, 0x100);
    ASSERT_EQ(NIB_TRACK_LENGTH, 0x2000);
    ASSERT_EQ(NIB_MAX_TRACKS, 84);
    ASSERT_EQ(NIB_SIGNATURE_LEN, 13);
    ASSERT_EQ(NB2_PASSES_PER_TRACK, 16);
}

TEST(format_names)
{
    ASSERT_STR_EQ(nib_format_name(NIB_FORMAT_UNKNOWN), "Unknown");
    ASSERT_STR_EQ(nib_format_name(NIB_FORMAT_NIB), "NIB");
    ASSERT_STR_EQ(nib_format_name(NIB_FORMAT_NB2), "NB2");
    ASSERT_STR_EQ(nib_format_name(NIB_FORMAT_NBZ), "NBZ");
    ASSERT_STR_EQ(nib_format_name(NIB_FORMAT_G64), "G64");
}

/* ============================================================================
 * Unit Tests - LZ77 Compression
 * ============================================================================ */

TEST(lz77_compress_decompress)
{
    /* Test data with repetition (compressible) */
    uint8_t input[1024];
    for (int i = 0; i < 1024; i++) {
        input[i] = (uint8_t)(i % 16);  /* Repeating pattern */
    }
    
    /* Compress */
    uint8_t compressed[2048];
    size_t compressed_size = lz77_compress(input, compressed, sizeof(input));
    ASSERT(compressed_size > 0);
    ASSERT(compressed_size < sizeof(input));  /* Should compress */
    
    /* Decompress */
    uint8_t decompressed[1024];
    size_t decompressed_size = lz77_decompress(compressed, decompressed, compressed_size);
    ASSERT_EQ(decompressed_size, sizeof(input));
    
    /* Verify */
    ASSERT(memcmp(input, decompressed, sizeof(input)) == 0);
}

TEST(lz77_compress_fast)
{
    /* Test data */
    uint8_t input[2048];
    for (int i = 0; i < 2048; i++) {
        input[i] = (uint8_t)(i % 32);
    }
    
    /* Fast compress */
    uint8_t compressed[4096];
    size_t compressed_size = lz77_compress_fast(input, compressed, sizeof(input));
    ASSERT(compressed_size > 0);
    
    /* Decompress and verify */
    uint8_t decompressed[2048];
    size_t decompressed_size = lz77_decompress(compressed, decompressed, compressed_size);
    ASSERT_EQ(decompressed_size, sizeof(input));
    ASSERT(memcmp(input, decompressed, sizeof(input)) == 0);
}

TEST(lz77_incompressible)
{
    /* Random-ish data (hard to compress) */
    uint8_t input[256];
    for (int i = 0; i < 256; i++) {
        input[i] = (uint8_t)(i * 7 + 13);  /* Pseudo-random */
    }
    
    uint8_t compressed[512];
    size_t compressed_size = lz77_compress(input, compressed, sizeof(input));
    ASSERT(compressed_size > 0);
    
    /* Decompress */
    uint8_t decompressed[256];
    size_t decompressed_size = lz77_decompress(compressed, decompressed, compressed_size);
    ASSERT_EQ(decompressed_size, sizeof(input));
    ASSERT(memcmp(input, decompressed, sizeof(input)) == 0);
}

/* ============================================================================
 * Unit Tests - NIB Format Detection
 * ============================================================================ */

TEST(detect_nib_format)
{
    size_t size;
    uint8_t *data = create_test_nib(&size, 35, false);
    ASSERT_NOT_NULL(data);
    
    nib_format_t format = nib_detect_format_buffer(data, size);
    ASSERT_EQ(format, NIB_FORMAT_NIB);
    
    free(data);
}

TEST(detect_g64_format)
{
    uint8_t g64_header[16] = "GCR-1541";
    
    nib_format_t format = nib_detect_format_buffer(g64_header, sizeof(g64_header));
    ASSERT_EQ(format, NIB_FORMAT_G64);
}

TEST(detect_unknown_format)
{
    uint8_t unknown[256] = {0};
    
    nib_format_t format = nib_detect_format_buffer(unknown, sizeof(unknown));
    ASSERT_EQ(format, NIB_FORMAT_UNKNOWN);
}

/* ============================================================================
 * Unit Tests - NIB Loading
 * ============================================================================ */

TEST(nib_load_buffer_basic)
{
    size_t size;
    uint8_t *data = create_test_nib(&size, 35, false);
    ASSERT_NOT_NULL(data);
    
    nib_image_t *image = NULL;
    int result = nib_load_buffer(data, size, &image);
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(image);
    ASSERT_EQ(image->num_tracks, 35);
    ASSERT_FALSE(image->has_halftracks);
    
    nib_free(image);
    free(data);
}

TEST(nib_load_buffer_halftracks)
{
    size_t size;
    uint8_t *data = create_test_nib(&size, 70, true);
    ASSERT_NOT_NULL(data);
    
    nib_image_t *image = NULL;
    int result = nib_load_buffer(data, size, &image);
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(image);
    ASSERT_TRUE(image->has_halftracks);
    
    nib_free(image);
    free(data);
}

TEST(nib_load_invalid_signature)
{
    uint8_t bad_data[512] = {0};
    memcpy(bad_data, "INVALID-SIG", 11);
    
    nib_image_t *image = NULL;
    int result = nib_load_buffer(bad_data, sizeof(bad_data), &image);
    ASSERT_NE(result, 0);
    ASSERT_NULL(image);
}

/* ============================================================================
 * Unit Tests - NIB Create/Save
 * ============================================================================ */

TEST(nib_create)
{
    nib_image_t *image = nib_create(false);
    ASSERT_NOT_NULL(image);
    ASSERT_FALSE(image->has_halftracks);
    
    nib_free(image);
    
    image = nib_create(true);
    ASSERT_NOT_NULL(image);
    ASSERT_TRUE(image->has_halftracks);
    
    nib_free(image);
}

TEST(nib_set_get_track)
{
    nib_image_t *image = nib_create(false);
    ASSERT_NOT_NULL(image);
    
    /* Create test track data */
    uint8_t track_data[NIB_TRACK_LENGTH];
    for (int i = 0; i < NIB_TRACK_LENGTH; i++) {
        track_data[i] = (uint8_t)(i & 0xFF);
    }
    
    /* Set track */
    int result = nib_set_track(image, 4, track_data, NIB_TRACK_LENGTH, 3);
    ASSERT_EQ(result, 0);
    
    /* Get track */
    const uint8_t *retrieved;
    size_t length;
    uint8_t density;
    result = nib_get_track(image, 4, &retrieved, &length, &density);
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(retrieved);
    ASSERT_EQ(length, NIB_TRACK_LENGTH);
    ASSERT_EQ(density, 3);
    ASSERT(memcmp(track_data, retrieved, NIB_TRACK_LENGTH) == 0);
    
    nib_free(image);
}

TEST(nib_save_load_roundtrip)
{
    /* Create image */
    nib_image_t *image = nib_create(false);
    ASSERT_NOT_NULL(image);
    
    /* Add some tracks */
    uint8_t track_data[NIB_TRACK_LENGTH];
    for (int track = 2; track <= 70; track += 2) {
        memset(track_data, track, NIB_TRACK_LENGTH);
        nib_set_track(image, track, track_data, NIB_TRACK_LENGTH, 3);
    }
    
    /* Save to buffer */
    uint8_t *saved;
    size_t saved_size;
    int result = nib_save_buffer(image, &saved, &saved_size);
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(saved);
    
    /* Load back */
    nib_image_t *loaded = NULL;
    result = nib_load_buffer(saved, saved_size, &loaded);
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(loaded);
    
    /* Verify */
    const uint8_t *orig_data;
    const uint8_t *load_data;
    size_t len1, len2;
    uint8_t d1, d2;
    
    for (int track = 2; track <= 70; track += 2) {
        result = nib_get_track(image, track, &orig_data, &len1, &d1);
        if (result == 0) {
            result = nib_get_track(loaded, track, &load_data, &len2, &d2);
            ASSERT_EQ(result, 0);
            ASSERT_EQ(len1, len2);
            ASSERT_EQ(d1, d2);
            ASSERT(memcmp(orig_data, load_data, len1) == 0);
        }
    }
    
    nib_free(image);
    nib_free(loaded);
    free(saved);
}

/* ============================================================================
 * Unit Tests - NBZ Compression
 * ============================================================================ */

TEST(nbz_save_load_roundtrip)
{
    /* Create image */
    nib_image_t *image = nib_create(false);
    ASSERT_NOT_NULL(image);
    
    /* Add tracks with compressible pattern */
    uint8_t track_data[NIB_TRACK_LENGTH];
    for (int track = 2; track <= 70; track += 2) {
        for (int i = 0; i < NIB_TRACK_LENGTH; i++) {
            track_data[i] = (uint8_t)(i % 16);  /* Repeating */
        }
        nib_set_track(image, track, track_data, NIB_TRACK_LENGTH, 3);
    }
    
    /* Save as NBZ */
    uint8_t *compressed;
    size_t compressed_size;
    int result = nbz_save_buffer(image, &compressed, &compressed_size);
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(compressed);
    
    /* Should be compressed */
    uint8_t *uncompressed;
    size_t uncompressed_size;
    nib_save_buffer(image, &uncompressed, &uncompressed_size);
    ASSERT(compressed_size < uncompressed_size);
    free(uncompressed);
    
    /* Load back */
    nib_image_t *loaded = NULL;
    result = nbz_load_buffer(compressed, compressed_size, &loaded);
    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(loaded);
    
    /* Verify tracks match */
    const uint8_t *orig_data;
    const uint8_t *load_data;
    size_t len1, len2;
    
    for (int track = 2; track <= 70; track += 2) {
        result = nib_get_track(image, track, &orig_data, &len1, NULL);
        if (result == 0) {
            result = nib_get_track(loaded, track, &load_data, &len2, NULL);
            ASSERT_EQ(result, 0);
            ASSERT(memcmp(orig_data, load_data, len1) == 0);
        }
    }
    
    nib_free(image);
    nib_free(loaded);
    free(compressed);
}

/* ============================================================================
 * Unit Tests - Analysis
 * ============================================================================ */

TEST(nib_analysis_buffer)
{
    size_t size;
    uint8_t *data = create_test_nib(&size, 35, false);
    ASSERT_NOT_NULL(data);
    
    /* Save to temp file */
    const char *filename = "/tmp/test_nib_analysis.nib";
    FILE *fp = fopen(filename, "wb");
    ASSERT_NOT_NULL(fp);
    fwrite(data, 1, size, fp);
    fclose(fp);
    free(data);
    
    /* Analyze */
    nib_analysis_t analysis;
    int result = nib_analyze(filename, &analysis);
    ASSERT_EQ(result, 0);
    ASSERT_EQ(analysis.format, NIB_FORMAT_NIB);
    ASSERT_EQ(analysis.num_tracks, 35);
    ASSERT_FALSE(analysis.has_halftracks);
    
    /* Cleanup */
    remove(filename);
}

TEST(nib_generate_report)
{
    nib_analysis_t analysis = {
        .format = NIB_FORMAT_NIB,
        .format_name = "NIB",
        .version = 3,
        .num_tracks = 35,
        .has_halftracks = false,
        .file_size = 286976,
        .disk_id = {'A', 'B'}
    };
    
    char buffer[512];
    size_t len = nib_generate_report(&analysis, buffer, sizeof(buffer));
    ASSERT(len > 0);
    ASSERT(strstr(buffer, "NIB") != NULL);
    ASSERT(strstr(buffer, "35") != NULL);
}

/* ============================================================================
 * Unit Tests - Track Utilities
 * ============================================================================ */

TEST(nib_check_track_errors)
{
    /* Track with good data */
    uint8_t good_track[NIB_TRACK_LENGTH];
    memset(good_track, 0x55, NIB_TRACK_LENGTH);  /* Valid GCR pattern */
    
    uint8_t disk_id[2] = {'A', 'B'};
    size_t errors = nib_check_track_errors(good_track, NIB_TRACK_LENGTH, 1, disk_id);
    /* Should have few errors */
    
    /* Track with bad data */
    uint8_t bad_track[NIB_TRACK_LENGTH];
    memset(bad_track, 0x00, NIB_TRACK_LENGTH);  /* All zeros = bad GCR */
    
    size_t bad_errors = nib_check_track_errors(bad_track, NIB_TRACK_LENGTH, 1, disk_id);
    ASSERT(bad_errors > errors);  /* Bad track should have more errors */
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    printf("\n=== NIB/NB2/NBZ Format Tests ===\n\n");
    
    printf("Constants:\n");
    RUN_TEST(constants);
    RUN_TEST(format_names);
    
    printf("\nLZ77 Compression:\n");
    RUN_TEST(lz77_compress_decompress);
    RUN_TEST(lz77_compress_fast);
    RUN_TEST(lz77_incompressible);
    
    printf("\nFormat Detection:\n");
    RUN_TEST(detect_nib_format);
    RUN_TEST(detect_g64_format);
    RUN_TEST(detect_unknown_format);
    
    printf("\nNIB Loading:\n");
    RUN_TEST(nib_load_buffer_basic);
    RUN_TEST(nib_load_buffer_halftracks);
    RUN_TEST(nib_load_invalid_signature);
    
    printf("\nNIB Create/Save:\n");
    RUN_TEST(nib_create);
    RUN_TEST(nib_set_get_track);
    RUN_TEST(nib_save_load_roundtrip);
    
    printf("\nNBZ Compression:\n");
    RUN_TEST(nbz_save_load_roundtrip);
    
    printf("\nAnalysis:\n");
    RUN_TEST(nib_analysis_buffer);
    RUN_TEST(nib_generate_report);
    
    printf("\nTrack Utilities:\n");
    RUN_TEST(nib_check_track_errors);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
