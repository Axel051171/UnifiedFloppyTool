/**
 * @file uft_track_boundary.c
 * @brief Track boundary and overlap detection implementation
 */

#include "uft_track_boundary.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static inline uint8_t get_bit(const uint8_t *data, size_t pos) {
    return (data[pos / 8] >> (7 - (pos % 8))) & 1;
}

static inline void set_bit(uint8_t *data, size_t pos, uint8_t val) {
    size_t byte_idx = pos / 8;
    uint8_t mask = 0x80 >> (pos % 8);
    if (val) {
        data[byte_idx] |= mask;
    } else {
        data[byte_idx] &= ~mask;
    }
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

void uft_boundary_config_init(uft_boundary_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    
    /* Defaults for MFM DD */
    cfg->expected_rotation_ns = UFT_ROTATION_300RPM_NS;
    cfg->sample_rate = 4e6;
    cfg->bit_rate = 500e3;
    cfg->tolerance = 0.1;  /* 10% */
    
    cfg->match_window_bits = 512;
    cfg->min_match_score = 0.90;
    cfg->has_index_data = false;
}

void uft_boundary_config_mfm_dd(uft_boundary_config_t *cfg) {
    uft_boundary_config_init(cfg);
    /* Defaults are for MFM DD */
}

void uft_boundary_config_mfm_hd(uft_boundary_config_t *cfg) {
    uft_boundary_config_init(cfg);
    cfg->bit_rate = 1e6;
}

void uft_boundary_config_gcr_c64(uft_boundary_config_t *cfg) {
    uft_boundary_config_init(cfg);
    cfg->bit_rate = 250e3;  /* Approximate average */
}

/* ============================================================================
 * Boundary Detection
 * ============================================================================ */

double uft_boundary_compare_bits(const uint8_t *bits1, size_t pos1,
                                 const uint8_t *bits2, size_t pos2,
                                 size_t len_bits) {
    if (!bits1 || !bits2 || len_bits == 0) return 0;
    
    size_t matches = 0;
    for (size_t i = 0; i < len_bits; i++) {
        if (get_bit(bits1, pos1 + i) == get_bit(bits2, pos2 + i)) {
            matches++;
        }
    }
    
    return (double)matches / len_bits;
}

uft_track_boundary_t uft_boundary_from_indices(
    const uint8_t *bits,
    size_t bit_count,
    const uft_index_pulse_t *indices,
    size_t index_count,
    const uft_boundary_config_t *cfg) {
    
    uft_track_boundary_t result = {0};
    
    if (!bits || bit_count == 0 || !indices || index_count == 0) {
        return result;
    }
    
    /* Copy index data */
    result.index_count = (index_count > 4) ? 4 : index_count;
    for (size_t i = 0; i < result.index_count; i++) {
        result.indices[i] = indices[i];
    }
    
    /* Use first two indices as track boundaries */
    if (index_count >= 2) {
        result.start_bit = indices[0].position;
        result.end_bit = indices[1].position;
        result.track_length = result.end_bit - result.start_bit;
        result.used_index_pulse = true;
        result.boundary_confidence = 95;  /* High confidence with index */
        
        /* Check for overlap */
        if (bit_count > result.end_bit) {
            result.has_overlap = true;
            result.overlap_start = result.end_bit;
            result.overlap_length = bit_count - result.end_bit;
        }
    } else {
        /* Only one index - estimate end based on expected rotation */
        double expected_bits = cfg->expected_rotation_ns * 
                              cfg->sample_rate / 1e9;
        result.start_bit = indices[0].position;
        result.end_bit = result.start_bit + (size_t)expected_bits;
        if (result.end_bit > bit_count) {
            result.end_bit = bit_count;
        }
        result.track_length = result.end_bit - result.start_bit;
        result.used_index_pulse = true;
        result.boundary_confidence = 70;  /* Lower confidence with estimate */
    }
    
    return result;
}

uft_track_boundary_t uft_boundary_from_pattern(
    const uint8_t *bits,
    size_t bit_count,
    const uft_boundary_config_t *cfg) {
    
    uft_track_boundary_t result = {0};
    
    if (!bits || bit_count == 0 || !cfg) {
        return result;
    }
    
    /* Calculate expected track length */
    double expected_bits = cfg->expected_rotation_ns * cfg->sample_rate / 1e9;
    size_t min_search = (size_t)(expected_bits * (1.0 - cfg->tolerance));
    size_t max_search = (size_t)(expected_bits * (1.0 + cfg->tolerance));
    
    if (max_search > bit_count) {
        max_search = bit_count;
    }
    
    /* Pattern matching: compare start of track with positions near expected end */
    size_t best_pos = 0;
    double best_score = 0;
    size_t window = cfg->match_window_bits;
    
    for (size_t pos = min_search; pos < max_search && pos + window <= bit_count; pos++) {
        double score = uft_boundary_compare_bits(bits, 0, bits, pos, window);
        
        if (score > best_score) {
            best_score = score;
            best_pos = pos;
        }
    }
    
    /* Check if match is good enough */
    if (best_score >= cfg->min_match_score) {
        result.start_bit = 0;
        result.end_bit = best_pos;
        result.track_length = best_pos;
        result.used_pattern_match = true;
        result.match_score = best_score;
        result.boundary_confidence = (uint8_t)(best_score * 100);
        
        /* Overlap region */
        if (bit_count > best_pos) {
            result.has_overlap = true;
            result.overlap_start = best_pos;
            result.overlap_length = bit_count - best_pos;
        }
    } else {
        /* Fallback to expected length */
        result.start_bit = 0;
        result.end_bit = (size_t)expected_bits;
        if (result.end_bit > bit_count) {
            result.end_bit = bit_count;
        }
        result.track_length = result.end_bit;
        result.boundary_confidence = 30;  /* Low confidence */
        result.match_score = best_score;
    }
    
    return result;
}

uft_track_boundary_t uft_boundary_detect(
    const uint8_t *bits,
    size_t bit_count,
    const uft_index_pulse_t *indices,
    size_t index_count,
    const uft_boundary_config_t *cfg) {
    
    uft_track_boundary_t result = {0};
    uft_boundary_config_t default_cfg;
    
    if (!cfg) {
        uft_boundary_config_init(&default_cfg);
        cfg = &default_cfg;
    }
    
    /* Try index pulses first if available */
    if (indices && index_count > 0) {
        result = uft_boundary_from_indices(bits, bit_count, indices, index_count, cfg);
        
        /* If high confidence, we're done */
        if (result.boundary_confidence >= 80) {
            return result;
        }
    }
    
    /* Try pattern matching */
    uft_track_boundary_t pattern_result = uft_boundary_from_pattern(bits, bit_count, cfg);
    
    /* Use pattern result if it's better */
    if (pattern_result.boundary_confidence > result.boundary_confidence) {
        result = pattern_result;
    }
    
    /* Combine methods if both available */
    if (result.used_index_pulse && pattern_result.used_pattern_match) {
        /* Check if they agree */
        size_t diff = (result.end_bit > pattern_result.end_bit) ?
                      result.end_bit - pattern_result.end_bit :
                      pattern_result.end_bit - result.end_bit;
        
        double expected_bits = cfg->expected_rotation_ns * cfg->sample_rate / 1e9;
        
        if (diff < expected_bits * 0.02) {  /* Within 2% */
            result.boundary_confidence = 98;  /* Very high confidence */
        }
    }
    
    return result;
}

/* ============================================================================
 * Trimming and Splicing
 * ============================================================================ */

size_t uft_boundary_trim(uint8_t *bits,
                         size_t bit_count,
                         const uft_track_boundary_t *boundary) {
    if (!bits || !boundary || !boundary->has_overlap) {
        return bit_count;
    }
    
    /* Simply truncate at end_bit */
    return boundary->end_bit;
}

size_t uft_boundary_find_splice(const uint8_t *bits,
                                const uft_track_boundary_t *boundary) {
    if (!bits || !boundary || !boundary->has_overlap) {
        return boundary ? boundary->end_bit : 0;
    }
    
    /* Find point in overlap where beginning and end match best */
    size_t best_splice = boundary->overlap_start;
    double best_score = 0;
    
    /* Look for sync patterns or high-match regions */
    size_t search_len = boundary->overlap_length;
    if (search_len > 256) search_len = 256;  /* Limit search */
    
    for (size_t i = 0; i < search_len; i++) {
        size_t splice_point = boundary->overlap_start + i;
        
        /* Compare 64 bits at splice with beginning */
        double score = uft_boundary_compare_bits(
            bits, 0,
            bits, splice_point,
            64);
        
        if (score > best_score) {
            best_score = score;
            best_splice = splice_point;
        }
    }
    
    return best_splice;
}

/* ============================================================================
 * Debug Output
 * ============================================================================ */

void uft_boundary_dump(const uft_track_boundary_t *boundary) {
    if (!boundary) {
        printf("Track Boundary: NULL\n");
        return;
    }
    
    printf("=== Track Boundary ===\n");
    printf("Range: %zu - %zu (%zu bits)\n",
           boundary->start_bit, boundary->end_bit, boundary->track_length);
    printf("Confidence: %u%%\n", boundary->boundary_confidence);
    printf("Methods: index=%s pattern=%s\n",
           boundary->used_index_pulse ? "yes" : "no",
           boundary->used_pattern_match ? "yes" : "no");
    
    if (boundary->used_pattern_match) {
        printf("Match score: %.1f%%\n", boundary->match_score * 100);
    }
    
    if (boundary->has_overlap) {
        printf("Overlap: %zu bits at position %zu\n",
               boundary->overlap_length, boundary->overlap_start);
    }
    
    if (boundary->index_count > 0) {
        printf("Index pulses: %zu\n", boundary->index_count);
        for (size_t i = 0; i < boundary->index_count; i++) {
            printf("  [%zu] pos=%zu\n", i, boundary->indices[i].position);
        }
    }
}
