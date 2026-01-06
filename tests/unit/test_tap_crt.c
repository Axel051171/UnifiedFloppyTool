/**
 * @file test_tap_crt.c
 * @brief Unit Tests for TAP and CRT Formats
 * @version 1.0.0
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <uft/cbm/uft_tap.h>
#include <uft/cbm/uft_crt.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        return false; \
    } \
    tests_passed++; \
} while(0)

#define RUN_TEST(fn) do { \
    printf("Running %s...\n", #fn); \
    if (fn()) { \
        printf("  PASS\n"); \
    } else { \
        printf("  FAILED\n"); \
    } \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 * TAP Test Data
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Minimal TAP header */
static const uint8_t TAP_HEADER_V0[] = {
    'C', '6', '4', '-', 'T', 'A', 'P', 'E', '-', 'R', 'A', 'W',
    0x00,       /* Version 0 */
    0x00, 0x00, 0x00, /* Reserved */
    0x05, 0x00, 0x00, 0x00, /* Data size: 5 bytes */
    /* Pulse data */
    0x30, 0x40, 0x50, 0x60, 0x70
};

static const uint8_t TAP_HEADER_V1[] = {
    'C', '6', '4', '-', 'T', 'A', 'P', 'E', '-', 'R', 'A', 'W',
    0x01,       /* Version 1 */
    0x00, 0x00, 0x00, /* Reserved */
    0x08, 0x00, 0x00, 0x00, /* Data size: 8 bytes */
    /* Pulse data */
    0x30, 0x40, 
    0x00, 0x00, 0x10, 0x00, /* Extended: 0x001000 cycles */
    0x50, 0x60
};

/* ═══════════════════════════════════════════════════════════════════════════
 * CRT Test Data
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Minimal CRT header (64 bytes) + one CHIP packet */
static const uint8_t CRT_DATA[] = {
    /* Header (64 bytes) */
    'C', '6', '4', ' ', 'C', 'A', 'R', 'T', 'R', 'I', 'D', 'G', 'E', ' ', ' ', ' ',
    0x00, 0x00, 0x00, 0x40, /* Header length: 64 */
    0x01, 0x00,             /* Version 1.0 */
    0x00, 0x00,             /* Hardware type: Generic */
    0x01,                   /* EXROM: 1 */
    0x00,                   /* GAME: 0 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Reserved */
    'T', 'E', 'S', 'T', ' ', 'C', 'A', 'R', 'T', 0, 0, 0, 0, 0, 0, 0, /* Name */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* Name continued + reserved */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    
    /* CHIP packet */
    'C', 'H', 'I', 'P',
    0x00, 0x00, 0x00, 0x18, /* Packet length: 24 (16 header + 8 data) */
    0x00, 0x00,             /* Chip type: ROM */
    0x00, 0x00,             /* Bank: 0 */
    0x80, 0x00,             /* Load address: $8000 */
    0x00, 0x08,             /* ROM length: 8 bytes */
    /* ROM data */
    0x09, 0x80, 0x00, 0x00, 0x41, 0x30, 0xC3, 0xC2
};

