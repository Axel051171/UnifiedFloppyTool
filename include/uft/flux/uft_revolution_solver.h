/**
 * @file uft_revolution_solver.h
 * @brief Multi-Revolution Alignment and Analysis
 * 
 * Based on DTC learnings - implements revolution solving for
 * aligning multiple disk rotations from flux captures.
 * 
 * CLEAN-ROOM implementation based on observable requirements.
 */

#ifndef UFT_REVOLUTION_SOLVER_H
#define UFT_REVOLUTION_SOLVER_H

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
#ifndef UFT_ERR_NO_DATA
#define UFT_ERR_NO_DATA UFT_ERR_IO
#endif
#ifndef UFT_ERR_NO_INDEX
#define UFT_ERR_NO_INDEX (-100)
#endif
#ifndef UFT_ERR_INSUFFICIENT_DATA
#define UFT_ERR_INSUFFICIENT_DATA (-101)
#endif
#ifndef UFT_ERR_OUT_OF_RANGE
#define UFT_ERR_OUT_OF_RANGE (-102)
#endif
#ifndef UFT_ERR_NOT_FOUND
#define UFT_ERR_NOT_FOUND UFT_ERR_FILE_NOT_FOUND
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

#define UFT_REV_MAX_REVOLUTIONS     16
#define UFT_REV_MIN_REVOLUTIONS     2
#define UFT_REV_DEFAULT_TOLERANCE   0.05  /* 5% timing tolerance */
#define UFT_REV_NOMINAL_RPM_300     300.0
#define UFT_REV_NOMINAL_RPM_360     360.0

/* ============================================================================
 * Data Types
 * ============================================================================ */

/**
 * @brief Information about a single revolution
 */
typedef struct {
    uint32_t revolution;          /**< Revolution number (0-based) */
    uint64_t index_position;      /**< Index pulse position in samples */
    uint64_t start_sample;        /**< Start of revolution data */
    uint64_t end_sample;          /**< End of revolution data */
    uint64_t sample_count;        /**< Total samples in revolution */
    double   duration_us;         /**< Duration in microseconds */
    double   rpm;                 /**< Calculated RPM for this revolution */
    double   drift_us;            /**< Drift from nominal in microseconds */
    uint8_t  quality;             /**< Quality score 0-100 */
    bool     index_valid;         /**< Index pulse was found */
} uft_revolution_info_t;

/**
 * @brief Result of revolution solving
 */
typedef struct {
    uft_revolution_info_t revolutions[UFT_REV_MAX_REVOLUTIONS];
    size_t count;                 /**< Number of revolutions found */
    
    /* Statistics */
    double average_rpm;           /**< Average RPM across all revolutions */
    double rpm_variance;          /**< RPM variance */
    double rpm_min;               /**< Minimum RPM observed */
    double rpm_max;               /**< Maximum RPM observed */
    
    double average_duration_us;   /**< Average revolution duration */
    double duration_variance;     /**< Duration variance */
    
    /* Quality indicators */
    bool   index_consistent;      /**< All index pulses consistent */
    bool   timing_stable;         /**< Timing variation within tolerance */
    uint8_t overall_quality;      /**< Overall quality 0-100 */
    
    /* Best revolution for single-pass decode */
    uint32_t best_revolution;     /**< Index of highest quality revolution */
} uft_revolution_result_t;

/**
 * @brief Options for revolution solving
 */
typedef struct {
    double nominal_rpm;           /**< Expected RPM (300 or 360) */
    double sample_rate_hz;        /**< Sample rate in Hz */
    double tolerance;             /**< Timing tolerance (0.0-1.0) */
    bool   use_index_pulse;       /**< Use index pulse for alignment */
    bool   allow_missing_index;   /**< Continue if some index pulses missing */
    uint32_t min_revolutions;     /**< Minimum revolutions required */
    uint32_t max_revolutions;     /**< Maximum revolutions to process */
} uft_revolution_options_t;

/**
 * @brief Index pulse information
 */
typedef struct {
    uint64_t *positions;          /**< Array of index positions */
    size_t count;                 /**< Number of index pulses */
} uft_index_data_t;

/**
 * @brief Merged revolution output
 */
typedef struct {
    uint8_t *data;                /**< Merged bit data */
    size_t bit_count;             /**< Number of bits */
    uint8_t *confidence;          /**< Per-bit confidence (0-100) */
    uint8_t *weak_bits;           /**< Weak bit mask */
    size_t weak_count;            /**< Number of weak bits detected */
} uft_merged_revolution_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Initialize revolution options with defaults
 */
void uft_revolution_options_init(uft_revolution_options_t *options);

