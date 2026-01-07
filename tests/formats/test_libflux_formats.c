/**
 * @file test_libflux_formats.c
 * @brief Unit Tests for HxC Format Detection and GCR Codec
 * @version 1.0.0
 * @date 2026-01-06
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Test framework */
#include "../uft_test_framework.h"

/* Modules under test */
#include "uft/formats/uft_libflux_formats.h"
#include "uft/codec/uft_opencbm_gcr.h"

/*============================================================================
 * Test Data
 *============================================================================*/

/* D64 magic: No magic, size-based detection */
static const size_t D64_SIZE_35_TRACK = 174848;
static const size_t D64_SIZE_35_ERRORS = 175531;
static const size_t D64_SIZE_40_TRACK = 196608;

/* ADF magic: No magic, size-based detection */
static const size_t ADF_SIZE_DD = 901120;
static const size_t ADF_SIZE_HD = 1802240;

/* WOZ header */
static const uint8_t woz_magic[] = {'W', 'O', 'Z', '2', 0xFF, 0x0A, 0x0D, 0x0A};

/* SCP header */
static const uint8_t scp_magic[] = {'S', 'C', 'P'};

/* HFE header */
static const uint8_t hfe_v1_magic[] = {'H', 'X', 'C', 'P', 'I', 'C', 'F', 'E'};
static const uint8_t hfe_v3_magic[] = {'H', 'X', 'C', 'H', 'F', 'E', 'V', '3'};

/* IPF header */
static const uint8_t ipf_magic[] = {'C', 'A', 'P', 'S'};

/* G64 header */
static const uint8_t g64_magic[] = {'G', 'C', 'R', '-', '1', '5', '4', '1'};

/* IMD header */
static const uint8_t imd_magic[] = {'I', 'M', 'D', ' '};

/* STX header */
static const uint8_t stx_magic[] = {'R', 'S', 'Y', 0x00};

/*============================================================================
 * Format Detection Tests
 *============================================================================*/

static void test_format_detection_woz(void)
{
    printf("  test_format_detection_woz...");
    
    uint8_t data[256];
    memset(data, 0, sizeof(data));
    memcpy(data, woz_magic, sizeof(woz_magic));
    
    uft_libflux_format_t format;
    uint8_t confidence;
    int ret = uft_libflux_detect_format(data, sizeof(data), &format, &confidence);
    
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(format == UFT_LIBFLUX_FMT_WOZ_V2);
    TEST_ASSERT(confidence >= 90);
    
    printf(" PASSED\n");
}

static void test_format_detection_scp(void)
{
    printf("  test_format_detection_scp...");
    
    uint8_t data[256];
    memset(data, 0, sizeof(data));
    memcpy(data, scp_magic, sizeof(scp_magic));
    data[3] = 0x24;  /* Version 2.4 */
    
    uft_libflux_format_t format;
    uint8_t confidence;
    int ret = uft_libflux_detect_format(data, sizeof(data), &format, &confidence);
    
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(format == UFT_LIBFLUX_FMT_SCP);
    TEST_ASSERT(confidence >= 90);
    
    printf(" PASSED\n");
}

static void test_format_detection_hfe(void)
{
    printf("  test_format_detection_hfe...");
    
    /* HFE v1 */
    uint8_t data[256];
    memset(data, 0, sizeof(data));
    memcpy(data, hfe_v1_magic, sizeof(hfe_v1_magic));
    
    uft_libflux_format_t format;
    uint8_t confidence;
    int ret = uft_libflux_detect_format(data, sizeof(data), &format, &confidence);
    
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(format == UFT_LIBFLUX_FMT_HFE_V1);
    TEST_ASSERT(confidence >= 90);
    
    /* HFE v3 */
    memset(data, 0, sizeof(data));
    memcpy(data, hfe_v3_magic, sizeof(hfe_v3_magic));
    
    ret = uft_libflux_detect_format(data, sizeof(data), &format, &confidence);
    
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(format == UFT_LIBFLUX_FMT_HFE_V3);
    TEST_ASSERT(confidence >= 90);
    
    printf(" PASSED\n");
}

