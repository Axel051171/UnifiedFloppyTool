/**
 * @file uft_pauline.h
 * @brief Pauline Floppy Controller Support
 * 
 * Pauline is a professional-grade floppy disk controller/analyzer
 * developed by Jean-François DEL NERO (HxC Floppy Emulator developer).
 * 
 * Features:
 * - High-precision flux capture (up to 100MHz sampling)
 * - Direct floppy drive control
 * - Index pulse timing capture
 * - Multi-revolution capture
 * - Write precompensation
 * - Real-time flux analysis
 * 
 * Communication: TCP/IP socket or USB serial
 * 
 * @see https://github.com/jfdelnero/Pauline
 * @copyright MIT License
 */

#ifndef UFT_PAULINE_H
#define UFT_PAULINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_PAULINE_DEFAULT_PORT        7629
#define UFT_PAULINE_MAX_TRACKS          84
#define UFT_PAULINE_MAX_HEADS           2
#define UFT_PAULINE_MAX_BUFFER          (16 * 1024 * 1024)  /* 16MB max buffer */

#define UFT_PAULINE_SAMPLE_RATE_25MHZ   25000000
#define UFT_PAULINE_SAMPLE_RATE_50MHZ   50000000
#define UFT_PAULINE_SAMPLE_RATE_100MHZ  100000000

/* Command codes */
#define UFT_PAULINE_CMD_INIT            0x01
#define UFT_PAULINE_CMD_RESET           0x02
#define UFT_PAULINE_CMD_GET_INFO        0x10
#define UFT_PAULINE_CMD_SET_DRIVE       0x20
#define UFT_PAULINE_CMD_MOTOR_ON        0x30
#define UFT_PAULINE_CMD_MOTOR_OFF       0x31
#define UFT_PAULINE_CMD_SEEK            0x40
#define UFT_PAULINE_CMD_RECALIBRATE     0x41
#define UFT_PAULINE_CMD_SELECT_HEAD     0x50
#define UFT_PAULINE_CMD_READ_TRACK      0x60
#define UFT_PAULINE_CMD_WRITE_TRACK     0x70
#define UFT_PAULINE_CMD_READ_INDEX      0x80
#define UFT_PAULINE_CMD_GET_STATUS      0x90

/* Status codes */
#define UFT_PAULINE_STATUS_OK           0x00
#define UFT_PAULINE_STATUS_ERROR        0x01
#define UFT_PAULINE_STATUS_NO_DISK      0x02
#define UFT_PAULINE_STATUS_WRITE_PROT   0x03
#define UFT_PAULINE_STATUS_NO_INDEX     0x04
#define UFT_PAULINE_STATUS_TIMEOUT      0x05

/* Drive types */
#define UFT_PAULINE_DRIVE_35_DD         0x01
#define UFT_PAULINE_DRIVE_35_HD         0x02
#define UFT_PAULINE_DRIVE_525_DD        0x03
#define UFT_PAULINE_DRIVE_525_HD        0x04
#define UFT_PAULINE_DRIVE_8_SD          0x05

/* ═══════════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Connection type
 */
typedef enum uft_pauline_conn_type {
    UFT_PAULINE_CONN_TCP,           /**< TCP/IP socket */
    UFT_PAULINE_CONN_USB,           /**< USB serial */
} uft_pauline_conn_type_t;

/**
 * @brief Device information
 */
typedef struct uft_pauline_info {
    char        firmware_version[32];
    char        hardware_version[32];
    uint32_t    capabilities;
    uint32_t    max_sample_rate;
    uint32_t    buffer_size;
    uint8_t     num_drives;
} uft_pauline_info_t;

/**
 * @brief Drive status
 */
typedef struct uft_pauline_status {
    bool        connected;
    bool        motor_on;
    bool        disk_present;
    bool        write_protected;
    bool        index_detected;
    uint8_t     current_track;
    uint8_t     current_head;
    uint16_t    rpm;
} uft_pauline_status_t;

/**
 * @brief Read parameters
 */
typedef struct uft_pauline_read_params {
    uint32_t    sample_rate;        /**< Sample rate in Hz */
    uint8_t     revolutions;        /**< Number of revolutions to capture */
    bool        index_sync;         /**< Sync to index pulse */
    bool        capture_index;      /**< Capture index timing */
    uint32_t    timeout_ms;         /**< Timeout in milliseconds */
} uft_pauline_read_params_t;

/**
 * @brief Captured flux data
 */
typedef struct uft_pauline_flux {
    uint8_t    *data;               /**< Raw flux data */
    size_t      data_size;          /**< Data size in bytes */
    size_t      bit_count;          /**< Number of bits */
    uint32_t   *index_times;        /**< Index pulse timestamps */
    size_t      index_count;        /**< Number of index pulses */
    uint32_t    sample_rate;        /**< Actual sample rate used */
    uint8_t     track;              /**< Track number */
    uint8_t     head;               /**< Head number */
} uft_pauline_flux_t;

/**
 * @brief Write parameters
 */
typedef struct uft_pauline_write_params {
    uint32_t    data_rate;          /**< Data rate in bits/sec */
    bool        precomp_enable;     /**< Enable write precompensation */
    uint8_t     precomp_ns;         /**< Precompensation in nanoseconds */
    bool        verify;             /**< Verify after write */
} uft_pauline_write_params_t;

/**
 * @brief Pauline device handle
 */
