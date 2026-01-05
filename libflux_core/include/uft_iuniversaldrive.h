/**
 * @file uft_iuniversaldrive.h
 * @brief IUniversalDrive - Universal Hardware Abstraction Layer
 * 
 * CRITICAL INTERFACE - Hardware Independence
 * 
 * This is THE most important API in the system. It provides complete
 * hardware abstraction for ALL flux-reading devices, ensuring:
 * 
 * 1. Hardware Lock-in Prevention
 * 2. Plug-and-Play Device Support
 * 3. Testability (Mock Devices)
 * 4. Future-Proof Architecture
 * 
 * Design Principles:
 * - Smallest Common Denominator (all devices can implement)
 * - Capability Negotiation (optional features discoverable)
 * - Normalized Output (all data in nanoseconds)
 * - Provider Pattern (device-specific implementations)
 * 
 * Supported Devices:
 * - Greaseweazle
 * - SuperCard Pro
 * - KryoFlux
 * - FluxEngine
 * - Any future device (just add provider!)
 * 
 * @version 2.14.0
 * @date 2024-12-27
 */

#ifndef UFT_IUNIVERSALDRIVE_H
#define UFT_IUNIVERSALDRIVE_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * PART 1: CAPABILITY SYSTEM
 * ======================================================================== */

/**
 * @brief Device capability flags
 * 
 * Used for capability negotiation. Not all devices support all features.
 */
typedef enum {
    UFT_CAP_FLUX_READ           = (1 << 0),  /**< Can read flux */
    UFT_CAP_FLUX_WRITE          = (1 << 1),  /**< Can write flux */
    UFT_CAP_INDEX_SIGNAL        = (1 << 2),  /**< Has index pulse detection */
    UFT_CAP_DENSITY_DETECT      = (1 << 3),  /**< Can detect HD/DD/ED */
    UFT_CAP_WRITE_PROTECT       = (1 << 4),  /**< Can detect write protect */
    UFT_CAP_TRACK0_SENSOR       = (1 << 5),  /**< Has track 0 sensor */
    UFT_CAP_MOTOR_CONTROL       = (1 << 6),  /**< Can control motor */
    UFT_CAP_VARIABLE_SPEED      = (1 << 7),  /**< Supports variable RPM */
    UFT_CAP_AUTO_CALIBRATE      = (1 << 8),  /**< Self-calibrating */
    UFT_CAP_MULTIPLE_REVS       = (1 << 9),  /**< Can capture multiple revolutions */
    UFT_CAP_REAL_TIME_STREAM    = (1 << 10), /**< Supports live streaming */
    UFT_CAP_HALF_TRACK          = (1 << 11), /**< Supports half-track positioning */
    UFT_CAP_WEAK_BIT_REPEAT     = (1 << 12), /**< Can read weak bits multiple times */
    UFT_CAP_HIGH_PRECISION      = (1 << 13)  /**< Sub-100ns timing precision */
} uft_drive_capability_t;

/**
 * @brief Device information
 */
typedef struct {
    /* Device identification */
    char name[64];              /**< Device name (e.g., "Greaseweazle F7") */
    char serial[32];            /**< Serial number */
    char firmware[32];          /**< Firmware version */
    
    /* Capabilities */
    uint32_t capabilities;      /**< Capability flags (OR'd) */
    
    /* Physical limits */
    uint8_t max_tracks;         /**< Maximum track number (usually 84) */
    uint8_t max_heads;          /**< Maximum heads (1-2) */
    uint8_t max_drives;         /**< Number of drives (1-4) */
    
    /* Timing specifications */
    uint32_t min_sample_rate_hz;    /**< Minimum sampling rate */
    uint32_t max_sample_rate_hz;    /**< Maximum sampling rate */
    uint32_t native_sample_rate_hz; /**< Native/preferred rate */
    uint32_t timing_precision_ns;   /**< Timing precision (e.g., 25ns for SCP) */
    
    /* Buffer limits */
    uint32_t max_flux_buffer_size;  /**< Max flux transitions per track */
    
} uft_drive_info_t;

/* ========================================================================
 * PART 2: TRANSPORT CONTROL (Mechanical)
 * ======================================================================== */

/**
 * @brief Step direction
 */
