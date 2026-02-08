/**
 * @file uft_histogram.h
 * @brief Unified Histogram Analysis Library
 * 
 * Consolidates histogram functionality from:
 * - fluxStreamAnalyzer.c (computehistogram, detectpeaks)
 * - adaptive_mfm.c (build_histogram, find_histogram_peaks)
 * - uft_encoding_detect.c (uft_encoding_build_histogram)
 * - uft_mfm_decoder.c (build_histogram)
 * 
 * Features:
 * - Generic histogram builder for any data type
 * - Peak detection with configurable thresholds
 * - MFM/FM cell timing analysis
 * - Statistical analysis (mean, stddev, etc.)
 * 
 * @version 1.0
 * @date 2026-01-07
 */

#ifndef UFT_HISTOGRAM_H
#define UFT_HISTOGRAM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum histogram bins */
#define UFT_HIST_MAX_BINS       65536

/** Maximum peaks to detect */
#define UFT_HIST_MAX_PEAKS      16

/** Default histogram size for byte analysis */
#define UFT_HIST_BYTE_BINS      256

/** Default histogram size for pulse timing */
#define UFT_HIST_PULSE_BINS     512

/*===========================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Peak information
 */
typedef struct {
    uint32_t position;       /**< Bin position of peak */
    uint32_t count;          /**< Count at peak */
    uint32_t width;          /**< Width at half-maximum */
    float    center;         /**< Weighted center (sub-bin precision) */
} uft_hist_peak_t;

/**
 * @brief Histogram statistics
 */
typedef struct {
    uint32_t min_bin;        /**< First non-zero bin */
    uint32_t max_bin;        /**< Last non-zero bin */
    uint32_t max_count;      /**< Maximum count in any bin */
    uint32_t max_bin_pos;    /**< Position of max count */
    uint64_t total_samples;  /**< Total samples counted */
    double   mean;           /**< Weighted mean */
    double   stddev;         /**< Standard deviation */
    double   median;         /**< Median value */
} uft_hist_stats_t;

/**
 * @brief Histogram context
 */
typedef struct {
    uint32_t *bins;          /**< Histogram bins */
    uint32_t  bin_count;     /**< Number of bins */
    uint32_t  bin_width;     /**< Width of each bin (for scaled histograms) */
    uint32_t  offset;        /**< Offset for first bin */
    
    /* Statistics (computed on demand) */
    uft_hist_stats_t stats;
    bool stats_valid;
    
    /* Detected peaks */
    uft_hist_peak_t peaks[UFT_HIST_MAX_PEAKS];
    uint32_t peak_count;
    bool peaks_valid;
    
} uft_histogram_t;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

/**
 * @brief Create histogram
 * 
 * @param bin_count  Number of bins (0 for auto based on data range)
 * @return Histogram context or NULL on error
 */
uft_histogram_t* uft_hist_create(uint32_t bin_count);

/**
 * @brief Destroy histogram
 */
void uft_hist_destroy(uft_histogram_t *hist);

/**
 * @brief Clear histogram (reset all bins to zero)
 */
void uft_hist_clear(uft_histogram_t *hist);

/*===========================================================================
 * Building Histograms
 *===========================================================================*/

/**
 * @brief Add uint8_t data to histogram
 */
void uft_hist_add_u8(uft_histogram_t *hist, const uint8_t *data, size_t len);

/**
 * @brief Add uint16_t data to histogram
 */
void uft_hist_add_u16(uft_histogram_t *hist, const uint16_t *data, size_t len);

/**
 * @brief Add uint32_t data to histogram
 */
void uft_hist_add_u32(uft_histogram_t *hist, const uint32_t *data, size_t len);

/**
 * @brief Add scaled values (val / scale maps to bin)
 */
void uft_hist_add_scaled(uft_histogram_t *hist, 
                         const uint32_t *data, size_t len,
                         uint32_t scale);

/**
 * @brief Add single value to histogram
 */
static inline void uft_hist_add_one(uft_histogram_t *hist, uint32_t value) {
    if (hist && value < hist->bin_count) {
        hist->bins[value]++;
        hist->stats_valid = false;
        hist->peaks_valid = false;
    }
}

/*===========================================================================
 * Analysis
 *===========================================================================*/

/**
 * @brief Compute histogram statistics
 * 
 * Computes: min, max, mean, stddev, median
 * Results cached until histogram modified.
 */
const uft_hist_stats_t* uft_hist_stats(uft_histogram_t *hist);

/**
 * @brief Find peaks in histogram
 * 
 * @param hist          Histogram to analyze
 * @param min_height    Minimum peak height (0 = auto)
 * @param min_distance  Minimum distance between peaks (0 = auto)
 * @return Number of peaks found
 */
uint32_t uft_hist_find_peaks(uft_histogram_t *hist,
                             uint32_t min_height,
                             uint32_t min_distance);

/**
 * @brief Get peak array
 */
const uft_hist_peak_t* uft_hist_get_peaks(const uft_histogram_t *hist,
                                          uint32_t *count);

/*===========================================================================
 * MFM/FM Timing Analysis
 *===========================================================================*/

/**
 * @brief Detect MFM cell timing from pulse histogram
 * 
 * MFM has 3 peaks at approximately 1T, 1.5T, 2T
 * where T is the bit cell time.
 * 
 * @param hist      Pulse timing histogram
 * @param cell_time Output: detected bit cell time
 * @return true if valid MFM pattern detected
 */
bool uft_hist_detect_mfm_timing(const uft_histogram_t *hist,
                                 uint32_t *cell_time);

/**
 * @brief Detect FM cell timing from pulse histogram
 * 
 * FM has 2 peaks at approximately 1T and 2T.
 * 
 * @param hist      Pulse timing histogram
 * @param cell_time Output: detected bit cell time
 * @return true if valid FM pattern detected
 */
bool uft_hist_detect_fm_timing(const uft_histogram_t *hist,
                                uint32_t *cell_time);

/**
 * @brief Estimate data rate from cell timing
 * 
 * @param cell_time_ns  Cell time in nanoseconds
 * @return Data rate in bits per second
 */
static inline uint32_t uft_hist_cell_to_datarate(uint32_t cell_time_ns) {
    if (cell_time_ns == 0) return 0;
    return (uint32_t)(1000000000ULL / cell_time_ns);
}

/*===========================================================================
 * Utility
 *===========================================================================*/

/**
 * @brief Get bin value
 */
static inline uint32_t uft_hist_get(const uft_histogram_t *hist, uint32_t bin) {
    if (!hist || bin >= hist->bin_count) return 0;
    return hist->bins[bin];
}

/**
 * @brief Set bin value
 */
static inline void uft_hist_set(uft_histogram_t *hist, uint32_t bin, uint32_t val) {
    if (hist && bin < hist->bin_count) {
        hist->bins[bin] = val;
        hist->stats_valid = false;
        hist->peaks_valid = false;
    }
}

/**
 * @brief Smooth histogram with moving average
 * 
 * @param hist    Histogram to smooth (modified in place)
 * @param window  Window size (3, 5, 7, etc.)
 */
void uft_hist_smooth(uft_histogram_t *hist, uint32_t window);

/**
 * @brief Normalize histogram to range [0, scale]
 * 
 * @param hist   Histogram to normalize (modified in place)
 * @param scale  Maximum value after normalization
 */
void uft_hist_normalize(uft_histogram_t *hist, uint32_t scale);

/**
 * @brief Print histogram to file (ASCII art style)
 */
void uft_hist_print(const uft_histogram_t *hist, 
                    FILE *out, 
                    uint32_t width,
                    uint32_t height);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HISTOGRAM_H */
