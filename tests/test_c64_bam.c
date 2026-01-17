/**
 * @file test_c64_bam.c
 * @brief Unit tests for C64 BAM Editor
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_c64_bam.h"

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

/**
 * @brief Create a blank D64 image
 */
static uint8_t *create_blank_d64(void)
{
    uint8_t *data = calloc(1, BAM_D64_SIZE_35);
    if (!data) return NULL;
    
    /* Set up BAM at track 18, sector 0 */
    int bam_offset = bam_sector_offset(18, 0, BAM_FORMAT_D64_35);
    uint8_t *bam = data + bam_offset;
    
    /* Directory link */
    bam[0x00] = 18;  /* Next track */
    bam[0x01] = 1;   /* Next sector */
    bam[0x02] = 'A'; /* DOS version */
    
    /* BAM entries for tracks 1-35 */
    int sectors[] = {
        0,
        21,21,21,21,21,21,21,21,21,21,  /*  1-10 */
        21,21,21,21,21,21,21,19,19,19,  /* 11-20 */
        19,19,19,19,18,18,18,18,18,18,  /* 21-30 */
        17,17,17,17,17                   /* 31-35 */
    };
    
    for (int track = 1; track <= 35; track++) {
        int entry_offset = 4 + (track - 1) * 4;
        int num_sectors = sectors[track];
        
        bam[entry_offset] = (track == 18) ? 0 : num_sectors;  /* Free count */
        
        /* Set all bits for free sectors */
        if (track != 18) {  /* Track 18 is reserved */
            for (int s = 0; s < num_sectors; s++) {
                int byte_idx = entry_offset + 1 + (s / 8);
                int bit_idx = s % 8;
                bam[byte_idx] |= (1 << bit_idx);
            }
        }
    }
    
    /* Disk name "TEST DISK" */
    memset(bam + 0x90, 0xA0, 16);  /* Fill with shifted space */
    memcpy(bam + 0x90, "TEST DISK", 9);
    
    /* Disk ID */
    bam[0xA2] = 'T';
    bam[0xA3] = 'D';
    bam[0xA4] = 0xA0;
    bam[0xA5] = '2';
    bam[0xA6] = 'A';
    
    return data;
}

/* ============================================================================
 * Unit Tests - Format Detection
 * ============================================================================ */

TEST(detect_format_d64_35)
{
    uint8_t *data = malloc(BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(data);
    
    bam_format_t format = bam_detect_format(data, BAM_D64_SIZE_35);
    ASSERT_EQ(format, BAM_FORMAT_D64_35);
    
    free(data);
}

TEST(detect_format_d64_40)
{
    uint8_t *data = malloc(BAM_D64_SIZE_40);
    ASSERT_NOT_NULL(data);
    
    bam_format_t format = bam_detect_format(data, BAM_D64_SIZE_40);
    ASSERT_EQ(format, BAM_FORMAT_D64_40);
    
    free(data);
}

TEST(detect_format_unknown)
{
    uint8_t data[1000];
    bam_format_t format = bam_detect_format(data, sizeof(data));
    ASSERT_EQ(format, BAM_FORMAT_UNKNOWN);
}

/* ============================================================================
 * Unit Tests - Context Management
 * ============================================================================ */

TEST(create_context)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    ASSERT_EQ(ctx->format, BAM_FORMAT_D64_35);
    ASSERT_EQ(ctx->num_tracks, 35);
    ASSERT(ctx->free_sectors > 0);
    
    bam_free_context(ctx);
    free(data);
}

TEST(disk_name)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    char name[17];
    bam_get_disk_name(ctx, name);
    ASSERT_STR_EQ(name, "TEST DISK");
    
    bam_free_context(ctx);
    free(data);
}

TEST(disk_id)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    char id[3];
    bam_get_disk_id(ctx, id);
    ASSERT_EQ(id[0], 'T');
    ASSERT_EQ(id[1], 'D');
    
    bam_free_context(ctx);
    free(data);
}

/* ============================================================================
 * Unit Tests - Block Operations
 * ============================================================================ */

TEST(is_block_free)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    /* Track 1 should be free */
    ASSERT_TRUE(bam_is_block_free(ctx, 1, 0));
    ASSERT_TRUE(bam_is_block_free(ctx, 1, 10));
    
    /* Track 18 is reserved (directory) */
    ASSERT_FALSE(bam_is_block_free(ctx, 18, 0));
    
    bam_free_context(ctx);
    free(data);
}