typedef enum {
    UFT_STEP_OUT = 0,   /**< Towards track 0 */
    UFT_STEP_IN = 1     /**< Towards higher tracks */
} uft_step_direction_t;

/**
 * @brief Motor state
 */
typedef enum {
    UFT_MOTOR_OFF = 0,
    UFT_MOTOR_ON = 1
} uft_motor_state_t;

/**
 * @brief Density detection
 */
typedef enum {
    UFT_DENSITY_UNKNOWN = 0,
    UFT_DENSITY_DD,     /**< Double Density */
    UFT_DENSITY_HD,     /**< High Density */
    UFT_DENSITY_ED      /**< Extra Density */
} uft_density_t;

/* ========================================================================
 * PART 3: FLUX DATA STRUCTURES
 * ======================================================================== */

/**
 * @brief Flux transition stream (normalized to nanoseconds)
 * 
 * ALL devices MUST convert their native format to this:
 * - Array of uint32_t
 * - Each value = time in NANOSECONDS between flux transitions
 * - Normalized to 1,000,000,000 Hz
 * 
 * Example:
 * flux_ns = [2000, 2000, 4000, 2000, ...]
 * Means: transition at 2µs, 2µs, 4µs, 2µs intervals
 */
typedef struct {
    uint32_t* transitions_ns;   /**< Array of transition times (nanoseconds) */
    uint32_t count;             /**< Number of transitions */
    
    /* Metadata */
    uint32_t index_offset;      /**< Offset of index pulse (if present) */
    uint32_t total_time_ns;     /**< Total track time */
    uint8_t revolution;         /**< Revolution number (for multi-rev) */
    
    /* Quality metrics */
    uint32_t min_interval_ns;   /**< Shortest interval (for validation) */
    uint32_t max_interval_ns;   /**< Longest interval */
    bool has_index;             /**< Index pulse detected */
    
} uft_flux_stream_t;

/* ========================================================================
 * PART 4: PROVIDER OPERATIONS (vtable)
 * ======================================================================== */

/**
 * @brief Universal Drive operations
 * 
 * All providers MUST implement this interface.
 * Functions may return UFT_ERR_UNSUPPORTED if capability not available.
 */
typedef struct uft_drive_ops {
    /* === LIFECYCLE === */
    
    /**
     * Open device
     * 
     * @param ctx Provider-specific context
     * @param device_path Device path (e.g., "/dev/ttyACM0", "COM3")
     * @return UFT_SUCCESS or error
     */
    uft_rc_t (*open)(void* ctx, const char* device_path);
    
    /**
     * Close device
     * 
     * @param ctx Provider context
     * @return UFT_SUCCESS or error
     */
    uft_rc_t (*close)(void* ctx);
    
    /**
     * Get device information
     * 
     * @param ctx Provider context
     * @param[out] info Device info structure
     * @return UFT_SUCCESS or error
     */
    uft_rc_t (*get_info)(void* ctx, uft_drive_info_t* info);
    
    /* === DRIVE SELECTION === */
    
    /**
     * Select drive
     * 
     * @param ctx Provider context
     * @param drive_id Drive number (0-3)
     * @return UFT_SUCCESS or error
     */
    uft_rc_t (*select_drive)(void* ctx, uint8_t drive_id);
    
    /**
     * Set motor state
     * 
     * @param ctx Provider context
     * @param state Motor state
     * @return UFT_SUCCESS or error
     */
    uft_rc_t (*set_motor)(void* ctx, uft_motor_state_t state);
    
    /* === POSITIONING === */
    
    /**
     * Calibrate (seek to track 0)
     * 
     * @param ctx Provider context
     * @return UFT_SUCCESS or error
     */
    uft_rc_t (*calibrate)(void* ctx);
    
    /**
     * Seek to track
     * 
     * @param ctx Provider context
     * @param track Track number (0-based)
     * @param head Head number (0-1)
     * @return UFT_SUCCESS or error
     */
    uft_rc_t (*seek)(void* ctx, uint8_t track, uint8_t head);
    
    /**
     * Step head
     * 
     * @param ctx Provider context
     * @param direction Step direction
     * @param steps Number of steps
     * @return UFT_SUCCESS or error
     */
    uft_rc_t (*step)(void* ctx, uft_step_direction_t direction, uint8_t steps);
    
    /* === FLUX I/O === */
    
    /**
     * Read flux stream (CRITICAL FUNCTION!)
     * 
     * This is THE most important function. It MUST:
     * 1. Read raw flux from current track/head
     * 2. Convert to NANOSECOND timing
     * 3. Allocate flux_stream (caller must free!)
     * 4. Fill in all metadata
     * 
     * @param ctx Provider context
     * @param[out] flux_stream Allocated flux stream (caller frees)
     * @return UFT_SUCCESS or error
     */
    uft_rc_t (*read_flux)(void* ctx, uft_flux_stream_t** flux_stream);
    
    /**
     * Write flux stream
     * 
     * @param ctx Provider context
     * @param flux_stream Flux stream to write
     * @return UFT_SUCCESS or UFT_ERR_UNSUPPORTED
     */
    uft_rc_t (*write_flux)(void* ctx, const uft_flux_stream_t* flux_stream);
    
    /* === STATUS QUERIES === */
    
    /**
     * Get density
     * 
     * @param ctx Provider context
     * @param[out] density Detected density
     * @return UFT_SUCCESS or error
     */
    uft_rc_t (*get_density)(void* ctx, uft_density_t* density);
    
    /**
     * Check write protect
     * 
     * @param ctx Provider context
     * @param[out] protected Write protect status
     * @return UFT_SUCCESS or error
     */
    uft_rc_t (*is_write_protected)(void* ctx, bool* protected);
    
    /**
     * Check track 0
     * 
     * @param ctx Provider context
     * @param[out] at_track0 Track 0 sensor status
     * @return UFT_SUCCESS or error
     */
    uft_rc_t (*is_track0)(void* ctx, bool* at_track0);
    
} uft_drive_ops_t;

