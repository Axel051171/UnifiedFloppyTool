/**
 * @file uft_cell_analyzer.h
 * @brief Enhanced Cell-Level Flux Analysis
 * 
 * Based on DTC's CCellAnalyzer - provides detailed cell timing
 * analysis and quality metrics.
 * 
 * CLEAN-ROOM implementation based on observable requirements.
 */

#ifndef UFT_CELL_ANALYZER_H
#define UFT_CELL_ANALYZER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_error.h"

/* Map UFT_OK to UFT_SUCCESS for compatibility */
#ifndef UFT_OK
#define UFT_OK UFT_SUCCESS
#endif
#ifndef UFT_ERR_INVALID_PARAM
#define UFT_ERR_INVALID_PARAM UFT_ERR_INVALID_ARG
#endif
#ifndef UFT_ERR_MEMORY
#define UFT_ERR_MEMORY (-20)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cell search modes (like DTC -x parameter) */
#define UFT_CELL_SEARCH_OFF         0
#define UFT_CELL_SEARCH_NORMAL      1
#define UFT_CELL_SEARCH_EXTENDED    2

/* Common cell times in nanoseconds */
#define UFT_CELL_MFM_DD_NS         2000   /* 250 kbps MFM DD */
#define UFT_CELL_MFM_HD_NS         1000   /* 500 kbps MFM HD */
#define UFT_CELL_MFM_ED_NS          500   /* 1 Mbps MFM ED */
#define UFT_CELL_FM_DD_NS          4000   /* 125 kbps FM DD */
#define UFT_CELL_GCR_C64_NS        3250   /* ~307 kbps C64 zone 0 */
#define UFT_CELL_GCR_APPLE_NS      4000   /* 250 kbps Apple */

/* Default tolerances */
#define UFT_CELL_DEFAULT_TOLERANCE  0.15  /* 15% tolerance */
#define UFT_CELL_TIGHT_TOLERANCE    0.08  /* 8% tight tolerance */
#define UFT_CELL_LOOSE_TOLERANCE    0.25  /* 25% loose tolerance */

/* ============================================================================
 * Data Types
 * ============================================================================ */

/**
 * @brief Cell analysis options
 */
typedef struct {
    double cell_time_ns;          /**< Nominal cell time in nanoseconds */
    double tolerance;             /**< Timing tolerance (0.0-1.0) */
    double sample_rate_hz;        /**< Sample rate in Hz */
    uint8_t search_mode;          /**< Cell band search mode */
    uint32_t pll_window;          /**< PLL tracking window size */
    double pll_gain;              /**< PLL gain factor */
    bool detect_weak_bits;        /**< Enable weak bit detection */
    double weak_threshold;        /**< Weak bit threshold */
    bool auto_detect_rate;        /**< Auto-detect cell rate */
} uft_cell_options_t;

/**
 * @brief Information about a single decoded cell
 */
typedef struct {
    uint64_t position;            /**< Bit position in stream */
    double actual_time_ns;        /**< Actual cell time */
    double deviation_ns;          /**< Deviation from nominal */
    double deviation_pct;         /**< Deviation percentage */
    uint8_t value;                /**< Decoded bit value (0 or 1) */
    uint8_t confidence;           /**< Confidence 0-100 */
    bool is_weak;                 /**< Weak bit flag */
    bool is_sync;                 /**< Part of sync pattern */
} uft_cell_info_t;

/**
 * @brief Cell timing histogram bin
 */
typedef struct {
    double center_ns;             /**< Bin center in ns */
    uint32_t count;               /**< Number of cells in bin */
    double percentage;            /**< Percentage of total */
} uft_cell_histogram_bin_t;

/**
 * @brief Cell timing histogram
 */
typedef struct {
    uft_cell_histogram_bin_t *bins;
    size_t bin_count;
    double min_time_ns;
    double max_time_ns;
    double peak_time_ns;          /**< Most common cell time */
    uint32_t total_cells;
} uft_cell_histogram_t;

/**
 * @brief Cell analysis result
 */
typedef struct {
    /* Cell data */
    uft_cell_info_t *cells;       /**< Array of cell info */
    size_t cell_count;            /**< Number of cells decoded */
    
    /* Decoded bits */
    uint8_t *decoded_data;        /**< Decoded bit stream */
    size_t bit_count;             /**< Number of decoded bits */
    
    /* Statistics */
    double average_cell_time;     /**< Average cell time */
    double cell_time_stddev;      /**< Standard deviation */
    double min_cell_time;         /**< Minimum observed */
    double max_cell_time;         /**< Maximum observed */
    
    /* Quality metrics */
    uint32_t weak_bit_count;      /**< Number of weak bits */
    uint32_t error_count;         /**< Decoding errors */
    uint8_t overall_quality;      /**< Overall quality 0-100 */
    
    /* Sync detection */
    uint32_t sync_count;          /**< Sync patterns found */
    uint64_t *sync_positions;     /**< Positions of sync patterns */
    
    /* Histogram */
    uft_cell_histogram_t histogram;
    
    /* PLL state */
    double final_pll_phase;
    double final_pll_freq;
    
    /* Auto-detected rate */
    double detected_cell_time;
    double detected_bitrate;
} uft_cell_result_t;

/**
 * @brief Cell band information (for multi-rate disks)
 */
typedef struct {
    uint8_t zone;                 /**< Speed zone (C64: 0-3) */
    double cell_time_ns;          /**< Cell time for this zone */
    uint64_t start_position;      /**< Start bit position */
    uint64_t end_position;        /**< End bit position */
    uint32_t cell_count;          /**< Cells in this band */
} uft_cell_band_t;

