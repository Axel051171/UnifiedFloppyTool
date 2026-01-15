/**
 * @file test_fat_extensions.c
 * @brief Test program for FAT32, Boot Templates, Bad Blocks, Atari modes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Include new headers */
#include "uft/fs/uft_fat32.h"
#include "uft/fs/uft_fat_boot.h"
#include "uft/fs/uft_fat_badblock.h"
#include "uft/fs/uft_fat_atari.h"

#define TEST_PASS "\033[32mPASS\033[0m"
#define TEST_FAIL "\033[31mFAIL\033[0m"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    if (cond) { \
        printf("  [%s] %s\n", TEST_PASS, msg); \
        tests_passed++; \
    } else { \
        printf("  [%s] %s\n", TEST_FAIL, msg); \
        tests_failed++; \
    } \
} while(0)

/*===========================================================================
 * Test 1: FAT32 Format and Detection
 *===========================================================================*/
void test_fat32(void)
{
    printf("\n=== Test FAT32 ===\n");
    
    /* Create 64MB FAT32 image */
    size_t size = 64 * 1024 * 1024;
    uint8_t *image = calloc(1, size);
    ASSERT(image != NULL, "Allocate 64MB image");
    
    if (!image) return;
    
    /* Initialize format options */
    uft_fat32_format_opts_t opts;
    uft_fat32_format_opts_init(&opts);
    opts.volume_size = size;
    strncpy(opts.volume_label, "UFT_TEST   ", 11);
    
    /* Format as FAT32 */
    int rc = uft_fat32_format(image, size, &opts);
    ASSERT(rc == 0, "Format image as FAT32");
    
    /* Detect FAT32 */
    bool detected = uft_fat32_detect(image, size);
    ASSERT(detected, "Detect FAT32 filesystem");
    
    /* Validate boot sector */
    const uft_fat32_bootsect_t *boot = uft_fat32_get_boot(image);
    ASSERT(boot != NULL, "Get boot sector");
    
    if (boot) {
        ASSERT(boot->bytes_per_sector == 512, "Sector size = 512");
        ASSERT(boot->root_cluster == 2, "Root cluster = 2");
        ASSERT(memcmp(boot->fs_type, "FAT32   ", 8) == 0, "FS type = FAT32");
    }
    
    /* Test FSInfo */
    uft_fat32_fsinfo_t fsinfo;
    rc = uft_fat32_read_fsinfo(image, boot, &fsinfo);
    ASSERT(rc == 0, "Read FSInfo sector");
    ASSERT(fsinfo.lead_sig == UFT_FAT32_FSINFO_SIG1, "FSInfo signature valid");
    
    /* Test cluster operations */
    uint32_t clusters = uft_fat32_count_clusters(boot);
    ASSERT(clusters > 65525, "FAT32 has >65525 clusters");
    printf("    Total clusters: %u\n", clusters);
    
    /* Type detection */
    uft_fat_type_t type = uft_fat_type_for_size(size);
    ASSERT(type == UFT_FAT_TYPE_FAT32, "Size 64MB -> FAT32");
    
    free(image);
    printf("  FAT32 tests complete\n");
}

/*===========================================================================
 * Test 2: Boot Templates
 *===========================================================================*/
