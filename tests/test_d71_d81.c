/**
 * @file test_d71_d81.c
 * @brief Unit tests for D71/D81 Disk Formats
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_d71_d81.h"

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
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/* ============================================================================
 * Unit Tests - D71 Constants
 * ============================================================================ */

TEST(d71_constants)
{
    ASSERT_EQ(D71_SIZE_STANDARD, 349696);
    ASSERT_EQ(D71_TRACKS, 70);
    ASSERT_EQ(D71_TRACKS_PER_SIDE, 35);
    ASSERT_EQ(D71_BAM_TRACK, 18);
    ASSERT_EQ(D71_BAM2_TRACK, 53);
}

TEST(d71_sectors_per_track)
{
    ASSERT_EQ(d71_sectors_per_track(1), 21);
    ASSERT_EQ(d71_sectors_per_track(18), 19);
    ASSERT_EQ(d71_sectors_per_track(35), 17);
    ASSERT_EQ(d71_sectors_per_track(36), 21);  /* Side 1 */
    ASSERT_EQ(d71_sectors_per_track(70), 17);
    ASSERT_EQ(d71_sectors_per_track(0), 0);
    ASSERT_EQ(d71_sectors_per_track(71), 0);
}

TEST(d71_sector_offset)
{
    /* Track 1, sector 0 = byte 0 */
    ASSERT_EQ(d71_sector_offset(1, 0), 0);
    
    /* Track 1, sector 1 = byte 256 */
    ASSERT_EQ(d71_sector_offset(1, 1), 256);
    
    /* Invalid */
    ASSERT_EQ(d71_sector_offset(0, 0), -1);
    ASSERT_EQ(d71_sector_offset(71, 0), -1);
}

/* ============================================================================
 * Unit Tests - D71 Image Operations
 * ============================================================================ */

TEST(d71_create)
{
    uint8_t *data;
    size_t size;
    
    int ret = d71_create("TEST D71", "71", &data, &size);
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(data);
    ASSERT_EQ(size, D71_SIZE_STANDARD);
    
    /* Validate */
    ASSERT_TRUE(d71_validate(data, size));
    
    free(data);
}

TEST(d71_editor_create)
{
    uint8_t *data;
    size_t size;
    d71_create("EDITOR TEST", "ET", &data, &size);
    
    d71_editor_t *editor = d71_editor_create(data, size);
    ASSERT_NOT_NULL(editor);
    ASSERT_EQ(editor->size, D71_SIZE_STANDARD);
    
    d71_editor_free(editor);
    free(data);
}

