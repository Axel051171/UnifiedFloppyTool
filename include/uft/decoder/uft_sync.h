/**
 * @file uft_sync.h
 * @brief Sync pattern finder API
 */

#ifndef UFT_DECODER_SYNC_H
#define UFT_DECODER_SYNC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sync pattern definition
 */
typedef struct {
    uint64_t pattern;       /**< Pattern to match */
    int pattern_bits;       /**< Number of bits in pattern */
    int min_repeats;        /**< Minimum repeats required */
    int flags;              /**< Pattern flags */
} uft_sync_pattern_t;

/**
 * @brief Sync match result
 */
typedef struct {
    size_t position;        /**< Bit position of match */
    int hamming_distance;   /**< Hamming distance (0 = exact) */
    bool is_fuzzy;          /**< True if fuzzy match */
    double confidence;      /**< Match confidence (0.0-1.0) */
} uft_sync_match_t;

/* Predefined sync patterns */
extern const uft_sync_pattern_t UFT_SYNC_MFM_IDAM;
extern const uft_sync_pattern_t UFT_SYNC_MFM_DAM;
extern const uft_sync_pattern_t UFT_SYNC_GCR_HEADER;

/**
 * @brief Calculate Hamming distance between two values
 */
int uft_sync_hamming(uint64_t a, uint64_t b, int bits);

/**
 * @brief Find sync patterns (exact match)
 */
int uft_sync_find(const uint8_t* data, size_t bit_count, 
                  const uft_sync_pattern_t* pattern,
                  uft_sync_match_t* matches, int max_matches);

/**
 * @brief Find sync patterns (fuzzy match with Hamming tolerance)
 */
int uft_sync_find_fuzzy(const uint8_t* data, size_t bit_count, 
                        const uft_sync_pattern_t* pattern,
                        int max_hamming,
                        uft_sync_match_t* matches, int max_matches);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DECODER_SYNC_H */
