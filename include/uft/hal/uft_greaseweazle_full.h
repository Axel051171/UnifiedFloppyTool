/**
 * @file uft_greaseweazle.h
 * 
 * floppy disk reading and writing.
 * 
 * 
 * Features:
 * - USB device discovery and connection
 * - Firmware version detection
 * - Drive selection and motor control
 * - Flux reading with multi-revolution capture
 * - Flux writing with verification
 * - Index pulse synchronization
 * - Configurable sample rate
 * 
 * @version 1.0.0
 * @date 2025
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_UFT_GW_H
#define UFT_UFT_GW_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS & LIMITS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_GW_USB_VID              0x1209      /**< USB Vendor ID */
#define UFT_GW_USB_PID              0x4D69      /**< USB Product ID (Greaseweazle) */
#define UFT_GW_USB_PID_F7           0x4D69      /**< F7 variant */

/** Communication parameters */
#define UFT_GW_USB_TIMEOUT_MS       5000        /**< USB transfer timeout */
#define UFT_GW_MAX_CMD_SIZE         64          /**< Max command packet size */
#define UFT_GW_MAX_FLUX_CHUNK       65536       /**< Max flux data chunk */
#define UFT_GW_SAMPLE_FREQ_HZ       72000000    /**< F7 sample frequency (72 MHz) */
#define UFT_GW_SAMPLE_FREQ_F7_PLUS  84000000    /**< F7 Plus sample frequency */

/** Track limits */
#define UFT_GW_MAX_CYLINDERS        85          /**< Maximum cylinder number */
#define UFT_GW_MAX_HEADS            2           /**< Maximum head number */
#define UFT_GW_MAX_REVOLUTIONS      16          /**< Maximum revolutions to capture */

/** Timing constants (in sample ticks at 72 MHz) */
#define UFT_GW_INDEX_TIMEOUT_TICKS  (UFT_GW_SAMPLE_FREQ_HZ / 2)  /**< 500ms index timeout */
#define UFT_GW_SEEK_SETTLE_MS       15          /**< Head settle time after seek */
#define UFT_GW_MOTOR_SPINUP_MS      500         /**< Motor spin-up time */

/* ═══════════════════════════════════════════════════════════════════════════
 * PROTOCOL COMMANDS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 */
typedef enum uft_gw_cmd {
    /* Basic commands */
    UFT_GW_CMD_GET_INFO           = 0x00,   /**< Get device info */
    UFT_GW_CMD_UPDATE             = 0x01,   /**< Enter update mode */
    UFT_GW_CMD_SEEK               = 0x02,   /**< Seek to cylinder */
    UFT_GW_CMD_HEAD               = 0x03,   /**< Select head */
    UFT_GW_CMD_SET_PARAMS         = 0x04,   /**< Set parameters */
    UFT_GW_CMD_GET_PARAMS         = 0x05,   /**< Get parameters */
    UFT_GW_CMD_MOTOR              = 0x06,   /**< Motor on/off */
    UFT_GW_CMD_READ_FLUX          = 0x07,   /**< Read flux data */
    UFT_GW_CMD_WRITE_FLUX         = 0x08,   /**< Write flux data */
    UFT_GW_CMD_GET_FLUX_STATUS    = 0x09,   /**< Get flux read/write status */
    UFT_GW_CMD_GET_INDEX_TIMES    = 0x0A,   /**< Get index pulse times */
    UFT_GW_CMD_SWITCH_FW_MODE     = 0x0B,   /**< Switch firmware mode */
    UFT_GW_CMD_SELECT             = 0x0C,   /**< Select drive */
    UFT_GW_CMD_DESELECT           = 0x0D,   /**< Deselect drive */
    UFT_GW_CMD_SET_BUS_TYPE       = 0x0E,   /**< Set bus type (Shugart/IBM PC) */
    UFT_GW_CMD_SET_PIN            = 0x0F,   /**< Set output pin */
    UFT_GW_CMD_RESET              = 0x10,   /**< Reset device */
    UFT_GW_CMD_ERASE_FLUX         = 0x11,   /**< Erase track */
    UFT_GW_CMD_SOURCE_BYTES       = 0x12,   /**< Source bytes (write) */
    UFT_GW_CMD_SINK_BYTES         = 0x13,   /**< Sink bytes (read) */
    UFT_GW_CMD_GET_PIN            = 0x14,   /**< Get input pin */
    UFT_GW_CMD_TEST_MODE          = 0x15,   /**< Enter test mode */
    UFT_GW_CMD_NO_CLICK_STEP      = 0x16,   /**< Step without click */
    
    /* Extended commands (firmware 1.0+) */
    UFT_GW_CMD_READ_MEM           = 0x20,   /**< Read device memory */
    UFT_GW_CMD_WRITE_MEM          = 0x21,   /**< Write device memory */
    UFT_GW_CMD_GET_INFO_EXT       = 0x22,   /**< Get extended info */
    
    /* Bandwidh optimization commands (firmware 1.1+) */
    UFT_GW_CMD_SET_DRIVE_DELAYS   = 0x30,   /**< Set drive timing delays */
    UFT_GW_CMD_GET_DRIVE_DELAYS   = 0x31,   /**< Get drive timing delays */
} uft_gw_cmd_t;

