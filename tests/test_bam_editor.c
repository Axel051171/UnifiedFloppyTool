/**
 * @file test_bam_editor.c
 * @brief Unit tests for BAM Editor
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_bam_editor.h"

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
#define ASSERT_NULL(p) ASSERT((p) == NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/* ============================================================================
 * Unit Tests - Constants
 * ============================================================================ */

TEST(constants)
{
    ASSERT_EQ(BAM_D64_35_TRACKS, 174848);
    ASSERT_EQ(BAM_D64_40_TRACKS, 196608);
    ASSERT_EQ(BAM_TRACK, 18);
    ASSERT_EQ(BAM_SECTOR, 0);
    ASSERT_EQ(DIR_TRACK, 18);
    ASSERT_EQ(DIR_FIRST_SECTOR, 1);
    ASSERT_EQ(BAM_TOTAL_BLOCKS_35, 683);
    ASSERT_EQ(BAM_TOTAL_BLOCKS_40, 768);
}

TEST(sectors_per_track)
{
    ASSERT_EQ(bam_sectors_for_track(1), 21);
    ASSERT_EQ(bam_sectors_for_track(17), 21);
    ASSERT_EQ(bam_sectors_for_track(18), 19);
    ASSERT_EQ(bam_sectors_for_track(24), 19);
    ASSERT_EQ(bam_sectors_for_track(25), 18);
    ASSERT_EQ(bam_sectors_for_track(30), 18);
    ASSERT_EQ(bam_sectors_for_track(31), 17);
    ASSERT_EQ(bam_sectors_for_track(35), 17);
    ASSERT_EQ(bam_sectors_for_track(0), 0);
    ASSERT_EQ(bam_sectors_for_track(43), 0);
}

TEST(sector_offset)
{
    /* Track 1, sector 0 = byte 0 */
    ASSERT_EQ(bam_sector_offset(1, 0), 0);
    
    /* Track 1, sector 1 = byte 256 */
    ASSERT_EQ(bam_sector_offset(1, 1), 256);
    
    /* Track 2, sector 0 = byte 21*256 */
    ASSERT_EQ(bam_sector_offset(2, 0), 21 * 256);
    
    /* Invalid track/sector */
    ASSERT_EQ(bam_sector_offset(0, 0), -1);
    ASSERT_EQ(bam_sector_offset(1, 21), -1);  /* Only 21 sectors on track 1 */
}

/* ============================================================================
 * Unit Tests - D64 Creation
 * ============================================================================ */

TEST(create_d64_35)
{
    uint8_t *data;
    size_t size;
    
    int ret = bam_create_d64(35, "TEST DISK", "01", &data, &size);
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(data);
    ASSERT_EQ(size, BAM_D64_35_TRACKS);
    
    /* Create editor to verify */
    bam_editor_t *editor = bam_editor_create(data, size);
    ASSERT_NOT_NULL(editor);
    ASSERT_EQ(editor->num_tracks, 35);
    
    disk_info_t info;
    bam_get_disk_info(editor, &info);
    ASSERT_STR_EQ(info.disk_name, "TEST DISK");
    ASSERT_STR_EQ(info.disk_id, "01");
    ASSERT(info.free_blocks > 600);
    
    bam_editor_free(editor);
    free(data);
}

TEST(create_d64_40)
{
    uint8_t *data;
    size_t size;
    
    int ret = bam_create_d64(40, "EXTENDED", "EX", &data, &size);
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(data);
    ASSERT_EQ(size, BAM_D64_40_TRACKS);
    
    bam_editor_t *editor = bam_editor_create(data, size);
    ASSERT_NOT_NULL(editor);
    ASSERT_EQ(editor->num_tracks, 40);
    
    bam_editor_free(editor);
    free(data);
}

/* ============================================================================
 * Unit Tests - Editor
 * ============================================================================ */

TEST(editor_create)
{
    uint8_t *data;
    size_t size;
    bam_create_d64(35, "TEST", "01", &data, &size);
    
    bam_editor_t *editor = bam_editor_create(data, size);
    ASSERT_NOT_NULL(editor);
    ASSERT_EQ(editor->d64_data, data);
    ASSERT_EQ(editor->d64_size, size);
    ASSERT_EQ(editor->num_tracks, 35);
    ASSERT_FALSE(editor->has_errors);
    
    bam_editor_free(editor);
    free(data);
}

TEST(editor_invalid)
{
    /* Too small */
    uint8_t small[1000];
    bam_editor_t *editor = bam_editor_create(small, sizeof(small));
    ASSERT_NULL(editor);
    
    /* NULL */
    editor = bam_editor_create(NULL, 0);
    ASSERT_NULL(editor);
}

