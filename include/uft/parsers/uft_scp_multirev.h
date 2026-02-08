/**
 * @file uft_scp_multirev.h
 * @brief SCP v3 Multi-Revolution Parser with Confidence Fusion
 * 
 * FEATURES:
 * ✅ Full SCP format support (v1.0 - v2.4)
 * ✅ Multi-Revolution reading (up to 5 revolutions)
 * ✅ Confidence-based fusion algorithm
 * ✅ Weak bit detection
 * ✅ Thread-safe design
 * 
 * @version 3.3.7
 * @date 2026-01-03
 */

#ifndef UFT_SCP_MULTIREV_H
#define UFT_SCP_MULTIREV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ========================================================================== */

#define UFT_SCP_MAX_REVOLUTIONS  5
#define UFT_SCP_MAX_TRACKS       168

/* SCP Flags */
#define UFT_SCP_FLAG_INDEXED     (1 << 0)  /**< Index signal present */
#define UFT_SCP_FLAG_96TPI       (1 << 1)  /**< 96 TPI drive */
#define UFT_SCP_FLAG_360RPM      (1 << 2)  /**< 360 RPM (else 300) */
#define UFT_SCP_FLAG_NORMALIZED  (1 << 3)  /**< Flux normalized */
#define UFT_SCP_FLAG_READWRITE   (1 << 4)  /**< Read/write capable */
#define UFT_SCP_FLAG_FOOTER      (1 << 5)  /**< Has footer */

/* ============================================================================
 * ERROR CODES
 * ========================================================================== */

typedef enum {
    UFT_SCP_OK = 0,
    UFT_SCP_ERR_NULL_ARG,
    UFT_SCP_ERR_FILE_OPEN,
    UFT_SCP_ERR_FILE_READ,
    UFT_SCP_ERR_BAD_SIGNATURE,
    UFT_SCP_ERR_BAD_TRACK,
    UFT_SCP_ERR_NO_DATA,
    UFT_SCP_ERR_MEMORY,
    UFT_SCP_ERR_OVERFLOW,
    UFT_SCP_ERR_INVALID_REV
} uft_scp_error_t;

/* ============================================================================
 * STRUCTURES
 * ========================================================================== */

/**
 * Single revolution data
 */
typedef struct {
    uint32_t* flux_ns;          /**< Flux intervals in nanoseconds */
    uint32_t  count;            /**< Number of intervals */
    uint32_t  duration_ns;      /**< Total revolution duration */
    uint32_t  index_time_ns;    /**< Index-to-index time */
} uft_scp_rev_data_t;

/**
 * Multi-revolution track data
 */
typedef struct {
    uft_scp_rev_data_t revs[UFT_SCP_MAX_REVOLUTIONS];
    uint8_t  num_revolutions;   /**< Number of valid revolutions */
    uint8_t  track_number;      /**< Physical track (0-83) */
    uint8_t  head;              /**< Head (0-1) */
    
    /* Statistics */
    uint32_t total_flux;        /**< Sum of flux across all revs */
    double   avg_rpm;           /**< Average RPM */
    double   rpm_variance;      /**< RPM variance */
} uft_scp_track_data_t;

/**
 * Fused flux result with confidence
 */
typedef struct {
    uint32_t* flux_ns;          /**< Best-estimate flux intervals */
    uint32_t  count;            /**< Number of intervals */
    float*    confidence;       /**< Per-interval confidence (0.0-1.0) */
    uint8_t*  weak_bits;        /**< Bitmap: 1 = weak/uncertain bit */
    uint32_t  weak_count;       /**< Number of weak bits detected */
    
    /* Quality metrics */
    float     overall_confidence; /**< Average confidence */
    float     consistency;      /**< Cross-revolution consistency */
} uft_scp_fused_track_t;

/**
 * SCP Reader Context (opaque)
 */
typedef struct uft_scp_reader uft_scp_reader_t;

/* ============================================================================
 * READER API
 * ========================================================================== */

/**
 * Create SCP reader
 * 
 * @return New reader, or NULL on failure
 */
uft_scp_reader_t* uft_scp_reader_create(void);

/**
 * Open SCP file
 * 
 * @param ctx Reader context
 * @param path Path to .scp file
 * @return UFT_SCP_OK or error code
 */
int uft_scp_reader_open(uft_scp_reader_t* ctx, const char* path);

/**
 * Close SCP file
 * 
 * @param ctx Reader context
 */