static void test_format_detection_ipf(void)
{
    printf("  test_format_detection_ipf...");
    
    uint8_t data[256];
    memset(data, 0, sizeof(data));
    memcpy(data, ipf_magic, sizeof(ipf_magic));
    
    uft_libflux_format_t format;
    uint8_t confidence;
    int ret = uft_libflux_detect_format(data, sizeof(data), &format, &confidence);
    
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(format == UFT_LIBFLUX_FMT_IPF);
    TEST_ASSERT(confidence >= 90);
    
    printf(" PASSED\n");
}

static void test_format_detection_g64(void)
{
    printf("  test_format_detection_g64...");
    
    uint8_t data[256];
    memset(data, 0, sizeof(data));
    memcpy(data, g64_magic, sizeof(g64_magic));
    
    uft_libflux_format_t format;
    uint8_t confidence;
    int ret = uft_libflux_detect_format(data, sizeof(data), &format, &confidence);
    
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(format == UFT_LIBFLUX_FMT_G64);
    TEST_ASSERT(confidence >= 90);
    
    printf(" PASSED\n");
}

static void test_format_detection_stx(void)
{
    printf("  test_format_detection_stx...");
    
    uint8_t data[256];
    memset(data, 0, sizeof(data));
    memcpy(data, stx_magic, sizeof(stx_magic));
    
    uft_libflux_format_t format;
    uint8_t confidence;
    int ret = uft_libflux_detect_format(data, sizeof(data), &format, &confidence);
    
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(format == UFT_LIBFLUX_FMT_STX);
    TEST_ASSERT(confidence >= 90);
    
    printf(" PASSED\n");
}

static void test_format_detection_imd(void)
{
    printf("  test_format_detection_imd...");
    
    uint8_t data[256];
    memset(data, 0, sizeof(data));
    memcpy(data, imd_magic, sizeof(imd_magic));
    
    uft_libflux_format_t format;
    uint8_t confidence;
    int ret = uft_libflux_detect_format(data, sizeof(data), &format, &confidence);
    
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(format == UFT_LIBFLUX_FMT_IMD);
    TEST_ASSERT(confidence >= 90);
    
    printf(" PASSED\n");
}

/*============================================================================
 * Format Name Tests
 *============================================================================*/

static void test_format_names(void)
{
    printf("  test_format_names...");
    
    TEST_ASSERT(strcmp(uft_libflux_format_name(UFT_LIBFLUX_FMT_WOZ_V1), "WOZ v1") == 0);
    TEST_ASSERT(strcmp(uft_libflux_format_name(UFT_LIBFLUX_FMT_WOZ_V2), "WOZ v2") == 0);
    TEST_ASSERT(strcmp(uft_libflux_format_name(UFT_LIBFLUX_FMT_SCP), "SuperCard Pro") == 0);
    TEST_ASSERT(strcmp(uft_libflux_format_name(UFT_LIBFLUX_FMT_IPF), "IPF (CAPS/SPS)") == 0);
    TEST_ASSERT(strcmp(uft_libflux_format_name(UFT_LIBFLUX_FMT_KRYOFLUX), "KryoFlux Stream") == 0);
    TEST_ASSERT(strcmp(uft_libflux_format_name(UFT_LIBFLUX_FMT_D64), "D64 (C64)") == 0);
    TEST_ASSERT(strcmp(uft_libflux_format_name(UFT_LIBFLUX_FMT_ADF), "ADF (Amiga)") == 0);
    TEST_ASSERT(strcmp(uft_libflux_format_name(UFT_LIBFLUX_FMT_HFE_V1), "HFE v1") == 0);
    
    /* Unknown format */
    TEST_ASSERT(strcmp(uft_libflux_format_name((uft_libflux_format_t)9999), "Unknown") == 0);
    
    printf(" PASSED\n");
}

