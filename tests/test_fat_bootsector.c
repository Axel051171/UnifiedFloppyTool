/**
 * @file test_fat_bootsector.c
 * @brief Unit tests for FAT Boot Sector Analysis Module
 * 
 * Based on OpenGate.at article specifications
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "uft/formats/fat/uft_fat_bootsector.h"

/* ============================================================================
 * Test Helper Macros
 * ============================================================================ */

#define TEST_PASS() printf("  ✓ %s passed\n", __func__)
#define TEST_FAIL(msg) do { printf("  ✗ %s failed: %s\n", __func__, msg); return 1; } while(0)
#define ASSERT_TRUE(cond, msg) if (!(cond)) TEST_FAIL(msg)
#define ASSERT_EQ(a, b, msg) if ((a) != (b)) TEST_FAIL(msg)
#define ASSERT_STREQ(a, b, msg) if (strcmp((a), (b)) != 0) TEST_FAIL(msg)

/* ============================================================================
 * Test Boot Sectors (based on OpenGate article)
 * ============================================================================ */

/* Create a standard 1.44MB boot sector */
static void create_144mb_boot_sector(uint8_t *buffer) {
    memset(buffer, 0, 512);
    
    /* Jump instruction: EB 3C 90 */
    buffer[0] = 0xEB;
    buffer[1] = 0x3C;
    buffer[2] = 0x90;
    
    /* OEM Name: MSDOS5.0 */
    memcpy(buffer + 3, "MSDOS5.0", 8);
    
    /* BPB for 1.44MB disk (from OpenGate article) */
    buffer[0x0B] = 0x00; buffer[0x0C] = 0x02;  /* Bytes per sector: 512 */
    buffer[0x0D] = 0x01;                        /* Sectors per cluster: 1 */
    buffer[0x0E] = 0x01; buffer[0x0F] = 0x00;  /* Reserved sectors: 1 */
    buffer[0x10] = 0x02;                        /* Number of FATs: 2 */
    buffer[0x11] = 0xE0; buffer[0x12] = 0x00;  /* Root entries: 224 */
    buffer[0x13] = 0x40; buffer[0x14] = 0x0B;  /* Total sectors: 2880 */
    buffer[0x15] = 0xF0;                        /* Media descriptor: 0xF0 */
    buffer[0x16] = 0x09; buffer[0x17] = 0x00;  /* Sectors per FAT: 9 */
    buffer[0x18] = 0x12; buffer[0x19] = 0x00;  /* Sectors per track: 18 */
    buffer[0x1A] = 0x02; buffer[0x1B] = 0x00;  /* Heads: 2 */
    buffer[0x1C] = 0x00; buffer[0x1D] = 0x00;  /* Hidden sectors (low) */
    buffer[0x1E] = 0x00; buffer[0x1F] = 0x00;  /* Hidden sectors (high) */
    buffer[0x20] = 0x00; buffer[0x21] = 0x00;  /* Total sectors 32-bit (low) */
    buffer[0x22] = 0x00; buffer[0x23] = 0x00;  /* Total sectors 32-bit (high) */
    
    /* Extended BPB */
    buffer[0x24] = 0x00;                        /* Drive number */
    buffer[0x25] = 0x00;                        /* Reserved */
    buffer[0x26] = 0x29;                        /* Extended boot signature */
    buffer[0x27] = 0x78; buffer[0x28] = 0x56;  /* Volume serial (low) */
    buffer[0x29] = 0x34; buffer[0x2A] = 0x12;  /* Volume serial (high) */
    memcpy(buffer + 0x2B, "TESTVOLUME ", 11);  /* Volume label */
    memcpy(buffer + 0x36, "FAT12   ", 8);      /* FS type */
    
    /* Boot signature */
    buffer[510] = 0x55;
    buffer[511] = 0xAA;
}

