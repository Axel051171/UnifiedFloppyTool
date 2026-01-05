/**
 * @file test_uff_pipeline.c
 * @brief UFF Archive Pipeline Tests
 * 
 * HAFTUNGSMODUS: Roundtrip, Bit-Identical, Metadata Integrity
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "uft/archive/uff_container.h"

// ============================================================================
// TEST INFRASTRUCTURE
// ============================================================================

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN(name) do { \
    printf("  TEST: %-40s ", #name); \
    fflush(stdout); \
    tests_run++; \
    test_##name(); \
} while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; return; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; return; } while(0)
#define ASSERT(c, m) if (!(c)) FAIL(m)

// Test file paths
#define TEST_UFF "/tmp/test.uff"
#define TEST_SCP "/tmp/test.scp"
#define TEST_SCP_OUT "/tmp/test_out.scp"

// ============================================================================
// HELPER: Create fake SCP data
// ============================================================================

static uint8_t* create_fake_scp(size_t* size) {
    // Minimal SCP header + track data
    size_t scp_size = 0x10 + 168 * 4 + 35 * 2 * 8000;  // Header + offsets + data
    uint8_t* scp = calloc(1, scp_size);
    if (!scp) return NULL;
    
    // SCP header
    scp[0] = 'S'; scp[1] = 'C'; scp[2] = 'P'; scp[3] = 0;  // Magic
    scp[4] = 0x18;  // Version
    scp[5] = 0x20;  // Disk type (5.25" DD)
    scp[6] = 3;     // Revolutions
    scp[7] = 0;     // Start track
    scp[8] = 34;    // End track
    scp[9] = 0x00;  // Flags
    scp[10] = 0;    // Bit cell encoding
    scp[11] = 0;    // Heads
    scp[12] = 0;    // Resolution
    
    // Fill with pseudo-random flux data
    for (size_t i = 0x10; i < scp_size; i++) {
        scp[i] = (i * 31337) & 0xFF;
    }
    
    *size = scp_size;
    return scp;
}

// ============================================================================
// TEST: Container Creation
// ============================================================================

TEST(create_empty) {
    uff_container_t* uff = uff_create(TEST_UFF);
    ASSERT(uff != NULL, "Failed to create container");
    
    uff_write_options_t opts;
    uff_write_options_default(&opts);
    
    int result = uff_finalize(uff, &opts);
    ASSERT(result == UFF_OK, "Finalize failed");
    
    uff_close(uff);
    
    // Verify file exists
    FILE* fp = fopen(TEST_UFF, "rb");
    ASSERT(fp != NULL, "Output file not created");
    fclose(fp);
    
    PASS();
}

TEST(create_with_meta) {
    uff_container_t* uff = uff_create(TEST_UFF);
    ASSERT(uff != NULL, "Failed to create container");
    
    uff_meta_data_t meta = {0};
    meta.acquisition_time = time(NULL) * 1000000ULL;
    strncpy(meta.device_name, "Greaseweazle F7", sizeof(meta.device_name));
    strncpy(meta.firmware_ver, "1.0", sizeof(meta.firmware_ver));
    strncpy(meta.software_ver, "UFT 3.5.0", sizeof(meta.software_ver));
    meta.disk_type = UFF_DISK_525_DD;
    meta.tracks = 35;
    meta.sides = 1;
    meta.revolutions = 3;
    
    int result = uff_set_meta(uff, &meta);
    ASSERT(result == UFF_OK, "Set meta failed");
    
    uff_write_options_t opts;
    uff_write_options_default(&opts);
    
    result = uff_finalize(uff, &opts);
    ASSERT(result == UFF_OK, "Finalize failed");
    
    uff_close(uff);
    PASS();
}

// ============================================================================
// TEST: ORIG Chunk Embedding
// ============================================================================

TEST(embed_scp) {
    // Create fake SCP
    size_t scp_size;
    uint8_t* scp_data = create_fake_scp(&scp_size);
    ASSERT(scp_data != NULL, "Failed to create fake SCP");
    
    // Calculate original hash
    uint8_t orig_hash[32];
    uff_sha256(scp_data, scp_size, orig_hash);
    
    // Create UFF with embedded SCP
    uff_container_t* uff = uff_create(TEST_UFF);
    ASSERT(uff != NULL, "Failed to create container");
    
    uff_meta_data_t meta = {0};
    meta.disk_type = UFF_DISK_525_DD;
    meta.tracks = 35;
    meta.sides = 1;
    uff_set_meta(uff, &meta);
    
    int result = uff_embed_original(uff, scp_data, scp_size, UFF_ORIG_SCP);
    ASSERT(result == UFF_OK, "Embed failed");
    
    uff_write_options_t opts;
    uff_write_options_default(&opts);
    result = uff_finalize(uff, &opts);
    ASSERT(result == UFF_OK, "Finalize failed");
    uff_close(uff);
    
    // Verify embedded SCP matches
    uff = uff_open(TEST_UFF, UFF_VALID_BASIC);
    ASSERT(uff != NULL, "Failed to open UFF");
    
    size_t read_size;
    const uint8_t* read_data = uff_get_orig(uff, &read_size);
    ASSERT(read_data != NULL, "Failed to get ORIG");
    ASSERT(read_size == scp_size, "Size mismatch");
    
    // Verify byte-identical
    ASSERT(memcmp(read_data, scp_data, scp_size) == 0, "Data mismatch");
    
    uff_close(uff);
    free(scp_data);
    PASS();
}

// ============================================================================
// TEST: Roundtrip - SCP Byte-Identical
// ============================================================================

TEST(roundtrip_scp_identical) {
    // Create fake SCP
    size_t scp_size;
    uint8_t* scp_data = create_fake_scp(&scp_size);
    ASSERT(scp_data != NULL, "Failed to create fake SCP");
    
    // Original hash
    uint8_t orig_hash[32];
    uff_sha256(scp_data, scp_size, orig_hash);
    
    // Save original to file
    FILE* fp = fopen(TEST_SCP, "wb");
    ASSERT(fp != NULL, "Failed to create SCP file");
    fwrite(scp_data, 1, scp_size, fp);
    fclose(fp);
    
    // Create UFF
    uff_container_t* uff = uff_create(TEST_UFF);
    uff_embed_original(uff, scp_data, scp_size, UFF_ORIG_SCP);
    
    uff_meta_data_t meta = {0};
    uff_set_meta(uff, &meta);
    
    uff_write_options_t opts;
    uff_write_options_default(&opts);
    uff_finalize(uff, &opts);
    uff_close(uff);
    
    // Re-open and export SCP
    uff = uff_open(TEST_UFF, UFF_VALID_BASIC);
    ASSERT(uff != NULL, "Failed to re-open UFF");
    
    int result = uff_export_scp(uff, TEST_SCP_OUT);
    ASSERT(result == UFF_OK, "Export failed");
    
    uff_close(uff);
    
    // Read exported SCP and verify
    fp = fopen(TEST_SCP_OUT, "rb");
    ASSERT(fp != NULL, "Failed to open exported SCP");
    
    fseek(fp, 0, SEEK_END);
    long export_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    ASSERT(export_size == (long)scp_size, "Export size mismatch");
    
    uint8_t* export_data = malloc(export_size);
    fread(export_data, 1, export_size, fp);
    fclose(fp);
    
    // Verify byte-identical
    uint8_t export_hash[32];
    uff_sha256(export_data, export_size, export_hash);
    
    ASSERT(memcmp(orig_hash, export_hash, 32) == 0, "Hash mismatch - NOT byte-identical!");
    ASSERT(memcmp(scp_data, export_data, scp_size) == 0, "Data mismatch");
    
    free(scp_data);
    free(export_data);
    PASS();
}

// ============================================================================
// TEST: Hash Verification
// ============================================================================

TEST(hash_verification) {
    // Create UFF with data
    size_t scp_size;
    uint8_t* scp_data = create_fake_scp(&scp_size);
    ASSERT(scp_data != NULL, "Failed to create fake SCP");
    
    uff_container_t* uff = uff_create(TEST_UFF);
    uff_meta_data_t meta = {0};
    uff_set_meta(uff, &meta);
    uff_embed_original(uff, scp_data, scp_size, UFF_ORIG_SCP);
    
    uff_write_options_t opts;
    uff_write_options_default(&opts);
    uff_finalize(uff, &opts);
    uff_close(uff);
    
    // Open with hash verification
    uff = uff_open(TEST_UFF, UFF_VALID_STANDARD);
    ASSERT(uff != NULL, "Failed to open with hash verification");
    ASSERT(uff->hashes_verified == true, "Hashes not verified");
    
    uff_close(uff);
    free(scp_data);
    PASS();
}

TEST(detect_corruption) {
    // Create valid UFF
    size_t scp_size;
    uint8_t* scp_data = create_fake_scp(&scp_size);
    
    uff_container_t* uff = uff_create(TEST_UFF);
    uff_meta_data_t meta = {0};
    uff_set_meta(uff, &meta);
    uff_embed_original(uff, scp_data, scp_size, UFF_ORIG_SCP);
    
    uff_write_options_t opts;
    uff_write_options_default(&opts);
    uff_finalize(uff, &opts);
    uff_close(uff);
    free(scp_data);
    
    // Corrupt the file
    FILE* fp = fopen(TEST_UFF, "r+b");
    ASSERT(fp != NULL, "Failed to open for corruption");
    fseek(fp, 200, SEEK_SET);  // Corrupt some data
    uint8_t garbage[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    fwrite(garbage, 1, 4, fp);
    fclose(fp);
    
    // Try to open with hash verification - should fail
    uff = uff_open(TEST_UFF, UFF_VALID_STANDARD);
    // Note: may succeed at basic level, fail at hash level
    
    if (uff) {
        int result = uff_verify_hashes(uff);
        ASSERT(result != UFF_OK, "Corruption not detected");
        uff_close(uff);
    }
    
    PASS();
}

// ============================================================================
// TEST: Metadata Integrity
// ============================================================================

TEST(metadata_roundtrip) {
    // Create UFF with full metadata
    uff_container_t* uff = uff_create(TEST_UFF);
    ASSERT(uff != NULL, "Create failed");
    
    uff_meta_data_t meta = {0};
    meta.acquisition_time = 1704067200000000ULL;  // 2024-01-01 00:00:00
    meta.original_time = 1704067100000000ULL;
    strncpy(meta.original_name, "test_disk.scp", sizeof(meta.original_name));
    strncpy(meta.device_name, "Greaseweazle F7 Plus", sizeof(meta.device_name));
    strncpy(meta.firmware_ver, "v1.15", sizeof(meta.firmware_ver));
    strncpy(meta.software_ver, "UFT 3.5.0 HAFTUNG", sizeof(meta.software_ver));
    strncpy(meta.operator_id, "TESTER-001", sizeof(meta.operator_id));
    strncpy(meta.notes, "Test acquisition for unit tests", sizeof(meta.notes));
    meta.disk_type = UFF_DISK_525_DD;
    meta.write_protect = 1;
    meta.tracks = 35;
    meta.sides = 1;
    meta.revolutions = 5;
    
    // Set a known hash
    memset(meta.original_sha256, 0x42, 32);
    
    int result = uff_set_meta(uff, &meta);
    ASSERT(result == UFF_OK, "Set meta failed");
    
    uff_write_options_t opts;
    uff_write_options_default(&opts);
    result = uff_finalize(uff, &opts);
    uff_close(uff);
    
    // Re-open and verify metadata
    uff = uff_open(TEST_UFF, UFF_VALID_BASIC);
    ASSERT(uff != NULL, "Re-open failed");
    
    const uff_meta_data_t* read_meta = uff_get_meta(uff);
    ASSERT(read_meta != NULL, "Get meta failed");
    
    // Verify all fields
    ASSERT(read_meta->acquisition_time == meta.acquisition_time, "acquisition_time mismatch");
    ASSERT(read_meta->original_time == meta.original_time, "original_time mismatch");
    ASSERT(strcmp(read_meta->original_name, meta.original_name) == 0, "original_name mismatch");
    ASSERT(strcmp(read_meta->device_name, meta.device_name) == 0, "device_name mismatch");
    ASSERT(strcmp(read_meta->firmware_ver, meta.firmware_ver) == 0, "firmware_ver mismatch");
    ASSERT(strcmp(read_meta->software_ver, meta.software_ver) == 0, "software_ver mismatch");
    ASSERT(strcmp(read_meta->operator_id, meta.operator_id) == 0, "operator_id mismatch");
    ASSERT(strcmp(read_meta->notes, meta.notes) == 0, "notes mismatch");
    ASSERT(read_meta->disk_type == meta.disk_type, "disk_type mismatch");
    ASSERT(read_meta->write_protect == meta.write_protect, "write_protect mismatch");
    ASSERT(read_meta->tracks == meta.tracks, "tracks mismatch");
    ASSERT(read_meta->sides == meta.sides, "sides mismatch");
    ASSERT(read_meta->revolutions == meta.revolutions, "revolutions mismatch");
    ASSERT(memcmp(read_meta->original_sha256, meta.original_sha256, 32) == 0, "hash mismatch");
    
    uff_close(uff);
    PASS();
}

// ============================================================================
// TEST: CRC32 Implementation
// ============================================================================

TEST(crc32_known_values) {
    // Test against known CRC32 values
    uint8_t test1[] = "123456789";
    uint32_t crc1 = uff_crc32(test1, 9);
    ASSERT(crc1 == 0xCBF43926, "CRC32 test1 failed");
    
    uint8_t test2[] = "";
    uint32_t crc2 = uff_crc32(test2, 0);
    ASSERT(crc2 == 0x00000000, "CRC32 empty failed");
    
    uint8_t test3[256];
    for (int i = 0; i < 256; i++) test3[i] = i;
    uint32_t crc3 = uff_crc32(test3, 256);
    ASSERT(crc3 == 0x29058C73, "CRC32 0-255 failed");
    
    PASS();
}

// ============================================================================
// TEST: SHA256 Implementation
// ============================================================================

TEST(sha256_known_values) {
    uint8_t hash[32];
    
    // SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    uff_sha256((uint8_t*)"", 0, hash);
    uint8_t expected1[32] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
        0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };
    ASSERT(memcmp(hash, expected1, 32) == 0, "SHA-256 empty failed");
    
    // SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    uff_sha256((uint8_t*)"abc", 3, hash);
    uint8_t expected2[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    ASSERT(memcmp(hash, expected2, 32) == 0, "SHA-256 'abc' failed");
    
    PASS();
}

// ============================================================================
// TEST: Validation Levels
// ============================================================================

TEST(validation_levels) {
    // Create valid UFF
    size_t scp_size;
    uint8_t* scp_data = create_fake_scp(&scp_size);
    
    uff_container_t* uff = uff_create(TEST_UFF);
    uff_meta_data_t meta = {0};
    meta.tracks = 35;
    
    // Calculate and set correct original hash
    uff_sha256(scp_data, scp_size, meta.original_sha256);
    
    uff_set_meta(uff, &meta);
    uff_embed_original(uff, scp_data, scp_size, UFF_ORIG_SCP);
    
    uff_write_options_t opts;
    uff_write_options_default(&opts);
    uff_finalize(uff, &opts);
    uff_close(uff);
    free(scp_data);
    
    // Test Level 0: Basic
    uff = uff_open(TEST_UFF, UFF_VALID_BASIC);
    ASSERT(uff != NULL, "Level 0 failed");
    ASSERT(uff->validation_level >= 0, "Level 0 not set");
    uff_close(uff);
    
    // Test Level 1: Standard (with hash verification)
    uff = uff_open(TEST_UFF, UFF_VALID_STANDARD);
    ASSERT(uff != NULL, "Level 1 failed");
    ASSERT(uff->hashes_verified == true, "Level 1 hashes not verified");
    uff_close(uff);
    
    // Test Level 2: Full (with ORIG verification)
    uff = uff_open(TEST_UFF, UFF_VALID_FULL);
    ASSERT(uff != NULL, "Level 2 failed");
    uff_close(uff);
    
    PASS();
}

// ============================================================================
// TEST: Error Handling
// ============================================================================

TEST(error_invalid_magic) {
    // Create file with wrong magic
    FILE* fp = fopen(TEST_UFF, "wb");
    ASSERT(fp != NULL, "Failed to create test file");
    
    uint8_t bad_header[64] = {0};
    bad_header[0] = 'B'; bad_header[1] = 'A'; bad_header[2] = 'D'; bad_header[3] = 0;
    fwrite(bad_header, 1, 64, fp);
    fclose(fp);
    
    uff_container_t* uff = uff_open(TEST_UFF, UFF_VALID_BASIC);
    ASSERT(uff == NULL, "Should reject bad magic");
    
    PASS();
}

TEST(error_null_params) {
    ASSERT(uff_open(NULL, 0) == NULL, "Should reject NULL path");
    ASSERT(uff_create(NULL) == NULL, "Should reject NULL path");
    
    uff_container_t* uff = uff_create(TEST_UFF);
    ASSERT(uff_set_meta(NULL, NULL) == UFF_ERR_INVALID, "Should reject NULL params");
    ASSERT(uff_embed_original(NULL, NULL, 0, 0) == UFF_ERR_INVALID, "Should reject NULL params");
    
    uff_close(uff);
    PASS();
}

// ============================================================================
// TEST: TOC Integrity
// ============================================================================

TEST(toc_entries) {
    // Create UFF with multiple chunks
    size_t scp_size;
    uint8_t* scp_data = create_fake_scp(&scp_size);
    
    uff_container_t* uff = uff_create(TEST_UFF);
    uff_meta_data_t meta = {0};
    uff_set_meta(uff, &meta);
    uff_embed_original(uff, scp_data, scp_size, UFF_ORIG_SCP);
    
    uff_write_options_t opts;
    uff_write_options_default(&opts);
    uff_finalize(uff, &opts);
    uff_close(uff);
    free(scp_data);
    
    // Re-open and check TOC
    uff = uff_open(TEST_UFF, UFF_VALID_BASIC);
    ASSERT(uff != NULL, "Re-open failed");
    ASSERT(uff->toc != NULL, "TOC not loaded");
    ASSERT(uff->toc_count >= 3, "Should have at least 3 chunks (META, ORIG, HASH)");
    
    // Verify we can find each chunk
    ASSERT(uff_has_chunk(uff, UFF_CHUNK_META) == true, "META not found");
    ASSERT(uff_has_chunk(uff, UFF_CHUNK_ORIG) == true, "ORIG not found");
    ASSERT(uff_has_chunk(uff, UFF_CHUNK_HASH) == true, "HASH not found");
    
    uff_close(uff);
    PASS();
}

// ============================================================================
// TEST: Large File Handling
// ============================================================================

TEST(large_file) {
    // Create large SCP (10 MB)
    size_t large_size = 10 * 1024 * 1024;
    uint8_t* large_data = malloc(large_size);
    ASSERT(large_data != NULL, "Failed to allocate large buffer");
    
    // Fill with pattern
    for (size_t i = 0; i < large_size; i++) {
        large_data[i] = (i * 17 + i / 256) & 0xFF;
    }
    
    // Calculate hash
    uint8_t orig_hash[32];
    uff_sha256(large_data, large_size, orig_hash);
    
    // Create UFF
    uff_container_t* uff = uff_create(TEST_UFF);
    uff_meta_data_t meta = {0};
    memcpy(meta.original_sha256, orig_hash, 32);
    uff_set_meta(uff, &meta);
    uff_embed_original(uff, large_data, large_size, UFF_ORIG_SCP);
    
    uff_write_options_t opts;
    uff_write_options_default(&opts);
    uff_finalize(uff, &opts);
    uff_close(uff);
    
    // Re-open and verify
    uff = uff_open(TEST_UFF, UFF_VALID_FULL);
    ASSERT(uff != NULL, "Failed to open large UFF");
    
    size_t read_size;
    const uint8_t* read_data = uff_get_orig(uff, &read_size);
    ASSERT(read_size == large_size, "Large file size mismatch");
    
    // Verify data integrity
    uint8_t read_hash[32];
    uff_sha256(read_data, read_size, read_hash);
    ASSERT(memcmp(orig_hash, read_hash, 32) == 0, "Large file hash mismatch");
    
    uff_close(uff);
    free(large_data);
    PASS();
}

// ============================================================================
// CLEANUP
// ============================================================================

static void cleanup(void) {
    remove(TEST_UFF);
    remove(TEST_SCP);
    remove(TEST_SCP_OUT);
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         UFF ARCHIVE PIPELINE TESTS - HAFTUNGSMODUS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("Crypto Implementation:\n");
    RUN(crc32_known_values);
    RUN(sha256_known_values);
    
    printf("\nContainer Creation:\n");
    RUN(create_empty);
    RUN(create_with_meta);
    
    printf("\nORIG Chunk Handling:\n");
    RUN(embed_scp);
    
    printf("\nRoundtrip Tests:\n");
    RUN(roundtrip_scp_identical);
    
    printf("\nHash Verification:\n");
    RUN(hash_verification);
    RUN(detect_corruption);
    
    printf("\nMetadata Integrity:\n");
    RUN(metadata_roundtrip);
    
    printf("\nValidation Levels:\n");
    RUN(validation_levels);
    
    printf("\nTOC Integrity:\n");
    RUN(toc_entries);
    
    printf("\nError Handling:\n");
    RUN(error_invalid_magic);
    RUN(error_null_params);
    
    printf("\nLarge File Handling:\n");
    RUN(large_file);
    
    printf("\n═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         RESULTS: %d/%d passed, %d failed\n", tests_passed, tests_run, tests_failed);
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    cleanup();
    
    return tests_failed > 0 ? 1 : 0;
}