/**
 */
typedef enum uft_gw_ack {
    UFT_GW_ACK_OK                 = 0x00,   /**< Success */
    UFT_GW_ACK_BAD_COMMAND        = 0x01,   /**< Unknown command */
    UFT_GW_ACK_NO_INDEX           = 0x02,   /**< No index pulse detected */
    UFT_GW_ACK_NO_TRK0            = 0x03,   /**< Track 0 sensor not found */
    UFT_GW_ACK_FLUX_OVERFLOW      = 0x04,   /**< Flux buffer overflow */
    UFT_GW_ACK_FLUX_UNDERFLOW     = 0x05,   /**< Flux buffer underflow */
    UFT_GW_ACK_WRPROT             = 0x06,   /**< Disk is write protected */
    UFT_GW_ACK_NO_UNIT            = 0x07,   /**< No drive unit selected */
    UFT_GW_ACK_NO_BUS             = 0x08,   /**< No bus type set */
    UFT_GW_ACK_BAD_UNIT           = 0x09,   /**< Invalid unit number */
    UFT_GW_ACK_BAD_PIN            = 0x0A,   /**< Invalid pin number */
    UFT_GW_ACK_BAD_CYLINDER       = 0x0B,   /**< Invalid cylinder number */
    UFT_GW_ACK_OUT_OF_SRAM        = 0x0C,   /**< Out of SRAM */
    UFT_GW_ACK_OUT_OF_FLASH       = 0x0D,   /**< Out of flash */
} uft_gw_ack_t;

/**
 * @brief Bus type selection
 */
typedef enum uft_gw_bus_type {
    UFT_GW_BUS_NONE               = 0,      /**< No bus configured */
    UFT_GW_BUS_IBM_PC             = 1,      /**< IBM PC (active low select) */
    UFT_GW_BUS_SHUGART            = 2,      /**< Shugart (active high select) */
} uft_gw_bus_type_t;

/**
 * @brief Drive type hints
 */
typedef enum uft_gw_drive_type {
    UFT_GW_DRIVE_UNKNOWN          = 0,      /**< Unknown drive type */
    UFT_GW_DRIVE_35_DD            = 1,      /**< 3.5" DD (720K) */
    UFT_GW_DRIVE_35_HD            = 2,      /**< 3.5" HD (1.44M) */
    UFT_GW_DRIVE_35_ED            = 3,      /**< 3.5" ED (2.88M) */
    UFT_GW_DRIVE_525_DD           = 4,      /**< 5.25" DD (360K) */
    UFT_GW_DRIVE_525_HD           = 5,      /**< 5.25" HD (1.2M) */
    UFT_GW_DRIVE_8_SD             = 6,      /**< 8" SD */
    UFT_GW_DRIVE_8_DD             = 7,      /**< 8" DD */
} uft_gw_drive_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Device information structure
 */
typedef struct uft_gw_info {
    uint8_t     fw_major;           /**< Firmware major version */
    uint8_t     fw_minor;           /**< Firmware minor version */
    uint8_t     is_main_fw;         /**< 1 if main firmware, 0 if bootloader */
    uint8_t     max_cmd;            /**< Maximum supported command */
    uint32_t    sample_freq;        /**< Sample frequency in Hz */
    uint8_t     hw_model;           /**< Hardware model */
    uint8_t     hw_submodel;        /**< Hardware sub-model */
    uint8_t     usb_speed;          /**< USB speed (1=Full, 2=High) */
    char        serial[32];         /**< Serial number string */
} uft_gw_info_t;

/**
 * @brief Drive delay parameters
 */
typedef struct uft_gw_delays {
    uint16_t    select_delay_us;    /**< Delay after drive select */
    uint16_t    step_delay_us;      /**< Delay after step pulse */
    uint16_t    settle_delay_ms;    /**< Head settle delay */
    uint16_t    motor_delay_ms;     /**< Motor spin-up delay */
    uint16_t    auto_off_ms;        /**< Auto motor-off timeout */
} uft_gw_delays_t;

