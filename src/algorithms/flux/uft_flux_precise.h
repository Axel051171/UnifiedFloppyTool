/**
 * @file uft_flux_precise.h
 * @brief High-precision flux timing with sub-sample accuracy
 * 
 * Fixes algorithmic weakness #10: Flux timing quantization errors
 * 
 * Features:
 * - Sub-sample precision (fractional timing)
 * - Error accumulation compensation
 * - Interpolation between samples
 * - Drift correction
 */

#ifndef UFT_FLUX_PRECISE_H
#define UFT_FLUX_PRECISE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief High-precision flux sample
 */
typedef struct {
    uint64_t timestamp_ns;   /**< Integer nanoseconds */
    double   fractional;     /**< Sub-nanosecond fraction [0.0, 1.0) */
    uint8_t  flags;          /**< Sample flags */
} uft_flux_sample_t;

/* Sample flags */
#define UFT_FLUX_FLAG_INDEX     0x01  /**< Index pulse marker */
#define UFT_FLUX_FLAG_WEAK      0x02  /**< Weak/uncertain transition */
#define UFT_FLUX_FLAG_SYNTHETIC 0x04  /**< Interpolated/synthetic */

/**
 * @brief Flux buffer with precision timing
 */
typedef struct {
    uft_flux_sample_t *samples;
    size_t count;
    size_t capacity;
    
    /* Timing info */
    double sample_rate;       /**< Hz */
    double ns_per_sample;     /**< Nanoseconds per sample */
    
    /* Track info */
    uint64_t rotation_ns;     /**< One rotation in ns */
    size_t   index_count;     /**< Number of index pulses */
    
} uft_flux_buffer_t;

/**
 * @brief Flux-to-bit converter state
 */
typedef struct {
    /* Cell timing */
    double cell_time_ns;      /**< Nominal cell time */
    double cell_tolerance;    /**< Tolerance factor (e.g., 0.4 for ±40%) */
    
    /* Error tracking */
    double accumulated_error; /**< Running error accumulation */
    double max_error;         /**< Maximum seen error */
    double error_threshold;   /**< Threshold for correction */
    
    /* Drift compensation */
    double drift_rate;        /**< Estimated clock drift */
    double drift_compensation;/**< Applied compensation */
    
    /* Statistics */
    size_t pulses_processed;
    size_t bits_generated;
    size_t corrections_applied;
    
} uft_flux_converter_t;

/* ============================================================================
 * Buffer Management
 * ============================================================================ */

/**
 * @brief Initialize flux buffer
 * @param buf Buffer to initialize
 * @param capacity Initial capacity
 * @param sample_rate Sample rate in Hz
 * @return 0 on success
 */
int uft_flux_buffer_init(uft_flux_buffer_t *buf,
                         size_t capacity,
                         double sample_rate);

/**
 * @brief Free flux buffer
 */
void uft_flux_buffer_free(uft_flux_buffer_t *buf);

/**
 * @brief Clear buffer (keep allocation)
 */
void uft_flux_buffer_clear(uft_flux_buffer_t *buf);

/**
 * @brief Add sample to buffer
 */
int uft_flux_buffer_add(uft_flux_buffer_t *buf,
                        uint64_t timestamp_ns,
                        double fractional,
                        uint8_t flags);

/**
 * @brief Add sample from raw sample count
 */
int uft_flux_buffer_add_sample(uft_flux_buffer_t *buf,
                               double sample_position,
                               uint8_t flags);

/* ============================================================================
 * Precision Timing
 * ============================================================================ */

/**
 * @brief Get precise time at position
 * @param buf Flux buffer
 * @param index Sample index (can be fractional)
 * @return Time in nanoseconds
 */
double uft_flux_get_time(const uft_flux_buffer_t *buf, double index);

/**
 * @brief Interpolate between samples
 * @param buf Flux buffer
 * @param time_ns Time in nanoseconds
 * @return Interpolated sample index
 */
double uft_flux_interpolate_position(const uft_flux_buffer_t *buf,
                                     double time_ns);

/**
 * @brief Calculate precise delta between pulses
 */
double uft_flux_delta(const uft_flux_sample_t *a,
                      const uft_flux_sample_t *b);

/* ============================================================================
 * Flux-to-Bit Conversion
 * ============================================================================ */

/**
 * @brief Initialize converter
 * @param conv Converter state
 * @param cell_time_ns Cell time in nanoseconds
 * @param tolerance Tolerance (0.4 = ±40%)
 */
void uft_flux_converter_init(uft_flux_converter_t *conv,
                             double cell_time_ns,
                             double tolerance);

/**
 * @brief Reset converter state
 */
void uft_flux_converter_reset(uft_flux_converter_t *conv);

/**
 * @brief Convert flux buffer to bits with precision
 * @param conv Converter state
 * @param flux Input flux buffer
 * @param out_bits Output bit buffer
 * @param out_capacity Output capacity in bytes
 * @param out_residual Output: remaining error
 * @return Number of bits generated
 */
size_t uft_flux_to_bits(uft_flux_converter_t *conv,
                        const uft_flux_buffer_t *flux,
                        uint8_t *out_bits,
                        size_t out_capacity,
                        double *out_residual);

/**
 * @brief Convert single pulse delta to bits
 * @param conv Converter state
 * @param delta_ns Pulse delta in nanoseconds
 * @param out_bits Output bits (MSB first)
 * @return Number of bits (0-8 typically)
 */
int uft_flux_delta_to_bits(uft_flux_converter_t *conv,
                           double delta_ns,
                           uint8_t *out_bits);

/* ============================================================================
 * Drift Analysis
 * ============================================================================ */

/**
 * @brief Estimate clock drift from flux data
 * @param flux Flux buffer
 * @param expected_rotation_ns Expected rotation time
 * @return Drift rate (>1.0 = faster, <1.0 = slower)
 */
double uft_flux_estimate_drift(const uft_flux_buffer_t *flux,
                               double expected_rotation_ns);

/**
 * @brief Apply drift compensation to buffer
 * @param flux Buffer to compensate (modified)
 * @param drift_rate Drift rate to compensate for
 */
void uft_flux_compensate_drift(uft_flux_buffer_t *flux,
                               double drift_rate);

/**
 * @brief Analyze timing consistency
 * @param flux Flux buffer
 * @param cell_time_ns Expected cell time
 * @param out_variance Output: timing variance
 * @return Mean timing error
 */
double uft_flux_analyze_timing(const uft_flux_buffer_t *flux,
                               double cell_time_ns,
                               double *out_variance);

/* ============================================================================
 * Debug
 * ============================================================================ */

/**
 * @brief Print buffer statistics
 */
void uft_flux_buffer_dump(const uft_flux_buffer_t *buf);

/**
 * @brief Print converter statistics
 */
void uft_flux_converter_dump(const uft_flux_converter_t *conv);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_PRECISE_H */
