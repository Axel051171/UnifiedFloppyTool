/**
 * @file test_cbm_disk.c
 * @brief Unit tests for CBM disk format handler
 */

#include "uft/c64/uft_cbm_disk.h"
#include "uft/c64/uft_fastloader_db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TEST(name) static int test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %-40s ", #name); \
    if (test_##name() == 0) { printf("[PASS]\n"); passed++; } \
    else { printf("[FAIL]\n"); failed++; } \
    total++; \
} while(0)

/* ============================================================================
 * Minimal D64 Test Image
 * ============================================================================ */

/* 
 * Minimal valid D64 with:
 * - BAM at track 18, sector 0
 * - Directory at track 18, sector 1
 * - One PRG file "TEST" 
 */
static uint8_t *create_test_d64(void)
{
    uint8_t *img = calloc(174848, 1);
    if (!img) return NULL;
    
    /* Calculate sector offsets */
    /* Track 18, Sector 0 = offset 91392 (357 * 256) */
    uint8_t *bam = img + 91392;
    /* Track 18, Sector 1 = offset 91648 (358 * 256) */
    uint8_t *dir = img + 91648;
    /* Track 1, Sector 0 = offset 0 - for file data */
    uint8_t *data = img;
    
    /* BAM sector */
    bam[0] = 18;  /* Dir track */
    bam[1] = 1;   /* Dir sector */
    bam[2] = 0x41; /* DOS version (A) */
    bam[3] = 0x00;
    
    /* BAM entries for tracks 1-35 */
    for (int t = 1; t <= 35; t++) {
        int off = 4 + (t - 1) * 4;
        uint8_t sectors;
        if (t <= 17) sectors = 21;
        else if (t <= 24) sectors = 19;
        else if (t <= 30) sectors = 18;
        else sectors = 17;
        
        bam[off] = sectors;  /* Free sectors */
        bam[off + 1] = 0xFF; /* Bitmap */
        bam[off + 2] = 0xFF;
        bam[off + 3] = (sectors > 16) ? ((1 << (sectors - 16)) - 1) : 0;
    }
    
    /* Track 18 - reserve sector 0 and 1 */
    bam[4 + 17 * 4] = 17;  /* 19 - 2 = 17 free */
    bam[4 + 17 * 4 + 1] = 0xFC;  /* Sectors 0,1 allocated */
    
    /* Track 1 - allocate sector 0 for file */
    bam[4 + 0 * 4] = 20;  /* 21 - 1 = 20 free */
    bam[4 + 0 * 4 + 1] = 0xFE;  /* Sector 0 allocated */
    
    /* Disk name "TEST DISK" at 0x90 */
    memcpy(bam + 0x90, "TEST DISK\xA0\xA0\xA0\xA0\xA0\xA0\xA0", 16);
    /* Disk ID at 0xA2 */
    bam[0xA2] = '1';
    bam[0xA3] = 'A';
    bam[0xA4] = 0xA0;
    bam[0xA5] = '2';  /* DOS type */
    bam[0xA6] = 'A';
    
    /* Directory sector */
    dir[0] = 0;   /* No next sector */
    dir[1] = 0xFF;
    
    /* First directory entry at offset 2 */
    dir[2] = 0x82;  /* PRG, closed */
    dir[3] = 1;     /* Start track */
    dir[4] = 0;     /* Start sector */
    /* Filename "TEST" padded with 0xA0 */
    memcpy(dir + 5, "TEST\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0", 16);
    dir[30] = 1;    /* Size: 1 block */
    dir[31] = 0;
    
    /* File data - simple PRG: 10 SYS2061 
     * Structure (payload must be 13 bytes for end marker check):
     * $0801: 0C 08 = next line pointer ($080C)
     * $0803: 0A 00 = line number 10
     * $0805: 9E = SYS token
     * $0806: "2061"
     * $080A: 00 = end of line
     * $080B: 00 00 00 = end of program (extra byte for bounds check)
     */
    data[0] = 0;   /* Last sector marker */
    data[1] = 16;  /* 15 bytes used (positions 2-16) */
    /* PRG data starts at offset 2 */
    data[2] = 0x01; /* Load address lo */
    data[3] = 0x08; /* Load address hi = $0801 */
    data[4] = 0x0C; /* Next line pointer lo */
    data[5] = 0x08; /* Next line pointer hi = $080C */
    data[6] = 0x0A; /* Line number 10 lo */
    data[7] = 0x00; /* Line number 10 hi */
    data[8] = 0x9E; /* SYS token */
    data[9] = '2';
    data[10] = '0';
    data[11] = '6';
    data[12] = '1';
    data[13] = 0x00; /* End of line */
    data[14] = 0x00; /* End of program marker 1 */
    data[15] = 0x00; /* End of program marker 2 */
    data[16] = 0x00; /* Extra padding for bounds check */
    
    return img;
}

