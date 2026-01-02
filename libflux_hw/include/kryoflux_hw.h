// SPDX-License-Identifier: MIT
/*
 * kryoflux_hw.h - KryoFlux Hardware API
 * 
 * Professional flux-level disk preservation with KryoFlux hardware.
 * 
 * Hardware: KryoFlux "Rosalie" USB device
 * USB VID/PID: 0x16d0/0x0498
 * 
 * Features extracted from:
 *   - disk-utilities (Keir Fraser)
 *   - dtc binary analysis
 *   - kryoflux-ui.jar analysis
 * 
 * @version 2.7.2
 * @date 2024-12-25
 */

#ifndef UFT_KRYOFLUX_HW_H
#define UFT_KRYOFLUX_HW_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * USB DEVICE INFO
 *============================================================================*/

#define KRYOFLUX_USB_VID    0x16d0  /**< USB Vendor ID */
#define KRYOFLUX_USB_PID    0x0498  /**< USB Product ID */

#define KRYOFLUX_EP_IN      0x86    /**< Bulk IN endpoint (flux data) */
#define KRYOFLUX_EP_OUT     0x06    /**< Bulk OUT endpoint (commands) */

/*============================================================================*
 * STREAM FORMAT
 *============================================================================*/

/**
 * @brief Stream opcodes (from dtc analysis)
 */
typedef enum {
    KF_OPCODE_FLUX_SHORT_0  = 0x00,  /**< Flux: 0 × 2µs */
    KF_OPCODE_FLUX_SHORT_1  = 0x01,  /**< Flux: 1 × 2µs */
    KF_OPCODE_FLUX_SHORT_2  = 0x02,  /**< Flux: 2 × 2µs */
    KF_OPCODE_FLUX_SHORT_3  = 0x03,  /**< Flux: 3 × 2µs */
    KF_OPCODE_FLUX_SHORT_4  = 0x04,  /**< Flux: 4 × 2µs */
    KF_OPCODE_FLUX_SHORT_5  = 0x05,  /**< Flux: 5 × 2µs */
    KF_OPCODE_FLUX_SHORT_6  = 0x06,  /**< Flux: 6 × 2µs */
    KF_OPCODE_FLUX_SHORT_7  = 0x07,  /**< Flux: 7 × 2µs */
    KF_OPCODE_NOP           = 0x08,  /**< Multiple NOPs (1-255 × 2µs) */
    KF_OPCODE_OVERFLOW      = 0x09,  /**< 16-bit overflow value */
    KF_OPCODE_OOB           = 0x0A,  /**< Out-of-Band data */
    KF_OPCODE_INFO          = 0x0B,  /**< Stream info */
    KF_OPCODE_INDEX         = 0x0C,  /**< Index pulse marker */
    KF_OPCODE_END           = 0x0D   /**< End of stream */
} kf_opcode_t;

/**
 * @brief Out-of-Band (OOB) data types
 */
typedef enum {
    KF_OOB_INVALID      = 0x00,
    KF_OOB_STREAM_INFO  = 0x01,  /**< Stream information */
    KF_OOB_INDEX        = 0x02,  /**< Index pulse position */
    KF_OOB_STREAM_END   = 0x03,  /**< End of stream */
    KF_OOB_KF_INFO      = 0x04,  /**< KryoFlux device info */
    KF_OOB_EOF          = 0x0D   /**< End of file */
} kf_oob_type_t;

/*============================================================================*
 * DEVICE HANDLE
 *============================================================================*/

/**
 * @brief KryoFlux device handle (opaque)
 */
typedef struct kryoflux_device kryoflux_device_t;

/*============================================================================*
 * STREAM DATA
 *============================================================================*/

/**
 * @brief Flux transition timing
 */
typedef struct {
    uint32_t timing_ns;      /**< Timing in nanoseconds */
    bool is_index;           /**< True if index pulse */
} kf_flux_transition_t;

/**
 * @brief Stream read result
 */
