// SPDX-License-Identifier: MIT
/*
 * weak_bits.h - Weak Bit Detection API
 * 
 * Detects weak/unstable bits in floppy disk tracks through
 * multi-revolution reading and variation analysis.
 * 
 * COPY PROTECTION: Many protection schemes (Rob Northen, Speedlock, etc.)
 * use intentionally weak bits that read differently on each revolution!
 * 
 * This is extracted from ADF-Copy-App behavior and enhanced for UFM.
 * 
 * @version 2.7.1
 * @date 2024-12-25
 */

#ifndef UFT_WEAK_BITS_H
#define UFT_WEAK_BITS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * WEAK BIT DETECTION
 *============================================================================*/

/**
 * @brief Weak bit detection parameters
 */
typedef struct {
    uint8_t revolution_count;       /**< Number of revolutions to compare (3-10) */
    uint8_t variation_threshold;    /**< Min % variation to flag as weak (20-50) */
    bool enable_byte_level;         /**< Also detect byte-level variations */
    bool enable_pattern_analysis;   /**< Analyze variation patterns */
} weak_bit_params_t;

/**
 * @brief Single weak bit location and characteristics
 */
typedef struct {
    uint32_t offset;                /**< Byte offset in track */
    uint8_t bit_position;           /**< Bit within byte (0-7) */
    uint8_t variation_percent;      /**< Percentage of variation (0-100) */
    uint8_t sample_count;           /**< Number of unique values seen */
    uint8_t samples[10];            /**< Actual bit values from each revolution */
    
    // Optional pattern info
    bool has_pattern;               /**< True if pattern detected */
    uint8_t pattern_type;           /**< 0=random, 1=alternating, 2=custom */
    
} weak_bit_t;

/**
 * @brief Weak bit detection results
 */
typedef struct {
    weak_bit_t *weak_bits;          /**< Array of detected weak bits */
    size_t weak_bit_count;          /**< Number of weak bits found */
    
    // Statistics
    uint32_t bytes_analyzed;        /**< Total bytes analyzed */
    uint32_t bits_analyzed;         /**< Total bits analyzed */
    float weak_bit_density;         /**< Weak bits per 1000 bits */
    
    // Byte-level variations (optional)
    uint32_t *weak_bytes;           /**< Offsets of varying bytes */
    size_t weak_byte_count;         /**< Number of varying bytes */
    
} weak_bit_result_t;

/*============================================================================*
 * CORE API
 *============================================================================*/

/**
 * @brief Initialize weak bit detection subsystem
 */
int weak_bits_init(void);

/**
 * @brief Shutdown weak bit detection subsystem
 */
void weak_bits_shutdown(void);

/**
 * @brief Detect weak bits by comparing multiple track reads
 * 
 * Takes multiple reads of the same track (from different revolutions)
 * and identifies bits that vary between reads. This is the CORE
 * algorithm for copy protection detection!
 * 
 * ALGORITHM:
 *   1. Compare each bit across all revolutions
 *   2. If a bit has different values, it's "weak"
 *   3. Calculate variation percentage
 *   4. Flag if exceeds threshold
 * 
 * SYNERGY WITH X-COPY:
 *   Weak bits complement X-Copy's long track detection!
 *   Many protections use BOTH techniques.
 * 
 * @param tracks Array of track data (multiple revolutions)
 * @param track_count Number of revolutions (3-10 recommended)
 * @param track_length Length of each track in bytes
 * @param params Detection parameters
 * @param result_out Detection results (allocated by this function)
 * @return 0 on success, negative on error
 */
int weak_bits_detect(
    const uint8_t **tracks,
    size_t track_count,
    size_t track_length,
    const weak_bit_params_t *params,
    weak_bit_result_t *result_out
);

/**
 * @brief Free weak bit detection results
 * 
 * @param result Results to free
 */
void weak_bits_free_result(weak_bit_result_t *result);

/*============================================================================*
 * UFM INTEGRATION
 *============================================================================*/

/**
 * @brief Add weak bit metadata to UFM track
 * 
 * Integrates detected weak bits into UFM track structure
 * and sets the UFM_CP_WEAKBITS flag.
 * 
 * @param track UFM track
 * @param weak_bits Detected weak bits
 * @param count Number of weak bits
 * @return 0 on success, negative on error
 */
