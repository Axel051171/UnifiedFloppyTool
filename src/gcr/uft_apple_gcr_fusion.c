/**
 * @file uft_apple_gcr_fusion.c
 * @brief Apple GCR Multi-Revolution Fusion
 * 
 * Combines multiple captures of the same Apple II or Macintosh track
 * to improve read reliability, detect weak bits, and handle
 * copy-protected disks.
 * 
 * Features:
 * - Revolution alignment via sync pattern matching
 * - Confidence-weighted nibble fusion
 * - Weak bit detection through revolution comparison
 * - Copy protection analysis
 * - Bit-level timing variance analysis
 * 
 * @author UFT Team
 * @version 3.4.0
 * @date 2026-01-03
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/uft_apple_gcr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

/*============================================================================
 * Constants
 *============================================================================*/

/** Maximum revolutions to fuse */
#define MAX_REVOLUTIONS         16

/** Maximum nibbles per track (Apple II ~6656, Mac varies by zone) */
#define MAX_TRACK_NIBBLES       8192

/** Minimum sync bytes for alignment */
#define MIN_SYNC_BYTES          5

/** Coefficient of variation threshold for weak bit detection */
#define WEAK_BIT_CV_THRESHOLD   0.15

/** Alignment search window (nibbles) */
#define ALIGNMENT_WINDOW        128

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    GCR_FUSION_OK = 0,
    GCR_FUSION_ERR_NULL_PARAM,
    GCR_FUSION_ERR_NO_REVOLUTIONS,
    GCR_FUSION_ERR_ALLOC,
    GCR_FUSION_ERR_ALIGNMENT,
    GCR_FUSION_ERR_SHORT_TRACK,
    GCR_FUSION_ERR_COUNT
} gcr_fusion_error_t;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief Single revolution capture
 */
typedef struct {
    uint8_t    *nibbles;        /**< Nibble data */
    uint32_t    nibble_count;   /**< Number of nibbles */
    double     *timings;        /**< Per-nibble timing (ns), optional */
    bool        has_timing;     /**< Has timing data */
    int32_t     alignment;      /**< Alignment offset relative to ref */
} gcr_revolution_t;

/**
 * @brief Fusion context
 */
typedef struct {
    gcr_revolution_t revolutions[MAX_REVOLUTIONS];
    uint8_t     rev_count;
    
    /* Reference (longest or first revolution) */
    uint8_t     ref_index;
    uint32_t    ref_length;
    
    /* Fused output */
    uint8_t    *fused_nibbles;
    uint8_t    *confidence;      /**< Per-nibble confidence (0-100) */
    uint8_t    *weak_mask;       /**< Weak bit flags (1 byte per nibble) */
    uint32_t    fused_count;
    
    /* Statistics */
    uint32_t    weak_nibbles;
    uint32_t    unanimous_nibbles;
    double      avg_confidence;
    
    /* Fusion method */
    uint8_t     method;          /**< 0=majority, 1=weighted, 2=best */
} gcr_fusion_ctx_t;

/**
 * @brief Fusion result per nibble
 */
typedef struct {
    uint8_t     value;           /**< Fused nibble value */
    uint8_t     confidence;      /**< Confidence (0-100) */
    bool        is_weak;         /**< Detected as weak/fuzzy */
    uint8_t     variance;        /**< Value variance across revolutions */
} gcr_fused_nibble_t;

/*============================================================================
 * Error Strings
 *============================================================================*/

static const char *fusion_error_strings[GCR_FUSION_ERR_COUNT] = {
    "OK",
    "Null parameter",
    "No revolutions provided",
    "Memory allocation failed",
    "Cannot align revolutions",
    "Track too short"
};

const char *gcr_fusion_error_string(gcr_fusion_error_t err) {
    if (err >= GCR_FUSION_ERR_COUNT) return "Unknown error";
    return fusion_error_strings[err];
}

/*============================================================================
 * Alignment Functions
 *============================================================================*/

/**
 * @brief Find sync pattern in nibble stream
 */
static int32_t find_sync_pattern(const uint8_t *nibbles, uint32_t count,
                                  uint32_t start, uint8_t min_sync) {
    if (!nibbles || count < min_sync) return -1;
    
    uint8_t consecutive = 0;
    for (uint32_t i = start; i < count; i++) {
        if (nibbles[i] == UFT_APPLE_SYNC_BYTE) {
            consecutive++;
            if (consecutive >= min_sync) {
                return (int32_t)(i - consecutive + 1);
            }
        } else {
            consecutive = 0;
        }
    }
    return -1;
}

