/**
 * @file uft_writer_verify.h
 * @brief UFT Writer Verification System
 * @version 3.2.0.003
 * @date 2026-01-04
 *
 * S-008: Writer Verification
 *
 * Provides comprehensive verification after writing to physical media,
 * ensuring data integrity through multi-pass reads, timing analysis,
 * and bit-accurate comparison.
 *
 * Key Features:
 * - Bit-accurate comparison between source and written data
 * - Timing verification for flux-level writes
 * - Multi-pass verify with statistical aggregation
 * - Automatic retry on verification failure
 * - Detailed error reporting by track/sector
 *
 * "Garantie dass geschriebene Daten korrekt sind"
 */

#ifndef UFT_WRITER_VERIFY_H
#define UFT_WRITER_VERIFY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Configuration Constants
 *============================================================================*/

/** Maximum verification passes */
#define UFT_VERIFY_MAX_PASSES           16

/** Maximum tracks for verification */
#define UFT_VERIFY_MAX_TRACKS          168

/** Maximum sectors per track */
#define UFT_VERIFY_MAX_SECTORS          64

/** Maximum retry attempts */
#define UFT_VERIFY_MAX_RETRIES          5

/** Default timing tolerance (percent) */
#define UFT_VERIFY_TIMING_TOLERANCE    5.0f

/** Minimum acceptable confidence for pass */
#define UFT_VERIFY_MIN_CONFIDENCE      95.0f

/*============================================================================
 * Enumerations
 *============================================================================*/

/**
 * @brief Verification mode
 */
typedef enum {
    UFT_VMODE_SECTOR    = 0x01,     /**< Sector-level CRC comparison */
    UFT_VMODE_BITSTREAM = 0x02,     /**< Bitstream-level comparison */
    UFT_VMODE_FLUX      = 0x03,     /**< Flux-level timing analysis */
    UFT_VMODE_FULL      = 0x0F      /**< All verification levels */
} uft_verify_mode_t;

/**
 * @brief Verification result status
 */
typedef enum {
    UFT_VRESULT_OK           = 0x00,    /**< Verification passed */
    UFT_VRESULT_MISMATCH     = 0x01,    /**< Data mismatch detected */
    UFT_VRESULT_TIMING_WARN  = 0x02,    /**< Timing out of spec (data OK) */
    UFT_VRESULT_TIMING_FAIL  = 0x03,    /**< Timing severely out of spec */
    UFT_VRESULT_READ_ERROR   = 0x04,    /**< Could not read back */
    UFT_VRESULT_CRC_FAIL     = 0x05,    /**< CRC mismatch */
    UFT_VRESULT_WEAK_BITS    = 0x06,    /**< Weak bits detected */
    UFT_VRESULT_PARTIAL      = 0x07,    /**< Partial verification only */
    UFT_VRESULT_RETRY_OK     = 0x10,    /**< OK after retry */
    UFT_VRESULT_RETRY_FAIL   = 0x11     /**< Failed after retries */
} uft_verify_result_t;

/**
 * @brief Error location type
 */
typedef enum {
    UFT_ELOC_NONE           = 0x00,     /**< No specific location */
    UFT_ELOC_TRACK          = 0x01,     /**< Track-level error */
    UFT_ELOC_SECTOR         = 0x02,     /**< Sector-level error */
    UFT_ELOC_GAP            = 0x03,     /**< Gap area error */
    UFT_ELOC_SYNC           = 0x04,     /**< Sync pattern error */
    UFT_ELOC_HEADER         = 0x05,     /**< Sector header error */
    UFT_ELOC_DATA           = 0x06,     /**< Sector data error */
    UFT_ELOC_CRC            = 0x07,     /**< CRC field error */
    UFT_ELOC_TIMING         = 0x08      /**< Timing region error */
} uft_error_location_type_t;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief Single error location
 */
typedef struct {
    uft_error_location_type_t type;     /**< Error location type */
    uint8_t  track;                     /**< Track number */
    uint8_t  head;                      /**< Head/side */
    uint8_t  sector;                    /**< Sector number (if applicable) */
    uint32_t bit_offset;                /**< Bit offset within unit */
    uint32_t bit_count;                 /**< Number of bits affected */
    uint32_t flux_sample;               /**< Flux sample index */
    uint8_t  expected;                  /**< Expected value */
    uint8_t  actual;                    /**< Actual value read */
    char     description[64];           /**< Human-readable description */
} uft_error_location_t;

/**
 * @brief Timing deviation record
 */
