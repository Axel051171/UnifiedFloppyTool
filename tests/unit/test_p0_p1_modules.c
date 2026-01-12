/**
 * @file test_p0_p1_modules.c
 * @brief Comprehensive test suite for P0/P1 modules
 * 
 * P1-001: Test coverage improvement
 * 
 * Tests:
 * - uft_unified_types
 * - uft_fusion
 * - uft_cqm
 * - uft_nib
 * - uft_g64_extended
 * - uft_adf_pipeline
 * - uft_td0_writer
 * - uft_protection_stubs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

/* Module headers */
#include "uft/core/uft_unified_types.h"
#include "uft/core/uft_fusion.h"
#include "uft/formats/uft_cqm.h"
#include "uft/formats/uft_nib.h"
#include "uft/formats/uft_g64_extended.h"
#include "uft/formats/uft_adf_pipeline.h"
#include "uft/formats/uft_td0_writer.h"
#include "uft/protection/uft_protection_stubs.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test macros */
#define TEST_START(name) do { \
    printf("  [TEST] %s... ", name); \
    tests_run++; \
} while(0)

#define TEST_PASS() do { \
    printf("✓\n"); \
    tests_passed++; \
} while(0)

#define TEST_FAIL(msg) do { \
    printf("✗ (%s)\n", msg); \
    tests_failed++; \
} while(0)

#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { TEST_FAIL(msg); return; } \
} while(0)

#define ASSERT_EQ(a, b, msg) ASSERT_TRUE((a) == (b), msg)
#define ASSERT_NE(a, b, msg) ASSERT_TRUE((a) != (b), msg)
#define ASSERT_NULL(p, msg) ASSERT_TRUE((p) == NULL, msg)
#define ASSERT_NOT_NULL(p, msg) ASSERT_TRUE((p) != NULL, msg)

/* ============================================================================
 * UNIFIED TYPES TESTS
 * ============================================================================ */

static void test_error_codes(void) {
    TEST_START("error_codes");
    
    ASSERT_EQ(UFT_OK, 0, "UFT_OK should be 0");
    ASSERT_NE(UFT_ERR_CRC, UFT_OK, "Error codes should differ");
    
    const char *str = uft_error_str(UFT_ERR_CRC);
    ASSERT_NOT_NULL(str, "Error string should not be NULL");
    ASSERT_TRUE(strlen(str) > 0, "Error string should not be empty");
    
    ASSERT_TRUE(uft_error_recoverable(UFT_ERR_CRC), "CRC should be recoverable");
    ASSERT_TRUE(!uft_error_recoverable(UFT_ERR_MEMORY), "Memory should not be recoverable");
    
    TEST_PASS();
}

static void test_sector_id(void) {
    TEST_START("sector_id");
    
    uft_sector_id_t id1 = {0};
    id1.track = 10;
    id1.head = 0;
    id1.sector = 5;
    id1.size_code = 2;
    
    uft_sector_id_t id2 = id1;
    id2.sector = 6;
    
    ASSERT_TRUE(uft_sector_id_equal(&id1, &id1), "Same ID should be equal");
    ASSERT_TRUE(!uft_sector_id_equal(&id1, &id2), "Different IDs should not be equal");
    
    TEST_PASS();
}

static void test_size_conversion(void) {
    TEST_START("size_conversion");
    
    ASSERT_EQ(uft_size_from_code(0), 128, "Code 0 = 128");
    ASSERT_EQ(uft_size_from_code(1), 256, "Code 1 = 256");
    ASSERT_EQ(uft_size_from_code(2), 512, "Code 2 = 512");
    ASSERT_EQ(uft_size_from_code(3), 1024, "Code 3 = 1024");
    
    ASSERT_EQ(uft_code_from_size(128), 0, "128 = Code 0");
    ASSERT_EQ(uft_code_from_size(512), 2, "512 = Code 2");
    
    TEST_PASS();
}

