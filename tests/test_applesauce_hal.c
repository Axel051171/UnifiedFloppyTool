/**
 * @file test_applesauce_hal.c
 * @brief Tests for the Applesauce HAL scaffold (M3.3).
 *
 * Verifies the same honesty contract as test_xum1541_hal.c:
 *   (a) Pure utility (format_name, ticks↔ns, sample clock) is correct.
 *   (b) Lifecycle: config_create / destroy work without serial port.
 *   (c) Setters validate input ranges (side, revolutions, etc.).
 *   (d) Serial-touching stubs return -1 with useful error message.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include "uft/hal/uft_applesauce.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* ───────────────────────── Pure-utility lookups ──────────────────── */

TEST(sample_clock_is_8_mhz) {
    /* Header constant + getter agree: 8 MHz = 8e6 Hz. */
    ASSERT(uft_as_get_sample_clock() == 8000000.0);
    ASSERT(UFT_AS_SAMPLE_CLOCK == 8000000);
}

TEST(ticks_to_ns_at_125_per_tick) {
    /* 1 tick = 125 ns exact. */
    ASSERT(uft_as_ticks_to_ns(0) == 0.0);
    ASSERT(uft_as_ticks_to_ns(1) == 125.0);
    ASSERT(uft_as_ticks_to_ns(8) == 1000.0);     /* 8 ticks = 1 μs */
    ASSERT(uft_as_ticks_to_ns(8000) == 1000000.0); /* 8000 ticks = 1 ms */
}

TEST(ns_to_ticks_inverse) {
    /* Round-trip: ticks → ns → ticks should preserve value. */
    for (uint32_t t = 0; t <= 1000; t += 7) {
        uint32_t round = uft_as_ns_to_ticks(uft_as_ticks_to_ns(t));
        ASSERT(round == t);
    }
}

TEST(ns_to_ticks_handles_invalid_input) {
    ASSERT(uft_as_ns_to_ticks(-1.0) == 0);
    ASSERT(uft_as_ns_to_ticks(0.0) == 0);
    /* Saturate at UINT32_MAX rather than wrap. */
    ASSERT(uft_as_ns_to_ticks(1.0e30) == 0xFFFFFFFFu);
}

TEST(format_name_known_formats) {
    ASSERT(strcmp(uft_as_format_name(UFT_AS_FMT_AUTO),     "auto-detect") == 0);
    ASSERT(strstr(uft_as_format_name(UFT_AS_FMT_DOS33),    "DOS 3.3") != NULL);
    ASSERT(strstr(uft_as_format_name(UFT_AS_FMT_DOS32),    "DOS 3.2") != NULL);
    ASSERT(strstr(uft_as_format_name(UFT_AS_FMT_PRODOS),   "ProDOS")  != NULL);
    ASSERT(strstr(uft_as_format_name(UFT_AS_FMT_MAC_400K), "400K")    != NULL);
    ASSERT(strstr(uft_as_format_name(UFT_AS_FMT_MAC_800K), "800K")    != NULL);
    ASSERT(strstr(uft_as_format_name(UFT_AS_FMT_RAW),      "raw")     != NULL);
}

TEST(format_name_unknown_returns_unknown) {
    ASSERT(strcmp(uft_as_format_name((uft_as_format_t)999), "unknown") == 0);
}

/* ───────────────────────── Lifecycle / setters ──────────────────── */

TEST(config_create_and_destroy) {
    uft_as_config_t *cfg = uft_as_config_create();
    ASSERT(cfg != NULL);
    ASSERT(uft_as_is_connected(cfg) == false);
    uft_as_config_destroy(cfg);
    uft_as_config_destroy(NULL);  /* must be safe */
}

TEST(set_format_validates) {
    uft_as_config_t *cfg = uft_as_config_create();
    ASSERT(uft_as_set_format(cfg, UFT_AS_FMT_DOS33)  == 0);
    ASSERT(uft_as_set_format(cfg, UFT_AS_FMT_RAW)    == 0);
    ASSERT(uft_as_set_format(cfg, (uft_as_format_t)999) == -1);
    ASSERT(uft_as_set_format(NULL, UFT_AS_FMT_DOS33) == -1);
    uft_as_config_destroy(cfg);
}

TEST(set_drive_validates) {
    uft_as_config_t *cfg = uft_as_config_create();
    ASSERT(uft_as_set_drive(cfg, UFT_AS_DRIVE_525) == 0);
    ASSERT(uft_as_set_drive(cfg, UFT_AS_DRIVE_35)  == 0);
    ASSERT(uft_as_set_drive(cfg, (uft_as_drive_t)42) == -1);
    uft_as_config_destroy(cfg);
}

