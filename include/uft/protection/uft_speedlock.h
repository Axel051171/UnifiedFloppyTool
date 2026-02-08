/**
 * @file uft_speedlock.h
 * @brief Speedlock Variable-Density Protection Handler
 * 
 * Implements detection and analysis of Speedlock protection.
 * Clean-room reimplementation for UFT.
 * 
 * Speedlock uses variable-density regions on the track:
 * - Normal region: 100% bitcell (2µs)
 * - Long region: ~110% bitcell (slower, starting ~77500 bits)
 * - Short region: ~90% bitcell (faster)
 * - Return to normal
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_SPEEDLOCK_H
#define UFT_SPEEDLOCK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Speedlock detection thresholds (percentage of nominal bitcell) */
#define UFT_SPEEDLOCK_LONG_THRESHOLD    108  /**< +8% = long bitcells detected */
#define UFT_SPEEDLOCK_SHORT_THRESHOLD    92  /**< -8% = short bitcells detected */
#define UFT_SPEEDLOCK_NORMAL_THRESHOLD   98  /**< -2% = back to normal */

/** Region timing ratios (percentage) */
#define UFT_SPEEDLOCK_NORMAL_RATIO      100
#define UFT_SPEEDLOCK_LONG_RATIO        110  /**< 10% slower */
#define UFT_SPEEDLOCK_SHORT_RATIO        90  /**< 10% faster */

/** Expected region positions (in bits from index) */
#define UFT_SPEEDLOCK_LONG_START_MIN    75000
#define UFT_SPEEDLOCK_LONG_START_MAX    80000
#define UFT_SPEEDLOCK_LONG_START_TYP    77500

/** Measurement parameters */
#define UFT_SPEEDLOCK_SAMPLE_COUNT      2000  /**< Samples for baseline measurement */
#define UFT_SPEEDLOCK_WINDOW_SIZE       32    /**< Bits to average for region detection */

/** Minimum track length for Speedlock (must be full Amiga track) */
#define UFT_SPEEDLOCK_MIN_TRACK_BITS    100000

/*===========================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Speedlock variant type
 */
typedef enum {
    UFT_SPEEDLOCK_UNKNOWN = 0,     /**< Not detected */
    UFT_SPEEDLOCK_V1,              /**< Original version */
    UFT_SPEEDLOCK_V2,              /**< Enhanced version with more regions */
    UFT_SPEEDLOCK_V3               /**< Latest version */
} uft_speedlock_variant_t;

/**
 * @brief Speedlock region type
 */
typedef enum {
    UFT_SPEEDLOCK_REGION_NORMAL = 0,
    UFT_SPEEDLOCK_REGION_LONG,     /**< Slow region (110%) */
    UFT_SPEEDLOCK_REGION_SHORT     /**< Fast region (90%) */
} uft_speedlock_region_type_t;

/**
 * @brief Speedlock confidence level
 */
typedef enum {
    UFT_SPEEDLOCK_CONF_NONE = 0,
    UFT_SPEEDLOCK_CONF_POSSIBLE,   /**< Some timing variation found */
    UFT_SPEEDLOCK_CONF_LIKELY,     /**< Correct region sequence */
    UFT_SPEEDLOCK_CONF_CERTAIN     /**< Full pattern + timing match */
} uft_speedlock_confidence_t;

/**
 * @brief Speedlock region description
 */
typedef struct {
    uft_speedlock_region_type_t type;
    uint32_t start_bit;            /**< Region start position */
    uint32_t end_bit;              /**< Region end position */
    uint32_t length_bits;          /**< Region length */
    float    measured_ratio;       /**< Actual timing ratio measured */
    float    expected_ratio;       /**< Expected timing ratio */
    bool     timing_valid;         /**< True if timing matches expected */
} uft_speedlock_region_t;

/**
 * @brief Speedlock detection parameters
 */
typedef struct {
    uint32_t long_region_start;    /**< Bit offset where long bitcells begin */
    uint32_t long_region_end;      /**< Bit offset where long region ends */
    uint32_t short_region_start;   /**< Bit offset where short bitcells begin */
    uint32_t short_region_end;     /**< Bit offset where short region ends */
    uint32_t normal_region_start;  /**< Bit offset where normal resumes */
    
    float    long_ratio;           /**< Measured long region ratio (~1.10) */
    float    short_ratio;          /**< Measured short region ratio (~0.90) */
    
    uint16_t sector_length;        /**< Typical sector length */
    uint16_t baseline_timing_ns;   /**< Baseline bitcell timing in ns */
} uft_speedlock_params_t;

/**
 * @brief Speedlock detection result
 */
typedef struct {
    /* Detection status */
    bool                       detected;
    uft_speedlock_variant_t    variant;
    uft_speedlock_confidence_t confidence;
    
    /* Parameters */
    uft_speedlock_params_t     params;
    
    /* Region analysis */
    uint8_t                    region_count;
    uft_speedlock_region_t     regions[8];     /**< Up to 8 distinct regions */
    
    /* Statistics */
    float                      baseline_avg;   /**< Baseline average timing */
    float                      baseline_stddev; /**< Baseline standard deviation */
    uint32_t                   samples_analyzed;
    
    /* Track info */
    uint8_t                    track;
    uint8_t                    head;
    uint32_t                   track_bits;
    
    /* Diagnostics */
    char                       info[256];
} uft_speedlock_result_t;

