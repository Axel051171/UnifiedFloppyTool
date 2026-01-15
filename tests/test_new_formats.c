/**
 * @file test_new_formats.c
 * @brief Tests for New Format Handlers (DiskImageTool features)
 * 
 * Tests: Image DB, IMZ, MFM, PCE (PSI/PRI), 86F, FAT Editor, MFM Codec, SOS
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "uft/uft_image_db.h"
#include "uft/uft_fat_editor.h"
#include "uft/uft_mfm_codec.h"
#include "uft/uft_sos_protection.h"
#include "uft/uft_apd_format.h"

/*============================================================================
 * TEST FRAMEWORK
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;
static int current_test_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s ... ", #name); \
    tests_run++; \
    current_test_failed = 0; \
    test_##name(); \
    if (!current_test_failed) { \
        tests_passed++; \
        printf("PASS\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        current_test_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/*============================================================================
 * TESTS: IMAGE DATABASE
 *============================================================================*/

TEST(image_db_init) {
    int rc = uft_image_db_init();
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(uft_image_db_count(), 0);
    uft_image_db_shutdown();
}

TEST(image_db_crc32) {
    uft_image_db_init();
    
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03};
    uint32_t crc = uft_image_db_crc32(data, sizeof(data));
    ASSERT_NE(crc, 0);
    
    /* Same data should give same CRC */
    uint32_t crc2 = uft_image_db_crc32(data, sizeof(data));
    ASSERT_EQ(crc, crc2);
    
    /* Different data should give different CRC */
    data[0] = 0xFF;
    uint32_t crc3 = uft_image_db_crc32(data, sizeof(data));
    ASSERT_NE(crc, crc3);
    
    uft_image_db_shutdown();
}

TEST(image_db_add_entry) {
    uft_image_db_init();
    
    uft_image_entry_t entry = {0};
    strcpy(entry.name, "Test Image");
    entry.category = UFT_IMG_CAT_GAME;
    entry.platform = UFT_IMG_PLAT_MSDOS;
    entry.hash.crc32 = 0x12345678;
    
    int rc = uft_image_db_add(&entry);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(uft_image_db_count(), 1);
    
    uft_image_db_shutdown();
}

TEST(image_db_find_by_crc) {
    uft_image_db_init();
    
    uft_image_entry_t entry = {0};
    strcpy(entry.name, "Test Game");
    entry.hash.crc32 = 0xDEADBEEF;
    uft_image_db_add(&entry);
    
    const uft_image_entry_t *found = uft_image_db_find_by_crc(0xDEADBEEF);
    ASSERT_NOT_NULL(found);
    ASSERT_STR_EQ(found->name, "Test Game");
    
    /* Not found */
    found = uft_image_db_find_by_crc(0x11111111);
    ASSERT(found == NULL);
    
    uft_image_db_shutdown();
}

TEST(image_db_identify) {
    uft_image_db_init();
    
    /* Add test entry */
    uint8_t test_data[512] = {0xEB, 0x3C, 0x90};  /* Boot sector */
    strcpy((char*)test_data + 3, "MSDOS5.0");
    
    uft_image_entry_t entry = {0};
    strcpy(entry.name, "MS-DOS Boot Disk");
    entry.hash.crc32 = uft_image_db_crc32(test_data, sizeof(test_data));
    entry.image_size = sizeof(test_data);
    uft_image_db_add(&entry);
    
    /* Try to identify */
    uft_match_result_t result;
    int rc = uft_image_db_identify(test_data, sizeof(test_data), &result);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(result.level, UFT_MATCH_EXACT);
    ASSERT_EQ(result.confidence, 100);
    
    uft_image_db_shutdown();
}

TEST(image_db_oem_lookup) {
    const uft_oem_entry_t *oem;
    
    oem = uft_image_db_lookup_oem("MSDOS5.0");
    ASSERT_NOT_NULL(oem);
    ASSERT_TRUE(oem->is_valid);
    
    oem = uft_image_db_lookup_oem("mkdosfs");
    ASSERT_NOT_NULL(oem);
    ASSERT_FALSE(oem->is_valid);
}

TEST(image_db_parse_boot) {
    uint8_t boot[512] = {0};
    
    /* Set up a boot sector */
    boot[0] = 0xEB; boot[1] = 0x3C; boot[2] = 0x90;
    memcpy(boot + 3, "MSDOS5.0", 8);
    boot[11] = 0x00; boot[12] = 0x02;  /* 512 bytes/sector */
    boot[13] = 1;                       /* 1 sector/cluster */
    boot[16] = 2;                       /* 2 FATs */
    boot[21] = 0xF0;                    /* Media descriptor */
    
    uft_boot_signature_t sig;
    int rc = uft_image_db_parse_boot(boot, &sig);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(sig.bytes_per_sector, 512);
    ASSERT_EQ(sig.fat_count, 2);
    ASSERT_EQ(sig.media_descriptor, 0xF0);
}