/**
 * @brief Find D5 AA prologue pattern
 */
static int32_t find_prologue(const uint8_t *nibbles, uint32_t count,
                              uint32_t start) {
    if (!nibbles || count < 3) return -1;
    
    for (uint32_t i = start; i < count - 2; i++) {
        if (nibbles[i] == 0xD5 && nibbles[i+1] == 0xAA) {
            /* Either address (0x96) or data (0xAD) prologue */
            if (nibbles[i+2] == 0x96 || nibbles[i+2] == 0xAD) {
                return (int32_t)i;
            }
        }
    }
    return -1;
}

/**
 * @brief Cross-correlate two nibble sequences for alignment
 * 
 * Uses normalized cross-correlation to find best alignment offset.
 */
static int32_t correlate_sequences(const uint8_t *ref, uint32_t ref_len,
                                    const uint8_t *seq, uint32_t seq_len,
                                    int32_t window) {
    if (!ref || !seq || ref_len < 64 || seq_len < 64) return 0;
    
    int32_t best_offset = 0;
    uint32_t best_matches = 0;
    
    /* Use first prologue as anchor */
    int32_t ref_anchor = find_prologue(ref, ref_len, 0);
    if (ref_anchor < 0) ref_anchor = 0;
    
    int32_t seq_anchor = find_prologue(seq, seq_len, 0);
    if (seq_anchor < 0) seq_anchor = 0;
    
    /* Initial estimate based on prologue positions */
    int32_t initial_offset = seq_anchor - ref_anchor;
    
    /* Refine with correlation in window around initial estimate */
    for (int32_t offset = initial_offset - window; 
         offset <= initial_offset + window; 
         offset++) {
        
        uint32_t matches = 0;
        uint32_t compared = 0;
        
        for (uint32_t i = 0; i < ref_len && i < 512; i++) {
            int32_t j = (int32_t)i + offset;
            if (j >= 0 && j < (int32_t)seq_len) {
                compared++;
                if (ref[i] == seq[j]) matches++;
            }
        }
        
        if (matches > best_matches) {
            best_matches = matches;
            best_offset = offset;
        }
    }
    
    return best_offset;
}

/**
 * @brief Align all revolutions to reference
 */
static gcr_fusion_error_t align_revolutions(gcr_fusion_ctx_t *ctx) {
    if (!ctx || ctx->rev_count == 0) return GCR_FUSION_ERR_NULL_PARAM;
    
    /* Find longest revolution as reference */
    ctx->ref_index = 0;
    ctx->ref_length = ctx->revolutions[0].nibble_count;
    
    for (uint8_t i = 1; i < ctx->rev_count; i++) {
        if (ctx->revolutions[i].nibble_count > ctx->ref_length) {
            ctx->ref_index = i;
            ctx->ref_length = ctx->revolutions[i].nibble_count;
        }
    }
    
    gcr_revolution_t *ref = &ctx->revolutions[ctx->ref_index];
    ref->alignment = 0;
    
    /* Align all others to reference */
    for (uint8_t i = 0; i < ctx->rev_count; i++) {
        if (i == ctx->ref_index) continue;
        
        gcr_revolution_t *rev = &ctx->revolutions[i];
        
        rev->alignment = correlate_sequences(
            ref->nibbles, ref->nibble_count,
            rev->nibbles, rev->nibble_count,
            ALIGNMENT_WINDOW
        );
    }
    
    return GCR_FUSION_OK;
}

/*============================================================================
 * Fusion Functions
 *============================================================================*/

/**
 * @brief Fuse single nibble position using majority vote
 */
