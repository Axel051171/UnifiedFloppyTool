/**
 * @file uft_multirev.h
 * @brief Multi-Revolution Voting Algorithm for UnifiedFloppyTool
 * 
 * Implements statistical analysis and voting across multiple revolutions
 * of the same track to:
 * - Detect weak/unstable bits
 * - Recover data from damaged media
 * - Identify copy protection signatures
 * - Produce high-confidence fused output
 * 
 * The algorithm uses a weighted voting system that considers:
 * - Bit stability across revolutions
 * - Timing consistency
 * - Known encoding patterns
 * - CRC validation results
 * 
 * @version 1.0.0
 * @date 2025
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_MULTIREV_H
#define UFT_MULTIREV_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_ir_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Maximum revolutions for voting */
#define UFT_MRV_MAX_REVOLUTIONS     16

/** Minimum revolutions for meaningful voting */
#define UFT_MRV_MIN_REVOLUTIONS     2

/** Default confidence threshold for stable bits */
#define UFT_MRV_CONFIDENCE_STABLE   90

/** Default confidence threshold for weak bit detection */
#define UFT_MRV_CONFIDENCE_WEAK     60

/** Maximum bit positions to track */
#define UFT_MRV_MAX_BITS            500000

/** Histogram buckets for timing analysis */
#define UFT_MRV_TIMING_BUCKETS      256

/* ═══════════════════════════════════════════════════════════════════════════
 * ENUMERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Bit stability classification
 */
typedef enum uft_mrv_bit_class {
    UFT_MRV_BIT_UNKNOWN     = 0,    /**< Not yet analyzed */
    UFT_MRV_BIT_STABLE_0    = 1,    /**< Consistently reads as 0 */
    UFT_MRV_BIT_STABLE_1    = 2,    /**< Consistently reads as 1 */
    UFT_MRV_BIT_WEAK        = 3,    /**< Reads inconsistently (weak bit) */
    UFT_MRV_BIT_MISSING     = 4,    /**< No flux transition detected */
    UFT_MRV_BIT_EXTRA       = 5,    /**< Extra/spurious transition */
    UFT_MRV_BIT_PROTECTED   = 6,    /**< Part of copy protection scheme */
} uft_mrv_bit_class_t;

/**
 * @brief Voting strategy
 */
typedef enum uft_mrv_strategy {
    UFT_MRV_STRATEGY_MAJORITY   = 0,    /**< Simple majority vote */
    UFT_MRV_STRATEGY_WEIGHTED   = 1,    /**< Weighted by timing quality */
    UFT_MRV_STRATEGY_CONSENSUS  = 2,    /**< Require all revolutions agree */
    UFT_MRV_STRATEGY_BEST_CRC   = 3,    /**< Prefer CRC-valid revolutions */
    UFT_MRV_STRATEGY_ADAPTIVE   = 4,    /**< Auto-select based on data */
} uft_mrv_strategy_t;

/**
 * @brief Weak bit pattern type
 */
typedef enum uft_mrv_weak_pattern {
    UFT_MRV_WEAK_RANDOM     = 0,    /**< Truly random (no flux) */
    UFT_MRV_WEAK_BIASED_0   = 1,    /**< Biased toward 0 */
    UFT_MRV_WEAK_BIASED_1   = 2,    /**< Biased toward 1 */
    UFT_MRV_WEAK_PERIODIC   = 3,    /**< Periodic pattern */
    UFT_MRV_WEAK_DEGRADED   = 4,    /**< Media degradation */
} uft_mrv_weak_pattern_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Per-bit voting statistics
 */
typedef struct uft_mrv_bit_stats {
    uint32_t        position;       /**< Bit position in track */
    uint8_t         votes_0;        /**< Count of 0 votes */
    uint8_t         votes_1;        /**< Count of 1 votes */
    uint8_t         votes_missing;  /**< Count of missing votes */
    uint8_t         confidence;     /**< Confidence 0-100 */
    uft_mrv_bit_class_t class;      /**< Classification */
    uint16_t        timing_spread;  /**< Timing variance (ns) */
} uft_mrv_bit_stats_t;