/**
 * @brief Solve revolution boundaries from flux data
 * 
 * Analyzes flux data to identify individual disk rotations,
 * using index pulses for precise alignment.
 * 
 * @param flux_samples   Raw flux timing samples
 * @param sample_count   Number of samples
 * @param index_data     Index pulse positions (optional, can be NULL)
 * @param options        Solver options
 * @param result         Output revolution information
 * @return UFT_OK on success
 */
uft_error_t uft_revolution_solve(
    const uint32_t *flux_samples,
    size_t sample_count,
    const uft_index_data_t *index_data,
    const uft_revolution_options_t *options,
    uft_revolution_result_t *result
);

/**
 * @brief Extract a single revolution from flux data
 * 
 * @param flux_samples   Raw flux timing samples
 * @param sample_count   Total sample count
 * @param revs           Revolution information from solver
 * @param revolution_idx Which revolution to extract (0-based)
 * @param output         Output buffer for revolution data
 * @param output_size    Size of output buffer
 * @param actual_size    Actual bytes written
 * @return UFT_OK on success
 */
uft_error_t uft_revolution_extract(
    const uint32_t *flux_samples,
    size_t sample_count,
    const uft_revolution_result_t *revs,
    uint32_t revolution_idx,
    uint32_t *output,
    size_t output_size,
    size_t *actual_size
);

/**
 * @brief Merge multiple revolutions with confidence weighting
 * 
 * Combines data from multiple revolutions, using voting and
 * confidence weighting to produce optimal output.
 * 
 * @param decoded_revs   Array of decoded revolution data
 * @param rev_count      Number of revolutions
 * @param bit_count      Bits per revolution
 * @param merged         Output merged data with confidence
 * @return UFT_OK on success
 */
uft_error_t uft_revolution_merge(
    const uint8_t **decoded_revs,
    size_t rev_count,
    size_t bit_count,
    uft_merged_revolution_t *merged
);

/**
 * @brief Detect weak bits by comparing revolutions
 * 
 * Identifies bits that vary between revolutions, indicating
 * weak or unstable magnetic regions.
 * 
 * @param decoded_revs   Array of decoded revolution data
 * @param rev_count      Number of revolutions
 * @param bit_count      Bits per revolution
 * @param weak_mask      Output weak bit mask
 * @param weak_count     Number of weak bits found
 * @return UFT_OK on success
 */
uft_error_t uft_revolution_detect_weak(
    const uint8_t **decoded_revs,
    size_t rev_count,
    size_t bit_count,
    uint8_t *weak_mask,
    size_t *weak_count
);

/**
 * @brief Calculate statistics for revolution set
 * 
 * @param result         Revolution result to analyze
 * @param nominal_rpm    Expected RPM for comparison
 */
void uft_revolution_calc_stats(
    uft_revolution_result_t *result,
    double nominal_rpm
);

/**
 * @brief Find best revolution based on quality metrics
 * 
 * @param result         Revolution result
 * @return Index of best revolution
 */
uint32_t uft_revolution_find_best(
    const uft_revolution_result_t *result
);

/**
 * @brief Free merged revolution data
 */
void uft_merged_revolution_free(uft_merged_revolution_t *merged);

/**
 * @brief Convert revolution result to JSON
 * 
 * @param result         Revolution result
 * @param buffer         Output buffer
 * @param size           Buffer size
 * @return Bytes written, or required size if buffer too small
 */
size_t uft_revolution_to_json(
    const uft_revolution_result_t *result,
    char *buffer,
    size_t size
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Calculate RPM from revolution duration
 * 
 * @param duration_us    Duration in microseconds
 * @return RPM value
 */
static inline double uft_duration_to_rpm(double duration_us) {
    if (duration_us <= 0) return 0;
    return 60000000.0 / duration_us;
}

/**
 * @brief Calculate expected duration from RPM
 * 
 * @param rpm            RPM value
 * @return Duration in microseconds
 */
static inline double uft_rpm_to_duration(double rpm) {
    if (rpm <= 0) return 0;
    return 60000000.0 / rpm;
}

/**
 * @brief Check if RPM is within tolerance
 * 
 * @param actual_rpm     Measured RPM
 * @param nominal_rpm    Expected RPM
 * @param tolerance      Tolerance (0.0-1.0, e.g., 0.05 for 5%)
 * @return true if within tolerance
 */
static inline bool uft_rpm_in_tolerance(
    double actual_rpm, 
    double nominal_rpm, 
    double tolerance
) {
    double min_rpm = nominal_rpm * (1.0 - tolerance);
    double max_rpm = nominal_rpm * (1.0 + tolerance);
    return (actual_rpm >= min_rpm && actual_rpm <= max_rpm);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_REVOLUTION_SOLVER_H */