/* ============================================================================
 * Test Cases
 * ============================================================================ */

TEST(format_detection)
{
    if (uft_cbm_detect_format(174848) != UFT_CBM_DISK_D64) return 1;
    if (uft_cbm_detect_format(175531) != UFT_CBM_DISK_D64) return 2;
    if (uft_cbm_detect_format(196608) != UFT_CBM_DISK_D64_40) return 3;
    if (uft_cbm_detect_format(349696) != UFT_CBM_DISK_D71) return 4;
    if (uft_cbm_detect_format(819200) != UFT_CBM_DISK_D81) return 5;
    if (uft_cbm_detect_format(12345) != UFT_CBM_DISK_UNKNOWN) return 6;
    return 0;
}

TEST(format_names)
{
    if (strstr(uft_cbm_format_name(UFT_CBM_DISK_D64), "D64") == NULL) return 1;
    if (strstr(uft_cbm_format_name(UFT_CBM_DISK_D71), "D71") == NULL) return 2;
    if (strstr(uft_cbm_format_name(UFT_CBM_DISK_D81), "D81") == NULL) return 3;
    return 0;
}

TEST(sectors_per_track_d64)
{
    /* Zone 3: tracks 1-17 = 21 sectors */
    if (uft_cbm_sectors_per_track(UFT_CBM_DISK_D64, 1) != 21) return 1;
    if (uft_cbm_sectors_per_track(UFT_CBM_DISK_D64, 17) != 21) return 2;
    
    /* Zone 2: tracks 18-24 = 19 sectors */
    if (uft_cbm_sectors_per_track(UFT_CBM_DISK_D64, 18) != 19) return 3;
    if (uft_cbm_sectors_per_track(UFT_CBM_DISK_D64, 24) != 19) return 4;
    
    /* Zone 1: tracks 25-30 = 18 sectors */
    if (uft_cbm_sectors_per_track(UFT_CBM_DISK_D64, 25) != 18) return 5;
    
    /* Zone 0: tracks 31-35 = 17 sectors */
    if (uft_cbm_sectors_per_track(UFT_CBM_DISK_D64, 35) != 17) return 6;
    
    return 0;
}

TEST(sector_offset)
{
    uint32_t offset;
    
    /* Track 1, Sector 0 = offset 0 */
    if (uft_cbm_sector_offset(UFT_CBM_DISK_D64, 1, 0, &offset) != 0) return 1;
    if (offset != 0) return 2;
    
    /* Track 18, Sector 0 (BAM) = offset 91392 */
    if (uft_cbm_sector_offset(UFT_CBM_DISK_D64, 18, 0, &offset) != 0) return 3;
    if (offset != 91392) return 4;
    
    /* Invalid track */
    if (uft_cbm_sector_offset(UFT_CBM_DISK_D64, 50, 0, &offset) == 0) return 5;
    
    /* Invalid sector */
    if (uft_cbm_sector_offset(UFT_CBM_DISK_D64, 1, 25, &offset) == 0) return 6;
    
    return 0;
}

TEST(load_test_d64)
{
    uint8_t *img = create_test_d64();
    if (!img) return 1;
    
    uft_cbm_disk_t disk;
    int ret = uft_cbm_disk_load(img, 174848, &disk);
    
    if (ret != 0) { free(img); return 2; }
    if (disk.format != UFT_CBM_DISK_D64) { free(img); return 3; }
    if (disk.tracks != 35) { free(img); return 4; }
    
    uft_cbm_disk_free(&disk);
    free(img);
    return 0;
}