TEST(image_db_category_names) {
    ASSERT_STR_EQ(uft_image_db_category_name(UFT_IMG_CAT_GAME), "Game");
    ASSERT_STR_EQ(uft_image_db_category_name(UFT_IMG_CAT_SYSTEM), "System");
}

TEST(image_db_platform_names) {
    ASSERT_STR_EQ(uft_image_db_platform_name(UFT_IMG_PLAT_MSDOS), "MS-DOS");
    ASSERT_STR_EQ(uft_image_db_platform_name(UFT_IMG_PLAT_AMIGA), "Amiga");
}

TEST(image_db_protection_names) {
    ASSERT_STR_EQ(uft_image_db_protection_name(UFT_IMG_PROT_NONE), "None");
    ASSERT_STR_EQ(uft_image_db_protection_name(UFT_IMG_PROT_WEAK_BITS), "Weak Bits");
}

/*============================================================================
 * TESTS: FAT EDITOR
 *============================================================================*/

/* Create a minimal FAT12 boot sector */
static void create_fat12_boot(uint8_t *boot) {
    memset(boot, 0, 512);
    
    /* Jump instruction */
    boot[0] = 0xEB; boot[1] = 0x3C; boot[2] = 0x90;
    
    /* OEM name */
    memcpy(boot + 3, "MSDOS5.0", 8);
    
    /* BPB for 1.44MB floppy */
    boot[11] = 0x00; boot[12] = 0x02;   /* 512 bytes/sector */
    boot[13] = 1;                        /* 1 sector/cluster */
    boot[14] = 1; boot[15] = 0;          /* 1 reserved sector */
    boot[16] = 2;                        /* 2 FATs */
    boot[17] = 0xE0; boot[18] = 0x00;    /* 224 root entries */
    boot[19] = 0x40; boot[20] = 0x0B;    /* 2880 total sectors */
    boot[21] = 0xF0;                     /* Media descriptor */
    boot[22] = 9; boot[23] = 0;          /* 9 sectors/FAT */
    boot[24] = 18; boot[25] = 0;         /* 18 sectors/track */
    boot[26] = 2; boot[27] = 0;          /* 2 heads */
    
    /* Volume label */
    memcpy(boot + 43, "NO NAME    ", 11);
    
    /* FS type */
    memcpy(boot + 54, "FAT12   ", 8);
    
    /* Boot signature */
    boot[510] = 0x55;
    boot[511] = 0xAA;
}

TEST(fat_probe) {
    uint8_t boot[512];
    create_fat12_boot(boot);
    
    ASSERT_TRUE(uft_fat_probe(boot, sizeof(boot)));
    
    /* Invalid boot sector */
    boot[510] = 0x00;
    ASSERT_FALSE(uft_fat_probe(boot, sizeof(boot)));
}

TEST(fat_type_detection) {
    ASSERT_STR_EQ(uft_fat_type_name(UFT_FAT_12), "FAT12");
    ASSERT_STR_EQ(uft_fat_type_name(UFT_FAT_16), "FAT16");
    ASSERT_STR_EQ(uft_fat_type_name(UFT_FAT_32), "FAT32");
    ASSERT_STR_EQ(uft_fat_type_name(UFT_FAT_UNKNOWN), "Unknown");
}

TEST(fat_date_encoding) {
    int year, month, day;
    
    /* Encode and decode a date */
    uint16_t date = uft_fat_encode_date(2025, 1, 15);
    uft_fat_decode_date(date, &year, &month, &day);
    
    ASSERT_EQ(year, 2025);
    ASSERT_EQ(month, 1);
    ASSERT_EQ(day, 15);
}

TEST(fat_time_encoding) {
    int hour, minute, second;
    
    /* Encode and decode a time */
    uint16_t time = uft_fat_encode_time(14, 30, 22);
    uft_fat_decode_time(time, &hour, &minute, &second);
    
    ASSERT_EQ(hour, 14);
    ASSERT_EQ(minute, 30);
    ASSERT_EQ(second, 22);  /* Rounded to 2-second precision */
}