static void test_sector_alloc(void) {
    TEST_START("sector_alloc");
    
    uft_sector_t *sect = uft_sector_alloc(512);
    ASSERT_NOT_NULL(sect, "Sector should be allocated");
    ASSERT_NOT_NULL(sect->data, "Sector data should be allocated");
    ASSERT_EQ(sect->data_len, 512, "Data length should be 512");
    
    /* Fill with pattern */
    memset(sect->data, 0xAA, 512);
    
    /* Copy */
    uft_sector_t *copy = uft_sector_alloc(512);
    ASSERT_NOT_NULL(copy, "Copy should be allocated");
    uft_sector_copy(copy, sect);
    
    ASSERT_EQ(memcmp(copy->data, sect->data, 512), 0, "Data should match");
    
    uft_sector_free(sect);
    uft_sector_free(copy);
    
    TEST_PASS();
}

static void test_track_alloc(void) {
    TEST_START("track_alloc");
    
    uft_track_t *track = uft_track_alloc(18, 50000);
    ASSERT_NOT_NULL(track, "Track should be allocated");
    ASSERT_NOT_NULL(track->sectors, "Sectors should be allocated");
    ASSERT_EQ(track->sector_capacity, 18, "Capacity should be 18");
    
    track->track_num = 5;
    track->head = 0;
    track->encoding = UFT_ENC_MFM;
    
    /* Copy */
    uft_track_t *copy = uft_track_alloc(18, 50000);
    uft_track_copy(copy, track);
    
    ASSERT_EQ(copy->track_num, 5, "Track num should match");
    ASSERT_EQ(copy->encoding, UFT_ENC_MFM, "Encoding should match");
    
    uft_track_free(track);
    uft_track_free(copy);
    
    TEST_PASS();
}

static void test_disk_alloc(void) {
    TEST_START("disk_alloc");
    
    uft_disk_image_t *disk = uft_disk_alloc(80, 2);
    ASSERT_NOT_NULL(disk, "Disk should be allocated");
    ASSERT_EQ(disk->tracks, 80, "Tracks should be 80");
    ASSERT_EQ(disk->heads, 2, "Heads should be 2");
    ASSERT_EQ(disk->track_count, 160, "Track count should be 160");
    
    uft_disk_free(disk);
    
    TEST_PASS();
}

static void test_format_names(void) {
    TEST_START("format_names");
    
    const char *name = uft_format_name(UFT_FMT_D64);
    ASSERT_NOT_NULL(name, "D64 name should exist");
    ASSERT_TRUE(strstr(name, "D64") != NULL, "Should contain D64");
    
    name = uft_format_name(UFT_FMT_ADF);
    ASSERT_NOT_NULL(name, "ADF name should exist");
    
    TEST_PASS();
}

/* ============================================================================
 * FUSION TESTS
 * ============================================================================ */

static void test_fusion_options(void) {
    TEST_START("fusion_options");
    
    uft_fusion_options_t opts;
    uft_fusion_options_init(&opts);
    
    ASSERT_EQ(opts.method, UFT_FUSION_WEIGHTED, "Default method");
    ASSERT_EQ(opts.crc_valid_bonus, 50, "Default CRC bonus");
    ASSERT_EQ(opts.weak_threshold, 2, "Default weak threshold");
    
    TEST_PASS();
}

