/**
 * @file test_hal_all.c
 * @brief Comprehensive HAL Unit Tests
 * 
 * Tests all controller APIs (no actual hardware required).
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "uft/hal/uft_hal.h"
#include "uft/hal/uft_kryoflux.h"
#include "uft/hal/uft_supercard.h"
#include "uft/hal/uft_xum1541.h"
#include "uft/hal/uft_fc5025.h"
#include "uft/hal/uft_applesauce.h"

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %-40s", #name); \
    test_##name(); \
    printf("OK\n"); \
} while(0)

/*============================================================================
 * HAL UNIFIED TESTS
 *============================================================================*/

TEST(hal_controller_count) {
    int count = uft_hal_get_controller_count();
    assert(count == HAL_CTRL_COUNT);
    assert(count >= 8);
}

TEST(hal_controller_names) {
    for (int i = 0; i < HAL_CTRL_COUNT; i++) {
        const char *name = uft_hal_controller_name((uft_hal_controller_t)i);
        assert(name != NULL);
        assert(strlen(name) > 0);
    }
}

TEST(hal_null_safety) {
    assert(uft_hal_get_caps(NULL, NULL) == -1);
    assert(uft_hal_read_flux(NULL, 0, 0, 1, NULL, NULL) == -1);
    assert(uft_hal_write_flux(NULL, 0, 0, NULL, 0) == -1);
    uft_hal_close(NULL);  /* Should not crash */
}

/*============================================================================
 * KRYOFLUX TESTS
 *============================================================================*/

TEST(kf_config_lifecycle) {
    uft_kf_config_t *cfg = uft_kf_config_create();
    assert(cfg != NULL);
    uft_kf_config_destroy(cfg);
}

TEST(kf_platform_presets) {
    uft_kf_config_t *cfg = uft_kf_config_create();
    
    assert(uft_kf_apply_platform_preset(cfg, UFT_KF_PLATFORM_AMIGA) == 0);
    assert(uft_kf_apply_platform_preset(cfg, UFT_KF_PLATFORM_C64) == 0);
    assert(uft_kf_apply_platform_preset(cfg, UFT_KF_PLATFORM_APPLE_II) == 0);
    
    uft_kf_config_destroy(cfg);
}

TEST(kf_timing) {
    double ns = uft_kf_ticks_to_ns(24);
    assert(ns > 990.0 && ns < 1010.0);
    
    double clock = uft_kf_get_sample_clock();
    assert(clock > 24000000.0 && clock < 25000000.0);
}

/*============================================================================
 * SUPERCARD PRO TESTS
 *============================================================================*/

TEST(scp_config_lifecycle) {
    uft_scp_config_t *cfg = uft_scp_config_create();
    assert(cfg != NULL);
    uft_scp_config_destroy(cfg);
}

TEST(scp_settings) {
    uft_scp_config_t *cfg = uft_scp_config_create();
    
    assert(uft_scp_set_track_range(cfg, 0, 79) == 0);
    assert(uft_scp_set_side(cfg, -1) == 0);
    assert(uft_scp_set_revolutions(cfg, 3) == 0);
    assert(uft_scp_set_retries(cfg, 5) == 0);
    
    /* Invalid values */
    assert(uft_scp_set_track_range(cfg, -1, 79) == -1);
    assert(uft_scp_set_revolutions(cfg, 0) == -1);
    assert(uft_scp_set_revolutions(cfg, 10) == -1);
    
    uft_scp_config_destroy(cfg);
}

TEST(scp_timing) {
    double ns = uft_scp_ticks_to_ns(40);
    assert(ns == 1000.0);  /* 40 ticks × 25ns = 1000ns */
    
    uint32_t ticks = uft_scp_ns_to_ticks(1000.0);
    assert(ticks == 40);
    
    double clock = uft_scp_get_sample_clock();
    assert(clock == 40000000.0);
}

TEST(scp_status_strings) {
    assert(strcmp(uft_scp_status_string(SCP_STATUS_OK), "OK") == 0);
    assert(strcmp(uft_scp_status_string(SCP_STATUS_WRITE_PROT), "Write protected") == 0);
    assert(strcmp(uft_scp_status_string(SCP_STATUS_NO_INDEX), "No index pulse") == 0);
}

TEST(scp_drive_names) {
    assert(strstr(uft_scp_drive_name(SCP_DRIVE_35_DD), "3.5") != NULL);
    assert(strstr(uft_scp_drive_name(SCP_DRIVE_525_HD), "5.25") != NULL);
}

/*============================================================================
 * XUM1541 TESTS
 *============================================================================*/

TEST(xum_config_lifecycle) {
    uft_xum_config_t *cfg = uft_xum_config_create();
    assert(cfg != NULL);
    uft_xum_config_destroy(cfg);
}

