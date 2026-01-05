/**
 * @file uft_hal.c
 * @brief HAL Implementation (Stub)
 */

#include "uft/hal/uft_hal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct uft_hal_s {
    uft_hal_controller_t type;
    char device_path[256];
    uft_hal_caps_t caps;
    char error[256];
    bool is_open;
};

static const char* CONTROLLER_NAMES[] = {
    "Greaseweazle", "FluxEngine", "KryoFlux", "SuperCard Pro",
    "Applesauce", "XUM1541", "ZoomFloppy", "FC5025"
};

int uft_hal_enumerate(uft_hal_controller_t* controllers, int max_count) {
    // Stub: would scan USB for devices
    return 0;
}

uft_hal_t* uft_hal_open(uft_hal_controller_t type, const char* device_path) {
    uft_hal_t* hal = calloc(1, sizeof(uft_hal_t));
    if (!hal) return NULL;
    
    hal->type = type;
    if (device_path) strncpy(hal->device_path, device_path, sizeof(hal->device_path) - 1);
    
    // Set default caps based on controller type
    hal->caps.max_tracks = 84;
    hal->caps.max_sides = 2;
    hal->caps.can_read_flux = true;
    hal->caps.can_write_flux = (type == HAL_CTRL_GREASEWEAZLE);
    hal->caps.sample_rate_hz = 72000000;
    hal->caps.capabilities = HAL_CAP_READ_FLUX | HAL_CAP_INDEX_SENSE | HAL_CAP_MOTOR_CTRL;
    
    hal->is_open = true;
    return hal;
}

int uft_hal_get_caps(uft_hal_t* hal, uft_hal_caps_t* caps) {
    if (!hal || !caps) return -1;
    *caps = hal->caps;
    return 0;
}

int uft_hal_read_flux(uft_hal_t* hal, int track, int side, int revolutions, uint32_t** flux, size_t* count) {
    if (!hal || !flux || !count) return -1;
    snprintf(hal->error, sizeof(hal->error), "Not connected to hardware");
    return -1;
}

int uft_hal_write_flux(uft_hal_t* hal, int track, int side, const uint32_t* flux, size_t count) {
    if (!hal) return -1;
    return -1;
}

int uft_hal_seek(uft_hal_t* hal, int track) {
    if (!hal || track < 0 || track > hal->caps.max_tracks) return -1;
    return 0;  // Would move head
}

int uft_hal_motor(uft_hal_t* hal, bool on) {
    if (!hal) return -1;
    return 0;
}

void uft_hal_close(uft_hal_t* hal) {
    if (hal) {
        hal->is_open = false;
        free(hal);
    }
}

const char* uft_hal_get_error(uft_hal_t* hal) {
    return hal ? hal->error : "NULL handle";
}

const char* uft_hal_controller_name(uft_hal_controller_t type) {
    if (type >= 0 && type <= HAL_CTRL_FC5025) return CONTROLLER_NAMES[type];
    return "Unknown";
}
