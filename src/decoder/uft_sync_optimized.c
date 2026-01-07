/**
 * @file uft_sync_optimized.c
 * @brief Optimized sync pattern finder implementation
 * 
 * P2-009: O(n) sync finding algorithm
 * 
 * Key optimizations:
 * 1. Sliding window - no nested loops
 * 2. Precomputed masks - instant pattern comparison
 * 3. Multi-pattern search in single pass
 * 4. SIMD where available
 */

#include "uft/decoder/uft_sync_optimized.h"
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Pattern Creation
 * ============================================================================ */

sync_pattern_t sync_pattern_create(const uint8_t *bytes, uint8_t bit_len) {
    sync_pattern_t p = {0};
    p.length = bit_len;
    p.tolerance = 0;
    
    /* Build pattern and mask */
    for (int i = 0; i < bit_len; i++) {
        int byte_idx = i / 8;
        int bit_idx = 7 - (i % 8);
        int bit = (bytes[byte_idx] >> bit_idx) & 1;
        
        p.pattern = (p.pattern << 1) | bit;
        p.mask = (p.mask << 1) | 1;
    }
    
    return p;
}

sync_pattern_t sync_pattern_mfm(void) {
    /* MFM sync: A1 A1 A1 with missing clock bits */
    /* Pattern: 0x4489 4489 4489 (48 bits) */
    sync_pattern_t p = {0};
    p.pattern = 0x448944894489ULL;
    p.mask = 0xFFFFFFFFFFFFULL;
    p.length = 48;
    p.tolerance = 0;
    p.id = 1;
    return p;
}

sync_pattern_t sync_pattern_fm(void) {
    /* FM sync: F5 7E (data mark) */
    sync_pattern_t p = {0};
    p.pattern = 0xF57E;
    p.mask = 0xFFFF;
    p.length = 16;
    p.tolerance = 0;
    p.id = 2;
    return p;
}

sync_pattern_t sync_pattern_gcr_c64(void) {
    /* C64 GCR sync: 10 consecutive 1-bits */
    sync_pattern_t p = {0};
    p.pattern = 0x3FF;  /* 10 ones */
    p.mask = 0x3FF;
    p.length = 10;
    p.tolerance = 0;
    p.id = 3;
    return p;
}

sync_pattern_t sync_pattern_gcr_apple(void) {
    /* Apple self-sync: FF D5 AA */
    sync_pattern_t p = {0};
    p.pattern = 0xFFD5AA;
    p.mask = 0xFFFFFF;
    p.length = 24;
    p.tolerance = 0;
    p.id = 4;
    return p;
}

sync_pattern_t sync_pattern_amiga(void) {
    /* Amiga MFM sync: 0x4489 */
    sync_pattern_t p = {0};
    p.pattern = 0x4489;
    p.mask = 0xFFFF;
    p.length = 16;
    p.tolerance = 0;
    p.id = 5;
    return p;
}

/* ============================================================================
 * Single Pattern Search - O(n)
 * ============================================================================ */

int sync_find_pattern(const uint8_t *data, size_t bit_count,
                      const sync_pattern_t *pattern,
                      sync_match_t *matches, size_t max_matches) {
    if (!data || !pattern || !matches || max_matches == 0) {
        return 0;
    }
    
    if (pattern->length == 0 || pattern->length > 64) {
        return 0;
    }
    
    size_t match_count = 0;
    uint64_t window = 0;
    uint64_t mask = pattern->mask;
    uint64_t target = pattern->pattern;
    uint8_t pat_len = pattern->length;
    
    /*
     * Sliding window algorithm:
     * - Maintain a window of the last N bits
     * - Shift in one bit at a time
     * - Compare against pattern using precomputed mask
     * - O(n) time, O(1) space
     */
    
    size_t byte_count = (bit_count + 7) / 8;
    
    for (size_t byte_idx = 0; byte_idx < byte_count; byte_idx++) {
        uint8_t byte = data[byte_idx];
        
        /* Process each bit in the byte */
        for (int bit = 7; bit >= 0; bit--) {
            size_t current_bit = byte_idx * 8 + (7 - bit);
            if (current_bit >= bit_count) break;
            
            /* Shift window and add new bit */
            window = ((window << 1) | ((byte >> bit) & 1)) & mask;
            
            /* Check for match once we have enough bits */
            if (current_bit >= pat_len - 1) {
                if (window == target) {
                    if (match_count < max_matches) {
                        matches[match_count].bit_position = 
                            current_bit - pat_len + 1;
                        matches[match_count].pattern_id = pattern->id;
                        matches[match_count].errors = 0;
                        matches[match_count].confidence = 100;
                    }
                    match_count++;
                }
            }
        }
    }
    
    return (match_count > max_matches) ? max_matches : match_count;
}