/* Create a 360KB boot sector */
static void create_360kb_boot_sector(uint8_t *buffer) {
    memset(buffer, 0, 512);
    
    buffer[0] = 0xEB;
    buffer[1] = 0x3C;
    buffer[2] = 0x90;
    
    memcpy(buffer + 3, "MSDOS3.3", 8);
    
    /* BPB for 360KB disk */
    buffer[0x0B] = 0x00; buffer[0x0C] = 0x02;  /* 512 bytes/sector */
    buffer[0x0D] = 0x02;                        /* 2 sectors/cluster */
    buffer[0x0E] = 0x01; buffer[0x0F] = 0x00;  /* 1 reserved sector */
    buffer[0x10] = 0x02;                        /* 2 FATs */
    buffer[0x11] = 0x70; buffer[0x12] = 0x00;  /* 112 root entries */
    buffer[0x13] = 0xD0; buffer[0x14] = 0x02;  /* 720 total sectors */
    buffer[0x15] = 0xFD;                        /* Media: 0xFD (360KB) */
    buffer[0x16] = 0x02; buffer[0x17] = 0x00;  /* 2 sectors/FAT */
    buffer[0x18] = 0x09; buffer[0x19] = 0x00;  /* 9 sectors/track */
    buffer[0x1A] = 0x02; buffer[0x1B] = 0x00;  /* 2 heads */
    
    buffer[510] = 0x55;
    buffer[511] = 0xAA;
}

/* ============================================================================
 * Tests
 * ============================================================================ */

static int test_boot_signature_check(void) {
    uint8_t valid[512] = {0};
    uint8_t invalid[512] = {0};
    
    valid[510] = 0x55;
    valid[511] = 0xAA;
    
    invalid[510] = 0x00;
    invalid[511] = 0x00;
    
    ASSERT_TRUE(fat_check_boot_signature(valid, 512), "Valid signature not detected");
    ASSERT_TRUE(!fat_check_boot_signature(invalid, 512), "Invalid signature not rejected");
    ASSERT_TRUE(!fat_check_boot_signature(NULL, 512), "NULL pointer not handled");
    ASSERT_TRUE(!fat_check_boot_signature(valid, 100), "Small buffer not rejected");
    
    TEST_PASS();
    return 0;
}

static int test_jump_instruction_check(void) {
    uint8_t jmp_short[3] = {0xEB, 0x3C, 0x90};  /* JMP SHORT + NOP */
    uint8_t jmp_near[3] = {0xE9, 0x00, 0x01};   /* JMP NEAR */
    uint8_t invalid[3] = {0x00, 0x00, 0x00};
    
    ASSERT_TRUE(fat_check_jump_instruction(jmp_short), "JMP SHORT not detected");
    ASSERT_TRUE(fat_check_jump_instruction(jmp_near), "JMP NEAR not detected");
    ASSERT_TRUE(!fat_check_jump_instruction(invalid), "Invalid jump not rejected");
    
    TEST_PASS();
    return 0;
}

static int test_media_descriptions(void) {
    /* From OpenGate article */
    ASSERT_TRUE(strstr(fat_media_description(0xF0), "1.44") != NULL, "0xF0 description wrong");
    ASSERT_TRUE(strstr(fat_media_description(0xF8), "Hard") != NULL, "0xF8 description wrong");
    ASSERT_TRUE(strstr(fat_media_description(0xF9), "720") != NULL, "0xF9 description wrong");
    ASSERT_TRUE(strstr(fat_media_description(0xFC), "180") != NULL, "0xFC description wrong");
    ASSERT_TRUE(strstr(fat_media_description(0xFD), "360") != NULL, "0xFD description wrong");
    ASSERT_TRUE(strstr(fat_media_description(0xFE), "160") != NULL, "0xFE description wrong");
    ASSERT_TRUE(strstr(fat_media_description(0xFF), "320") != NULL, "0xFF description wrong");
    
    TEST_PASS();
    return 0;
}

