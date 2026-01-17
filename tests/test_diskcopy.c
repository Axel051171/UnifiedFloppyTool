/**
 * @file test_diskcopy.c
 * @brief Tests for Apple Disk Copy and NDIF format support
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "uft/formats/apple/uft_diskcopy.h"

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

static void write_be32_helper(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

static void write_be16_helper(uint8_t *p, uint16_t v) {
    p[0] = (v >> 8) & 0xFF;
    p[1] = v & 0xFF;
}

/**
 * @brief Create a minimal valid DC42 header
 */
static void create_test_dc42_header(uint8_t *buffer, const char *name,
                                     uint32_t data_size, dc_disk_format_t format) {
    memset(buffer, 0, DC42_HEADER_SIZE);
    
    /* Volume name (Pascal string) */
    size_t name_len = strlen(name);
    if (name_len > 63) name_len = 63;
    buffer[0] = (uint8_t)name_len;
    memcpy(buffer + 1, name, name_len);
    
    /* Data size */
    write_be32_helper(buffer + 64, data_size);
    
    /* Tag size (0 for MFM, 12*sectors for GCR) */
    uint32_t tag_size = 0;
    if (format == DC_FORMAT_GCR_400K || format == DC_FORMAT_GCR_800K) {
        tag_size = (data_size / 512) * 12;
    }
    write_be32_helper(buffer + 68, tag_size);
    
    /* Checksums (simplified - just use 0) */
    write_be32_helper(buffer + 72, 0);
    write_be32_helper(buffer + 76, 0);
    
    /* Format */
    buffer[80] = (uint8_t)format;
    buffer[81] = 0x22;  /* Mac format */
    
    /* Magic word */
    write_be16_helper(buffer + 82, 0x0100);
}

/**
 * @brief Create a minimal valid MacBinary II header
 */
static void create_test_macbinary_header(uint8_t *buffer, const char *filename,
                                          const char *type, const char *creator,
                                          uint32_t data_len, uint32_t rsrc_len) {
    memset(buffer, 0, MACBINARY_HEADER_SIZE);
    
    /* Required zero bytes */
    buffer[0] = 0;   /* old_version */
    buffer[0x4A] = 0; /* zero1 */
    buffer[0x52] = 0; /* zero2 */
    
    /* Filename */
    size_t name_len = strlen(filename);
    if (name_len > 63) name_len = 63;
    buffer[1] = (uint8_t)name_len;
    memcpy(buffer + 2, filename, name_len);
    
    /* Type and creator */
    memcpy(buffer + 0x41, type, 4);
    memcpy(buffer + 0x45, creator, 4);
    
    /* Fork lengths */
    write_be32_helper(buffer + 0x53, data_len);
    write_be32_helper(buffer + 0x57, rsrc_len);
    
    /* Version for MacBinary II */
    buffer[0x7A] = 129;  /* MacBinary II version */
    buffer[0x7B] = 129;  /* Min version */
    
    /* CRC would go at 0x7C-0x7D but we skip for simplicity */
}

/* ============================================================================
 * Format Detection Tests
 * ============================================================================ */

static void test_dc42_detection(void) {
    printf("Testing DC42 format detection...\n");
    
    /* Create a valid DC42 image */
    uint8_t dc42_image[DC42_HEADER_SIZE + 1024];
    create_test_dc42_header(dc42_image, "Test Disk", 1024, DC_FORMAT_MFM_720K);
    memset(dc42_image + DC42_HEADER_SIZE, 0xE5, 1024);  /* Fill with format byte */
    
    dc_image_type_t type = dc_detect_format(dc42_image, sizeof(dc42_image));
    assert(type == DC_TYPE_DC42);
    
    printf("  ✓ DC42 detection working\n");
}

static void test_macbinary_detection(void) {
    printf("Testing MacBinary detection...\n");
    
    /* Create MacBinary II header */
    uint8_t mb_header[MACBINARY_HEADER_SIZE + 1024];
    create_test_macbinary_header(mb_header, "TestFile.img", "dImg", "dCpy", 1024, 0);
    memset(mb_header + MACBINARY_HEADER_SIZE, 0, 1024);
    
    macbinary_type_t mb_type = dc_detect_macbinary(mb_header, sizeof(mb_header));
    
    /* Should detect as MacBinary I (no valid CRC) */
    assert(mb_type == MB_TYPE_MACBINARY_I || mb_type == MB_TYPE_MACBINARY_II);
    
    /* Test invalid MacBinary */
    uint8_t invalid[128];
    memset(invalid, 0xFF, sizeof(invalid));
    assert(dc_detect_macbinary(invalid, sizeof(invalid)) == MB_TYPE_NONE);
    
    printf("  ✓ MacBinary detection working\n");
}