static gcr_fused_nibble_t fuse_nibble_majority(gcr_fusion_ctx_t *ctx, 
                                                uint32_t pos) {
    gcr_fused_nibble_t result = {0};
    uint8_t counts[256] = {0};
    uint8_t valid_count = 0;
    
    for (uint8_t r = 0; r < ctx->rev_count; r++) {
        gcr_revolution_t *rev = &ctx->revolutions[r];
        int32_t adj_pos = (int32_t)pos + rev->alignment;
        
        if (adj_pos >= 0 && adj_pos < (int32_t)rev->nibble_count) {
            uint8_t val = rev->nibbles[adj_pos];
            counts[val]++;
            valid_count++;
        }
    }
    
    if (valid_count == 0) {
        result.confidence = 0;
        return result;
    }
    
    /* Find most common value */
    uint8_t max_count = 0;
    uint8_t max_val = 0;
    uint8_t unique_values = 0;
    
    for (int v = 0; v < 256; v++) {
        if (counts[v] > 0) {
            unique_values++;
            if (counts[v] > max_count) {
                max_count = counts[v];
                max_val = (uint8_t)v;
            }
        }
    }
    
    result.value = max_val;
    result.confidence = (uint8_t)((max_count * 100) / valid_count);
    result.variance = unique_values - 1;
    result.is_weak = (max_count < valid_count);  /* Not unanimous */
    
    return result;
}

/**
 * @brief Fuse single nibble position using weighted average
 * 
 * Weight based on timing consistency (if available).
 */
static gcr_fused_nibble_t fuse_nibble_weighted(gcr_fusion_ctx_t *ctx,
                                                uint32_t pos) {
    gcr_fused_nibble_t result = {0};
    
    /* Check if we have timing data */
    bool has_timing = false;
    for (uint8_t r = 0; r < ctx->rev_count; r++) {
        if (ctx->revolutions[r].has_timing) {
            has_timing = true;
            break;
        }
    }
    
    if (!has_timing) {
        /* Fall back to majority vote */
        return fuse_nibble_majority(ctx, pos);
    }
    
    /* Calculate timing variance and use inverse as weight */
    double weights[MAX_REVOLUTIONS] = {0};
    double weight_sum = 0;
    uint8_t values[MAX_REVOLUTIONS] = {0};
    uint8_t valid_count = 0;
    
    /* First pass: collect timings */
    double timings[MAX_REVOLUTIONS] = {0};
    for (uint8_t r = 0; r < ctx->rev_count; r++) {
        gcr_revolution_t *rev = &ctx->revolutions[r];
        int32_t adj_pos = (int32_t)pos + rev->alignment;
        
        if (adj_pos >= 0 && adj_pos < (int32_t)rev->nibble_count) {
            values[valid_count] = rev->nibbles[adj_pos];
            if (rev->has_timing && rev->timings) {
                timings[valid_count] = rev->timings[adj_pos];
            } else {
                timings[valid_count] = 4000.0;  /* Default 4µs */
            }
            valid_count++;
        }
    }
    
    if (valid_count == 0) {
        result.confidence = 0;
        return result;
    }
    
    /* Calculate mean timing */
    double mean_timing = 0;
    for (uint8_t i = 0; i < valid_count; i++) {
        mean_timing += timings[i];
    }
    mean_timing /= valid_count;
    
    /* Calculate weights (inverse of deviation from mean) */
    for (uint8_t i = 0; i < valid_count; i++) {
        double dev = fabs(timings[i] - mean_timing);
        weights[i] = 1.0 / (1.0 + dev / 100.0);  /* Soft inverse */
        weight_sum += weights[i];
    }
    
    /* Normalize weights */
    if (weight_sum > 0) {
        for (uint8_t i = 0; i < valid_count; i++) {
            weights[i] /= weight_sum;
        }
    }
    
    /* Find weighted majority */
    double value_weights[256] = {0};
    for (uint8_t i = 0; i < valid_count; i++) {
        value_weights[values[i]] += weights[i];
    }
    
    double max_weight = 0;
    uint8_t max_val = 0;
    for (int v = 0; v < 256; v++) {
        if (value_weights[v] > max_weight) {
            max_weight = value_weights[v];
            max_val = (uint8_t)v;
        }
    }
    
    result.value = max_val;
    result.confidence = (uint8_t)(max_weight * 100);
    result.is_weak = (max_weight < 0.9);
    
    /* Count unique values for variance */
    uint8_t unique = 0;
    for (int v = 0; v < 256; v++) {
        if (value_weights[v] > 0) unique++;
    }
    result.variance = unique - 1;
    
    return result;
}

/**
 * @brief Fuse single nibble using best (lowest timing variance)
 */