/**
 * @brief Speedlock reconstruction parameters
 */
typedef struct {
    uft_speedlock_params_t params;
    uft_speedlock_variant_t variant;
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector_data[11][512]; /**< Sector data to encode */
} uft_speedlock_recon_params_t;

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

/**
 * @brief Detect Speedlock protection on track
 * 
 * Analyzes flux timing data for Speedlock protection:
 * - Measures baseline timing
 * - Scans for long bitcell region (+8% threshold)
 * - Scans for short bitcell region (-8% threshold)
 * - Verifies region sequence and positions
 * 
 * @param track_data Raw track data (for pattern verification)
 * @param track_bits Number of bits in track
 * @param timing_data Per-bit timing in nanoseconds (REQUIRED)
 * @param track Track number
 * @param head Head number
 * @param result Output: detection result
 * @return 0 on success, negative on error
 */
int uft_speedlock_detect(const uint8_t *track_data,
                         uint32_t track_bits,
                         const uint16_t *timing_data,
                         uint8_t track,
                         uint8_t head,
                         uft_speedlock_result_t *result);

/**
 * @brief Quick check for Speedlock timing variations
 * 
 * Fast screening without full analysis.
 * 
 * @param timing_data Per-bit timing data
 * @param count Number of timing samples
 * @return true if timing variations suggest Speedlock
 */
bool uft_speedlock_quick_check(const uint16_t *timing_data, uint32_t count);

/**
 * @brief Calculate baseline timing statistics
 * 
 * @param timing_data Timing data array
 * @param count Number of samples
 * @param avg Output: average timing
 * @param stddev Output: standard deviation
 */
void uft_speedlock_calc_baseline(const uint16_t *timing_data, uint32_t count,
                                  float *avg, float *stddev);

/*===========================================================================
 * Analysis Functions
 *===========================================================================*/

/**
 * @brief Find region transition point
 * 
 * Scans timing data for transition to specific region type.
 * 
 * @param timing_data Timing data
 * @param count Number of samples
 * @param baseline Baseline average timing
 * @param type Region type to find
 * @param start_pos Starting position for search
 * @return Bit position of transition, or -1 if not found
 */
int32_t uft_speedlock_find_region(const uint16_t *timing_data, uint32_t count,
                                   float baseline,
                                   uft_speedlock_region_type_t type,
                                   uint32_t start_pos);

/**
 * @brief Measure timing ratio in region
 * 
 * @param timing_data Timing data
 * @param start Start position
 * @param end End position
 * @param baseline Baseline timing
 * @return Ratio as percentage (e.g., 110 for 110%)
 */
float uft_speedlock_measure_ratio(const uint16_t *timing_data,
                                   uint32_t start, uint32_t end,
                                   float baseline);

/**
 * @brief Verify region sequence is correct
 * 
 * Speedlock has specific sequence: Normal → Long → Short → Normal
 * 
 * @param result Detection result with regions
 * @return true if sequence is valid
 */
bool uft_speedlock_verify_sequence(const uft_speedlock_result_t *result);

/*===========================================================================
 * Writing Functions
 *===========================================================================*/

/**
 * @brief Generate Speedlock timing data for writing
 * 
 * Creates timing array with correct variable-density regions.
 * 
 * @param params Speedlock parameters
 * @param track_bits Total track bits
 * @param timing_out Output timing array
 * @param timing_count Size of timing array
 * @return 0 on success
 */
int uft_speedlock_generate_timing(const uft_speedlock_params_t *params,
                                   uint32_t track_bits,
                                   uint16_t *timing_out,
                                   uint32_t timing_count);

/**
 * @brief Write Speedlock track with timing variations
 * 
 * Full reconstruction of Speedlock-protected track.
 * 
 * @param params Reconstruction parameters
 * @param output Output track data buffer
 * @param output_bits Output: actual bits written
 * @param timing_out Output timing data
 * @return 0 on success
 */
int uft_speedlock_write(const uft_speedlock_recon_params_t *params,
                         uint8_t *output,
                         uint32_t *output_bits,
                         uint16_t *timing_out);

/*===========================================================================
 * Reporting Functions
 *===========================================================================*/

/**
 * @brief Generate detailed Speedlock analysis report
 * 
 * @param result Detection result
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t uft_speedlock_report(const uft_speedlock_result_t *result,
                             char *buffer, size_t buffer_size);

/**
 * @brief Export Speedlock analysis to JSON
 * 
 * @param result Detection result
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t uft_speedlock_export_json(const uft_speedlock_result_t *result,
                                  char *buffer, size_t buffer_size);

/**
 * @brief Get variant name as string
 */
const char* uft_speedlock_variant_name(uft_speedlock_variant_t variant);

/**
 * @brief Get confidence name as string
 */
const char* uft_speedlock_confidence_name(uft_speedlock_confidence_t conf);

/**
 * @brief Get region type name as string
 */
const char* uft_speedlock_region_name(uft_speedlock_region_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SPEEDLOCK_H */
