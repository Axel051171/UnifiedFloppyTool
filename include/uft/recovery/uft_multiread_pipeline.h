/**
 * @file uft_multiread_pipeline.h
 * @brief Unified Multi-Read Recovery Pipeline API
 * 
 * High-level API for multi-pass reading with majority voting.
 * Combines multiple read attempts to recover data from damaged
 * or degraded floppy disks.
 * 
 * Features:
 * - Multi-pass reading with automatic retry
 * - Byte-level majority voting across reads
 * - Confidence scoring (0-100 per byte)
 * - Weak bit detection
 * - Adaptive read strategy
 * - Report generation
 * 
 * @author UFT Team
 * @version 3.5.0
 * @date 2026-01-03
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_MULTIREAD_PIPELINE_H
#define UFT_MULTIREAD_PIPELINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Maximum read passes supported */
#define MULTIREAD_MAX_PASSES        16

/** Default number of read passes */
#define MULTIREAD_DEFAULT_PASSES    5

/** Minimum confidence for successful recovery */
#define MULTIREAD_MIN_CONFIDENCE    75

/** Default majority vote percentage */
#define MULTIREAD_MAJORITY_PCT      66

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    MULTIREAD_OK = 0,               /**< Success */
    MULTIREAD_ERR_NULL_PARAM,       /**< Null parameter */
    MULTIREAD_ERR_ALLOC,            /**< Memory allocation failed */
    MULTIREAD_ERR_NO_DATA,          /**< No data available */
    MULTIREAD_ERR_READ_FAILED,      /**< Read operation failed */
    MULTIREAD_ERR_INSUFFICIENT_PASSES, /**< Not enough read passes */
    MULTIREAD_ERR_LOW_CONFIDENCE,   /**< Confidence below threshold */
    MULTIREAD_ERR_CRC_FAILED,       /**< CRC verification failed */
    MULTIREAD_ERR_COUNT
} multiread_error_t;

/*============================================================================
 * Opaque Types
 *============================================================================*/

typedef struct multiread_ctx_s multiread_ctx_t;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief Sector recovery result
 */
typedef struct {
    uint8_t     track;              /**< Track number */
    uint8_t     head;               /**< Head (side) */
    uint8_t     sector;             /**< Sector number */
    uint8_t    *data;               /**< Recovered data */
    size_t      data_len;           /**< Data length */
    uint8_t     confidence;         /**< Recovery confidence (0-100) */
    uint8_t     good_reads;         /**< Number of good reads */
    uint8_t     total_reads;        /**< Total read attempts */
    bool        recovered;          /**< Successfully recovered */
    bool        has_weak_bits;      /**< Contains weak/uncertain bits */
    uint8_t    *weak_mask;          /**< Weak bit mask (optional) */
} multiread_sector_t;

/**
 * @brief Track recovery result
 */
typedef struct {
    uint8_t     track;              /**< Track number */
    uint8_t     head;               /**< Head (side) */
    multiread_sector_t *sectors;    /**< Array of sectors */
    uint8_t     sector_count;       /**< Number of sectors */
    uint8_t     good_sectors;       /**< Sectors with 100% confidence */
    uint8_t     recovered_sectors;  /**< Sectors recovered with voting */
    uint8_t     failed_sectors;     /**< Unrecoverable sectors */
    uint8_t     overall_confidence; /**< Track-level confidence */
} multiread_track_t;

/**
 * @brief Pipeline configuration
 */
typedef struct {
    uint8_t     min_passes;         /**< Minimum read passes (default: 3) */
    uint8_t     max_passes;         /**< Maximum read passes (default: 5) */
    uint8_t     min_confidence;     /**< Minimum required confidence (default: 75) */
    uint8_t     majority_pct;       /**< Majority vote percentage (default: 66) */
    bool        adaptive_passes;    /**< Increase passes on failure (default: true) */
    bool        detect_weak_bits;   /**< Enable weak bit detection (default: true) */
    bool        generate_report;    /**< Generate detailed report (default: false) */
    
    /* Callbacks */
    void       *user_data;          /**< User data for callbacks */
    
    /**
     * @brief Read callback function
     * @param user_data User data pointer
     * @param track Track number
     * @param head Head number
     * @param data Output buffer
     * @param len Input: buffer size, Output: bytes read
     * @return 0 on success (CRC OK), >0 on CRC error, <0 on read failure
     */
    int       (*read_callback)(void *user_data, uint8_t track, uint8_t head,
                               uint8_t *data, size_t *len);
    
    /**
     * @brief Progress callback function
     * @param user_data User data pointer
     * @param track Current track
     * @param head Current head
     * @param pass Current pass number
     * @param total_passes Total passes planned
     */
    void      (*progress_callback)(void *user_data, uint8_t track, uint8_t head,
                                   uint8_t pass, uint8_t total_passes);
} multiread_config_t;

