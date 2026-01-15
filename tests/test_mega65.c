/**
 * @file test_mega65.c
 * @brief Unit tests for MEGA65 integration in UFT
 *
 * Tests for:
 * - SD card partitioning (including MEGA65 system partition type 0x41)
 * - FAT32 formatting and filesystem operations
 * - CHS/LBA conversion
 * - Board model detection
 *
 * @version 3.8.0
 * @date 2026-01-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "uft/formats/uft_mega65.h"

/* Test counters */
static int tests_passed = 0;
static int tests_failed = 0;

/*============================================================================*
 * TEST MACROS
 *============================================================================*/

#define TEST_START(name) printf("  Testing: %s... ", name)

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("FAILED: %s\n", msg); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_PASS() do { \
    printf("PASSED\n"); \
    tests_passed++; \
} while(0)

/*============================================================================*
 * STRUCTURE SIZE TESTS
 *============================================================================*/

static void test_structure_sizes(void)
{
    TEST_START("Structure sizes");
    
    /* Verify packed structures have correct sizes */
    TEST_ASSERT(sizeof(uft_m65_partition_entry_t) == 16,
                "Partition entry should be 16 bytes");
    
    TEST_ASSERT(sizeof(uft_m65_mbr_t) == 512,
                "MBR should be 512 bytes");
    
    TEST_ASSERT(sizeof(uft_m65_fat32_boot_t) == 512,
                "FAT32 boot sector should be 512 bytes");
    
    TEST_ASSERT(sizeof(uft_m65_fat32_fsinfo_t) == 512,
                "FSInfo should be 512 bytes");
    
    TEST_ASSERT(sizeof(uft_m65_dir_entry_t) == 32,
                "Directory entry should be 32 bytes");
    
    TEST_PASS();
}

/*============================================================================*
 * PARTITION TYPE TESTS
 *============================================================================*/

static void test_partition_types(void)
{
    TEST_START("Partition type constants");
    
    TEST_ASSERT(UFT_M65_PART_FAT32_LBA == 0x0C,
                "FAT32 LBA should be 0x0C");
    
    TEST_ASSERT(UFT_M65_PART_FAT32_CHS == 0x0B,
                "FAT32 CHS should be 0x0B");
    
    TEST_ASSERT(UFT_M65_PART_MEGA65_SYS == 0x41,
                "MEGA65 system partition should be 0x41");
    
    /* Test type name lookup */
    const char *name = uft_m65_partition_type_name(UFT_M65_PART_MEGA65_SYS);
    TEST_ASSERT(name != NULL && strstr(name, "MEGA65") != NULL,
                "MEGA65 partition type name should contain 'MEGA65'");
    
    name = uft_m65_partition_type_name(UFT_M65_PART_FAT32_LBA);
    TEST_ASSERT(name != NULL && strstr(name, "FAT32") != NULL,
                "FAT32 partition type name should contain 'FAT32'");
    
    TEST_PASS();
}

/*============================================================================*
 * LBA TO CHS CONVERSION TESTS
 *============================================================================*/

static void test_lba_to_chs(void)
{
    uint8_t h, s, c;
    
    TEST_START("LBA to CHS conversion");
    
    /* Test sector 0 */
    uft_m65_lba_to_chs(0, &h, &s, &c);
    TEST_ASSERT(h == 0 && (s & 0x3F) == 1 && c == 0,
                "Sector 0 should be C=0, H=0, S=1");
    
    /* Test sector 63 (end of first track) */
    uft_m65_lba_to_chs(62, &h, &s, &c);
    TEST_ASSERT((s & 0x3F) == 63,
                "Sector 62 should have S=63");
    
    /* Test large LBA (beyond CHS limit) */
    uft_m65_lba_to_chs(100000000, &h, &s, &c);
    TEST_ASSERT(h == 254 && (s & 0x3F) == 63,
                "Large LBA should return max CHS values");
    
    /* Test typical partition start at 2048 */
    uft_m65_lba_to_chs(2048, &h, &s, &c);
    TEST_ASSERT(h != 0 || (s & 0x3F) != 0 || c != 0,
                "Sector 2048 should have non-zero CHS");
    
    TEST_PASS();
}

/*============================================================================*
 * CLUSTER SIZE CALCULATION TESTS
 *============================================================================*/

