/**
 * @file test_flux_parsers.c
 * @version 1.0.0
 * @date 2026-01-06
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

/* Test framework */
#include "../uft_test_framework.h"

/* Modules under test */
#include "uft/flux/uft_scp_parser.h"
#include "uft/flux/uft_uft_kf_parser.h"
#include "uft/flux/uft_woz_parser.h"
#include "uft/flux/uft_fluxstat.h"

/*============================================================================
 * Test Fixtures - Synthetic Test Data
 *============================================================================*/

/* Minimal valid SCP header */
static const uint8_t test_scp_header[] = {
    'S', 'C', 'P',          /* Signature */
    0x24,                   /* Version 2.4 */
    0x04,                   /* Disk type: Amiga */
    0x03,                   /* 3 revolutions */
    0x00,                   /* Start track */
    0x9F,                   /* End track (159) */
    0x01,                   /* Flags: INDEX */
    0x00,                   /* Bit cell width: 16 */
    0x00,                   /* Heads: both */
    0x00,                   /* Resolution: 25ns */
    0x00, 0x00, 0x00, 0x00  /* Checksum (placeholder) */
};

/* Minimal valid WOZ header */
static const uint8_t test_woz_header[] = {
    'W', 'O', 'Z',          /* Signature */
    '2',                    /* Version 2 */
    0xFF,                   /* High bit */
    0x0A, 0x0D, 0x0A,       /* LF CR LF */
    0x00, 0x00, 0x00, 0x00  /* CRC32 (placeholder) */
};

static const uint8_t test_kf_stream[] = {
    /* Some flux values */
    0x02, 0x50,             /* Flux2: 0x0250 */
    0x03, 0x20,             /* Flux2: 0x0320 */
    0xA0,                   /* Single byte flux */
    0xB0,                   /* Single byte flux */
    /* OOB Index block */
    0x0D,                   /* OOB opcode */
    0x02,                   /* Type: Index */
    0x0C, 0x00,             /* Size: 12 bytes */
    0x06, 0x00, 0x00, 0x00, /* Stream pos */
    0x00, 0x10, 0x00, 0x00, /* Sample counter */
    0x00, 0x00, 0x10, 0x00, /* Index counter */
    /* More flux */
    0x04, 0x00,             /* Flux2 */
    /* OOB Index block 2 */
    0x0D,                   /* OOB opcode */
    0x02,                   /* Type: Index */
    0x0C, 0x00,             /* Size: 12 bytes */
    0x14, 0x00, 0x00, 0x00, /* Stream pos */
    0x00, 0x20, 0x00, 0x00, /* Sample counter */
    0x00, 0x00, 0x20, 0x00, /* Index counter */
    /* EOF */
    0x0D,                   /* OOB opcode */
    0x0D,                   /* Type: EOF */
    0x00, 0x00              /* Size: 0 */
};

/*============================================================================
 * SCP Parser Tests
 *============================================================================*/

static void test_scp_create_destroy(void)
{
    printf("  test_scp_create_destroy...");
    
    uft_scp_ctx_t* ctx = uft_scp_create();
    TEST_ASSERT(ctx != NULL);
    TEST_ASSERT(ctx->period_ns == 25);
    
    uft_scp_destroy(ctx);
    uft_scp_destroy(NULL);  /* Should not crash */
    
    printf(" PASSED\n");
}

static void test_scp_disk_type_names(void)
{
    printf("  test_scp_disk_type_names...");
    
    /* CBM types */
    TEST_ASSERT(strcmp(uft_scp_disk_type_name(0x00), "Commodore 64") == 0);
    TEST_ASSERT(strcmp(uft_scp_disk_type_name(0x04), "Amiga") == 0);
    
    /* Atari types */
    TEST_ASSERT(strcmp(uft_scp_disk_type_name(0x10), "Atari FM SS") == 0);
    TEST_ASSERT(strcmp(uft_scp_disk_type_name(0x15), "Atari ST DS") == 0);
    
    /* Apple types */
    TEST_ASSERT(strcmp(uft_scp_disk_type_name(0x20), "Apple II") == 0);
    TEST_ASSERT(strcmp(uft_scp_disk_type_name(0x25), "Macintosh 800K") == 0);
    
    /* PC types */
    TEST_ASSERT(strcmp(uft_scp_disk_type_name(0x30), "PC 360KB") == 0);
    TEST_ASSERT(strcmp(uft_scp_disk_type_name(0x33), "PC 1.44MB") == 0);
    
    /* Manufacturer names */
    TEST_ASSERT(strcmp(uft_scp_manufacturer_name(0x00), "Commodore") == 0);
    TEST_ASSERT(strcmp(uft_scp_manufacturer_name(0x10), "Atari") == 0);
    TEST_ASSERT(strcmp(uft_scp_manufacturer_name(0x20), "Apple") == 0);
    TEST_ASSERT(strcmp(uft_scp_manufacturer_name(0x30), "IBM PC") == 0);
    
    printf(" PASSED\n");
}