/**
 * @brief Weak bit region descriptor
 */
typedef struct uft_mrv_weak_region {
    uint32_t        start_bit;      /**< Starting bit position */
    uint32_t        length;         /**< Length in bits */
    uft_mrv_weak_pattern_t pattern; /**< Pattern type */
    uint8_t         bias;           /**< Bias percentage (0-100) */
    uint8_t         avg_confidence; /**< Average confidence in region */
    bool            is_protection;  /**< Likely copy protection */
    char            protection_id[32]; /**< Protection scheme name */
} uft_mrv_weak_region_t;

/**
 * @brief Revolution quality metrics
 */
typedef struct uft_mrv_rev_quality {
    uint8_t         rev_index;      /**< Revolution index */
    uint32_t        crc_good;       /**< Sectors with good CRC */
    uint32_t        crc_bad;        /**< Sectors with bad CRC */
    uint32_t        timing_errors;  /**< Timing anomalies */
    uint32_t        missing_bits;   /**< Missing flux transitions */
    uint32_t        extra_bits;     /**< Extra flux transitions */
    float           quality_score;  /**< Overall quality 0.0-1.0 */
    bool            usable;         /**< Suitable for voting */
} uft_mrv_rev_quality_t;

/**
 * @brief Voting parameters
 */
typedef struct uft_mrv_params {
    uft_mrv_strategy_t strategy;    /**< Voting strategy */
    uint8_t         min_confidence; /**< Minimum confidence threshold */
    uint8_t         weak_threshold; /**< Weak bit threshold */
    bool            detect_protection; /**< Enable protection detection */
    bool            preserve_weak;  /**< Preserve weak bits (don't flatten) */
    uint16_t        timing_tolerance_ns; /**< Timing comparison tolerance */
    uint16_t        min_weak_run;   /**< Minimum weak bits for region */
} uft_mrv_params_t;

/**
 * @brief Fused track result
 */
typedef struct uft_mrv_result {
    /* Fused data */
    uint8_t*        data;           /**< Fused bit data (packed) */
    uint32_t        data_bits;      /**< Total bits */
    uint32_t        data_bytes;     /**< Total bytes */
    
    /* Confidence map */
    uint8_t*        confidence;     /**< Per-bit confidence (0-100) */
    
    /* Bit statistics */
    uft_mrv_bit_stats_t* bit_stats; /**< Detailed per-bit stats */
    uint32_t        stats_count;    /**< Number of stat entries */
    
    /* Summary statistics */
    uint32_t        total_bits;     /**< Total bits analyzed */
    uint32_t        stable_bits;    /**< Bits with high confidence */
    uint32_t        weak_bits;      /**< Weak/unstable bits */
    uint32_t        missing_bits;   /**< Missing bits */
    float           overall_confidence; /**< Overall confidence 0.0-1.0 */
    
    /* Weak regions */
    uft_mrv_weak_region_t* weak_regions; /**< Detected weak regions */
    uint16_t        weak_region_count;   /**< Number of weak regions */
    
    /* Revolution analysis */
    uft_mrv_rev_quality_t* rev_quality;  /**< Per-revolution quality */
    uint8_t         rev_count;           /**< Number of revolutions */
    uint8_t         best_rev;            /**< Best single revolution */
    
    /* Protection detection */
    bool            has_protection;      /**< Protection detected */
    char            protection_scheme[64]; /**< Detected scheme name */
    uint8_t         protection_confidence; /**< Detection confidence */
} uft_mrv_result_t;

/**
 * @brief Multi-revolution analyzer context
 */
typedef struct uft_mrv_context uft_mrv_context_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * API: CONTEXT MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create analyzer context
 * @param params Parameters (NULL for defaults)
 * @return New context or NULL on error
 */
