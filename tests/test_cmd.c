/**
 * @file test_cmd.c
 * @brief Unit tests for CMD FD2000/FD4000 Disk Format
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_cmd.h"

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
 * Unit Tests - Detection
 * ============================================================================ */

TEST(detect_type_d1m)
{
    ASSERT_EQ(cmd_detect_type(D1M_SIZE), CMD_TYPE_D1M);
}

TEST(detect_type_d2m)
{
    ASSERT_EQ(cmd_detect_type(D2M_SIZE), CMD_TYPE_D2M);
}

TEST(detect_type_d4m)
{
    ASSERT_EQ(cmd_detect_type(D4M_SIZE), CMD_TYPE_D4M);
}

TEST(detect_type_unknown)
{
    ASSERT_EQ(cmd_detect_type(1000), CMD_TYPE_UNKNOWN);
}

TEST(type_name)
{
    ASSERT_STR_EQ(cmd_type_name(CMD_TYPE_D1M), "D1M (1581 Emulation)");
    ASSERT_STR_EQ(cmd_type_name(CMD_TYPE_D2M), "D2M (FD2000)");
    ASSERT_STR_EQ(cmd_type_name(CMD_TYPE_D4M), "D4M (FD4000)");
}

TEST(type_size)
{
    ASSERT_EQ(cmd_type_size(CMD_TYPE_D1M), D1M_SIZE);
    ASSERT_EQ(cmd_type_size(CMD_TYPE_D2M), D2M_SIZE);
    ASSERT_EQ(cmd_type_size(CMD_TYPE_D4M), D4M_SIZE);
}

TEST(type_tracks)
{
    ASSERT_EQ(cmd_type_tracks(CMD_TYPE_D1M), D1M_TRACKS);
    ASSERT_EQ(cmd_type_tracks(CMD_TYPE_D2M), D2M_TRACKS);
    ASSERT_EQ(cmd_type_tracks(CMD_TYPE_D4M), D4M_TRACKS);
}

TEST(type_sectors)
{
    ASSERT_EQ(cmd_type_sectors(CMD_TYPE_D1M), D1M_SECTORS_PER_TRACK);
    ASSERT_EQ(cmd_type_sectors(CMD_TYPE_D2M), D2M_SECTORS_PER_TRACK);
    ASSERT_EQ(cmd_type_sectors(CMD_TYPE_D4M), D4M_SECTORS_PER_TRACK);
}

/* ============================================================================
 * Unit Tests - Editor Operations
 * ============================================================================ */

TEST(create_d2m)
{
    cmd_editor_t editor;
    int ret = cmd_create(CMD_TYPE_D2M, &editor);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(editor.data);
    ASSERT_EQ(editor.size, D2M_SIZE);
    ASSERT_EQ(editor.type, CMD_TYPE_D2M);
    ASSERT_EQ(editor.tracks, D2M_TRACKS);
    ASSERT_EQ(editor.sectors_per_track, D2M_SECTORS_PER_TRACK);
    
    cmd_editor_close(&editor);
}

TEST(create_d4m)
{
    cmd_editor_t editor;
    int ret = cmd_create(CMD_TYPE_D4M, &editor);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(editor.size, D4M_SIZE);
    ASSERT_EQ(editor.type, CMD_TYPE_D4M);
    
    cmd_editor_close(&editor);
}

TEST(format_disk)
{
    cmd_editor_t editor;
    cmd_create(CMD_TYPE_D2M, &editor);
    
    int ret = cmd_format(&editor, "TEST DISK", "TD");
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(editor.modified);
    
    cmd_disk_info_t info;
    cmd_get_info(&editor, &info);
    
    /* Should have most blocks free after format */
    ASSERT(info.free_blocks > 0);
    
    cmd_editor_close(&editor);
}

TEST(editor_close)
{
    cmd_editor_t editor;
    cmd_create(CMD_TYPE_D2M, &editor);
    cmd_editor_close(&editor);
    
    ASSERT_EQ(editor.data, NULL);
    ASSERT_EQ(editor.size, 0);
}

TEST(get_info)
{
    cmd_editor_t editor;
    cmd_create(CMD_TYPE_D2M, &editor);
    cmd_format(&editor, "INFO TEST", "IT");
    
    cmd_disk_info_t info;
    int ret = cmd_get_info(&editor, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.type, CMD_TYPE_D2M);
    ASSERT_EQ(info.total_tracks, D2M_TRACKS);
    ASSERT_EQ(info.sectors_per_track, D2M_SECTORS_PER_TRACK);
    
    cmd_editor_close(&editor);
}