/**
 * @brief Multi-zone analysis result
 */
typedef struct {
    uft_cell_band_t *bands;
    size_t band_count;
    bool is_multi_rate;           /**< True if multiple rates detected */
} uft_cell_band_result_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Initialize cell options with defaults
 */
void uft_cell_options_init(uft_cell_options_t *options);

/**
 * @brief Initialize cell options for specific encoding
 */
void uft_cell_options_for_encoding(
    uft_cell_options_t *options,
    const char *encoding  /* "MFM_DD", "MFM_HD", "FM", "GCR_C64", "GCR_APPLE" */
);

/**
 * @brief Analyze flux data at cell level
 * 
 * @param flux_times     Array of flux timing intervals
 * @param flux_count     Number of flux intervals
 * @param options        Analysis options
 * @param result         Output analysis result
 * @return UFT_OK on success
 */
uft_error_t uft_cell_analyze(
    const uint32_t *flux_times,
    size_t flux_count,
    const uft_cell_options_t *options,
    uft_cell_result_t *result
);

/**
 * @brief Auto-detect cell timing from flux data
 * 
 * @param flux_times     Array of flux timing intervals
 * @param flux_count     Number of flux intervals
 * @param sample_rate_hz Sample rate
 * @param detected_ns    Output detected cell time in ns
 * @param confidence     Output confidence 0-100
 * @return UFT_OK on success
 */
uft_error_t uft_cell_auto_detect(
    const uint32_t *flux_times,
    size_t flux_count,
    double sample_rate_hz,
    double *detected_ns,
    uint8_t *confidence
);

/**
 * @brief Detect cell bands (for multi-rate disks like C64)
 * 
 * @param flux_times     Array of flux timing intervals
 * @param flux_count     Number of flux intervals
 * @param options        Analysis options
 * @param band_result    Output band information
 * @return UFT_OK on success
 */
uft_error_t uft_cell_detect_bands(
    const uint32_t *flux_times,
    size_t flux_count,
    const uft_cell_options_t *options,
    uft_cell_band_result_t *band_result
);

/**
 * @brief Build cell timing histogram
 * 
 * @param flux_times     Array of flux timing intervals
 * @param flux_count     Number of flux intervals
 * @param sample_rate_hz Sample rate
 * @param bin_width_ns   Histogram bin width
 * @param histogram      Output histogram
 * @return UFT_OK on success
 */
uft_error_t uft_cell_build_histogram(
    const uint32_t *flux_times,
    size_t flux_count,
    double sample_rate_hz,
    double bin_width_ns,
    uft_cell_histogram_t *histogram
);

/**
 * @brief Decode flux to bits using PLL
 * 
 * @param flux_times     Array of flux timing intervals
 * @param flux_count     Number of flux intervals
 * @param options        Cell options
 * @param output_bits    Output bit buffer
 * @param max_bits       Maximum bits to decode
 * @param actual_bits    Actual bits decoded
 * @return UFT_OK on success
 */
uft_error_t uft_cell_decode_pll(
    const uint32_t *flux_times,
    size_t flux_count,
    const uft_cell_options_t *options,
    uint8_t *output_bits,
    size_t max_bits,
    size_t *actual_bits
);

/**
 * @brief Find sync patterns in decoded data
 * 
 * @param decoded_bits   Decoded bit stream
 * @param bit_count      Number of bits
 * @param sync_pattern   Pattern to find
 * @param pattern_bits   Pattern length in bits
 * @param positions      Output array of positions
 * @param max_positions  Maximum positions to find
 * @param found_count    Actual count found
 * @return UFT_OK on success
 */
uft_error_t uft_cell_find_sync(
    const uint8_t *decoded_bits,
    size_t bit_count,
    uint64_t sync_pattern,
    uint8_t pattern_bits,
    uint64_t *positions,
    size_t max_positions,
    size_t *found_count
);

/**
 * @brief Calculate cell quality metrics
 */
void uft_cell_calc_quality(uft_cell_result_t *result);

/**
 * @brief Free cell analysis result
 */
void uft_cell_result_free(uft_cell_result_t *result);

/**
 * @brief Free cell band result
 */
void uft_cell_band_free(uft_cell_band_result_t *result);

/**
 * @brief Free histogram
 */
void uft_cell_histogram_free(uft_cell_histogram_t *histogram);

/**
 * @brief Export cell analysis to JSON
 */
size_t uft_cell_result_to_json(
    const uft_cell_result_t *result,
    char *buffer,
    size_t size
);

/* ============================================================================
 * PLL State (for advanced use)
 * ============================================================================ */

/**
 * @brief PLL state for manual stepping
 */
typedef struct {
    double phase;             /**< Current phase in samples */
    double frequency;         /**< Current frequency in samples/cell */
    double nominal_freq;      /**< Nominal frequency */
    double gain;              /**< PLL gain */
    uint32_t window;          /**< Window size */
    bool locked;              /**< PLL is locked */
    uint64_t bit_position;    /**< Current bit position */
} uft_pll_state_t;

/**
 * @brief Initialize PLL state
 */
void uft_pll_init(
    uft_pll_state_t *pll,
    double cell_time_samples,
    double gain
);

/**
 * @brief Step PLL with flux transition
 * 
 * @param pll            PLL state
 * @param flux_interval  Flux interval in samples
 * @param output_bit     Output decoded bit
 * @return Number of bits output (0, 1, or more)
 */
int uft_pll_step(
    uft_pll_state_t *pll,
    uint32_t flux_interval,
    uint8_t *output_bit
);

/**
 * @brief Reset PLL to initial state
 */
void uft_pll_reset(uft_pll_state_t *pll);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CELL_ANALYZER_H */