TEST(allocate_block)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    int free_before = ctx->free_sectors;
    
    /* Allocate a block */
    ASSERT_TRUE(bam_is_block_free(ctx, 1, 0));
    int ret = bam_allocate_block(ctx, 1, 0);
    ASSERT_EQ(ret, 0);
    
    /* Verify it's now allocated */
    ASSERT_FALSE(bam_is_block_free(ctx, 1, 0));
    ASSERT_EQ(ctx->free_sectors, free_before - 1);
    
    bam_free_context(ctx);
    free(data);
}

TEST(free_block)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    /* Allocate then free */
    bam_allocate_block(ctx, 1, 0);
    ASSERT_FALSE(bam_is_block_free(ctx, 1, 0));
    
    int ret = bam_free_block(ctx, 1, 0);
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(bam_is_block_free(ctx, 1, 0));
    
    bam_free_context(ctx);
    free(data);
}

TEST(allocate_first_free)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    bam_alloc_result_t result;
    int ret = bam_allocate_first_free(ctx, &result);
    
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(result.success);
    ASSERT(result.track >= 1 && result.track <= 35);
    ASSERT_NE(result.track, 18);  /* Should skip directory track */
    
    bam_free_context(ctx);
    free(data);
}

TEST(allocate_near)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    bam_alloc_result_t result;
    int ret = bam_allocate_near(ctx, 10, &result);
    
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(result.success);
    /* Should allocate near track 10 */
    ASSERT(result.track >= 8 && result.track <= 12);
    
    bam_free_context(ctx);
    free(data);
}

TEST(free_on_track)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    /* Track 1 should have 21 free sectors */
    ASSERT_EQ(bam_free_on_track(ctx, 1), 21);
    
    /* Track 18 should have 0 free (reserved) */
    ASSERT_EQ(bam_free_on_track(ctx, 18), 0);
    
    bam_free_context(ctx);
    free(data);
}

TEST(total_free)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    int total = bam_total_free(ctx);
    /* 683 total sectors - track 18 (19 sectors) = 664 free */
    ASSERT_EQ(total, 664);
    
    bam_free_context(ctx);
    free(data);
}

TEST(sectors_per_track)
{
    ASSERT_EQ(bam_sectors_per_track(1, BAM_FORMAT_D64_35), 21);
    ASSERT_EQ(bam_sectors_per_track(17, BAM_FORMAT_D64_35), 21);
    ASSERT_EQ(bam_sectors_per_track(18, BAM_FORMAT_D64_35), 19);
    ASSERT_EQ(bam_sectors_per_track(25, BAM_FORMAT_D64_35), 18);
    ASSERT_EQ(bam_sectors_per_track(31, BAM_FORMAT_D64_35), 17);
    ASSERT_EQ(bam_sectors_per_track(35, BAM_FORMAT_D64_35), 17);
}

/* ============================================================================
 * Unit Tests - Sector Access
 * ============================================================================ */

TEST(sector_offset)
{
    /* Track 1, sector 0 should be at offset 0 */
    ASSERT_EQ(bam_sector_offset(1, 0, BAM_FORMAT_D64_35), 0);
    
    /* Track 1, sector 1 should be at offset 256 */
    ASSERT_EQ(bam_sector_offset(1, 1, BAM_FORMAT_D64_35), 256);
    
    /* Track 2, sector 0 should be at offset 21*256 */
    ASSERT_EQ(bam_sector_offset(2, 0, BAM_FORMAT_D64_35), 21 * 256);
}

TEST(read_write_sector)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    /* Write test data */
    uint8_t write_buf[256];
    memset(write_buf, 0xAA, sizeof(write_buf));
    int ret = bam_write_sector(ctx, 1, 0, write_buf);
    ASSERT_EQ(ret, 0);
    
    /* Read it back */
    uint8_t read_buf[256];
    ret = bam_read_sector(ctx, 1, 0, read_buf);
    ASSERT_EQ(ret, 0);
    
    ASSERT(memcmp(write_buf, read_buf, 256) == 0);
    
    bam_free_context(ctx);
    free(data);
}

/* ============================================================================
 * Unit Tests - BAM Writing
 * ============================================================================ */

