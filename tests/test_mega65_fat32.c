/**
 * @file test_mega65_fat32.c
 * @brief Tests for MEGA65 D81 and FAT32/MBR support
 * 
 * Tests ported from MEGA65 fdisk project
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include headers */
#include "uft/formats/uft_fat32_mbr.h"

/*============================================================================*
 * TEST FRAMEWORK
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Testing: %s... ", #name); \
    test_##name(); \
    tests_run++; \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAILED\n    Expected %ld, got %ld at line %d\n", \
               (long)(b), (long)(a), __LINE__); \
        exit(1); \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("FAILED\n    Expected \"%s\", got \"%s\" at line %d\n", \
               (b), (a), __LINE__); \
        exit(1); \
    } \
} while(0)

#define ASSERT_TRUE(x) ASSERT_EQ(!!(x), 1)
#define ASSERT_FALSE(x) ASSERT_EQ(!!(x), 0)

/*============================================================================*
 * MOCK DISK I/O
 *============================================================================*/

static uint8_t mock_disk[1024 * 1024];  /* 1MB mock disk */
static uint32_t mock_disk_sectors = 2048;

static int mock_read(uint32_t sector, uint8_t *buffer, void *user_data)
{
    (void)user_data;
    if (sector >= mock_disk_sectors) return -1;
    memcpy(buffer, mock_disk + sector * 512, 512);
    return 0;
}

static int mock_write(uint32_t sector, const uint8_t *buffer, void *user_data)
{
    (void)user_data;
    if (sector >= mock_disk_sectors) return -1;
    memcpy(mock_disk + sector * 512, buffer, 512);
    return 0;
}

static void mock_disk_reset(void)
{
    memset(mock_disk, 0, sizeof(mock_disk));
}

/*============================================================================*
 * PARTITION TYPE NAME TESTS
 *============================================================================*/

TEST(partition_type_names)
{
    ASSERT_STR_EQ(uft_partition_type_name(0x00), "Empty");
    ASSERT_STR_EQ(uft_partition_type_name(0x0B), "FAT32 (CHS)");
    ASSERT_STR_EQ(uft_partition_type_name(0x0C), "FAT32 (LBA)");
    ASSERT_STR_EQ(uft_partition_type_name(0x41), "MEGA65 System");
    ASSERT_STR_EQ(uft_partition_type_name(0x83), "Linux");
    ASSERT_STR_EQ(uft_partition_type_name(0xFF), "Unknown");
}

/*============================================================================*
 * CHS/LBA CONVERSION TESTS
 *============================================================================*/

TEST(lba_to_chs_basic)
{
    uint8_t h, s, c;
    
    /* Sector 0 */
    uft_lba_to_chs(0, &h, &s, &c);
    ASSERT_EQ(h, 0);
    ASSERT_EQ(s & 0x3F, 1);  /* Sector 1 (1-based) */
    ASSERT_EQ(c, 0);
    
    /* Sector 63 (end of first track) */
    uft_lba_to_chs(62, &h, &s, &c);
    ASSERT_EQ(h, 0);
    ASSERT_EQ(s & 0x3F, 63);
    ASSERT_EQ(c, 0);
    
    /* Sector 63 (first sector of second head) */
    uft_lba_to_chs(63, &h, &s, &c);
    ASSERT_EQ(h, 1);
    ASSERT_EQ(s & 0x3F, 1);
    ASSERT_EQ(c, 0);
}

TEST(lba_to_chs_overflow)
{
    uint8_t h, s, c;
    
    /* Very large LBA (beyond 8GB CHS limit) */
    uft_lba_to_chs(0xFFFFFFFF, &h, &s, &c);
    
    /* Should return maximum CHS values */
    ASSERT_EQ(h, 254);
    ASSERT_EQ(s & 0x3F, 63);
}

/*============================================================================*
 * MBR TESTS
 *============================================================================*/

TEST(mbr_is_valid_empty)
{
    uft_disk_io_t io = { mock_read, mock_write, NULL, mock_disk_sectors };
    
    mock_disk_reset();
    
    /* Empty disk should not have valid MBR */
    ASSERT_FALSE(uft_mbr_is_valid(&io));
}