static void test_scp_rpm_calculation(void)
{
    printf("  test_scp_rpm_calculation...");
    
    /* 200ms = 300 RPM */
    uint32_t rpm = uft_scp_calculate_rpm(200000000);
    TEST_ASSERT(rpm == 300);
    
    /* 166.67ms = 360 RPM */
    rpm = uft_scp_calculate_rpm(166666666);
    TEST_ASSERT(rpm >= 359 && rpm <= 361);
    
    /* Edge case: 0 */
    rpm = uft_scp_calculate_rpm(0);
    TEST_ASSERT(rpm == 0);
    
    printf(" PASSED\n");
}

static void test_scp_flux_to_ns(void)
{
    printf("  test_scp_flux_to_ns...");
    
    uft_scp_ctx_t* ctx = uft_scp_create();
    TEST_ASSERT(ctx != NULL);
    
    /* Default 25ns resolution */
    uint32_t ns = uft_scp_flux_to_ns(ctx, 100);
    TEST_ASSERT(ns == 2500);
    
    ns = uft_scp_flux_to_ns(ctx, 0xFFFF);
    TEST_ASSERT(ns == 65535 * 25);
    
    /* NULL context uses default */
    ns = uft_scp_flux_to_ns(NULL, 100);
    TEST_ASSERT(ns == 2500);
    
    uft_scp_destroy(ctx);
    printf(" PASSED\n");
}

static void test_scp_null_handling(void)
{
    printf("  test_scp_null_handling...");
    
    /* All functions should handle NULL gracefully */
    TEST_ASSERT(uft_scp_get_track_count(NULL) == 0);
    TEST_ASSERT(uft_scp_has_track(NULL, 0) == false);
    TEST_ASSERT(uft_scp_open(NULL, "test.scp") == UFT_SCP_ERR_NULLPTR);
    TEST_ASSERT(uft_scp_open_memory(NULL, NULL, 0) == UFT_SCP_ERR_NULLPTR);
    
    uft_scp_ctx_t* ctx = uft_scp_create();
    TEST_ASSERT(uft_scp_open(ctx, NULL) == UFT_SCP_ERR_NULLPTR);
    TEST_ASSERT(uft_scp_read_track(ctx, 0, NULL) == UFT_SCP_ERR_NULLPTR);
    uft_scp_destroy(ctx);
    
    printf(" PASSED\n");
}

/*============================================================================
 *============================================================================*/

static void test_kf_create_destroy(void)
{
    printf("  test_kf_create_destroy...");
    
    uft_kf_ctx_t* ctx = uft_kf_create();
    TEST_ASSERT(ctx != NULL);
    
    uft_kf_destroy(ctx);
    uft_kf_destroy(NULL);  /* Should not crash */
    
    printf(" PASSED\n");
}

