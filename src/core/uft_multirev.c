/**
 * @file uft_multirev.c
 * @brief Multi-Revolution Voting Algorithm Implementation
 * 
 * @version 1.0.0
 * @date 2025
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/uft_multirev.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Per-revolution decoded data */
typedef struct {
    uint8_t*    bits;           /**< Decoded bits (packed, MSB first) */
    uint32_t    bit_count;      /**< Number of bits */
    uint8_t*    confidence;     /**< Per-bit confidence (can be NULL) */
    uint32_t*   timing;         /**< Per-bit timing (ns, can be NULL) */
    float       quality;        /**< Revolution quality score */
    bool        valid;          /**< Data is valid */
} rev_data_t;

/** Voting accumulator per bit position */
typedef struct {
    uint8_t     count_0;        /**< Votes for 0 */
    uint8_t     count_1;        /**< Votes for 1 */
    uint8_t     count_miss;     /**< Missing votes */
    uint32_t    timing_sum;     /**< Sum of timings */
    uint32_t    timing_sqsum;   /**< Sum of timing squares */
    uint8_t     timing_count;   /**< Timing samples */
} vote_accum_t;

/** Context structure */
struct uft_mrv_context {
    uft_mrv_params_t params;
    rev_data_t  revolutions[UFT_MRV_MAX_REVOLUTIONS];
    int         rev_count;
    uint32_t    max_bits;       /**< Maximum bits across all revolutions */
    uint32_t    aligned_bits;   /**< Bits after alignment */
    vote_accum_t* accumulators; /**< Voting accumulators */
    bool        analyzed;       /**< Analysis complete */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

static void* safe_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;
    if (count > SIZE_MAX / size) return NULL;
    return calloc(count, size);
}