int weak_bits_add_to_ufm_track(
    void *track,  // ufm_track_t pointer
    const weak_bit_t *weak_bits,
    size_t count
);

/**
 * @brief Get weak bits from UFM track
 * 
 * @param track UFM track
 * @param weak_bits_out Output array (caller must free)
 * @param count_out Number of weak bits
 * @return 0 on success, negative on error
 */
int weak_bits_get_from_ufm_track(
    const void *track,  // ufm_track_t pointer
    weak_bit_t **weak_bits_out,
    size_t *count_out
);

/*============================================================================*
 * X-COPY INTEGRATION
 *============================================================================*/

/**
 * @brief Check if weak bit detection would trigger X-Copy error
 * 
 * X-Copy Error Code 8 (Verify error) can be caused by weak bits.
 * This function determines if detected weak bits would cause
 * verification failures.
 * 
 * SYNERGY: X-Copy + Weak Bits = Complete protection analysis!
 * 
 * @param result Weak bit detection results
 * @param threshold Threshold for flagging (e.g., 10 weak bits = error)
 * @return True if would trigger X-Copy error 8
 */
bool weak_bits_triggers_xcopy_error(
    const weak_bit_result_t *result,
    uint32_t threshold
);

/**
 * @brief Generate X-Copy-style error message for weak bits
 * 
 * @param result Weak bit detection results
 * @param message_out Output buffer
 * @param message_size Size of output buffer
 */
void weak_bits_format_xcopy_message(
    const weak_bit_result_t *result,
    char *message_out,
    size_t message_size
);

/*============================================================================*
 * ANALYSIS HELPERS
 *============================================================================*/

/**
 * @brief Analyze weak bit patterns
 * 
 * Determines if weak bits follow a pattern:
 *   - Random (no pattern)
 *   - Alternating (0,1,0,1,...)
 *   - Custom pattern
 * 
 * @param weak_bit Weak bit to analyze
 * @return Pattern type (0=random, 1=alternating, 2=custom)
 */
uint8_t weak_bits_analyze_pattern(const weak_bit_t *weak_bit);

/**
 * @brief Get recommended detection parameters for format
 * 
 * Different disk formats may need different detection parameters.
 * 
 * @param format Format type (0=Amiga, 1=PC, 2=C64, etc.)
 * @param params_out Output parameters
 */
void weak_bits_get_default_params(
    uint8_t format,
    weak_bit_params_t *params_out
);

/**
 * @brief Calculate weak bit density
 * 
 * Returns weak bits per 1000 bits analyzed.
 * High density (>10) usually indicates copy protection.
 * 
 * @param result Detection results
 * @return Density (weak bits per 1000 bits)
 */
float weak_bits_calculate_density(const weak_bit_result_t *result);

/*============================================================================*
 * STATISTICS
 *============================================================================*/

/**
 * @brief Weak bit detection statistics
 */
typedef struct {
    uint64_t tracks_analyzed;       /**< Total tracks analyzed */
    uint64_t weak_bits_found;       /**< Total weak bits found */
    uint64_t protections_detected;  /**< Tracks with weak bit protection */
    float avg_density;              /**< Average weak bit density */
    uint64_t total_time_ms;         /**< Total analysis time */
} weak_bits_stats_t;

/**
 * @brief Get weak bit detection statistics
 */
void weak_bits_get_stats(weak_bits_stats_t *stats);

/**
 * @brief Reset statistics
 */
void weak_bits_reset_stats(void);

/*============================================================================*
 * UTILITIES
 *============================================================================*/

/**
 * @brief Print weak bit detection results (debug)
 * 
 * @param result Results to print
 */
void weak_bits_print_results(const weak_bit_result_t *result);

/**
 * @brief Export weak bits to JSON format
 * 
 * For archival purposes and external analysis.
 * 
 * @param result Results to export
 * @param json_out Output buffer
 * @param json_size Size of buffer
 * @return 0 on success, negative on error
 */
int weak_bits_export_json(
    const weak_bit_result_t *result,
    char *json_out,
    size_t json_size
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WEAK_BITS_H */