TEST(fat_name_conversion) {
    uft_fat_dirent_t entry;
    char name[13];
    
    /* Set up entry with "FILENAME.TXT" */
    memcpy(entry.name, "FILENAME", 8);
    memcpy(entry.ext, "TXT", 3);
    entry.attributes = 0;
    
    uft_fat_name_to_string(&entry, name, sizeof(name));
    ASSERT_STR_EQ(name, "FILENAME.TXT");
    
    /* Entry with no extension */
    memcpy(entry.name, "NOEXT   ", 8);
    memcpy(entry.ext, "   ", 3);
    
    uft_fat_name_to_string(&entry, name, sizeof(name));
    ASSERT_STR_EQ(name, "NOEXT");
}

TEST(fat_open_minimal) {
    /* Create minimal FAT12 disk image (just boot sector + FATs) */
    size_t img_size = 1474560;  /* 1.44MB */
    uint8_t *img = (uint8_t *)calloc(1, img_size);
    ASSERT_NOT_NULL(img);
    
    /* Set up boot sector */
    create_fat12_boot(img);
    
    /* Initialize FAT (first FAT at sector 1) */
    /* FAT12 first two entries: F0 FF FF */
    img[512] = 0xF0;
    img[513] = 0xFF;
    img[514] = 0xFF;
    
    /* Second FAT copy (at sector 10) */
    img[512 * 10] = 0xF0;
    img[512 * 10 + 1] = 0xFF;
    img[512 * 10 + 2] = 0xFF;
    
    /* Try to open */
    uft_fat_t *fat = uft_fat_open(img, img_size);
    ASSERT_NOT_NULL(fat);
    
    ASSERT_EQ(uft_fat_get_type(fat), UFT_FAT_12);
    
    /* Get stats */
    uft_fat_stats_t stats;
    int rc = uft_fat_get_stats(fat, &stats);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(stats.type, UFT_FAT_12);
    ASSERT_TRUE(stats.total_clusters > 0);
    
    uft_fat_close(fat);
    free(img);
}

/*============================================================================
 * TESTS: MFM CODEC
 *============================================================================*/

TEST(mfm_codec_create) {
    uft_mfm_codec_t *codec = uft_mfm_codec_create();
    ASSERT_NOT_NULL(codec);
    uft_mfm_codec_destroy(codec);
}

TEST(mfm_encode_decode_byte) {
    /* Test encoding and decoding a single byte */
    uint8_t test_byte = 0xA5;
    
    uint16_t encoded = uft_mfm_encode_byte(test_byte, 0);
    uint8_t decoded = uft_mfm_decode_byte(encoded);
    
    ASSERT_EQ(decoded, test_byte);
}

TEST(mfm_encode_decode_buffer) {
    uint8_t data[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    uint8_t mfm[16];
    uint8_t result[8];
    
    int encoded = uft_mfm_encode(data, sizeof(data), mfm, sizeof(mfm));
    ASSERT_EQ(encoded, 16);  /* 2x data size */
    
    int decoded = uft_mfm_decode(mfm, sizeof(data) * 16, result, sizeof(result));
    ASSERT_EQ(decoded, 8);
    
    /* Verify round-trip */
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(result[i], data[i]);
    }
}

TEST(mfm_sync_pattern) {
    uint16_t sync = uft_mfm_encode_sync();
    ASSERT_EQ(sync, 0x4489);  /* Standard MFM sync */
}

TEST(fm_encode_decode) {
    uint8_t test_byte = 0x55;
    
    uint16_t encoded = uft_fm_encode_byte(test_byte);
    uint8_t decoded = uft_fm_decode_byte(encoded);
    
    ASSERT_EQ(decoded, test_byte);
}

TEST(disk_crc) {
    uint8_t data[] = {0xFE, 0x00, 0x00, 0x01, 0x02};  /* ID field */
    uint16_t crc = uft_disk_crc(data, sizeof(data));
    
    /* CRC should be non-zero */
    ASSERT_NE(crc, 0);
    
    /* Same data should give same CRC */
    uint16_t crc2 = uft_disk_crc(data, sizeof(data));
    ASSERT_EQ(crc, crc2);
}

TEST(sector_size_conversion) {
    ASSERT_EQ(uft_sector_size_from_code(0), 128);
    ASSERT_EQ(uft_sector_size_from_code(1), 256);
    ASSERT_EQ(uft_sector_size_from_code(2), 512);
    ASSERT_EQ(uft_sector_size_from_code(3), 1024);
    
    ASSERT_EQ(uft_sector_code_from_size(128), 0);
    ASSERT_EQ(uft_sector_code_from_size(256), 1);
    ASSERT_EQ(uft_sector_code_from_size(512), 2);
    ASSERT_EQ(uft_sector_code_from_size(1024), 3);
}