/**
 * @brief Flux read parameters
 */
typedef struct uft_gw_read_params {
    uint8_t     revolutions;        /**< Number of revolutions to capture */
    bool        index_sync;         /**< Synchronize to index pulse */
    uint32_t    ticks;              /**< Max ticks to capture (0 = use revolutions) */
    bool        read_flux_ticks;    /**< Read in ticks (else raw bytes) */
} uft_gw_read_params_t;

/**
 * @brief Flux write parameters
 */
typedef struct uft_gw_write_params {
    bool        index_sync;         /**< Synchronize write to index */
    bool        erase_empty;        /**< Erase before write */
    bool        verify;             /**< Verify after write */
    uint32_t    pre_erase_ticks;    /**< Pre-erase time in ticks */
    uint32_t    terminate_at_index; /**< Terminate at Nth index (0 = continuous) */
} uft_gw_write_params_t;

/**
 * @brief Captured flux data from one read operation
 */
typedef struct uft_gw_flux_data {
    uint32_t*   samples;            /**< Flux timing samples (ticks) */
    uint32_t    sample_count;       /**< Number of samples */
    uint32_t*   index_times;        /**< Index pulse positions (ticks from start) */
    uint8_t     index_count;        /**< Number of index pulses captured */
    uint32_t    total_ticks;        /**< Total capture time in ticks */
    uint8_t     status;             /**< Capture status (uft_gw_ack_t) */
    uint32_t    sample_freq;        /**< Sample frequency used */
} uft_gw_flux_data_t;

/**
 * @brief Device handle (opaque)
 */
typedef struct uft_gw_device uft_gw_device_t;

/**
 * @brief Progress callback for long operations
 */
typedef void (*uft_gw_progress_cb)(void* user_data, int percent, const char* message);

/**
 * @brief Device discovery callback
 */
typedef void (*uft_gw_discover_cb)(void* user_data, const char* port, const uft_gw_info_t* info);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: DEVICE DISCOVERY & CONNECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @param callback Called for each discovered device
 * @param user_data User context passed to callback
 * @return Number of devices found
 */
int uft_gw_discover(uft_gw_discover_cb callback, void* user_data);

/**
 * @param ports Output array of port names
 * @param max_ports Maximum ports to return
 * @return Number of ports found
 */
int uft_gw_list_ports(char** ports, int max_ports);

/**
 * @param port Serial port name (e.g., "/dev/ttyACM0", "COM3")
 * @param device Output: device handle
 * @return 0 on success, error code on failure
 */
int uft_gw_open(const char* port, uft_gw_device_t** device);

/**
 * @param device Output: device handle
 * @return 0 on success, error code on failure
 */
int uft_gw_open_first(uft_gw_device_t** device);

/**
 * @brief Close device connection
 * @param device Device handle (can be NULL)
 */
void uft_gw_close(uft_gw_device_t* device);

/**
 * @brief Check if device is connected and responsive
 * @param device Device handle
 * @return true if connected
 */
bool uft_gw_is_connected(uft_gw_device_t* device);

/**
 * @brief Reset device to known state
 * @param device Device handle
 * @return 0 on success, error code on failure
 */
int uft_gw_reset(uft_gw_device_t* device);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: DEVICE INFORMATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get device information
 * @param device Device handle
 * @param info Output: device info structure
 * @return 0 on success, error code on failure
 */
int uft_gw_get_info(uft_gw_device_t* device, uft_gw_info_t* info);

/**
 * @brief Get firmware version string
 * @param device Device handle
 * @return Version string (e.g., "1.4") or NULL
 */
const char* uft_gw_get_version_string(uft_gw_device_t* device);

/**
 * @brief Get device serial number
 * @param device Device handle
 * @return Serial number string or NULL
 */
const char* uft_gw_get_serial(uft_gw_device_t* device);

/**
 * @brief Get sample frequency
 * @param device Device handle
 * @return Sample frequency in Hz
 */
uint32_t uft_gw_get_sample_freq(uft_gw_device_t* device);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: DRIVE CONTROL
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Set bus type (Shugart or IBM PC)
 * @param device Device handle
 * @param bus_type Bus type to use
 * @return 0 on success, error code on failure
 */
int uft_gw_set_bus_type(uft_gw_device_t* device, uft_gw_bus_type_t bus_type);

/**
 * @brief Select drive unit
 * @param device Device handle
 * @param unit Drive unit number (0 or 1)
 * @return 0 on success, error code on failure
 */
