/**
 * @file test_hal_unified.c
 * @brief Unit tests for uft_hal_unified.h
 */

#include "uft/hal/uft_hal_unified.h"
#include <stdio.h>
#include <string.h>

#define TEST(name) static int test_##name(void)
#define RUN(name) do { \
    printf("  TEST: %-40s ", #name); \
    if (test_##name() == 0) { printf("[PASS]\n"); passed++; } \
    else { printf("[FAIL]\n"); failed++; } \
    total++; \
} while(0)

static int passed = 0, failed = 0, total = 0;

TEST(hal_type_names) {
    if (!uft_hal_type_name(UFT_HAL_GREASEWEAZLE)) return -1;
    if (!uft_hal_type_name(UFT_HAL_FLUXENGINE)) return -1;
    if (!uft_hal_type_name(UFT_HAL_KRYOFLUX)) return -1;
    if (!uft_hal_type_name(UFT_HAL_FC5025)) return -1;
    if (!uft_hal_type_name(UFT_HAL_XUM1541)) return -1;
    if (!uft_hal_type_name(UFT_HAL_ZOOMFLOPPY)) return -1;
    return 0;
}

TEST(hal_type_caps) {
    uint32_t caps;
    
    caps = uft_hal_type_caps(UFT_HAL_GREASEWEAZLE);
    if (!(caps & UFT_HAL_CAP_READ_FLUX)) return -1;
    if (!(caps & UFT_HAL_CAP_WRITE_FLUX)) return -1;
    
    caps = uft_hal_type_caps(UFT_HAL_KRYOFLUX);
    if (!(caps & UFT_HAL_CAP_READ_FLUX)) return -1;
    // KryoFlux can't write
    if (caps & UFT_HAL_CAP_WRITE_FLUX) return -1;
    
    return 0;
}

TEST(raw_track_init_free) {
    uft_raw_track_t track;
    uft_raw_track_init(&track);
    
    if (track.flux != NULL) return -1;
    if (track.flux_count != 0) return -1;
    if (track.cylinder != 0) return -1;
    if (track.head != 0) return -1;
    
    uft_raw_track_free(&track);
    return 0;
}

TEST(raw_track_clone_null) {
    uft_raw_track_t* clone = uft_raw_track_clone(NULL);
    if (clone != NULL) return -1;  // Should return NULL for NULL input
    return 0;
}

TEST(hal_open_invalid) {
    // Opening invalid device should return NULL
    uft_hal_t* hal = uft_hal_open(UFT_HAL_GREASEWEAZLE, "/dev/nonexistent_device_xyz");
    if (hal != NULL) {
        uft_hal_close(hal);
        return -1;  // Should have failed
    }
    return 0;
}

TEST(hal_enumerate) {
    uft_hal_info_t infos[16];
    int count = uft_hal_enumerate(infos, 16);
    
    // Count should be >= 0 (might be 0 if no hardware)
    if (count < 0) return -1;
    
    return 0;
}

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         UFT HAL UNIFIED API TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    RUN(hal_type_names);
    RUN(hal_type_caps);
    RUN(raw_track_init_free);
    RUN(raw_track_clone_null);
    RUN(hal_open_invalid);
    RUN(hal_enumerate);
    
    printf("\n═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         RESULTS: %d/%d passed, %d failed\n", passed, total, failed);
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    return (failed == 0) ? 0 : 1;
}
