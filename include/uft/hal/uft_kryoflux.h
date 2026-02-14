#ifndef UFT_KRYOFLUX_H
#define UFT_KRYOFLUX_H

/**
 * @file uft_kryoflux.h
 * @brief KryoFlux DTC Integration Header
 * 
 * Provides a unified API for KryoFlux hardware via the DTC command-line tool.
 * Supports both standalone use and integration with the UFT parameter system.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 * 
 * Usage example:
 * @code
 * uft_kf_config_t *cfg = uft_kf_config_create();
 * 
 * // Optional: Set custom DTC path
 * uft_kf_set_dtc_path(cfg, "/opt/kryoflux/dtc");
 * 
 * // Configure capture
 * uft_kf_set_track_range(cfg, 0, 79);
 * uft_kf_set_side(cfg, -1);  // Both sides
 * 
 * // Capture single track
 * uint32_t *flux;
 * size_t count;
 * uft_kf_capture_track(cfg, 0, 0, &flux, &count, NULL, NULL);
 * 
 * // Or capture entire disk with callback
 * uft_kf_capture_disk(cfg, my_callback, my_userdata);
 * 
 * uft_kf_config_destroy(cfg);
 * @endcode
 */


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/** KryoFlux sample clock frequency (Hz) */
#define UFT_KF_SAMPLE_CLOCK     24027428.5714285

/** Maximum supported track number */
#define UFT_KF_MAX_TRACKS       84

/** Maximum supported sides */
#define UFT_KF_MAX_SIDES        2

/*============================================================================
 * TYPES
 *============================================================================*/

/**
 * @brief KryoFlux configuration handle (opaque)
 */
typedef struct uft_kf_config_s uft_kf_config_t;

/**
 * @brief Track capture result data
 * 
 * Passed to the callback during disk capture.
 */
typedef struct {
    int track;              /**< Track number (0-83) */
    int side;               /**< Side (0 or 1) */
    uint32_t *flux;         /**< Flux transition times (KF ticks) */
    size_t flux_count;      /**< Number of flux transitions */
    uint32_t *index;        /**< Index pulse positions (KF ticks) */
    size_t index_count;     /**< Number of index pulses */
    double sample_clock;    /**< Sample clock frequency (Hz) */
    bool success;           /**< true if capture succeeded */
    const char *error_msg;  /**< Error message if !success */
} uft_kf_track_data_t;

/**
 * @brief Disk capture callback function
 * 
 * @param data Track data (caller owns flux/index memory until callback returns)
 * @param user_data User-provided context
 * @return 0 to continue, non-zero to abort capture
 */
typedef int (*uft_kf_disk_callback_t)(const uft_kf_track_data_t *data, 
                                       void *user_data);

/**
 * @brief Drive type presets
 */
typedef enum {
    UFT_KF_DRIVE_AUTO = 0,      /**< Auto-detect */
    UFT_KF_DRIVE_35_DD,         /**< 3.5" DD (720K) */
    UFT_KF_DRIVE_35_HD,         /**< 3.5" HD (1.44M) */
    UFT_KF_DRIVE_525_DD,        /**< 5.25" DD (360K) */
    UFT_KF_DRIVE_525_HD,        /**< 5.25" HD (1.2M) */
    UFT_KF_DRIVE_525_40,        /**< 5.25" 40-track (C64/Apple) */
    UFT_KF_DRIVE_8_SSSD,        /**< 8" SS/SD */
    UFT_KF_DRIVE_8_DSDD         /**< 8" DS/DD */
} uft_kf_drive_type_t;

/**
 * @brief Platform presets for common systems
 */
typedef enum {
    UFT_KF_PLATFORM_GENERIC = 0, /**< Generic settings */
    UFT_KF_PLATFORM_AMIGA,       /**< Amiga (DD 80-track) */
    UFT_KF_PLATFORM_ATARI_ST,    /**< Atari ST */
    UFT_KF_PLATFORM_C64,         /**< Commodore 64 (40-track GCR) */
    UFT_KF_PLATFORM_C1541,       /**< C1541 disk drive */
    UFT_KF_PLATFORM_APPLE_II,    /**< Apple II (40-track GCR) */
    UFT_KF_PLATFORM_IBM_PC,      /**< IBM PC (MFM) */
    UFT_KF_PLATFORM_BBC_MICRO,   /**< BBC Micro */
    UFT_KF_PLATFORM_TRS80,       /**< TRS-80 */
    UFT_KF_PLATFORM_AMSTRAD_CPC, /**< Amstrad CPC */
    UFT_KF_PLATFORM_MSX,         /**< MSX */
    UFT_KF_PLATFORM_PC98,        /**< NEC PC-98 */
    UFT_KF_PLATFORM_X68000,      /**< Sharp X68000 */
    UFT_KF_PLATFORM_FM_TOWNS     /**< Fujitsu FM Towns */
} uft_kf_platform_t;

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

/**
 * @brief Create new KryoFlux configuration
 * 
 * Automatically searches for DTC in common locations.
 * 
 * @return Configuration handle or NULL on error
 */
