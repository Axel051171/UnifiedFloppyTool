/**
 * @file test_kryoflux.c
 * @brief Unit tests for KryoFlux DTC wrapper
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "uft/hal/uft_kryoflux.h"

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Testing %s... ", #name); \
    test_##name(); \
    printf("OK\n"); \
} while(0)

TEST(config_create_destroy) {
    uft_kf_config_t *cfg = uft_kf_config_create();
    assert(cfg != NULL);
    uft_kf_config_destroy(cfg);
}

TEST(platform_presets) {
    /* All platform presets should have names */
    assert(uft_kf_platform_name(UFT_KF_PLATFORM_GENERIC) != NULL);
    assert(uft_kf_platform_name(UFT_KF_PLATFORM_AMIGA) != NULL);
    assert(uft_kf_platform_name(UFT_KF_PLATFORM_C64) != NULL);
    assert(uft_kf_platform_name(UFT_KF_PLATFORM_APPLE_II) != NULL);
    assert(uft_kf_platform_name(UFT_KF_PLATFORM_IBM_PC) != NULL);
    
    assert(strcmp(uft_kf_platform_name(UFT_KF_PLATFORM_AMIGA), "Amiga") == 0);
    assert(strcmp(uft_kf_platform_name(UFT_KF_PLATFORM_C64), "Commodore 64") == 0);
}

TEST(drive_presets) {
    assert(uft_kf_drive_name(UFT_KF_DRIVE_AUTO) != NULL);
    assert(uft_kf_drive_name(UFT_KF_DRIVE_35_DD) != NULL);
    assert(uft_kf_drive_name(UFT_KF_DRIVE_525_40) != NULL);
    
    assert(strstr(uft_kf_drive_name(UFT_KF_DRIVE_35_DD), "3.5") != NULL);
}

TEST(apply_platform_preset) {
    uft_kf_config_t *cfg = uft_kf_config_create();
    assert(cfg != NULL);
    
    /* Apply C64 preset */
    int result = uft_kf_apply_platform_preset(cfg, UFT_KF_PLATFORM_C64);
    assert(result == 0);
    
    /* Apply Amiga preset */
    result = uft_kf_apply_platform_preset(cfg, UFT_KF_PLATFORM_AMIGA);
    assert(result == 0);
    
    uft_kf_config_destroy(cfg);
}

TEST(apply_drive_preset) {
    uft_kf_config_t *cfg = uft_kf_config_create();
    assert(cfg != NULL);
    
    int result = uft_kf_apply_drive_preset(cfg, UFT_KF_DRIVE_35_DD);
    assert(result == 0);
    
    result = uft_kf_apply_drive_preset(cfg, UFT_KF_DRIVE_525_40);
    assert(result == 0);
    
    uft_kf_config_destroy(cfg);
}

TEST(set_track_range) {
    uft_kf_config_t *cfg = uft_kf_config_create();
    assert(cfg != NULL);
    
    /* Valid ranges */
    assert(uft_kf_set_track_range(cfg, 0, 79) == 0);
    assert(uft_kf_set_track_range(cfg, 0, 39) == 0);
    assert(uft_kf_set_track_range(cfg, 10, 40) == 0);
    
    /* Invalid ranges */
    assert(uft_kf_set_track_range(cfg, -1, 79) == -1);
    assert(uft_kf_set_track_range(cfg, 80, 90) == -1);
    assert(uft_kf_set_track_range(cfg, 50, 40) == -1);
    
    uft_kf_config_destroy(cfg);
}

TEST(set_side) {
    uft_kf_config_t *cfg = uft_kf_config_create();
    assert(cfg != NULL);
    
    assert(uft_kf_set_side(cfg, 0) == 0);
    assert(uft_kf_set_side(cfg, 1) == 0);
    assert(uft_kf_set_side(cfg, -1) == 0);  /* Both sides */
    
    assert(uft_kf_set_side(cfg, 2) == -1);  /* Invalid */
    assert(uft_kf_set_side(cfg, -2) == -1); /* Invalid */
    
    uft_kf_config_destroy(cfg);
}

TEST(set_revolutions) {
    uft_kf_config_t *cfg = uft_kf_config_create();
    assert(cfg != NULL);
    
    assert(uft_kf_set_revolutions(cfg, 1) == 0);
    assert(uft_kf_set_revolutions(cfg, 5) == 0);
    assert(uft_kf_set_revolutions(cfg, 10) == 0);
    
    assert(uft_kf_set_revolutions(cfg, 0) == -1);
    assert(uft_kf_set_revolutions(cfg, 11) == -1);
    
    uft_kf_config_destroy(cfg);
}

TEST(timing_conversion) {
    /* Test tick to time conversion */
    /* 24 ticks at ~24MHz = ~1µs = ~1000ns */
    double ns = uft_kf_ticks_to_ns(24);
    assert(ns > 990.0 && ns < 1010.0);
    
    /* 24027 ticks = ~1ms = ~1000µs */
    double us = uft_kf_ticks_to_us(24027);
    assert(us > 990.0 && us < 1010.0);
    
    /* Test time to tick conversion */
    uint32_t ticks = uft_kf_ns_to_ticks(1000.0);  /* 1µs = 1000ns */
    assert(ticks > 20 && ticks < 30);  /* Should be ~24 */
    
    /* Sample clock should be ~24MHz */
    double clock = uft_kf_get_sample_clock();
    assert(clock > 24000000.0 && clock < 25000000.0);
}

TEST(dtc_availability) {
    uft_kf_config_t *cfg = uft_kf_config_create();
    assert(cfg != NULL);
    
    /* DTC may or may not be available - just check no crash */
    bool available = uft_kf_is_available(cfg);
    (void)available;  /* May be true or false */
    
    const char *path = uft_kf_get_dtc_path(cfg);
    (void)path;  /* May be NULL or valid path */
    
    const char *error = uft_kf_get_error(cfg);
    assert(error != NULL);  /* Should always return valid string */
    
    uft_kf_config_destroy(cfg);
}

TEST(null_safety) {
    /* All functions should handle NULL gracefully */
    assert(uft_kf_set_dtc_path(NULL, "/path") == -1);
    assert(uft_kf_set_output_dir(NULL, "/path") == -1);
    assert(uft_kf_set_track_range(NULL, 0, 79) == -1);
    assert(uft_kf_set_side(NULL, 0) == -1);
    assert(uft_kf_set_revolutions(NULL, 2) == -1);
    assert(uft_kf_apply_platform_preset(NULL, UFT_KF_PLATFORM_AMIGA) == -1);
    assert(uft_kf_apply_drive_preset(NULL, UFT_KF_DRIVE_35_DD) == -1);
    
    assert(uft_kf_is_available(NULL) == false);
    assert(uft_kf_get_dtc_path(NULL) == NULL);
    
    const char *err = uft_kf_get_error(NULL);
    assert(err != NULL);
    
    /* Destroy NULL should not crash */
    uft_kf_config_destroy(NULL);
}

int main(void) {
    printf("=== KryoFlux DTC Wrapper Tests ===\n");
    
    RUN_TEST(config_create_destroy);
    RUN_TEST(platform_presets);
    RUN_TEST(drive_presets);
    RUN_TEST(apply_platform_preset);
    RUN_TEST(apply_drive_preset);
    RUN_TEST(set_track_range);
    RUN_TEST(set_side);
    RUN_TEST(set_revolutions);
    RUN_TEST(timing_conversion);
    RUN_TEST(dtc_availability);
    RUN_TEST(null_safety);
    
    printf("\nAll tests passed!\n");
    return 0;
}