TEST(xum_drive_info) {
    assert(strcmp(uft_xum_drive_name(UFT_CBM_DRIVE_1541), "Commodore 1541") == 0);
    assert(strcmp(uft_xum_drive_name(UFT_CBM_DRIVE_1571), "Commodore 1571") == 0);
    assert(strcmp(uft_xum_drive_name(UFT_CBM_DRIVE_1581), "Commodore 1581") == 0);
    
    assert(uft_xum_tracks_for_drive(UFT_CBM_DRIVE_1541) == 35);
    assert(uft_xum_tracks_for_drive(UFT_CBM_DRIVE_1581) == 80);
    assert(uft_xum_tracks_for_drive(UFT_CBM_DRIVE_8250) == 77);
}

TEST(xum_sector_layout) {
    /* 1541 sector layout */
    assert(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 1) == 21);
    assert(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 18) == 19);
    assert(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 25) == 18);
    assert(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 31) == 17);
    
    /* 1581 - 40 sectors per track */
    assert(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1581, 1) == 40);
    assert(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1581, 80) == 40);
}

/*============================================================================
 * FC5025 TESTS
 *============================================================================*/

TEST(fc_config_lifecycle) {
    uft_fc_config_t *cfg = uft_fc_config_create();
    assert(cfg != NULL);
    uft_fc_config_destroy(cfg);
}

TEST(fc_format_info) {
    assert(strstr(uft_fc_format_name(UFT_FC_FMT_APPLE_DOS33), "Apple") != NULL);
    assert(strstr(uft_fc_format_name(UFT_FC_FMT_TRS80_SSSD), "TRS-80") != NULL);
    
    assert(uft_fc_tracks_for_format(UFT_FC_FMT_APPLE_DOS33) == 35);
    assert(uft_fc_sectors_for_format(UFT_FC_FMT_APPLE_DOS33) == 16);
    assert(uft_fc_sectors_for_format(UFT_FC_FMT_APPLE_DOS32) == 13);
}

TEST(fc_drive_names) {
    assert(strstr(uft_fc_drive_name(UFT_FC_DRIVE_525_48TPI), "5.25") != NULL);
    assert(strstr(uft_fc_drive_name(UFT_FC_DRIVE_8_SSSD), "8") != NULL);
}

/*============================================================================
 * APPLESAUCE TESTS
 *============================================================================*/

TEST(as_config_lifecycle) {
    uft_as_config_t *cfg = uft_as_config_create();
    assert(cfg != NULL);
    uft_as_config_destroy(cfg);
}

TEST(as_format_names) {
    assert(strstr(uft_as_format_name(UFT_AS_FMT_DOS33), "DOS 3.3") != NULL);
    assert(strstr(uft_as_format_name(UFT_AS_FMT_MAC_800K), "Macintosh") != NULL);
    assert(strstr(uft_as_format_name(UFT_AS_FMT_PRODOS), "ProDOS") != NULL);
}

TEST(as_timing) {
    double ns = uft_as_ticks_to_ns(8);
    assert(ns == 1000.0);  /* 8 ticks × 125ns = 1000ns */
    
    uint32_t ticks = uft_as_ns_to_ticks(1000.0);
    assert(ticks == 8);
    
    double clock = uft_as_get_sample_clock();
    assert(clock == 8000000.0);
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(void) {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  UFT Hardware Abstraction Layer - Unit Tests                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("=== HAL Unified ===\n");
    RUN_TEST(hal_controller_count);
    RUN_TEST(hal_controller_names);
    RUN_TEST(hal_null_safety);
    
    printf("\n=== KryoFlux ===\n");
    RUN_TEST(kf_config_lifecycle);
    RUN_TEST(kf_platform_presets);
    RUN_TEST(kf_timing);
    
    printf("\n=== SuperCard Pro ===\n");
    RUN_TEST(scp_config_lifecycle);
    RUN_TEST(scp_settings);
    RUN_TEST(scp_timing);
    RUN_TEST(scp_status_strings);
    RUN_TEST(scp_drive_names);
    
    printf("\n=== XUM1541/ZoomFloppy ===\n");
    RUN_TEST(xum_config_lifecycle);
    RUN_TEST(xum_drive_info);
    RUN_TEST(xum_sector_layout);
    
    printf("\n=== FC5025 ===\n");
    RUN_TEST(fc_config_lifecycle);
    RUN_TEST(fc_format_info);
    RUN_TEST(fc_drive_names);
    
    printf("\n=== Applesauce ===\n");
    RUN_TEST(as_config_lifecycle);
    RUN_TEST(as_format_names);
    RUN_TEST(as_timing);
    
    printf("\n════════════════════════════════════════════════════════════════\n");
    printf("  All %d tests passed!\n", 21);
    printf("════════════════════════════════════════════════════════════════\n");
    
    return 0;
}