static int test_fat_type_determination(void) {
    ASSERT_EQ(fat_determine_type(100), FAT_TYPE_FAT12, "Small cluster count should be FAT12");
    ASSERT_EQ(fat_determine_type(4000), FAT_TYPE_FAT12, "4000 clusters should be FAT12");
    ASSERT_EQ(fat_determine_type(4085), FAT_TYPE_FAT16, "4085 clusters should be FAT16");
    ASSERT_EQ(fat_determine_type(65524), FAT_TYPE_FAT16, "65524 clusters should be FAT16");
    ASSERT_EQ(fat_determine_type(65525), FAT_TYPE_FAT32, "65525 clusters should be FAT32");
    
    TEST_PASS();
    return 0;
}

static int test_geometry_lookup(void) {
    const fat_disk_geometry_t *geom;
    
    /* 1.44MB disk */
    geom = fat_find_geometry(2880, 0xF0);
    ASSERT_TRUE(geom != NULL, "1.44MB geometry not found");
    ASSERT_TRUE(strstr(geom->name, "1.44") != NULL, "1.44MB geometry name wrong");
    
    /* 360KB disk */
    geom = fat_find_geometry(720, 0xFD);
    ASSERT_TRUE(geom != NULL, "360KB geometry not found");
    ASSERT_TRUE(strstr(geom->name, "360") != NULL, "360KB geometry name wrong");
    
    /* Unknown geometry */
    geom = fat_find_geometry(12345, 0xF0);
    ASSERT_TRUE(geom == NULL, "Unknown geometry should return NULL");
    
    TEST_PASS();
    return 0;
}

static int test_144mb_analysis(void) {
    uint8_t boot_sector[512];
    fat_analysis_result_t result;
    
    create_144mb_boot_sector(boot_sector);
    
    int ret = fat_analyze_boot_sector(boot_sector, 512, &result);
    ASSERT_EQ(ret, FAT_OK, "Analysis should succeed");
    
    ASSERT_TRUE(result.valid, "Boot sector should be valid");
    ASSERT_TRUE(result.has_boot_signature, "Boot signature should be present");
    ASSERT_TRUE(result.has_jump_instruction, "Jump instruction should be valid");
    ASSERT_TRUE(result.has_valid_bpb, "BPB should be valid");
    ASSERT_TRUE(result.has_extended_bpb, "Extended BPB should be present");
    
    ASSERT_EQ(result.bytes_per_sector, 512, "Bytes per sector wrong");
    ASSERT_EQ(result.sectors_per_cluster, 1, "Sectors per cluster wrong");
    ASSERT_EQ(result.fat_count, 2, "FAT count wrong");
    ASSERT_EQ(result.root_entry_count, 224, "Root entries wrong");
    ASSERT_EQ(result.total_sectors, 2880, "Total sectors wrong");
    ASSERT_EQ(result.media_type, 0xF0, "Media type wrong");
    ASSERT_EQ(result.sectors_per_fat, 9, "Sectors per FAT wrong");
    ASSERT_EQ(result.sectors_per_track, 18, "Sectors per track wrong");
    ASSERT_EQ(result.head_count, 2, "Head count wrong");
    
    ASSERT_EQ(result.fat_type, FAT_TYPE_FAT12, "FAT type should be FAT12");
    ASSERT_EQ(result.total_bytes, 2880 * 512, "Total bytes wrong");
    
    ASSERT_TRUE(result.geometry != NULL, "Standard geometry should be found");
    
    TEST_PASS();
    return 0;
}

static int test_360kb_analysis(void) {
    uint8_t boot_sector[512];
    fat_analysis_result_t result;
    
    create_360kb_boot_sector(boot_sector);
    
    int ret = fat_analyze_boot_sector(boot_sector, 512, &result);
    ASSERT_EQ(ret, FAT_OK, "Analysis should succeed");
    
    ASSERT_TRUE(result.valid, "Boot sector should be valid");
    ASSERT_EQ(result.total_sectors, 720, "Total sectors wrong");
    ASSERT_EQ(result.media_type, 0xFD, "Media type wrong");
    ASSERT_EQ(result.sectors_per_track, 9, "Sectors per track wrong");
    
    TEST_PASS();
    return 0;
}