int64_t sync_find_first(const uint8_t *data, size_t bit_count,
                        const sync_pattern_t *pattern) {
    sync_match_t match;
    
    if (sync_find_pattern(data, bit_count, pattern, &match, 1) > 0) {
        return (int64_t)match.bit_position;
    }
    
    return -1;
}

int sync_find_fuzzy(const uint8_t *data, size_t bit_count,
                    const sync_pattern_t *pattern, uint8_t tolerance,
                    sync_match_t *matches, size_t max_matches) {
    if (!data || !pattern || !matches || max_matches == 0) {
        return 0;
    }
    
    if (pattern->length == 0 || pattern->length > 64) {
        return 0;
    }
    
    size_t match_count = 0;
    uint64_t window = 0;
    uint64_t mask = pattern->mask;
    uint64_t target = pattern->pattern;
    uint8_t pat_len = pattern->length;
    
    size_t byte_count = (bit_count + 7) / 8;
    
    for (size_t byte_idx = 0; byte_idx < byte_count; byte_idx++) {
        uint8_t byte = data[byte_idx];
        
        for (int bit = 7; bit >= 0; bit--) {
            size_t current_bit = byte_idx * 8 + (7 - bit);
            if (current_bit >= bit_count) break;
            
            window = ((window << 1) | ((byte >> bit) & 1)) & mask;
            
            if (current_bit >= pat_len - 1) {
                /* Calculate Hamming distance */
                int errors = sync_hamming(window, target);
                
                if (errors <= tolerance) {
                    if (match_count < max_matches) {
                        matches[match_count].bit_position = 
                            current_bit - pat_len + 1;
                        matches[match_count].pattern_id = pattern->id;
                        matches[match_count].errors = errors;
                        matches[match_count].confidence = 
                            100 - (errors * 100 / pat_len);
                    }
                    match_count++;
                }
            }
        }
    }
    
    return (match_count > max_matches) ? max_matches : match_count;
}

/* ============================================================================
 * Multi-Pattern Search - O(n) for all patterns
 * ============================================================================ */

void sync_multi_init(sync_multi_ctx_t *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->min_length = 255;
}

int sync_multi_add(sync_multi_ctx_t *ctx, const sync_pattern_t *pattern) {
    if (!ctx || !pattern) return -1;
    if (ctx->pattern_count >= SYNC_MAX_PATTERNS) return -1;
    
    int idx = ctx->pattern_count++;
    
    ctx->pattern_table[idx] = pattern->pattern;
    ctx->mask_table[idx] = pattern->mask;
    ctx->length_table[idx] = pattern->length;
    
    if (pattern->length < ctx->min_length) {
        ctx->min_length = pattern->length;
    }
    if (pattern->length > ctx->max_length) {
        ctx->max_length = pattern->length;
    }
    
    return idx;
}

int sync_multi_find(const sync_multi_ctx_t *ctx,
                    const uint8_t *data, size_t bit_count,
                    sync_match_t *matches, size_t max_matches) {
    if (!ctx || !data || !matches || max_matches == 0) {
        return 0;
    }
    if (ctx->pattern_count == 0) {
        return 0;
    }
    
    size_t match_count = 0;
    uint64_t window = 0;
    
    /* Use maximum length mask for window */
    uint64_t window_mask = (ctx->max_length < 64) ? 
                           ((1ULL << ctx->max_length) - 1) : ~0ULL;
    
    size_t byte_count = (bit_count + 7) / 8;
    
    for (size_t byte_idx = 0; byte_idx < byte_count; byte_idx++) {
        uint8_t byte = data[byte_idx];
        
        for (int bit = 7; bit >= 0; bit--) {
            size_t current_bit = byte_idx * 8 + (7 - bit);
            if (current_bit >= bit_count) break;
            
            /* Shift window */
            window = ((window << 1) | ((byte >> bit) & 1)) & window_mask;
            
            /* Check all patterns */
            if (current_bit >= ctx->min_length - 1) {
                for (int p = 0; p < ctx->pattern_count; p++) {
                    if (current_bit < ctx->length_table[p] - 1) continue;
                    
                    uint64_t masked = window & ctx->mask_table[p];
                    
                    if (masked == ctx->pattern_table[p]) {
                        if (match_count < max_matches) {
                            matches[match_count].bit_position = 
                                current_bit - ctx->length_table[p] + 1;
                            matches[match_count].pattern_id = p;
                            matches[match_count].errors = 0;
                            matches[match_count].confidence = 100;
                        }
                        match_count++;
                    }
                }
            }
        }
    }
    
    return (match_count > max_matches) ? max_matches : match_count;
}

