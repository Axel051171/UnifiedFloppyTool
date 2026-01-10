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

#ifdef __cplusplus
}
#endif

#endif /* UFT_HAL_H */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Extended Controller Types (for profiles)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_CTRL_GREASEWEAZLE = 0,
    UFT_CTRL_FLUXENGINE,
    UFT_CTRL_KRYOFLUX,
    UFT_CTRL_SCP,
    UFT_CTRL_FC5025,
    UFT_CTRL_XUM1541,
    UFT_CTRL_APPLESAUCE,
    UFT_CTRL_COUNT
} uft_controller_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Extended Controller Capabilities (for detailed profiles)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_controller_type_t type;     /**< Controller type */
    const char* name;               /**< Display name */
    const char* version;            /**< Version/model info */
    
    /* Timing characteristics */
    double sample_rate_mhz;         /**< Sample clock (MHz) */
    double sample_resolution_ns;    /**< Resolution (ns) */
    double jitter_ns;               /**< Typical jitter (ns) */
    
    /* Read/Write capabilities */
    bool can_read_flux;             /**< Raw flux capture */
    bool can_read_bitstream;        /**< Decoded bitstream */
    bool can_read_sector;           /**< Sector-level reads */
    bool can_write_flux;            /**< Raw flux writing */
    bool can_write_bitstream;       /**< Bitstream writing */
    
    /* Index sensing */
    bool hardware_index;            /**< Hardware index detection */
    double index_accuracy_ns;       /**< Index accuracy (ns) */
    int max_revolutions;            /**< Max revolutions per read */
    
    /* Physical limits */
    int max_cylinders;              /**< Max cylinder number */
    int max_heads;                  /**< Max heads (sides) */
    bool supports_half_tracks;      /**< Half-track stepping */
    
    /* Data rate */
    double max_data_rate_kbps;      /**< Max data rate (kbit/s) */
    bool variable_data_rate;        /**< Variable rate support */
    
    /* Special features */
    bool copy_protection_support;   /**< Copy protection handling */
    bool weak_bit_detection;        /**< Weak bit detection */
    bool density_select;            /**< HD/DD selection */
    
    /* Limitations */
    const char* limitations[8];     /**< Known limitations */
} uft_controller_caps_t;

/* Predefined controller capabilities */
extern const uft_controller_caps_t UFT_CAPS_GREASEWEAZLE;
extern const uft_controller_caps_t UFT_CAPS_FLUXENGINE;
extern const uft_controller_caps_t UFT_CAPS_KRYOFLUX;
extern const uft_controller_caps_t UFT_CAPS_SCP;
extern const uft_controller_caps_t UFT_CAPS_FC5025;
extern const uft_controller_caps_t UFT_CAPS_XUM1541;

/**
 * @brief Get controller capabilities by type
 */
const uft_controller_caps_t* uft_hal_get_controller_caps(uft_controller_type_t type);

/**
 * @brief Print controller capabilities (debug)
 */
void uft_hal_print_controller_caps(const uft_controller_caps_t* caps);

/**
 * @brief Get controller type name
 */
const char* uft_hal_controller_type_name(uft_controller_type_t type);

/**
 * @brief Check if controller supports a feature
 * @param caps Capabilities structure
 * @param feature Feature string ("flux", "halftrack", "weakbit", etc.)
 * @return true if supported
 */
bool uft_hal_controller_has_feature(const uft_controller_caps_t* caps, const char* feature);

/**
 * @brief Get recommended controller for a task
 * @param need_flux Requires raw flux capture
 * @param need_write Requires write capability
 * @param need_halftrack Requires half-track support
 * @return Best controller type or UFT_CTRL_GREASEWEAZLE (default)
 */
uft_controller_type_t uft_hal_recommend_controller(bool need_flux, bool need_write, bool need_halftrack);

