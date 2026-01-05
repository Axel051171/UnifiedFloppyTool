/**
 * @file uft_hal.h
 * @brief Hardware Abstraction Layer
 * @version 1.0.0
 * 
 * Unified interface for floppy disk controllers:
 * - Greaseweazle
 * - FluxEngine
 * - KryoFlux
 * - SuperCard Pro
 * - Applesauce
 * - XUM1541/ZoomFloppy
 * - FC5025
 */

#ifndef UFT_HAL_H
#define UFT_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft/hal/uft_drive.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Controller Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    HAL_CTRL_GREASEWEAZLE = 0,
    HAL_CTRL_FLUXENGINE,
    HAL_CTRL_KRYOFLUX,
    HAL_CTRL_SCP,
    HAL_CTRL_APPLESAUCE,
    HAL_CTRL_XUM1541,
    HAL_CTRL_ZOOMFLOPPY,
    HAL_CTRL_FC5025,
    HAL_CTRL_COUNT
} uft_hal_controller_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Capability Flags
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    HAL_CAP_READ_FLUX     = (1 << 0),  /**< Can read flux data */
    HAL_CAP_WRITE_FLUX    = (1 << 1),  /**< Can write flux data */
    HAL_CAP_INDEX_SENSE   = (1 << 2),  /**< Has index pulse sensing */
    HAL_CAP_MOTOR_CTRL    = (1 << 3),  /**< Can control drive motor */
    HAL_CAP_DENSITY_CTRL  = (1 << 4),  /**< Can set density select */
    HAL_CAP_ERASE         = (1 << 5),  /**< Can erase tracks */
    HAL_CAP_HALF_TRACK    = (1 << 6),  /**< Supports half-track stepping */
    HAL_CAP_WRITE_PROTECT = (1 << 7)   /**< Can sense write protect */
} uft_hal_cap_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Capabilities Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    int max_tracks;           /**< Maximum track number */
    int max_sides;            /**< Number of sides (1 or 2) */
    bool can_read_flux;       /**< Can read raw flux */
    bool can_write_flux;      /**< Can write raw flux */
    uint32_t sample_rate_hz;  /**< Sample clock frequency */
    uint32_t capabilities;    /**< Bitmask of uft_hal_cap_t */
} uft_hal_caps_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * HAL Handle (opaque)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_hal_s uft_hal_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Enumerate available controllers
 * @param controllers Array to fill with found controllers
 * @param max_count Maximum entries in array
 * @return Number of controllers found
 */
int uft_hal_enumerate(uft_hal_controller_t* controllers, int max_count);

/**
 * @brief Open a hardware controller
 * @param type Controller type
 * @param device_path Device path (e.g., "/dev/ttyACM0", "COM3")
 * @return HAL handle or NULL on error
 */
uft_hal_t* uft_hal_open(uft_hal_controller_t type, const char* device_path);

/**
 * @brief Get controller capabilities
 * @param hal HAL handle
 * @param caps Capabilities structure to fill
 * @return 0 on success, -1 on error
 */
int uft_hal_get_caps(uft_hal_t* hal, uft_hal_caps_t* caps);

/**
 * @brief Read flux data from disk
 * @param hal HAL handle
 * @param track Track number
 * @param side Side (0 or 1)
 * @param revolutions Number of revolutions to capture
 * @param flux Output: flux timing data (caller must free)
 * @param count Output: number of flux transitions
 * @return 0 on success, -1 on error
 */
int uft_hal_read_flux(uft_hal_t* hal, int track, int side, int revolutions,
                      uint32_t** flux, size_t* count);

/**
 * @brief Write flux data to disk
 * @param hal HAL handle
 * @param track Track number
 * @param side Side (0 or 1)
 * @param flux Flux timing data
 * @param count Number of flux transitions
 * @return 0 on success, -1 on error
 */
int uft_hal_write_flux(uft_hal_t* hal, int track, int side,
                       const uint32_t* flux, size_t count);

/**
 * @brief Seek to track
 * @param hal HAL handle
 * @param track Track number
 * @return 0 on success, -1 on error
 */
int uft_hal_seek(uft_hal_t* hal, int track);

/**
 * @brief Control drive motor
 * @param hal HAL handle
 * @param on true to turn on, false to turn off
 * @return 0 on success, -1 on error
 */
int uft_hal_motor(uft_hal_t* hal, bool on);

/**
 * @brief Close hardware controller
 * @param hal HAL handle
 */
void uft_hal_close(uft_hal_t* hal);

/**
 * @brief Get last error message
 * @param hal HAL handle
 * @return Error message string
 */
const char* uft_hal_error(uft_hal_t* hal);

/**
 * @brief Get controller name
 * @param type Controller type
 * @return Human-readable name
 */
const char* uft_hal_controller_name(uft_hal_controller_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HAL_H */
