/**
 * @file uft_hal.h
 * @brief Hardware Abstraction Layer - Unified interface for flux controllers
 * 
 * The HAL provides a unified interface for different flux imaging hardware,
 * converting their native formats to/from UFT-IR.
 * 
 * Supported Controllers:
 * - FC5025 (planned)
 * - XUM1541 (planned)
 * 
 * @version 1.0.0
 * @date 2025
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_HAL_H
#define UFT_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_ir_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * CONTROLLER TYPES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Hardware controller type
 */
typedef enum uft_hal_controller {
    UFT_HAL_NONE              = 0,
    UFT_HAL_GREASEWEAZLE      = 1,
    UFT_HAL_FLUXENGINE        = 2,
    UFT_HAL_KRYOFLUX          = 3,
    UFT_HAL_FC5025            = 4,
    UFT_HAL_XUM1541           = 5,
    UFT_HAL_SUPERCARD_PRO     = 6,
    UFT_HAL_PAULINE           = 7,
    UFT_HAL_APPLESAUCE        = 8,
} uft_hal_controller_t;

/**
 * @brief Drive profile for common drive types
 */
typedef enum uft_hal_drive_profile {
    UFT_HAL_DRIVE_AUTO        = 0,   /**< Auto-detect */
    UFT_HAL_DRIVE_35_DD       = 1,   /**< 3.5" DD (720K) */
    UFT_HAL_DRIVE_35_HD       = 2,   /**< 3.5" HD (1.44M) */
    UFT_HAL_DRIVE_35_ED       = 3,   /**< 3.5" ED (2.88M) */
    UFT_HAL_DRIVE_525_DD      = 4,   /**< 5.25" DD (360K) */
    UFT_HAL_DRIVE_525_HD      = 5,   /**< 5.25" HD (1.2M) */
    UFT_HAL_DRIVE_8_SD        = 6,   /**< 8" SD */
    UFT_HAL_DRIVE_8_DD        = 7,   /**< 8" DD */
    UFT_HAL_DRIVE_C64_1541    = 8,   /**< Commodore 1541 */
    UFT_HAL_DRIVE_AMIGA_DD    = 9,   /**< Amiga DD */
    UFT_HAL_DRIVE_AMIGA_HD    = 10,  /**< Amiga HD */
    UFT_HAL_DRIVE_APPLE_525   = 11,  /**< Apple II 5.25" */
    UFT_HAL_DRIVE_APPLE_35    = 12,  /**< Apple 3.5" */
} uft_hal_drive_profile_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Controller information
 */
typedef struct uft_hal_info {
    uft_hal_controller_t type;      /**< Controller type */
    char        name[64];           /**< Controller name */
    char        version[32];        /**< Firmware/version string */
    char        serial[64];         /**< Serial number */
    char        port[256];          /**< Port/device path */
    uint32_t    sample_freq;        /**< Sample frequency in Hz */
    uint8_t     max_drives;         /**< Maximum drives supported */
    bool        can_write;          /**< Write capability */
    bool        supports_hd;        /**< HD support */
    bool        supports_ed;        /**< ED support */
} uft_hal_info_t;

/**
 * @brief Read operation parameters
 */
typedef struct uft_hal_read_params {
    uint8_t     cylinder_start;     /**< Starting cylinder */
    uint8_t     cylinder_end;       /**< Ending cylinder (inclusive) */
    uint8_t     head_mask;          /**< Heads to read (bit 0 = head 0, bit 1 = head 1) */
    uint8_t     revolutions;        /**< Revolutions per track */
    uint8_t     retries;            /**< Retry count on errors */
    bool        index_sync;         /**< Synchronize to index pulse */
    bool        skip_empty;         /**< Skip unformatted tracks */
    uft_hal_drive_profile_t profile;/**< Drive profile */
} uft_hal_read_params_t;

/**
 * @brief Write operation parameters
 */