/* ═══════════════════════════════════════════════════════════════════════════
 * TAP Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool test_tap_detect(void)
{
    TEST_ASSERT(uft_tap_detect(TAP_HEADER_V0, sizeof(TAP_HEADER_V0)),
                "detect v0 TAP");
    TEST_ASSERT(uft_tap_detect(TAP_HEADER_V1, sizeof(TAP_HEADER_V1)),
                "detect v1 TAP");
    TEST_ASSERT(!uft_tap_detect(NULL, 0), "NULL returns false");
    TEST_ASSERT(!uft_tap_detect((uint8_t*)"GARBAGE", 7), "garbage returns false");
    
    return true;
}

static bool test_tap_confidence(void)
{
    int conf_v0 = uft_tap_detect_confidence(TAP_HEADER_V0, sizeof(TAP_HEADER_V0));
    int conf_v1 = uft_tap_detect_confidence(TAP_HEADER_V1, sizeof(TAP_HEADER_V1));
    
    TEST_ASSERT(conf_v0 >= 80, "v0 high confidence");
    TEST_ASSERT(conf_v1 >= 80, "v1 high confidence");
    
    int conf_bad = uft_tap_detect_confidence((uint8_t*)"GARBAGE", 7);
    TEST_ASSERT(conf_bad == 0, "garbage zero confidence");
    
    return true;
}

static bool test_tap_open_v0(void)
{
    uft_tap_view_t view;
    uft_tap_status_t st = uft_tap_open(TAP_HEADER_V0, sizeof(TAP_HEADER_V0), &view);
    
    TEST_ASSERT(st == UFT_TAP_OK, "open v0 succeeds");
    TEST_ASSERT(view.header.version == 0, "version is 0");
    TEST_ASSERT(view.header.data_size == 5, "data size is 5");
    TEST_ASSERT(view.pulse_count == 5, "pulse count is 5");
    
    return true;
}

static bool test_tap_open_v1(void)
{
    uft_tap_view_t view;
    uft_tap_status_t st = uft_tap_open(TAP_HEADER_V1, sizeof(TAP_HEADER_V1), &view);
    
    TEST_ASSERT(st == UFT_TAP_OK, "open v1 succeeds");
    TEST_ASSERT(view.header.version == 1, "version is 1");
    TEST_ASSERT(view.header.data_size == 8, "data size is 8");
    
    return true;
}

static bool test_tap_iterate(void)
{
    uft_tap_view_t view;
    uft_tap_open(TAP_HEADER_V0, sizeof(TAP_HEADER_V0), &view);
    
    uft_tap_iter_t iter;
    uft_tap_iter_begin(&view, &iter);
    
    uft_tap_pulse_t pulse;
    int count = 0;
    
    while (uft_tap_iter_has_next(&iter)) {
        uft_tap_status_t st = uft_tap_iter_next(&iter, &pulse);
        TEST_ASSERT(st == UFT_TAP_OK, "iter_next succeeds");
        count++;
    }
    
    TEST_ASSERT(count == 5, "iterated 5 pulses");
    
    return true;
}

static bool test_tap_writer(void)
{
    uft_tap_writer_t writer;
    uft_tap_status_t st = uft_tap_writer_init(&writer, 1);
    TEST_ASSERT(st == UFT_TAP_OK, "writer init");
    
    /* Add some pulses */
    st = uft_tap_writer_add_pulse(&writer, 0x180);  /* Short */
    TEST_ASSERT(st == UFT_TAP_OK, "add pulse 1");
    
    st = uft_tap_writer_add_pulse(&writer, 0x200);  /* Medium */
    TEST_ASSERT(st == UFT_TAP_OK, "add pulse 2");
    
    st = uft_tap_writer_add_pulse(&writer, 0x10000); /* Long (extended) */
    TEST_ASSERT(st == UFT_TAP_OK, "add pulse 3");
    
    uint8_t* data;
    size_t size;
    st = uft_tap_writer_finish(&writer, &data, &size);
    TEST_ASSERT(st == UFT_TAP_OK, "writer finish");
    TEST_ASSERT(data != NULL, "data not NULL");
    TEST_ASSERT(size > UFT_TAP_HEADER_SIZE, "size > header");
    
    /* Verify we can read it back */
    uft_tap_view_t view;
    st = uft_tap_open(data, size, &view);
    TEST_ASSERT(st == UFT_TAP_OK, "can reopen");
    TEST_ASSERT(view.pulse_count == 3, "3 pulses");
    
    free(data);
    return true;
}