uft_mrv_context_t* uft_mrv_create(const uft_mrv_params_t* params);

/**
 * @brief Free analyzer context
 * @param ctx Context to free
 */
void uft_mrv_free(uft_mrv_context_t* ctx);

/**
 * @brief Reset context for new track
 * @param ctx Context
 */
void uft_mrv_reset(uft_mrv_context_t* ctx);

/**
 * @brief Get default parameters
 * @param params Output: default parameters
 */
void uft_mrv_get_defaults(uft_mrv_params_t* params);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: REVOLUTION INPUT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Add revolution data for analysis
 * @param ctx Context
 * @param rev UFT-IR revolution data
 * @return 0 on success, error code on failure
 */
int uft_mrv_add_revolution(uft_mrv_context_t* ctx, const uft_ir_revolution_t* rev);

/**
 * @brief Add revolution from raw flux deltas
 * @param ctx Context
 * @param deltas Flux timing deltas (nanoseconds)
 * @param count Number of deltas
 * @param bitcell_ns Nominal bitcell time
 * @return 0 on success, error code on failure
 */
int uft_mrv_add_flux(uft_mrv_context_t* ctx, const uint32_t* deltas,
                     uint32_t count, uint32_t bitcell_ns);

/**
 * @brief Add revolution from decoded bits
 * @param ctx Context
 * @param bits Bit data (packed, MSB first)
 * @param bit_count Number of bits
 * @param confidence Per-bit confidence (can be NULL)
 * @return 0 on success, error code on failure
 */
int uft_mrv_add_bits(uft_mrv_context_t* ctx, const uint8_t* bits,
                     uint32_t bit_count, const uint8_t* confidence);

/**
 * @brief Add complete UFT-IR track
 * @param ctx Context
 * @param track Track with multiple revolutions
 * @return 0 on success, error code on failure
 */
int uft_mrv_add_track(uft_mrv_context_t* ctx, const uft_ir_track_t* track);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: ANALYSIS & VOTING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Perform voting analysis
 * @param ctx Context with added revolutions
 * @param result Output: voting result (caller must free with uft_mrv_result_free)
 * @return 0 on success, error code on failure
 */
int uft_mrv_analyze(uft_mrv_context_t* ctx, uft_mrv_result_t** result);

/**
 * @brief Perform quick analysis (no detailed stats)
 * @param ctx Context
 * @param data Output: fused data buffer (caller allocates)
 * @param data_size Buffer size in bytes
 * @param bits_out Output: actual bits written
 * @return 0 on success, error code on failure
 */
int uft_mrv_analyze_quick(uft_mrv_context_t* ctx, uint8_t* data,
                          size_t data_size, uint32_t* bits_out);

/**
 * @brief Free result structure
 * @param result Result to free
 */
void uft_mrv_result_free(uft_mrv_result_t* result);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: WEAK BIT ANALYSIS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect weak bit regions
 * @param ctx Context (after adding revolutions)
 * @param regions Output: weak region array (caller allocates)
 * @param max_regions Maximum regions
 * @return Number of regions detected
 */
int uft_mrv_detect_weak_regions(uft_mrv_context_t* ctx,
                                 uft_mrv_weak_region_t* regions,
                                 int max_regions);

/**
 * @brief Analyze weak bit pattern
 * @param ctx Context
 * @param start_bit Starting bit position
 * @param length Bit length to analyze
 * @param pattern Output: detected pattern
 * @param bias Output: bias percentage
 * @return 0 on success, error code on failure
 */
int uft_mrv_analyze_weak_pattern(uft_mrv_context_t* ctx,
                                  uint32_t start_bit, uint32_t length,
                                  uft_mrv_weak_pattern_t* pattern,
                                  uint8_t* bias);

/**
 * @brief Check if position is weak bit
 * @param result Analysis result
 * @param bit_pos Bit position
 * @return true if weak bit
 */