static void test_fusion_single_revision(void) {
    TEST_START("fusion_single_revision");
    
    /* Single revision - should pass through */
    uint8_t data[8] = {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
    
    uft_revision_input_t rev = {0};
    rev.data = data;
    rev.bit_count = 64;
    rev.quality = 100;
    rev.crc_valid = true;
    
    uint8_t output[8];
    size_t out_bits;
    
    uft_error_t err = uft_fusion_merge(&rev, 1, output, &out_bits,
                                       NULL, NULL, NULL, NULL);
    
    ASSERT_EQ(err, UFT_OK, "Fusion should succeed");
    ASSERT_EQ(out_bits, 64, "Output bits should match");
    ASSERT_EQ(memcmp(output, data, 8), 0, "Data should match");
    
    TEST_PASS();
}

static void test_fusion_majority_voting(void) {
    TEST_START("fusion_majority_voting");
    
    /* Three revisions with disagreement on some bits */
    uint8_t rev1[4] = {0xFF, 0x00, 0xFF, 0x00};
    uint8_t rev2[4] = {0xFF, 0x00, 0xFF, 0x00};
    uint8_t rev3[4] = {0xFF, 0xFF, 0xFF, 0x00};  /* Differs in byte 1 */
    
    uft_revision_input_t revisions[3] = {
        { .data = rev1, .bit_count = 32, .quality = 100 },
        { .data = rev2, .bit_count = 32, .quality = 100 },
        { .data = rev3, .bit_count = 32, .quality = 100 }
    };
    
    uint8_t output[4];
    size_t out_bits;
    uft_fusion_result_t result;
    
    uft_error_t err = uft_fusion_merge(revisions, 3, output, &out_bits,
                                       NULL, NULL, NULL, &result);
    
    ASSERT_EQ(err, UFT_OK, "Fusion should succeed");
    ASSERT_EQ(output[0], 0xFF, "Byte 0 should be 0xFF");
    ASSERT_EQ(output[1], 0x00, "Byte 1 should be 0x00 (majority)");
    ASSERT_TRUE(result.success, "Result should indicate success");
    
    TEST_PASS();
}

static void test_fusion_crc_weighting(void) {
    TEST_START("fusion_crc_weighting");
    
    /* Two revisions - CRC valid one should win despite being minority */
    uint8_t rev1[4] = {0x00, 0x00, 0x00, 0x00};
    uint8_t rev2[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    
    uft_revision_input_t revisions[2] = {
        { .data = rev1, .bit_count = 32, .quality = 50, .crc_valid = false },
        { .data = rev2, .bit_count = 32, .quality = 50, .crc_valid = true }
    };
    
    uint8_t output[4];
    size_t out_bits;
    
    uft_fusion_options_t opts;
    uft_fusion_options_init(&opts);
    opts.crc_valid_bonus = 100;  /* Strong CRC preference */
    
    uft_error_t err = uft_fusion_merge(revisions, 2, output, &out_bits,
                                       NULL, NULL, &opts, NULL);
    
    ASSERT_EQ(err, UFT_OK, "Fusion should succeed");
    ASSERT_EQ(output[0], 0xFF, "CRC-valid revision should win");
    
    TEST_PASS();
}

/* ============================================================================
 * CQM TESTS
 * ============================================================================ */

static void test_cqm_header_validation(void) {
    TEST_START("cqm_header_validation");
    
    cqm_header_t header = {0};
    
    /* Invalid signature */
    ASSERT_TRUE(!uft_cqm_validate_header(&header), "Empty header invalid");
    
    /* Valid header */
    memcpy(header.signature, "CQ\x14", 3);
    header.version = 1;
    header.sector_size = 512;
    header.total_tracks = 40;
    
    ASSERT_TRUE(uft_cqm_validate_header(&header), "Valid header");
    
    /* Invalid sector size */
    header.sector_size = 100;
    ASSERT_TRUE(!uft_cqm_validate_header(&header), "Invalid sector size");
    
    TEST_PASS();
}

static void test_cqm_compression(void) {
    TEST_START("cqm_compression");
    
    /* Test data with repeating pattern */
    uint8_t input[1024];
    memset(input, 0xE5, 1024);  /* Repeated byte - should compress well */
    
    uint8_t compressed[2048];
    int comp_size = cqm_compress(input, 1024, compressed, 2048, 6);
    
    ASSERT_TRUE(comp_size > 0, "Compression should succeed");
    ASSERT_TRUE(comp_size < 1024, "Should compress smaller");
    
    /* Decompress */
    uint8_t decompressed[1024];
    size_t decomp_size;
    int result = cqm_decompress_full(compressed, comp_size,
                                     decompressed, 1024, &decomp_size);
    
    ASSERT_TRUE(result >= 0, "Decompression should succeed");
    ASSERT_EQ(decomp_size, 1024, "Size should match");
    ASSERT_EQ(memcmp(input, decompressed, 1024), 0, "Data should match");
    
    TEST_PASS();
}

static void test_cqm_options(void) {
    TEST_START("cqm_options");
    
    cqm_write_options_t opts;
    uft_cqm_write_options_init(&opts);
    
    ASSERT_TRUE(opts.compress, "Default compress on");
    ASSERT_EQ(opts.compression_level, 6, "Default level 6");
    ASSERT_TRUE(opts.include_bpb, "Default include BPB");
    
    TEST_PASS();
}

/* ============================================================================
 * NIB TESTS
 * ============================================================================ */

static void test_nib_gcr_encode_decode(void) {
    TEST_START("nib_gcr_encode_decode");
    
    /* Test all valid 6-bit values */
    for (int i = 0; i < 64; i++) {
        uint8_t encoded = apple_gcr_encode(i);
        uint8_t decoded = apple_gcr_decode(encoded);
        
        ASSERT_TRUE(apple_gcr_valid(encoded), "Encoded should be valid GCR");
        ASSERT_EQ(decoded, i, "Decode should match original");
    }
    
    TEST_PASS();
}

static void test_nib_sector_encoding(void) {
    TEST_START("nib_sector_encoding");
    
    /* Test sector data encoding */
    uint8_t sector_data[256];
    for (int i = 0; i < 256; i++) {
        sector_data[i] = i;
    }
    
    uint8_t gcr_data[342];
    nib_encode_sector_data(sector_data, gcr_data);
    
    uint8_t decoded[256];
    int result = nib_decode_sector_data(gcr_data, decoded);
    
    ASSERT_EQ(result, 0, "Decode should succeed");
    ASSERT_EQ(memcmp(sector_data, decoded, 256), 0, "Data should match");
    
    TEST_PASS();
}

static void test_nib_options(void) {
    TEST_START("nib_options");
    
    nib_write_options_t opts;
    uft_nib_write_options_init(&opts);
    
    ASSERT_EQ(opts.tracks, 35, "Default 35 tracks");
    ASSERT_EQ(opts.volume, 254, "Default volume 254");
    ASSERT_TRUE(opts.sync_align, "Default sync align on");
    
    TEST_PASS();
}

static void test_nib_validation(void) {
    TEST_START("nib_validation");
    
    /* Invalid size */
    uint8_t small_buf[100];
    ASSERT_TRUE(!uft_nib_validate(small_buf, 100), "Too small invalid");
    
    /* Valid size but bad content */
    uint8_t *nib = calloc(1, NIB_FILE_SIZE_35);
    ASSERT_TRUE(!uft_nib_validate(nib, NIB_FILE_SIZE_35), "Zero content invalid");
    
    /* Fill with sync bytes - should be valid */
    memset(nib, 0xFF, NIB_FILE_SIZE_35);
    ASSERT_TRUE(uft_nib_validate(nib, NIB_FILE_SIZE_35), "Sync bytes valid");
    
    free(nib);
    TEST_PASS();
}

/* ============================================================================
 * G64 EXTENDED TESTS
 * ============================================================================ */

static void test_g64_error_map_init(void) {
    TEST_START("g64_error_map_init");
    
    g64_error_map_t map;
    g64_error_map_init(&map);
    
    ASSERT_EQ(memcmp(map.magic, "UFTX", 4), 0, "Magic should be UFTX");
    ASSERT_EQ(map.version, G64_EXT_VERSION, "Version should match");
    ASSERT_EQ(map.error_count, 0, "Error count should be 0");
    
    g64_error_map_free(&map);
    
    TEST_PASS();
}

static void test_g64_error_map_add(void) {
    TEST_START("g64_error_map_add");
    
    g64_error_map_t map;
    g64_error_map_init(&map);
    
    int result = g64_error_map_add(&map, 10, 5, G64_ERR_CRC, 200);
    ASSERT_EQ(result, 0, "Add should succeed");
    ASSERT_EQ(map.error_count, 1, "Count should be 1");
    
    g64_error_entry_t *entry = g64_error_map_get(&map, 10, 5);
    ASSERT_NOT_NULL(entry, "Entry should be found");
    ASSERT_EQ(entry->error_type, G64_ERR_CRC, "Type should match");
    ASSERT_EQ(entry->confidence, 200, "Confidence should match");
    
    /* Add more */
    g64_error_map_add(&map, 10, 6, G64_ERR_NO_DATA, 150);
    g64_error_map_add(&map, 11, 0, G64_ERR_WEAK_BITS, 100);
    
    ASSERT_EQ(map.error_count, 3, "Count should be 3");
    ASSERT_EQ(g64_error_map_count_track(&map, 10), 2, "Track 10 count");
    ASSERT_EQ(g64_error_map_count_track(&map, 11), 1, "Track 11 count");
    
    g64_error_map_free(&map);
    
    TEST_PASS();
}

static void test_g64_error_type_names(void) {
    TEST_START("g64_error_type_names");
    
    ASSERT_TRUE(strcmp(g64_error_type_name(G64_ERR_NONE), "None") == 0, "None");
    ASSERT_TRUE(strcmp(g64_error_type_name(G64_ERR_CRC), "CRC Error") == 0, "CRC");
    ASSERT_TRUE(strcmp(g64_error_type_name(G64_ERR_WEAK_BITS), "Weak Bits") == 0, "Weak");
    
    TEST_PASS();
}

static void test_g64_write_options(void) {
    TEST_START("g64_write_options");
    
    g64_write_options_t opts;
    uft_g64_write_options_init(&opts);
    
    ASSERT_TRUE(opts.include_error_map, "Default include errors");
    ASSERT_TRUE(opts.include_metadata, "Default include metadata");
    
    TEST_PASS();
}

/* ============================================================================
 * ADF PIPELINE TESTS
 * ============================================================================ */

static void test_adf_pipeline_options(void) {
    TEST_START("adf_pipeline_options");
    
    adf_pipeline_options_t opts;
    adf_pipeline_options_init(&opts);
    
    ASSERT_TRUE(opts.analyze_checksums, "Default analyze checksums");
    ASSERT_TRUE(opts.detect_weak_bits, "Default detect weak bits");
    ASSERT_EQ(opts.min_confidence, 80, "Default min confidence");
    
    TEST_PASS();
}

static void test_adf_pipeline_init(void) {
    TEST_START("adf_pipeline_init");
    
    adf_pipeline_ctx_t ctx;
    adf_pipeline_init(&ctx);
    
    ASSERT_EQ(ctx.stage, ADF_STAGE_INIT, "Initial stage");
    ASSERT_NULL(ctx.disk, "No disk initially");
    ASSERT_EQ(ctx.revision_count, 0, "No revisions");
    
    adf_pipeline_free(&ctx);
    
    TEST_PASS();
}

static void test_adf_checksum(void) {
    TEST_START("adf_checksum");
    
    /* Test known checksum */
    uint8_t data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint32_t cs = adf_checksum(data, 8);
    ASSERT_EQ(cs, 0xFFFFFFFF, "Zero data checksum");
    
    TEST_PASS();
}

static void test_adf_filesystem_detection(void) {
    TEST_START("adf_filesystem_detection");
    
    uint8_t boot[4] = {'D', 'O', 'S', 0x00};
    ASSERT_EQ(adf_detect_filesystem(boot), 0, "OFS");
    
    boot[3] = 0x01;
    ASSERT_EQ(adf_detect_filesystem(boot), 1, "FFS");
    
    ASSERT_TRUE(strcmp(adf_filesystem_name(0), "OFS") == 0, "OFS name");
    ASSERT_TRUE(strcmp(adf_filesystem_name(1), "FFS") == 0, "FFS name");
    
    TEST_PASS();
}

static void test_adf_validation(void) {
    TEST_START("adf_validation");
    
    /* Invalid size */
    uint8_t small_buf[100];
    ASSERT_TRUE(!adf_validate(small_buf, 100), "Too small invalid");
    
    /* Valid DD size */
    uint8_t *adf = malloc(ADF_FILE_SIZE_DD);
    memset(adf, 0, ADF_FILE_SIZE_DD);
    memcpy(adf, "DOS", 3);  /* Valid boot block */
    
    ASSERT_TRUE(adf_validate(adf, ADF_FILE_SIZE_DD), "DD size valid");
    
    free(adf);
    TEST_PASS();
}

/* ============================================================================
 * TD0 WRITER TESTS
 * ============================================================================ */

static void test_td0_options(void) {
    TEST_START("td0_options");
    
    uft_td0_write_options_t opts;
    uft_td0_write_options_init(&opts);
    
    ASSERT_TRUE(opts.compress, "Default compress on");
    ASSERT_EQ(opts.density, TD0_DENSITY_AUTO, "Default density auto");
    
    TEST_PASS();
}

static void test_td0_auto_settings(void) {
    TEST_START("td0_auto_settings");
    
    uft_td0_write_options_t opts;
    uft_td0_write_options_init(&opts);
    
    /* DD disk */
    uft_td0_auto_settings(&opts, 40, 2, 9, 512);
    ASSERT_EQ(opts.drive_type, TD0_DRIVE_525, "5.25\" drive");
    
    /* HD disk */
    uft_td0_auto_settings(&opts, 80, 2, 18, 512);
    ASSERT_EQ(opts.density, TD0_DENSITY_HD, "HD density");
    
    TEST_PASS();
}

/* ============================================================================
 * PROTECTION TESTS
 * ============================================================================ */

static void test_protection_type_names(void) {
    TEST_START("protection_type_names");
    
    ASSERT_TRUE(strcmp(protection_type_name(UFT_PROT_NONE), "None") == 0, "None");
    ASSERT_TRUE(strcmp(protection_type_name(UFT_PROT_VORPAL), "Vorpal") == 0, "Vorpal");
    ASSERT_TRUE(strcmp(protection_type_name(UFT_PROT_VMAX3), "V-Max v3") == 0, "V-Max");
    
    TEST_PASS();
}

static void test_protection_copy_strategy(void) {
    TEST_START("protection_copy_strategy");
    
    protection_copy_strategy_t s = get_copy_strategy(UFT_PROT_VORPAL);
    ASSERT_TRUE(s.use_flux_copy, "Vorpal needs flux");
    ASSERT_TRUE(s.preserve_timing, "Vorpal needs timing");
    ASSERT_TRUE(s.min_revisions >= 3, "Vorpal needs revisions");
    
    s = get_copy_strategy(UFT_PROT_NONE);
    ASSERT_TRUE(!s.use_flux_copy, "None doesn't need flux");
    
    TEST_PASS();
}

/* ============================================================================
 * TEST RUNNER
 * ============================================================================ */

static void run_unified_types_tests(void) {
    printf("\n=== UNIFIED TYPES TESTS ===\n");
    test_error_codes();
    test_sector_id();
    test_size_conversion();
    test_sector_alloc();
    test_track_alloc();
    test_disk_alloc();
    test_format_names();
}

static void run_fusion_tests(void) {
    printf("\n=== FUSION TESTS ===\n");
    test_fusion_options();
    test_fusion_single_revision();
    test_fusion_majority_voting();
    test_fusion_crc_weighting();
}

static void run_cqm_tests(void) {
    printf("\n=== CQM TESTS ===\n");
    test_cqm_header_validation();
    test_cqm_compression();
    test_cqm_options();
}

static void run_nib_tests(void) {
    printf("\n=== NIB TESTS ===\n");
    test_nib_gcr_encode_decode();
    test_nib_sector_encoding();
    test_nib_options();
    test_nib_validation();
}

static void run_g64_tests(void) {
    printf("\n=== G64 EXTENDED TESTS ===\n");
    test_g64_error_map_init();
    test_g64_error_map_add();
    test_g64_error_type_names();
    test_g64_write_options();
}

static void run_adf_tests(void) {
    printf("\n=== ADF PIPELINE TESTS ===\n");
    test_adf_pipeline_options();
    test_adf_pipeline_init();
    test_adf_checksum();
    test_adf_filesystem_detection();
    test_adf_validation();
}

static void run_td0_tests(void) {
    printf("\n=== TD0 WRITER TESTS ===\n");
    test_td0_options();
    test_td0_auto_settings();
}

static void run_protection_tests(void) {
    printf("\n=== PROTECTION TESTS ===\n");
    test_protection_type_names();
    test_protection_copy_strategy();
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("UFT P0/P1 Module Test Suite\n");
    printf("===========================\n");
    
    run_unified_types_tests();
    run_fusion_tests();
    run_cqm_tests();
    run_nib_tests();
    run_g64_tests();
    run_adf_tests();
    run_td0_tests();
    run_protection_tests();
    
    printf("\n===========================\n");
    printf("Tests: %d | Passed: %d | Failed: %d\n", 
           tests_run, tests_passed, tests_failed);
    printf("Coverage: %.1f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    
    return tests_failed > 0 ? 1 : 0;
}
