/**
 * @file uft_weak_bits.h
 * @brief Weak bit detection and handling
 * 
 * Fixes algorithmic weakness #4: Weak bit handling
 * 
 * Weak bits are magnetic domains with undefined magnetization that
 * produce different values on each read. They are used in copy protection
 * and must be preserved for accurate disk forensics.
 * 
 * Features:
 * - Multi-revision fusion
 * - Probabilistic bit representation
 * - Confidence tracking
 * - Weak region detection
 */

#ifndef UFT_WEAK_BITS_H
#define UFT_WEAK_BITS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum revisions to track */
#define UFT_WEAK_MAX_REVISIONS 8

/* Confidence thresholds */
#define UFT_CONF_WEAK_THRESHOLD    128  /* Below this = weak */
#define UFT_CONF_STRONG_THRESHOLD  200  /* Above this = strong */

/**
 * @brief Probabilistic bit with confidence
 */
typedef struct {
    uint8_t value;       /**< Bit value (0 or 1) */
    uint8_t confidence;  /**< Confidence 0-255 */
    bool    is_weak;     /**< True if detected as weak bit */
} uft_prob_bit_t;

/**
 * @brief Bit sample from one revision
 */
typedef struct {
    uint8_t value;
    uint8_t confidence;
    double  timing_error;  /**< Phase error at this bit */
} uft_bit_sample_t;

/**
 * @brief Multi-revision bit fusion state
 */
typedef struct {
    uft_bit_sample_t samples[UFT_WEAK_MAX_REVISIONS];
    size_t sample_count;
} uft_bit_fusion_t;

/**
 * @brief Weak bit region
 */
typedef struct {
    size_t start_bit;
    size_t end_bit;
    size_t length;
    uint8_t min_confidence;
    uint8_t avg_confidence;
    size_t weak_count;      /**< Number of weak bits in region */
} uft_weak_region_t;

/**
 * @brief Track with weak bit information
 */
typedef struct {
    size_t bit_count;
    
    /* Per-bit data */
    uint8_t *bits;           /**< Bit values */
    uint8_t *confidence;     /**< Per-bit confidence */
    bool    *weak_flags;     /**< Per-bit weak flag */
    
    /* Detected regions */
    uft_weak_region_t *regions;
    size_t region_count;
    size_t region_capacity;
    
    /* Statistics */
    size_t total_weak_bits;
    size_t total_strong_bits;
    double weak_ratio;
    
} uft_weak_track_t;

/* ============================================================================
 * Single Bit Operations
 * ============================================================================ */

/**
 * @brief Create probabilistic bit from timing
 * @param pulse_in_window True if pulse detected in data window
 * @param distance_to_center Distance from cell center (0 = perfect)
 * @param window_size Size of data window
 * @return Probabilistic bit
 */
uft_prob_bit_t uft_prob_bit_from_timing(bool pulse_in_window,
                                        double distance_to_center,
                                        double window_size);

/**
 * @brief Add sample to fusion
 */
void uft_fusion_add_sample(uft_bit_fusion_t *fusion,
                           uint8_t value,
                           uint8_t confidence);

/**
 * @brief Fuse samples into final bit
 */
uft_prob_bit_t uft_fusion_fuse(const uft_bit_fusion_t *fusion);

/**
 * @brief Clear fusion state
 */
void uft_fusion_clear(uft_bit_fusion_t *fusion);

/* ============================================================================
 * Track Operations
 * ============================================================================ */

/**
 * @brief Initialize weak track structure
 * @param track Track structure to initialize
 * @param bit_count Number of bits
 * @return 0 on success, negative on error
 */
int uft_weak_track_init(uft_weak_track_t *track, size_t bit_count);

/**
 * @brief Free weak track resources
 */
void uft_weak_track_free(uft_weak_track_t *track);

/**
 * @brief Set bit with confidence
 */
void uft_weak_track_set_bit(uft_weak_track_t *track,
                            size_t index,
                            uint8_t value,
                            uint8_t confidence);

/**
 * @brief Set bit from probabilistic bit
 */
void uft_weak_track_set_prob_bit(uft_weak_track_t *track,
                                 size_t index,
                                 const uft_prob_bit_t *bit);

/**
 * @brief Detect weak regions
 * @param track Track to analyze
 * @param min_region_size Minimum bits for a region
 * @return Number of regions detected
 */
size_t uft_weak_track_detect_regions(uft_weak_track_t *track,
                                     size_t min_region_size);

/**
 * @brief Merge multiple track revisions
 * @param out Output track (must be initialized)
 * @param tracks Array of input tracks
 * @param track_count Number of tracks to merge
 * @return 0 on success
 */
int uft_weak_track_merge(uft_weak_track_t *out,
                         const uft_weak_track_t **tracks,
                         size_t track_count);

/**
 * @brief Compare two tracks for weak bit differences
 * @param a First track
 * @param b Second track
 * @param out_diff_positions Output: positions where values differ
 * @param diff_capacity Capacity of output array
 * @return Number of differences
 */
size_t uft_weak_track_compare(const uft_weak_track_t *a,
                              const uft_weak_track_t *b,
                              size_t *out_diff_positions,
                              size_t diff_capacity);

/**
 * @brief Generate weak bit mask (for copy protection)
 * @param track Track to analyze
 * @param out_mask Output: 1 = weak, 0 = strong
 * @param mask_size Size in bytes
 * @return Number of weak bits
 */
size_t uft_weak_track_get_mask(const uft_weak_track_t *track,
                               uint8_t *out_mask,
                               size_t mask_size);

/**
 * @brief Print track weak bit summary
 */
void uft_weak_track_dump(const uft_weak_track_t *track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WEAK_BITS_H */
