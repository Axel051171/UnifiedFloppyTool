/**
 * @file uft_track_boundary.h
 * @brief Track boundary and overlap detection
 * 
 * Fixes algorithmic weakness #5: Track boundary detection
 * 
 * Features:
 * - Index pulse detection
 * - Pattern-based boundary recognition
 * - Overlap region handling
 * - Track trimming for seamless wrap
 */

#ifndef UFT_TRACK_BOUNDARY_H
#define UFT_TRACK_BOUNDARY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Standard rotation times */
#define UFT_ROTATION_300RPM_NS  200000000ULL  /* 200ms */
#define UFT_ROTATION_360RPM_NS  166666667ULL  /* 166.67ms */

/**
 * @brief Index pulse information
 */
typedef struct {
    size_t position;        /**< Bit position of index */
    uint64_t timestamp_ns;  /**< Timestamp if available */
    uint8_t confidence;     /**< Detection confidence 0-100 */
} uft_index_pulse_t;

/**
 * @brief Track boundary result
 */
typedef struct {
    size_t start_bit;       /**< First valid bit */
    size_t end_bit;         /**< Last valid bit + 1 */
    size_t track_length;    /**< end_bit - start_bit */
    
    /* Overlap handling */
    bool   has_overlap;
    size_t overlap_start;   /**< Where overlap region begins */
    size_t overlap_length;  /**< Length of overlap */
    
    /* Quality metrics */
    uint8_t boundary_confidence;  /**< 0-100 */
    bool    used_index_pulse;
    bool    used_pattern_match;
    double  match_score;    /**< Pattern match quality 0-1 */
    
    /* Index pulses found */
    uft_index_pulse_t indices[4];
    size_t index_count;
    
} uft_track_boundary_t;

/**
 * @brief Track boundary detector configuration
 */
typedef struct {
    /* Expected track parameters */
    double expected_rotation_ns;   /**< Expected rotation time (ns) */
    double sample_rate;            /**< Sampling rate (Hz) */
    double bit_rate;               /**< Data bit rate (Hz) */
    double tolerance;              /**< Tolerance for matching (0.1 = 10%) */
    
    /* Pattern matching settings */
    size_t match_window_bits;      /**< Bits to compare for pattern match */
    double min_match_score;        /**< Minimum match score to accept */
    
    /* Index pulse settings */
    bool   has_index_data;         /**< True if index pulse data available */
    
} uft_boundary_config_t;

/**
 * @brief Initialize boundary config with defaults
 */
void uft_boundary_config_init(uft_boundary_config_t *cfg);

/**
 * @brief Configure for specific format
 */
void uft_boundary_config_mfm_dd(uft_boundary_config_t *cfg);  /* 300 RPM, 500 Kbps */
void uft_boundary_config_mfm_hd(uft_boundary_config_t *cfg);  /* 300 RPM, 1 Mbps */
void uft_boundary_config_gcr_c64(uft_boundary_config_t *cfg); /* ~300 RPM, variable */

/**
 * @brief Find track boundaries using index pulses
 * @param bits Track bit data
 * @param bit_count Total bits in buffer
 * @param indices Array of index pulse positions
 * @param index_count Number of index pulses
 * @param cfg Configuration
 * @return Boundary result
 */
uft_track_boundary_t uft_boundary_from_indices(
    const uint8_t *bits,
    size_t bit_count,
    const uft_index_pulse_t *indices,
    size_t index_count,
    const uft_boundary_config_t *cfg);

/**
 * @brief Find track boundaries using pattern matching
 * @param bits Track bit data
 * @param bit_count Total bits in buffer
 * @param cfg Configuration
 * @return Boundary result
 */
uft_track_boundary_t uft_boundary_from_pattern(
    const uint8_t *bits,
    size_t bit_count,
    const uft_boundary_config_t *cfg);

/**
 * @brief Find track boundaries (auto-detect method)
 * @param bits Track bit data
 * @param bit_count Total bits in buffer
 * @param indices Optional index pulse array (can be NULL)
 * @param index_count Number of index pulses (0 if none)
 * @param cfg Configuration
 * @return Boundary result
 */
uft_track_boundary_t uft_boundary_detect(
    const uint8_t *bits,
    size_t bit_count,
    const uft_index_pulse_t *indices,
    size_t index_count,
    const uft_boundary_config_t *cfg);

/**
 * @brief Trim track to remove overlap
 * @param bits Track data (modified in place)
 * @param bit_count Current bit count
 * @param boundary Boundary information
 * @return New bit count after trimming
 */
size_t uft_boundary_trim(uint8_t *bits,
                         size_t bit_count,
                         const uft_track_boundary_t *boundary);

/**
 * @brief Find best splice point in overlap region
 * @param bits Track data
 * @param boundary Boundary information
 * @return Optimal splice position
 */
size_t uft_boundary_find_splice(const uint8_t *bits,
                                const uft_track_boundary_t *boundary);

/**
 * @brief Compare bit sequences for similarity
 * @param bits1 First sequence
 * @param pos1 Start position in bits1
 * @param bits2 Second sequence
 * @param pos2 Start position in bits2
 * @param len_bits Length to compare
 * @return Match score 0.0-1.0
 */
double uft_boundary_compare_bits(const uint8_t *bits1, size_t pos1,
                                 const uint8_t *bits2, size_t pos2,
                                 size_t len_bits);

/**
 * @brief Print boundary information
 */
void uft_boundary_dump(const uft_track_boundary_t *boundary);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_BOUNDARY_H */
