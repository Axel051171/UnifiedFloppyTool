/**
 * @file uft_sync_optimized.h
 * @brief Optimized sync pattern finder
 * 
 * P2-009: Sync finder O(n²) → O(n) optimization
 * 
 * Key improvements:
 * - Sliding window algorithm
 * - Precomputed pattern tables
 * - SIMD acceleration (where available)
 * - Multiple pattern search in single pass
 */

#ifndef UFT_SYNC_OPTIMIZED_H
#define UFT_SYNC_OPTIMIZED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum sync pattern length (bits) */
#define SYNC_MAX_PATTERN_BITS   64
#define SYNC_MAX_PATTERNS       16

/**
 * @brief Sync pattern definition
 */
typedef struct {
    uint64_t pattern;           /* Bit pattern */
    uint64_t mask;              /* Valid bits mask */
    uint8_t length;             /* Pattern length in bits */
    uint8_t tolerance;          /* Allowed bit errors */
    uint8_t id;                 /* Pattern ID */
} sync_pattern_t;

/**
 * @brief Sync match result
 */
typedef struct {
    size_t bit_position;        /* Position in bitstream */
    uint8_t pattern_id;         /* Which pattern matched */
    uint8_t errors;             /* Number of bit errors */
    uint8_t confidence;         /* Match confidence 0-100 */
} sync_match_t;

/**
 * @brief Sync finder context (for stateful searching)
 */
typedef struct {
    /* Patterns */
    sync_pattern_t patterns[SYNC_MAX_PATTERNS];
    uint8_t pattern_count;
    
    /* Sliding window */
    uint64_t window;            /* Current bit window */
    size_t window_valid;        /* Valid bits in window */
    
    /* State */
    size_t current_bit;         /* Current position */
    bool initialized;
    
    /* Statistics */
    size_t matches_found;
    size_t bytes_processed;
    
} sync_finder_ctx_t;

/**
 * @brief Multi-pattern search context
 */
typedef struct {
    /* Precomputed tables for fast matching */
    uint64_t pattern_table[SYNC_MAX_PATTERNS];
    uint64_t mask_table[SYNC_MAX_PATTERNS];
    uint8_t length_table[SYNC_MAX_PATTERNS];
    uint8_t pattern_count;
    
    /* Minimum pattern length (for window optimization) */
    uint8_t min_length;
    uint8_t max_length;
    
} sync_multi_ctx_t;

/* ============================================================================
 * Pattern Helpers
 * ============================================================================ */

/**
 * @brief Create sync pattern from bytes
 */
sync_pattern_t sync_pattern_create(const uint8_t *bytes, uint8_t bit_len);

/**
 * @brief Create common patterns
 */
sync_pattern_t sync_pattern_mfm(void);      /* 0xA1A1A1 with missing clock */
sync_pattern_t sync_pattern_fm(void);       /* 0xF57E or 0xF56A */
sync_pattern_t sync_pattern_gcr_c64(void);  /* 10 consecutive 1-bits */
sync_pattern_t sync_pattern_gcr_apple(void);/* 0xFF self-sync bytes */
sync_pattern_t sync_pattern_amiga(void);    /* 0x4489 */

/* ============================================================================
 * Single Pattern Search (O(n))
 * ============================================================================ */

/**
 * @brief Find all occurrences of pattern in bitstream
 * @param data Bitstream data
 * @param bit_count Number of bits
 * @param pattern Pattern to find
 * @param matches Output array
 * @param max_matches Maximum matches to return
 * @return Number of matches found
 * 
 * Algorithm: Sliding window with precomputed mask
 * Time complexity: O(n) where n = bit_count
 * Space complexity: O(1)
 */
int sync_find_pattern(const uint8_t *data, size_t bit_count,
                      const sync_pattern_t *pattern,
                      sync_match_t *matches, size_t max_matches);