/*============================================================================
 * Format Classification Tests
 *============================================================================*/

static void test_format_classification(void)
{
    printf("  test_format_classification...");
    
    /* Flux formats */
    TEST_ASSERT(uft_libflux_is_flux_format(UFT_LIBFLUX_FMT_SCP) == true);
    TEST_ASSERT(uft_libflux_is_flux_format(UFT_LIBFLUX_FMT_KRYOFLUX) == true);
    TEST_ASSERT(uft_libflux_is_flux_format(UFT_LIBFLUX_FMT_A2R) == true);
    
    /* Non-flux formats */
    TEST_ASSERT(uft_libflux_is_flux_format(UFT_LIBFLUX_FMT_D64) == false);
    TEST_ASSERT(uft_libflux_is_flux_format(UFT_LIBFLUX_FMT_ADF) == false);
    TEST_ASSERT(uft_libflux_is_flux_format(UFT_LIBFLUX_FMT_IMG) == false);
    
    /* Preservation formats */
    TEST_ASSERT(uft_libflux_is_preservation_format(UFT_LIBFLUX_FMT_SCP) == true);
    TEST_ASSERT(uft_libflux_is_preservation_format(UFT_LIBFLUX_FMT_IPF) == true);
    TEST_ASSERT(uft_libflux_is_preservation_format(UFT_LIBFLUX_FMT_KRYOFLUX) == true);
    TEST_ASSERT(uft_libflux_is_preservation_format(UFT_LIBFLUX_FMT_G64) == true);
    
    /* Non-preservation formats */
    TEST_ASSERT(uft_libflux_is_preservation_format(UFT_LIBFLUX_FMT_D64) == false);
    TEST_ASSERT(uft_libflux_is_preservation_format(UFT_LIBFLUX_FMT_ADF) == false);
    
    printf(" PASSED\n");
}

/*============================================================================
 * GCR Codec Tests
 *============================================================================*/

static void test_gcr_encode_decode(void)
{
    printf("  test_gcr_encode_decode...");
    
    /* Test 4-to-5 encoding */
    for (uint8_t i = 0; i < 16; i++) {
        uint8_t encoded = uft_gcr_encode_nibble(i);
        TEST_ASSERT(encoded != 0xFF);  /* Valid encoding */
        
        uint8_t decoded = uft_gcr_decode_nibble(encoded);
        TEST_ASSERT(decoded == i);  /* Round trip */
    }
    
    printf(" PASSED\n");
}

static void test_gcr_byte_encode_decode(void)
{
    printf("  test_gcr_byte_encode_decode...");
    
    /* Test byte encoding (produces 10 bits / 2 GCR nibbles) */
    uint8_t test_bytes[] = {0x00, 0x55, 0xAA, 0xFF, 0x12, 0x34};
    
    for (size_t i = 0; i < sizeof(test_bytes); i++) {
        uint8_t high = (test_bytes[i] >> 4) & 0x0F;
        uint8_t low = test_bytes[i] & 0x0F;
        
        uint8_t enc_high = uft_gcr_encode_nibble(high);
        uint8_t enc_low = uft_gcr_encode_nibble(low);
        
        uint8_t dec_high = uft_gcr_decode_nibble(enc_high);
        uint8_t dec_low = uft_gcr_decode_nibble(enc_low);
        
        uint8_t result = (dec_high << 4) | dec_low;
        TEST_ASSERT(result == test_bytes[i]);
    }
    
    printf(" PASSED\n");
}

static void test_gcr_block_encode(void)
{
    printf("  test_gcr_block_encode...");
    
    /* Test block encoding (4 bytes -> 5 GCR bytes) */
    uint8_t input[4] = {0x12, 0x34, 0x56, 0x78};
    uint8_t gcr_output[5];
    
    int ret = uft_gcr_encode_block(input, gcr_output);
    TEST_ASSERT(ret == 0);
    
    /* Decode and verify */
    uint8_t decoded[4];
    ret = uft_gcr_decode_block(gcr_output, decoded);
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(memcmp(input, decoded, 4) == 0);
    
    printf(" PASSED\n");
}

