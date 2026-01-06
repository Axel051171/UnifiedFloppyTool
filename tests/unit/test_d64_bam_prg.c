/**
 * @file test_d64_bam_prg.c
 * @brief Unit Tests for D64 BAM and PRG API
 * @version 1.0.0
 * 
 * Tests the new BAM and PRG manipulation functions.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <uft/cbm/uft_d64_layout.h>
#include <uft/cbm/uft_d64_bam.h>
#include <uft/cbm/uft_d64_prg.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        return false; \
    } \
    tests_passed++; \
} while(0)

#define RUN_TEST(fn) do { \
    printf("Running %s...\n", #fn); \
    if (fn()) { \
        printf("  PASS\n"); \
    } else { \
        printf("  FAILED\n"); \
    } \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 * Mock D64 Image Creation
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool create_test_d64(uft_d64_image_t* img)
{
    /* Create a minimal valid D64 image */
    int rc = uft_d64_create(UFT_D64_TYPE_35, img);
    if (rc != 0) return false;
    
    /* Initialize BAM sector (Track 18, Sector 0) */
    uint8_t* bam = uft_d64_get_sector(img, 18, 0);
    if (!bam) return false;
    
    /* Set up basic BAM structure */
    bam[0] = 18;    /* Directory track */
    bam[1] = 1;     /* Directory sector */
    bam[2] = 0x41;  /* DOS version 'A' */
    bam[3] = 0x00;  /* Unused */
    
    /* Initialize BAM entries for all 35 tracks */
    for (int t = 1; t <= 35; t++) {
        uint8_t spt = uft_d64_sectors_per_track(img->layout, (uint8_t)t);
        size_t base = 0x04 + (size_t)(t - 1) * 4;
        
        /* All sectors free initially (except track 18) */
        if (t == 18) {
            bam[base] = spt - 2;  /* BAM and first dir sector used */
            bam[base + 1] = 0xFC; /* Sectors 0,1 allocated */
            bam[base + 2] = 0xFF;
            bam[base + 3] = 0x1F; /* Only 19 sectors on track 18 */
        } else {
            bam[base] = spt;
            bam[base + 1] = 0xFF;
            bam[base + 2] = 0xFF;
            bam[base + 3] = (spt > 16) ? 0xFF : (uint8_t)((1 << spt) - 1);
        }
    }
    
    /* Set disk name "TEST DISK" at offset 0x90 */
    memset(&bam[0x90], 0xA0, 16);  /* Pad with shifted space */
    memcpy(&bam[0x90], "TEST DISK", 9);
    
    /* Set disk ID "TD" at offset 0xA2 */
    bam[0xA2] = 'T';
    bam[0xA3] = 'D';
    bam[0xA4] = 0xA0;
    bam[0xA5] = '2';
    bam[0xA6] = 'A';
    
    /* Initialize directory sector (Track 18, Sector 1) */
    uint8_t* dir = uft_d64_get_sector(img, 18, 1);
    if (!dir) return false;
    
    memset(dir, 0, 256);
    dir[0] = 0;   /* No next sector */
    dir[1] = 0xFF;
    
    /* Add a test PRG file entry */
    uint8_t* entry = dir + 0x00;
    entry[2] = 0x82;  /* PRG, closed */
    entry[3] = 17;    /* Start track */
    entry[4] = 0;     /* Start sector */
    memset(&entry[5], 0xA0, 16);
    memcpy(&entry[5], "HELLO", 5);
    entry[30] = 1;    /* Size: 1 block */
    entry[31] = 0;
    
    /* Create the PRG file content (Track 17, Sector 0) */
    uint8_t* prg = uft_d64_get_sector(img, 17, 0);
    if (!prg) return false;
    
    memset(prg, 0, 256);
    prg[0] = 0;       /* Last sector */
    prg[1] = 10;      /* 10 bytes used */
    prg[2] = 0x01;    /* Load address low: $0801 */
    prg[3] = 0x08;    /* Load address high */
    prg[4] = 0x00;    /* BASIC: next line pointer low */
    prg[5] = 0x00;    /* BASIC: next line pointer high (end) */
    
    /* Mark the PRG sector as allocated */
    bam = uft_d64_get_sector(img, 18, 0);
    size_t base = 0x04 + 16 * 4;  /* Track 17 */
    bam[base] = 20;  /* 21 - 1 = 20 free */
    bam[base + 1] = 0xFE;  /* Sector 0 allocated */
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * BAM Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool test_bam_read_info(void)
{
    uft_d64_image_t img;
    TEST_ASSERT(create_test_d64(&img), "create_test_d64");
    
    uft_d64_bam_info_t info;
    int rc = uft_d64_bam_read_info(&img, &info);
    
    TEST_ASSERT(rc == 0, "bam_read_info returns 0");
    TEST_ASSERT(info.dir_track == 18, "dir_track == 18");
    TEST_ASSERT(info.dir_sector == 1, "dir_sector == 1");
    TEST_ASSERT(info.dos_version == 0x41, "dos_version == 0x41");
    TEST_ASSERT(strcmp(info.disk_name, "TEST DISK") == 0, "disk_name matches");
    TEST_ASSERT(info.disk_id[0] == 'T', "disk_id[0] == 'T'");
    TEST_ASSERT(!info.is_write_protected, "not write protected");
    
    uft_d64_free(&img);
    return true;
}

static bool test_bam_get_free_blocks(void)
{
    uft_d64_image_t img;
    TEST_ASSERT(create_test_d64(&img), "create_test_d64");
    
    uint16_t free_blocks = uft_d64_bam_get_free_blocks(&img);
    
    /* 664 blocks on standard D64 minus some for dir/bam/test file */
    TEST_ASSERT(free_blocks > 600, "free_blocks > 600");
    TEST_ASSERT(free_blocks < 670, "free_blocks < 670");
    
    uft_d64_free(&img);
    return true;
}