static bool test_tap_utilities(void)
{
    /* Cycle to microseconds */
    double us_pal = uft_tap_cycles_to_us(985248, UFT_TAP_PAL);
    TEST_ASSERT(us_pal > 999000 && us_pal < 1001000, "PAL 1 second");
    
    double us_ntsc = uft_tap_cycles_to_us(1022727, UFT_TAP_NTSC);
    TEST_ASSERT(us_ntsc > 999000 && us_ntsc < 1001000, "NTSC 1 second");
    
    /* Pulse classification */
    TEST_ASSERT(uft_tap_classify_pulse(300) == 0, "short pulse");
    TEST_ASSERT(uft_tap_classify_pulse(500) == 1, "medium pulse");
    TEST_ASSERT(uft_tap_classify_pulse(700) == 2, "long pulse");
    
    /* Machine names */
    TEST_ASSERT(strcmp(uft_tap_machine_name(UFT_TAP_C64), "C64") == 0, "C64 name");
    TEST_ASSERT(strcmp(uft_tap_video_name(UFT_TAP_PAL), "PAL") == 0, "PAL name");
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CRT Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool test_crt_parse(void)
{
    uft_crt_view_t view;
    uft_crt_status_t st = uft_crt_parse(CRT_DATA, sizeof(CRT_DATA), &view);
    
    TEST_ASSERT(st == UFT_CRT_OK, "parse succeeds");
    TEST_ASSERT(view.hdr.header_len == 0x40, "header len 64");
    TEST_ASSERT(view.hdr.version == 0x0100, "version 1.0");
    TEST_ASSERT(view.hdr.hw_type == 0, "hw type 0");
    TEST_ASSERT(view.hdr.exrom == 1, "exrom 1");
    TEST_ASSERT(view.hdr.game == 0, "game 0");
    
    return true;
}

static bool test_crt_chip_iterate(void)
{
    uft_crt_view_t view;
    uft_crt_parse(CRT_DATA, sizeof(CRT_DATA), &view);
    
    size_t cursor = view.chip_off;
    uft_crt_chip_view_t chip;
    
    uft_crt_status_t st = uft_crt_next_chip(&view, &cursor, &chip);
    TEST_ASSERT(st == UFT_CRT_OK, "first chip");
    TEST_ASSERT(chip.chip_hdr.chip_type == 0, "chip type ROM");
    TEST_ASSERT(chip.chip_hdr.bank == 0, "bank 0");
    TEST_ASSERT(chip.chip_hdr.load_addr == 0x8000, "load addr $8000");
    TEST_ASSERT(chip.chip_hdr.rom_len == 8, "rom len 8");
    TEST_ASSERT(chip.data_len == 8, "data len 8");
    
    /* Should return TRUNC (done) for next call */
    st = uft_crt_next_chip(&view, &cursor, &chip);
    TEST_ASSERT(st == UFT_CRT_E_TRUNC, "no more chips");
    
    return true;
}

static bool test_crt_invalid(void)
{
    uint8_t garbage[100];
    memset(garbage, 0, sizeof(garbage));
    
    uft_crt_view_t view;
    uft_crt_status_t st = uft_crt_parse(garbage, sizeof(garbage), &view);
    TEST_ASSERT(st == UFT_CRT_E_MAGIC, "garbage returns magic error");
    
    st = uft_crt_parse(NULL, 0, &view);
    TEST_ASSERT(st == UFT_CRT_E_INVALID, "NULL returns invalid");
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("═══════════════════════════════════════════════════════════════\n");
    printf(" UFT TAP & CRT Format Tests\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    printf("TAP Format Tests:\n");
    RUN_TEST(test_tap_detect);
    RUN_TEST(test_tap_confidence);
    RUN_TEST(test_tap_open_v0);
    RUN_TEST(test_tap_open_v1);
    RUN_TEST(test_tap_iterate);
    RUN_TEST(test_tap_writer);
    RUN_TEST(test_tap_utilities);
    
    printf("\nCRT Format Tests:\n");
    RUN_TEST(test_crt_parse);
    RUN_TEST(test_crt_chip_iterate);
    RUN_TEST(test_crt_invalid);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf(" Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