typedef struct {
    uint8_t  track;                     /**< Track number */
    uint8_t  head;                      /**< Head/side */
    uint32_t flux_sample;               /**< Flux sample index */
    float    expected_us;               /**< Expected timing (microseconds) */
    float    actual_us;                 /**< Actual timing */
    float    deviation_percent;         /**< Deviation percentage */
    bool     in_tolerance;              /**< Within tolerance? */
} uft_timing_deviation_t;

/**
 * @brief Sector verification result
 */
typedef struct {
    uint8_t  track;                     /**< Track number */
    uint8_t  head;                      /**< Head/side */
    uint8_t  sector;                    /**< Sector number */
    
    uft_verify_result_t result;         /**< Overall result */
    
    /* CRC comparison */
    bool     crc_match;                 /**< CRC matches */
    uint32_t expected_crc;              /**< Expected CRC */
    uint32_t actual_crc;                /**< Read-back CRC */
    
    /* Bit comparison */
    uint32_t total_bits;                /**< Total bits compared */
    uint32_t matching_bits;             /**< Matching bits */
    uint32_t differing_bits;            /**< Differing bits */
    float    match_percent;             /**< Match percentage */
    
    /* Timing analysis */
    float    timing_deviation_avg;      /**< Average timing deviation % */
    float    timing_deviation_max;      /**< Maximum timing deviation % */
    bool     timing_in_spec;            /**< Within timing spec */
    
    /* Error details */
    uft_error_location_t *errors;       /**< Array of errors */
    size_t   error_count;               /**< Number of errors */
    
    /* Retry info */
    uint8_t  retry_count;               /**< Number of retries needed */
    bool     retry_successful;          /**< Retry fixed the issue */
    
} uft_sector_verify_t;

/**
 * @brief Track verification result
 */
typedef struct {
    uint8_t  track;                     /**< Track number */
    uint8_t  head;                      /**< Head/side */
    
    uft_verify_result_t result;         /**< Overall result */
    
    /* Sector results */
    uft_sector_verify_t *sectors;       /**< Array of sector results */
    size_t   sector_count;              /**< Number of sectors */
    size_t   sectors_ok;                /**< Sectors passed */
    size_t   sectors_failed;            /**< Sectors failed */
    size_t   sectors_retried;           /**< Sectors needed retry */
    
    /* Bitstream comparison (if applicable) */
    uint32_t total_bits;                /**< Total track bits */
    uint32_t matching_bits;             /**< Matching bits */
    float    match_percent;             /**< Match percentage */
    
    /* Timing analysis */
    float    avg_deviation;             /**< Average timing deviation */
    float    max_deviation;             /**< Maximum timing deviation */
    uft_timing_deviation_t *timing_issues; /**< Array of timing issues */
    size_t   timing_issue_count;        /**< Number of timing issues */
    
    /* Flux-level statistics */
    uint32_t flux_transitions;          /**< Total flux transitions */
    uint32_t flux_errors;               /**< Flux timing errors */
    float    flux_quality;              /**< Flux quality score 0-100 */
    
} uft_track_verify_t;

/**
 * @brief Multi-pass verification statistics
 */
typedef struct {
    uint8_t  pass_count;                /**< Number of passes */
    
    /* Per-pass results */
    struct {
        uft_verify_result_t result;     /**< Pass result */
        float    match_percent;         /**< Match percentage */
        uint32_t errors;                /**< Error count */
        float    timing_deviation;      /**< Average timing deviation */
    } passes[UFT_VERIFY_MAX_PASSES];
    
    /* Aggregate statistics */
    float    avg_match_percent;         /**< Average match across passes */
    float    min_match_percent;         /**< Minimum match */
    float    max_match_percent;         /**< Maximum match */
    float    consistency;               /**< Read consistency 0-100 */
    
    /* Weak bit detection */
    uint32_t weak_bit_positions;        /**< Bits that vary between reads */
    bool     has_weak_bits;             /**< Any weak bits detected */
    
} uft_multipass_stats_t;

/**
 * @brief Complete verification session
 */
typedef struct {
    char     session_id[40];            /**< Unique session ID */
    time_t   start_time;                /**< Verification start */
    time_t   end_time;                  /**< Verification end */
    
    /* Configuration */
    uft_verify_mode_t mode;             /**< Verification mode */
    uint8_t  pass_count;                /**< Requested passes */
    uint8_t  max_retries;               /**< Max retries per sector */
    float    timing_tolerance;          /**< Timing tolerance % */
    
    /* Results */
    uft_verify_result_t overall_result; /**< Overall result */
    
    /* Track results */
    uft_track_verify_t *tracks;         /**< Array of track results */
    size_t   track_count;               /**< Number of tracks */
    
    /* Multi-pass statistics */
    uft_multipass_stats_t multipass;    /**< Multi-pass statistics */
    
    /* Summary statistics */
    size_t   total_sectors;             /**< Total sectors verified */
    size_t   sectors_passed;            /**< Sectors passed */
    size_t   sectors_failed;            /**< Sectors failed */
    size_t   sectors_retried;           /**< Sectors needed retry */
    float    overall_match;             /**< Overall match percentage */
    float    overall_timing;            /**< Overall timing score */
    
    /* Error summary */
    uft_error_location_t *all_errors;   /**< All errors across disk */
    size_t   total_errors;              /**< Total error count */
    
} uft_verify_session_t;

