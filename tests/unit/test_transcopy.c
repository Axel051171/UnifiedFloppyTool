/**
 * @file test_transcopy.c
 * @brief Unit tests for Transcopy (.tc) format support
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "uft/formats/uft_transcopy.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Testing %s... ", #name); \
    test_##name(); \
    tests_run++; \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint8_t* create_minimal_tc(size_t* out_size)
{
    /* Create minimal valid TC file */
    size_t size = 0x4000 + 256;  /* Header + one track */
    uint8_t* data = (uint8_t*)calloc(1, size);
    
    /* Signature */
    data[0] = 'T';
    data[1] = 'C';
    
    /* Comment */
    memcpy(&data[0x002], "Test Transcopy Image", 20);
    
    /* Disk type: MFM DD */
    data[0x100] = 0x07;
    
    /* Tracks 0-39, 2 sides */
    data[0x101] = 0;   /* track start */
    data[0x102] = 39;  /* track end */
    data[0x103] = 2;   /* sides */
    data[0x104] = 1;   /* increment */
    
    /* Track 0, side 0: offset=0, length=256 */
    data[0x305] = 0;   /* offset low */
    data[0x306] = 0;   /* offset high */
    data[0x505] = 0;   /* length low (256 = 0x100) */
    data[0x506] = 1;   /* length high */
    
    /* Track data at 0x4000 */
    for (int i = 0; i < 256; i++) {
        data[0x4000 + i] = (uint8_t)(i & 0xFF);
    }
    
    *out_size = size;
    return data;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Detection Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(detect_valid)
{
    size_t size;
    uint8_t* data = create_minimal_tc(&size);
    
    ASSERT(uft_tc_detect(data, size) == true);
    
    int conf = uft_tc_detect_confidence(data, size);
    ASSERT(conf >= 70);
    
    free(data);
}

TEST(detect_invalid_signature)
{
    uint8_t data[0x4000] = {0};
    data[0] = 'X';
    data[1] = 'Y';
    
    ASSERT(uft_tc_detect(data, sizeof(data)) == false);
    ASSERT_EQ(uft_tc_detect_confidence(data, sizeof(data)), 0);
}

TEST(detect_too_small)
{
    uint8_t data[100] = {'T', 'C'};
    
    ASSERT(uft_tc_detect(data, sizeof(data)) == false);
}

