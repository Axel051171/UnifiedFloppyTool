/**
 * @file test_libdsk_formats.c
 * @brief Tests for libdsk-derived format implementations
 * @version 3.9.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include format common header first */
#include "uft/uft_format_common.h"

/* Include format headers */
#include "uft/formats/uft_apridisk.h"
#include "uft/formats/uft_nanowasp.h"
#include "uft/formats/uft_qrst.h"
#include "uft/formats/uft_cfi.h"
#include "uft/formats/uft_ydsk.h"
#include "uft/formats/uft_simh.h"
#include "uft/formats/uft_cpm_defs.h"

/* Test utilities */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED at line %d: %s\n", __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

/* ============================================================================
 * ApriDisk Tests
 * ============================================================================ */

TEST(apridisk_signature) {
    /* Test signature detection */
    uint8_t valid_header[128];
    memset(valid_header, 0, sizeof(valid_header));
    memcpy(valid_header, APRIDISK_SIGNATURE, APRIDISK_SIGNATURE_LEN);
    
    int confidence = 0;
    ASSERT(uft_apridisk_probe(valid_header, sizeof(valid_header), &confidence) == true);
    ASSERT(confidence >= 90);
    
    /* Test invalid signature */
    uint8_t invalid_header[128] = {0};
    ASSERT(uft_apridisk_probe(invalid_header, sizeof(invalid_header), &confidence) == false);
}

TEST(apridisk_rle_compression) {
    /* Test RLE round-trip */
    uint8_t input[512];
    uint8_t compressed[1024];
    uint8_t decompressed[512];
    
    /* Create test pattern with runs */
    memset(input, 0xAA, 100);
    memset(input + 100, 0xBB, 200);
    memset(input + 300, 0xCC, 212);
    
    int comp_len = apridisk_rle_compress(input, sizeof(input), 
                                          compressed, sizeof(compressed));
    ASSERT(comp_len > 0);
    ASSERT((size_t)comp_len < sizeof(input));  /* Should compress */
    
    int decomp_len = apridisk_rle_decompress(compressed, comp_len,
                                              decompressed, sizeof(decompressed));
    ASSERT(decomp_len == sizeof(input));
    ASSERT(memcmp(input, decompressed, sizeof(input)) == 0);
}

/* ============================================================================
 * NanoWasp Tests
 * ============================================================================ */

TEST(nanowasp_signature) {
    uint8_t valid_header[80];
    memset(valid_header, 0, sizeof(valid_header));
    memcpy(valid_header, NANOWASP_SIGNATURE, NANOWASP_SIGNATURE_LEN);
    
    int confidence = 0;
    ASSERT(uft_nanowasp_probe(valid_header, sizeof(valid_header), &confidence) == true);
    ASSERT(confidence >= 90);
}

TEST(nanowasp_header_validation) {
    nanowasp_header_t header;
    memset(&header, 0, sizeof(header));
    
    /* Invalid without signature */
    ASSERT(uft_nanowasp_validate_header(&header) == false);
    
    /* Valid with signature */
    memcpy(header.signature, NANOWASP_SIGNATURE, NANOWASP_SIGNATURE_LEN);
    ASSERT(uft_nanowasp_validate_header(&header) == true);
}

/* ============================================================================
 * QRST Tests
 * ============================================================================ */

TEST(qrst_signature) {
    uint8_t valid_header[22];
    memset(valid_header, 0, sizeof(valid_header));
    memcpy(valid_header, QRST_SIGNATURE, QRST_SIGNATURE_LEN);
    
    int confidence = 0;
    ASSERT(uft_qrst_probe(valid_header, sizeof(valid_header), &confidence) == true);
    ASSERT(confidence >= 90);
}