static void* safe_realloc(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

/** Get bit at position from packed array */
static inline uint8_t get_bit(const uint8_t* data, uint32_t pos) {
    return (data[pos / 8] >> (7 - (pos % 8))) & 1;
}

/** Set bit at position in packed array */
static inline void set_bit(uint8_t* data, uint32_t pos, uint8_t value) {
    uint32_t byte_pos = pos / 8;
    uint8_t bit_pos = 7 - (pos % 8);
    if (value) {
        data[byte_pos] |= (1 << bit_pos);
    } else {
        data[byte_pos] &= ~(1 << bit_pos);
    }
}

/** Calculate standard deviation from sum and sum of squares */
static inline float calc_stddev(uint32_t sum, uint32_t sqsum, int count) {
    if (count < 2) return 0.0f;
    float mean = (float)sum / count;
    float variance = ((float)sqsum / count) - (mean * mean);
    return (variance > 0) ? sqrtf(variance) : 0.0f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FLUX TO BITS DECODER
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Decode flux deltas to bits using PLL
 * 
 * Simple PLL decoder for MFM-style encoding.
 * More sophisticated decoders exist in the decode pipeline.
 */
static int decode_flux_to_bits(const uint32_t* deltas, uint32_t count,
                                uint32_t bitcell_ns, uint8_t** bits_out,
                                uint32_t** timing_out, uint32_t* bit_count_out) {
    if (!deltas || count == 0 || !bits_out || !bit_count_out) {
        return UFT_MRV_ERR_INVALID;
    }
    
    /* Estimate bit count (worst case: every delta is minimum cell) */
    uint32_t max_bits = count * 4;
    uint32_t byte_count = (max_bits + 7) / 8;
    
    uint8_t* bits = (uint8_t*)safe_calloc(byte_count, 1);
    uint32_t* timing = timing_out ? (uint32_t*)safe_calloc(max_bits, sizeof(uint32_t)) : NULL;
    
    if (!bits || (timing_out && !timing)) {
        free(bits);
        free(timing);
        return UFT_MRV_ERR_NOMEM;
    }
    
    /* PLL state */
    float pll_period = (float)bitcell_ns;
    float pll_phase = 0.0f;
    const float pll_gain = 0.05f;  /* Phase adjustment gain */
    
    uint32_t bit_pos = 0;
    
    for (uint32_t i = 0; i < count && bit_pos < max_bits; i++) {
        float delta = (float)deltas[i];
        
        /* Calculate number of bit cells in this delta */
        float cells = delta / pll_period;
        int cell_count = (int)(cells + 0.5f);
        
        if (cell_count < 1) cell_count = 1;
        if (cell_count > 4) cell_count = 4;  /* Clamp for MFM */
        
        /* Insert zeros for intermediate cells, 1 for flux transition */
        for (int c = 0; c < cell_count - 1 && bit_pos < max_bits; c++) {
            set_bit(bits, bit_pos, 0);
            if (timing) timing[bit_pos] = bitcell_ns;
            bit_pos++;
        }
        
        /* Flux transition = 1 */
        if (bit_pos < max_bits) {
            set_bit(bits, bit_pos, 1);
            if (timing) timing[bit_pos] = deltas[i] - (uint32_t)((cell_count - 1) * pll_period);
            bit_pos++;
        }
        
        /* Adjust PLL */
        float error = delta - (cell_count * pll_period);
        pll_phase += error * pll_gain;
        
        /* Limit phase adjustment */
        if (pll_phase > pll_period * 0.2f) pll_phase = pll_period * 0.2f;
        if (pll_phase < -pll_period * 0.2f) pll_phase = -pll_period * 0.2f;
        
        /* Slowly adjust period (data separator) */
        pll_period += error * pll_gain * 0.1f;
        
        /* Keep period in reasonable range */
        float min_period = bitcell_ns * 0.9f;
        float max_period = bitcell_ns * 1.1f;
        if (pll_period < min_period) pll_period = min_period;
        if (pll_period > max_period) pll_period = max_period;
    }
    
    *bits_out = bits;
    *bit_count_out = bit_pos;
    if (timing_out) *timing_out = timing;
    
    return UFT_MRV_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * REVOLUTION ALIGNMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Align two bit sequences using cross-correlation
 * @return Offset to apply to seq2 to align with seq1
 */
static int align_sequences(const uint8_t* seq1, uint32_t len1,
                           const uint8_t* seq2, uint32_t len2,
                           int search_range) {
    if (!seq1 || !seq2 || len1 == 0 || len2 == 0) return 0;
    
    uint32_t compare_len = (len1 < len2) ? len1 : len2;
    if (compare_len > 1000) compare_len = 1000;  /* Limit for performance */
    
    int best_offset = 0;
    int best_score = -1;
    
    for (int offset = -search_range; offset <= search_range; offset++) {
        int score = 0;
        
        for (uint32_t i = 0; i < compare_len; i++) {
            int pos1 = (int)i;
            int pos2 = (int)i + offset;
            
            if (pos2 < 0 || pos2 >= (int)len2) continue;
            
            uint8_t bit1 = get_bit(seq1, (uint32_t)pos1);
            uint8_t bit2 = get_bit(seq2, (uint32_t)pos2);
            
            if (bit1 == bit2) score++;
        }
        
        if (score > best_score) {
            best_score = score;
            best_offset = offset;
        }
    }
    
    return best_offset;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CONTEXT MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

void uft_mrv_get_defaults(uft_mrv_params_t* params) {
    if (!params) return;
    
    params->strategy = UFT_MRV_STRATEGY_WEIGHTED;
    params->min_confidence = UFT_MRV_CONFIDENCE_STABLE;
    params->weak_threshold = UFT_MRV_CONFIDENCE_WEAK;
    params->detect_protection = true;
    params->preserve_weak = true;
    params->timing_tolerance_ns = 500;
    params->min_weak_run = 8;
}

uft_mrv_context_t* uft_mrv_create(const uft_mrv_params_t* params) {
    uft_mrv_context_t* ctx = (uft_mrv_context_t*)safe_calloc(1, sizeof(uft_mrv_context_t));
    if (!ctx) return NULL;
    
    if (params) {
        ctx->params = *params;
    } else {
        uft_mrv_get_defaults(&ctx->params);
    }
    
    return ctx;
}

void uft_mrv_free(uft_mrv_context_t* ctx) {
    if (!ctx) return;
    
    /* Free revolution data */
    for (int i = 0; i < ctx->rev_count; i++) {
        free(ctx->revolutions[i].bits);
        free(ctx->revolutions[i].confidence);
        free(ctx->revolutions[i].timing);
    }
    
    free(ctx->accumulators);
    free(ctx);
}

void uft_mrv_reset(uft_mrv_context_t* ctx) {
    if (!ctx) return;
    
    /* Free revolution data */
    for (int i = 0; i < ctx->rev_count; i++) {
        free(ctx->revolutions[i].bits);
        free(ctx->revolutions[i].confidence);
        free(ctx->revolutions[i].timing);
        memset(&ctx->revolutions[i], 0, sizeof(rev_data_t));
    }
    
    free(ctx->accumulators);
    ctx->accumulators = NULL;
    
    ctx->rev_count = 0;
    ctx->max_bits = 0;
    ctx->aligned_bits = 0;
    ctx->analyzed = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * REVOLUTION INPUT
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_mrv_add_revolution(uft_mrv_context_t* ctx, const uft_ir_revolution_t* rev) {
    if (!ctx || !rev) return UFT_MRV_ERR_INVALID;
    if (ctx->rev_count >= UFT_MRV_MAX_REVOLUTIONS) return UFT_MRV_ERR_OVERFLOW;
    
    /* Get bitcell time from stats or use default */
    uint32_t bitcell_ns = 2000;  /* Default MFM */
    if (rev->stats.clock_period_ns > 0) {
        bitcell_ns = rev->stats.clock_period_ns;
    }
    
    return uft_mrv_add_flux(ctx, rev->flux_deltas, rev->flux_count, bitcell_ns);
}

int uft_mrv_add_flux(uft_mrv_context_t* ctx, const uint32_t* deltas,
                     uint32_t count, uint32_t bitcell_ns) {
    if (!ctx || !deltas || count == 0) return UFT_MRV_ERR_INVALID;
    if (ctx->rev_count >= UFT_MRV_MAX_REVOLUTIONS) return UFT_MRV_ERR_OVERFLOW;
    
    rev_data_t* rev = &ctx->revolutions[ctx->rev_count];
    
    /* Decode flux to bits */
    int ret = decode_flux_to_bits(deltas, count, bitcell_ns,
                                   &rev->bits, &rev->timing, &rev->bit_count);
    if (ret != UFT_MRV_OK) return ret;
    
    rev->valid = true;
    rev->quality = 1.0f;  /* Will be calculated later */
    
    /* Update max bits */
    if (rev->bit_count > ctx->max_bits) {
        ctx->max_bits = rev->bit_count;
    }
    
    ctx->rev_count++;
    ctx->analyzed = false;
    
    return UFT_MRV_OK;
}

int uft_mrv_add_bits(uft_mrv_context_t* ctx, const uint8_t* bits,
                     uint32_t bit_count, const uint8_t* confidence) {
    if (!ctx || !bits || bit_count == 0) return UFT_MRV_ERR_INVALID;
    if (ctx->rev_count >= UFT_MRV_MAX_REVOLUTIONS) return UFT_MRV_ERR_OVERFLOW;
    
    rev_data_t* rev = &ctx->revolutions[ctx->rev_count];
    
    /* Copy bits */
    uint32_t byte_count = (bit_count + 7) / 8;
    rev->bits = (uint8_t*)safe_calloc(byte_count, 1);
    if (!rev->bits) return UFT_MRV_ERR_NOMEM;
    
    memcpy(rev->bits, bits, byte_count);
    rev->bit_count = bit_count;
    
    /* Copy confidence if provided */
    if (confidence) {
        rev->confidence = (uint8_t*)safe_calloc(bit_count, 1);
        if (rev->confidence) {
            memcpy(rev->confidence, confidence, bit_count);
        }
    }
    
    rev->valid = true;
    rev->quality = 1.0f;
    
    if (bit_count > ctx->max_bits) {
        ctx->max_bits = bit_count;
    }
    
    ctx->rev_count++;
    ctx->analyzed = false;
    
    return UFT_MRV_OK;
}

int uft_mrv_add_track(uft_mrv_context_t* ctx, const uft_ir_track_t* track) {
    if (!ctx || !track) return UFT_MRV_ERR_INVALID;
    
    for (int i = 0; i < track->revolution_count && i < UFT_MRV_MAX_REVOLUTIONS; i++) {
        if (!track->revolutions[i]) continue;
        
        int ret = uft_mrv_add_revolution(ctx, track->revolutions[i]);
        if (ret != UFT_MRV_OK && ret != UFT_MRV_ERR_OVERFLOW) {
            return ret;
        }
    }
    
    return UFT_MRV_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ANALYSIS & VOTING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Perform alignment of all revolutions to first revolution
 */
static void align_all_revolutions(uft_mrv_context_t* ctx) {
    if (ctx->rev_count < 2) return;
    
    const rev_data_t* ref = &ctx->revolutions[0];
    
    for (int i = 1; i < ctx->rev_count; i++) {
        rev_data_t* rev = &ctx->revolutions[i];
        if (!rev->valid) continue;
        
        /* Find alignment offset */
        int offset = align_sequences(ref->bits, ref->bit_count,
                                      rev->bits, rev->bit_count, 100);
        
        /* Apply offset by adjusting bit count interpretation */
        /* For simplicity, we store the offset conceptually */
        /* In a full implementation, we'd shift the data */
        (void)offset;  /* TODO: Apply offset in voting */
    }
}

/**
 * @brief Accumulate votes from all revolutions
 */
static int accumulate_votes(uft_mrv_context_t* ctx) {
    if (ctx->max_bits == 0) return UFT_MRV_ERR_NO_DATA;
    
    /* Allocate accumulators */
    ctx->accumulators = (vote_accum_t*)safe_calloc(ctx->max_bits, sizeof(vote_accum_t));
    if (!ctx->accumulators) return UFT_MRV_ERR_NOMEM;
    
    ctx->aligned_bits = ctx->max_bits;
    
    /* Collect votes from each revolution */
    for (int r = 0; r < ctx->rev_count; r++) {
        const rev_data_t* rev = &ctx->revolutions[r];
        if (!rev->valid) continue;
        
        for (uint32_t b = 0; b < rev->bit_count && b < ctx->aligned_bits; b++) {
            vote_accum_t* acc = &ctx->accumulators[b];
            uint8_t bit = get_bit(rev->bits, b);
            
            /* Get confidence weight */
            uint8_t conf = 100;
            if (rev->confidence) {
                conf = rev->confidence[b];
            }
            
            /* Only count confident bits */
            if (conf < 20) {
                acc->count_miss++;
            } else if (bit) {
                acc->count_1++;
            } else {
                acc->count_0++;
            }
            
            /* Accumulate timing statistics */
            if (rev->timing && rev->timing[b] > 0) {
                acc->timing_sum += rev->timing[b];
                acc->timing_sqsum += rev->timing[b] * rev->timing[b];
                acc->timing_count++;
            }
        }
        
        /* Mark bits beyond this revolution as missing */
        for (uint32_t b = rev->bit_count; b < ctx->aligned_bits; b++) {
            ctx->accumulators[b].count_miss++;
        }
    }
    
    return UFT_MRV_OK;
}

/**
 * @brief Perform voting and classification
 */
static int perform_voting(uft_mrv_context_t* ctx, uft_mrv_result_t* result) {
    uint32_t byte_count = (ctx->aligned_bits + 7) / 8;
    
    /* Allocate output buffers */
    result->data = (uint8_t*)safe_calloc(byte_count, 1);
    result->confidence = (uint8_t*)safe_calloc(ctx->aligned_bits, 1);
    result->bit_stats = (uft_mrv_bit_stats_t*)safe_calloc(ctx->aligned_bits, 
                                                           sizeof(uft_mrv_bit_stats_t));
    
    if (!result->data || !result->confidence || !result->bit_stats) {
        return UFT_MRV_ERR_NOMEM;
    }
    
    result->data_bits = ctx->aligned_bits;
    result->data_bytes = byte_count;
    result->total_bits = ctx->aligned_bits;
    result->stats_count = ctx->aligned_bits;
    
    /* Process each bit */
    for (uint32_t b = 0; b < ctx->aligned_bits; b++) {
        vote_accum_t* acc = &ctx->accumulators[b];
        uft_mrv_bit_stats_t* stat = &result->bit_stats[b];
        
        stat->position = b;
        stat->votes_0 = acc->count_0;
        stat->votes_1 = acc->count_1;
        stat->votes_missing = acc->count_miss;
        
        /* Calculate timing spread */
        if (acc->timing_count > 1) {
            float stddev = calc_stddev(acc->timing_sum, acc->timing_sqsum, acc->timing_count);
            stat->timing_spread = (uint16_t)(stddev + 0.5f);
        }
        
        /* Determine winner and confidence */
        int total_votes = acc->count_0 + acc->count_1;
        uint8_t voted_bit;
        uint8_t confidence;
        uft_mrv_bit_class_t bit_class;
        
        if (total_votes == 0) {
            /* No valid votes */
            voted_bit = 0;
            confidence = 0;
            bit_class = UFT_MRV_BIT_MISSING;
            result->missing_bits++;
        } else {
            int winner_votes = (acc->count_1 > acc->count_0) ? acc->count_1 : acc->count_0;
            voted_bit = (acc->count_1 > acc->count_0) ? 1 : 0;
            
            /* Calculate confidence as percentage of agreement */
            confidence = (uint8_t)((winner_votes * 100) / total_votes);
            
            /* Apply strategy adjustments */
            if (ctx->params.strategy == UFT_MRV_STRATEGY_CONSENSUS) {
                /* Require all votes agree */
                if (winner_votes != total_votes) {
                    confidence = (uint8_t)(confidence * 0.5f);
                }
            } else if (ctx->params.strategy == UFT_MRV_STRATEGY_WEIGHTED) {
                /* Reduce confidence for high timing variance */
                if (stat->timing_spread > ctx->params.timing_tolerance_ns) {
                    confidence = (uint8_t)(confidence * 0.8f);
                }
            }
            
            /* Classify bit */
            if (confidence >= ctx->params.min_confidence) {
                bit_class = voted_bit ? UFT_MRV_BIT_STABLE_1 : UFT_MRV_BIT_STABLE_0;
                result->stable_bits++;
            } else if (confidence >= ctx->params.weak_threshold) {
                bit_class = UFT_MRV_BIT_WEAK;
                result->weak_bits++;
            } else {
                bit_class = UFT_MRV_BIT_WEAK;
                result->weak_bits++;
            }
        }
        
        /* Store results */
        set_bit(result->data, b, voted_bit);
        result->confidence[b] = confidence;
        stat->confidence = confidence;
        stat->class = bit_class;
    }
    
    /* Calculate overall confidence */
    if (result->total_bits > 0) {
        result->overall_confidence = (float)result->stable_bits / result->total_bits;
    }
    
    return UFT_MRV_OK;
}

/**
 * @brief Detect contiguous weak bit regions
 */
static void detect_weak_regions_internal(uft_mrv_result_t* result, 
                                          uint16_t min_run) {
    uint32_t max_regions = 256;
    result->weak_regions = (uft_mrv_weak_region_t*)safe_calloc(max_regions,
                                                                sizeof(uft_mrv_weak_region_t));
    if (!result->weak_regions) return;
    
    uint32_t region_start = 0;
    uint32_t weak_run = 0;
    bool in_weak_region = false;
    
    for (uint32_t b = 0; b <= result->data_bits; b++) {
        bool is_weak = (b < result->data_bits) && 
                       (result->bit_stats[b].class == UFT_MRV_BIT_WEAK);
        
        if (is_weak) {
            if (!in_weak_region) {
                region_start = b;
                in_weak_region = true;
            }
            weak_run++;
        } else {
            if (in_weak_region && weak_run >= min_run) {
                if (result->weak_region_count < max_regions) {
                    uft_mrv_weak_region_t* region = &result->weak_regions[result->weak_region_count];
                    region->start_bit = region_start;
                    region->length = weak_run;
                    region->pattern = UFT_MRV_WEAK_RANDOM;
                    
                    /* Calculate average confidence and bias */
                    uint32_t conf_sum = 0;
                    uint32_t ones = 0;
                    for (uint32_t i = region_start; i < region_start + weak_run; i++) {
                        conf_sum += result->confidence[i];
                        if (get_bit(result->data, i)) ones++;
                    }
                    region->avg_confidence = (uint8_t)(conf_sum / weak_run);
                    region->bias = (uint8_t)((ones * 100) / weak_run);
                    
                    /* Determine pattern type */
                    if (region->bias > 70) {
                        region->pattern = UFT_MRV_WEAK_BIASED_1;
                    } else if (region->bias < 30) {
                        region->pattern = UFT_MRV_WEAK_BIASED_0;
                    }
                    
                    result->weak_region_count++;
                }
            }
            in_weak_region = false;
            weak_run = 0;
        }
    }
}

int uft_mrv_analyze(uft_mrv_context_t* ctx, uft_mrv_result_t** result) {
    if (!ctx || !result) return UFT_MRV_ERR_INVALID;
    if (ctx->rev_count < UFT_MRV_MIN_REVOLUTIONS) return UFT_MRV_ERR_TOO_FEW_REVS;
    
    *result = NULL;
    
    /* Align revolutions */
    align_all_revolutions(ctx);
    
    /* Accumulate votes */
    int ret = accumulate_votes(ctx);
    if (ret != UFT_MRV_OK) return ret;
    
    /* Allocate result */
    uft_mrv_result_t* res = (uft_mrv_result_t*)safe_calloc(1, sizeof(uft_mrv_result_t));
    if (!res) return UFT_MRV_ERR_NOMEM;
    
    /* Perform voting */
    ret = perform_voting(ctx, res);
    if (ret != UFT_MRV_OK) {
        uft_mrv_result_free(res);
        return ret;
    }
    
    /* Detect weak regions */
    detect_weak_regions_internal(res, ctx->params.min_weak_run);
    
    /* Store revolution quality */
    res->rev_quality = (uft_mrv_rev_quality_t*)safe_calloc((size_t)ctx->rev_count,
                                                            sizeof(uft_mrv_rev_quality_t));
    res->rev_count = (uint8_t)ctx->rev_count;
    
    if (res->rev_quality) {
        for (int r = 0; r < ctx->rev_count; r++) {
            res->rev_quality[r].rev_index = (uint8_t)r;
            res->rev_quality[r].quality_score = ctx->revolutions[r].quality;
            res->rev_quality[r].usable = ctx->revolutions[r].valid;
        }
    }
    
    /* Find best revolution */
    res->best_rev = (uint8_t)uft_mrv_find_best_revolution(ctx);
    
    /* Detect copy protection */
    if (ctx->params.detect_protection && res->weak_region_count > 0) {
        uint32_t scheme_id;
        uint8_t match = uft_mrv_match_protection(res->weak_regions, 
                                                  res->weak_region_count,
                                                  &scheme_id);
        if (match > 50) {
            res->has_protection = true;
            res->protection_confidence = match;
            /* Set protection name based on scheme_id */
            const char* names[] = {"Unknown", "V-MAX!", "RapidLok", "CopyLock", "Speedlock"};
            if (scheme_id < sizeof(names)/sizeof(names[0])) {
                strncpy(res->protection_scheme, names[scheme_id], 
                        sizeof(res->protection_scheme) - 1);
            }
        }
    }
    
    ctx->analyzed = true;
    *result = res;
    
    return UFT_MRV_OK;
}

int uft_mrv_analyze_quick(uft_mrv_context_t* ctx, uint8_t* data,
                          size_t data_size, uint32_t* bits_out) {
    if (!ctx || !data || !bits_out) return UFT_MRV_ERR_INVALID;
    if (ctx->rev_count < UFT_MRV_MIN_REVOLUTIONS) return UFT_MRV_ERR_TOO_FEW_REVS;
    
    /* Accumulate votes */
    int ret = accumulate_votes(ctx);
    if (ret != UFT_MRV_OK) return ret;
    
    uint32_t max_bits = (uint32_t)(data_size * 8);
    if (max_bits > ctx->aligned_bits) max_bits = ctx->aligned_bits;
    
    memset(data, 0, data_size);
    
    /* Simple majority voting */
    for (uint32_t b = 0; b < max_bits; b++) {
        vote_accum_t* acc = &ctx->accumulators[b];
        uint8_t voted_bit = (acc->count_1 > acc->count_0) ? 1 : 0;
        set_bit(data, b, voted_bit);
    }
    
    *bits_out = max_bits;
    return UFT_MRV_OK;
}

void uft_mrv_result_free(uft_mrv_result_t* result) {
    if (!result) return;
    
    free(result->data);
    free(result->confidence);
    free(result->bit_stats);
    free(result->weak_regions);
    free(result->rev_quality);
    free(result);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * WEAK BIT ANALYSIS
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_mrv_detect_weak_regions(uft_mrv_context_t* ctx,
                                 uft_mrv_weak_region_t* regions,
                                 int max_regions) {
    if (!ctx || !regions) return 0;
    
    /* Need to run analysis first */
    uft_mrv_result_t* result;
    if (uft_mrv_analyze(ctx, &result) != UFT_MRV_OK) return 0;
    
    int count = (result->weak_region_count < (uint16_t)max_regions) ? 
                result->weak_region_count : (uint16_t)max_regions;
    
    memcpy(regions, result->weak_regions, (size_t)count * sizeof(uft_mrv_weak_region_t));
    
    uft_mrv_result_free(result);
    return count;
}

int uft_mrv_analyze_weak_pattern(uft_mrv_context_t* ctx,
                                  uint32_t start_bit, uint32_t length,
                                  uft_mrv_weak_pattern_t* pattern,
                                  uint8_t* bias) {
    if (!ctx || !pattern || !bias) return UFT_MRV_ERR_INVALID;
    if (ctx->rev_count < 2) return UFT_MRV_ERR_TOO_FEW_REVS;
    
    /* Analyze specific region across revolutions */
    uint32_t ones[UFT_MRV_MAX_REVOLUTIONS] = {0};
    uint32_t total = 0;
    
    for (int r = 0; r < ctx->rev_count; r++) {
        const rev_data_t* rev = &ctx->revolutions[r];
        if (!rev->valid) continue;
        
        for (uint32_t b = start_bit; b < start_bit + length && b < rev->bit_count; b++) {
            if (get_bit(rev->bits, b)) {
                ones[r]++;
            }
            total++;
        }
    }
    
    /* Calculate overall bias */
    uint32_t total_ones = 0;
    for (int r = 0; r < ctx->rev_count; r++) {
        total_ones += ones[r];
    }
    
    if (total == 0) {
        *pattern = UFT_MRV_WEAK_RANDOM;
        *bias = 50;
        return UFT_MRV_OK;
    }
    
    *bias = (uint8_t)((total_ones * 100) / total);
    
    /* Determine pattern */
    if (*bias > 80) {
        *pattern = UFT_MRV_WEAK_BIASED_1;
    } else if (*bias < 20) {
        *pattern = UFT_MRV_WEAK_BIASED_0;
    } else if (*bias > 40 && *bias < 60) {
        *pattern = UFT_MRV_WEAK_RANDOM;
    } else {
        /* Check for periodicity */
        bool periodic = true;
        for (int r = 1; r < ctx->rev_count && periodic; r++) {
            int diff = (int)ones[r] - (int)ones[0];
            if (abs(diff) > (int)(length / 4)) {
                periodic = false;
            }
        }
        *pattern = periodic ? UFT_MRV_WEAK_PERIODIC : UFT_MRV_WEAK_DEGRADED;
    }
    
    return UFT_MRV_OK;
}

bool uft_mrv_is_weak_bit(const uft_mrv_result_t* result, uint32_t bit_pos) {
    if (!result || bit_pos >= result->stats_count) return false;
    return (result->bit_stats[bit_pos].class == UFT_MRV_BIT_WEAK);
}

uint8_t uft_mrv_get_weak_probability(const uft_mrv_result_t* result,
                                      uint32_t bit_pos) {
    if (!result || bit_pos >= result->stats_count) return 50;
    
    const uft_mrv_bit_stats_t* stat = &result->bit_stats[bit_pos];
    int total = stat->votes_0 + stat->votes_1;
    if (total == 0) return 50;
    
    return (uint8_t)((stat->votes_1 * 100) / total);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * COPY PROTECTION DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_mrv_detect_protection(const uft_mrv_result_t* result,
                                char* scheme_name, size_t name_size,
                                uint8_t* confidence) {
    if (!result) return false;
    
    if (result->has_protection) {
        if (scheme_name && name_size > 0) {
            strncpy(scheme_name, result->protection_scheme, name_size - 1);
            scheme_name[name_size - 1] = '\0';
        }
        if (confidence) {
            *confidence = result->protection_confidence;
        }
        return true;
    }
    
    return false;
}

uint8_t uft_mrv_match_protection(const uft_mrv_weak_region_t* regions,
                                  int region_count, uint32_t* scheme_id) {
    if (!regions || region_count == 0) {
        if (scheme_id) *scheme_id = 0;
        return 0;
    }
    
    /* Known protection patterns */
    
    /* V-MAX! (C64): Multiple short weak regions (8-16 bits) at specific intervals */
    int vmax_match = 0;
    for (int i = 0; i < region_count; i++) {
        if (regions[i].length >= 8 && regions[i].length <= 32 &&
            regions[i].pattern == UFT_MRV_WEAK_RANDOM) {
            vmax_match++;
        }
    }
    if (vmax_match >= 3) {
        if (scheme_id) *scheme_id = 1;
        return (uint8_t)(60 + vmax_match * 5);
    }
    
    /* RapidLok (C64): Long weak region (>64 bits) with 50% bias */
    for (int i = 0; i < region_count; i++) {
        if (regions[i].length >= 64 && regions[i].bias >= 40 && regions[i].bias <= 60) {
            if (scheme_id) *scheme_id = 2;
            return 75;
        }
    }
    
    /* CopyLock/Speedlock (Amiga): Weak bits in track gap area */
    /* These typically appear at high bit positions (near track end) */
    for (int i = 0; i < region_count; i++) {
        if (regions[i].start_bit > 80000 && regions[i].length >= 16) {
            if (scheme_id) *scheme_id = 3;
            return 70;
        }
    }
    
    if (scheme_id) *scheme_id = 0;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * QUALITY ASSESSMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_mrv_eval_revolution(uft_mrv_context_t* ctx, int rev_index,
                            uft_mrv_rev_quality_t* quality) {
    if (!ctx || !quality) return UFT_MRV_ERR_INVALID;
    if (rev_index < 0 || rev_index >= ctx->rev_count) return UFT_MRV_ERR_INVALID;
    
    const rev_data_t* rev = &ctx->revolutions[rev_index];
    
    quality->rev_index = (uint8_t)rev_index;
    quality->usable = rev->valid;
    quality->quality_score = rev->quality;
    
    /* Count timing anomalies */
    if (rev->timing) {
        uint32_t anomalies = 0;
        for (uint32_t i = 1; i < rev->bit_count; i++) {
            int diff = (int)rev->timing[i] - (int)rev->timing[i-1];
            if (abs(diff) > 1000) {  /* >1µs jump */
                anomalies++;
            }
        }
        quality->timing_errors = anomalies;
    }
    
    return UFT_MRV_OK;
}

int uft_mrv_find_best_revolution(uft_mrv_context_t* ctx) {
    if (!ctx || ctx->rev_count == 0) return -1;
    
    int best = 0;
    float best_score = -1.0f;
    
    for (int i = 0; i < ctx->rev_count; i++) {
        if (ctx->revolutions[i].valid && ctx->revolutions[i].quality > best_score) {
            best_score = ctx->revolutions[i].quality;
            best = i;
        }
    }
    
    return best;
}

float uft_mrv_get_quality(const uft_mrv_result_t* result) {
    if (!result) return 0.0f;
    return result->overall_confidence;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * OUTPUT GENERATION
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_mrv_to_ir_track(const uft_mrv_result_t* result, uft_ir_track_t** track) {
    if (!result || !track) return UFT_MRV_ERR_INVALID;
    
    uft_ir_track_t* ir_track = uft_ir_track_create(0, 0);
    if (!ir_track) return UFT_MRV_ERR_NOMEM;
    
    /* Set flags */
    ir_track->flags |= UFT_IR_TF_MULTI_REV_FUSED;
    
    /* Copy decoded data if present */
    if (result->data && result->data_bytes > 0) {
        ir_track->decoded_data = (uint8_t*)malloc(result->data_bytes);
        if (ir_track->decoded_data) {
            memcpy(ir_track->decoded_data, result->data, result->data_bytes);
            ir_track->decoded_size = result->data_bytes;
        }
    }
    
    /* Set quality based on analysis */
    if (result->overall_confidence >= 0.95f) {
        ir_track->quality = UFT_IR_QUAL_PERFECT;
    } else if (result->overall_confidence >= 0.85f) {
        ir_track->quality = UFT_IR_QUAL_GOOD;
    } else if (result->overall_confidence >= 0.70f) {
        ir_track->quality = UFT_IR_QUAL_DEGRADED;
    } else {
        ir_track->quality = UFT_IR_QUAL_MARGINAL;
    }
    
    ir_track->quality_score = (uint8_t)(result->overall_confidence * 100);
    
    /* Copy weak regions */
    if (result->weak_region_count > 0 && result->weak_regions) {
        ir_track->weak_region_count = (result->weak_region_count > UFT_IR_MAX_WEAK_REGIONS) ?
                                       UFT_IR_MAX_WEAK_REGIONS : result->weak_region_count;
        
        for (int i = 0; i < ir_track->weak_region_count; i++) {
            ir_track->weak_regions[i].start_bit = result->weak_regions[i].start_bit;
            ir_track->weak_regions[i].length_bits = result->weak_regions[i].length;
            ir_track->weak_regions[i].confidence = result->weak_regions[i].avg_confidence;
            
            switch (result->weak_regions[i].pattern) {
                case UFT_MRV_WEAK_RANDOM:
                    ir_track->weak_regions[i].pattern = UFT_IR_WEAK_RANDOM;
                    break;
                case UFT_MRV_WEAK_BIASED_0:
                    ir_track->weak_regions[i].pattern = UFT_IR_WEAK_STUCK_0;
                    break;
                case UFT_MRV_WEAK_BIASED_1:
                    ir_track->weak_regions[i].pattern = UFT_IR_WEAK_STUCK_1;
                    break;
                default:
                    ir_track->weak_regions[i].pattern = UFT_IR_WEAK_RANDOM;
                    break;
            }
        }
    }
    
    /* Set protection flags */
    if (result->has_protection) {
        ir_track->flags |= UFT_IR_TF_PROTECTED;
        
        /* Add protection marker */
        if (ir_track->protection_count < UFT_IR_MAX_PROTECTIONS) {
            uft_ir_protection_t* prot = &ir_track->protections[ir_track->protection_count++];
            strncpy(prot->name, result->protection_scheme, sizeof(prot->name) - 1);
            prot->severity = (result->protection_confidence > 80) ? 3 : 2;
        }
    }
    
    *track = ir_track;
    return UFT_MRV_OK;
}

char* uft_mrv_to_json(const uft_mrv_result_t* result, bool include_bit_stats) {
    if (!result) return NULL;
    
    /* Estimate JSON size */
    size_t size = 4096;
    if (include_bit_stats) {
        size += result->stats_count * 64;  /* ~64 bytes per bit stat */
    }
    
    char* json = (char*)malloc(size);
    if (!json) return NULL;
    
    int pos = 0;
    pos += snprintf(json + pos, size - (size_t)pos, "{\n");
    pos += snprintf(json + pos, size - (size_t)pos, "  \"total_bits\": %u,\n", result->total_bits);
    pos += snprintf(json + pos, size - (size_t)pos, "  \"stable_bits\": %u,\n", result->stable_bits);
    pos += snprintf(json + pos, size - (size_t)pos, "  \"weak_bits\": %u,\n", result->weak_bits);
    pos += snprintf(json + pos, size - (size_t)pos, "  \"missing_bits\": %u,\n", result->missing_bits);
    pos += snprintf(json + pos, size - (size_t)pos, "  \"overall_confidence\": %.4f,\n", 
                    (double)result->overall_confidence);
    pos += snprintf(json + pos, size - (size_t)pos, "  \"revolutions_used\": %d,\n", result->rev_count);
    pos += snprintf(json + pos, size - (size_t)pos, "  \"best_revolution\": %d,\n", result->best_rev);
    
    /* Weak regions */
    pos += snprintf(json + pos, size - (size_t)pos, "  \"weak_regions\": [\n");
    for (int i = 0; i < result->weak_region_count; i++) {
        const uft_mrv_weak_region_t* reg = &result->weak_regions[i];
        pos += snprintf(json + pos, size - (size_t)pos, 
                        "    {\"start\": %u, \"length\": %u, \"pattern\": \"%s\", \"bias\": %d}%s\n",
                        reg->start_bit, reg->length,
                        uft_mrv_weak_pattern_name(reg->pattern),
                        reg->bias,
                        (i < result->weak_region_count - 1) ? "," : "");
    }
    pos += snprintf(json + pos, size - (size_t)pos, "  ],\n");
    
    /* Protection */
    pos += snprintf(json + pos, size - (size_t)pos, "  \"has_protection\": %s,\n",
                    result->has_protection ? "true" : "false");
    if (result->has_protection) {
        pos += snprintf(json + pos, size - (size_t)pos, "  \"protection_scheme\": \"%s\",\n",
                        result->protection_scheme);
        pos += snprintf(json + pos, size - (size_t)pos, "  \"protection_confidence\": %d,\n",
                        result->protection_confidence);
    }
    
    /* Optional bit stats */
    if (include_bit_stats && result->bit_stats) {
        pos += snprintf(json + pos, size - (size_t)pos, "  \"bit_stats\": [\n");
        for (uint32_t i = 0; i < result->stats_count && (size_t)pos < size - 100; i++) {
            const uft_mrv_bit_stats_t* stat = &result->bit_stats[i];
            if (stat->class == UFT_MRV_BIT_WEAK || stat->class == UFT_MRV_BIT_MISSING) {
                pos += snprintf(json + pos, size - (size_t)pos,
                                "    {\"pos\": %u, \"class\": \"%s\", \"conf\": %d, \"v0\": %d, \"v1\": %d}%s\n",
                                stat->position, uft_mrv_bit_class_name(stat->class),
                                stat->confidence, stat->votes_0, stat->votes_1,
                                (i < result->stats_count - 1) ? "," : "");
            }
        }
        pos += snprintf(json + pos, size - (size_t)pos, "  ],\n");
    }
    
    /* Remove trailing comma */
    if (pos > 2 && json[pos-2] == ',') {
        json[pos-2] = '\n';
        pos--;
    }
    
    pos += snprintf(json + pos, size - (size_t)pos, "}\n");
    
    return json;
}

char* uft_mrv_to_summary(const uft_mrv_result_t* result) {
    if (!result) return NULL;
    
    char* summary = (char*)malloc(2048);
    if (!summary) return NULL;
    
    int pos = 0;
    
    pos += snprintf(summary + pos, 2048 - (size_t)pos,
                    "╔══════════════════════════════════════════════════════════════╗\n");
    pos += snprintf(summary + pos, 2048 - (size_t)pos,
                    "║            MULTI-REVOLUTION VOTING ANALYSIS                  ║\n");
    pos += snprintf(summary + pos, 2048 - (size_t)pos,
                    "╠══════════════════════════════════════════════════════════════╣\n");
    pos += snprintf(summary + pos, 2048 - (size_t)pos,
                    "║  Revolutions Analyzed: %-5d  Best Revolution: %-5d         ║\n",
                    result->rev_count, result->best_rev);
    pos += snprintf(summary + pos, 2048 - (size_t)pos,
                    "║  Total Bits: %-10u  Overall Confidence: %.1f%%          ║\n",
                    result->total_bits, (double)result->overall_confidence * 100);
    pos += snprintf(summary + pos, 2048 - (size_t)pos,
                    "╠══════════════════════════════════════════════════════════════╣\n");
    pos += snprintf(summary + pos, 2048 - (size_t)pos,
                    "║  Stable Bits:  %-10u (%.1f%%)                            ║\n",
                    result->stable_bits, 
                    (result->total_bits > 0) ? (double)result->stable_bits * 100 / result->total_bits : 0.0);
    pos += snprintf(summary + pos, 2048 - (size_t)pos,
                    "║  Weak Bits:    %-10u (%.1f%%)                            ║\n",
                    result->weak_bits,
                    (result->total_bits > 0) ? (double)result->weak_bits * 100 / result->total_bits : 0.0);
    pos += snprintf(summary + pos, 2048 - (size_t)pos,
                    "║  Missing Bits: %-10u (%.1f%%)                            ║\n",
                    result->missing_bits,
                    (result->total_bits > 0) ? (double)result->missing_bits * 100 / result->total_bits : 0.0);
    
    if (result->weak_region_count > 0) {
        pos += snprintf(summary + pos, 2048 - (size_t)pos,
                        "╠══════════════════════════════════════════════════════════════╣\n");
        pos += snprintf(summary + pos, 2048 - (size_t)pos,
                        "║  Weak Regions Detected: %-5d                                ║\n",
                        result->weak_region_count);
        for (int i = 0; i < result->weak_region_count && i < 5; i++) {
            const uft_mrv_weak_region_t* reg = &result->weak_regions[i];
            pos += snprintf(summary + pos, 2048 - (size_t)pos,
                            "║    #%d: bits %u-%u (%s, bias %d%%)                    ║\n",
                            i + 1, reg->start_bit, reg->start_bit + reg->length,
                            uft_mrv_weak_pattern_name(reg->pattern), reg->bias);
        }
    }
    
    if (result->has_protection) {
        pos += snprintf(summary + pos, 2048 - (size_t)pos,
                        "╠══════════════════════════════════════════════════════════════╣\n");
        pos += snprintf(summary + pos, 2048 - (size_t)pos,
                        "║  ⚠ COPY PROTECTION DETECTED: %-20s (%d%%)    ║\n",
                        result->protection_scheme, result->protection_confidence);
    }
    
    pos += snprintf(summary + pos, 2048 - (size_t)pos,
                    "╚══════════════════════════════════════════════════════════════╝\n");
    
    return summary;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITIES
 * ═══════════════════════════════════════════════════════════════════════════ */

const char* uft_mrv_bit_class_name(uft_mrv_bit_class_t class) {
    switch (class) {
        case UFT_MRV_BIT_UNKNOWN:    return "unknown";
        case UFT_MRV_BIT_STABLE_0:   return "stable_0";
        case UFT_MRV_BIT_STABLE_1:   return "stable_1";
        case UFT_MRV_BIT_WEAK:       return "weak";
        case UFT_MRV_BIT_MISSING:    return "missing";
        case UFT_MRV_BIT_EXTRA:      return "extra";
        case UFT_MRV_BIT_PROTECTED:  return "protected";
        default:                     return "invalid";
    }
}

const char* uft_mrv_strategy_name(uft_mrv_strategy_t strategy) {
    switch (strategy) {
        case UFT_MRV_STRATEGY_MAJORITY:  return "majority";
        case UFT_MRV_STRATEGY_WEIGHTED:  return "weighted";
        case UFT_MRV_STRATEGY_CONSENSUS: return "consensus";
        case UFT_MRV_STRATEGY_BEST_CRC:  return "best_crc";
        case UFT_MRV_STRATEGY_ADAPTIVE:  return "adaptive";
        default:                         return "unknown";
    }
}

const char* uft_mrv_weak_pattern_name(uft_mrv_weak_pattern_t pattern) {
    switch (pattern) {
        case UFT_MRV_WEAK_RANDOM:    return "random";
        case UFT_MRV_WEAK_BIASED_0:  return "biased_0";
        case UFT_MRV_WEAK_BIASED_1:  return "biased_1";
        case UFT_MRV_WEAK_PERIODIC:  return "periodic";
        case UFT_MRV_WEAK_DEGRADED:  return "degraded";
        default:                     return "unknown";
    }
}

const char* uft_mrv_strerror(int err) {
    switch (err) {
        case UFT_MRV_OK:              return "Success";
        case UFT_MRV_ERR_INVALID:     return "Invalid parameter";
        case UFT_MRV_ERR_NOMEM:       return "Out of memory";
        case UFT_MRV_ERR_NO_DATA:     return "No data available";
        case UFT_MRV_ERR_TOO_FEW_REVS:return "Too few revolutions (minimum 2)";
        case UFT_MRV_ERR_OVERFLOW:    return "Buffer overflow";
        case UFT_MRV_ERR_ALIGNMENT:   return "Alignment error";
        default:                      return "Unknown error";
    }
}