void uft_scp_reader_close(uft_scp_reader_t* ctx);

/**
 * Destroy SCP reader
 * 
 * @param ctx Pointer to reader (set to NULL)
 */
void uft_scp_reader_destroy(uft_scp_reader_t** ctx);

/**
 * Get reader info
 * 
 * @param ctx Reader context
 * @param version_major Output: major version
 * @param version_minor Output: minor version
 * @param num_revolutions Output: revolutions per track
 * @param start_track Output: first track
 * @param end_track Output: last track
 * @param num_heads Output: number of heads
 * @param resolution_ns Output: time resolution in ns
 * @param flags Output: SCP flags
 * @return UFT_SCP_OK or error code
 */
int uft_scp_reader_get_info(
    const uft_scp_reader_t* ctx,
    uint8_t* version_major,
    uint8_t* version_minor,
    uint8_t* num_revolutions,
    uint8_t* start_track,
    uint8_t* end_track,
    uint8_t* num_heads,
    uint32_t* resolution_ns,
    uint8_t* flags
);

/* ============================================================================
 * REVOLUTION READING
 * ========================================================================== */

/**
 * Read single revolution flux data
 * 
 * @param ctx SCP reader context
 * @param track Physical track (0-83)
 * @param head Head (0-1)
 * @param revolution Revolution number (0 - num_revolutions-1)
 * @param flux_ns Output: flux intervals (caller must free)
 * @param count Output: number of intervals
 * @param duration_ns Output: revolution duration (optional, can be NULL)
 * @return UFT_SCP_OK or error code
 */
int uft_scp_read_revolution(
    uft_scp_reader_t* ctx,
    uint8_t track,
    uint8_t head,
    uint8_t revolution,
    uint32_t** flux_ns,
    uint32_t* count,
    uint32_t* duration_ns
);

/**
 * Read all revolutions for a track
 * 
 * @param ctx SCP reader context
 * @param track Physical track (0-83)
 * @param head Head (0-1)
 * @param track_data Output: track data (use uft_scp_track_data_free to free)
 * @return UFT_SCP_OK or error code
 */
int uft_scp_read_all_revolutions(
    uft_scp_reader_t* ctx,
    uint8_t track,
    uint8_t head,
    uft_scp_track_data_t* track_data
);

/**
 * Free track data
 * 
 * @param track_data Track data to free
 */
void uft_scp_track_data_free(uft_scp_track_data_t* track_data);

/* ============================================================================
 * MULTI-REVOLUTION FUSION
 * ========================================================================== */

/**
 * Fuse multiple revolutions into single best-estimate
 * 
 * Algorithm:
 * - Use revolution 0 as reference timeline
 * - For each flux in reference, find matching flux in other revs
 * - If all revs agree → high confidence, average the timing
 * - If revs disagree → low confidence, mark as weak bit
 * 
 * @param track_data Input: multi-revolution data
 * @param fused Output: fused result (use uft_scp_fused_free to free)
 * @return UFT_SCP_OK or error code
 */
int uft_scp_fuse_revolutions(
    const uft_scp_track_data_t* track_data,
    uft_scp_fused_track_t* fused
);

/**
 * Free fused track data
 * 
 * @param fused Fused data to free
 */
void uft_scp_fused_free(uft_scp_fused_track_t* fused);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ========================================================================== */

/**
 * Get disk type name
 * 
 * @param disk_type SCP disk type byte
 * @return Human-readable name
 */
const char* uft_scp_disk_type_name(uint8_t disk_type);

/**
 * Get version string
 * 
 * @param version SCP version byte
 * @return Version string (e.g., "2.4")
 */
const char* uft_scp_version_str(uint8_t version);

/**
 * Print reader info to stdout
 * 
 * @param ctx Reader context
 */
void uft_scp_print_info(const uft_scp_reader_t* ctx);

/**
 * Check if track exists in file
 * 
 * @param ctx Reader context
 * @param track Physical track (0-83)
 * @param head Head (0-1)
 * @return true if track has data
 */
bool uft_scp_has_track(const uft_scp_reader_t* ctx, uint8_t track, uint8_t head);

/**
 * Get last error code
 * 
 * @param ctx Reader context
 * @return Last error code
 */
int uft_scp_get_error(const uft_scp_reader_t* ctx);

/**
 * Get last error message
 * 
 * @param ctx Reader context
 * @return Error message (static string)
 */
const char* uft_scp_get_error_msg(const uft_scp_reader_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SCP_MULTIREV_H */