TEST(qrst_rle_compression) {
    uint8_t input[256];
    uint8_t compressed[512];
    uint8_t decompressed[256];
    
    /* Create test pattern */
    memset(input, 0x55, 128);
    for (int i = 128; i < 256; i++) {
        input[i] = i & 0xFF;
    }
    
    int comp_len = qrst_rle_compress(input, sizeof(input),
                                      compressed, sizeof(compressed));
    ASSERT(comp_len > 0);
    
    int decomp_len = qrst_rle_decompress(compressed, comp_len,
                                          decompressed, sizeof(decompressed));
    ASSERT(decomp_len == sizeof(input));
    ASSERT(memcmp(input, decompressed, sizeof(input)) == 0);
}

/* ============================================================================
 * CFI Tests
 * ============================================================================ */

TEST(cfi_compression) {
    uint8_t input[4608];  /* 9 sectors * 512 bytes */
    uint8_t compressed[8192];
    uint8_t decompressed[4608];
    
    /* Create test track data */
    for (int s = 0; s < 9; s++) {
        memset(input + s * 512, s + 1, 512);
    }
    
    int comp_len = cfi_compress_track(input, sizeof(input),
                                       compressed, sizeof(compressed));
    ASSERT(comp_len > 0);
    ASSERT((size_t)comp_len < sizeof(input));  /* Repetitive data should compress */
    
    int decomp_len = cfi_decompress_track(compressed, comp_len,
                                           decompressed, sizeof(decompressed));
    ASSERT(decomp_len == sizeof(input));
    ASSERT(memcmp(input, decompressed, sizeof(input)) == 0);
}

TEST(cfi_write_options) {
    cfi_write_options_t opts;
    uft_cfi_write_options_init(&opts);
    
    ASSERT(opts.use_compression == true);
}

/* ============================================================================
 * YDSK Tests
 * ============================================================================ */

TEST(ydsk_signature) {
    uint8_t valid_header[128];
    memset(valid_header, 0, sizeof(valid_header));
    memcpy(valid_header, YDSK_SIGNATURE, YDSK_SIGNATURE_LEN);
    
    int confidence = 0;
    ASSERT(uft_ydsk_probe(valid_header, sizeof(valid_header), &confidence) == true);
    ASSERT(confidence >= 90);
}

TEST(ydsk_header_validation) {
    ydsk_header_t header;
    memset(&header, 0, sizeof(header));
    
    /* Invalid without signature */
    ASSERT(uft_ydsk_validate_header(&header) == false);
    
    /* Valid with signature */
    memcpy(header.signature, YDSK_SIGNATURE, YDSK_SIGNATURE_LEN);
    ASSERT(uft_ydsk_validate_header(&header) == true);
}

/* ============================================================================
 * SIMH Tests
 * ============================================================================ */

TEST(simh_detect_rx01) {
    /* RX01: 77 * 1 * 26 * 128 = 256256 bytes */
    simh_disk_type_t type = uft_simh_detect_type(256256);
    ASSERT(type == SIMH_DISK_RX01);
}

TEST(simh_detect_rx02) {
    /* RX02: 77 * 1 * 26 * 256 = 512512 bytes */
    simh_disk_type_t type = uft_simh_detect_type(512512);
    ASSERT(type == SIMH_DISK_RX02);
}

TEST(simh_detect_pc_formats) {
    ASSERT(uft_simh_detect_type(368640) == SIMH_DISK_PC_360K);
    ASSERT(uft_simh_detect_type(737280) == SIMH_DISK_PC_720K);
    ASSERT(uft_simh_detect_type(1228800) == SIMH_DISK_PC_1200K);
    ASSERT(uft_simh_detect_type(1474560) == SIMH_DISK_PC_1440K);
}

TEST(simh_geometry_lookup) {
    const simh_geometry_t *geom = uft_simh_get_geometry(SIMH_DISK_RX01);
    ASSERT(geom != NULL);
    ASSERT(geom->cylinders == 77);
    ASSERT(geom->heads == 1);
    ASSERT(geom->sectors == 26);
    ASSERT(geom->sector_size == 128);
}