TEST(d71_get_info)
{
    uint8_t *data;
    size_t size;
    d71_create("INFO TEST", "IT", &data, &size);
    
    d71_editor_t *editor = d71_editor_create(data, size);
    
    d71_info_t info;
    int ret = d71_get_info(editor, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(info.disk_id, "IT");
    ASSERT_TRUE(info.double_sided);
    ASSERT(info.free_blocks > 0);
    ASSERT(info.total_blocks > 1000);  /* D71 has ~1328 blocks */
    
    d71_editor_free(editor);
    free(data);
}

TEST(d71_block_allocation)
{
    uint8_t *data;
    size_t size;
    d71_create("ALLOC TEST", "AT", &data, &size);
    
    d71_editor_t *editor = d71_editor_create(data, size);
    
    /* Track 1, sector 0 should be free initially */
    ASSERT_TRUE(d71_is_block_free(editor, 1, 0));
    
    /* Allocate */
    int ret = d71_allocate_block(editor, 1, 0);
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(d71_is_block_free(editor, 1, 0));
    
    /* Free */
    ret = d71_free_block(editor, 1, 0);
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(d71_is_block_free(editor, 1, 0));
    
    d71_editor_free(editor);
    free(data);
}

TEST(d71_side1_allocation)
{
    uint8_t *data;
    size_t size;
    d71_create("SIDE1 TEST", "S1", &data, &size);
    
    d71_editor_t *editor = d71_editor_create(data, size);
    
    /* Test side 1 (track 36+) */
    ASSERT_TRUE(d71_is_block_free(editor, 36, 0));
    
    d71_allocate_block(editor, 36, 0);
    ASSERT_FALSE(d71_is_block_free(editor, 36, 0));
    
    d71_free_block(editor, 36, 0);
    ASSERT_TRUE(d71_is_block_free(editor, 36, 0));
    
    d71_editor_free(editor);
    free(data);
}

TEST(d71_read_write_sector)
{
    uint8_t *data;
    size_t size;
    d71_create("RW TEST", "RW", &data, &size);
    
    d71_editor_t *editor = d71_editor_create(data, size);
    
    /* Write test data */
    uint8_t write_buf[256];
    for (int i = 0; i < 256; i++) write_buf[i] = i;
    
    int ret = d71_write_sector(editor, 1, 0, write_buf);
    ASSERT_EQ(ret, 0);
    
    /* Read back */
    uint8_t read_buf[256];
    ret = d71_read_sector(editor, 1, 0, read_buf);
    ASSERT_EQ(ret, 0);
    
    ASSERT(memcmp(write_buf, read_buf, 256) == 0);
    
    d71_editor_free(editor);
    free(data);
}

/* ============================================================================
 * Unit Tests - D81 Constants
 * ============================================================================ */

TEST(d81_constants)
{
    ASSERT_EQ(D81_SIZE_STANDARD, 819200);
    ASSERT_EQ(D81_TRACKS, 80);
    ASSERT_EQ(D81_SECTORS_PER_TRACK, 40);
    ASSERT_EQ(D81_TOTAL_SECTORS, 3200);
    ASSERT_EQ(D81_HEADER_TRACK, 40);
}

TEST(d81_sector_offset)
{
    /* Track 1, sector 0 = byte 0 */
    ASSERT_EQ(d81_sector_offset(1, 0), 0);
    
    /* Track 1, sector 1 = byte 256 */
    ASSERT_EQ(d81_sector_offset(1, 1), 256);
    
    /* Track 2, sector 0 = byte 40*256 */
    ASSERT_EQ(d81_sector_offset(2, 0), 40 * 256);
    
    /* Invalid */
    ASSERT_EQ(d81_sector_offset(0, 0), -1);
    ASSERT_EQ(d81_sector_offset(81, 0), -1);
    ASSERT_EQ(d81_sector_offset(1, 40), -1);
}

/* ============================================================================
 * Unit Tests - D81 Image Operations
 * ============================================================================ */

TEST(d81_create)
{
    uint8_t *data;
    size_t size;
    
    int ret = d81_create("TEST D81", "81", &data, &size);
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(data);
    ASSERT_EQ(size, D81_SIZE_STANDARD);
    
    /* Validate */
    ASSERT_TRUE(d81_validate(data, size));
    
    free(data);
}

TEST(d81_editor_create)
{
    uint8_t *data;
    size_t size;
    d81_create("EDITOR TEST", "ET", &data, &size);
    
    d81_editor_t *editor = d81_editor_create(data, size);
    ASSERT_NOT_NULL(editor);
    ASSERT_EQ(editor->size, D81_SIZE_STANDARD);
    
    d81_editor_free(editor);
    free(data);
}

TEST(d81_get_info)
{
    uint8_t *data;
    size_t size;
    d81_create("INFO TEST", "81", &data, &size);
    
    d81_editor_t *editor = d81_editor_create(data, size);
    
    d81_info_t info;
    int ret = d81_get_info(editor, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(info.disk_id, "81");
    ASSERT(info.free_blocks > 0);
    ASSERT(info.total_blocks > 3000);  /* D81 has 3160 usable blocks */
    
    d81_editor_free(editor);
    free(data);
}

TEST(d81_block_allocation)
{
    uint8_t *data;
    size_t size;
    d81_create("ALLOC TEST", "AT", &data, &size);
    
    d81_editor_t *editor = d81_editor_create(data, size);
    
    /* Track 1, sector 0 should be free */
    ASSERT_TRUE(d81_is_block_free(editor, 1, 0));
    
    /* Allocate */
    int ret = d81_allocate_block(editor, 1, 0);
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(d81_is_block_free(editor, 1, 0));
    
    /* Free */
    ret = d81_free_block(editor, 1, 0);
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(d81_is_block_free(editor, 1, 0));
    
    d81_editor_free(editor);
    free(data);
}

TEST(d81_track_41_plus)
{
    uint8_t *data;
    size_t size;
    d81_create("TRACK41 TEST", "41", &data, &size);
    
    d81_editor_t *editor = d81_editor_create(data, size);
    
    /* Test tracks 41-80 (second BAM sector) */
    ASSERT_TRUE(d81_is_block_free(editor, 41, 0));
    
    d81_allocate_block(editor, 41, 0);
    ASSERT_FALSE(d81_is_block_free(editor, 41, 0));
    
    d81_free_block(editor, 41, 0);
    ASSERT_TRUE(d81_is_block_free(editor, 41, 0));
    
    /* Test track 80 */
    ASSERT_TRUE(d81_is_block_free(editor, 80, 0));
    
    d81_editor_free(editor);
    free(data);
}

TEST(d81_read_write_sector)
{
    uint8_t *data;
    size_t size;
    d81_create("RW TEST", "RW", &data, &size);
    
    d81_editor_t *editor = d81_editor_create(data, size);
    
    /* Write test data */
    uint8_t write_buf[256];
    for (int i = 0; i < 256; i++) write_buf[i] = i;
    
    int ret = d81_write_sector(editor, 1, 0, write_buf);
    ASSERT_EQ(ret, 0);
    
    /* Read back */
    uint8_t read_buf[256];
    ret = d81_read_sector(editor, 1, 0, read_buf);
    ASSERT_EQ(ret, 0);
    
    ASSERT(memcmp(write_buf, read_buf, 256) == 0);
    
    d81_editor_free(editor);
    free(data);
}

/* ============================================================================
 * Unit Tests - Utilities
 * ============================================================================ */

TEST(detect_cbm_disk_type)
{
    uint8_t *d71_data, *d81_data;
    size_t d71_size, d81_size;
    
    d71_create("D71", "71", &d71_data, &d71_size);
    d81_create("D81", "81", &d81_data, &d81_size);
    
    ASSERT_EQ(detect_cbm_disk_type(d71_data, d71_size), '7');
    ASSERT_EQ(detect_cbm_disk_type(d81_data, d81_size), '8');
    
    free(d71_data);
    free(d81_data);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== D71/D81 Disk Format Tests ===\n\n");
    
    printf("D71 Constants:\n");
    RUN_TEST(d71_constants);
    RUN_TEST(d71_sectors_per_track);
    RUN_TEST(d71_sector_offset);
    
    printf("\nD71 Image Operations:\n");
    RUN_TEST(d71_create);
    RUN_TEST(d71_editor_create);
    RUN_TEST(d71_get_info);
    RUN_TEST(d71_block_allocation);
    RUN_TEST(d71_side1_allocation);
    RUN_TEST(d71_read_write_sector);
    
    printf("\nD81 Constants:\n");
    RUN_TEST(d81_constants);
    RUN_TEST(d81_sector_offset);
    
    printf("\nD81 Image Operations:\n");
    RUN_TEST(d81_create);
    RUN_TEST(d81_editor_create);
    RUN_TEST(d81_get_info);
    RUN_TEST(d81_block_allocation);
    RUN_TEST(d81_track_41_plus);
    RUN_TEST(d81_read_write_sector);
    
    printf("\nUtilities:\n");
    RUN_TEST(detect_cbm_disk_type);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
