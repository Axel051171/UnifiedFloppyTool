/**
 * @file uft_fluxstat.h
 * @version 1.0.0
 * @date 2026-01-06
 *
 *
 * This module provides statistical flux analysis for recovering marginal sectors:
 * - Multi-pass capture support
 * - Histogram-based bitrate detection
 * - Bit-level confidence scoring
 * - CRC-guided error correction
 * - Weak bit preservation
 *
 */

#ifndef UFT_FLUXSTAT_H
#define UFT_FLUXSTAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Maximum capture passes */
#define UFT_FLUXSTAT_MAX_PASSES     64

/** Minimum passes for statistics */
#define UFT_FLUXSTAT_MIN_PASSES     2

/** Default pass count */
#define UFT_FLUXSTAT_DEFAULT_PASSES 8

/** Histogram bin count */
#define UFT_FLUXSTAT_HIST_BINS      256

/** Maximum sector size (bytes) */
#define UFT_FLUXSTAT_MAX_SECTOR     4096

/** Maximum sectors per track */
#define UFT_FLUXSTAT_MAX_SECTORS    32

/** Maximum weak bit positions to track */
#define UFT_FLUXSTAT_MAX_WEAK_POS   64

/*============================================================================
 * Bit Cell Classifications
 *============================================================================*/

typedef enum {
    UFT_BITCELL_STRONG_1    = 0,    /**< High confidence "1" */
    UFT_BITCELL_WEAK_1      = 1,    /**< Low confidence "1" */
    UFT_BITCELL_STRONG_0    = 2,    /**< High confidence "0" */
    UFT_BITCELL_WEAK_0      = 3,    /**< Low confidence "0" */
    UFT_BITCELL_AMBIGUOUS   = 4     /**< Cannot determine */
} uft_bitcell_class_t;

/** Confidence thresholds (percentage) */
#define UFT_CONF_STRONG         90  /**< >= 90% = strong */
#define UFT_CONF_WEAK           60  /**< 60-89% = weak */
#define UFT_CONF_AMBIGUOUS      60  /**< < 60% = ambiguous */

/*============================================================================
 * Encoding Types
 *============================================================================*/

typedef enum {
    UFT_FLUXSTAT_ENC_MFM    = 0,    /**< MFM encoding */
    UFT_FLUXSTAT_ENC_FM     = 1,    /**< FM encoding */
    UFT_FLUXSTAT_ENC_GCR    = 2,    /**< GCR encoding */
    UFT_FLUXSTAT_ENC_AMIGA  = 3,    /**< Amiga MFM */
    UFT_FLUXSTAT_ENC_APPLE  = 4,    /**< Apple GCR */
    UFT_FLUXSTAT_ENC_C64    = 5     /**< Commodore GCR */
} uft_fluxstat_encoding_t;

/*============================================================================
 * Error Codes
 *============================================================================*/

#define UFT_FLUXSTAT_OK             0
#define UFT_FLUXSTAT_ERR_NULLPTR    (-1)
#define UFT_FLUXSTAT_ERR_INVALID    (-2)
#define UFT_FLUXSTAT_ERR_BUSY       (-3)
#define UFT_FLUXSTAT_ERR_TIMEOUT    (-4)
#define UFT_FLUXSTAT_ERR_OVERFLOW   (-5)
#define UFT_FLUXSTAT_ERR_NO_DATA    (-6)
#define UFT_FLUXSTAT_ERR_MEMORY     (-7)

/*============================================================================
 * Configuration Structure
 *============================================================================*/

/**
 * @brief FluxStat configuration
 */
typedef struct {
    uint8_t  pass_count;            /**< Number of passes (2-64) */
    uint8_t  confidence_threshold;  /**< Min confidence for "good" bit (0-100) */
    uint8_t  max_correction_bits;   /**< Max bits to try correcting per sector */
    uft_fluxstat_encoding_t encoding; /**< Expected encoding */
    uint32_t data_rate;             /**< Expected data rate in bps */
    bool     use_crc_correction;    /**< Enable CRC-guided correction */
    bool     preserve_weak_bits;    /**< Preserve weak bit info in output */
} uft_fluxstat_config_t;

/*============================================================================
 * Per-Pass Capture Data
 *============================================================================*/

/**
 * @brief Metadata for a single capture pass
 */
typedef struct {
    uint32_t flux_count;            /**< Number of flux transitions */
    uint32_t index_time_ns;         /**< Index-to-index time (nanoseconds) */
    uint32_t start_time_ns;         /**< Capture start timestamp */
    uint32_t data_size;             /**< Bytes of flux data */
    uint32_t* flux_data;            /**< Pointer to flux timing data */
} uft_fluxstat_pass_t;

/*============================================================================
 * Multi-Pass Capture Result
 *============================================================================*/

/**
 * @brief Result of multi-pass capture
 */