int uft_gw_select_drive(uft_gw_device_t* device, uint8_t unit);

/**
 * @brief Deselect current drive
 * @param device Device handle
 * @return 0 on success, error code on failure
 */
int uft_gw_deselect_drive(uft_gw_device_t* device);

/**
 * @brief Set motor state
 * @param device Device handle
 * @param on true to turn motor on
 * @return 0 on success, error code on failure
 */
int uft_gw_set_motor(uft_gw_device_t* device, bool on);

/**
 * @brief Seek to cylinder
 * @param device Device handle
 * @param cylinder Target cylinder (0-84)
 * @return 0 on success, error code on failure
 */
int uft_gw_seek(uft_gw_device_t* device, uint8_t cylinder);

/**
 * @brief Select head
 * @param device Device handle
 * @param head Head number (0 or 1)
 * @return 0 on success, error code on failure
 */
int uft_gw_select_head(uft_gw_device_t* device, uint8_t head);

/**
 * @brief Get current cylinder position
 * @param device Device handle
 * @return Current cylinder or -1 on error
 */
int uft_gw_get_cylinder(uft_gw_device_t* device);

/**
 * @brief Get current head
 * @param device Device handle
 * @return Current head (0 or 1) or -1 on error
 */
int uft_gw_get_head(uft_gw_device_t* device);

/**
 * @brief Check if disk is write protected
 * @param device Device handle
 * @return true if write protected, false if not or error
 */
bool uft_gw_is_write_protected(uft_gw_device_t* device);

/**
 * @brief Set drive timing delays
 * @param device Device handle
 * @param delays Delay parameters
 * @return 0 on success, error code on failure
 */
int uft_gw_set_delays(uft_gw_device_t* device, const uft_gw_delays_t* delays);

/**
 * @brief Get drive timing delays
 * @param device Device handle
 * @param delays Output: delay parameters
 * @return 0 on success, error code on failure
 */
int uft_gw_get_delays(uft_gw_device_t* device, uft_gw_delays_t* delays);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: FLUX READING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read flux data from current track position
 * @param device Device handle
 * @param params Read parameters
 * @param flux Output: captured flux data (caller must free with uft_gw_flux_free)
 * @return 0 on success, error code on failure
 */
int uft_gw_read_flux(uft_gw_device_t* device, const uft_gw_read_params_t* params,
                 uft_gw_flux_data_t** flux);

/**
 * @brief Read flux data with default parameters
 * @param device Device handle
 * @param revolutions Number of revolutions to capture
 * @param flux Output: captured flux data
 * @return 0 on success, error code on failure
 */
int uft_gw_read_flux_simple(uft_gw_device_t* device, uint8_t revolutions,
                        uft_gw_flux_data_t** flux);

/**
 * @brief Read raw flux samples directly
 * @param device Device handle
 * @param buffer Output buffer for raw samples
 * @param buffer_size Buffer size in bytes
 * @param bytes_read Output: actual bytes read
 * @return 0 on success, error code on failure
 */
int uft_gw_read_flux_raw(uft_gw_device_t* device, uint8_t* buffer, size_t buffer_size,
                     size_t* bytes_read);

/**
 * @brief Free flux data structure
 * @param flux Flux data to free (can be NULL)
 */
void uft_gw_flux_free(uft_gw_flux_data_t* flux);

/**
 * @brief Get index times from last read
 * @param device Device handle
 * @param times Output array for index times (ticks)
 * @param max_times Maximum times to return
 * @return Number of index times retrieved
 */
int uft_gw_get_index_times(uft_gw_device_t* device, uint32_t* times, int max_times);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: FLUX WRITING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Write flux data to current track position
 * @param device Device handle
 * @param params Write parameters
 * @param samples Flux timing samples (ticks)
 * @param sample_count Number of samples
 * @return 0 on success, error code on failure
 */
int uft_gw_write_flux(uft_gw_device_t* device, const uft_gw_write_params_t* params,
                  const uint32_t* samples, uint32_t sample_count);

/**
 * @brief Write flux data with default parameters
 * @param device Device handle
 * @param samples Flux timing samples (ticks)
 * @param sample_count Number of samples
 * @return 0 on success, error code on failure
 */
int uft_gw_write_flux_simple(uft_gw_device_t* device, const uint32_t* samples,
                         uint32_t sample_count);

/**
 * @brief Erase track
 * @param device Device handle
 * @param revolutions Number of revolutions to erase
 * @return 0 on success, error code on failure
 */
