/**
 * @file uft_auto_trim.c
 * @brief Automatic Track Trimming Implementation
 * 
 * @copyright MIT License
 */

#include "uft/flux/uft_auto_trim.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_trim_options_init(uft_trim_options_t *opts) {
    if (!opts) return;
    
    opts->min_overlap = UFT_TRIM_MIN_OVERLAP;
    opts->max_overlap = UFT_TRIM_MAX_OVERLAP;
    opts->min_correlation = UFT_TRIM_MIN_CORRELATION;
    opts->window_size = UFT_TRIM_CORRELATION_WINDOW;
    opts->use_index_pulse = true;
    opts->rpm = 300.0;
    opts->data_rate = 250000.0;  /* MFM DD */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Bit Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_trim_copy_bits(uint8_t *dst, size_t dst_pos,
                        const uint8_t *src, size_t src_pos,
                        size_t count) {
    for (size_t i = 0; i < count; i++) {
        int bit = uft_trim_get_bit(src, src_pos + i);
        uft_trim_set_bit(dst, dst_pos + i, bit);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Correlation
 * ═══════════════════════════════════════════════════════════════════════════════ */

double uft_trim_correlate(const uint8_t *data, size_t bit_length,
                          size_t pos1, size_t pos2, size_t window) {
    if (!data || pos1 + window > bit_length || pos2 + window > bit_length) {
        return 0.0;
    }
    
    size_t matches = 0;
    for (size_t i = 0; i < window; i++) {
        int b1 = uft_trim_get_bit(data, pos1 + i);
        int b2 = uft_trim_get_bit(data, pos2 + i);
        if (b1 == b2) matches++;
    }
    
    return (double)matches / (double)window;
}

/**
 * @brief Calculate sliding correlation score
 * 
 * Uses multiple windows to get robust correlation.
 */
static double calc_robust_correlation(const uint8_t *data, size_t bit_length,
                                      size_t pos1, size_t pos2, size_t window) {
    double total = 0.0;
    int samples = 0;
    
    /* Sample at multiple offsets */
    for (size_t offset = 0; offset < window * 4 && pos1 + offset + window <= bit_length 
         && pos2 + offset + window <= bit_length; offset += window / 2) {
        total += uft_trim_correlate(data, bit_length, pos1 + offset, pos2 + offset, window);
        samples++;
    }
    
    return (samples > 0) ? (total / samples) : 0.0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Length Estimation
 * ═══════════════════════════════════════════════════════════════════════════════ */

size_t uft_trim_expected_length(double data_rate, double rpm, double sample_rate) {
    /* Time for one revolution in seconds */
    double revolution_time = 60.0 / rpm;
    
    /* Number of samples per revolution */
    return (size_t)(sample_rate * revolution_time);
}

size_t uft_trim_index_length(const uft_trim_track_t *track, int revolution) {
    if (!track || !track->index_positions || track->index_count < 2) {
        return 0;
    }
    
    if (revolution < 0 || (size_t)(revolution + 1) >= track->index_count) {
        return 0;
    }
    
    return track->index_positions[revolution + 1] - track->index_positions[revolution];
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Overlap Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_trim_detect_overlap(const uint8_t *data, size_t bit_length,
                            size_t expected,
                            size_t *overlap_start, size_t *overlap_length) {
    if (!data || bit_length == 0) return -1;
    
    /* If we don't have expected length, estimate based on common formats */
    if (expected == 0) {
        /* Try common MFM DD at 4MHz sampling: ~200000 bits per track */
        expected = 200000;
    }
    
    /* Search around expected length */
    size_t search_start = (expected > 10000) ? (expected - 10000) : 0;
    size_t search_end = expected + 50000;
    if (search_end > bit_length) search_end = bit_length;
    
    double best_corr = 0.0;
    size_t best_pos = 0;
    
    /* Coarse search first */
    for (size_t pos = search_start; pos < search_end; pos += 100) {
        if (pos + 1000 > bit_length) break;
        
        double corr = calc_robust_correlation(data, bit_length, 0, pos, 64);
        if (corr > best_corr) {
            best_corr = corr;
            best_pos = pos;
        }
    }
    
    if (best_corr < 0.7) return -1;
    
    /* Fine search around best position */
    size_t fine_start = (best_pos > 500) ? (best_pos - 500) : 0;
    size_t fine_end = best_pos + 500;
    if (fine_end > search_end) fine_end = search_end;
    
    for (size_t pos = fine_start; pos < fine_end; pos++) {
        if (pos + 1000 > bit_length) break;
        
        double corr = calc_robust_correlation(data, bit_length, 0, pos, 64);
        if (corr > best_corr) {
            best_corr = corr;
            best_pos = pos;
        }
    }
    
    if (best_corr < 0.8) return -1;
    
    if (overlap_start) *overlap_start = best_pos;
    if (overlap_length) *overlap_length = bit_length - best_pos;
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Find Optimal Trim Point
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_trim_find_optimal(const uft_trim_track_t *track,
                          const uft_trim_options_t *opts,
                          uft_trim_result_t *result) {
    if (!track || !track->data || !result) return -1;
    
    uft_trim_options_t default_opts;
    if (!opts) {
        uft_trim_options_init(&default_opts);
        opts = &default_opts;
    }
    
    memset(result, 0, sizeof(*result));
    result->original_length = track->bit_length;
    
    /* Try using index pulses first */
    if (opts->use_index_pulse && track->index_positions && track->index_count >= 2) {
        size_t idx_length = uft_trim_index_length(track, 0);
        if (idx_length > 0 && idx_length < track->bit_length) {
            /* Verify with correlation */
            double corr = calc_robust_correlation(track->data, track->bit_length,
                                                   0, idx_length, opts->window_size);
            if (corr >= opts->min_correlation) {
                result->found = true;
                result->trim_position = idx_length;
                result->trimmed_length = idx_length;
                result->correlation = corr;
                result->overlap_start = idx_length;
                result->overlap_length = track->bit_length - idx_length;
                return 0;
            }
        }
    }
    
    /* Calculate expected length */
    size_t expected = uft_trim_expected_length(opts->data_rate, opts->rpm, 
                                                track->sample_rate);
    if (expected == 0) {
        expected = track->bit_length * 8 / 10;  /* Assume 20% overlap */
    }
    
    /* Detect overlap region */
    size_t overlap_start, overlap_length;
    if (uft_trim_detect_overlap(track->data, track->bit_length, expected,
                                &overlap_start, &overlap_length) != 0) {
        return -1;  /* No overlap found */
    }
    
    /* Fine-tune trim point within overlap region */
    double best_corr = 0.0;
    size_t best_pos = overlap_start;
    
    size_t search_range = (overlap_length > 1000) ? 1000 : overlap_length;
    
    for (size_t offset = 0; offset < search_range; offset++) {
        size_t pos = overlap_start + offset;
        if (pos + opts->window_size * 4 > track->bit_length) break;
        
        /* Check correlation at this trim point */
        double corr = calc_robust_correlation(track->data, track->bit_length,
                                               0, pos, opts->window_size);
        
        /* Also check if bit patterns align well */
        double align_score = uft_trim_correlate(track->data, track->bit_length,
                                                 pos - 32, track->bit_length - 32 - offset,
                                                 64);
        
        double combined = (corr * 0.7) + (align_score * 0.3);
        
        if (combined > best_corr) {
            best_corr = combined;
            best_pos = pos;
        }
    }
    
    if (best_corr < opts->min_correlation) {
        return -1;
    }
    
    result->found = true;
    result->trim_position = best_pos;
    result->trimmed_length = best_pos;
    result->correlation = best_corr;
    result->overlap_start = overlap_start;
    result->overlap_length = overlap_length;
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Apply Trim
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_trim_apply(const uft_trim_track_t *track,
                   const uft_trim_result_t *result,
                   uint8_t **out_data, size_t *out_length) {
    if (!track || !result || !out_data || !out_length) return -1;
    if (!result->found) return -2;
    
    size_t trimmed_bytes = (result->trimmed_length + 7) / 8;
    *out_data = (uint8_t *)malloc(trimmed_bytes);
    if (!*out_data) return -3;
    
    memset(*out_data, 0, trimmed_bytes);
    
    /* Copy trimmed data */
    uft_trim_copy_bits(*out_data, 0, track->data, 0, result->trimmed_length);
    
    *out_length = result->trimmed_length;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Convenience Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_trim_auto(uint8_t *data, size_t *bit_length,
                  const uft_trim_options_t *opts) {
    if (!data || !bit_length || *bit_length == 0) return -1;
    
    uft_trim_track_t track = {
        .data = data,
        .bit_length = *bit_length,
        .index_positions = NULL,
        .index_count = 0,
        .sample_rate = 4000000.0  /* Default 4MHz */
    };
    
    uft_trim_result_t result;
    if (uft_trim_find_optimal(&track, opts, &result) != 0) {
        return -1;
    }
    
    if (!result.found) return -1;
    
    /* Trim in place */
    *bit_length = result.trimmed_length;
    
    return 0;
}
