/**
 * @file test_d64_view.c
 * @brief D64 View Unit Tests
 */

#include "uft_test.h"
#include "../src/formats/commodore/uft_d64_view.h"

/* Minimal D64 image (35 tracks, 683 sectors * 256 bytes = 174848 bytes) */
static uint8_t g_d64_image[174848];

static void init_test_d64(void) {
    memset(g_d64_image, 0, sizeof(g_d64_image));
    
    /* Track 18, Sector 0: BAM */
    /* Offset for T18/S0: sum of sectors for T1-17 * 256 */
    /* T1-17: 21*17 = 357 sectors, offset = 357*256 = 91392 */
    size_t bam_offset = 91392;
    g_d64_image[bam_offset + 0] = 18;  /* Dir track */
    g_d64_image[bam_offset + 1] = 1;   /* Dir sector */
    g_d64_image[bam_offset + 2] = 0x41; /* DOS version 'A' */
    
    /* Track 18, Sector 1: Directory */
    size_t dir_offset = 91392 + 256;
    g_d64_image[dir_offset + 0] = 0;   /* No next dir sector */
    g_d64_image[dir_offset + 1] = 0xFF;
    
    /* First directory entry at offset +2 */
    g_d64_image[dir_offset + 2] = 0x82;  /* PRG, closed */
    g_d64_image[dir_offset + 3] = 1;     /* Start track */
    g_d64_image[dir_offset + 4] = 0;     /* Start sector */
    /* Filename: "TEST" padded with $A0 */
    memcpy(&g_d64_image[dir_offset + 5], "TEST", 4);
    memset(&g_d64_image[dir_offset + 9], 0xA0, 12);
    g_d64_image[dir_offset + 30] = 1;    /* Blocks low */
    g_d64_image[dir_offset + 31] = 0;    /* Blocks high */
}

UFT_TEST(test_d64_open_valid) {
    init_test_d64();
    uft_d64_view_t view;
    uft_d64_status_t st = uft_d64_open(g_d64_image, sizeof(g_d64_image), &view);
    UFT_ASSERT_EQ(st, UFT_D64_OK);
    UFT_ASSERT_EQ(view.geom.tracks, 35);
    UFT_ASSERT_EQ(view.geom.total_sectors, 683);
    UFT_ASSERT_EQ(view.geom.has_error_bytes, 0);
}

UFT_TEST(test_d64_open_with_errors) {
    init_test_d64();
    /* D64 with error bytes: 174848 + 683 = 175531 */
    uint8_t d64_with_err[175531];
    memcpy(d64_with_err, g_d64_image, sizeof(g_d64_image));
    memset(d64_with_err + 174848, 0x01, 683);  /* All OK errors */
    
    uft_d64_view_t view;
    uft_d64_status_t st = uft_d64_open(d64_with_err, sizeof(d64_with_err), &view);
    UFT_ASSERT_EQ(st, UFT_D64_OK);
    UFT_ASSERT_EQ(view.geom.has_error_bytes, 1);
}

UFT_TEST(test_d64_open_invalid_size) {
    uint8_t small[1000];
    uft_d64_view_t view;
    uft_d64_status_t st = uft_d64_open(small, sizeof(small), &view);
    UFT_ASSERT_EQ(st, UFT_D64_E_GEOM);
}

UFT_TEST(test_d64_dir_iteration) {
    init_test_d64();
    uft_d64_view_t view;
    uft_d64_open(g_d64_image, sizeof(g_d64_image), &view);
    
    uft_d64_dir_iter_t it;
    uft_d64_dir_begin(&it);
    UFT_ASSERT_EQ(it.track, 18);
    UFT_ASSERT_EQ(it.sector, 1);
    
    uft_d64_dirent_t ent;
    uft_d64_status_t st = uft_d64_dir_next(&view, &it, &ent);
    UFT_ASSERT_EQ(st, UFT_D64_OK);
    UFT_ASSERT_EQ(ent.type, UFT_D64_FT_PRG);
    UFT_ASSERT_EQ(ent.closed, 1);
    UFT_ASSERT_STR_EQ(ent.name_ascii, "TEST");
}

UFT_TEST_MAIN_BEGIN()
    printf("D64 View Tests:\n");
    UFT_RUN_TEST(test_d64_open_valid);
    UFT_RUN_TEST(test_d64_open_with_errors);
    UFT_RUN_TEST(test_d64_open_invalid_size);
    UFT_RUN_TEST(test_d64_dir_iteration);
UFT_TEST_MAIN_END()