void test_boot_templates(void)
{
    printf("\n=== Test Boot Templates ===\n");
    
    /* List available templates */
    size_t count;
    const uft_boot_info_t *templates = uft_boot_list_templates(&count);
    ASSERT(count >= 5, "At least 5 boot templates available");
    printf("    Found %zu boot templates\n", count);
    
    for (size_t i = 0; i < count; i++) {
        printf("    - %s: %s\n", templates[i].name, templates[i].description);
    }
    
    /* Find template by name */
    uft_boot_template_t type = uft_boot_find_by_name("freedos");
    ASSERT(type == UFT_BOOT_FREEDOS, "Find 'freedos' template");
    
    type = uft_boot_find_by_name("not-bootable");
    ASSERT(type == UFT_BOOT_NOT_BOOTABLE, "Find 'not-bootable' template");
    
    /* Create test boot sector */
    uint8_t boot[512];
    memset(boot, 0, sizeof(boot));
    boot[510] = 0x55;
    boot[511] = 0xAA;
    
    /* Apply template */
    int rc = uft_boot_apply_template(boot, UFT_BOOT_NOT_BOOTABLE, UFT_FAT_TYPE_FAT12);
    ASSERT(rc == 0, "Apply NOT_BOOTABLE template");
    ASSERT(boot[0] == 0xEB, "Jump instruction set");
    
    /* Check if bootable */
    bool bootable = uft_boot_is_bootable(boot, UFT_FAT_TYPE_FAT12);
    ASSERT(bootable, "Boot sector is bootable");
    
    /* Set OEM name */
    rc = uft_boot_set_oem(boot, "TESTBOOT");
    ASSERT(rc == 0, "Set OEM name");
    
    char oem[9];
    uft_boot_get_oem(boot, oem);
    ASSERT(strcmp(oem, "TESTBOOT") == 0, "OEM name matches");
    
    /* Get required files */
    const char *files = uft_boot_required_files(UFT_BOOT_FREEDOS);
    ASSERT(files != NULL, "FreeDOS has required files");
    if (files) {
        printf("    FreeDOS requires: %s\n", files);
    }
    
    printf("  Boot template tests complete\n");
}

/*===========================================================================
 * Test 3: Bad Block Import
 *===========================================================================*/
void test_bad_blocks(void)
{
    printf("\n=== Test Bad Block Import ===\n");
    
    /* Create list */
    uft_badblock_list_t *list = uft_badblock_list_create();
    ASSERT(list != NULL, "Create bad block list");
    
    if (!list) return;
    
    /* Add entries */
    int rc = uft_badblock_list_add(list, 100, UFT_BADBLOCK_SECTOR);
    ASSERT(rc == 0, "Add sector 100");
    
    rc = uft_badblock_list_add(list, 200, UFT_BADBLOCK_SECTOR);
    ASSERT(rc == 0, "Add sector 200");
    
    rc = uft_badblock_list_add(list, 50, UFT_BADBLOCK_CLUSTER);
    ASSERT(rc == 0, "Add cluster 50");
    
    ASSERT(list->count == 3, "List has 3 entries");
    
    /* Sort */
    uft_badblock_list_sort(list);
    ASSERT(list->entries[0].location == 50, "First entry is cluster 50");
    
    /* Add duplicate and dedupe */
    uft_badblock_list_add(list, 100, UFT_BADBLOCK_SECTOR);
    ASSERT(list->count == 4, "List has 4 entries before dedupe");
    
    size_t removed = uft_badblock_list_dedupe(list);
    ASSERT(removed == 1, "One duplicate removed");
    ASSERT(list->count == 3, "List has 3 entries after dedupe");
    
    /* Test buffer import */
    const char *test_data = 
        "# Bad block list\n"
        "500\n"
        "600\n"
        "0x2BC\n"  /* 700 in hex */
        "# Comment line\n"
        "800\n";
    
    uft_badblock_list_clear(list);
    rc = uft_badblock_import_buffer(list, test_data, strlen(test_data), 
                                    UFT_BADBLOCK_SECTOR);
    ASSERT(rc == 4, "Imported 4 entries from buffer");
    ASSERT(list->count == 4, "List has 4 entries");
    
    /* Export to buffer (via file simulation) */
    printf("    Imported sectors:");
    for (size_t i = 0; i < list->count; i++) {
        printf(" %llu", (unsigned long long)list->entries[i].location);
    }
    printf("\n");
    
    /* Test unit strings */
    ASSERT(strcmp(uft_badblock_unit_str(UFT_BADBLOCK_SECTOR), "sector") == 0, 
           "Unit string 'sector'");
    ASSERT(strcmp(uft_badblock_unit_str(UFT_BADBLOCK_BLOCK_1K), "1KB-block") == 0,
           "Unit string '1KB-block'");
    
    uft_badblock_list_destroy(list);
    printf("  Bad block tests complete\n");
}

/*===========================================================================
 * Test 4: Atari ST FAT Mode
 *===========================================================================*/