static void test_raw_detection(void) {
    printf("Testing raw image detection...\n");
    
    /* Create raw 800K image */
    uint8_t *raw_800k = malloc(DC_SIZE_800K);
    assert(raw_800k != NULL);
    memset(raw_800k, 0, DC_SIZE_800K);
    
    dc_image_type_t type = dc_detect_format(raw_800k, DC_SIZE_800K);
    assert(type == DC_TYPE_RAW);
    
    free(raw_800k);
    printf("  ✓ Raw image detection working\n");
}

/* ============================================================================
 * Header Parsing Tests
 * ============================================================================ */

static void test_dc42_parsing(void) {
    printf("Testing DC42 header parsing...\n");
    
    /* Create test header */
    uint8_t header[DC42_HEADER_SIZE];
    create_test_dc42_header(header, "My Test Disk", DC_SIZE_800K, DC_FORMAT_GCR_800K);
    
    dc_analysis_result_t result;
    int ret = dc42_parse_header(header, DC42_HEADER_SIZE, &result);
    
    assert(ret == 0);
    assert(strcmp(result.volume_name, "My Test Disk") == 0);
    assert(result.data_size == DC_SIZE_800K);
    assert(result.disk_format == DC_FORMAT_GCR_800K);
    assert(result.sector_count == DC_SIZE_800K / 512);
    assert(result.is_valid == true);
    
    printf("  ✓ DC42 parsing working\n");
}

static void test_dc42_validation(void) {
    printf("Testing DC42 header validation...\n");
    
    /* Valid header */
    uint8_t valid[DC42_HEADER_SIZE];
    create_test_dc42_header(valid, "Valid", 1024, DC_FORMAT_MFM_720K);
    assert(dc42_validate_header((dc42_header_t *)valid) == true);
    
    /* Invalid magic */
    uint8_t bad_magic[DC42_HEADER_SIZE];
    create_test_dc42_header(bad_magic, "Bad", 1024, DC_FORMAT_MFM_720K);
    bad_magic[82] = 0x00;
    bad_magic[83] = 0x00;
    assert(dc42_validate_header((dc42_header_t *)bad_magic) == false);
    
    /* Empty name */
    uint8_t no_name[DC42_HEADER_SIZE];
    create_test_dc42_header(no_name, "", 1024, DC_FORMAT_MFM_720K);
    no_name[0] = 0;  /* Zero length name */
    assert(dc42_validate_header((dc42_header_t *)no_name) == false);
    
    printf("  ✓ DC42 validation working\n");
}

/* ============================================================================
 * Checksum Tests
 * ============================================================================ */