TEST(detect_null)
{
    ASSERT(uft_tc_detect(NULL, 100) == false);
    ASSERT_EQ(uft_tc_detect_confidence(NULL, 100), 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Open/Close Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(open_valid)
{
    size_t size;
    uint8_t* data = create_minimal_tc(&size);
    
    uft_tc_image_t img;
    uft_tc_status_t status = uft_tc_open(data, size, &img);
    
    ASSERT_EQ(status, UFT_TC_OK);
    ASSERT_EQ(img.disk_type, UFT_TC_TYPE_MFM_DD);
    ASSERT_EQ(img.track_start, 0);
    ASSERT_EQ(img.track_end, 39);
    ASSERT_EQ(img.sides, 2);
    ASSERT_EQ(img.track_increment, 1);
    ASSERT_NE(img.tracks, NULL);
    ASSERT_EQ(img.track_count, 80);  /* 40 tracks * 2 sides */
    
    uft_tc_close(&img);
    free(data);
}

TEST(open_invalid_signature)
{
    uint8_t data[0x4000] = {0};
    data[0] = 'X';
    
    uft_tc_image_t img;
    uft_tc_status_t status = uft_tc_open(data, sizeof(data), &img);
    
    ASSERT_EQ(status, UFT_TC_E_SIGNATURE);
}

TEST(open_null)
{
    uft_tc_image_t img;
    ASSERT_EQ(uft_tc_open(NULL, 100, &img), UFT_TC_E_INVALID);
    
    uint8_t data[0x4000] = {'T', 'C'};
    ASSERT_EQ(uft_tc_open(data, sizeof(data), NULL), UFT_TC_E_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Track Access Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(get_track)
{
    size_t size;
    uint8_t* data = create_minimal_tc(&size);
    
    uft_tc_image_t img;
    ASSERT_EQ(uft_tc_open(data, size, &img), UFT_TC_OK);
    
    const uint8_t* track_data;
    size_t track_len;
    
    uft_tc_status_t status = uft_tc_get_track(&img, 0, 0, &track_data, &track_len);
    ASSERT_EQ(status, UFT_TC_OK);
    ASSERT_EQ(track_len, 256);
    ASSERT_NE(track_data, NULL);
    ASSERT_EQ(track_data[0], 0x00);
    ASSERT_EQ(track_data[255], 0xFF);
    
    uft_tc_close(&img);
    free(data);
}

TEST(get_track_invalid)
{
    size_t size;
    uint8_t* data = create_minimal_tc(&size);
    
    uft_tc_image_t img;
    ASSERT_EQ(uft_tc_open(data, size, &img), UFT_TC_OK);
    
    const uint8_t* track_data;
    size_t track_len;
    
    /* Invalid track number */
    ASSERT_EQ(uft_tc_get_track(&img, 100, 0, &track_data, &track_len), UFT_TC_E_TRACK);
    
    /* Invalid side */
    ASSERT_EQ(uft_tc_get_track(&img, 0, 5, &track_data, &track_len), UFT_TC_E_TRACK);
    
    uft_tc_close(&img);
    free(data);
}

TEST(load_track)
{
    size_t size;
    uint8_t* data = create_minimal_tc(&size);
    
    uft_tc_image_t img;
    ASSERT_EQ(uft_tc_open(data, size, &img), UFT_TC_OK);
    
    /* Load track into memory */
    ASSERT_EQ(uft_tc_load_track(&img, 0, 0), UFT_TC_OK);
    ASSERT_NE(img.tracks[0].data, NULL);
    
    /* Loading again should succeed (no-op) */
    ASSERT_EQ(uft_tc_load_track(&img, 0, 0), UFT_TC_OK);
    
    uft_tc_close(&img);
    free(data);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Track Flags Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(track_flags)
{
    size_t size;
    uint8_t* data = create_minimal_tc(&size);
    
    /* Set some flags */
    data[0x705] = UFT_TC_FLAG_COPY_WEAK | UFT_TC_FLAG_KEEP_LENGTH;
    
    uft_tc_image_t img;
    ASSERT_EQ(uft_tc_open(data, size, &img), UFT_TC_OK);
    
    uint8_t flags = uft_tc_get_track_flags(&img, 0, 0);
    ASSERT(flags & UFT_TC_FLAG_COPY_WEAK);
    ASSERT(flags & UFT_TC_FLAG_KEEP_LENGTH);
    ASSERT(!(flags & UFT_TC_FLAG_VERIFY_WRITE));
    
    uft_tc_close(&img);
    free(data);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Writer Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(writer_init)
{
    uft_tc_writer_t writer;
    
    uft_tc_status_t status = uft_tc_writer_init(&writer, UFT_TC_TYPE_MFM_DD, 40, 2);
    ASSERT_EQ(status, UFT_TC_OK);
    ASSERT_EQ(writer.disk_type, UFT_TC_TYPE_MFM_DD);
    ASSERT_EQ(writer.track_end, 39);
    ASSERT_EQ(writer.sides, 2);
    ASSERT_EQ(writer.track_count, 80);
    ASSERT_NE(writer.tracks, NULL);
    
    uft_tc_writer_free(&writer);
}

TEST(writer_add_track)
{
    uft_tc_writer_t writer;
    ASSERT_EQ(uft_tc_writer_init(&writer, UFT_TC_TYPE_C64_GCR, 35, 1), UFT_TC_OK);
    
    /* Create track data */
    uint8_t track_data[7692];
    for (size_t i = 0; i < sizeof(track_data); i++) {
        track_data[i] = (uint8_t)(i & 0xFF);
    }
    
    ASSERT_EQ(uft_tc_writer_add_track(&writer, 0, 0, track_data, sizeof(track_data), 0), UFT_TC_OK);
    ASSERT_NE(writer.tracks[0].data, NULL);
    ASSERT_EQ(writer.tracks[0].length, sizeof(track_data));
    
    uft_tc_writer_free(&writer);
}

TEST(writer_roundtrip)
{
    /* Create image */
    uft_tc_writer_t writer;
    ASSERT_EQ(uft_tc_writer_init(&writer, UFT_TC_TYPE_MFM_DD, 40, 2), UFT_TC_OK);
    uft_tc_writer_set_comment(&writer, "Roundtrip Test");
    
    /* Add track data */
    uint8_t track_data[6250];
    for (size_t i = 0; i < sizeof(track_data); i++) {
        track_data[i] = (uint8_t)((i * 7) & 0xFF);
    }
    
    ASSERT_EQ(uft_tc_writer_add_track(&writer, 0, 0, track_data, sizeof(track_data), 
                                       UFT_TC_FLAG_KEEP_LENGTH), UFT_TC_OK);
    
    /* Finish and get output */
    uint8_t* out_data;
    size_t out_size;
    ASSERT_EQ(uft_tc_writer_finish(&writer, &out_data, &out_size), UFT_TC_OK);
    ASSERT_NE(out_data, NULL);
    ASSERT(out_size >= UFT_TC_HEADER_SIZE);
    
    /* Read back */
    uft_tc_image_t img;
    ASSERT_EQ(uft_tc_open(out_data, out_size, &img), UFT_TC_OK);
    
    /* Verify */
    ASSERT_EQ(img.disk_type, UFT_TC_TYPE_MFM_DD);
    ASSERT_EQ(img.track_end, 39);
    ASSERT_EQ(img.sides, 2);
    
    const uint8_t* read_data;
    size_t read_len;
    ASSERT_EQ(uft_tc_get_track(&img, 0, 0, &read_data, &read_len), UFT_TC_OK);
    ASSERT_EQ(read_len, sizeof(track_data));
    ASSERT(memcmp(read_data, track_data, sizeof(track_data)) == 0);
    
    uft_tc_close(&img);
    free(out_data);
    uft_tc_writer_free(&writer);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Disk Type Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(disk_type_names)
{
    ASSERT_STR_EQ(uft_tc_disk_type_name(UFT_TC_TYPE_MFM_HD), "MFM High Density");
    ASSERT_STR_EQ(uft_tc_disk_type_name(UFT_TC_TYPE_C64_GCR), "Commodore GCR");
    ASSERT_STR_EQ(uft_tc_disk_type_name(UFT_TC_TYPE_AMIGA_MFM), "Commodore Amiga MFM");
    ASSERT_STR_EQ(uft_tc_disk_type_name(UFT_TC_TYPE_UNKNOWN), "Unknown");
}

TEST(encoding_types)
{
    ASSERT_EQ(uft_tc_get_encoding(UFT_TC_TYPE_MFM_HD), 1);   /* MFM */
    ASSERT_EQ(uft_tc_get_encoding(UFT_TC_TYPE_FM_SD), 2);    /* FM */
    ASSERT_EQ(uft_tc_get_encoding(UFT_TC_TYPE_C64_GCR), 3);  /* GCR */
}

TEST(variable_density)
{
    ASSERT(uft_tc_is_variable_density(UFT_TC_TYPE_C64_GCR) == true);
    ASSERT(uft_tc_is_variable_density(UFT_TC_TYPE_APPLE_GCR) == true);
    ASSERT(uft_tc_is_variable_density(UFT_TC_TYPE_MFM_DD) == false);
}

TEST(expected_track_length)
{
    /* C64 speed zones */
    ASSERT_EQ(uft_tc_expected_track_length(UFT_TC_TYPE_C64_GCR, 1), 7692);   /* Zone 0 */
    ASSERT_EQ(uft_tc_expected_track_length(UFT_TC_TYPE_C64_GCR, 18), 7142);  /* Zone 1 */
    ASSERT_EQ(uft_tc_expected_track_length(UFT_TC_TYPE_C64_GCR, 25), 6666);  /* Zone 2 */
    ASSERT_EQ(uft_tc_expected_track_length(UFT_TC_TYPE_C64_GCR, 31), 6250);  /* Zone 3 */
    
    /* Fixed density */
    ASSERT_EQ(uft_tc_expected_track_length(UFT_TC_TYPE_MFM_HD, 0), 12500);
    ASSERT_EQ(uft_tc_expected_track_length(UFT_TC_TYPE_MFM_DD, 0), 6250);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT Transcopy Format Unit Tests\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    printf("[Detection Tests]\n");
    RUN_TEST(detect_valid);
    RUN_TEST(detect_invalid_signature);
    RUN_TEST(detect_too_small);
    RUN_TEST(detect_null);
    
    printf("\n[Open/Close Tests]\n");
    RUN_TEST(open_valid);
    RUN_TEST(open_invalid_signature);
    RUN_TEST(open_null);
    
    printf("\n[Track Access Tests]\n");
    RUN_TEST(get_track);
    RUN_TEST(get_track_invalid);
    RUN_TEST(load_track);
    
    printf("\n[Track Flags Tests]\n");
    RUN_TEST(track_flags);
    
    printf("\n[Writer Tests]\n");
    RUN_TEST(writer_init);
    RUN_TEST(writer_add_track);
    RUN_TEST(writer_roundtrip);
    
    printf("\n[Disk Type Tests]\n");
    RUN_TEST(disk_type_names);
    RUN_TEST(encoding_types);
    RUN_TEST(variable_density);
    RUN_TEST(expected_track_length);
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
