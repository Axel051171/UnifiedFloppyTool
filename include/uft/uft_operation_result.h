/**
 * @file uft_operation_result.h
 * @brief Unified Operation Result System for UnifiedFloppyTool
 * 
 * This header defines the standard result structures used for all major
 * operations (Read, Decode, Analyze, Write, Convert). GUI and backend
 * communicate exclusively through these structures.
 * 
 * Design Principles:
 * - All operations return a result object
 * - No implicit success - every operation has explicit status
 * - Statistics always populated (zero if not applicable)
 * - Messages are actionable, not generic
 * 
 * @version 1.0.0
 * @date 2026-01-08
 */

#ifndef UFT_OPERATION_RESULT_H
#define UFT_OPERATION_RESULT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define UFT_RESULT_MSG_MAX      512
#define UFT_RESULT_DETAIL_MAX   2048
#define UFT_RESULT_PATH_MAX     260

/* ============================================================================
 * Operation Types
 * ============================================================================ */

/**
 * @brief Operation type enumeration
 */
typedef enum {
    UFT_OP_UNKNOWN = 0,
    UFT_OP_READ,          /**< Disk/flux read operation */
    UFT_OP_DECODE,        /**< Flux/track decode operation */
    UFT_OP_ANALYZE,       /**< Format/protection analysis */
    UFT_OP_WRITE,         /**< Disk/image write operation */
    UFT_OP_CONVERT,       /**< Format conversion */
    UFT_OP_VERIFY,        /**< Data verification */
    UFT_OP_RECOVER,       /**< Data recovery */
    UFT_OP_COPY,          /**< Disk copy (XCopy) */
    UFT_OP_DETECT,        /**< Format detection */
    UFT_OP_VALIDATE       /**< Image validation */
} uft_operation_type_t;

/**
 * @brief Operation status (more granular than success/fail)
 */
typedef enum {
    UFT_STATUS_PENDING = 0,     /**< Operation not started */
    UFT_STATUS_RUNNING,         /**< Operation in progress */
    UFT_STATUS_SUCCESS,         /**< Completed successfully */
    UFT_STATUS_PARTIAL,         /**< Completed with some errors */
    UFT_STATUS_FAILED,          /**< Failed completely */
    UFT_STATUS_CANCELLED,       /**< Cancelled by user */
    UFT_STATUS_TIMEOUT,         /**< Operation timed out */
    UFT_STATUS_NOT_IMPLEMENTED  /**< Feature not implemented */
} uft_operation_status_t;

/* ============================================================================
 * Track/Sector Statistics
 * ============================================================================ */

/**
 * @brief Track-level statistics
 */
typedef struct {
    uint32_t total;          /**< Total tracks processed */
    uint32_t good;           /**< Tracks with no errors */
    uint32_t weak;           /**< Tracks with weak bits */
    uint32_t bad;            /**< Tracks with unrecoverable errors */
    uint32_t skipped;        /**< Tracks skipped */
} uft_track_stats_t;

/**
 * @brief Sector-level statistics
 */
typedef struct {
    uint32_t total;          /**< Total sectors processed */
    uint32_t good;           /**< Sectors with valid CRC */
    uint32_t crc_error;      /**< Sectors with CRC errors */
    uint32_t header_error;   /**< Sectors with header errors */
    uint32_t missing;        /**< Missing sectors */
    uint32_t recovered;      /**< Sectors recovered */
    uint32_t weak_bits;      /**< Sectors with weak bits */
} uft_sector_stats_t;

/**
 * @brief Byte-level statistics
 */
typedef struct {
    uint64_t total_read;     /**< Total bytes read */
    uint64_t total_written;  /**< Total bytes written */
    uint64_t good;           /**< Good bytes */
    uint64_t uncertain;      /**< Bytes with uncertainty */
    uint64_t bad;            /**< Unrecoverable bytes */
} uft_byte_stats_t;

/* ============================================================================
 * Timing Information
 * ============================================================================ */

/**
 * @brief Operation timing information
 */
typedef struct {
    time_t start_time;       /**< Operation start time (epoch) */
    time_t end_time;         /**< Operation end time (epoch) */
    uint32_t elapsed_ms;     /**< Elapsed time in milliseconds */
    uint32_t estimated_ms;   /**< Estimated remaining time */
    float progress;          /**< Progress 0.0 - 1.0 */
} uft_timing_t;

/* ============================================================================
 * Main Result Structure
 * ============================================================================ */

/**
 * @brief Unified operation result
 * 
 * This structure is the primary communication mechanism between
 * backend operations and the GUI. All major operations must
 * return a properly filled uft_operation_result_t.
 * 
 * Usage:
 * @code
 * uft_operation_result_t result = {0};
 * uft_result_init(&result, UFT_OP_READ);
 * 
 * // ... perform operation ...
 * 
 * if (all_good) {
 *     uft_result_set_success(&result, "Read completed");
 * } else {
 *     uft_result_set_error(&result, UFT_ERR_CRC, "CRC errors on tracks 5, 12");
 * }
 * 
 * // GUI reads result
 * if (result.status == UFT_STATUS_SUCCESS) { ... }
 * @endcode
 */