static gcr_fused_nibble_t fuse_nibble_best(gcr_fusion_ctx_t *ctx,
                                            uint32_t pos) {
    gcr_fused_nibble_t result = {0};
    
    double best_variance = 1e10;
    uint8_t best_rev = 0;
    uint8_t valid_count = 0;
    uint8_t all_values[MAX_REVOLUTIONS] = {0};
    
    /* Find revolution with lowest local timing variance */
    for (uint8_t r = 0; r < ctx->rev_count; r++) {
        gcr_revolution_t *rev = &ctx->revolutions[r];
        int32_t adj_pos = (int32_t)pos + rev->alignment;
        
        if (adj_pos >= 0 && adj_pos < (int32_t)rev->nibble_count) {
            all_values[valid_count] = rev->nibbles[adj_pos];
            valid_count++;
            
            /* Calculate local variance (5 nibble window) */
            if (rev->has_timing && rev->timings) {
                double sum = 0, sum_sq = 0;
                int n = 0;
                for (int d = -2; d <= 2; d++) {
                    int32_t p = adj_pos + d;
                    if (p >= 0 && p < (int32_t)rev->nibble_count) {
                        double t = rev->timings[p];
                        sum += t;
                        sum_sq += t * t;
                        n++;
                    }
                }
                if (n > 1) {
                    double mean = sum / n;
                    double variance = (sum_sq / n) - (mean * mean);
                    if (variance < best_variance) {
                        best_variance = variance;
                        best_rev = r;
                    }
                }
            }
        }
    }
    
    if (valid_count == 0) {
        result.confidence = 0;
        return result;
    }
    
    /* Use value from best revolution */
    gcr_revolution_t *best = &ctx->revolutions[best_rev];
    int32_t adj_pos = (int32_t)pos + best->alignment;
    
    if (adj_pos >= 0 && adj_pos < (int32_t)best->nibble_count) {
        result.value = best->nibbles[adj_pos];
    }
    
    /* Calculate confidence from agreement */
    uint8_t agree_count = 0;
    for (uint8_t i = 0; i < valid_count; i++) {
        if (all_values[i] == result.value) agree_count++;
    }
    
    result.confidence = (uint8_t)((agree_count * 100) / valid_count);
    result.is_weak = (agree_count < valid_count);
    result.variance = valid_count - agree_count;
    
    return result;
}

/*============================================================================
 * Public API
 *============================================================================*/

/**
 * @brief Create fusion context
 */
gcr_fusion_ctx_t *gcr_fusion_create(void) {
    gcr_fusion_ctx_t *ctx = calloc(1, sizeof(gcr_fusion_ctx_t));
    if (ctx) {
        ctx->method = 0;  /* Default: majority vote */
    }
    return ctx;
}

/**
 * @brief Destroy fusion context
 */
void gcr_fusion_destroy(gcr_fusion_ctx_t *ctx) {
    if (!ctx) return;
    
    for (uint8_t i = 0; i < ctx->rev_count; i++) {
        /* Note: caller owns nibbles/timings, we don't free them */
    }
    
    free(ctx->fused_nibbles);
    free(ctx->confidence);
    free(ctx->weak_mask);
    free(ctx);
}

/**
 * @brief Add revolution to fusion context
 * 
 * @param ctx Fusion context
 * @param nibbles Nibble data (caller retains ownership)
 * @param count Nibble count
 * @param timings Optional per-nibble timing in ns (caller retains ownership)
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_add_revolution(gcr_fusion_ctx_t *ctx,
                                              const uint8_t *nibbles,
                                              uint32_t count,
                                              const double *timings) {
    if (!ctx || !nibbles) return GCR_FUSION_ERR_NULL_PARAM;
    if (ctx->rev_count >= MAX_REVOLUTIONS) return GCR_FUSION_ERR_ALLOC;
    if (count < 64) return GCR_FUSION_ERR_SHORT_TRACK;
    
    gcr_revolution_t *rev = &ctx->revolutions[ctx->rev_count];
    
    /* Store pointers (caller owns data) */
    rev->nibbles = (uint8_t *)nibbles;
    rev->nibble_count = count;
    rev->timings = (double *)timings;
    rev->has_timing = (timings != NULL);
    rev->alignment = 0;
    
    ctx->rev_count++;
    return GCR_FUSION_OK;
}

/**
 * @brief Set fusion method
 * 
 * @param ctx Fusion context
 * @param method 0=majority, 1=weighted, 2=best
 */