/* ========================================================================
 * PART 5: UNIVERSAL DRIVE HANDLE
 * ======================================================================== */

/**
 * @brief Universal drive handle
 * 
 * Opaque handle to hardware-abstracted drive.
 * Applications work ONLY with this, never with specific hardware.
 */
typedef struct {
    const uft_drive_ops_t* ops;     /**< Operations vtable */
    void* provider_ctx;             /**< Provider-specific context */
    uft_drive_info_t info;          /**< Cached device info */
    
    /* State tracking */
    uint8_t current_drive;
    uint8_t current_track;
    uint8_t current_head;
    bool motor_on;
    bool calibrated;
    
} uft_universal_drive_t;

/* ========================================================================
 * PART 6: PUBLIC API
 * ======================================================================== */

/**
 * @brief Create universal drive from provider
 * 
 * Factory function that creates hardware-abstracted drive.
 * 
 * @param provider_name Provider name ("greaseweazle", "scp", "kryoflux", etc.)
 * @param device_path Device path
 * @param[out] drive Universal drive handle
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_drive_create(
    const char* provider_name,
    const char* device_path,
    uft_universal_drive_t** drive
);

/**
 * @brief Destroy universal drive
 * 
 * @param[in,out] drive Drive to destroy
 */
void uft_drive_destroy(uft_universal_drive_t** drive);

/**
 * @brief Get device information
 * 
 * @param drive Universal drive
 * @return Device info (pointer to internal structure)
 */
const uft_drive_info_t* uft_drive_get_info(const uft_universal_drive_t* drive);

/**
 * @brief Check capability
 * 
 * @param drive Universal drive
 * @param capability Capability to check
 * @return true if supported
 */
bool uft_drive_has_capability(
    const uft_universal_drive_t* drive,
    uft_drive_capability_t capability
);

/* === Convenience wrappers === */

uft_rc_t uft_drive_calibrate(uft_universal_drive_t* drive);
uft_rc_t uft_drive_select(uft_universal_drive_t* drive, uint8_t drive_id);
uft_rc_t uft_drive_motor(uft_universal_drive_t* drive, bool on);
uft_rc_t uft_drive_seek(uft_universal_drive_t* drive, uint8_t track, uint8_t head);
uft_rc_t uft_drive_read_flux(uft_universal_drive_t* drive, uft_flux_stream_t** flux);
uft_rc_t uft_drive_write_flux(uft_universal_drive_t* drive, const uft_flux_stream_t* flux);

/**
 * @brief Free flux stream
 * 
 * @param[in,out] flux Flux stream to free
 */
void uft_flux_stream_free(uft_flux_stream_t** flux);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IUNIVERSALDRIVE_H */