uft_kf_config_t* uft_kf_config_create(void);

/**
 * @brief Destroy configuration
 * @param cfg Configuration handle
 */
void uft_kf_config_destroy(uft_kf_config_t *cfg);

/**
 * @brief Set DTC executable path
 * 
 * @param cfg Configuration handle
 * @param path Path to DTC executable
 * @return 0 on success, -1 if not found/executable
 */
int uft_kf_set_dtc_path(uft_kf_config_t *cfg, const char *path);

/**
 * @brief Set output directory for captured files
 * 
 * @param cfg Configuration handle
 * @param path Directory path (will be created if needed)
 * @return 0 on success, -1 on error
 */
int uft_kf_set_output_dir(uft_kf_config_t *cfg, const char *path);

/**
 * @brief Set track range to capture
 * 
 * @param cfg Configuration handle
 * @param start Start track (0-83)
 * @param end End track (start-83)
 * @return 0 on success, -1 on invalid range
 */
int uft_kf_set_track_range(uft_kf_config_t *cfg, int start, int end);

/**
 * @brief Set side to capture
 * 
 * @param cfg Configuration handle
 * @param side 0=bottom, 1=top, -1=both sides
 * @return 0 on success, -1 on invalid side
 */
int uft_kf_set_side(uft_kf_config_t *cfg, int side);

/**
 * @brief Set number of revolutions to capture
 * 
 * @param cfg Configuration handle
 * @param revs Number of revolutions (1-10)
 * @return 0 on success, -1 on invalid value
 */
int uft_kf_set_revolutions(uft_kf_config_t *cfg, int revs);

/**
 * @brief Set KryoFlux device index
 * 
 * @param cfg Configuration handle
 * @param device_index Device index (0-based), -1 for auto
 * @return 0 on success
 */
int uft_kf_set_device(uft_kf_config_t *cfg, int device_index);

/**
 * @brief Enable double-step mode for 40-track drives
 * 
 * Use this when reading 40-track disks (C64, Apple II) on 80-track drives.
 * 
 * @param cfg Configuration handle
 * @param enabled true to enable double-step
 * @return 0 on success
 */
int uft_kf_set_double_step(uft_kf_config_t *cfg, bool enabled);

/**
 * @brief Set retry count on read errors
 * 
 * @param cfg Configuration handle
 * @param retries Number of retries (0-20)
 * @return 0 on success, -1 on invalid value
 */
int uft_kf_set_retry_count(uft_kf_config_t *cfg, int retries);

/*============================================================================
 * PRESETS
 *============================================================================*/

/**
 * @brief Apply drive type preset
 * 
 * Configures track range and stepping based on drive type.
 * 
 * @param cfg Configuration handle
 * @param drive_type Drive type preset
 * @return 0 on success
 */
int uft_kf_apply_drive_preset(uft_kf_config_t *cfg, uft_kf_drive_type_t drive_type);

/**
 * @brief Apply platform preset
 * 
 * Configures all parameters for a specific platform/system.
 * 
 * @param cfg Configuration handle
 * @param platform Platform preset
 * @return 0 on success
 */
int uft_kf_apply_platform_preset(uft_kf_config_t *cfg, uft_kf_platform_t platform);

/**
 * @brief Get preset name
 * @param platform Platform preset
 * @return Human-readable name
 */
const char* uft_kf_platform_name(uft_kf_platform_t platform);

/**
 * @brief Get drive type name
 * @param drive_type Drive type
 * @return Human-readable name
 */
const char* uft_kf_drive_name(uft_kf_drive_type_t drive_type);

/*============================================================================
 * STATUS
 *============================================================================*/

/**
 * @brief Check if DTC is available
 * 
 * @param cfg Configuration handle
 * @return true if DTC found and executable
 */
bool uft_kf_is_available(const uft_kf_config_t *cfg);

/**
 * @brief Get DTC executable path
 * 
 * @param cfg Configuration handle
 * @return Path to DTC or NULL if not found
 */
const char* uft_kf_get_dtc_path(const uft_kf_config_t *cfg);

/**
 * @brief Get last error message
 * 
 * @param cfg Configuration handle
 * @return Error message string
 */
const char* uft_kf_get_error(const uft_kf_config_t *cfg);

/**
 * @brief Detect connected KryoFlux devices
 * 
 * @param cfg Configuration handle
 * @param device_count Output: number of devices found
 * @return 0 on success, -1 on error
 */
int uft_kf_detect_devices(uft_kf_config_t *cfg, int *device_count);

/*============================================================================
 * CAPTURE
 *============================================================================*/

/**
 * @brief Capture a single track
 * 
 * @param cfg Configuration handle
 * @param track Track number (0-83)
 * @param side Side (0 or 1)
 * @param flux Output: flux transition times (caller must free)
 * @param flux_count Output: number of transitions
 * @param index Output: index pulse positions (caller must free, can be NULL)
 * @param index_count Output: number of index pulses (can be NULL)
 * @return 0 on success, -1 on error
 */