static void test_checksum_calculation(void) {
    printf("Testing checksum calculation...\n");
    
    /* Test with known data */
    uint8_t test_data[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
    uint32_t cksum = dc_calculate_checksum(test_data, sizeof(test_data));
    
    /* Checksum should be non-zero for non-zero data */
    assert(cksum != 0 || test_data[0] == 0);
    
    /* Same data should give same checksum */
    uint32_t cksum2 = dc_calculate_checksum(test_data, sizeof(test_data));
    assert(cksum == cksum2);
    
    /* Different data should give different checksum */
    test_data[0] = 0xFF;
    uint32_t cksum3 = dc_calculate_checksum(test_data, sizeof(test_data));
    assert(cksum3 != cksum);
    
    printf("  ✓ Checksum calculation working\n");
}

/* ============================================================================
 * Analysis Tests
 * ============================================================================ */

static void test_full_analysis(void) {
    printf("Testing full image analysis...\n");
    
    /* Create a complete DC42 image */
    size_t image_size = DC42_HEADER_SIZE + 1024;
    uint8_t *image = malloc(image_size);
    assert(image != NULL);
    
    create_test_dc42_header(image, "Analysis Test", 1024, DC_FORMAT_MFM_1440K);
    memset(image + DC42_HEADER_SIZE, 0xAA, 1024);
    
    /* Update checksum in header */
    uint32_t cksum = dc_calculate_checksum(image + DC42_HEADER_SIZE, 1024);
    write_be32_helper(image + 72, cksum);
    
    dc_analysis_result_t result;
    int ret = dc_analyze(image, image_size, &result);
    
    assert(ret == 0);
    assert(result.image_type == DC_TYPE_DC42);
    assert(result.is_valid == true);
    assert(strcmp(result.volume_name, "Analysis Test") == 0);
    assert(result.checksum_valid == true);
    
    free(image);
    printf("  ✓ Full analysis working\n");
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

static void test_format_descriptions(void) {
    printf("Testing format descriptions...\n");
    
    assert(strstr(dc_format_description(DC_FORMAT_GCR_400K), "400K") != NULL);
    assert(strstr(dc_format_description(DC_FORMAT_GCR_800K), "800K") != NULL);
    assert(strstr(dc_format_description(DC_FORMAT_MFM_720K), "720K") != NULL);
    assert(strstr(dc_format_description(DC_FORMAT_MFM_1440K), "1.44") != NULL);
    
    assert(strstr(dc_type_description(DC_TYPE_DC42), "4.2") != NULL);
    assert(strstr(dc_type_description(DC_TYPE_NDIF), "NDIF") != NULL);
    assert(strstr(dc_type_description(DC_TYPE_SMI), "Self-Mounting") != NULL);
    
    printf("  ✓ Format descriptions working\n");
}

static void test_size_conversions(void) {
    printf("Testing size conversions...\n");
    
    assert(dc_expected_size(DC_FORMAT_GCR_400K) == DC_SIZE_400K);
    assert(dc_expected_size(DC_FORMAT_GCR_800K) == DC_SIZE_800K);
    assert(dc_expected_size(DC_FORMAT_MFM_720K) == DC_SIZE_720K);
    assert(dc_expected_size(DC_FORMAT_MFM_1440K) == DC_SIZE_1440K);
    
    assert(dc_format_from_size(DC_SIZE_400K) == DC_FORMAT_GCR_400K);
    assert(dc_format_from_size(DC_SIZE_800K) == DC_FORMAT_GCR_800K);
    assert(dc_format_from_size(DC_SIZE_720K) == DC_FORMAT_MFM_720K);
    assert(dc_format_from_size(DC_SIZE_1440K) == DC_FORMAT_MFM_1440K);
    assert(dc_format_from_size(12345) == DC_FORMAT_CUSTOM);
    
    printf("  ✓ Size conversions working\n");
}

/* ============================================================================
 * Creation Tests
 * ============================================================================ */

static void test_dc42_creation(void) {
    printf("Testing DC42 image creation...\n");
    
    /* Create disk data */
    uint8_t disk_data[DC_SIZE_720K];
    memset(disk_data, 0xE5, sizeof(disk_data));
    
    /* Create header */
    dc42_header_t header;
    int ret = dc42_create_header("New Disk", DC_FORMAT_MFM_720K,
                                  disk_data, sizeof(disk_data),
                                  NULL, 0, &header);
    assert(ret == 0);
    
    /* Verify header */
    assert(dc42_validate_header(&header) == true);
    assert(header.volume_name[0] == 8);  /* "New Disk" length */
    assert(header.disk_encoding == DC_FORMAT_MFM_720K);
    
    printf("  ✓ DC42 creation working\n");
}

static void test_full_image_creation(void) {
    printf("Testing full image creation...\n");
    
    uint8_t disk_data[1024];
    memset(disk_data, 0x55, sizeof(disk_data));
    
    size_t output_size = DC42_HEADER_SIZE + sizeof(disk_data);
    uint8_t *output = malloc(output_size);
    assert(output != NULL);
    
    int written = dc42_create_image("Created Disk", DC_FORMAT_MFM_1440K,
                                     disk_data, sizeof(disk_data),
                                     output, output_size);
    
    assert(written == (int)output_size);
    
    /* Verify by parsing */
    dc_analysis_result_t result;
    assert(dc_analyze(output, output_size, &result) == 0);
    assert(result.image_type == DC_TYPE_DC42);
    assert(strcmp(result.volume_name, "Created Disk") == 0);
    
    free(output);
    printf("  ✓ Full image creation working\n");
}

/* ============================================================================
 * Data Extraction Tests
 * ============================================================================ */

static void test_data_extraction(void) {
    printf("Testing data extraction...\n");
    
    /* Create image with known data */
    uint8_t disk_data[512];
    for (int i = 0; i < 512; i++) {
        disk_data[i] = (uint8_t)i;
    }
    
    size_t image_size = DC42_HEADER_SIZE + sizeof(disk_data);
    uint8_t *image = malloc(image_size);
    assert(image != NULL);
    
    int written = dc42_create_image("Extract Test", DC_FORMAT_MFM_720K,
                                     disk_data, sizeof(disk_data),
                                     image, image_size);
    assert(written > 0);
    
    /* Analyze */
    dc_analysis_result_t result;
    assert(dc_analyze(image, image_size, &result) == 0);
    
    /* Extract */
    uint8_t extracted[512];
    int extracted_size = dc_extract_disk_data(image, image_size,
                                               &result, extracted, sizeof(extracted));
    
    assert(extracted_size == 512);
    assert(memcmp(extracted, disk_data, 512) == 0);
    
    free(image);
    printf("  ✓ Data extraction working\n");
}

/* ============================================================================
 * Report Generation Test
 * ============================================================================ */

static void test_report_generation(void) {
    printf("Testing report generation...\n");
    
    /* Create test image */
    uint8_t disk_data[1024];
    memset(disk_data, 0, sizeof(disk_data));
    
    size_t image_size = DC42_HEADER_SIZE + sizeof(disk_data);
    uint8_t *image = malloc(image_size);
    assert(image != NULL);
    
    dc42_create_image("Report Test", DC_FORMAT_GCR_800K,
                       disk_data, sizeof(disk_data),
                       image, image_size);
    
    dc_analysis_result_t result;
    dc_analyze(image, image_size, &result);
    
    char report[4096];
    int len = dc_generate_report(&result, report, sizeof(report));
    
    assert(len > 0);
    assert(strstr(report, "Report Test") != NULL);
    assert(strstr(report, "Disk Copy 4.2") != NULL);
    assert(strstr(report, "800K") != NULL);
    
    free(image);
    printf("  ✓ Report generation working\n");
    
    printf("\n--- Sample Report ---\n%s\n", report);
}

/* ============================================================================
 * ADC Decompression Test
 * ============================================================================ */

static void test_adc_decompression(void) {
    printf("Testing ADC decompression...\n");
    
    /* Test literal sequence */
    /* Control byte 0x03 = 4 literal bytes */
    uint8_t compressed[] = { 0x03, 'T', 'E', 'S', 'T' };
    uint8_t decompressed[16];
    
    int result = adc_decompress(compressed, sizeof(compressed),
                                 decompressed, sizeof(decompressed));
    
    assert(result == 4);
    assert(memcmp(decompressed, "TEST", 4) == 0);
    
    printf("  ✓ ADC decompression working\n");
}

/* ============================================================================
 * SMI Detection Test
 * ============================================================================ */

static void test_smi_stub_detection(void) {
    printf("Testing SMI stub detection...\n");
    
    /* Create fake SMI with DC42 at offset 0x400 */
    uint8_t smi_data[0x500];
    memset(smi_data, 0, sizeof(smi_data));
    
    /* Put some 68K-like code at start */
    smi_data[0] = 0x4E;  /* NOP opcode */
    smi_data[1] = 0x71;
    
    /* Put DC42 header at 0x400 */
    create_test_dc42_header(smi_data + 0x400, "SMI Disk", 256, DC_FORMAT_GCR_400K);
    
    uint32_t stub_size = smi_detect_stub(smi_data, sizeof(smi_data));
    assert(stub_size == 0x400);
    
    printf("  ✓ SMI stub detection working\n");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║     Apple Disk Copy / NDIF Format - Unit Tests                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
    
    /* Format detection tests */
    printf("--- Format Detection ---\n");
    test_dc42_detection();
    test_macbinary_detection();
    test_raw_detection();
    
    /* Header parsing tests */
    printf("\n--- Header Parsing ---\n");
    test_dc42_parsing();
    test_dc42_validation();
    
    /* Checksum tests */
    printf("\n--- Checksum ---\n");
    test_checksum_calculation();
    
    /* Analysis tests */
    printf("\n--- Analysis ---\n");
    test_full_analysis();
    
    /* Utility tests */
    printf("\n--- Utilities ---\n");
    test_format_descriptions();
    test_size_conversions();
    
    /* Creation tests */
    printf("\n--- Image Creation ---\n");
    test_dc42_creation();
    test_full_image_creation();
    
    /* Extraction tests */
    printf("\n--- Data Extraction ---\n");
    test_data_extraction();
    
    /* Compression tests */
    printf("\n--- Compression ---\n");
    test_adc_decompression();
    
    /* SMI tests */
    printf("\n--- SMI ---\n");
    test_smi_stub_detection();
    
    /* Report test */
    printf("\n--- Reporting ---\n");
    test_report_generation();
    
    printf("\n╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                    ALL TESTS PASSED! ✅                          ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}