TEST(mbr_is_valid_with_signature)
{
    uft_disk_io_t io = { mock_read, mock_write, NULL, mock_disk_sectors };
    
    mock_disk_reset();
    
    /* Add MBR signature */
    mock_disk[510] = 0x55;
    mock_disk[511] = 0xAA;
    
    ASSERT_TRUE(uft_mbr_is_valid(&io));
}

TEST(mbr_create_default)
{
    uft_disk_io_t io = { mock_read, mock_write, NULL, mock_disk_sectors };
    uft_partition_info_t partitions[4];
    int count;
    
    mock_disk_reset();
    
    /* Create default MBR (no system partition) */
    ASSERT_EQ(uft_mbr_create_default(&io, 0), UFT_FAT32_OK);
    
    /* Verify MBR is valid */
    ASSERT_TRUE(uft_mbr_is_valid(&io));
    
    /* Read partitions back */
    ASSERT_EQ(uft_mbr_read_partitions(&io, partitions, &count), UFT_FAT32_OK);
    ASSERT_EQ(count, 1);
    
    /* Check partition type */
    ASSERT_EQ(partitions[0].type, UFT_PART_TYPE_FAT32_LBA);
    ASSERT_TRUE(partitions[0].bootable);
}

TEST(mbr_create_with_mega65_partition)
{
    uft_disk_io_t io = { mock_read, mock_write, NULL, mock_disk_sectors };
    uft_partition_info_t partitions[4];
    int count;
    
    mock_disk_reset();
    
    /* Create MBR with MEGA65 system partition (64 sectors = 32KB) */
    ASSERT_EQ(uft_mbr_create_default(&io, 64), UFT_FAT32_OK);
    
    /* Read partitions back */
    ASSERT_EQ(uft_mbr_read_partitions(&io, partitions, &count), UFT_FAT32_OK);
    ASSERT_EQ(count, 2);
    
    /* First partition should be MEGA65 system */
    ASSERT_EQ(partitions[0].type, UFT_PART_TYPE_MEGA65_SYS);
    ASSERT_FALSE(partitions[0].bootable);
    
    /* Second partition should be FAT32 */
    ASSERT_EQ(partitions[1].type, UFT_PART_TYPE_FAT32_LBA);
    ASSERT_TRUE(partitions[1].bootable);
}

/*============================================================================*
 * FAT32 TESTS
 *============================================================================*/

TEST(fat32_cluster_size_calculation)
{
    /* < 260MB: 1 sector/cluster */
    ASSERT_EQ(uft_fat32_calc_cluster_size(500000), 1);
    
    /* < 8GB: 8 sectors/cluster */
    ASSERT_EQ(uft_fat32_calc_cluster_size(1000000), 8);
    
    /* < 16GB: 16 sectors/cluster */
    ASSERT_EQ(uft_fat32_calc_cluster_size(20000000), 16);
    
    /* < 32GB: 32 sectors/cluster */
    ASSERT_EQ(uft_fat32_calc_cluster_size(50000000), 32);
    
    /* >= 32GB: 64 sectors/cluster */
    ASSERT_EQ(uft_fat32_calc_cluster_size(100000000), 64);
}

TEST(fat32_volume_id_generation)
{
    uint32_t id1 = uft_fat32_generate_volume_id();
    uint32_t id2 = uft_fat32_generate_volume_id();
    
    /* IDs should be non-zero */
    ASSERT_TRUE(id1 != 0);
    ASSERT_TRUE(id2 != 0);
    
    /* Note: IDs might be the same if called very quickly,
     * but they should generally differ */
}

/*============================================================================*
 * SIZE FORMATTING TESTS
 *============================================================================*/

TEST(format_size_string)
{
    char buffer[32];
    
    /* Bytes */
    uft_format_size_string(1, buffer, sizeof(buffer));
    ASSERT_STR_EQ(buffer, "512 B");
    
    /* Kilobytes */
    uft_format_size_string(4, buffer, sizeof(buffer));
    ASSERT_STR_EQ(buffer, "2.00 KB");
    
    /* Megabytes */
    uft_format_size_string(4096, buffer, sizeof(buffer));
    ASSERT_STR_EQ(buffer, "2.00 MB");
    
    /* Gigabytes */
    uft_format_size_string(4194304, buffer, sizeof(buffer));
    ASSERT_STR_EQ(buffer, "2.00 GB");
}