int uft_kf_capture_track(uft_kf_config_t *cfg, int track, int side,
                          uint32_t **flux, size_t *flux_count,
                          uint32_t **index, size_t *index_count);

/**
 * @brief Capture entire disk with callback
 * 
 * Captures all tracks in the configured range and calls the callback
 * for each track. The callback receives the flux data and can process
 * or store it as needed.
 * 
 * @param cfg Configuration handle
 * @param callback Function called for each track
 * @param user_data User context passed to callback
 * @return Number of tracks captured, -1 on error
 */
int uft_kf_capture_disk(uft_kf_config_t *cfg, uft_kf_disk_callback_t callback,
                         void *user_data);

/*============================================================================
 * CONVERSION UTILITIES
 *============================================================================*/

/**
 * @brief Convert KryoFlux ticks to nanoseconds
 * @param ticks KryoFlux sample ticks
 * @return Time in nanoseconds
 */
double uft_kf_ticks_to_ns(uint32_t ticks);

/**
 * @brief Convert KryoFlux ticks to microseconds
 * @param ticks KryoFlux sample ticks
 * @return Time in microseconds
 */
double uft_kf_ticks_to_us(uint32_t ticks);

/**
 * @brief Convert nanoseconds to KryoFlux ticks
 * @param ns Time in nanoseconds
 * @return KryoFlux sample ticks
 */
uint32_t uft_kf_ns_to_ticks(double ns);

/**
 * @brief Convert microseconds to KryoFlux ticks
 * @param us Time in microseconds
 * @return KryoFlux sample ticks
 */
uint32_t uft_kf_us_to_ticks(double us);

/**
 * @brief Get KryoFlux sample clock frequency
 * @return Sample clock in Hz
 */
double uft_kf_get_sample_clock(void);

/*============================================================================
 * UFT PARAMETER INTEGRATION
 *============================================================================*/

/**
 * @brief Create config from UFT parameters
 * 
 * Reads settings from UFT parameter system:
 * - kryoflux.dtc_path
 * - kryoflux.device
 * - kryoflux.start_track
 * - kryoflux.end_track
 * - kryoflux.side
 * - kryoflux.revolutions
 * - kryoflux.double_step
 * - kryoflux.retry_count
 * - kryoflux.platform
 * 
 * @param params UFT parameter context (can be NULL for defaults)
 * @return Configuration handle or NULL on error
 */
uft_kf_config_t* uft_kf_config_from_params(const void *params);

/**
 * @brief Export config to UFT parameters
 * 
 * @param cfg Configuration handle
 * @param params UFT parameter context
 * @return 0 on success
 */
int uft_kf_config_to_params(const uft_kf_config_t *cfg, void *params);

#ifdef __cplusplus
}
#endif

/*============================================================================
 * WRITE OPERATIONS
 *============================================================================*/

/* Platform compatibility for ssize_t */
#ifdef _MSC_VER
    #include <BaseTsd.h>
    #ifndef _SSIZE_T_DEFINED
    #define _SSIZE_T_DEFINED
    typedef SSIZE_T ssize_t;
    #endif
#else
    #include <sys/types.h>
#endif

/**
 * @brief Write track to disk using DTC
 * 
 * DTC write mode uses the -w option with raw flux data.
 * Input format is KryoFlux RAW stream.
 * 
 * @param config Configuration
 * @param track Track number
 * @param side Side (0 or 1)
 * @param flux Flux timing data (in KryoFlux ticks)
 * @param count Number of flux transitions
 * @return 0 on success, -1 on error
 */
int uft_kf_write_track(uft_kf_config_t* config, int track, int side,
                        const uint32_t* flux, size_t count);

/**
 * @brief Write disk from raw files
 * 
 * Writes all tracks from a directory of .raw files.
 * 
 * @param config Configuration
 * @param input_dir Directory containing .raw files
 * @param callback Progress callback (optional)
 * @param user User data for callback
 * @return Number of tracks written, -1 on error
 */
int uft_kf_write_disk(uft_kf_config_t* config, const char* input_dir,
                       uft_kf_disk_callback_t callback, void* user);

/**
 * @brief Convert flux data to KryoFlux RAW format
 * 
 * Creates a .raw file from flux timing data.
 * 
 * @param flux Flux timing data
 * @param count Number of flux transitions
 * @param index Index pulse positions (optional)
 * @param index_count Number of index pulses
 * @param output Output buffer (caller provides)
 * @param max_size Maximum output size
 * @return Bytes written, -1 on error
 */
int uft_kf_flux_to_raw(const uint32_t* flux, size_t count,
                        const uint32_t* index, size_t index_count,
                        uint8_t* output, size_t max_size);

/**
 * @brief Check if write is supported
 * 
 * Not all KryoFlux firmware versions support writing.
 * 
 * @param config Configuration
 * @return true if write is supported
 */
bool uft_kf_write_supported(const uft_kf_config_t* config);

#endif /* UFT_KRYOFLUX_H */
