/**
 * @file test_tap.c
 * @brief Unit tests for TAP Tape Format
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/c64/uft_tap.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(condition) do { \
    if (!(condition)) { \
        printf("FAILED at line %d: %s\n", __LINE__, #condition); \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT((x))
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/**
 * @brief Create minimal TAP image
 */
static uint8_t *create_test_tap(size_t *size)
{
    size_t total = TAP_HEADER_SIZE + 20;  /* Header + some pulses */
    uint8_t *data = calloc(1, total);
    
    /* Magic */
    memcpy(data, TAP_MAGIC, TAP_MAGIC_LEN);
    
    /* Header */
    data[12] = TAP_VERSION_1;   /* Version */
    data[13] = TAP_MACHINE_C64; /* Machine */
    data[14] = 0;               /* PAL */
    data[15] = 0;               /* Reserved */
    
    /* Data size (little endian) */
    data[16] = 20;
    data[17] = 0;
    data[18] = 0;
    data[19] = 0;
    
    /* Pulse data: mix of short, medium, long pulses */
    uint8_t *pulses = data + TAP_HEADER_SIZE;
    pulses[0] = 0x30;  /* Short */
    pulses[1] = 0x30;  /* Short */
    pulses[2] = 0x42;  /* Medium */
    pulses[3] = 0x42;  /* Medium */
    pulses[4] = 0x56;  /* Long */
    pulses[5] = 0x00;  /* Overflow marker */
    pulses[6] = 0x00;  /* Low byte */
    pulses[7] = 0x10;  /* Mid byte */
    pulses[8] = 0x00;  /* High byte = 0x001000 cycles */
    
    /* More normal pulses */
    for (int i = 9; i < 20; i++) {
        pulses[i] = 0x30;  /* Short pulses */
    }
    
    *size = total;
    return data;
}

/* ============================================================================
 * Unit Tests - Detection
 * ============================================================================ */

TEST(detect_valid)
{
    size_t size;
    uint8_t *data = create_test_tap(&size);
    
    ASSERT_TRUE(tap_detect(data, size));
    
    free(data);
}

TEST(detect_invalid)
{
    uint8_t data[100] = {0};
    ASSERT_FALSE(tap_detect(data, 100));
    ASSERT_FALSE(tap_detect(NULL, 100));
    ASSERT_FALSE(tap_detect(data, 10));  /* Too small */
}

TEST(validate_valid)
{
    size_t size;
    uint8_t *data = create_test_tap(&size);
    
    ASSERT_TRUE(tap_validate(data, size));
    
    free(data);
}

/* ============================================================================
 * Unit Tests - Image Management
 * ============================================================================ */