TEST(set_disk_name)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    int ret = bam_set_disk_name(ctx, "NEW NAME");
    ASSERT_EQ(ret, 0);
    
    char name[17];
    bam_get_disk_name(ctx, name);
    ASSERT_STR_EQ(name, "NEW NAME");
    
    bam_free_context(ctx);
    free(data);
}

TEST(set_disk_id)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    int ret = bam_set_disk_id(ctx, "XY");
    ASSERT_EQ(ret, 0);
    
    char id[3];
    bam_get_disk_id(ctx, id);
    ASSERT_EQ(id[0], 'X');
    ASSERT_EQ(id[1], 'Y');
    
    bam_free_context(ctx);
    free(data);
}

/* ============================================================================
 * Unit Tests - Validation
 * ============================================================================ */

TEST(validate_good_bam)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    int errors;
    bool valid = bam_validate(ctx, &errors);
    ASSERT_TRUE(valid);
    ASSERT_EQ(errors, 0);
    
    bam_free_context(ctx);
    free(data);
}

TEST(repair_bam)
{
    uint8_t *data = create_blank_d64();
    ASSERT_NOT_NULL(data);
    
    bam_context_t *ctx = bam_create_context(data, BAM_D64_SIZE_35);
    ASSERT_NOT_NULL(ctx);
    
    int recovered;
    int ret = bam_repair(ctx, &recovered);
    ASSERT_EQ(ret, 0);
    
    bam_free_context(ctx);
    free(data);
}

/* ============================================================================
 * Unit Tests - Utilities
 * ============================================================================ */

TEST(format_name)
{
    ASSERT_STR_EQ(bam_format_name(BAM_FORMAT_D64_35), "D64 (35 tracks)");
    ASSERT_STR_EQ(bam_format_name(BAM_FORMAT_D64_40), "D64 (40 tracks)");
    ASSERT_STR_EQ(bam_format_name(BAM_FORMAT_D71), "D71 (70 tracks)");
    ASSERT_STR_EQ(bam_format_name(BAM_FORMAT_D81), "D81 (80 tracks)");
}

TEST(file_type_name)
{
    ASSERT_STR_EQ(bam_file_type_name(BAM_FILE_DEL), "DEL");
    ASSERT_STR_EQ(bam_file_type_name(BAM_FILE_SEQ), "SEQ");
    ASSERT_STR_EQ(bam_file_type_name(BAM_FILE_PRG), "PRG");
    ASSERT_STR_EQ(bam_file_type_name(BAM_FILE_USR), "USR");
    ASSERT_STR_EQ(bam_file_type_name(BAM_FILE_REL), "REL");
}

TEST(ascii_petscii)
{
    uint8_t petscii[16];
    char ascii[17];
    
    memset(petscii, 0, sizeof(petscii));
    bam_ascii_to_petscii("HELLO", petscii, 5);
    
    bam_petscii_to_ascii(petscii, ascii, 5);
    ASSERT_STR_EQ(ascii, "HELLO");
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== C64 BAM Editor Tests ===\n\n");
    
    printf("Format Detection:\n");
    RUN_TEST(detect_format_d64_35);
    RUN_TEST(detect_format_d64_40);
    RUN_TEST(detect_format_unknown);
    
    printf("\nContext Management:\n");
    RUN_TEST(create_context);
    RUN_TEST(disk_name);
    RUN_TEST(disk_id);
    
    printf("\nBlock Operations:\n");
    RUN_TEST(is_block_free);
    RUN_TEST(allocate_block);
    RUN_TEST(free_block);
    RUN_TEST(allocate_first_free);
    RUN_TEST(allocate_near);
    RUN_TEST(free_on_track);
    RUN_TEST(total_free);
    RUN_TEST(sectors_per_track);
    
    printf("\nSector Access:\n");
    RUN_TEST(sector_offset);
    RUN_TEST(read_write_sector);
    
    printf("\nBAM Writing:\n");
    RUN_TEST(set_disk_name);
    RUN_TEST(set_disk_id);
    
    printf("\nValidation:\n");
    RUN_TEST(validate_good_bam);
    RUN_TEST(repair_bam);
    
    printf("\nUtilities:\n");
    RUN_TEST(format_name);
    RUN_TEST(file_type_name);
    RUN_TEST(ascii_petscii);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