/* ============================================================================
 * Unit Tests - Disk Info
 * ============================================================================ */

TEST(disk_info)
{
    uint8_t *data;
    size_t size;
    bam_create_d64(35, "MY DISK", "42", &data, &size);
    
    bam_editor_t *editor = bam_editor_create(data, size);
    
    disk_info_t info;
    int ret = bam_get_disk_info(editor, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(info.disk_name, "MY DISK");
    ASSERT_STR_EQ(info.disk_id, "42");
    ASSERT_STR_EQ(info.dos_type, "2A");
    ASSERT_EQ(info.num_tracks, 35);
    ASSERT_EQ(info.total_blocks, BAM_TOTAL_BLOCKS_35);
    ASSERT(info.free_blocks > 0);
    ASSERT_EQ(info.num_files, 0);
    
    bam_editor_free(editor);
    free(data);
}

TEST(set_disk_name)
{
    uint8_t *data;
    size_t size;
    bam_create_d64(35, "OLD NAME", "01", &data, &size);
    
    bam_editor_t *editor = bam_editor_create(data, size);
    
    bam_set_disk_name(editor, "NEW NAME");
    
    disk_info_t info;
    bam_get_disk_info(editor, &info);
    ASSERT_STR_EQ(info.disk_name, "NEW NAME");
    ASSERT_TRUE(editor->modified);
    
    bam_editor_free(editor);
    free(data);
}

TEST(set_disk_id)
{
    uint8_t *data;
    size_t size;
    bam_create_d64(35, "DISK", "AA", &data, &size);
    
    bam_editor_t *editor = bam_editor_create(data, size);
    
    bam_set_disk_id(editor, "ZZ");
    
    disk_info_t info;
    bam_get_disk_info(editor, &info);
    ASSERT_STR_EQ(info.disk_id, "ZZ");
    
    bam_editor_free(editor);
    free(data);
}

/* ============================================================================
 * Unit Tests - Block Allocation
 * ============================================================================ */

TEST(block_free_check)
{
    uint8_t *data;
    size_t size;
    bam_create_d64(35, "TEST", "01", &data, &size);
    
    bam_editor_t *editor = bam_editor_create(data, size);
    
    /* Track 1 sectors should be free */
    ASSERT_TRUE(bam_is_block_free(editor, 1, 0));
    ASSERT_TRUE(bam_is_block_free(editor, 1, 10));
    
    /* Track 18 sector 0 (BAM) should be allocated */
    ASSERT_FALSE(bam_is_block_free(editor, 18, 0));
    
    /* Track 18 sector 1 (directory) should be allocated */
    ASSERT_FALSE(bam_is_block_free(editor, 18, 1));
    
    bam_editor_free(editor);
    free(data);
}

TEST(allocate_block)
{
    uint8_t *data;
    size_t size;
    bam_create_d64(35, "TEST", "01", &data, &size);
    
    bam_editor_t *editor = bam_editor_create(data, size);
    
    ASSERT_TRUE(bam_is_block_free(editor, 1, 0));
    
    int ret = bam_allocate_block(editor, 1, 0);
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(bam_is_block_free(editor, 1, 0));
    
    /* Try to allocate again */
    ret = bam_allocate_block(editor, 1, 0);
    ASSERT_EQ(ret, -2);  /* Already allocated */
    
    bam_editor_free(editor);
    free(data);
}

TEST(free_block)
{
    uint8_t *data;
    size_t size;
    bam_create_d64(35, "TEST", "01", &data, &size);
    
    bam_editor_t *editor = bam_editor_create(data, size);
    
    /* Allocate then free */
    bam_allocate_block(editor, 1, 5);
    ASSERT_FALSE(bam_is_block_free(editor, 1, 5));
    
    bam_free_block(editor, 1, 5);
    ASSERT_TRUE(bam_is_block_free(editor, 1, 5));
    
    bam_editor_free(editor);
    free(data);
}

TEST(allocate_next_free)
{
    uint8_t *data;
    size_t size;
    bam_create_d64(35, "TEST", "01", &data, &size);
    
    bam_editor_t *editor = bam_editor_create(data, size);
    
    int track, sector;
    int ret = bam_allocate_next_free(editor, 0, &track, &sector);
    
    ASSERT_EQ(ret, 0);
    ASSERT(track >= 1 && track <= 35);
    ASSERT(sector >= 0);
    ASSERT_FALSE(bam_is_block_free(editor, track, sector));
    
    bam_editor_free(editor);
    free(data);
}

TEST(get_track_free)
{
    uint8_t *data;
    size_t size;
    bam_create_d64(35, "TEST", "01", &data, &size);
    
    bam_editor_t *editor = bam_editor_create(data, size);
    
    /* Track 1 should have 21 free sectors */
    ASSERT_EQ(bam_get_track_free(editor, 1), 21);
    
    /* Track 18 has BAM and directory allocated */
    ASSERT(bam_get_track_free(editor, 18) < 19);
    
    bam_editor_free(editor);
    free(data);
}

/* ============================================================================
 * Unit Tests - Validation
 * ============================================================================ */

TEST(validate_clean)
{
    uint8_t *data;
    size_t size;
    bam_create_d64(35, "TEST", "01", &data, &size);
    
    bam_editor_t *editor = bam_editor_create(data, size);
    
    int errors;
    int ret = bam_validate(editor, &errors, NULL, 0);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(errors, 0);
    
    bam_editor_free(editor);
    free(data);
}

TEST(repair_bam)
{
    uint8_t *data;
    size_t size;
    bam_create_d64(35, "TEST", "01", &data, &size);
    
    bam_editor_t *editor = bam_editor_create(data, size);
    
    int fixed;
    int ret = bam_repair(editor, &fixed);
    
    ASSERT_EQ(ret, 0);
    
    bam_editor_free(editor);
    free(data);
}

/* ============================================================================
 * Unit Tests - Sector I/O
 * ============================================================================ */

TEST(read_write_sector)
{
    uint8_t *data;
    size_t size;
    bam_create_d64(35, "TEST", "01", &data, &size);
    
    bam_editor_t *editor = bam_editor_create(data, size);
    
    /* Write test data */
    uint8_t write_buf[256];
    for (int i = 0; i < 256; i++) write_buf[i] = i;
    
    int ret = bam_write_sector(editor, 1, 0, write_buf);
    ASSERT_EQ(ret, 0);
    
    /* Read back */
    uint8_t read_buf[256];
    ret = bam_read_sector(editor, 1, 0, read_buf);
    ASSERT_EQ(ret, 0);
    
    ASSERT(memcmp(write_buf, read_buf, 256) == 0);
    
    bam_editor_free(editor);
    free(data);
}

/* ============================================================================
 * Unit Tests - Utilities
 * ============================================================================ */

TEST(ascii_to_petscii)
{
    char petscii[17];
    memset(petscii, 0, sizeof(petscii));
    
    bam_ascii_to_petscii("hello", petscii, 16);
    /* Lowercase converts to uppercase in PETSCII */
    ASSERT_EQ(petscii[0], 'H');
    ASSERT_EQ(petscii[1], 'E');
    ASSERT_EQ(petscii[2], 'L');
    ASSERT_EQ(petscii[3], 'L');
    ASSERT_EQ(petscii[4], 'O');
}

TEST(petscii_to_ascii)
{
    char petscii[] = "HELLO";
    char ascii[17];
    memset(ascii, 0, sizeof(ascii));
    
    bam_petscii_to_ascii(petscii, ascii, 5);
    ASSERT_STR_EQ(ascii, "HELLO");
}

TEST(file_type_name)
{
    ASSERT_STR_EQ(bam_file_type_name(FILE_TYPE_DEL), "DEL");
    ASSERT_STR_EQ(bam_file_type_name(FILE_TYPE_SEQ), "SEQ");
    ASSERT_STR_EQ(bam_file_type_name(FILE_TYPE_PRG), "PRG");
    ASSERT_STR_EQ(bam_file_type_name(FILE_TYPE_USR), "USR");
    ASSERT_STR_EQ(bam_file_type_name(FILE_TYPE_REL), "REL");
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== BAM Editor Tests ===\n\n");
    
    printf("Constants:\n");
    RUN_TEST(constants);
    RUN_TEST(sectors_per_track);
    RUN_TEST(sector_offset);
    
    printf("\nD64 Creation:\n");
    RUN_TEST(create_d64_35);
    RUN_TEST(create_d64_40);
    
    printf("\nEditor:\n");
    RUN_TEST(editor_create);
    RUN_TEST(editor_invalid);
    
    printf("\nDisk Info:\n");
    RUN_TEST(disk_info);
    RUN_TEST(set_disk_name);
    RUN_TEST(set_disk_id);
    
    printf("\nBlock Allocation:\n");
    RUN_TEST(block_free_check);
    RUN_TEST(allocate_block);
    RUN_TEST(free_block);
    RUN_TEST(allocate_next_free);
    RUN_TEST(get_track_free);
    
    printf("\nValidation:\n");
    RUN_TEST(validate_clean);
    RUN_TEST(repair_bam);
    
    printf("\nSector I/O:\n");
    RUN_TEST(read_write_sector);
    
    printf("\nUtilities:\n");
    RUN_TEST(ascii_to_petscii);
    RUN_TEST(petscii_to_ascii);
    RUN_TEST(file_type_name);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