typedef struct {
    uint8_t  pass_count;            /**< Number of passes captured */
    uint32_t total_flux;            /**< Sum of all flux counts */
    uint32_t min_flux;              /**< Minimum flux count (any pass) */
    uint32_t max_flux;              /**< Maximum flux count (any pass) */
    uint32_t avg_rpm;               /**< Average RPM across passes */
    uft_fluxstat_pass_t passes[UFT_FLUXSTAT_MAX_PASSES];
} uft_fluxstat_capture_t;

/*============================================================================
 * Histogram Statistics
 *============================================================================*/

/**
 * @brief Histogram analysis results
 */
typedef struct {
    uint32_t total_count;           /**< Total flux transitions */
    uint16_t interval_min;          /**< Minimum interval seen */
    uint16_t interval_max;          /**< Maximum interval seen */
    uint8_t  peak_bin;              /**< Bin with highest count */
    uint32_t peak_count;            /**< Count in peak bin */
    uint16_t mean_interval;         /**< Mean flux interval */
    uint32_t overflow_count;        /**< Intervals above max bin */
    uint32_t bins[UFT_FLUXSTAT_HIST_BINS]; /**< Bin counts */
} uft_fluxstat_histogram_t;

/*============================================================================
 * Per-Bit Analysis
 *============================================================================*/

/**
 * @brief Analysis result for a single bit
 */
typedef struct {
    uint8_t  value;                 /**< Most likely bit value (0 or 1) */
    uint8_t  confidence;            /**< Confidence 0-100% */
    uft_bitcell_class_t classification; /**< Bit classification */
    bool     corrected;             /**< Was CRC-corrected */
    uint16_t transition_count;      /**< Passes with transition */
    uint16_t timing_stddev;         /**< Timing standard deviation */
} uft_fluxstat_bit_t;

/*============================================================================
 * Sector Recovery Result
 *============================================================================*/

/**
 * @brief Recovery result for a single sector
 */
typedef struct {
    uint8_t  data[UFT_FLUXSTAT_MAX_SECTOR]; /**< Recovered sector data */
    uint16_t size;                  /**< Sector size in bytes */
    bool     crc_ok;                /**< CRC verified */
    bool     recovered;             /**< Recovery successful */
    uint8_t  confidence_min;        /**< Minimum bit confidence */
    uint8_t  confidence_avg;        /**< Average bit confidence */
    uint8_t  weak_bit_count;        /**< Number of weak bits */
    uint8_t  corrected_count;       /**< Bits corrected by CRC guidance */
    uint16_t weak_positions[UFT_FLUXSTAT_MAX_WEAK_POS]; /**< Weak bit positions */
    uint8_t  sector_num;            /**< Logical sector number */
    uint8_t  track;                 /**< Track number */
    uint8_t  head;                  /**< Head number */
} uft_fluxstat_sector_t;

/*============================================================================
 * Track Recovery Result
 *============================================================================*/

/**
 * @brief Recovery result for a complete track
 */
typedef struct {
    uint8_t  track;                 /**< Track number */
    uint8_t  head;                  /**< Head number */
    uint8_t  sector_count;          /**< Total sectors on track */
    uint8_t  sectors_recovered;     /**< Fully recovered */
    uint8_t  sectors_partial;       /**< Partially recovered (weak bits) */
    uint8_t  sectors_failed;        /**< Completely failed */
    uint8_t  overall_confidence;    /**< Overall track confidence % */
    uft_fluxstat_sector_t sectors[UFT_FLUXSTAT_MAX_SECTORS];
} uft_fluxstat_track_t;

/*============================================================================
 * Context Handle
 *============================================================================*/

typedef struct uft_fluxstat_ctx uft_fluxstat_ctx_t;

/*============================================================================
 * Lifecycle Functions
 *============================================================================*/

/**
 * @brief Create FluxStat context
 * @return Context handle or NULL on error
 */
uft_fluxstat_ctx_t* uft_fluxstat_create(void);

/**
 * @brief Destroy FluxStat context
 * @param ctx Context to destroy
 */
void uft_fluxstat_destroy(uft_fluxstat_ctx_t* ctx);

/**
 * @brief Configure FluxStat parameters
 * @param ctx Context
 * @param config Configuration
 * @return UFT_FLUXSTAT_OK on success
 */
int uft_fluxstat_configure(uft_fluxstat_ctx_t* ctx, 
                           const uft_fluxstat_config_t* config);

/**
 * @brief Get current configuration
 * @param ctx Context
 * @param config Output configuration
 * @return UFT_FLUXSTAT_OK on success
 */
int uft_fluxstat_get_config(uft_fluxstat_ctx_t* ctx,
                            uft_fluxstat_config_t* config);

/*============================================================================
 * Multi-Pass Analysis
 *============================================================================*/