/*============================================================================
 * Lifecycle Functions
 *============================================================================*/

/**
 * @brief Get default configuration
 * @return Default configuration structure
 */
multiread_config_t multiread_config_default(void);

/**
 * @brief Create pipeline context
 * @param config Configuration (NULL for defaults)
 * @return Context pointer or NULL on failure
 */
multiread_ctx_t *multiread_create(const multiread_config_t *config);

/**
 * @brief Destroy pipeline context
 * @param ctx Context to destroy
 */
void multiread_destroy(multiread_ctx_t *ctx);

/*============================================================================
 * Pass Management
 *============================================================================*/

/**
 * @brief Add a read pass manually
 * @param ctx Pipeline context
 * @param data Read data
 * @param len Data length
 * @param quality Read quality (0-100)
 * @param crc_ok CRC verification passed
 * @return MULTIREAD_OK on success
 */
multiread_error_t multiread_add_pass(multiread_ctx_t *ctx,
                                      const uint8_t *data,
                                      size_t len,
                                      uint8_t quality,
                                      bool crc_ok);

/*============================================================================
 * Execution
 *============================================================================*/

/**
 * @brief Execute voting on added passes
 * @param ctx Pipeline context
 * @param output Output buffer (caller allocates)
 * @param output_len Output buffer size
 * @param result Recovery result structure
 * @return MULTIREAD_OK on success
 */
multiread_error_t multiread_execute(multiread_ctx_t *ctx,
                                     uint8_t *output,
                                     size_t output_len,
                                     multiread_sector_t *result);

/**
 * @brief Read track with automatic multi-pass voting
 * 
 * Uses configured read callback for actual reads.
 * 
 * @param ctx Pipeline context
 * @param track Track number
 * @param head Head number
 * @param result Track recovery result
 * @return MULTIREAD_OK on success
 */
multiread_error_t multiread_track(multiread_ctx_t *ctx,
                                   uint8_t track,
                                   uint8_t head,
                                   multiread_track_t *result);

/*============================================================================
 * Convenience Functions
 *============================================================================*/

/**
 * @brief Simple majority vote on multiple buffers
 * 
 * Stateless function for quick voting without context.
 * 
 * @param buffers Array of data buffers
 * @param lengths Array of buffer lengths
 * @param buffer_count Number of buffers
 * @param output Output buffer (caller allocates)
 * @param output_len Output buffer size
 * @param confidence Average confidence (0-100)
 * @return MULTIREAD_OK on success
 */
multiread_error_t multiread_vote_buffers(const uint8_t **buffers,
                                          const size_t *lengths,
                                          uint8_t buffer_count,
                                          uint8_t *output,
                                          size_t output_len,
                                          uint8_t *confidence);

/*============================================================================
 * Statistics & Reporting
 *============================================================================*/

/**
 * @brief Get recovery statistics
 * @param ctx Pipeline context
 * @param total_reads Total read attempts
 * @param successful_reads Successful reads (CRC OK)
 * @param bytes_recovered Total bytes recovered
 * @param avg_confidence Average confidence percentage
 */
void multiread_get_stats(const multiread_ctx_t *ctx,
                          uint32_t *total_reads,
                          uint32_t *successful_reads,
                          uint32_t *bytes_recovered,
                          double *avg_confidence);

/**
 * @brief Generate recovery report
 * @param ctx Pipeline context
 * @param tracks Track results array (optional)
 * @param track_count Number of tracks
 * @return Allocated report string (caller must free)
 */
char *multiread_generate_report(const multiread_ctx_t *ctx,
                                 const multiread_track_t *tracks,
                                 uint8_t track_count);

/*============================================================================
 * Error Handling
 *============================================================================*/

/**
 * @brief Get error string
 * @param err Error code
 * @return Human-readable error string
 */
const char *multiread_error_string(multiread_error_t err);

/*============================================================================
 * Unit Tests
 *============================================================================*/

#ifdef UFT_UNIT_TESTS
void uft_multiread_pipeline_tests(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_MULTIREAD_PIPELINE_H */