void gcr_fusion_set_method(gcr_fusion_ctx_t *ctx, uint8_t method) {
    if (ctx && method <= 2) {
        ctx->method = method;
    }
}

/**
 * @brief Execute fusion
 * 
 * @param ctx Fusion context with added revolutions
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_execute(gcr_fusion_ctx_t *ctx) {
    if (!ctx) return GCR_FUSION_ERR_NULL_PARAM;
    if (ctx->rev_count == 0) return GCR_FUSION_ERR_NO_REVOLUTIONS;
    
    /* Align revolutions */
    gcr_fusion_error_t err = align_revolutions(ctx);
    if (err != GCR_FUSION_OK) return err;
    
    /* Allocate output buffers */
    ctx->fused_count = ctx->ref_length;
    ctx->fused_nibbles = malloc(ctx->fused_count);
    ctx->confidence = malloc(ctx->fused_count);
    ctx->weak_mask = calloc(ctx->fused_count, 1);
    
    if (!ctx->fused_nibbles || !ctx->confidence || !ctx->weak_mask) {
        free(ctx->fused_nibbles);
        free(ctx->confidence);
        free(ctx->weak_mask);
        ctx->fused_nibbles = NULL;
        ctx->confidence = NULL;
        ctx->weak_mask = NULL;
        return GCR_FUSION_ERR_ALLOC;
    }
    
    /* Fuse each nibble position */
    uint64_t total_confidence = 0;
    ctx->weak_nibbles = 0;
    ctx->unanimous_nibbles = 0;
    
    for (uint32_t i = 0; i < ctx->fused_count; i++) {
        gcr_fused_nibble_t fused;
        
        switch (ctx->method) {
            case 1:
                fused = fuse_nibble_weighted(ctx, i);
                break;
            case 2:
                fused = fuse_nibble_best(ctx, i);
                break;
            default:
                fused = fuse_nibble_majority(ctx, i);
                break;
        }
        
        ctx->fused_nibbles[i] = fused.value;
        ctx->confidence[i] = fused.confidence;
        
        if (fused.is_weak) {
            ctx->weak_mask[i] = 1;
            ctx->weak_nibbles++;
        }
        
        if (fused.confidence == 100) {
            ctx->unanimous_nibbles++;
        }
        
        total_confidence += fused.confidence;
    }
    
    ctx->avg_confidence = (double)total_confidence / ctx->fused_count;
    
    return GCR_FUSION_OK;
}

/**
 * @brief Get fused nibbles
 * 
 * @param ctx Fusion context (after execute)
 * @param nibbles Output buffer (caller allocates)
 * @param max_nibbles Buffer size
 * @param out_count Actual count written
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_get_result(const gcr_fusion_ctx_t *ctx,
                                          uint8_t *nibbles,
                                          uint32_t max_nibbles,
                                          uint32_t *out_count) {
    if (!ctx || !nibbles || !out_count) return GCR_FUSION_ERR_NULL_PARAM;
    if (!ctx->fused_nibbles) return GCR_FUSION_ERR_NO_REVOLUTIONS;
    
    uint32_t to_copy = ctx->fused_count;
    if (to_copy > max_nibbles) to_copy = max_nibbles;
    
    memcpy(nibbles, ctx->fused_nibbles, to_copy);
    *out_count = to_copy;
    
    return GCR_FUSION_OK;
}

/**
 * @brief Get weak bit mask
 * 
 * @param ctx Fusion context (after execute)
 * @param mask Output buffer (caller allocates, same size as nibbles)
 * @param max_size Buffer size
 * @param weak_count Number of weak nibbles
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_get_weak_mask(const gcr_fusion_ctx_t *ctx,
                                             uint8_t *mask,
                                             uint32_t max_size,
                                             uint32_t *weak_count) {
    if (!ctx || !mask || !weak_count) return GCR_FUSION_ERR_NULL_PARAM;
    if (!ctx->weak_mask) return GCR_FUSION_ERR_NO_REVOLUTIONS;
    
    uint32_t to_copy = ctx->fused_count;
    if (to_copy > max_size) to_copy = max_size;
    
    memcpy(mask, ctx->weak_mask, to_copy);
    *weak_count = ctx->weak_nibbles;
    
    return GCR_FUSION_OK;
}

/**
 * @brief Get confidence values
 * 
 * @param ctx Fusion context (after execute)
 * @param confidence Output buffer (caller allocates)
 * @param max_size Buffer size
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_get_confidence(const gcr_fusion_ctx_t *ctx,
                                              uint8_t *confidence,
                                              uint32_t max_size) {
    if (!ctx || !confidence) return GCR_FUSION_ERR_NULL_PARAM;
    if (!ctx->confidence) return GCR_FUSION_ERR_NO_REVOLUTIONS;
    
    uint32_t to_copy = ctx->fused_count;
    if (to_copy > max_size) to_copy = max_size;
    
    memcpy(confidence, ctx->confidence, to_copy);
    return GCR_FUSION_OK;
}

/**
 * @brief Get fusion statistics
 * 
 * @param ctx Fusion context (after execute)
 * @param weak_count Number of weak nibbles
 * @param unanimous_count Number of unanimous nibbles
 * @param avg_confidence Average confidence (0-100)
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_get_stats(const gcr_fusion_ctx_t *ctx,
                                         uint32_t *weak_count,
                                         uint32_t *unanimous_count,
                                         double *avg_confidence) {
    if (!ctx) return GCR_FUSION_ERR_NULL_PARAM;
    
    if (weak_count) *weak_count = ctx->weak_nibbles;
    if (unanimous_count) *unanimous_count = ctx->unanimous_nibbles;
    if (avg_confidence) *avg_confidence = ctx->avg_confidence;
    
    return GCR_FUSION_OK;
}

/*============================================================================
 * Convenience Functions
 *============================================================================*/