static bool test_bam_allocate_sector(void)
{
    uft_d64_image_t img;
    TEST_ASSERT(create_test_d64(&img), "create_test_d64");
    
    /* Check sector 1 on track 1 is initially free */
    TEST_ASSERT(!uft_d64_bam_is_allocated(&img, 1, 1), "T1/S1 initially free");
    
    /* Allocate it */
    int rc = uft_d64_bam_allocate_sector(&img, 1, 1);
    TEST_ASSERT(rc == 0, "allocate_sector returns 0");
    
    /* Verify it's now allocated */
    TEST_ASSERT(uft_d64_bam_is_allocated(&img, 1, 1), "T1/S1 now allocated");
    
    /* Free it */
    rc = uft_d64_bam_free_sector(&img, 1, 1);
    TEST_ASSERT(rc == 0, "free_sector returns 0");
    
    /* Verify it's free again */
    TEST_ASSERT(!uft_d64_bam_is_allocated(&img, 1, 1), "T1/S1 free again");
    
    uft_d64_free(&img);
    return true;
}

static bool test_bam_allocate_all(void)
{
    uft_d64_image_t img;
    TEST_ASSERT(create_test_d64(&img), "create_test_d64");
    
    uint16_t before = uft_d64_bam_get_free_blocks(&img);
    
    /* Allocate all */
    int rc = uft_d64_bam_allocate_all(&img, NULL);
    TEST_ASSERT(rc == 0, "allocate_all returns 0");
    
    uint16_t after = uft_d64_bam_get_free_blocks(&img);
    TEST_ASSERT(after < before, "fewer free blocks after allocate_all");
    TEST_ASSERT(after < 50, "very few free blocks remain");
    
    uft_d64_free(&img);
    return true;
}

static bool test_bam_unwrite_protect(void)
{
    uft_d64_image_t img;
    TEST_ASSERT(create_test_d64(&img), "create_test_d64");
    
    /* Set a non-standard DOS version (simulate protection) */
    int rc = uft_d64_bam_write_dos_version(&img, 0x00);
    TEST_ASSERT(rc == 0, "write_dos_version returns 0");
    
    uft_d64_bam_info_t info;
    uft_d64_bam_read_info(&img, &info);
    TEST_ASSERT(info.is_write_protected, "now write protected");
    
    /* Remove protection */
    rc = uft_d64_bam_unwrite_protect(&img);
    TEST_ASSERT(rc == 0, "unwrite_protect returns 0");
    
    uft_d64_bam_read_info(&img, &info);
    TEST_ASSERT(!info.is_write_protected, "no longer write protected");
    TEST_ASSERT(info.dos_version == 0x41, "dos_version restored to 0x41");
    
    uft_d64_free(&img);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PRG Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool test_prg_get_info(void)
{
    uft_d64_image_t img;
    TEST_ASSERT(create_test_d64(&img), "create_test_d64");
    
    uft_d64_prg_info_t info;
    int rc = uft_d64_prg_get_info(&img, "HELLO", &info);
    
    TEST_ASSERT(rc == 0, "prg_get_info returns 0");
    TEST_ASSERT(strcmp(info.filename, "HELLO") == 0, "filename matches");
    TEST_ASSERT(info.start_track == 17, "start_track == 17");
    TEST_ASSERT(info.start_sector == 0, "start_sector == 0");
    TEST_ASSERT(info.load_address == 0x0801, "load_address == 0x0801");
    TEST_ASSERT(info.is_basic, "detected as BASIC");
    
    uft_d64_free(&img);
    return true;
}

static bool test_prg_set_load_address(void)
{
    uft_d64_image_t img;
    TEST_ASSERT(create_test_d64(&img), "create_test_d64");
    
    /* Change load address */
    int rc = uft_d64_prg_set_load_address(&img, "HELLO", 0xC000);
    TEST_ASSERT(rc == 0, "set_load_address returns 0");
    
    /* Verify change */
    uint16_t addr;
    rc = uft_d64_prg_get_load_address(&img, "HELLO", &addr);
    TEST_ASSERT(rc == 0, "get_load_address returns 0");
    TEST_ASSERT(addr == 0xC000, "load_address changed to 0xC000");
    
    /* Verify is_basic is now false */
    uft_d64_prg_info_t info;
    uft_d64_prg_get_info(&img, "HELLO", &info);
    TEST_ASSERT(!info.is_basic, "no longer detected as BASIC");
    
    uft_d64_free(&img);
    return true;
}

static bool test_prg_not_found(void)
{
    uft_d64_image_t img;
    TEST_ASSERT(create_test_d64(&img), "create_test_d64");
    
    uft_d64_prg_info_t info;
    int rc = uft_d64_prg_get_info(&img, "NOTEXIST", &info);
    
    TEST_ASSERT(rc != 0, "file not found returns error");
    
    uft_d64_free(&img);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("═══════════════════════════════════════════════════════════════\n");
    printf(" UFT D64 BAM & PRG API Tests\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    /* BAM Tests */
    printf("BAM Tests:\n");
    RUN_TEST(test_bam_read_info);
    RUN_TEST(test_bam_get_free_blocks);
    RUN_TEST(test_bam_allocate_sector);
    RUN_TEST(test_bam_allocate_all);
    RUN_TEST(test_bam_unwrite_protect);
    
    printf("\nPRG Tests:\n");
    RUN_TEST(test_prg_get_info);
    RUN_TEST(test_prg_set_load_address);
    RUN_TEST(test_prg_not_found);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf(" Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
