/**
 * @file uft_hal_unified.c
 * @brief Unified HAL Implementation
 */

#include "uft/hal/uft_hal_unified.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// TYPE NAMES
// ============================================================================

static const char* type_names[] = {
    "NONE",
    "Greaseweazle",
    "FluxEngine",
    "KryoFlux",
    "FC5025",
    "XUM1541",
    "ZoomFloppy",
    "Applesauce",
    "SuperCard Pro",
    "Pauline"
};

const char* uft_hal_type_name(uft_hal_type_t type) {
    if (type < 0 || type >= UFT_HAL_COUNT) return "Unknown";
    return type_names[type];
}

// ============================================================================
// TYPE CAPABILITIES
// ============================================================================

static const uint32_t type_caps[] = {
    0,  // NONE
    UFT_HAL_CAP_READ_FLUX | UFT_HAL_CAP_WRITE_FLUX | UFT_HAL_CAP_MULTI_REV | 
    UFT_HAL_CAP_HD | UFT_HAL_CAP_INDEX,  // Greaseweazle
    UFT_HAL_CAP_READ_FLUX | UFT_HAL_CAP_WRITE_FLUX | UFT_HAL_CAP_MULTI_REV | 
    UFT_HAL_CAP_HD | UFT_HAL_CAP_INDEX,  // FluxEngine
    UFT_HAL_CAP_READ_FLUX | UFT_HAL_CAP_MULTI_REV | UFT_HAL_CAP_HD | 
    UFT_HAL_CAP_INDEX,  // KryoFlux (no write)
    UFT_HAL_CAP_READ_MFM | UFT_HAL_CAP_WRITE_MFM | UFT_HAL_CAP_HD,  // FC5025
    UFT_HAL_CAP_GCR_NATIVE,  // XUM1541
    UFT_HAL_CAP_GCR_NATIVE,  // ZoomFloppy
    UFT_HAL_CAP_READ_FLUX | UFT_HAL_CAP_WRITE_FLUX | UFT_HAL_CAP_MULTI_REV | 
    UFT_HAL_CAP_INDEX,  // Applesauce
    UFT_HAL_CAP_READ_FLUX | UFT_HAL_CAP_MULTI_REV | UFT_HAL_CAP_INDEX,  // SuperCard
    UFT_HAL_CAP_READ_FLUX | UFT_HAL_CAP_WRITE_FLUX | UFT_HAL_CAP_MULTI_REV  // Pauline
};

uint32_t uft_hal_type_caps(uft_hal_type_t type) {
    if (type < 0 || type >= UFT_HAL_COUNT) return 0;
    return type_caps[type];
}

// ============================================================================
// RAW TRACK UTILITIES
// ============================================================================

void uft_raw_track_init(uft_raw_track_t* track) {
    if (!track) return;
    memset(track, 0, sizeof(*track));
}

void uft_raw_track_free(uft_raw_track_t* track) {
    if (!track) return;
    free(track->flux);
    if (track->revolutions) {
        for (int i = 0; i < track->revolution_count; i++) {
            free(track->revolutions[i].flux);
        }
        free(track->revolutions);
    }
    memset(track, 0, sizeof(*track));
}

uft_raw_track_t* uft_raw_track_clone(const uft_raw_track_t* track) {
    if (!track) return NULL;
    
    uft_raw_track_t* clone = calloc(1, sizeof(uft_raw_track_t));
    if (!clone) return NULL;
    
    *clone = *track;
    clone->flux = NULL;
    clone->revolutions = NULL;
    
    if (track->flux && track->flux_count > 0) {
        clone->flux = malloc(track->flux_count * sizeof(uint32_t));
        if (!clone->flux) {
            free(clone);
            return NULL;
        }
        memcpy(clone->flux, track->flux, track->flux_count * sizeof(uint32_t));
    }
    
    return clone;
}

// ============================================================================
// HAL OPERATIONS (STUBS)
// ============================================================================

struct uft_hal_s {
    uft_hal_type_t type;
    char device_path[256];
    void* driver_ctx;
    const uft_hal_driver_t* driver;
    int error_code;
    char error_msg[256];
};

uft_hal_t* uft_hal_open(uft_hal_type_t type, const char* device) {
    // For now, just return NULL (no hardware)
    (void)type;
    (void)device;
    return NULL;
}

void uft_hal_close(uft_hal_t* hal) {
    if (hal) {
        free(hal);
    }
}

int uft_hal_enumerate(uft_hal_info_t* infos, int max_count) {
    (void)infos;
    (void)max_count;
    return 0;  // No devices
}

bool uft_hal_type_available(uft_hal_type_t type) {
    (void)type;
    return false;
}
