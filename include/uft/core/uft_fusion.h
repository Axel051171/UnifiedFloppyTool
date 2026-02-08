/**
 * @file uft_fusion.h
 * @brief Multi-revision data fusion with weighted confidence
 * 
 * P1-007: Replaces simple majority voting with weighted merge
 * 
 * Features:
 * - Confidence-weighted voting
 * - Timing correlation
 * - Weak bit detection
 * - CRC-based weight boosting
 */

#ifndef UFT_FUSION_H
#define UFT_FUSION_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum revisions for fusion */
#define UFT_FUSION_MAX_REVISIONS 32

/**
 * @brief Fusion algorithm selection
 */
typedef enum {
    UFT_FUSION_MAJORITY,       /**< Simple majority voting */
    UFT_FUSION_WEIGHTED,       /**< Confidence-weighted voting */
    UFT_FUSION_TIMING,         /**< Timing-correlated fusion */
    UFT_FUSION_ADAPTIVE,       /**< Adaptive (auto-select best) */
} uft_fusion_method_t;

/**
 * @brief Per-bit fusion result
 */
typedef struct {
    uint8_t value;             /**< Final bit value */
    uint8_t confidence;        /**< Confidence 0-255 */
    bool is_weak;              /**< Detected as weak bit */
    uint8_t agreement;         /**< Revisions that agree (count) */
    double timing_variance;    /**< Timing variance across revisions */
} uft_fused_bit_t;

/**
 * @brief Fusion options
 */
typedef struct {
    uft_fusion_method_t method;
    
    /* Weighting */
    uint8_t crc_valid_bonus;   /**< Extra weight for CRC-valid revisions (0-100) */
    uint8_t recent_bonus;      /**< Extra weight for recent revisions (0-50) */
    
    /* Thresholds */
    uint8_t weak_threshold;    /**< Disagreement count to mark weak (default: 2) */
    float confidence_min;      /**< Minimum confidence to use revision (0.0-1.0) */
    
    /* Timing */
    bool use_timing;           /**< Use timing correlation */
    double timing_tolerance;   /**< Timing tolerance in ns */
    
    /* Output */
    bool generate_weak_map;    /**< Generate weak bit map */
    bool generate_confidence;  /**< Generate per-bit confidence */
    
} uft_fusion_options_t;

/**
 * @brief Fusion result/statistics
 */
typedef struct {
    bool success;
    uft_error_t error;
    
    /* Statistics */
    size_t total_bits;
    size_t unanimous_bits;     /**< All revisions agree */
    size_t majority_bits;      /**< Majority agrees */
    size_t weak_bits;          /**< Detected weak bits */
    size_t uncertain_bits;     /**< Low confidence */
    
    /* Quality */
    float overall_confidence;  /**< 0.0-1.0 */
    float agreement_ratio;     /**< Average agreement */
    
    /* Per-revision stats */
    struct {
        uint8_t quality;       /**< 0-100 */
        bool crc_valid;
        size_t bits_contributed;
    } revision_stats[UFT_FUSION_MAX_REVISIONS];
    size_t revision_count;
    
} uft_fusion_result_t;

/**
 * @brief Revision input for fusion
 */
typedef struct {
    const uint8_t *data;       /**< Bit data */
    size_t bit_count;          /**< Number of bits */
    
    /* Optional metadata */
    const uint8_t *confidence; /**< Per-bit confidence (optional) */
    const double *timing;      /**< Per-bit timing in ns (optional) */
    
    /* Quality hints */
    bool crc_valid;            /**< CRC was valid */
    uint8_t quality;           /**< Overall quality 0-100 */
    uint8_t revision_index;    /**< Original revision number */
    
} uft_revision_input_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Initialize fusion options with defaults
 */
void uft_fusion_options_init(uft_fusion_options_t *opts);

/**
 * @brief Fuse multiple revisions into single output
 * @param revisions Array of revision inputs
 * @param rev_count Number of revisions
 * @param out_data Output data buffer (must be pre-allocated)
 * @param out_bits Output bit count
 * @param out_confidence Per-bit confidence (optional, pre-allocated)
 * @param out_weak Per-bit weak flags (optional, pre-allocated)
 * @param opts Fusion options (NULL for defaults)
 * @param result Output statistics (optional)
 * @return UFT_OK on success
 */
uft_error_t uft_fusion_merge(const uft_revision_input_t *revisions,
                             size_t rev_count,
                             uint8_t *out_data,
                             size_t *out_bits,
                             uint8_t *out_confidence,
                             bool *out_weak,
                             const uft_fusion_options_t *opts,
                             uft_fusion_result_t *result);

/**
 * @brief Fuse single bit position
 */
uft_fused_bit_t uft_fusion_bit(const uft_revision_input_t *revisions,
                               size_t rev_count,
                               size_t bit_pos,
                               const uft_fusion_options_t *opts);

/**
 * @brief Fuse tracks
 */
uft_error_t uft_fusion_tracks(const uft_track_t *const *tracks,
                              size_t track_count,
                              uft_track_t *out_track,
                              const uft_fusion_options_t *opts,
                              uft_fusion_result_t *result);

/**
 * @brief Fuse sectors
 */
uft_error_t uft_fusion_sectors(const uft_sector_t *const *sectors,
                               size_t sector_count,
                               uft_sector_t *out_sector,
                               const uft_fusion_options_t *opts,
                               uft_fusion_result_t *result);

/**
 * @brief Analyze revisions for quality
 */
void uft_fusion_analyze(const uft_revision_input_t *revisions,
                        size_t rev_count,
                        uft_fusion_result_t *result);

/**
 * @brief Get recommended fusion method
 */
uft_fusion_method_t uft_fusion_recommend_method(
    const uft_revision_input_t *revisions,
    size_t rev_count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FUSION_H */