typedef struct {
    kf_flux_transition_t *transitions;  /**< Array of flux transitions */
    size_t transition_count;             /**< Number of transitions */
    
    // Index pulse info
    uint32_t *index_positions;           /**< Index pulse positions */
    size_t index_count;                  /**< Number of indices */
    
    // Statistics
    uint64_t total_time_ns;              /**< Total track time */
    uint32_t rpm;                        /**< Detected RPM */
    
} kf_stream_result_t;

/*============================================================================*
 * READ OPTIONS
 *============================================================================*/

/**
 * @brief KryoFlux read options
 */
typedef struct {
    // Track selection
    uint8_t cylinder;        /**< Cylinder (0-79) */
    uint8_t head;            /**< Head (0-1) */
    
    // Read parameters
    uint8_t revolutions;     /**< Revolutions to read (1-10, default: 5) */
    uint8_t retries;         /**< Retry count on error (default: 3) */
    
    // Mode
    bool preservation_mode;  /**< True = raw flux, False = format guided */
    
    // RPM
    uint16_t target_rpm;     /**< Target RPM (default: 300 for Amiga) */
    
} kf_read_opts_t;

/*============================================================================*
 * ERROR TYPES (From kryoflux-ui.jar analysis)
 *============================================================================*/

/**
 * @brief Error severity levels
 */
typedef enum {
    KF_SEVERITY_INFO     = 0,
    KF_SEVERITY_WARNING  = 1,
    KF_SEVERITY_ERROR    = 2,
    KF_SEVERITY_CRITICAL = 3
} kf_error_severity_t;

/**
 * @brief Error domains
 */
typedef enum {
    KF_ERROR_HARDWARE    = 0,  /**< Hardware/device error */
    KF_ERROR_READ        = 1,  /**< Read operation error */
    KF_ERROR_FORMAT      = 2,  /**< Format decoding error */
    KF_ERROR_DATA        = 3,  /**< Data integrity error */
    KF_ERROR_STREAM      = 4   /**< Stream processing error */
} kf_error_domain_t;

/**
 * @brief Specific error codes
 */
typedef enum {
    KF_ERROR_NONE            = 0,
    KF_ERROR_TRANSFER        = 1,  /**< USB transfer error */
    KF_ERROR_BUFFERING       = 2,  /**< Buffer overflow */
    KF_ERROR_INDEX_MISSING   = 3,  /**< No disk in drive */
    KF_ERROR_INVALID_CODE    = 4,  /**< Invalid stream opcode */
    KF_ERROR_INVALID_OOB     = 5,  /**< Invalid OOB data */
    KF_ERROR_NO_OOB_END      = 6,  /**< Missing OOB end marker */
    KF_ERROR_STREAM_TOO_LONG = 7,  /**< Stream exceeds buffer */
    KF_ERROR_INCOMPLETE      = 8,  /**< Incomplete stream */
    KF_ERROR_BAD_POSITION    = 9,  /**< Bad stream position */
    KF_ERROR_BAD_INDEX       = 10  /**< Bad index reference */
} kf_error_code_t;

/**
 * @brief Error information
 */
typedef struct {
    kf_error_code_t code;
    kf_error_severity_t severity;
    kf_error_domain_t domain;
    char message[256];
} kf_error_info_t;

/*============================================================================*
 * DEVICE API
 *============================================================================*/

/**
 * @brief Initialize KryoFlux subsystem
 */
int kryoflux_init(void);

/**
 * @brief Shutdown KryoFlux subsystem
 */
void kryoflux_shutdown(void);

/**
 * @brief Open KryoFlux device
 * 
 * @param device_index Device index (default: 0)
 * @param device_out Device handle (output)
 * @return 0 on success, negative on error
 */
int kryoflux_open(int device_index, kryoflux_device_t **device_out);

/**
 * @brief Close KryoFlux device
 * 
 * @param device Device handle
 */
void kryoflux_close(kryoflux_device_t *device);

/**
 * @brief Get device information
 * 
 * @param device Device handle
 * @param info_out Information string (output, 256 bytes)
 * @return 0 on success, negative on error
 */
int kryoflux_get_device_info(
    kryoflux_device_t *device,
    char *info_out
);

