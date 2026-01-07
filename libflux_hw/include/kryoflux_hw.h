// SPDX-License-Identifier: MIT
/*
 * 
 * 
 * USB VID/PID: 0x16d0/0x0498
 * 
 *   - dtc binary analysis
 *   - kryoflux-ui.jar analysis
 * 
 * @version 2.7.2
 * @date 2024-12-25
 */

#ifndef UFT_UFT_UFT_KF_HW_H
#define UFT_UFT_UFT_KF_HW_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * USB DEVICE INFO
 *============================================================================*/

#define UFT_UFT_KF_USB_VID    0x16d0  /**< USB Vendor ID */
#define UFT_UFT_KF_USB_PID    0x0498  /**< USB Product ID */

#define UFT_UFT_KF_EP_IN      0x86    /**< Bulk IN endpoint (flux data) */
#define UFT_UFT_KF_EP_OUT     0x06    /**< Bulk OUT endpoint (commands) */

/*============================================================================*
 * STREAM FORMAT
 *============================================================================*/

/**
 * @brief Stream opcodes (from dtc analysis)
 */
typedef enum {
    UFT_KF_OPCODE_FLUX_SHORT_0  = 0x00,  /**< Flux: 0 × 2µs */
    UFT_KF_OPCODE_FLUX_SHORT_1  = 0x01,  /**< Flux: 1 × 2µs */
    UFT_KF_OPCODE_FLUX_SHORT_2  = 0x02,  /**< Flux: 2 × 2µs */
    UFT_KF_OPCODE_FLUX_SHORT_3  = 0x03,  /**< Flux: 3 × 2µs */
    UFT_KF_OPCODE_FLUX_SHORT_4  = 0x04,  /**< Flux: 4 × 2µs */
    UFT_KF_OPCODE_FLUX_SHORT_5  = 0x05,  /**< Flux: 5 × 2µs */
    UFT_KF_OPCODE_FLUX_SHORT_6  = 0x06,  /**< Flux: 6 × 2µs */
    UFT_KF_OPCODE_FLUX_SHORT_7  = 0x07,  /**< Flux: 7 × 2µs */
    UFT_KF_OPCODE_NOP           = 0x08,  /**< Multiple NOPs (1-255 × 2µs) */
    UFT_KF_OPCODE_OVERFLOW      = 0x09,  /**< 16-bit overflow value */
    UFT_KF_OPCODE_OOB           = 0x0A,  /**< Out-of-Band data */
    UFT_KF_OPCODE_INFO          = 0x0B,  /**< Stream info */
    UFT_KF_OPCODE_INDEX         = 0x0C,  /**< Index pulse marker */
    UFT_KF_OPCODE_END           = 0x0D   /**< End of stream */
} uft_kf_opcode_t;

/**
 * @brief Out-of-Band (OOB) data types
 */
typedef enum {
    UFT_KF_OOB_INVALID      = 0x00,
    UFT_KF_OOB_STREAM_INFO  = 0x01,  /**< Stream information */
    UFT_KF_OOB_INDEX        = 0x02,  /**< Index pulse position */
    UFT_KF_OOB_STREAM_END   = 0x03,  /**< End of stream */
    UFT_KF_OOB_UFT_KF_INFO      = 0x04,  /**< KryoFlux device info */
    UFT_KF_OOB_EOF          = 0x0D   /**< End of file */
} uft_kf_oob_type_t;

/*============================================================================*
 * DEVICE HANDLE
 *============================================================================*/

/**
 */
typedef struct uft_kf_device uft_kf_device_t;

/*============================================================================*
 * STREAM DATA
 *============================================================================*/

/**
 * @brief Flux transition timing
 */
typedef struct {
    uint32_t timing_ns;      /**< Timing in nanoseconds */
    bool is_index;           /**< True if index pulse */
} uft_kf_flux_transition_t;

/**
 * @brief Stream read result
 */
typedef struct {
    uft_kf_flux_transition_t *transitions;  /**< Array of flux transitions */
    size_t transition_count;             /**< Number of transitions */
    
    // Index pulse info
    uint32_t *index_positions;           /**< Index pulse positions */
    size_t index_count;                  /**< Number of indices */
    
    // Statistics
    uint64_t total_time_ns;              /**< Total track time */
    uint32_t rpm;                        /**< Detected RPM */
    
} uft_kf_stream_result_t;

/*============================================================================*
 * READ OPTIONS
 *============================================================================*/

/**
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
    
} uft_kf_read_opts_t;

/*============================================================================*
 * ERROR TYPES (From kryoflux-ui.jar analysis)
 *============================================================================*/

/**
 * @brief Error severity levels
 */
typedef enum {
    UFT_KF_SEVERITY_INFO     = 0,
    UFT_KF_SEVERITY_WARNING  = 1,
    UFT_KF_SEVERITY_ERROR    = 2,
    UFT_KF_SEVERITY_CRITICAL = 3
} uft_kf_error_severity_t;