bool uft_mrv_is_weak_bit(const uft_mrv_result_t* result, uint32_t bit_pos);

/**
 * @brief Get weak bit probability
 * @param result Analysis result
 * @param bit_pos Bit position
 * @return Probability of being 1 (0-100)
 */
uint8_t uft_mrv_get_weak_probability(const uft_mrv_result_t* result,
                                      uint32_t bit_pos);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: COPY PROTECTION DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect copy protection from weak bit patterns
 * @param result Analysis result
 * @param scheme_name Output: protection scheme name
 * @param name_size Buffer size
 * @param confidence Output: detection confidence (0-100)
 * @return true if protection detected
 */
bool uft_mrv_detect_protection(const uft_mrv_result_t* result,
                                char* scheme_name, size_t name_size,
                                uint8_t* confidence);

/**
 * @brief Match weak pattern against known protection
 * @param regions Weak regions
 * @param region_count Number of regions
 * @param scheme_id Output: protection scheme ID
 * @return Match confidence (0-100), 0 if no match
 */
uint8_t uft_mrv_match_protection(const uft_mrv_weak_region_t* regions,
                                  int region_count, uint32_t* scheme_id);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: QUALITY ASSESSMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Evaluate revolution quality
 * @param ctx Context
 * @param rev_index Revolution index
 * @param quality Output: quality metrics
 * @return 0 on success, error code on failure
 */
int uft_mrv_eval_revolution(uft_mrv_context_t* ctx, int rev_index,
                            uft_mrv_rev_quality_t* quality);

/**
 * @brief Find best revolution
 * @param ctx Context (after adding revolutions)
 * @return Best revolution index, -1 on error
 */
int uft_mrv_find_best_revolution(uft_mrv_context_t* ctx);

/**
 * @brief Get overall track quality
 * @param result Analysis result
 * @return Quality 0.0-1.0
 */
float uft_mrv_get_quality(const uft_mrv_result_t* result);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: OUTPUT GENERATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generate fused UFT-IR track
 * @param result Analysis result
 * @param track Output: new UFT-IR track (caller must free)
 * @return 0 on success, error code on failure
 */
int uft_mrv_to_ir_track(const uft_mrv_result_t* result, uft_ir_track_t** track);

/**
 * @brief Export result to JSON
 * @param result Analysis result
 * @param include_bit_stats Include per-bit statistics
 * @return JSON string (caller must free) or NULL
 */
char* uft_mrv_to_json(const uft_mrv_result_t* result, bool include_bit_stats);

/**
 * @brief Generate text summary
 * @param result Analysis result
 * @return Summary string (caller must free) or NULL
 */
char* uft_mrv_to_summary(const uft_mrv_result_t* result);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: UTILITIES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get bit class name
 * @param class Bit class
 * @return Static name string
 */
const char* uft_mrv_bit_class_name(uft_mrv_bit_class_t class);

/**
 * @brief Get strategy name
 * @param strategy Voting strategy
 * @return Static name string
 */
const char* uft_mrv_strategy_name(uft_mrv_strategy_t strategy);

/**
 * @brief Get weak pattern name
 * @param pattern Weak pattern type
 * @return Static name string
 */
const char* uft_mrv_weak_pattern_name(uft_mrv_weak_pattern_t pattern);

/* ═══════════════════════════════════════════════════════════════════════════
 * ERROR CODES
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_MRV_OK                   0
#define UFT_MRV_ERR_INVALID         -1
#define UFT_MRV_ERR_NOMEM           -2
#define UFT_MRV_ERR_NO_DATA         -3
#define UFT_MRV_ERR_TOO_FEW_REVS    -4
#define UFT_MRV_ERR_OVERFLOW        -5
#define UFT_MRV_ERR_ALIGNMENT       -6

/**
 * @brief Get error message
 * @param err Error code
 * @return Static error message string
 */
const char* uft_mrv_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MULTIREV_H */