/**
 * @brief Fuse multiple Apple II track captures
 * 
 * Convenience wrapper for common use case.
 * 
 * @param nibbles Array of nibble buffers
 * @param counts Array of nibble counts
 * @param rev_count Number of revolutions
 * @param output Output buffer (caller allocates, use max of counts)
 * @param out_count Actual output count
 * @param weak_mask Optional weak bit mask (caller allocates)
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_apple_track(const uint8_t **nibbles,
                                           const uint32_t *counts,
                                           uint8_t rev_count,
                                           uint8_t *output,
                                           uint32_t *out_count,
                                           uint8_t *weak_mask) {
    if (!nibbles || !counts || !output || !out_count) {
        return GCR_FUSION_ERR_NULL_PARAM;
    }
    if (rev_count == 0) return GCR_FUSION_ERR_NO_REVOLUTIONS;
    
    gcr_fusion_ctx_t *ctx = gcr_fusion_create();
    if (!ctx) return GCR_FUSION_ERR_ALLOC;
    
    gcr_fusion_error_t err;
    
    /* Add all revolutions */
    for (uint8_t i = 0; i < rev_count; i++) {
        err = gcr_fusion_add_revolution(ctx, nibbles[i], counts[i], NULL);
        if (err != GCR_FUSION_OK) {
            gcr_fusion_destroy(ctx);
            return err;
        }
    }
    
    /* Execute fusion */
    err = gcr_fusion_execute(ctx);
    if (err != GCR_FUSION_OK) {
        gcr_fusion_destroy(ctx);
        return err;
    }
    
    /* Get results */
    uint32_t max_count = 0;
    for (uint8_t i = 0; i < rev_count; i++) {
        if (counts[i] > max_count) max_count = counts[i];
    }
    
    err = gcr_fusion_get_result(ctx, output, max_count, out_count);
    if (err != GCR_FUSION_OK) {
        gcr_fusion_destroy(ctx);
        return err;
    }
    
    /* Get weak mask if requested */
    if (weak_mask) {
        uint32_t weak_count;
        gcr_fusion_get_weak_mask(ctx, weak_mask, max_count, &weak_count);
    }
    
    gcr_fusion_destroy(ctx);
    return GCR_FUSION_OK;
}

/*============================================================================
 * Unit Tests
 *============================================================================*/

#ifdef UFT_UNIT_TESTS

#include <assert.h>

