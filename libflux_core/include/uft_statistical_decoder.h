/**
 * @file uft_statistical_decoder.h
 * @brief Statistical Flux Decoder - PROFESSIONAL EDITION
 * 
 * FORENSIC-GRADE DECODING
 * 
 * Features:
 * ✅ Jitter histograms
 * ✅ Adaptive clock recovery (PLL simulation)
 * ✅ Weak bit detection (multi-read)
 * ✅ Confidence scoring
 * ✅ Variable bitrate support (Speedlock)
 * ✅ Error statistics
 * 
 * Based on techniques from:
 * - Disk2FDI
 * - DiscFerret
 * - Professional forensic tools
 * 
 * @version 3.0.0 (Professional Edition)
 * @date 2024-12-27
 */

#ifndef UFT_STATISTICAL_DECODER_H
#define UFT_STATISTICAL_DECODER_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * HISTOGRAM ANALYSIS
 * ======================================================================== */

#define UFT_HISTOGRAM_BINS 256

/**
 * @brief Flux histogram for jitter analysis
 */
typedef struct {
    uint32_t bins[UFT_HISTOGRAM_BINS];  /**< Histogram bins */
    uint32_t bin_width_ns;              /**< Width of each bin */
    uint32_t total_samples;             /**< Total samples */
    
    /* Peaks (clock periods) */
    uint32_t peak_count;
    struct {
        uint32_t bin_index;
        uint32_t value_ns;
        uint32_t count;
        float confidence;
    } peaks[8];
    
} uft_histogram_t;

/**
 * @brief Build histogram from flux data
 */
uft_rc_t uft_histogram_build(
    const uint32_t* flux_ns,
    uint32_t count,
    uint32_t bin_width_ns,
    uft_histogram_t** histogram
);

/**
 * @brief Find peaks in histogram (clock periods)
 */
uft_rc_t uft_histogram_find_peaks(
    uft_histogram_t* histogram,
    uint32_t min_peak_height
);

/**
 * @brief Get most likely cell time from histogram
 */
uint32_t uft_histogram_get_cell_time(const uft_histogram_t* histogram);

/* ========================================================================
 * ADAPTIVE CLOCK RECOVERY (PLL)
 * ======================================================================== */

/**
 * @brief PLL (Phase-Locked Loop) state
 */
typedef struct {
    /* Configuration */
    uint32_t nominal_cell_ns;       /**< Nominal cell time */
    float gain;                     /**< PLL gain (0.1-0.5) */
    float damping;                  /**< Damping factor */
    
    /* State */
    uint32_t current_cell_ns;       /**< Current cell estimate */
    int32_t phase_error;            /**< Phase error accumulator */
    uint32_t cell_counter;          /**< Cell counter */
    
    /* Statistics */
    uint32_t transitions_processed;
    uint32_t phase_corrections;
    int32_t max_phase_error;
    
} uft_pll_t;

/**
 * @brief Create PLL
 */
uft_rc_t uft_pll_create(
    uint32_t nominal_cell_ns,
    uft_pll_t** pll
);

/**
 * @brief Process flux transition through PLL
 * 
 * @param pll PLL state
 * @param flux_ns Flux time
 * @param[out] cells Number of cells
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_pll_process(
    uft_pll_t* pll,
    uint32_t flux_ns,
    uint32_t* cells
);

/**
 * @brief Reset PLL
 */
void uft_pll_reset(uft_pll_t* pll);

/**
 * @brief Destroy PLL
 */
void uft_pll_destroy(uft_pll_t** pll);

/* ========================================================================
 * STATISTICAL DECODER
 * ======================================================================== */

/**
 * @brief Decoding confidence
 */
typedef enum {
    UFT_CONFIDENCE_HIGH = 0,     /**< >95% confidence */
    UFT_CONFIDENCE_MEDIUM,       /**< 80-95% */
    UFT_CONFIDENCE_LOW,          /**< 60-80% */
    UFT_CONFIDENCE_WEAK_BIT,     /**< <60%, likely weak bit */
    UFT_CONFIDENCE_ERROR         /**< Cannot decode */
} uft_confidence_level_t;

/**
 * @brief Decoded bit with confidence
 */
typedef struct {
    uint8_t value;                  /**< Bit value (0 or 1) */
    uft_confidence_level_t confidence;
    float confidence_score;         /**< 0.0-1.0 */
    
    /* Weak bit info */
    bool is_weak;
    uint8_t read_values[8];         /**< Multiple read results */
    uint8_t read_count;
    
} uft_decoded_bit_t;

/**
 * @brief Decode statistics
 */
typedef struct {
    uint32_t total_bits;
    uint32_t high_confidence;
    uint32_t medium_confidence;
    uint32_t low_confidence;
    uint32_t weak_bits;
    uint32_t errors;
    
    /* Jitter stats */
    float avg_jitter_percent;
    float max_jitter_percent;
    
} uft_decode_stats_t;

/**
 * @brief Statistical decoder context
 */
typedef struct uft_statistical_decoder uft_statistical_decoder_t;

/**
 * @brief Create statistical decoder
 */
uft_rc_t uft_statistical_decoder_create(
    uint32_t nominal_bitrate,       /**< Expected bitrate (Hz) */
    uft_statistical_decoder_t** decoder
);

/**
 * @brief Decode flux stream to bits
 * 
 * @param decoder Decoder context
 * @param flux_ns Flux transitions (nanoseconds)
 * @param flux_count Number of transitions
 * @param[out] bits Decoded bits (caller frees)
 * @param[out] bit_count Number of bits
 * @param[out] stats Decode statistics (optional)
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_statistical_decode(
    uft_statistical_decoder_t* decoder,
    const uint32_t* flux_ns,
    uint32_t flux_count,
    uft_decoded_bit_t** bits,
    uint32_t* bit_count,
    uft_decode_stats_t* stats
);

/**
 * @brief Enable weak bit detection (multi-read mode)
 */
uft_rc_t uft_statistical_decoder_set_weak_bit_mode(
    uft_statistical_decoder_t* decoder,
    bool enabled,
    uint8_t read_count
);

/**
 * @brief Enable variable bitrate mode (Speedlock)
 */
uft_rc_t uft_statistical_decoder_set_variable_bitrate(
    uft_statistical_decoder_t* decoder,
    bool enabled,
    uint32_t min_bitrate,
    uint32_t max_bitrate
);

/**
 * @brief Destroy decoder
 */
void uft_statistical_decoder_destroy(uft_statistical_decoder_t** decoder);

/* ========================================================================
 * MULTI-READ WEAK BIT DETECTION
 * ======================================================================== */

/**
 * @brief Compare multiple reads to detect weak bits
 */
uft_rc_t uft_weak_bit_compare(
    const uft_decoded_bit_t** reads,   /**< Array of read results */
    uint32_t read_count,                /**< Number of reads */
    uint32_t bit_count,                 /**< Bits per read */
    uft_decoded_bit_t** consensus,      /**< Consensus result */
    uint32_t* weak_bit_positions        /**< Positions of weak bits */
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_STATISTICAL_DECODER_H */