typedef struct uft_pauline_device {
    uft_pauline_conn_type_t conn_type;
    union {
        int         socket_fd;      /**< TCP socket */
        int         serial_fd;      /**< Serial port handle */
    };
    char            host[256];      /**< Hostname/IP for TCP */
    uint16_t        port;           /**< TCP port */
    char            serial_port[64];/**< Serial port name */
    uft_pauline_info_t info;        /**< Device info */
    uft_pauline_status_t status;    /**< Current status */
    uint8_t        *rx_buffer;      /**< Receive buffer */
    size_t          rx_buffer_size; /**< Buffer size */
    bool            connected;      /**< Connection state */
} uft_pauline_device_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Connection Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Connect to Pauline via TCP/IP
 * @param dev Device handle to initialize
 * @param host Hostname or IP address
 * @param port Port number (default: 7629)
 * @return 0 on success, error code on failure
 */
int uft_pauline_connect_tcp(uft_pauline_device_t *dev, 
                            const char *host, uint16_t port);

/**
 * @brief Connect to Pauline via USB serial
 * @param dev Device handle to initialize
 * @param port Serial port name (e.g., "/dev/ttyUSB0", "COM3")
 * @return 0 on success, error code on failure
 */
int uft_pauline_connect_usb(uft_pauline_device_t *dev, const char *port);

/**
 * @brief Disconnect from Pauline
 */
void uft_pauline_disconnect(uft_pauline_device_t *dev);

/**
 * @brief Check if connected
 */
bool uft_pauline_is_connected(const uft_pauline_device_t *dev);

/**
 * @brief Get device information
 */
int uft_pauline_get_info(uft_pauline_device_t *dev, uft_pauline_info_t *info);

/**
 * @brief Get current status
 */
int uft_pauline_get_status(uft_pauline_device_t *dev, uft_pauline_status_t *status);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Drive Control
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Select drive
 * @param dev Device handle
 * @param drive Drive number (0 or 1)
 * @param type Drive type
 * @return 0 on success
 */
int uft_pauline_select_drive(uft_pauline_device_t *dev, uint8_t drive, uint8_t type);

/**
 * @brief Turn motor on
 */
int uft_pauline_motor_on(uft_pauline_device_t *dev);

/**
 * @brief Turn motor off
 */
int uft_pauline_motor_off(uft_pauline_device_t *dev);

/**
 * @brief Seek to track
 * @param dev Device handle
 * @param track Target track number
 * @return 0 on success
 */
int uft_pauline_seek(uft_pauline_device_t *dev, uint8_t track);

/**
 * @brief Recalibrate (seek to track 0)
 */
int uft_pauline_recalibrate(uft_pauline_device_t *dev);

/**
 * @brief Select head
 * @param dev Device handle
 * @param head Head number (0 or 1)
 */
int uft_pauline_select_head(uft_pauline_device_t *dev, uint8_t head);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize read parameters with defaults
 */
void uft_pauline_read_params_init(uft_pauline_read_params_t *params);

/**
 * @brief Read raw flux from current track
 * @param dev Device handle
 * @param params Read parameters
 * @param flux Output flux data (caller must free with uft_pauline_flux_free)
 * @return 0 on success
 */
int uft_pauline_read_flux(uft_pauline_device_t *dev,
                          const uft_pauline_read_params_t *params,
                          uft_pauline_flux_t *flux);

/**
 * @brief Read track (convenience function)
 * 
 * Combines seek + select head + read flux.
 */
int uft_pauline_read_track(uft_pauline_device_t *dev,
                           uint8_t track, uint8_t head,
                           const uft_pauline_read_params_t *params,
                           uft_pauline_flux_t *flux);

/**
 * @brief Free flux data
 */
void uft_pauline_flux_free(uft_pauline_flux_t *flux);

/**
 * @brief Measure RPM
 * @param dev Device handle
 * @param rpm Output: measured RPM
 * @return 0 on success
 */
int uft_pauline_measure_rpm(uft_pauline_device_t *dev, uint16_t *rpm);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize write parameters with defaults
 */
void uft_pauline_write_params_init(uft_pauline_write_params_t *params);

/**
 * @brief Write flux to current track
 * @param dev Device handle
 * @param params Write parameters
 * @param data Flux data to write
 * @param data_size Data size in bytes
 * @return 0 on success
 */
int uft_pauline_write_flux(uft_pauline_device_t *dev,
                           const uft_pauline_write_params_t *params,
                           const uint8_t *data, size_t data_size);

/**
 * @brief Write track (convenience function)
 */
int uft_pauline_write_track(uft_pauline_device_t *dev,
                            uint8_t track, uint8_t head,
                            const uft_pauline_write_params_t *params,
                            const uint8_t *data, size_t data_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Conversion Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert Pauline raw format to HFE
 */
int uft_pauline_to_hfe(const uft_pauline_flux_t *flux, 
                       uint8_t **hfe_data, size_t *hfe_size);

/**
 * @brief Convert Pauline raw format to SCP
 */
int uft_pauline_to_scp(const uft_pauline_flux_t *flux,
                       uint8_t **scp_data, size_t *scp_size);

/**
 * @brief Convert Pauline raw format to MFM bitstream
 * 
 * Uses the fdc_bitstream library for decoding.
 */
int uft_pauline_to_mfm(const uft_pauline_flux_t *flux,
                       uint8_t **mfm_data, size_t *mfm_bits);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PAULINE_H */