/**
 * @brief Verification configuration
 */
typedef struct {
    uft_verify_mode_t mode;             /**< Verification mode */
    uint8_t  pass_count;                /**< Number of verify passes (1-16) */
    uint8_t  max_retries;               /**< Max retries per sector (0-5) */
    float    timing_tolerance;          /**< Timing tolerance % (0-20) */
    float    min_match_percent;         /**< Minimum acceptable match % */
    bool     abort_on_fail;             /**< Abort on first failure */
    bool     verify_gaps;               /**< Also verify gap areas */
    bool     verify_sync;               /**< Verify sync patterns */
    bool     collect_timing;            /**< Collect timing statistics */
    bool     enable_retry;              /**< Enable auto-retry */
    bool     log_progress;              /**< Log progress to console */
} uft_verify_config_t;

/**
 * @brief Progress callback for verification
 */
typedef void (*uft_verify_progress_cb)(
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uft_verify_result_t result,
    float percent_complete,
    void *user_data
);

/*============================================================================
 * API Functions - Session Management
 *============================================================================*/

/**
 * @brief Create verification session
 * @param config Configuration (NULL for defaults)
 * @return New session or NULL
 */
uft_verify_session_t* uft_wv_session_create(
    const uft_verify_config_t *config
);

/**
 * @brief Destroy verification session
 * @param session Session to destroy
 */
void uft_wv_session_destroy(uft_verify_session_t *session);

/**
 * @brief Reset session for reuse
 * @param session Session to reset
 * @return UFT_OK on success
 */
int uft_wv_session_reset(uft_verify_session_t *session);

/*============================================================================
 * API Functions - Verification
 *============================================================================*/

/**
 * @brief Verify sector data
 * @param session Session
 * @param track Track number
 * @param head Head/side
 * @param sector Sector number
 * @param expected Expected data
 * @param expected_size Expected data size
 * @param actual Actual read-back data
 * @param actual_size Actual data size
 * @return Verification result
 */
uft_verify_result_t uft_wv_verify_sector(
    uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    const uint8_t *expected,
    size_t expected_size,
    const uint8_t *actual,
    size_t actual_size
);

/**
 * @brief Verify sector with CRC check
 * @param session Session
 * @param track Track number
 * @param head Head/side
 * @param sector Sector number
 * @param expected_crc Expected CRC
 * @param actual_crc Actual CRC from read-back
 * @return Verification result
 */
uft_verify_result_t uft_wv_verify_sector_crc(
    uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uint32_t expected_crc,
    uint32_t actual_crc
);

/**
 * @brief Verify track bitstream
 * @param session Session
 * @param track Track number
 * @param head Head/side
 * @param expected Expected bitstream
 * @param expected_bits Number of expected bits
 * @param actual Actual read-back bitstream
 * @param actual_bits Actual bit count
 * @return Verification result
 */
uft_verify_result_t uft_wv_verify_track_bitstream(
    uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    const uint8_t *expected,
    size_t expected_bits,
    const uint8_t *actual,
    size_t actual_bits
);

/**
 * @brief Verify flux timing
 * @param session Session
 * @param track Track number
 * @param head Head/side
 * @param expected_flux Expected flux intervals (in sample units)
 * @param expected_count Number of expected intervals
 * @param actual_flux Actual flux intervals
 * @param actual_count Actual count
 * @param sample_rate Sample rate in Hz
 * @return Verification result
 */
uft_verify_result_t uft_wv_verify_flux_timing(
    uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    const uint32_t *expected_flux,
    size_t expected_count,
    const uint32_t *actual_flux,
    size_t actual_count,
    uint32_t sample_rate
);

/**
 * @brief Perform multi-pass verification
 * @param session Session
 * @param track Track number
 * @param head Head/side
 * @param expected Expected data
 * @param size Data size
 * @param read_cb Callback to read data for each pass
 * @param user_data User data for callback
 * @return Overall verification result
 */
typedef int (*uft_wv_read_cb)(
    uint8_t track,
    uint8_t head,
    uint8_t *buffer,
    size_t size,
    void *user_data
);