TEST(set_track_range_validates) {
    uft_as_config_t *cfg = uft_as_config_create();
    ASSERT(uft_as_set_track_range(cfg, 0, 34) == 0);  /* DOS 3.3 */
    ASSERT(uft_as_set_track_range(cfg, 0, 79) == 0);  /* 3.5"     */
    ASSERT(uft_as_set_track_range(cfg, -1, 0) == -1); /* below 0  */
    ASSERT(uft_as_set_track_range(cfg, 5, 4)  == -1); /* end<start */
    ASSERT(uft_as_set_track_range(cfg, 0, 80) == -1); /* above 79 */
    uft_as_config_destroy(cfg);
}

TEST(set_side_validates) {
    uft_as_config_t *cfg = uft_as_config_create();
    ASSERT(uft_as_set_side(cfg, 0)  == 0);
    ASSERT(uft_as_set_side(cfg, 1)  == 0);
    ASSERT(uft_as_set_side(cfg, 2)  == -1);
    ASSERT(uft_as_set_side(cfg, -1) == -1);
    uft_as_config_destroy(cfg);
}

TEST(set_revolutions_validates) {
    uft_as_config_t *cfg = uft_as_config_create();
    ASSERT(uft_as_set_revolutions(cfg, 1) == 0);
    ASSERT(uft_as_set_revolutions(cfg, 5) == 0);  /* protocol max */
    ASSERT(uft_as_set_revolutions(cfg, 0) == -1);
    ASSERT(uft_as_set_revolutions(cfg, 6) == -1);
    uft_as_config_destroy(cfg);
}

/* ───────────────────────── Stubs return error ────────────────────── */

TEST(open_returns_minus_one_with_error_msg) {
    uft_as_config_t *cfg = uft_as_config_create();
    ASSERT(uft_as_open(cfg, "/dev/ttyUSB0") == -1);
    ASSERT(strstr(uft_as_get_error(cfg), "serial") != NULL);
    /* Bad inputs return -1 (not 0). */
    ASSERT(uft_as_open(NULL, "/dev/ttyUSB0") == -1);
    ASSERT(uft_as_open(cfg, NULL)            == -1);
    uft_as_config_destroy(cfg);
}

TEST(detect_returns_zero_ports_safely) {
    char ports[4][64];
    int n = uft_as_detect(ports, 4);
    /* Stub: zero detected (no actual enumeration). NULL/zero size = -1. */
    ASSERT(n == 0);
    ASSERT(uft_as_detect(NULL, 4) == -1);
    ASSERT(uft_as_detect(ports, 0) == -1);
}

TEST(read_track_validates_args_before_stub) {
    uft_as_config_t *cfg = uft_as_config_create();
    uint32_t *flux = NULL;
    size_t n = 0;
    ASSERT(uft_as_read_track(cfg, -1, 0, &flux, &n) == -1);  /* track */
    ASSERT(uft_as_read_track(cfg, 0, 2, &flux, &n)  == -1);  /* side  */
    ASSERT(uft_as_read_track(cfg, 0, 0, NULL, &n)   == -1);  /* flux ptr */
    /* Valid args still hit not-implemented. */
    ASSERT(uft_as_read_track(cfg, 0, 0, &flux, &n) == -1);
    ASSERT(strstr(uft_as_get_error(cfg), "not implemented") != NULL);
    uft_as_config_destroy(cfg);
}

TEST(write_track_validates_args) {
    uft_as_config_t *cfg = uft_as_config_create();
    uint32_t flux[10] = {0};
    ASSERT(uft_as_write_track(cfg, 0, 0, flux, 0)  == -1);  /* count==0 */
    ASSERT(uft_as_write_track(cfg, -1, 0, flux, 5) == -1);
    ASSERT(uft_as_write_track(cfg, 0, 2, flux, 5)  == -1);
    ASSERT(uft_as_write_track(NULL, 0, 0, flux, 5) == -1);
    uft_as_config_destroy(cfg);
}

TEST(get_error_handles_null) {
    ASSERT(uft_as_get_error(NULL) != NULL);
    ASSERT(strstr(uft_as_get_error(NULL), "NULL") != NULL);
}

int main(void) {
    printf("=== Applesauce HAL scaffold tests ===\n");
    RUN(sample_clock_is_8_mhz);
    RUN(ticks_to_ns_at_125_per_tick);
    RUN(ns_to_ticks_inverse);
    RUN(ns_to_ticks_handles_invalid_input);
    RUN(format_name_known_formats);
    RUN(format_name_unknown_returns_unknown);
    RUN(config_create_and_destroy);
    RUN(set_format_validates);
    RUN(set_drive_validates);
    RUN(set_track_range_validates);
    RUN(set_side_validates);
    RUN(set_revolutions_validates);
    RUN(open_returns_minus_one_with_error_msg);
    RUN(detect_returns_zero_ports_safely);
    RUN(read_track_validates_args_before_stub);
    RUN(write_track_validates_args);
    RUN(get_error_handles_null);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