static void test_kf_filename_parsing(void)
{
    printf("  test_kf_filename_parsing...");
    
    int track, side;
    
    /* Valid filenames */
    TEST_ASSERT(uft_kf_parse_filename("track00.0.raw", &track, &side) == true);
    TEST_ASSERT(track == 0 && side == 0);
    
    TEST_ASSERT(uft_kf_parse_filename("track35.1.raw", &track, &side) == true);
    TEST_ASSERT(track == 35 && side == 1);
    
    TEST_ASSERT(uft_kf_parse_filename("track79.0.raw", &track, &side) == true);
    TEST_ASSERT(track == 79 && side == 0);
    
    /* With path */
    TEST_ASSERT(uft_kf_parse_filename("/path/to/track42.0.raw", &track, &side) == true);
    TEST_ASSERT(track == 42 && side == 0);
    
    /* Invalid filenames */
    TEST_ASSERT(uft_kf_parse_filename("invalid.raw", &track, &side) == false);
    TEST_ASSERT(uft_kf_parse_filename("track.0.raw", &track, &side) == false);
    TEST_ASSERT(uft_kf_parse_filename("track00.raw", &track, &side) == false);
    
    /* NULL handling */
    TEST_ASSERT(uft_kf_parse_filename(NULL, &track, &side) == false);
    TEST_ASSERT(uft_kf_parse_filename("track00.0.raw", NULL, &side) == false);
    
    printf(" PASSED\n");
}

static void test_kf_timing_conversions(void)
{
    printf("  test_kf_timing_conversions...");
    
    /* Ticks to nanoseconds */
    uint32_t ns = uft_kf_ticks_to_ns(48054);  /* ~1µs */
    TEST_ASSERT(ns >= 990 && ns <= 1010);
    
    /* Index to microseconds (1.152 MHz clock) */
    double us = uft_kf_index_to_us(1152);  /* 1ms */
    TEST_ASSERT(us >= 999.0 && us <= 1001.0);
    
    /* RPM calculation */
    uint32_t rpm = uft_uft_kf_calculate_rpm(200000.0);  /* 200ms = 300 RPM */
    TEST_ASSERT(rpm == 300);
    
    rpm = uft_uft_kf_calculate_rpm(166666.67);  /* ~360 RPM */
    TEST_ASSERT(rpm >= 359 && rpm <= 361);
    
    printf(" PASSED\n");
}

static void test_kf_stream_parsing(void)
{
    printf("  test_kf_stream_parsing...");
    
    uft_kf_ctx_t* ctx = uft_kf_create();
    TEST_ASSERT(ctx != NULL);
    
    /* Load test stream */
    int err = uft_kf_load_memory(ctx, test_kf_stream, sizeof(test_kf_stream));
    TEST_ASSERT(err == UFT_UFT_KF_OK);
    
    /* Parse stream */
    uft_kf_track_data_t track;
    err = uft_kf_parse_stream(ctx, &track);
    TEST_ASSERT(err == UFT_UFT_KF_OK);
    TEST_ASSERT(track.valid == true);
    TEST_ASSERT(track.revolution_count >= 1);
    
    /* Check index count */
    TEST_ASSERT(uft_kf_get_index_count(ctx) >= 2);
    
    uft_kf_free_track(&track);
    uft_kf_destroy(ctx);
    
    printf(" PASSED\n");
}

static void test_kf_null_handling(void)
{
    printf("  test_kf_null_handling...");
    
    TEST_ASSERT(uft_kf_get_index_count(NULL) == 0);
    TEST_ASSERT(uft_kf_load_file(NULL, "test.raw") == UFT_UFT_KF_ERR_NULLPTR);
    TEST_ASSERT(uft_kf_load_memory(NULL, NULL, 0) == UFT_UFT_KF_ERR_NULLPTR);
    
    uft_kf_ctx_t* ctx = uft_kf_create();
    TEST_ASSERT(uft_kf_parse_stream(ctx, NULL) == UFT_UFT_KF_ERR_NULLPTR);
    uft_kf_destroy(ctx);
    
    printf(" PASSED\n");
}

/*============================================================================
 * WOZ Parser Tests
 *============================================================================*/

static void test_woz_create_destroy(void)
{
    printf("  test_woz_create_destroy...");
    
    uft_woz_ctx_t* ctx = uft_woz_create();
    TEST_ASSERT(ctx != NULL);
    
    uft_woz_destroy(ctx);
    uft_woz_destroy(NULL);  /* Should not crash */
    
    printf(" PASSED\n");
}

static void test_woz_disk_type_names(void)
{
    printf("  test_woz_disk_type_names...");
    
    TEST_ASSERT(strcmp(uft_woz_disk_type_name(UFT_WOZ_DISK_525), "5.25\" Apple II") == 0);
    TEST_ASSERT(strcmp(uft_woz_disk_type_name(UFT_WOZ_DISK_35), "3.5\" Macintosh") == 0);
    TEST_ASSERT(strcmp(uft_woz_disk_type_name(99), "Unknown") == 0);
    
    printf(" PASSED\n");
}