/*============================================================================*
 * READ API
 *============================================================================*/

/**
 * @brief Read track as flux stream
 * 
 * Reads flux transitions from hardware and decodes stream format.
 * 
 * STREAM DECODING:
 *   - Opcodes 0x00-0x0D
 *   - OOB data handling
 *   - Index pulse detection
 *   - Timing conversion (2µs units → ns)
 * 
 * SYNERGY WITH WEAK BITS (v2.7.1):
 *   - Multi-revolution reading
 *   - Flux variance detection
 *   - Weak bit identification!
 * 
 * @param device Device handle
 * @param opts Read options
 * @param result_out Stream result (allocated by this function)
 * @return 0 on success, negative on error
 */
int kryoflux_read_track(
    kryoflux_device_t *device,
    const kf_read_opts_t *opts,
    kf_stream_result_t *result_out
);

/**
 * @brief Free stream result
 * 
 * @param result Result to free
 */
void kryoflux_free_stream_result(kf_stream_result_t *result);

/*============================================================================*
 * STREAM UTILITIES
 *============================================================================*/

/**
 * @brief Decode stream file
 * 
 * Reads a KryoFlux stream file (.raw) and decodes it.
 * 
 * @param filename Stream file path
 * @param result_out Decoded stream (output)
 * @return 0 on success, negative on error
 */
int kryoflux_decode_stream_file(
    const char *filename,
    kf_stream_result_t *result_out
);

/**
 * @brief Write stream file
 * 
 * Saves flux stream to KryoFlux format file.
 * 
 * @param filename Output file path
 * @param stream Stream data
 * @return 0 on success, negative on error
 */
int kryoflux_write_stream_file(
    const char *filename,
    const kf_stream_result_t *stream
);

/*============================================================================*
 * UFM INTEGRATION
 *============================================================================*/

/**
 * @brief Convert flux stream to UFM track
 * 
 * Decodes flux timings to logical data.
 * Integrates with weak bit detection (v2.7.1)!
 * 
 * @param stream Flux stream
 * @param track_out UFM track (output)
 * @return 0 on success, negative on error
 */
int kryoflux_stream_to_ufm_track(
    const kf_stream_result_t *stream,
    void *track_out  // ufm_track_t pointer
);

/**
 * @brief Detect weak bits from flux variance
 * 
 * SYNERGY WITH v2.7.1!
 * Analyzes flux timing variance to detect weak bits.
 * 
 * @param streams Array of streams (multiple revolutions)
 * @param stream_count Number of revolutions
 * @param weak_bits_out Weak bit results (output)
 * @return 0 on success, negative on error
 */
int kryoflux_detect_weak_bits_from_flux(
    const kf_stream_result_t *streams,
    size_t stream_count,
    void *weak_bits_out  // weak_bit_result_t pointer
);

/*============================================================================*
 * ERROR HANDLING
 *============================================================================*/

/**
 * @brief Get last error
 * 
 * @param device Device handle
 * @param error_out Error information (output)
 * @return 0 if error available
 */
int kryoflux_get_last_error(
    kryoflux_device_t *device,
    kf_error_info_t *error_out
);

/**
 * @brief Print error
 * 
 * @param error Error to print
 */
void kryoflux_print_error(const kf_error_info_t *error);

/*============================================================================*
 * UTILITIES
 *============================================================================*/

/**
 * @brief Detect available KryoFlux devices
 * 
 * @param count_out Number of devices found
 * @return 0 on success, negative on error
 */
int kryoflux_detect_devices(int *count_out);

/**
 * @brief Get default read options
 * 
 * @param opts Options structure to fill
 */
void kryoflux_get_default_opts(kf_read_opts_t *opts);

/**
 * @brief Calculate RPM from flux stream
 * 
 * @param stream Flux stream
 * @return RPM or 0 on error
 */
uint32_t kryoflux_calculate_rpm(const kf_stream_result_t *stream);

#ifdef __cplusplus
}
#endif

#endif /* UFT_KRYOFLUX_HW_H */