typedef struct uft_operation_result {
    /* === Identification === */
    uft_operation_type_t operation;    /**< Type of operation */
    uft_operation_status_t status;     /**< Current status */
    uft_rc_t error_code;               /**< Error code if failed */
    
    /* === Messages === */
    char message[UFT_RESULT_MSG_MAX];  /**< Human-readable summary */
    char detail[UFT_RESULT_DETAIL_MAX];/**< Detailed information/log */
    
    /* === Statistics === */
    uft_track_stats_t tracks;          /**< Track statistics */
    uft_sector_stats_t sectors;        /**< Sector statistics */
    uft_byte_stats_t bytes;            /**< Byte statistics */
    
    /* === Timing === */
    uft_timing_t timing;               /**< Timing information */
    
    /* === Source/Destination === */
    char source_path[UFT_RESULT_PATH_MAX];  /**< Input file/device */
    char dest_path[UFT_RESULT_PATH_MAX];    /**< Output file/device */
    
    /* === Format Information === */
    uint32_t format_id;                /**< Detected/used format ID */
    char format_name[64];              /**< Format name string */
    
    /* === Flags === */
    uint32_t flags;                    /**< Operation-specific flags */
    
    /* === Extension Point === */
    void* user_data;                   /**< Custom data (caller owns) */
} uft_operation_result_t;

/* ============================================================================
 * Convenience Macros
 * ============================================================================ */

/**
 * @brief Check if operation was successful
 */
#define UFT_RESULT_OK(r) ((r)->status == UFT_STATUS_SUCCESS)

/**
 * @brief Check if operation failed
 */
#define UFT_RESULT_FAILED(r) ((r)->status == UFT_STATUS_FAILED)

/**
 * @brief Check if operation completed (success or partial)
 */
#define UFT_RESULT_COMPLETED(r) \
    ((r)->status == UFT_STATUS_SUCCESS || (r)->status == UFT_STATUS_PARTIAL)

/**
 * @brief Calculate sector error rate (0.0 - 1.0)
 */
#define UFT_SECTOR_ERROR_RATE(r) \
    ((r)->sectors.total > 0 ? \
     (float)((r)->sectors.crc_error + (r)->sectors.missing) / (r)->sectors.total : 0.0f)

/* ============================================================================
 * Result Functions
 * ============================================================================ */

/**
 * @brief Initialize a result structure
 * 
 * @param result Result structure to initialize
 * @param operation Type of operation
 */
void uft_result_init(uft_operation_result_t* result, uft_operation_type_t operation);

/**
 * @brief Set result to success status
 * 
 * @param result Result structure
 * @param message Success message
 */
void uft_result_set_success(uft_operation_result_t* result, const char* message);

/**
 * @brief Set result to partial success status
 * 
 * @param result Result structure
 * @param message Status message
 */
void uft_result_set_partial(uft_operation_result_t* result, const char* message);

/**
 * @brief Set result to error status
 * 
 * @param result Result structure
 * @param code Error code
 * @param message Error message
 */
void uft_result_set_error(uft_operation_result_t* result, 
                          uft_rc_t code, const char* message);

/**
 * @brief Append detail text to result
 * 
 * @param result Result structure
 * @param detail Detail text to append
 */
void uft_result_append_detail(uft_operation_result_t* result, const char* detail);

/**
 * @brief Update progress
 * 
 * @param result Result structure
 * @param progress Progress value 0.0 - 1.0
 */
void uft_result_set_progress(uft_operation_result_t* result, float progress);

/**
 * @brief Start timing for operation
 * 
 * @param result Result structure
 */
void uft_result_start_timing(uft_operation_result_t* result);

/**
 * @brief Stop timing for operation
 * 
 * @param result Result structure
 */
void uft_result_stop_timing(uft_operation_result_t* result);

/**
 * @brief Get operation type as string
 * 
 * @param op Operation type
 * @return Static string
 */
const char* uft_operation_type_str(uft_operation_type_t op);

/**
 * @brief Get status as string
 * 
 * @param status Status value
 * @return Static string
 */
const char* uft_operation_status_str(uft_operation_status_t status);

/**
 * @brief Generate summary string for result
 * 
 * @param result Result structure
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written
 */
size_t uft_result_summary(const uft_operation_result_t* result,
                          char* buffer, size_t buffer_size);

/* ============================================================================
 * Specialized Result Initializers
 * ============================================================================ */

/**
 * @brief Create a "not implemented" result
 * 
 * @param result Result structure
 * @param operation Operation type
 * @param feature_name Name of unimplemented feature
 */
void uft_result_not_implemented(uft_operation_result_t* result,
                                uft_operation_type_t operation,
                                const char* feature_name);

/**
 * @brief Create a "hardware not connected" result
 * 
 * @param result Result structure
 * @param device_name Device name
 */
void uft_result_no_hardware(uft_operation_result_t* result,
                            const char* device_name);

/**
 * @brief Create a "cancelled" result
 * 
 * @param result Result structure
 */
void uft_result_cancelled(uft_operation_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_OPERATION_RESULT_H */