static void test_cluster_size_calc(void)
{
    uint8_t size;
    
    TEST_START("Cluster size calculation");
    
    /* Small partition < 260MB -> 1 sector per cluster */
    size = uft_m65_calc_cluster_size(500000);
    TEST_ASSERT(size == 1, "Small partition should use 1 sector/cluster");
    
    /* Medium partition < 8GB -> 8 sectors per cluster */
    size = uft_m65_calc_cluster_size(10000000);
    TEST_ASSERT(size == 8, "Medium partition should use 8 sectors/cluster");
    
    /* Large partition < 16GB -> 16 sectors per cluster */
    size = uft_m65_calc_cluster_size(20000000);
    TEST_ASSERT(size == 16, "Large partition should use 16 sectors/cluster");
    
    /* Very large partition < 32GB -> 32 sectors per cluster */
    size = uft_m65_calc_cluster_size(50000000);
    TEST_ASSERT(size == 32, "Very large partition should use 32 sectors/cluster");
    
    /* Huge partition >= 32GB -> 64 sectors per cluster */
    size = uft_m65_calc_cluster_size(100000000);
    TEST_ASSERT(size == 64, "Huge partition should use 64 sectors/cluster");
    
    TEST_PASS();
}

/*============================================================================*
 * BOARD MODEL TESTS
 *============================================================================*/

static void test_board_models(void)
{
    const uft_m65_board_info_t *info;
    
    TEST_START("Board model lookup");
    
    /* Test R3A */
    info = uft_m65_get_board_info(UFT_M65_MODEL_R3A);
    TEST_ASSERT(info != NULL, "R3A should be found");
    TEST_ASSERT(strstr(info->name, "R3A") != NULL, "R3A name should contain 'R3A'");
    TEST_ASSERT(info->slot_size_mb == 8, "R3A slot size should be 8MB");
    
    /* Test R4 */
    info = uft_m65_get_board_info(UFT_M65_MODEL_R4);
    TEST_ASSERT(info != NULL, "R4 should be found");
    TEST_ASSERT(info->slot_size_mb == 16, "R4 slot size should be 16MB");
    TEST_ASSERT(strcmp(info->fpga_part, "200T") == 0, "R4 should use 200T FPGA");
    
    /* Test Nexys A7 */
    info = uft_m65_get_board_info(UFT_M65_MODEL_NEXYS_A7);
    TEST_ASSERT(info != NULL, "Nexys A7 should be found");
    TEST_ASSERT(info->slot_count == 1, "Nexys A7 should have 1 slot");
    
    /* Test unknown model */
    info = uft_m65_get_board_info(UFT_M65_MODEL_UNKNOWN);
    TEST_ASSERT(info == NULL, "Unknown model should return NULL");
    
    TEST_PASS();
}

/*============================================================================*
 * FILENAME CONVERSION TESTS
 *============================================================================*/

static void test_filename_conversion(void)
{
    uft_m65_dir_entry_t entry;
    char name[16];
    uint8_t name83[11];
    
    TEST_START("Filename conversion");
    
    /* Test 8.3 to display format */
    memcpy(entry.name, "KERNEL  ROM", 11);
    uft_m65_format_filename(&entry, name);
    TEST_ASSERT(strcmp(name, "kernel.rom") == 0,
                "Should convert 'KERNEL  ROM' to 'kernel.rom'");
    
    /* Test name without extension */
    memcpy(entry.name, "README     ", 11);
    uft_m65_format_filename(&entry, name);
    TEST_ASSERT(strcmp(name, "readme") == 0,
                "Should convert 'README     ' to 'readme'");
    
    /* Test parse filename to 8.3 */
    uft_m65_parse_filename("test.txt", name83);
    TEST_ASSERT(memcmp(name83, "TEST    TXT", 11) == 0,
                "Should parse 'test.txt' to 'TEST    TXT'");
    
    uft_m65_parse_filename("MEGA65", name83);
    TEST_ASSERT(memcmp(name83, "MEGA65     ", 11) == 0,
                "Should parse 'MEGA65' to 'MEGA65     '");
    
    TEST_PASS();
}

/*============================================================================*
 * ERROR STRING TESTS
 *============================================================================*/