/*============================================================================*
 * D81 TESTS (from MEGA65 integration)
 *============================================================================*/

/* External declarations for D81 functions */
extern void uft_petscii_to_ascii(const uint8_t *petscii, char *ascii, size_t len);
extern void uft_ascii_to_petscii(const char *ascii, uint8_t *petscii, size_t len);
extern const char *uft_d81_file_type_str(uint8_t type);
extern uint32_t uft_d81_sector_offset(uint8_t track, uint8_t sector);
extern int uft_d81_probe(const uint8_t *data, size_t size);

TEST(petscii_conversion)
{
    uint8_t petscii[16];
    char ascii[17] = {0};
    
    /* ASCII to PETSCII */
    uft_ascii_to_petscii("HELLO", petscii, 16);
    ASSERT_EQ(petscii[0], 0xC8);  /* H shifted */
    
    /* PETSCII to ASCII */
    uint8_t test_petscii[] = { 0x48, 0x45, 0x4C, 0x4C, 0x4F, 0xA0 };
    uft_petscii_to_ascii(test_petscii, ascii, 16);
    ASSERT_STR_EQ(ascii, "hello");
}

TEST(d81_file_types)
{
    ASSERT_STR_EQ(uft_d81_file_type_str(0x00), "DEL");
    ASSERT_STR_EQ(uft_d81_file_type_str(0x01), "SEQ");
    ASSERT_STR_EQ(uft_d81_file_type_str(0x02), "PRG");
    ASSERT_STR_EQ(uft_d81_file_type_str(0x03), "USR");
    ASSERT_STR_EQ(uft_d81_file_type_str(0x04), "REL");
    ASSERT_STR_EQ(uft_d81_file_type_str(0x82), "PRG");  /* With closed flag */
}

TEST(d81_sector_offset)
{
    /* Track 1, Sector 0 */
    ASSERT_EQ(uft_d81_sector_offset(1, 0), 0);
    
    /* Track 1, Sector 1 */
    ASSERT_EQ(uft_d81_sector_offset(1, 1), 256);
    
    /* Track 2, Sector 0 */
    ASSERT_EQ(uft_d81_sector_offset(2, 0), 40 * 256);
    
    /* Track 40 (directory), Sector 0 */
    ASSERT_EQ(uft_d81_sector_offset(40, 0), 39 * 40 * 256);
    
    /* Invalid track */
    ASSERT_EQ(uft_d81_sector_offset(0, 0), 0xFFFFFFFF);
    ASSERT_EQ(uft_d81_sector_offset(81, 0), 0xFFFFFFFF);
    
    /* Invalid sector */
    ASSERT_EQ(uft_d81_sector_offset(1, 40), 0xFFFFFFFF);
}

TEST(d81_probe_size)
{
    uint8_t data[819200];
    
    /* Wrong size should fail */
    ASSERT_FALSE(uft_d81_probe(data, 819199));
    ASSERT_FALSE(uft_d81_probe(data, 819201));
    ASSERT_FALSE(uft_d81_probe(data, 0));
}

/*============================================================================*
 * MAIN
 *============================================================================*/

int main(void)
{
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("  MEGA65 / FAT32 / MBR Integration Tests\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    printf("Partition Type Names:\n");
    RUN_TEST(partition_type_names);
    
    printf("\nCHS/LBA Conversion:\n");
    RUN_TEST(lba_to_chs_basic);
    RUN_TEST(lba_to_chs_overflow);
    
    printf("\nMBR Tests:\n");
    RUN_TEST(mbr_is_valid_empty);
    RUN_TEST(mbr_is_valid_with_signature);
    RUN_TEST(mbr_create_default);
    RUN_TEST(mbr_create_with_mega65_partition);
    
    printf("\nFAT32 Tests:\n");
    RUN_TEST(fat32_cluster_size_calculation);
    RUN_TEST(fat32_volume_id_generation);
    
    printf("\nSize Formatting:\n");
    RUN_TEST(format_size_string);
    
    printf("\nD81 Format Tests:\n");
    RUN_TEST(petscii_conversion);
    RUN_TEST(d81_file_types);
    RUN_TEST(d81_sector_offset);
    RUN_TEST(d81_probe_size);
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n", 
           tests_passed, tests_run - tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