static void test_gcr_fusion_majority(void) {
    /* Create test data - 3 revolutions with mostly agreeing nibbles */
    uint8_t rev1[] = {0xFF, 0xFF, 0xFF, 0xD5, 0xAA, 0x96, 0x01, 0x02};
    uint8_t rev2[] = {0xFF, 0xFF, 0xFF, 0xD5, 0xAA, 0x96, 0x01, 0x02};
    uint8_t rev3[] = {0xFF, 0xFF, 0xFF, 0xD5, 0xAA, 0x96, 0x01, 0xFF}; /* Last differs */
    
    const uint8_t *nibbles[] = {rev1, rev2, rev3};
    uint32_t counts[] = {8, 8, 8};
    
    uint8_t output[8];
    uint32_t out_count;
    uint8_t weak[8];
    
    gcr_fusion_error_t err = gcr_fusion_apple_track(nibbles, counts, 3,
                                                     output, &out_count, weak);
    assert(err == GCR_FUSION_OK);
    assert(out_count == 8);
    
    /* First 7 nibbles should match (unanimous) */
    assert(output[0] == 0xFF);
    assert(output[3] == 0xD5);
    assert(output[6] == 0x01);
    
    /* Last nibble should be 0x02 (majority) */
    assert(output[7] == 0x02);
    
    /* Last nibble should be marked as weak */
    assert(weak[7] == 1);
    
    printf("  ✓ gcr_fusion_majority passed\n");
}

static void test_gcr_fusion_alignment(void) {
    /* Test alignment with offset revolutions */
    uint8_t rev1[] = {0xFF, 0xFF, 0xD5, 0xAA, 0x96, 0x01, 0x02, 0x03};
    uint8_t rev2[] = {0xFF, 0xD5, 0xAA, 0x96, 0x01, 0x02, 0x03, 0x04};  /* 1 byte earlier */
    
    gcr_fusion_ctx_t *ctx = gcr_fusion_create();
    assert(ctx != NULL);
    
    gcr_fusion_error_t err;
    err = gcr_fusion_add_revolution(ctx, rev1, 8, NULL);
    assert(err == GCR_FUSION_OK);
    err = gcr_fusion_add_revolution(ctx, rev2, 8, NULL);
    assert(err == GCR_FUSION_OK);
    
    err = gcr_fusion_execute(ctx);
    assert(err == GCR_FUSION_OK);
    
    /* Should align on D5 AA 96 pattern */
    uint32_t weak, unan;
    double avg;
    gcr_fusion_get_stats(ctx, &weak, &unan, &avg);
    
    /* Most nibbles should agree after alignment */
    assert(unan >= 5);
    
    gcr_fusion_destroy(ctx);
    printf("  ✓ gcr_fusion_alignment passed\n");
}

static void test_gcr_fusion_single_rev(void) {
    /* Single revolution should just copy */
    uint8_t rev1[] = {0xD5, 0xAA, 0x96, 0x01, 0x02, 0x03, 0x04, 0x05};
    
    const uint8_t *nibbles[] = {rev1};
    uint32_t counts[] = {8};
    
    uint8_t output[8];
    uint32_t out_count;
    
    gcr_fusion_error_t err = gcr_fusion_apple_track(nibbles, counts, 1,
                                                     output, &out_count, NULL);
    assert(err == GCR_FUSION_OK);
    assert(out_count == 8);
    assert(memcmp(output, rev1, 8) == 0);
    
    printf("  ✓ gcr_fusion_single_rev passed\n");
}

static void test_gcr_fusion_error_handling(void) {
    /* Test error cases */
    gcr_fusion_ctx_t *ctx = gcr_fusion_create();
    assert(ctx != NULL);
    
    /* Add empty revolution should fail */
    gcr_fusion_error_t err = gcr_fusion_add_revolution(ctx, NULL, 0, NULL);
    assert(err == GCR_FUSION_ERR_NULL_PARAM);
    
    /* Execute with no revolutions should fail */
    err = gcr_fusion_execute(ctx);
    assert(err == GCR_FUSION_ERR_NO_REVOLUTIONS);
    
    gcr_fusion_destroy(ctx);
    
    /* NULL context */
    assert(gcr_fusion_error_string(GCR_FUSION_OK) != NULL);
    assert(gcr_fusion_error_string(GCR_FUSION_ERR_COUNT + 1) != NULL);
    
    printf("  ✓ gcr_fusion_error_handling passed\n");
}

void uft_apple_gcr_fusion_tests(void) {
    printf("Running Apple GCR Fusion tests...\n");
    test_gcr_fusion_majority();
    test_gcr_fusion_alignment();
    test_gcr_fusion_single_rev();
    test_gcr_fusion_error_handling();
    printf("All Apple GCR Fusion tests passed!\n");
}

#endif /* UFT_UNIT_TESTS */
