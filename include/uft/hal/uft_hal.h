/**
 * @file uft_hal.h
 * @brief Hardware Abstraction Layer
 * @version 1.0.0
 * 
 * Unified interface for floppy disk controllers:
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

/**
 * @brief Controller type enum (UFT_CTRL_ prefix version)
 */
typedef enum {
    UFT_CTRL_NONE = 0,
    UFT_CTRL_GREASEWEAZLE,
    UFT_CTRL_FLUXENGINE,
    UFT_CTRL_KRYOFLUX,
    UFT_CTRL_FC5025,
    UFT_CTRL_XUM1541,
    UFT_CTRL_SUPERCARD_PRO,
    UFT_CTRL_PAULINE,
    UFT_CTRL_APPLESAUCE,
    UFT_CTRL_COUNT
} uft_controller_type_t;

/* Maximum number of limitation strings */
#define UFT_CAPS_MAX_LIMITATIONS 16

/**
 * @brief Controller capabilities structure
 */
typedef struct {
    uft_controller_type_t type;       /**< Controller type */
    const char* name;                 /**< Controller name */
    const char* version;              /**< Version/model string */
    
    /* Timing characteristics */
    double sample_rate_mhz;           /**< Sample rate in MHz */
    double sample_resolution_ns;      /**< Sample resolution in nanoseconds */
    double jitter_ns;                 /**< Timing jitter in nanoseconds */
    
    /* Read capabilities */
    bool can_read_flux;               /**< Can read raw flux data */
    bool can_read_bitstream;          /**< Can read decoded bitstream */
    bool can_read_sector;             /**< Can read sector data */
    
    /* Write capabilities */
    bool can_write_flux;              /**< Can write raw flux data */
    bool can_write_bitstream;         /**< Can write from bitstream */
    
    /* Index handling */
    bool hardware_index;              /**< Has hardware index sensing */
    double index_accuracy_ns;         /**< Index pulse accuracy in ns */
    int max_revolutions;              /**< Maximum revolutions per read */
    
    /* Physical limits */
    int max_cylinders;                /**< Maximum cylinder number */
    int max_heads;                    /**< Maximum number of heads */
    bool supports_half_tracks;        /**< Half-track stepping support */
    
    /* Data rate */
    double max_data_rate_kbps;        /**< Maximum data rate in kbps */
    bool variable_data_rate;          /**< Supports variable data rate */
    
    /* Copy protection */
    bool copy_protection_support;     /**< Copy protection analysis support */
    bool weak_bit_detection;          /**< Can detect weak bits */
    bool density_select;              /**< Has density select control */
    
    /* Limitations (NULL-terminated array of strings) */
    const char* limitations[UFT_CAPS_MAX_LIMITATIONS];
} uft_controller_caps_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Predefined Controller Capabilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

extern const uft_controller_caps_t UFT_CAPS_GREASEWEAZLE;
extern const uft_controller_caps_t UFT_CAPS_FLUXENGINE;
extern const uft_controller_caps_t UFT_CAPS_KRYOFLUX;
extern const uft_controller_caps_t UFT_CAPS_FC5025;
extern const uft_controller_caps_t UFT_CAPS_XUM1541;

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
 * @brief Get last error message (alias)
 */
const char* uft_hal_get_error(uft_hal_t* hal);

/**
 * @brief Get controller name
 * @param type Controller type
 * @return Human-readable name
 */
const char* uft_hal_controller_name(uft_hal_controller_t type);

/**
 * @brief Get number of supported controllers
 * @return Controller count
 */
int uft_hal_get_controller_count(void);

/**
 * @brief Get controller name by index
 * @param index Controller index (0 to count-1)
 * @return Controller name or NULL
 */
const char* uft_hal_get_controller_name_by_index(int index);

/**
 * @brief Check if controller is implemented
 * @param type Controller type
 * @return true if implemented, false if stub
 */
bool uft_hal_is_controller_implemented(uft_hal_controller_t type);

/**
 * @brief Get drive profile by type
 * @param type Drive type
 * @return Profile pointer or NULL if invalid
 */
const uft_drive_profile_t* uft_hal_get_drive_profile(uft_drive_type_t type);

/**
 * @brief Get controller capabilities by type
 * @param type Controller type
 * @return Capabilities pointer or NULL if invalid
 */
const uft_controller_caps_t* uft_hal_get_controller_caps(uft_controller_type_t type);

/**
 * @brief Print controller capabilities to stdout
 * @param caps Capabilities to print
 */
void uft_hal_print_controller_caps(const uft_controller_caps_t* caps);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HAL_H */