/**
 * @brief Find first occurrence of pattern
 * @return Bit position or -1 if not found
 */
int64_t sync_find_first(const uint8_t *data, size_t bit_count,
                        const sync_pattern_t *pattern);

/**
 * @brief Find pattern with fuzzy matching
 * @param tolerance Maximum bit errors allowed
 */
int sync_find_fuzzy(const uint8_t *data, size_t bit_count,
                    const sync_pattern_t *pattern, uint8_t tolerance,
                    sync_match_t *matches, size_t max_matches);

/* ============================================================================
 * Multi-Pattern Search (O(n) for all patterns)
 * ============================================================================ */

/**
 * @brief Initialize multi-pattern context
 */
void sync_multi_init(sync_multi_ctx_t *ctx);

/**
 * @brief Add pattern to multi-search
 */
int sync_multi_add(sync_multi_ctx_t *ctx, const sync_pattern_t *pattern);

/**
 * @brief Search for all patterns in single pass
 * @return Total matches found
 * 
 * Algorithm: Parallel sliding window
 * Time complexity: O(n) regardless of pattern count
 */
int sync_multi_find(const sync_multi_ctx_t *ctx,
                    const uint8_t *data, size_t bit_count,
                    sync_match_t *matches, size_t max_matches);

/* ============================================================================
 * Streaming Search (for large bitstreams)
 * ============================================================================ */

/**
 * @brief Initialize streaming context
 */
void sync_stream_init(sync_finder_ctx_t *ctx);

/**
 * @brief Add pattern to streaming search
 */
int sync_stream_add_pattern(sync_finder_ctx_t *ctx, 
                            const sync_pattern_t *pattern);

/**
 * @brief Process chunk of data
 * @return Number of matches in this chunk
 */
int sync_stream_process(sync_finder_ctx_t *ctx,
                        const uint8_t *data, size_t byte_count,
                        sync_match_t *matches, size_t max_matches);

/**
 * @brief Reset stream state (keep patterns)
 */
void sync_stream_reset(sync_finder_ctx_t *ctx);

/* ============================================================================
 * SIMD Acceleration (x86/ARM)
 * ============================================================================ */

/**
 * @brief Check if SIMD is available
 */
bool sync_simd_available(void);

/**
 * @brief Find pattern using SIMD
 * Falls back to scalar if unavailable
 */
int sync_find_pattern_simd(const uint8_t *data, size_t bit_count,
                           const sync_pattern_t *pattern,
                           sync_match_t *matches, size_t max_matches);

/* ============================================================================
 * Bit Manipulation Helpers
 * ============================================================================ */

/**
 * @brief Get bit at position
 */
static inline int sync_get_bit(const uint8_t *data, size_t bit_pos) {
    return (data[bit_pos >> 3] >> (7 - (bit_pos & 7))) & 1;
}

/**
 * @brief Count differing bits (Hamming distance)
 */
static inline int sync_hamming(uint64_t a, uint64_t b) {
    uint64_t diff = a ^ b;
    int count = 0;
    while (diff) {
        count++;
        diff &= diff - 1;
    }
    return count;
}

/**
 * @brief Count leading zeros
 */
static inline int sync_clz64(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return x ? __builtin_clzll(x) : 64;
#else
    int n = 0;
    if (x == 0) return 64;
    if ((x & 0xFFFFFFFF00000000ULL) == 0) { n += 32; x <<= 32; }
    if ((x & 0xFFFF000000000000ULL) == 0) { n += 16; x <<= 16; }
    if ((x & 0xFF00000000000000ULL) == 0) { n += 8; x <<= 8; }
    if ((x & 0xF000000000000000ULL) == 0) { n += 4; x <<= 4; }
    if ((x & 0xC000000000000000ULL) == 0) { n += 2; x <<= 2; }
    if ((x & 0x8000000000000000ULL) == 0) { n += 1; }
    return n;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_SYNC_OPTIMIZED_H */