/**
 * @brief Add a capture pass to the context
 * @param ctx Context
 * @param flux_data Flux timing data (copied internally)
 * @param flux_count Number of flux transitions
 * @param index_time_ns Index-to-index time in nanoseconds
 * @return Pass index (0-based) or negative error code
 */
int uft_fluxstat_add_pass(uft_fluxstat_ctx_t* ctx,
                          const uint32_t* flux_data,
                          size_t flux_count,
                          uint32_t index_time_ns);

/**
 * @brief Clear all passes
 * @param ctx Context
 */
void uft_fluxstat_clear_passes(uft_fluxstat_ctx_t* ctx);

/**
 * @brief Get number of passes
 * @param ctx Context
 * @return Number of passes added
 */
int uft_fluxstat_pass_count(uft_fluxstat_ctx_t* ctx);

/**
 * @brief Get capture summary
 * @param ctx Context
 * @param capture Output capture data
 * @return UFT_FLUXSTAT_OK on success
 */
int uft_fluxstat_get_capture(uft_fluxstat_ctx_t* ctx,
                              uft_fluxstat_capture_t* capture);

/*============================================================================
 * Histogram Analysis
 *============================================================================*/

/**
 * @brief Compute histogram from all passes
 * @param ctx Context
 * @param hist Output histogram
 * @return UFT_FLUXSTAT_OK on success
 */
int uft_fluxstat_compute_histogram(uft_fluxstat_ctx_t* ctx,
                                    uft_fluxstat_histogram_t* hist);

/**
 * @brief Estimate data rate from histogram
 * @param ctx Context
 * @param rate_bps Output data rate in bps
 * @return UFT_FLUXSTAT_OK on success
 */
int uft_fluxstat_estimate_rate(uft_fluxstat_ctx_t* ctx,
                                uint32_t* rate_bps);

/**
 * @brief Detect encoding from histogram
 * @param ctx Context
 * @param encoding Output detected encoding
 * @return UFT_FLUXSTAT_OK on success
 */
int uft_fluxstat_detect_encoding(uft_fluxstat_ctx_t* ctx,
                                  uft_fluxstat_encoding_t* encoding);

/*============================================================================
 * Correlation Analysis
 *============================================================================*/

/**
 * @brief Correlate flux transitions across passes
 * @param ctx Context
 * @return UFT_FLUXSTAT_OK on success
 * 
 * This aligns all passes to find corresponding transitions and
 * calculates statistics for each bit position.
 */
int uft_fluxstat_correlate(uft_fluxstat_ctx_t* ctx);

/**
 * @brief Get bit-level analysis for range
 * @param ctx Context (must be correlated first)
 * @param bit_offset Starting bit position
 * @param count Number of bits
 * @param bits Output array (caller allocated)
 * @return UFT_FLUXSTAT_OK on success
 */
int uft_fluxstat_get_bits(uft_fluxstat_ctx_t* ctx,
                          uint32_t bit_offset,
                          uint32_t count,
                          uft_fluxstat_bit_t* bits);

/*============================================================================
 * Track/Sector Recovery
 *============================================================================*/

/**
 * @brief Analyze and recover track
 * @param ctx Context (must be correlated first)
 * @param track Track result output
 * @return UFT_FLUXSTAT_OK on success
 */
int uft_fluxstat_recover_track(uft_fluxstat_ctx_t* ctx,
                                uft_fluxstat_track_t* track);

/**
 * @brief Recover specific sector
 * @param ctx Context
 * @param sector_num Sector number
 * @param sector Sector result output
 * @return UFT_FLUXSTAT_OK on success
 */
int uft_fluxstat_recover_sector(uft_fluxstat_ctx_t* ctx,
                                 uint8_t sector_num,
                                 uft_fluxstat_sector_t* sector);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Calculate confidence score for data buffer
 * @param ctx Context
 * @param data Data buffer
 * @param length Length in bytes
 * @param min_conf Minimum bit confidence (output)
 * @param avg_conf Average bit confidence (output)
 * @return UFT_FLUXSTAT_OK on success
 */
int uft_fluxstat_calculate_confidence(uft_fluxstat_ctx_t* ctx,
                                       const uint8_t* data,
                                       size_t length,
                                       uint8_t* min_conf,
                                       uint8_t* avg_conf);

/**
 * @brief Get classification name
 * @param classification Bit classification
 * @return String name
 */
const char* uft_fluxstat_class_name(uft_bitcell_class_t classification);

/**
 * @brief Calculate RPM from index period
 * @param index_time_ns Index period in nanoseconds
 * @return RPM value
 */
uint32_t uft_fluxstat_calculate_rpm(uint32_t index_time_ns);

/**
 * @brief Create default configuration
 * @return Default configuration
 */
uft_fluxstat_config_t uft_fluxstat_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUXSTAT_H */