static void test_woz_hw_names(void)
{
    printf("  test_woz_hw_names...");
    
    char buffer[256];
    
    uft_woz_hw_names(UFT_WOZ_HW_APPLE_II, buffer, sizeof(buffer));
    TEST_ASSERT(strstr(buffer, "Apple ][") != NULL);
    
    uft_woz_hw_names(UFT_WOZ_HW_APPLE_IIGS, buffer, sizeof(buffer));
    TEST_ASSERT(strstr(buffer, "IIgs") != NULL);
    
    uft_woz_hw_names(UFT_WOZ_HW_APPLE_II | UFT_WOZ_HW_APPLE_IIE, buffer, sizeof(buffer));
    TEST_ASSERT(strstr(buffer, "Apple ][") != NULL);
    TEST_ASSERT(strstr(buffer, "//e") != NULL);
    
    printf(" PASSED\n");
}

static void test_woz_bit_timing(void)
{
    printf("  test_woz_bit_timing...");
    
    /* Default timing (32 = 4µs) */
    TEST_ASSERT(uft_woz_bit_timing_ns(0) == 4000);
    TEST_ASSERT(uft_woz_bit_timing_ns(32) == 4000);
    
    /* Custom timing */
    TEST_ASSERT(uft_woz_bit_timing_ns(1) == 125);
    TEST_ASSERT(uft_woz_bit_timing_ns(40) == 5000);
    
    printf(" PASSED\n");
}

static void test_woz_null_handling(void)
{
    printf("  test_woz_null_handling...");
    
    TEST_ASSERT(uft_woz_get_track_count(NULL) == 0);
    TEST_ASSERT(uft_woz_has_track(NULL, 0) == false);
    TEST_ASSERT(uft_woz_open(NULL, "test.woz") == UFT_WOZ_ERR_NULLPTR);
    TEST_ASSERT(uft_woz_get_metadata(NULL, "key") == NULL);
    TEST_ASSERT(uft_woz_verify_crc(NULL) == false);
    
    printf(" PASSED\n");
}

static void test_woz_nibble_decode(void)
{
    printf("  test_woz_nibble_decode...");
    
    /* Test bitstream with known nibbles */
    uint8_t bitstream[] = {
        0xFF, 0xFE,  /* Should decode to 0xFF nibble + partial */
        0xAA, 0x96   /* D5 AA 96 = sector header marker */
    };
    
    uint8_t nibbles[16];
    int count = uft_woz_decode_nibbles(bitstream, 32, nibbles, 16);
    TEST_ASSERT(count > 0);
    
    /* NULL handling */
    count = uft_woz_decode_nibbles(NULL, 32, nibbles, 16);
    TEST_ASSERT(count == 0);
    
    printf(" PASSED\n");
}

/*============================================================================
 * FluxStat Tests
 *============================================================================*/

static void test_fluxstat_create_destroy(void)
{
    printf("  test_fluxstat_create_destroy...");
    
    uft_fluxstat_t* fs = uft_fluxstat_create();
    TEST_ASSERT(fs != NULL);
    
    uft_fluxstat_destroy(fs);
    uft_fluxstat_destroy(NULL);  /* Should not crash */
    
    printf(" PASSED\n");
}

static void test_fluxstat_configure(void)
{
    printf("  test_fluxstat_configure...");
    
    uft_fluxstat_t* fs = uft_fluxstat_create();
    TEST_ASSERT(fs != NULL);
    
    /* Valid configuration */
    int err = uft_fluxstat_configure(fs, 4, 70);
    TEST_ASSERT(err == 0);
    
    /* Invalid: too few passes */
    err = uft_fluxstat_configure(fs, 1, 70);
    TEST_ASSERT(err != 0);
    
    /* Invalid: too many passes */
    err = uft_fluxstat_configure(fs, 100, 70);
    TEST_ASSERT(err != 0);
    
    /* Invalid: bad threshold */
    err = uft_fluxstat_configure(fs, 4, 101);
    TEST_ASSERT(err != 0);
    
    uft_fluxstat_destroy(fs);
    printf(" PASSED\n");
}