static void test_gcr_sector_header(void)
{
    printf("  test_gcr_sector_header...");
    
    /* Create a sector header */
    uft_gcr_sector_header_t header;
    header.track = 18;
    header.sector = 0;
    header.id1 = 0x41;  /* 'A' */
    header.id2 = 0x42;  /* 'B' */
    
    /* Encode header */
    uint8_t gcr_header[10];
    int ret = uft_gcr_encode_sector_header(&header, gcr_header);
    TEST_ASSERT(ret == 0);
    
    /* Decode and verify */
    uft_gcr_sector_header_t decoded;
    ret = uft_gcr_decode_sector_header(gcr_header, &decoded);
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(decoded.track == 18);
    TEST_ASSERT(decoded.sector == 0);
    TEST_ASSERT(decoded.id1 == 0x41);
    TEST_ASSERT(decoded.id2 == 0x42);
    
    printf(" PASSED\n");
}

static void test_gcr_checksum(void)
{
    printf("  test_gcr_checksum...");
    
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    uint8_t checksum = uft_gcr_checksum(data, sizeof(data));
    
    /* XOR checksum should be reproducible */
    uint8_t expected = 0;
    for (size_t i = 0; i < sizeof(data); i++) {
        expected ^= data[i];
    }
    TEST_ASSERT(checksum == expected);
    
    /* Verify checksum */
    bool valid = uft_gcr_verify_checksum(data, sizeof(data), checksum);
    TEST_ASSERT(valid == true);
    
    /* Invalid checksum */
    valid = uft_gcr_verify_checksum(data, sizeof(data), checksum ^ 0xFF);
    TEST_ASSERT(valid == false);
    
    printf(" PASSED\n");
}

static void test_gcr_null_handling(void)
{
    printf("  test_gcr_null_handling...");
    
    uint8_t buf[16];
    
    /* NULL handling */
    TEST_ASSERT(uft_gcr_encode_block(NULL, buf) != 0);
    TEST_ASSERT(uft_gcr_encode_block(buf, NULL) != 0);
    TEST_ASSERT(uft_gcr_decode_block(NULL, buf) != 0);
    TEST_ASSERT(uft_gcr_decode_block(buf, NULL) != 0);
    
    uft_gcr_sector_header_t header;
    TEST_ASSERT(uft_gcr_encode_sector_header(NULL, buf) != 0);
    TEST_ASSERT(uft_gcr_encode_sector_header(&header, NULL) != 0);
    TEST_ASSERT(uft_gcr_decode_sector_header(NULL, &header) != 0);
    TEST_ASSERT(uft_gcr_decode_sector_header(buf, NULL) != 0);
    
    printf(" PASSED\n");
}

/*============================================================================
 * Test Runner
 *============================================================================*/

void run_libflux_format_tests(void)
{
    printf("\n=== HxC Format Tests ===\n\n");
    
    printf("Format Detection:\n");
    test_format_detection_woz();
    test_format_detection_scp();
    test_format_detection_hfe();
    test_format_detection_ipf();
    test_format_detection_g64();
    test_format_detection_stx();
    test_format_detection_imd();
    
    printf("\nFormat Names:\n");
    test_format_names();
    
    printf("\nFormat Classification:\n");
    test_format_classification();
    
    printf("\n=== GCR Codec Tests ===\n\n");
    
    printf("GCR Encoding:\n");
    test_gcr_encode_decode();
    test_gcr_byte_encode_decode();
    test_gcr_block_encode();
    test_gcr_sector_header();
    test_gcr_checksum();
    test_gcr_null_handling();
    
    printf("\n=== All HxC Format & GCR Tests PASSED ===\n");
}

#ifdef TEST_HXC_FORMATS_MAIN
int main(void)
{
    run_libflux_format_tests();
    return 0;
}
#endif