typedef struct uft_hal_write_params {
    uint8_t     cylinder_start;     /**< Starting cylinder */
    uint8_t     cylinder_end;       /**< Ending cylinder (inclusive) */
    uint8_t     head_mask;          /**< Heads to write */
    bool        verify;             /**< Verify after write */
    bool        erase_empty;        /**< Erase unwritten tracks */
    uft_hal_drive_profile_t profile;/**< Drive profile */
} uft_hal_write_params_t;

/**
 * @brief Progress callback information
 */
typedef struct uft_hal_progress {
    uint8_t     cylinder;           /**< Current cylinder */
    uint8_t     head;               /**< Current head */
    uint8_t     revolution;         /**< Current revolution */
    uint8_t     retry;              /**< Current retry count */
    int         percent;            /**< Overall progress 0-100 */
    const char* message;            /**< Status message */
    bool        error;              /**< Error occurred */
    int         error_code;         /**< Error code if error */
} uft_hal_progress_t;

/**
 * @brief Progress callback function type
 */
typedef bool (*uft_hal_progress_cb)(void* user_data, const uft_hal_progress_t* progress);

/**
 * @brief HAL device handle (opaque)
 */
typedef struct uft_hal_device uft_hal_device_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * API: DEVICE DISCOVERY
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Discovery callback
 */
typedef void (*uft_hal_discover_cb)(void* user_data, const uft_hal_info_t* info);

/**
 * @brief Discover all connected controllers
 * @param callback Called for each discovered device
 * @param user_data User context passed to callback
 * @return Number of devices found
 */
int uft_hal_discover(uft_hal_discover_cb callback, void* user_data);

/**
 * @brief Get list of available controllers
 * @param infos Output array
 * @param max_count Maximum entries
 * @return Number of controllers found
 */
int uft_hal_list(uft_hal_info_t* infos, int max_count);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: DEVICE CONNECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open a specific controller
 * @param type Controller type
 * @param port Port/device path (NULL for first available)
 * @param device Output: device handle
 * @return 0 on success, error code on failure
 */
int uft_hal_open(uft_hal_controller_t type, const char* port,
                 uft_hal_device_t** device);

/**
 * @brief Open first available controller
 * @param device Output: device handle
 * @return 0 on success, error code on failure
 */
int uft_hal_open_first(uft_hal_device_t** device);

/**
 * @brief Close device connection
 * @param device Device handle
 */
void uft_hal_close(uft_hal_device_t* device);

/**
 * @brief Get device information
 * @param device Device handle
 * @param info Output: device info
 * @return 0 on success, error code on failure
 */
int uft_hal_get_info(uft_hal_device_t* device, uft_hal_info_t* info);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: DRIVE CONTROL
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Select drive unit
 * @param device Device handle
 * @param unit Drive unit (0 or 1)
 * @return 0 on success, error code on failure
 */
int uft_hal_select_drive(uft_hal_device_t* device, uint8_t unit);

/**
 * @brief Set drive profile
 * @param device Device handle
 * @param profile Drive profile
 * @return 0 on success, error code on failure
 */
int uft_hal_set_profile(uft_hal_device_t* device, uft_hal_drive_profile_t profile);

/**
 * @brief Recalibrate drive (seek to track 0)
 * @param device Device handle
 * @return 0 on success, error code on failure
 */
int uft_hal_recalibrate(uft_hal_device_t* device);

/**
 * @brief Check if disk is write protected
 * @param device Device handle
 * @return true if write protected
 */
bool uft_hal_is_write_protected(uft_hal_device_t* device);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: READING - UFT-IR OUTPUT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read single track to UFT-IR format
 * @param device Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param revolutions Number of revolutions
 * @param track Output: UFT-IR track (caller must free)
 * @return 0 on success, error code on failure
 */
int uft_hal_read_track(uft_hal_device_t* device, uint8_t cylinder, uint8_t head,
                       uint8_t revolutions, uft_ir_track_t** track);