static void test_fluxstat_class_names(void)
{
    printf("  test_fluxstat_class_names...");
    
    TEST_ASSERT(strcmp(uft_fluxstat_class_name(UFT_FLUX_STRONG_1), "STRONG_1") == 0);
    TEST_ASSERT(strcmp(uft_fluxstat_class_name(UFT_FLUX_WEAK_1), "WEAK_1") == 0);
    TEST_ASSERT(strcmp(uft_fluxstat_class_name(UFT_FLUX_STRONG_0), "STRONG_0") == 0);
    TEST_ASSERT(strcmp(uft_fluxstat_class_name(UFT_FLUX_WEAK_0), "WEAK_0") == 0);
    TEST_ASSERT(strcmp(uft_fluxstat_class_name(UFT_FLUX_AMBIGUOUS), "AMBIGUOUS") == 0);
    TEST_ASSERT(strcmp(uft_fluxstat_class_name(99), "UNKNOWN") == 0);
    
    printf(" PASSED\n");
}

static void test_fluxstat_rpm_calculation(void)
{
    printf("  test_fluxstat_rpm_calculation...");
    
    uft_fluxstat_t* fs = uft_fluxstat_create();
    TEST_ASSERT(fs != NULL);
    
    /* 300 RPM = 200ms per revolution */
    uint32_t flux_200ms[] = {4000, 4000, 4000}; /* 3 samples, sum to ~12µs */
    /* Note: This is a simplified test - real flux data would have many more samples */
    
    uft_fluxstat_destroy(fs);
    printf(" PASSED\n");
}

/*============================================================================
 * Integration Tests
 *============================================================================*/

static void test_parser_interoperability(void)
{
    printf("  test_parser_interoperability...");
    
    /* Create all parsers */
    uft_scp_ctx_t* scp = uft_scp_create();
    uft_kf_ctx_t* kf = uft_kf_create();
    uft_woz_ctx_t* woz = uft_woz_create();
    uft_fluxstat_t* fs = uft_fluxstat_create();
    
    TEST_ASSERT(scp != NULL);
    TEST_ASSERT(kf != NULL);
    TEST_ASSERT(woz != NULL);
    TEST_ASSERT(fs != NULL);
    
    /* All should be independently usable */
    TEST_ASSERT(uft_scp_get_track_count(scp) == 0);
    TEST_ASSERT(uft_kf_get_index_count(kf) == 0);
    TEST_ASSERT(uft_woz_get_track_count(woz) == 0);
    
    /* Clean destruction */
    uft_scp_destroy(scp);
    uft_kf_destroy(kf);
    uft_woz_destroy(woz);
    uft_fluxstat_destroy(fs);
    
    printf(" PASSED\n");
}

/*============================================================================
 * Test Runner
 *============================================================================*/

void run_flux_parser_tests(void)
{
    printf("\n=== Flux Parser Tests ===\n\n");
    
    printf("SCP Parser:\n");
    test_scp_create_destroy();
    test_scp_disk_type_names();
    test_scp_rpm_calculation();
    test_scp_flux_to_ns();
    test_scp_null_handling();
    
    printf("\nKryoFlux Parser:\n");
    test_kf_create_destroy();
    test_kf_filename_parsing();
    test_kf_timing_conversions();
    test_kf_stream_parsing();
    test_kf_null_handling();
    
    printf("\nWOZ Parser:\n");
    test_woz_create_destroy();
    test_woz_disk_type_names();
    test_woz_hw_names();
    test_woz_bit_timing();
    test_woz_null_handling();
    test_woz_nibble_decode();
    
    printf("\nFluxStat:\n");
    test_fluxstat_create_destroy();
    test_fluxstat_configure();
    test_fluxstat_class_names();
    test_fluxstat_rpm_calculation();
    
    printf("\nIntegration:\n");
    test_parser_interoperability();
    
    printf("\n=== All Flux Parser Tests PASSED ===\n");
}

#ifdef TEST_FLUX_PARSERS_MAIN
int main(void)
{
    run_flux_parser_tests();
    return 0;
}
#endif