/**
 * @brief Error domains
 */
typedef enum {
    UFT_KF_ERROR_HARDWARE    = 0,  /**< Hardware/device error */
    UFT_KF_ERROR_READ        = 1,  /**< Read operation error */
    UFT_KF_ERROR_FORMAT      = 2,  /**< Format decoding error */
    UFT_KF_ERROR_DATA        = 3,  /**< Data integrity error */
    UFT_KF_ERROR_STREAM      = 4   /**< Stream processing error */
} uft_kf_error_domain_t;

/**
 * @brief Specific error codes
 */
typedef enum {
    UFT_KF_ERROR_NONE            = 0,
    UFT_KF_ERROR_TRANSFER        = 1,  /**< USB transfer error */
    UFT_KF_ERROR_BUFFERING       = 2,  /**< Buffer overflow */
    UFT_KF_ERROR_INDEX_MISSING   = 3,  /**< No disk in drive */
    UFT_KF_ERROR_INVALID_CODE    = 4,  /**< Invalid stream opcode */
    UFT_KF_ERROR_INVALID_OOB     = 5,  /**< Invalid OOB data */
    UFT_KF_ERROR_NO_OOB_END      = 6,  /**< Missing OOB end marker */
    UFT_KF_ERROR_STREAM_TOO_LONG = 7,  /**< Stream exceeds buffer */
    UFT_KF_ERROR_INCOMPLETE      = 8,  /**< Incomplete stream */
    UFT_KF_ERROR_BAD_POSITION    = 9,  /**< Bad stream position */
    UFT_KF_ERROR_BAD_INDEX       = 10  /**< Bad index reference */
} uft_kf_error_code_t;

/**
 * @brief Error information
 */
typedef struct {
    uft_kf_error_code_t code;
    uft_kf_error_severity_t severity;
    uft_kf_error_domain_t domain;
    char message[256];
} uft_kf_error_info_t;

/*============================================================================*
 * DEVICE API
 *============================================================================*/

/**
 */
int uft_kf_init(void);

/**
 */
void uft_kf_shutdown(void);

/**
 * 
 * @param device_index Device index (default: 0)
 * @param device_out Device handle (output)
 * @return 0 on success, negative on error
 */
int uft_kf_open(int device_index, uft_kf_device_t **device_out);

/**
 * 
 * @param device Device handle
 */
void uft_kf_close(uft_kf_device_t *device);

/**
 * @brief Get device information
 * 
 * @param device Device handle
 * @param info_out Information string (output, 256 bytes)
 * @return 0 on success, negative on error
 */
int uft_kf_get_device_info(
    uft_kf_device_t *device,
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
int uft_kf_read_track(
    uft_kf_device_t *device,
    const uft_kf_read_opts_t *opts,
    uft_kf_stream_result_t *result_out
);

/**
 * @brief Free stream result
 * 
 * @param result Result to free
 */
void uft_kf_free_stream_result(uft_kf_stream_result_t *result);

/*============================================================================*
 * STREAM UTILITIES
 *============================================================================*/

/**
 * @brief Decode stream file
 * 
 * 
 * @param filename Stream file path
 * @param result_out Decoded stream (output)
 * @return 0 on success, negative on error
 */
int uft_kf_decode_stream_file(
    const char *filename,
    uft_kf_stream_result_t *result_out
);

/**
 * @brief Write stream file
 * 
 * 
 * @param filename Output file path
 * @param stream Stream data
 * @return 0 on success, negative on error
 */
int uft_kf_write_stream_file(
    const char *filename,
    const uft_kf_stream_result_t *stream
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
int uft_kf_stream_to_ufm_track(
    const uft_kf_stream_result_t *stream,
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
int uft_kf_detect_weak_bits_from_flux(
    const uft_kf_stream_result_t *streams,
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
int uft_kf_get_last_error(
    uft_kf_device_t *device,
    uft_kf_error_info_t *error_out
);

/**
 * @brief Print error
 * 
 * @param error Error to print
 */
void uft_kf_print_error(const uft_kf_error_info_t *error);

/*============================================================================*
 * UTILITIES
 *============================================================================*/

/**
 * 
 * @param count_out Number of devices found
 * @return 0 on success, negative on error
 */
int uft_kf_detect_devices(int *count_out);

/**
 * @brief Get default read options
 * 
 * @param opts Options structure to fill
 */
void uft_kf_get_default_opts(uft_kf_read_opts_t *opts);

/**
 * @brief Calculate RPM from flux stream
 * 
 * @param stream Flux stream
 * @return RPM or 0 on error
 */
uint32_t uft_uft_kf_calculate_rpm(const uft_kf_stream_result_t *stream);

#ifdef __cplusplus
}
#endif

#endif /* UFT_UFT_UFT_KF_HW_H */