void test_atari_fat(void)
{
    printf("\n=== Test Atari ST FAT ===\n");
    
    /* List standard formats */
    printf("    Standard Atari formats:\n");
    for (size_t i = 0; i < uft_atari_std_format_count; i++) {
        const uft_atari_geometry_t *g = &uft_atari_std_formats[i];
        printf("      - %s: %d sectors, %dx%dx%d\n", 
               g->name, g->sectors, g->tracks, g->sides, g->spt);
    }
    
    /* Test serial number generation */
    uint32_t serial1 = uft_atari_generate_serial();
    uint32_t serial2 = uft_atari_generate_serial();
    ASSERT(serial1 != 0, "Serial number generated");
    ASSERT(serial1 <= 0xFFFFFF, "Serial is 24-bit");
    printf("    Generated serials: 0x%06X, 0x%06X\n", serial1, serial2);
    
    /* Get geometry by format */
    const uft_atari_geometry_t *geom = uft_atari_get_geometry(UFT_ATARI_FMT_DS_DD_9);
    ASSERT(geom != NULL, "Get DS/DD 9 geometry");
    if (geom) {
        ASSERT(geom->sectors == 1440, "DS/DD 9 = 1440 sectors");
        ASSERT(geom->spc == 2, "Atari uses 2 sectors/cluster");
    }
    
    /* Get geometry by size */
    geom = uft_atari_geometry_from_size(720 * 512);
    ASSERT(geom != NULL, "Geometry from 360KB size");
    if (geom) {
        ASSERT(geom->type == UFT_ATARI_FMT_SS_DD_9, "360KB = SS/DD 9");
    }
    
    /* Create and format Atari disk */
    size_t disk_size = 1440 * 512;  /* DS/DD 720KB */
    uint8_t *disk = calloc(1, disk_size);
    ASSERT(disk != NULL, "Allocate 720KB disk");
    
    if (disk) {
        int rc = uft_atari_format(disk, disk_size, UFT_ATARI_FMT_DS_DD_9, "ATARITEST");
        ASSERT(rc == 0, "Format as Atari DS/DD 9");
        
        /* Check boot sector */
        uft_atari_bootsect_t *boot = (uft_atari_bootsect_t *)disk;
        
        ASSERT(boot->sectors_per_cluster == 2, "Atari SPC = 2");
        ASSERT(boot->total_sectors == 1440, "Total sectors = 1440");
        
        /* Test serial number in boot sector */
        uint32_t disk_serial = uft_atari_get_serial(boot);
        ASSERT(disk_serial != 0, "Disk has serial number");
        printf("    Disk serial: 0x%06X\n", disk_serial);
        
        /* Test bootable checksum */
        ASSERT(!uft_atari_is_bootable(boot), "Disk is NOT bootable initially");
        
        uft_atari_make_bootable(boot);
        ASSERT(uft_atari_is_bootable(boot), "Disk IS bootable after make_bootable");
        
        uft_atari_make_non_bootable(boot);
        ASSERT(!uft_atari_is_bootable(boot), "Disk NOT bootable after make_non_bootable");
        
        /* Test detection */
        bool detected = uft_atari_detect(disk, disk_size);
        ASSERT(detected, "Detect as Atari format");
        
        uft_atari_format_t fmt = uft_atari_identify_format(disk, disk_size);
        ASSERT(fmt == UFT_ATARI_FMT_DS_DD_9, "Identify as DS/DD 9");
        
        free(disk);
    }
    
    /* Test logical sector size calculation */
    uint16_t sec_size = uft_atari_calc_sector_size(32ULL * 1024 * 1024);
    ASSERT(sec_size == 512, "32MB -> 512 byte sectors");
    
    sec_size = uft_atari_calc_sector_size(64ULL * 1024 * 1024);
    ASSERT(sec_size == 1024, "64MB -> 1024 byte sectors");
    
    printf("  Atari ST FAT tests complete\n");
}

/*===========================================================================
 * Main
 *===========================================================================*/
int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║     UFT FAT Extensions Test Suite                             ║\n");
    printf("║     FAT32, Boot Templates, Bad Blocks, Atari ST               ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    test_fat32();
    test_boot_templates();
    test_bad_blocks();
    test_atari_fat();
    
    printf("\n════════════════════════════════════════════════════════════════\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("════════════════════════════════════════════════════════════════\n");
    
    return tests_failed > 0 ? 1 : 0;
}