TEST(encoding_names) {
    ASSERT_STR_EQ(uft_encoding_name(UFT_ENC_FM), "FM");
    ASSERT_STR_EQ(uft_encoding_name(UFT_ENC_MFM), "MFM");
    ASSERT_STR_EQ(uft_encoding_name(UFT_ENC_GCR_APPLE), "Apple GCR");
}

TEST(bit_utilities) {
    /* Test bit reversal */
    ASSERT_EQ(uft_reverse_bits(0x80), 0x01);
    ASSERT_EQ(uft_reverse_bits(0x01), 0x80);
    ASSERT_EQ(uft_reverse_bits(0xF0), 0x0F);
    
    /* Test popcount */
    ASSERT_EQ(uft_popcount(0), 0);
    ASSERT_EQ(uft_popcount(1), 1);
    ASSERT_EQ(uft_popcount(0xFF), 8);
    ASSERT_EQ(uft_popcount(0xFFFFFFFF), 32);
}

/*============================================================================
 * TESTS: SOS PROTECTION
 *============================================================================*/

TEST(sos_create_destroy) {
    uft_sos_t *sos = uft_sos_create();
    ASSERT_NOT_NULL(sos);
    uft_sos_destroy(sos);
}

TEST(sos_game_names) {
    ASSERT_STR_EQ(uft_sos_game_name(UFT_SOS_GAME_CANNON_FODDER), "Cannon Fodder");
    ASSERT_STR_EQ(uft_sos_game_name(UFT_SOS_GAME_SENSIBLE_SOCCER), "Sensible Soccer");
    ASSERT_STR_EQ(uft_sos_game_name(UFT_SOS_GAME_MEGA_LO_MANIA), "Mega Lo Mania");
    ASSERT_STR_EQ(uft_sos_game_name(UFT_SOS_GAME_UNKNOWN), "Unknown");
}