/**
 * @brief Read entire disk to UFT-IR format
 * @param device Device handle
 * @param params Read parameters
 * @param progress Progress callback (can be NULL)
 * @param user_data User context for callback
 * @param disk Output: UFT-IR disk (caller must free)
 * @return 0 on success, error code on failure
 */
int uft_hal_read_disk(uft_hal_device_t* device, const uft_hal_read_params_t* params,
                      uft_hal_progress_cb progress, void* user_data,
                      uft_ir_disk_t** disk);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: WRITING - UFT-IR INPUT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Write single track from UFT-IR format
 * @param device Device handle
 * @param track UFT-IR track data
 * @return 0 on success, error code on failure
 */
int uft_hal_write_track(uft_hal_device_t* device, const uft_ir_track_t* track);

/**
 * @brief Write entire disk from UFT-IR format
 * @param device Device handle
 * @param disk UFT-IR disk data
 * @param params Write parameters
 * @param progress Progress callback (can be NULL)
 * @param user_data User context for callback
 * @return 0 on success, error code on failure
 */
int uft_hal_write_disk(uft_hal_device_t* device, const uft_ir_disk_t* disk,
                       const uft_hal_write_params_t* params,
                       uft_hal_progress_cb progress, void* user_data);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: LOW-LEVEL ACCESS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Seek to cylinder
 * @param device Device handle
 * @param cylinder Target cylinder
 * @return 0 on success, error code on failure
 */
int uft_hal_seek(uft_hal_device_t* device, uint8_t cylinder);

/**
 * @brief Select head
 * @param device Device handle
 * @param head Head number (0 or 1)
 * @return 0 on success, error code on failure
 */
int uft_hal_select_head(uft_hal_device_t* device, uint8_t head);

/**
 * @brief Control motor
 * @param device Device handle
 * @param on true to turn on
 * @return 0 on success, error code on failure
 */
int uft_hal_set_motor(uft_hal_device_t* device, bool on);

/**
 * @brief Erase track
 * @param device Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @return 0 on success, error code on failure
 */
int uft_hal_erase_track(uft_hal_device_t* device, uint8_t cylinder, uint8_t head);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: UTILITIES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get default read parameters for drive profile
 * @param profile Drive profile
 * @param params Output: read parameters
 */
void uft_hal_get_default_read_params(uft_hal_drive_profile_t profile,
                                      uft_hal_read_params_t* params);

/**
 * @brief Get default write parameters for drive profile
 * @param profile Drive profile
 * @param params Output: write parameters
 */
void uft_hal_get_default_write_params(uft_hal_drive_profile_t profile,
                                       uft_hal_write_params_t* params);

/**
 * @brief Get controller type name
 * @param type Controller type
 * @return Static name string
 */
const char* uft_hal_controller_name(uft_hal_controller_t type);

/**
 * @brief Get drive profile name
 * @param profile Drive profile
 * @return Static name string
 */
const char* uft_hal_profile_name(uft_hal_drive_profile_t profile);

/* ═══════════════════════════════════════════════════════════════════════════
 * ERROR CODES
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_HAL_OK                   0
#define UFT_HAL_ERR_NOT_FOUND       -1
#define UFT_HAL_ERR_OPEN_FAILED     -2
#define UFT_HAL_ERR_IO              -3
#define UFT_HAL_ERR_TIMEOUT         -4
#define UFT_HAL_ERR_NO_INDEX        -5
#define UFT_HAL_ERR_NO_TRK0         -6
#define UFT_HAL_ERR_OVERFLOW        -7
#define UFT_HAL_ERR_WRPROT          -8
#define UFT_HAL_ERR_INVALID         -9
#define UFT_HAL_ERR_NOMEM           -10
#define UFT_HAL_ERR_NOT_CONNECTED   -11
#define UFT_HAL_ERR_UNSUPPORTED     -12
#define UFT_HAL_ERR_CANCELLED       -13

/**
 * @brief Get error message
 * @param err Error code
 * @return Static error message string
 */
const char* uft_hal_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HAL_H */