static int test_invalid_boot_sector(void) {
    uint8_t invalid[512] = {0};
    fat_analysis_result_t result;
    
    int ret = fat_analyze_boot_sector(invalid, 512, &result);
    ASSERT_EQ(ret, FAT_OK, "Analysis should complete");
    ASSERT_TRUE(!result.valid, "Invalid boot sector should be marked invalid");
    ASSERT_TRUE(!result.has_boot_signature, "Missing signature should be detected");
    
    TEST_PASS();
    return 0;
}

static int test_report_generation(void) {
    uint8_t boot_sector[512];
    fat_analysis_result_t result;
    char report[4096];
    
    create_144mb_boot_sector(boot_sector);
    fat_analyze_boot_sector(boot_sector, 512, &result);
    
    int len = fat_generate_report(&result, report, sizeof(report));
    ASSERT_TRUE(len > 0, "Report should have content");
    ASSERT_TRUE(strstr(report, "1.44") != NULL, "Report should mention 1.44MB");
    ASSERT_TRUE(strstr(report, "FAT12") != NULL, "Report should mention FAT12");
    ASSERT_TRUE(strstr(report, "0xF0") != NULL, "Report should show media type");
    
    TEST_PASS();
    return 0;
}

static int test_boot_sector_creation(void) {
    uint8_t buffer[512];
    fat_analysis_result_t result;
    
    int ret = fat_create_boot_sector(&fat_geometry_1440k, "TESTNAME", "MY VOLUME  ", buffer);
    ASSERT_EQ(ret, FAT_OK, "Creation should succeed");
    
    ret = fat_analyze_boot_sector(buffer, 512, &result);
    ASSERT_EQ(ret, FAT_OK, "Analysis of created sector should succeed");
    ASSERT_TRUE(result.valid, "Created boot sector should be valid");
    ASSERT_EQ(result.total_sectors, 2880, "Created sector should have 2880 sectors");
    
    TEST_PASS();
    return 0;
}

static int test_serial_formatting(void) {
    char buffer[16];
    
    fat_format_serial(0x12345678, buffer);
    ASSERT_STREQ(buffer, "1234-5678", "Serial format wrong");
    
    fat_format_serial(0xABCD1234, buffer);
    ASSERT_STREQ(buffer, "ABCD-1234", "Serial format wrong for hex");
    
    TEST_PASS();
    return 0;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    int failed = 0;
    
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║     FAT Boot Sector Analysis Module - Unit Tests                ║\n");
    printf("║     Based on OpenGate.at article specifications                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Testing boot signature check...\n");
    failed += test_boot_signature_check();
    
    printf("Testing jump instruction check...\n");
    failed += test_jump_instruction_check();
    
    printf("Testing media descriptions (OpenGate article)...\n");
    failed += test_media_descriptions();
    
    printf("Testing FAT type determination...\n");
    failed += test_fat_type_determination();
    
    printf("Testing geometry lookup...\n");
    failed += test_geometry_lookup();
    
    printf("Testing 1.44MB boot sector analysis...\n");
    failed += test_144mb_analysis();
    
    printf("Testing 360KB boot sector analysis...\n");
    failed += test_360kb_analysis();
    
    printf("Testing invalid boot sector handling...\n");
    failed += test_invalid_boot_sector();
    
    printf("Testing report generation...\n");
    failed += test_report_generation();
    
    printf("Testing boot sector creation...\n");
    failed += test_boot_sector_creation();
    
    printf("Testing serial number formatting...\n");
    failed += test_serial_formatting();
    
    printf("\n");
    if (failed == 0) {
        printf("╔══════════════════════════════════════════════════════════════════╗\n");
        printf("║                    ALL TESTS PASSED! ✅                          ║\n");
        printf("╚══════════════════════════════════════════════════════════════════╝\n");
    } else {
        printf("╔══════════════════════════════════════════════════════════════════╗\n");
        printf("║                    %d TEST(S) FAILED! ❌                          ║\n", failed);
        printf("╚══════════════════════════════════════════════════════════════════╝\n");
    }
    
    return failed;
}