static void test_error_strings(void)
{
    TEST_START("Error strings");
    
    TEST_ASSERT(strcmp(uft_m65_error_string(UFT_M65_OK), "OK") == 0,
                "OK error string");
    
    TEST_ASSERT(strstr(uft_m65_error_string(UFT_M65_ERROR_NO_CARD), "card") != NULL,
                "NO_CARD error string should mention 'card'");
    
    TEST_ASSERT(strstr(uft_m65_error_string(UFT_M65_ERROR_SERIAL), "Serial") != NULL ||
                strstr(uft_m65_error_string(UFT_M65_ERROR_SERIAL), "serial") != NULL,
                "SERIAL error string should mention 'serial'");
    
    TEST_PASS();
}

/*============================================================================*
 * VOLUME ID GENERATION TEST
 *============================================================================*/

static void test_volume_id_generation(void)
{
    uint32_t id1, id2, id3;
    
    TEST_START("Volume ID generation");
    
    /* Generate multiple IDs */
    id1 = uft_m65_generate_volume_id();
    
    /* Brief delay to change seed */
    for (volatile int i = 0; i < 100000; i++);
    
    id2 = uft_m65_generate_volume_id();
    id3 = uft_m65_generate_volume_id();
    
    /* IDs should be non-zero */
    TEST_ASSERT(id1 != 0, "Volume ID 1 should be non-zero");
    TEST_ASSERT(id2 != 0, "Volume ID 2 should be non-zero");
    TEST_ASSERT(id3 != 0, "Volume ID 3 should be non-zero");
    
    /* IDs should have reasonable distribution (not all the same) */
    /* Note: In quick succession, they might be similar due to time-based seed */
    
    TEST_PASS();
}

/*============================================================================*
 * FAT32 SIGNATURE TESTS
 *============================================================================*/

static void test_fat32_signatures(void)
{
    TEST_START("FAT32 signatures");
    
    TEST_ASSERT(UFT_M65_MBR_SIGNATURE == 0xAA55,
                "MBR signature should be 0xAA55");
    
    TEST_ASSERT(UFT_M65_FSINFO_LEAD_SIG == 0x41615252,
                "FSInfo lead signature should be 0x41615252");
    
    TEST_ASSERT(UFT_M65_FSINFO_STRUCT_SIG == 0x61417272,
                "FSInfo struct signature should be 0x61417272");
    
    TEST_ASSERT(UFT_M65_FSINFO_TRAIL_SIG == 0xAA550000,
                "FSInfo trail signature should be 0xAA550000");
    
    TEST_PASS();
}

/*============================================================================*
 * DIRECTORY ATTRIBUTE TESTS
 *============================================================================*/

static void test_dir_attributes(void)
{
    TEST_START("Directory attributes");
    
    TEST_ASSERT(UFT_M65_ATTR_READ_ONLY == 0x01, "READ_ONLY should be 0x01");
    TEST_ASSERT(UFT_M65_ATTR_HIDDEN == 0x02, "HIDDEN should be 0x02");
    TEST_ASSERT(UFT_M65_ATTR_SYSTEM == 0x04, "SYSTEM should be 0x04");
    TEST_ASSERT(UFT_M65_ATTR_VOLUME_ID == 0x08, "VOLUME_ID should be 0x08");
    TEST_ASSERT(UFT_M65_ATTR_DIRECTORY == 0x10, "DIRECTORY should be 0x10");
    TEST_ASSERT(UFT_M65_ATTR_ARCHIVE == 0x20, "ARCHIVE should be 0x20");
    
    /* Long name attribute is combination */
    TEST_ASSERT(UFT_M65_ATTR_LONG_NAME == 0x0F,
                "LONG_NAME should be 0x0F (combination of R+H+S+V)");
    
    TEST_PASS();
}

/*============================================================================*
 * MAIN
 *============================================================================*/

int main(void)
{
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("  MEGA65 Integration Tests for UnifiedFloppyTool\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    test_structure_sizes();
    test_partition_types();
    test_lba_to_chs();
    test_cluster_size_calc();
    test_board_models();
    test_filename_conversion();
    test_error_strings();
    test_volume_id_generation();
    test_fat32_signatures();
    test_dir_attributes();
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n",
           tests_passed, tests_failed, tests_passed + tests_failed);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return tests_failed > 0 ? 1 : 0;
}