/* ============================================================================
 * Streaming Search
 * ============================================================================ */

void sync_stream_init(sync_finder_ctx_t *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->initialized = true;
}

int sync_stream_add_pattern(sync_finder_ctx_t *ctx, 
                            const sync_pattern_t *pattern) {
    if (!ctx || !pattern) return -1;
    if (ctx->pattern_count >= SYNC_MAX_PATTERNS) return -1;
    
    ctx->patterns[ctx->pattern_count++] = *pattern;
    return ctx->pattern_count - 1;
}

int sync_stream_process(sync_finder_ctx_t *ctx,
                        const uint8_t *data, size_t byte_count,
                        sync_match_t *matches, size_t max_matches) {
    if (!ctx || !data || !matches || max_matches == 0) {
        return 0;
    }
    if (ctx->pattern_count == 0) {
        return 0;
    }
    
    size_t match_count = 0;
    
    /* Find max pattern length */
    uint8_t max_len = 0;
    for (int p = 0; p < ctx->pattern_count; p++) {
        if (ctx->patterns[p].length > max_len) {
            max_len = ctx->patterns[p].length;
        }
    }
    
    uint64_t window_mask = (max_len < 64) ? 
                           ((1ULL << max_len) - 1) : ~0ULL;
    
    for (size_t byte_idx = 0; byte_idx < byte_count; byte_idx++) {
        uint8_t byte = data[byte_idx];
        
        for (int bit = 7; bit >= 0; bit--) {
            /* Shift window */
            ctx->window = ((ctx->window << 1) | ((byte >> bit) & 1)) & window_mask;
            ctx->window_valid++;
            
            /* Check all patterns */
            for (int p = 0; p < ctx->pattern_count; p++) {
                const sync_pattern_t *pat = &ctx->patterns[p];
                
                if (ctx->window_valid < pat->length) continue;
                
                uint64_t masked = ctx->window & pat->mask;
                
                if (masked == pat->pattern) {
                    if (match_count < max_matches) {
                        matches[match_count].bit_position = 
                            ctx->current_bit - pat->length + 1;
                        matches[match_count].pattern_id = pat->id;
                        matches[match_count].errors = 0;
                        matches[match_count].confidence = 100;
                    }
                    match_count++;
                    ctx->matches_found++;
                }
            }
            
            ctx->current_bit++;
        }
        
        ctx->bytes_processed++;
    }
    
    return (match_count > max_matches) ? max_matches : match_count;
}

void sync_stream_reset(sync_finder_ctx_t *ctx) {
    if (!ctx) return;
    
    ctx->window = 0;
    ctx->window_valid = 0;
    ctx->current_bit = 0;
    ctx->matches_found = 0;
    ctx->bytes_processed = 0;
}

/* ============================================================================
 * SIMD Detection
 * ============================================================================ */

bool sync_simd_available(void) {
#if defined(__SSE2__) || defined(__ARM_NEON)
    return true;
#else
    return false;
#endif
}

int sync_find_pattern_simd(const uint8_t *data, size_t bit_count,
                           const sync_pattern_t *pattern,
                           sync_match_t *matches, size_t max_matches) {
    /* 
     * For short patterns (<= 64 bits), the scalar sliding window
     * is already very fast. SIMD provides benefit mainly for:
     * - Bulk byte-aligned searching
     * - Very long patterns
     * - Multi-pattern with many patterns
     * 
     * For now, use scalar implementation.
     * SIMD optimization can be added later for specific cases.
     */
    return sync_find_pattern(data, bit_count, pattern, matches, max_matches);
}
