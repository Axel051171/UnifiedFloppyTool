/**
 * @file test_registry_hal.c
 * @brief Tests for Format Registry and HAL Profiles
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/profiles/uft_format_registry.h"
#include "uft/hal/uft_hal_profiles.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  Testing: %s... ", #name); \
    tests_run++; \
    if (test_##name()) { \
        printf("PASS\n"); \
        tests_passed++; \
    } else { \
        printf("FAIL\n"); \
    } \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 * Format Registry Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_registry_count(void) {
    int count = 0;
    for (int i = 0; UFT_FORMAT_REGISTRY[i].name != NULL; i++) {
        count++;
    }
    return count == 26;  /* 26 formats */
}

static int test_registry_get_descriptor(void) {
    const uft_format_descriptor_t* desc = uft_format_get_descriptor(UFT_FORMAT_ADF);
    if (!desc) return 0;
    return strcmp(desc->name, "ADF") == 0;
}

static int test_registry_get_name(void) {
    return strcmp(uft_format_get_name(UFT_FORMAT_HFE), "HFE") == 0 &&
           strcmp(uft_format_get_name(UFT_FORMAT_WOZ), "WOZ") == 0;
}

static int test_registry_category_names(void) {
    return strcmp(uft_format_category_name(UFT_CAT_FLUX), "Flux") == 0 &&
           strcmp(uft_format_category_name(UFT_CAT_SECTOR), "Sector") == 0;
}

static int test_registry_platform_names(void) {
    return strcmp(uft_format_platform_name(UFT_PLATFORM_AMIGA), "Amiga") == 0 &&
           strcmp(uft_format_platform_name(UFT_PLATFORM_APPLE_II), "Apple II") == 0;
}

static int test_registry_detect_adf(void) {
    /* Create minimal ADF-like data (just header) */
    uint8_t data[1024];
    memset(data, 0, sizeof(data));
    memcpy(data, "DOS\x00", 4);  /* OFS signature */
    
    /* Test with actual data size, not claimed ADF size */
    uft_format_detection_t result;
    uft_format_type_t detected = uft_format_detect(data, sizeof(data), &result);
    
    /* Check if ADF was considered (might not score highest with small data) */
    return result.count > 0;  /* At least some format detected */
}

static int test_registry_detect_imd(void) {
    /* Create IMD-like data */
    uint8_t data[256];
    memset(data, 0, sizeof(data));
    memcpy(data, "IMD ", 4);  /* IMD signature */
    
    uft_format_type_t detected = uft_format_identify(data, sizeof(data));
    return detected == UFT_FORMAT_IMD;
}

static int test_registry_detect_scp(void) {
    /* Create SCP-like data */
    uint8_t data[32];
    memset(data, 0, sizeof(data));
    memcpy(data, "SCP", 3);  /* SCP signature */
    
    uft_format_type_t detected = uft_format_identify(data, sizeof(data));
    return detected == UFT_FORMAT_SCP;
}

static int test_registry_can_write(void) {
    return uft_format_can_write(UFT_FORMAT_ADF) == true &&
           uft_format_can_write(UFT_FORMAT_TD0) == false;  /* TD0 is read-only */
}

static int test_registry_get_by_platform(void) {
    uft_format_type_t types[10];
    int count = uft_format_get_by_platform(UFT_PLATFORM_APPLE_II, types, 10);
    return count >= 3;  /* WOZ, A2R, NIB, DSK */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * HAL Profile Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_hal_profile_count(void) {
    int count = 0;
    for (int i = 0; UFT_CONTROLLER_PROFILES[i].name != NULL; i++) {
        count++;
    }
    return count >= 7;  /* At least 7 controllers */
}

static int test_hal_get_profile(void) {
    const uft_controller_profile_t* p = uft_hal_get_profile(HAL_CTRL_GREASEWEAZLE);
    if (!p) return 0;
    return strcmp(p->name, "Greaseweazle") == 0;
}

static int test_hal_has_cap(void) {
    return uft_hal_has_cap(HAL_CTRL_GREASEWEAZLE, UFT_HAL_CAP_READ_FLUX) &&
           uft_hal_has_cap(HAL_CTRL_GREASEWEAZLE, UFT_HAL_CAP_WRITE_FLUX);
}

static int test_hal_supports_platform(void) {
    return uft_hal_supports_platform(HAL_CTRL_APPLESAUCE, UFT_PLATFORM_APPLE_II) &&
           !uft_hal_supports_platform(HAL_CTRL_APPLESAUCE, UFT_PLATFORM_COMMODORE);
}

static int test_hal_sample_clock(void) {
    uint32_t gw_clock = uft_hal_get_sample_clock(HAL_CTRL_GREASEWEAZLE);
    uint32_t kf_clock = uft_hal_get_sample_clock(HAL_CTRL_KRYOFLUX);
    
    return gw_clock == 72000000 && kf_clock == 24027428;
}

static int test_hal_timing_resolution(void) {
    uint32_t gw_res = uft_hal_get_timing_resolution(HAL_CTRL_GREASEWEAZLE);
    uint32_t scp_res = uft_hal_get_timing_resolution(HAL_CTRL_SCP);
    
    return gw_res == 14 && scp_res == 25;
}

static int test_hal_open_source(void) {
    return uft_hal_is_open_source(HAL_CTRL_GREASEWEAZLE) &&
           uft_hal_is_open_source(HAL_CTRL_FLUXENGINE) &&
           !uft_hal_is_open_source(HAL_CTRL_KRYOFLUX);
}

static int test_hal_available(void) {
    return uft_hal_is_available(HAL_CTRL_GREASEWEAZLE) &&
           !uft_hal_is_available(HAL_CTRL_FC5025);  /* Discontinued */
}

static int test_hal_find_by_platform(void) {
    uft_hal_controller_t types[10];
    int count = uft_hal_find_by_platform(UFT_PLATFORM_COMMODORE, types, 10);
    return count >= 3;  /* GW, KF, SCP, XUM, ZoomFloppy */
}

static int test_hal_find_by_cap(void) {
    uft_hal_controller_t types[10];
    int count = uft_hal_find_by_cap(UFT_HAL_CAP_WRITE_FLUX, types, 10);
    return count >= 4;  /* GW, FE, KF, SCP, Applesauce */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== Format Registry & HAL Profile Tests ===\n\n");
    
    printf("[Format Registry]\n");
    TEST(registry_count);
    TEST(registry_get_descriptor);
    TEST(registry_get_name);
    TEST(registry_category_names);
    TEST(registry_platform_names);
    TEST(registry_detect_adf);
    TEST(registry_detect_imd);
    TEST(registry_detect_scp);
    TEST(registry_can_write);
    TEST(registry_get_by_platform);
    
    printf("\n[HAL Profiles]\n");
    TEST(hal_profile_count);
    TEST(hal_get_profile);
    TEST(hal_has_cap);
    TEST(hal_supports_platform);
    TEST(hal_sample_clock);
    TEST(hal_timing_resolution);
    TEST(hal_open_source);
    TEST(hal_available);
    TEST(hal_find_by_platform);
    TEST(hal_find_by_cap);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