/* ============================================================================
 * Unit Tests - Sector Operations
 * ============================================================================ */

TEST(sector_offset)
{
    cmd_editor_t editor;
    cmd_create(CMD_TYPE_D2M, &editor);
    
    /* Track 1, sector 0 should be at offset 0 */
    ASSERT_EQ(cmd_sector_offset(&editor, 1, 0), 0);
    
    /* Track 1, sector 1 should be at offset 256 */
    ASSERT_EQ(cmd_sector_offset(&editor, 1, 1), CMD_SECTOR_SIZE);
    
    /* Track 2, sector 0 should be at offset sectors_per_track * 256 */
    ASSERT_EQ(cmd_sector_offset(&editor, 2, 0), 
              D2M_SECTORS_PER_TRACK * CMD_SECTOR_SIZE);
    
    cmd_editor_close(&editor);
}

TEST(read_write_sector)
{
    cmd_editor_t editor;
    cmd_create(CMD_TYPE_D2M, &editor);
    
    /* Write test data */
    uint8_t write_buf[CMD_SECTOR_SIZE];
    memset(write_buf, 0x42, CMD_SECTOR_SIZE);
    write_buf[0] = 0xDE;
    write_buf[1] = 0xAD;
    
    int ret = cmd_write_sector(&editor, 5, 3, write_buf);
    ASSERT_EQ(ret, 0);
    
    /* Read back */
    uint8_t read_buf[CMD_SECTOR_SIZE];
    ret = cmd_read_sector(&editor, 5, 3, read_buf);
    ASSERT_EQ(ret, 0);
    
    ASSERT(memcmp(read_buf, write_buf, CMD_SECTOR_SIZE) == 0);
    
    cmd_editor_close(&editor);
}

/* ============================================================================
 * Unit Tests - BAM Operations
 * ============================================================================ */

TEST(allocate_free_block)
{
    cmd_editor_t editor;
    cmd_create(CMD_TYPE_D2M, &editor);
    cmd_format(&editor, "BAM TEST", "BT");
    
    /* Find a free block (should be many after format) */
    bool found_free = cmd_is_block_free(&editor, 10, 5);
    ASSERT_TRUE(found_free);
    
    /* Allocate it */
    int ret = cmd_allocate_block(&editor, 10, 5);
    ASSERT_EQ(ret, 0);
    
    /* Should now be allocated */
    ASSERT_FALSE(cmd_is_block_free(&editor, 10, 5));
    
    /* Free it */
    ret = cmd_free_block(&editor, 10, 5);
    ASSERT_EQ(ret, 0);
    
    /* Should be free again */
    ASSERT_TRUE(cmd_is_block_free(&editor, 10, 5));
    
    cmd_editor_close(&editor);
}

TEST(get_free_blocks)
{
    cmd_editor_t editor;
    cmd_create(CMD_TYPE_D2M, &editor);
    cmd_format(&editor, "FREE TEST", "FT");
    
    int free_before = cmd_get_free_blocks(&editor);
    ASSERT(free_before > 0);
    
    /* Allocate a block */
    cmd_allocate_block(&editor, 20, 5);
    
    int free_after = cmd_get_free_blocks(&editor);
    ASSERT_EQ(free_after, free_before - 1);
    
    cmd_editor_close(&editor);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== CMD FD2000/FD4000 Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_type_d1m);
    RUN_TEST(detect_type_d2m);
    RUN_TEST(detect_type_d4m);
    RUN_TEST(detect_type_unknown);
    RUN_TEST(type_name);
    RUN_TEST(type_size);
    RUN_TEST(type_tracks);
    RUN_TEST(type_sectors);
    
    printf("\nEditor Operations:\n");
    RUN_TEST(create_d2m);
    RUN_TEST(create_d4m);
    RUN_TEST(format_disk);
    RUN_TEST(editor_close);
    RUN_TEST(get_info);
    
    printf("\nSector Operations:\n");
    RUN_TEST(sector_offset);
    RUN_TEST(read_write_sector);
    
    printf("\nBAM Operations:\n");
    RUN_TEST(allocate_free_block);
    RUN_TEST(get_free_blocks);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