TEST(simh_read_options) {
    simh_read_options_t opts;
    uft_simh_read_options_init(&opts);
    
    ASSERT(opts.disk_type == SIMH_DISK_UNKNOWN);
}

/* ============================================================================
 * CP/M Definitions Tests
 * ============================================================================ */

TEST(cpm_format_count) {
    size_t count = 0;
    const cpm_format_def_t **formats = uft_cpm_get_all_formats(&count);
    
    ASSERT(formats != NULL);
    ASSERT(count >= 25);  /* We defined at least 25 formats */
}

TEST(cpm_find_by_name) {
    const cpm_format_def_t *fmt = uft_cpm_find_format("kaypro-ii");
    ASSERT(fmt != NULL);
    ASSERT(strcmp(fmt->name, "kaypro-ii") == 0);
    ASSERT(fmt->cylinders == 40);
    ASSERT(fmt->heads == 1);
}

TEST(cpm_find_by_geometry) {
    /* Find IBM 8" SS SD by geometry */
    const cpm_format_def_t *fmt = uft_cpm_find_by_geometry(77, 1, 26, 128);
    ASSERT(fmt != NULL);
    ASSERT(strcmp(fmt->name, "ibm-8-sssd") == 0);
}

TEST(cpm_block_size_calculation) {
    const cpm_format_def_t *fmt = uft_cpm_find_format("ibm-8-sssd");
    ASSERT(fmt != NULL);
    
    uint16_t block_size = cpm_block_size(&fmt->dpb);
    ASSERT(block_size == 1024);  /* BSH=3 means 1K blocks */
}

TEST(cpm_amstrad_formats) {
    const cpm_format_def_t *pcw = uft_cpm_find_format("amstrad-pcw");
    const cpm_format_def_t *cpc = uft_cpm_find_format("amstrad-cpc-system");
    
    ASSERT(pcw != NULL);
    ASSERT(cpc != NULL);
    ASSERT(cpc->first_sector == 0x41);  /* Amstrad special sector numbering */
}

/* ============================================================================
 * Write Options Tests
 * ============================================================================ */

TEST(apridisk_write_options) {
    apridisk_write_options_t opts;
    uft_apridisk_write_options_init(&opts);
    
    ASSERT(opts.use_rle == true);
    ASSERT(opts.creator != NULL);
}

TEST(qrst_write_options) {
    qrst_write_options_t opts;
    uft_qrst_write_options_init(&opts);
    
    ASSERT(opts.use_compression == true);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("=== libdsk-derived Formats Test Suite ===\n\n");
    
    printf("ApriDisk Tests:\n");
    RUN_TEST(apridisk_signature);
    RUN_TEST(apridisk_rle_compression);
    RUN_TEST(apridisk_write_options);
    
    printf("\nNanoWasp Tests:\n");
    RUN_TEST(nanowasp_signature);
    RUN_TEST(nanowasp_header_validation);
    
    printf("\nQRST Tests:\n");
    RUN_TEST(qrst_signature);
    RUN_TEST(qrst_rle_compression);
    RUN_TEST(qrst_write_options);
    
    printf("\nCFI Tests:\n");
    RUN_TEST(cfi_compression);
    RUN_TEST(cfi_write_options);
    
    printf("\nYDSK Tests:\n");
    RUN_TEST(ydsk_signature);
    RUN_TEST(ydsk_header_validation);
    
    printf("\nSIMH Tests:\n");
    RUN_TEST(simh_detect_rx01);
    RUN_TEST(simh_detect_rx02);
    RUN_TEST(simh_detect_pc_formats);
    RUN_TEST(simh_geometry_lookup);
    RUN_TEST(simh_read_options);
    
    printf("\nCP/M Definitions Tests:\n");
    RUN_TEST(cpm_format_count);
    RUN_TEST(cpm_find_by_name);
    RUN_TEST(cpm_find_by_geometry);
    RUN_TEST(cpm_block_size_calculation);
    RUN_TEST(cpm_amstrad_formats);
    
    printf("\n=== Summary ===\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}