TEST(open_tap)
{
    size_t size;
    uint8_t *data = create_test_tap(&size);
    
    tap_image_t image;
    int ret = tap_open(data, size, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(image.data);
    ASSERT_EQ(image.header.version, TAP_VERSION_1);
    ASSERT_EQ(image.header.machine, TAP_MACHINE_C64);
    ASSERT_EQ(image.pulse_data_size, 20);
    
    tap_close(&image);
    free(data);
}

TEST(create_tap)
{
    tap_image_t image;
    int ret = tap_create(TAP_VERSION_1, TAP_MACHINE_C64, &image);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(image.data);
    ASSERT_EQ(image.header.version, TAP_VERSION_1);
    ASSERT_EQ(image.header.machine, TAP_MACHINE_C64);
    ASSERT_EQ(image.pulse_data_size, 0);
    
    tap_close(&image);
}

TEST(close_tap)
{
    tap_image_t image;
    tap_create(TAP_VERSION_0, TAP_MACHINE_C64, &image);
    
    tap_close(&image);
    
    ASSERT_EQ(image.data, NULL);
    ASSERT_EQ(image.size, 0);
}

/* ============================================================================
 * Unit Tests - Pulse Operations
 * ============================================================================ */

TEST(get_pulse_count)
{
    size_t size;
    uint8_t *data = create_test_tap(&size);
    
    tap_image_t image;
    tap_open(data, size, &image);
    
    size_t count = tap_get_pulse_count(&image);
    /* 5 normal + 1 overflow (4 bytes) + 11 normal = 17 pulses in 20 bytes */
    ASSERT(count > 0);
    
    tap_close(&image);
    free(data);
}

TEST(classify_pulse)
{
    ASSERT_EQ(tap_classify_pulse(0x30 * 8), PULSE_SHORT);
    ASSERT_EQ(tap_classify_pulse(0x42 * 8), PULSE_MEDIUM);
    ASSERT_EQ(tap_classify_pulse(0x56 * 8), PULSE_LONG);
    ASSERT_EQ(tap_classify_pulse(0x100000), PULSE_PAUSE);
}

TEST(read_pulse_cycles)
{
    size_t size;
    uint8_t *data = create_test_tap(&size);
    
    tap_image_t image;
    tap_open(data, size, &image);
    
    uint32_t cycles;
    int bytes;
    
    /* Normal pulse */
    int ret = tap_read_pulse_cycles(&image, 0, &cycles, &bytes);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(cycles, 0x30 * 8);
    ASSERT_EQ(bytes, 1);
    
    /* Overflow pulse at offset 5 */
    ret = tap_read_pulse_cycles(&image, 5, &cycles, &bytes);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(bytes, 4);
    ASSERT_EQ(cycles, 0x001000);
    
    tap_close(&image);
    free(data);
}

TEST(get_duration)
{
    size_t size;
    uint8_t *data = create_test_tap(&size);
    
    tap_image_t image;
    tap_open(data, size, &image);
    
    double duration = tap_get_duration(&image);
    ASSERT(duration > 0);
    
    tap_close(&image);
    free(data);
}

/* ============================================================================
 * Unit Tests - Analysis
 * ============================================================================ */

TEST(get_statistics)
{
    size_t size;
    uint8_t *data = create_test_tap(&size);
    
    tap_image_t image;
    tap_open(data, size, &image);
    
    int s, m, l, p;
    tap_get_statistics(&image, &s, &m, &l, &p);
    
    ASSERT(s > 0);  /* We have short pulses */
    ASSERT(m > 0);  /* We have medium pulses */
    
    tap_close(&image);
    free(data);
}

TEST(analyze_tap)
{
    size_t size;
    uint8_t *data = create_test_tap(&size);
    
    tap_image_t image;
    tap_open(data, size, &image);
    
    tap_analysis_t analysis;
    int ret = tap_analyze(&image, &analysis);
    
    ASSERT_EQ(ret, 0);
    ASSERT(analysis.total_pulses > 0);
    ASSERT(analysis.duration_seconds > 0);
    
    tap_free_analysis(&analysis);
    tap_close(&image);
    free(data);
}

/* ============================================================================
 * Unit Tests - TAP Creation
 * ============================================================================ */

TEST(add_pulse)
{
    tap_image_t image;
    tap_create(TAP_VERSION_1, TAP_MACHINE_C64, &image);
    
    int ret = tap_add_pulse(&image, 0x30 * 8);  /* Short pulse */
    ASSERT_EQ(ret, 0);
    
    ret = tap_add_pulse(&image, 0x42 * 8);  /* Medium pulse */
    ASSERT_EQ(ret, 0);
    
    ASSERT(image.pulse_data_size >= 2);
    
    tap_close(&image);
}

TEST(add_overflow_pulse)
{
    tap_image_t image;
    tap_create(TAP_VERSION_1, TAP_MACHINE_C64, &image);
    
    /* Add long pulse that requires overflow marker */
    int ret = tap_add_pulse(&image, 0x10000);
    ASSERT_EQ(ret, 0);
    
    /* Should use 4 bytes (overflow marker + 3 byte value) */
    ASSERT_EQ(image.pulse_data_size, 4);
    
    tap_close(&image);
}

TEST(add_pilot)
{
    tap_image_t image;
    tap_create(TAP_VERSION_1, TAP_MACHINE_C64, &image);
    
    int ret = tap_add_pilot(&image, 100, TAP_SHORT_PULSE * 8);
    ASSERT_EQ(ret, 0);
    
    ASSERT_EQ(image.pulse_data_size, 100);  /* 100 single-byte pulses */
    
    tap_close(&image);
}

TEST(add_data_byte)
{
    tap_image_t image;
    tap_create(TAP_VERSION_1, TAP_MACHINE_C64, &image);
    
    int ret = tap_add_data_byte(&image, 0xAA);  /* 10101010 */
    ASSERT_EQ(ret, 0);
    
    /* 8 bits = 8 pulses */
    ASSERT_EQ(image.pulse_data_size, 8);
    
    tap_close(&image);
}

/* ============================================================================
 * Unit Tests - Utilities
 * ============================================================================ */

TEST(version_name)
{
    ASSERT_STR_EQ(tap_version_name(TAP_VERSION_0), "v0 (Original)");
    ASSERT_STR_EQ(tap_version_name(TAP_VERSION_1), "v1 (Half-wave)");
    ASSERT_STR_EQ(tap_version_name(TAP_VERSION_2), "v2 (Extended)");
}

TEST(machine_name)
{
    ASSERT_STR_EQ(tap_machine_name(TAP_MACHINE_C64), "C64");
    ASSERT_STR_EQ(tap_machine_name(TAP_MACHINE_VIC20), "VIC-20");
    ASSERT_STR_EQ(tap_machine_name(TAP_MACHINE_C16), "C16/Plus4");
}

TEST(cycles_conversion)
{
    /* 985248 cycles = 1 second = 1000000 µs */
    double us = tap_cycles_to_us(985248);
    ASSERT(us > 999000 && us < 1001000);  /* ~1 second */
    
    uint32_t cycles = tap_us_to_cycles(1000000);
    ASSERT(cycles > 980000 && cycles < 990000);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== TAP Tape Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_valid);
    RUN_TEST(detect_invalid);
    RUN_TEST(validate_valid);
    
    printf("\nImage Management:\n");
    RUN_TEST(open_tap);
    RUN_TEST(create_tap);
    RUN_TEST(close_tap);
    
    printf("\nPulse Operations:\n");
    RUN_TEST(get_pulse_count);
    RUN_TEST(classify_pulse);
    RUN_TEST(read_pulse_cycles);
    RUN_TEST(get_duration);
    
    printf("\nAnalysis:\n");
    RUN_TEST(get_statistics);
    RUN_TEST(analyze_tap);
    
    printf("\nTAP Creation:\n");
    RUN_TEST(add_pulse);
    RUN_TEST(add_overflow_pulse);
    RUN_TEST(add_pilot);
    RUN_TEST(add_data_byte);
    
    printf("\nUtilities:\n");
    RUN_TEST(version_name);
    RUN_TEST(machine_name);
    RUN_TEST(cycles_conversion);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