int uft_gw_erase_track(uft_gw_device_t* device, uint8_t revolutions);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: HIGH-LEVEL OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read a complete track (seek + select head + read flux)
 * @param device Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param revolutions Number of revolutions
 * @param flux Output: captured flux data
 * @return 0 on success, error code on failure
 */
int uft_gw_read_track(uft_gw_device_t* device, uint8_t cylinder, uint8_t head,
                  uint8_t revolutions, uft_gw_flux_data_t** flux);

/**
 * @brief Write a complete track (seek + select head + write flux)
 * @param device Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param samples Flux timing samples
 * @param sample_count Number of samples
 * @return 0 on success, error code on failure
 */
int uft_gw_write_track(uft_gw_device_t* device, uint8_t cylinder, uint8_t head,
                   const uint32_t* samples, uint32_t sample_count);

/**
 * @brief Recalibrate drive (seek to track 0)
 * @param device Device handle
 * @return 0 on success, error code on failure
 */
int uft_gw_recalibrate(uft_gw_device_t* device);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: CONVERSION UTILITIES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert ticks to nanoseconds
 * @param ticks Tick count
 * @param sample_freq Sample frequency in Hz
 * @return Time in nanoseconds
 */
static inline uint32_t uft_gw_ticks_to_ns(uint32_t ticks, uint32_t sample_freq) {
    return (uint32_t)(((uint64_t)ticks * 1000000000ULL) / sample_freq);
}

/**
 * @brief Convert nanoseconds to ticks
 * @param ns Time in nanoseconds
 * @param sample_freq Sample frequency in Hz
 * @return Tick count
 */
static inline uint32_t uft_gw_ns_to_ticks(uint32_t ns, uint32_t sample_freq) {
    return (uint32_t)(((uint64_t)ns * sample_freq) / 1000000000ULL);
}

/**
 * @brief Convert flux ticks to RPM
 * @param ticks Ticks for one revolution
 * @param sample_freq Sample frequency in Hz
 * @return RPM × 100 (e.g., 30000 = 300.00 RPM)
 */
static inline uint32_t uft_gw_ticks_to_rpm(uint32_t ticks, uint32_t sample_freq) {
    if (ticks == 0) return 0;
    return (uint32_t)((60ULL * sample_freq * 100) / ticks);
}

/**
 * @param raw Raw flux stream bytes
 * @param raw_len Raw stream length
 * @param samples Output: decoded samples (caller allocates)
 * @param max_samples Maximum samples to decode
 * @param sample_freq Output: sample frequency used
 * @return Number of samples decoded
 */
uint32_t uft_gw_decode_flux_stream(const uint8_t* raw, size_t raw_len,
                                uint32_t* samples, uint32_t max_samples,
                                uint32_t* sample_freq);

/**
 * @param samples Flux samples (ticks)
 * @param sample_count Number of samples
 * @param raw Output: encoded stream (caller allocates)
 * @param max_raw Maximum raw bytes
 * @return Number of bytes encoded
 */
size_t uft_gw_encode_flux_stream(const uint32_t* samples, uint32_t sample_count,
                              uint8_t* raw, size_t max_raw);

/* ═══════════════════════════════════════════════════════════════════════════
 * ERROR CODES
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_GW_OK                    0
#define UFT_GW_ERR_NOT_FOUND        -1      /**< Device not found */
#define UFT_GW_ERR_OPEN_FAILED      -2      /**< Failed to open device */
#define UFT_GW_ERR_IO               -3      /**< I/O error */
#define UFT_GW_ERR_TIMEOUT          -4      /**< Operation timed out */
#define UFT_GW_ERR_PROTOCOL         -5      /**< Protocol error */
#define UFT_GW_ERR_NO_INDEX         -6      /**< No index pulse detected */
#define UFT_GW_ERR_NO_TRK0          -7      /**< Track 0 not found */
#define UFT_GW_ERR_OVERFLOW         -8      /**< Buffer overflow */
#define UFT_GW_ERR_UNDERFLOW        -9      /**< Buffer underflow */
#define UFT_GW_ERR_WRPROT           -10     /**< Write protected */
#define UFT_GW_ERR_INVALID          -11     /**< Invalid parameter */
#define UFT_GW_ERR_NOMEM            -12     /**< Out of memory */
#define UFT_GW_ERR_NOT_CONNECTED    -13     /**< Device not connected */
#define UFT_GW_ERR_UNSUPPORTED      -14     /**< Operation not supported */

/**
 * @brief Get error message for error code
 * @param err Error code
 * @return Static error message string
 */
const char* uft_gw_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif /* UFT_UFT_GW_H */
