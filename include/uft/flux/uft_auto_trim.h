/**
 * @file uft_auto_trim.h
 * @brief Automatic Track Trimming for Seamless Loops
 * 
 * Provides automatic track data trimming to create seamless loops.
 * This is essential for:
 * - Copy protection reproduction (timing-sensitive data)
 * - Clean disk image conversion
 * - Removing overlapping read data
 * 
 * The algorithm finds the optimal cut point where the end of the track
 * data can be seamlessly stitched to the beginning, maintaining bit
 * pattern continuity.
 * 
 * @copyright MIT License
 */

#ifndef UFT_AUTO_TRIM_H
#define UFT_AUTO_TRIM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_TRIM_MIN_OVERLAP        1000    /**< Minimum overlap bits to search */
#define UFT_TRIM_MAX_OVERLAP        50000   /**< Maximum overlap bits to search */
#define UFT_TRIM_CORRELATION_WINDOW 64      /**< Correlation window size (bits) */
#define UFT_TRIM_MIN_CORRELATION    0.85    /**< Minimum correlation threshold */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Trim search result
 */
typedef struct uft_trim_result {
    bool        found;              /**< True if good trim point found */
    size_t      trim_position;      /**< Bit position to trim at */
    size_t      original_length;    /**< Original track length in bits */
    size_t      trimmed_length;     /**< Trimmed track length in bits */
    double      correlation;        /**< Correlation score at trim point */
    size_t      overlap_start;      /**< Start of overlap region */
    size_t      overlap_length;     /**< Length of overlap region */
} uft_trim_result_t;

/**
 * @brief Trim options
 */
typedef struct uft_trim_options {
    size_t      min_overlap;        /**< Minimum overlap to search (bits) */
    size_t      max_overlap;        /**< Maximum overlap to search (bits) */
    double      min_correlation;    /**< Minimum correlation threshold */
    size_t      window_size;        /**< Correlation window size */
    bool        use_index_pulse;    /**< Use index pulse if available */
    double      rpm;                /**< Disk RPM (300 or 360) */
    double      data_rate;          /**< Data rate in bits/sec */
} uft_trim_options_t;

/**
 * @brief Track data for trimming
 */
typedef struct uft_trim_track {
    uint8_t    *data;               /**< Bit stream data */
    size_t      bit_length;         /**< Track length in bits */
    uint32_t   *index_positions;    /**< Index pulse positions (optional) */
    size_t      index_count;        /**< Number of index pulses */
    double      sample_rate;        /**< Sample rate in Hz */
} uft_trim_track_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Core Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize trim options with defaults
 */
void uft_trim_options_init(uft_trim_options_t *opts);

/**
 * @brief Find optimal trim point for seamless loop
 * 
 * Searches for the best position to trim the track data so that
 * the end seamlessly connects to the beginning.
 * 
 * @param track Track data to analyze
 * @param opts Trim options
 * @param result Output result
 * @return 0 on success, error code on failure
 */
int uft_trim_find_optimal(const uft_trim_track_t *track,
                          const uft_trim_options_t *opts,
                          uft_trim_result_t *result);

/**
 * @brief Apply trim to track data
 * 
 * Creates a new trimmed track buffer. Caller must free.
 * 
 * @param track Original track data
 * @param result Trim result from uft_trim_find_optimal
 * @param out_data Output: trimmed data buffer
 * @param out_length Output: trimmed length in bits
 * @return 0 on success
 */
int uft_trim_apply(const uft_trim_track_t *track,
                   const uft_trim_result_t *result,
                   uint8_t **out_data, size_t *out_length);

/**
 * @brief Auto-trim track in place
 * 
 * Convenience function that finds and applies trim.
 * 
 * @param data Track data buffer (modified in place)
 * @param bit_length Pointer to track length (updated)
 * @param opts Options (or NULL for defaults)
 * @return 0 on success, -1 if no good trim found
 */
int uft_trim_auto(uint8_t *data, size_t *bit_length,
                  const uft_trim_options_t *opts);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Analysis Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate bit correlation between two positions
 * 
 * @param data Bit stream data
 * @param bit_length Total length in bits
 * @param pos1 First position
 * @param pos2 Second position
 * @param window Window size for comparison
 * @return Correlation coefficient (0.0 - 1.0)
 */
double uft_trim_correlate(const uint8_t *data, size_t bit_length,
                          size_t pos1, size_t pos2, size_t window);

/**
 * @brief Find index-to-index track length
 * 
 * Uses index pulses to determine exact track length.
 * 
 * @param track Track with index pulse data
 * @param revolution Which revolution (0 = first)
 * @return Track length in bits, 0 if not determinable
 */
size_t uft_trim_index_length(const uft_trim_track_t *track, int revolution);

/**
 * @brief Estimate track length from data rate and RPM
 * 
 * @param data_rate Data rate in bits/sec (e.g., 250000 for MFM DD)
 * @param rpm Disk RPM (300 or 360)
 * @param sample_rate Sample rate in Hz
 * @return Expected track length in samples
 */
size_t uft_trim_expected_length(double data_rate, double rpm, double sample_rate);

/**
 * @brief Detect overlap region in track
 * 
 * Finds where the track data starts repeating.
 * 
 * @param data Track data
 * @param bit_length Track length
 * @param expected Expected track length (hint)
 * @param overlap_start Output: overlap start position
 * @param overlap_length Output: overlap length
 * @return 0 if overlap found, -1 otherwise
 */
int uft_trim_detect_overlap(const uint8_t *data, size_t bit_length,
                            size_t expected,
                            size_t *overlap_start, size_t *overlap_length);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get bit at position
 */
static inline int uft_trim_get_bit(const uint8_t *data, size_t pos) {
    return (data[pos >> 3] >> (7 - (pos & 7))) & 1;
}

/**
 * @brief Set bit at position
 */
static inline void uft_trim_set_bit(uint8_t *data, size_t pos, int value) {
    size_t byte_pos = pos >> 3;
    int bit_pos = 7 - (pos & 7);
    if (value)
        data[byte_pos] |= (1 << bit_pos);
    else
        data[byte_pos] &= ~(1 << bit_pos);
}

/**
 * @brief Copy bits from source to destination
 */
void uft_trim_copy_bits(uint8_t *dst, size_t dst_pos,
                        const uint8_t *src, size_t src_pos,
                        size_t count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AUTO_TRIM_H */