uft_verify_result_t uft_wv_multipass_verify(
    uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    const uint8_t *expected,
    size_t size,
    uint8_t passes,
    uft_wv_read_cb read_cb,
    void *user_data
);

/*============================================================================
 * API Functions - Retry
 *============================================================================*/

/**
 * @brief Retry failed sector write
 * @param session Session
 * @param track Track number
 * @param head Head/side
 * @param sector Sector number
 * @param data Data to write
 * @param size Data size
 * @param write_cb Callback to perform write
 * @param read_cb Callback to perform read
 * @param user_data User data for callbacks
 * @return Verification result after retry
 */
typedef int (*uft_wv_write_cb)(
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    const uint8_t *data,
    size_t size,
    void *user_data
);

typedef int (*uft_wv_read_sector_cb)(
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uint8_t *buffer,
    size_t size,
    void *user_data
);

uft_verify_result_t uft_wv_retry_sector(
    uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    const uint8_t *data,
    size_t size,
    uft_wv_write_cb write_cb,
    uft_wv_read_sector_cb read_cb,
    void *user_data
);

/**
 * @brief Get retry statistics
 * @param session Session
 * @param total_retries Output: total retries attempted
 * @param successful_retries Output: successful retries
 * @return UFT_OK on success
 */
int uft_wv_get_retry_stats(
    const uft_verify_session_t *session,
    uint32_t *total_retries,
    uint32_t *successful_retries
);

/*============================================================================
 * API Functions - Analysis
 *============================================================================*/

/**
 * @brief Get sector verification result
 * @param session Session
 * @param track Track number
 * @param head Head/side
 * @param sector Sector number
 * @return Sector result or NULL
 */
const uft_sector_verify_t* uft_wv_get_sector_result(
    const uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    uint8_t sector
);

/**
 * @brief Get track verification result
 * @param session Session
 * @param track Track number
 * @param head Head/side
 * @return Track result or NULL
 */
const uft_track_verify_t* uft_wv_get_track_result(
    const uft_verify_session_t *session,
    uint8_t track,
    uint8_t head
);

/**
 * @brief Get all failed sectors
 * @param session Session
 * @param sectors Output array of sector results
 * @param max_sectors Maximum entries
 * @return Number of failed sectors
 */
size_t uft_wv_get_failed_sectors(
    const uft_verify_session_t *session,
    const uft_sector_verify_t **sectors,
    size_t max_sectors
);

/**
 * @brief Get all errors
 * @param session Session
 * @param errors Output array of errors
 * @param max_errors Maximum entries
 * @return Number of errors
 */
size_t uft_wv_get_all_errors(
    const uft_verify_session_t *session,
    const uft_error_location_t **errors,
    size_t max_errors
);

/**
 * @brief Calculate overall verification score
 * @param session Session
 * @return Score 0-100
 */
float uft_wv_calculate_score(const uft_verify_session_t *session);

/*============================================================================
 * API Functions - Export
 *============================================================================*/

/**
 * @brief Export session to JSON
 * @param session Session
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or required size
 */
size_t uft_wv_export_json(
    const uft_verify_session_t *session,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Export session to Markdown report
 * @param session Session
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or required size
 */
size_t uft_wv_export_markdown(
    const uft_verify_session_t *session,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Export detailed error report
 * @param session Session
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t uft_wv_export_error_report(
    const uft_verify_session_t *session,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Print summary to console
 * @param session Session
 */
void uft_wv_print_summary(const uft_verify_session_t *session);

/*============================================================================
 * API Functions - Utilities
 *============================================================================*/

/**
 * @brief Get result name
 * @param result Result code
 * @return Static string
 */
const char* uft_wv_result_name(uft_verify_result_t result);

/**
 * @brief Get error location type name
 * @param type Error location type
 * @return Static string
 */
const char* uft_wv_error_type_name(uft_error_location_type_t type);

/**
 * @brief Default configuration
 * @param config Config to fill
 */
void uft_wv_config_defaults(uft_verify_config_t *config);

/**
 * @brief Calculate CRC32 for verification
 * @param data Data buffer
 * @param size Data size
 * @return CRC32 value
 */
uint32_t uft_wv_crc32(const uint8_t *data, size_t size);

/**
 * @brief Compare two byte arrays and return difference positions
 * @param a First array
 * @param b Second array
 * @param size Size to compare
 * @param diff_positions Output array for difference positions
 * @param max_diffs Maximum differences to record
 * @return Number of differing bytes
 */
size_t uft_wv_compare_bytes(
    const uint8_t *a,
    const uint8_t *b,
    size_t size,
    uint32_t *diff_positions,
    size_t max_diffs
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WRITER_VERIFY_H */