TEST(sos_checksum) {
    uint8_t data[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
                        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    
    uint32_t checksum = uft_sos_checksum(data, sizeof(data));
    
    /* Checksum should be non-zero */
    ASSERT_NE(checksum, 0);
    
    /* Same data gives same checksum */
    uint32_t checksum2 = uft_sos_checksum(data, sizeof(data));
    ASSERT_EQ(checksum, checksum2);
    
    /* Verify function should work */
    ASSERT_TRUE(uft_sos_verify_checksum(data, sizeof(data), checksum));
    ASSERT_FALSE(uft_sos_verify_checksum(data, sizeof(data), checksum + 1));
}

TEST(sos_detect_empty) {
    /* Empty or small data should return 0 */
    uint8_t empty[10] = {0};
    ASSERT_EQ(uft_sos_detect_track(empty, sizeof(empty)), 0);
    ASSERT_EQ(uft_sos_detect_track(NULL, 0), 0);
}

TEST(sos_track_free) {
    uft_sos_track_t track;
    memset(&track, 0, sizeof(track));
    
    /* Allocate some data */
    track.data = (uint8_t *)malloc(100);
    track.data_size = 100;
    
    /* Free should work */
    uft_sos_track_free(&track);
    
    ASSERT_TRUE(track.data == NULL);
    ASSERT_EQ(track.data_size, 0);
}

/*============================================================================
 * TESTS: APD FORMAT
 *============================================================================*/

TEST(apd_create_destroy) {
    uft_apd_t *apd = uft_apd_create();
    ASSERT_NOT_NULL(apd);
    uft_apd_destroy(apd);
}

TEST(apd_detect_empty) {
    /* Empty or small data should return 0 */
    uint8_t empty[10] = {0};
    ASSERT_EQ(uft_apd_detect(empty, sizeof(empty)), 0);
    ASSERT_EQ(uft_apd_detect(NULL, 0), 0);
}

TEST(apd_detect_magic) {
    /* Test with valid APD magic */
    uint8_t header[2048];
    memset(header, 0, sizeof(header));
    memcpy(header, "APDX0001", 8);
    
    ASSERT_EQ(uft_apd_detect(header, sizeof(header)), 100);
}

TEST(apd_detect_gzip) {
    /* Gzip magic should give tentative score - need larger buffer */
    uint8_t gzip_header[2048];
    memset(gzip_header, 0, sizeof(gzip_header));
    gzip_header[0] = 0x1F;
    gzip_header[1] = 0x8B;
    gzip_header[2] = 0x08;
    
    /* Should give tentative score (30) for potential gzipped APD */
    int score = uft_apd_detect(gzip_header, sizeof(gzip_header));
    ASSERT_TRUE(score >= 30);  /* At least tentative */
}

TEST(apd_format_names) {
    ASSERT_STR_EQ(uft_adfs_format_name(UFT_ADFS_FORMAT_E), "ADFS E (800K)");
    ASSERT_STR_EQ(uft_adfs_format_name(UFT_ADFS_FORMAT_L), "ADFS L (640K)");
    ASSERT_STR_EQ(uft_adfs_format_name(UFT_ADFS_FORMAT_F), "ADFS F (1600K)");
    ASSERT_STR_EQ(uft_adfs_format_name(-1), "Unknown");
}

TEST(apd_protection_names) {
    ASSERT_STR_EQ(uft_acorn_protection_name(UFT_ACORN_PROT_NONE), "None");
    ASSERT_STR_EQ(uft_acorn_protection_name(UFT_ACORN_PROT_WEAK_BITS), "Weak Bits");
    ASSERT_STR_EQ(uft_acorn_protection_name(UFT_ACORN_PROT_LONG_TRACK), "Long Track");
    ASSERT_STR_EQ(uft_acorn_protection_name(UFT_ACORN_PROT_MIXED_DENSITY), "Mixed Density");
}

TEST(apd_track_helpers) {
    /* Test track number calculation */
    ASSERT_EQ(uft_apd_track_num(0, 0), 0);
    ASSERT_EQ(uft_apd_track_num(0, 1), 1);
    ASSERT_EQ(uft_apd_track_num(1, 0), 2);
    ASSERT_EQ(uft_apd_track_num(79, 1), 159);
    
    /* Test reverse calculation */
    ASSERT_EQ(uft_apd_cylinder(0), 0);
    ASSERT_EQ(uft_apd_cylinder(159), 79);
    ASSERT_EQ(uft_apd_head(0), 0);
    ASSERT_EQ(uft_apd_head(1), 1);
    ASSERT_EQ(uft_apd_head(159), 1);
}

TEST(apd_track_free) {
    uft_apd_track_t track;
    memset(&track, 0, sizeof(track));
    
    /* Allocate some data */
    track.sd_data = (uint8_t *)malloc(100);
    track.dd_data = (uint8_t *)malloc(200);
    track.qd_data = (uint8_t *)malloc(300);
    track.sd_size = 100;
    track.dd_size = 200;
    track.qd_size = 300;
    
    /* Free should work */
    uft_apd_track_free(&track);
    
    ASSERT_TRUE(track.sd_data == NULL);
    ASSERT_TRUE(track.dd_data == NULL);
    ASSERT_TRUE(track.qd_data == NULL);
    ASSERT_EQ(track.sd_size, 0);
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT New Formats Tests (DiskImageTool Features)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    printf("[SUITE] Image Database\n");
    RUN_TEST(image_db_init);
    RUN_TEST(image_db_crc32);
    RUN_TEST(image_db_add_entry);
    RUN_TEST(image_db_find_by_crc);
    RUN_TEST(image_db_identify);
    RUN_TEST(image_db_oem_lookup);
    RUN_TEST(image_db_parse_boot);
    RUN_TEST(image_db_category_names);
    RUN_TEST(image_db_platform_names);
    RUN_TEST(image_db_protection_names);
    
    printf("\n[SUITE] FAT Editor\n");
    RUN_TEST(fat_probe);
    RUN_TEST(fat_type_detection);
    RUN_TEST(fat_date_encoding);
    RUN_TEST(fat_time_encoding);
    RUN_TEST(fat_name_conversion);
    RUN_TEST(fat_open_minimal);
    
    printf("\n[SUITE] MFM Codec\n");
    RUN_TEST(mfm_codec_create);
    RUN_TEST(mfm_encode_decode_byte);
    RUN_TEST(mfm_encode_decode_buffer);
    RUN_TEST(mfm_sync_pattern);
    RUN_TEST(fm_encode_decode);
    RUN_TEST(disk_crc);
    RUN_TEST(sector_size_conversion);
    RUN_TEST(encoding_names);
    RUN_TEST(bit_utilities);
    
    printf("\n[SUITE] SOS Protection\n");
    RUN_TEST(sos_create_destroy);
    RUN_TEST(sos_game_names);
    RUN_TEST(sos_checksum);
    RUN_TEST(sos_detect_empty);
    RUN_TEST(sos_track_free);
    
    printf("\n[SUITE] APD Format (Acorn)\n");
    RUN_TEST(apd_create_destroy);
    RUN_TEST(apd_detect_empty);
    RUN_TEST(apd_detect_magic);
    RUN_TEST(apd_detect_gzip);
    RUN_TEST(apd_format_names);
    RUN_TEST(apd_protection_names);
    RUN_TEST(apd_track_helpers);
    RUN_TEST(apd_track_free);
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n", 
           tests_passed, tests_run - tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