TEST(read_bam)
{
    uint8_t *img = create_test_d64();
    if (!img) return 1;
    
    uft_cbm_disk_t disk;
    uft_cbm_disk_load(img, 174848, &disk);
    
    /* Check disk name */
    if (strstr(disk.bam.disk_name, "test") == NULL &&
        strstr(disk.bam.disk_name, "TEST") == NULL) {
        uft_cbm_disk_free(&disk);
        free(img);
        return 2;
    }
    
    /* Check disk ID */
    if (disk.bam.disk_id[0] != '1' || disk.bam.disk_id[1] != 'A') {
        uft_cbm_disk_free(&disk);
        free(img);
        return 3;
    }
    
    uft_cbm_disk_free(&disk);
    free(img);
    return 0;
}

TEST(read_directory)
{
    uint8_t *img = create_test_d64();
    if (!img) return 1;
    
    uft_cbm_disk_t disk;
    uft_cbm_disk_load(img, 174848, &disk);
    
    if (disk.dir_count != 1) {
        uft_cbm_disk_free(&disk);
        free(img);
        return 2;
    }
    
    const uft_cbm_dir_entry_t *e = uft_cbm_get_entry(&disk, 0);
    if (!e) {
        uft_cbm_disk_free(&disk);
        free(img);
        return 3;
    }
    
    if (strstr(e->filename, "test") == NULL &&
        strstr(e->filename, "TEST") == NULL) {
        uft_cbm_disk_free(&disk);
        free(img);
        return 4;
    }
    
    if (e->type != UFT_CBM_FILE_PRG) {
        uft_cbm_disk_free(&disk);
        free(img);
        return 5;
    }
    
    uft_cbm_disk_free(&disk);
    free(img);
    return 0;
}

TEST(extract_file)
{
    uint8_t *img = create_test_d64();
    if (!img) return 1;
    
    uft_cbm_disk_t disk;
    uft_cbm_disk_load(img, 174848, &disk);
    
    uft_cbm_file_t file;
    int ret = uft_cbm_extract_file(&disk, &disk.directory[0], &file);
    
    if (ret != 0) {
        uft_cbm_disk_free(&disk);
        free(img);
        return 2;
    }
    
    /* Check PRG data */
    if (file.data_size < 10) {
        uft_cbm_file_free(&file);
        uft_cbm_disk_free(&disk);
        free(img);
        return 3;
    }
    
    /* Check load address */
    if (file.prg_info.view.load_addr != 0x0801) {
        uft_cbm_file_free(&file);
        uft_cbm_disk_free(&disk);
        free(img);
        return 4;
    }
    
    /* Check SYS detection */
    if (!file.prg_info.has_sys_call || file.prg_info.sys_address != 2061) {
        uft_cbm_file_free(&file);
        uft_cbm_disk_free(&disk);
        free(img);
        return 5;
    }
    
    uft_cbm_file_free(&file);
    uft_cbm_disk_free(&disk);
    free(img);
    return 0;
}

TEST(format_directory)
{
    uint8_t *img = create_test_d64();
    if (!img) return 1;
    
    uft_cbm_disk_t disk;
    uft_cbm_disk_load(img, 174848, &disk);
    
    char buf[1024];
    size_t len = uft_cbm_format_directory(&disk, buf, sizeof(buf));
    
    if (len == 0) {
        uft_cbm_disk_free(&disk);
        free(img);
        return 2;
    }
    
    /* Should contain disk name */
    if (strstr(buf, "TEST") == NULL && strstr(buf, "test") == NULL) {
        uft_cbm_disk_free(&disk);
        free(img);
        return 3;
    }
    
    /* Should contain "PRG" */
    if (strstr(buf, "PRG") == NULL) {
        uft_cbm_disk_free(&disk);
        free(img);
        return 4;
    }
    
    /* Should contain "BLOCKS FREE" */
    if (strstr(buf, "BLOCKS FREE") == NULL) {
        uft_cbm_disk_free(&disk);
        free(img);
        return 5;
    }
    
    uft_cbm_disk_free(&disk);
    free(img);
    return 0;
}

TEST(petscii_conversion)
{
    char ascii[32];
    
    /* Normal text */
    uint8_t pet1[] = { 0x54, 0x45, 0x53, 0x54, 0xA0 };  /* "TEST" + padding */
    uft_cbm_petscii_to_ascii(pet1, 5, ascii, sizeof(ascii));
    if (strcmp(ascii, "test") != 0) return 1;
    
    /* Shifted uppercase */
    uint8_t pet2[] = { 0xC8, 0xC5, 0xCC, 0xCC, 0xCF };  /* "HELLO" shifted */
    uft_cbm_petscii_to_ascii(pet2, 5, ascii, sizeof(ascii));
    if (strcmp(ascii, "HELLO") != 0) return 2;
    
    return 0;
}

/* ============================================================================
 * Fastloader Database Tests
 * ============================================================================ */

TEST(sig_db_size)
{
    size_t size = uft_sig_db_size();
    if (size < 10) return 1;  /* Should have at least 10 signatures */
    return 0;
}

TEST(sig_db_get)
{
    const uft_sig_entry_t *e = uft_sig_db_get(0);
    if (!e) return 1;
    if (e->name[0] == '\0') return 2;
    
    /* Out of bounds should return NULL */
    if (uft_sig_db_get(10000) != NULL) return 3;
    
    return 0;
}

TEST(sig_find_name)
{
    const uft_sig_entry_t *e = uft_sig_db_find_name("Turbo Nibbler");
    if (!e) return 1;
    if (e->category != UFT_SIG_CAT_NIBBLER) return 2;
    return 0;
}

TEST(sig_category_filter)
{
    const uft_sig_entry_t *entries[32];
    size_t count = uft_sig_db_find_category(UFT_SIG_CAT_NIBBLER, entries, 32);
    
    if (count < 2) return 1;  /* Should have multiple nibblers */
    
    for (size_t i = 0; i < count; i++) {
        if (entries[i]->category != UFT_SIG_CAT_NIBBLER) return 2;
    }
    
    return 0;
}

TEST(sig_scan_nibbler)
{
    /* Test data containing "TURBO NIBBLER V1" */
    uint8_t data[] = "SOME PREFIX TURBO NIBBLER V1.0 SOME SUFFIX";
    
    uft_sig_result_t result;
    int count = uft_sig_scan(data, sizeof(data) - 1, 0x0801, &result);
    
    if (count == 0) return 1;
    if (result.matches[0].entry->category != UFT_SIG_CAT_NIBBLER) return 2;
    
    return 0;
}

TEST(sig_category_names)
{
    if (strstr(uft_sig_category_name(UFT_SIG_CAT_FASTLOADER), "Fastloader") == NULL) return 1;
    if (strstr(uft_sig_category_name(UFT_SIG_CAT_NIBBLER), "Nibbler") == NULL) return 2;
    if (strstr(uft_sig_category_name(UFT_SIG_CAT_PROTECTION), "Protection") == NULL) return 3;
    return 0;
}

TEST(file_type_names)
{
    if (strcmp(uft_cbm_file_type_name(UFT_CBM_FILE_PRG), "PRG") != 0) return 1;
    if (strcmp(uft_cbm_file_type_name(UFT_CBM_FILE_SEQ), "SEQ") != 0) return 2;
    if (strcmp(uft_cbm_file_type_name(UFT_CBM_FILE_DEL), "DEL") != 0) return 3;
    return 0;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    int passed = 0, failed = 0, total = 0;
    
    printf("\n=== CBM Disk Format Tests ===\n\n");
    
    RUN_TEST(format_detection);
    RUN_TEST(format_names);
    RUN_TEST(sectors_per_track_d64);
    RUN_TEST(sector_offset);
    RUN_TEST(load_test_d64);
    RUN_TEST(read_bam);
    RUN_TEST(read_directory);
    RUN_TEST(extract_file);
    RUN_TEST(format_directory);
    RUN_TEST(petscii_conversion);
    
    printf("\n=== Fastloader Database Tests ===\n\n");
    
    RUN_TEST(sig_db_size);
    RUN_TEST(sig_db_get);
    RUN_TEST(sig_find_name);
    RUN_TEST(sig_category_filter);
    RUN_TEST(sig_scan_nibbler);
    RUN_TEST(sig_category_names);
    RUN_TEST(file_type_names);
    
    printf("\n-----------------------------------------\n");
    printf("Results: %d/%d passed", passed, total);
    if (failed > 0) printf(", %d FAILED", failed);
    printf("\n\n");
    
    return failed > 0 ? 1 : 0;
}
