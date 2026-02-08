/**
 * @file uft_crosstalk_filter.c
 * @brief Crosstalk Filter Implementation
 * 
 * CLEAN-ROOM implementation based on observable requirements.
 */

#include "uft/hardware/uft_crosstalk_filter.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ============================================================================
 * Configuration Functions
 * ============================================================================ */

void uft_crosstalk_config_init(uft_crosstalk_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    
    config->sides_enabled = UFT_CT_SIDE_BOTH;
    config->mode = UFT_CT_MODE_FILTER;
    config->threshold = UFT_CT_THRESHOLD_MED;
    config->enabled = true;
    
    config->window_tracks = 1;
    config->amplitude_weight = 0.7;
    config->phase_weight = 0.3;
    config->adaptive = true;
}

/* ============================================================================
 * State Functions
 * ============================================================================ */

void uft_crosstalk_state_init(
    uft_crosstalk_state_t *state,
    const uft_crosstalk_config_t *config
) {
    if (!state) return;
    
    memset(state, 0, sizeof(*state));
    
    if (config) {
        state->config = *config;
    } else {
        uft_crosstalk_config_init(&state->config);
    }
}

void uft_crosstalk_state_free(uft_crosstalk_state_t *state) {
    if (!state) return;
    
    free(state->ref_track_minus);
    free(state->ref_track_plus);
    
    state->ref_track_minus = NULL;
    state->ref_track_plus = NULL;
    state->ref_size = 0;
}

void uft_crosstalk_set_reference(
    uft_crosstalk_state_t *state,
    const uint8_t *track_minus,
    const uint8_t *track_plus,
    size_t size
) {
    if (!state) return;
    
    /* Free old references */
    free(state->ref_track_minus);
    free(state->ref_track_plus);
    state->ref_track_minus = NULL;
    state->ref_track_plus = NULL;
    
    state->ref_size = size;
    
    /* Copy new references */
    if (track_minus && size > 0) {
        state->ref_track_minus = (uint8_t *)malloc(size);
        if (state->ref_track_minus) {
            memcpy(state->ref_track_minus, track_minus, size);
        }
    }
    
    if (track_plus && size > 0) {
        state->ref_track_plus = (uint8_t *)malloc(size);
        if (state->ref_track_plus) {
            memcpy(state->ref_track_plus, track_plus, size);
        }
    }
}

void uft_crosstalk_set_track(
    uft_crosstalk_state_t *state,
    uint8_t track,
    uint8_t head
) {
    if (!state) return;
    
    state->current_track = track;
    state->current_head = head;
}

/* ============================================================================
 * Analysis Functions
 * ============================================================================ */

double uft_crosstalk_correlate(
    const uint8_t *data1,
    const uint8_t *data2,
    size_t size,
    int offset
) {
    if (!data1 || !data2 || size == 0) return 0;
    
    /* Calculate correlation with offset */
    double sum_xy = 0, sum_x = 0, sum_y = 0;
    double sum_x2 = 0, sum_y2 = 0;
    size_t count = 0;
    
    for (size_t i = 0; i < size; i++) {
        int j = (int)i + offset;
        if (j < 0 || j >= (int)size) continue;
        
        double x = data1[i];
        double y = data2[j];
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
        sum_y2 += y * y;
        count++;
    }
    
    if (count == 0) return 0;
    
    double n = (double)count;
    double numerator = n * sum_xy - sum_x * sum_y;
    double denominator = sqrt((n * sum_x2 - sum_x * sum_x) * 
                              (n * sum_y2 - sum_y * sum_y));
    
    if (denominator < 1e-10) return 0;
    
    return numerator / denominator;
}

int uft_crosstalk_find_offset(
    const uint8_t *data1,
    const uint8_t *data2,
    size_t size,
    int max_offset,
    double *best_corr
) {
    if (!data1 || !data2) return 0;
    
    int best_offset = 0;
    double max_corr = -2.0;
    
    for (int off = -max_offset; off <= max_offset; off++) {
        double corr = uft_crosstalk_correlate(data1, data2, size, off);
        if (corr > max_corr) {
            max_corr = corr;
            best_offset = off;
        }
    }
    
    if (best_corr) *best_corr = max_corr;
    return best_offset;
}

double uft_crosstalk_estimate_level(
    const uint8_t *track_data,
    const uint8_t *ref_data,
    size_t position,
    size_t window
) {
    if (!track_data || !ref_data) return 0;
    
    /* Compare local patterns */
    size_t start = (position >= window/2) ? position - window/2 : 0;
    size_t end = position + window/2;
    
    double match_count = 0;
    double total_count = 0;
    
    for (size_t i = start; i < end; i++) {
        /* Simple XOR-based similarity */
        uint8_t diff = track_data[i] ^ ref_data[i];
        
        /* Count matching bits */
        int matching = 8 - __builtin_popcount(diff);
        match_count += matching;
        total_count += 8;
    }
    
    if (total_count == 0) return 0;
    
    /* Higher match = higher crosstalk */
    double similarity = match_count / total_count;
    
    /* Normalize: 50% match is baseline (random), above indicates crosstalk */
    double crosstalk = (similarity - 0.5) * 2.0;
    if (crosstalk < 0) crosstalk = 0;
    if (crosstalk > 1) crosstalk = 1;
    
    return crosstalk;
}

/* ============================================================================
 * Detection and Filtering
 * ============================================================================ */

int uft_crosstalk_detect(
    uft_crosstalk_state_t *state,
    const uint8_t *track_data,
    size_t data_size,
    uft_crosstalk_result_t *result
) {
    if (!state || !track_data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->track = state->current_track;
    result->head = state->current_head;
    
    if (!state->config.enabled || 
        state->config.mode == UFT_CT_MODE_OFF) {
        return 0;
    }
    
    /* Check if we have reference data */
    bool have_ref = (state->ref_track_minus || state->ref_track_plus);
    if (!have_ref) {
        return 0;
    }
    
    /* Analyze track in windows */
    size_t window = 64;  /* Bytes per analysis window */
    size_t detected = 0;
    double max_level = 0;
    double sum_level = 0;
    
    for (size_t pos = 0; pos < data_size; pos += window) {
        double level_minus = 0, level_plus = 0;
        
        if (state->ref_track_minus && pos < state->ref_size) {
            level_minus = uft_crosstalk_estimate_level(
                track_data, state->ref_track_minus, pos, window
            );
        }
        
        if (state->ref_track_plus && pos < state->ref_size) {
            level_plus = uft_crosstalk_estimate_level(
                track_data, state->ref_track_plus, pos, window
            );
        }
        
        double level = (level_minus > level_plus) ? level_minus : level_plus;
        
        if (level > state->config.threshold) {
            detected++;
            if (level > max_level) max_level = level;
            sum_level += level;
        }
        
        result->points_analyzed++;
        state->total_analyzed++;
    }
    
    result->points_detected = (uint32_t)detected;
    result->max_crosstalk_level = max_level;
    result->avg_crosstalk_level = (detected > 0) ? sum_level / detected : 0;
    result->crosstalk_percentage = 100.0 * detected / result->points_analyzed;
    
    state->total_detected += detected;
    
    /* Determine primary source */
    if (state->ref_track_minus && !state->ref_track_plus) {
        result->primary_source_delta = -1;
    } else if (state->ref_track_plus && !state->ref_track_minus) {
        result->primary_source_delta = 1;
    } else {
        /* Both available - check which has higher correlation */
        double corr_minus = uft_crosstalk_correlate(
            track_data, state->ref_track_minus, 
            (data_size < state->ref_size) ? data_size : state->ref_size, 0
        );
        double corr_plus = uft_crosstalk_correlate(
            track_data, state->ref_track_plus,
            (data_size < state->ref_size) ? data_size : state->ref_size, 0
        );
        result->primary_source_delta = (corr_minus > corr_plus) ? -1 : 1;
    }
    
    return 0;
}

size_t uft_crosstalk_filter(
    uft_crosstalk_state_t *state,
    uint8_t *track_data,
    size_t data_size,
    uft_crosstalk_result_t *result
) {
    if (!state || !track_data || !result) return 0;
    
    if (!state->config.enabled || 
        state->config.mode < UFT_CT_MODE_FILTER) {
        return 0;
    }
    
    /* First detect */
    uft_crosstalk_detect(state, track_data, data_size, result);
    
    if (result->points_detected == 0) {
        return 0;
    }
    
    /* Filter by subtracting crosstalk contribution */
    size_t window = 64;
    size_t filtered = 0;
    
    const uint8_t *ref = (result->primary_source_delta < 0) 
                        ? state->ref_track_minus 
                        : state->ref_track_plus;
    
    if (!ref) return 0;
    
    for (size_t pos = 0; pos < data_size && pos < state->ref_size; pos += window) {
        double level = uft_crosstalk_estimate_level(track_data, ref, pos, window);
        
        if (level > state->config.threshold) {
            /* Apply filtering: reduce contribution from reference */
            double factor = level * 0.5;  /* Partial removal */
            
            for (size_t i = pos; i < pos + window && i < data_size; i++) {
                /* Simple subtraction-based filtering */
                int16_t val = track_data[i];
                int16_t ref_val = ref[i];
                
                val = val - (int16_t)(ref_val * factor);
                if (val < 0) val = 0;
                if (val > 255) val = 255;
                
                track_data[i] = (uint8_t)val;
            }
            
            filtered++;
        }
    }
    
    result->points_filtered = (uint32_t)filtered;
    state->total_filtered += filtered;
    
    return filtered;
}

int uft_crosstalk_process(
    uft_crosstalk_state_t *state,
    uint8_t *track_data,
    size_t data_size,
    uft_crosstalk_result_t *result
) {
    if (!state || !track_data || !result) return -1;
    
    /* Store quality before */
    result->quality_before = 100;  /* Placeholder */
    
    /* Run detection and optional filtering */
    if (state->config.mode >= UFT_CT_MODE_FILTER) {
        uft_crosstalk_filter(state, track_data, data_size, result);
    } else {
        uft_crosstalk_detect(state, track_data, data_size, result);
    }
    
    /* Store quality after */
    result->quality_after = 100 - (uint8_t)(result->crosstalk_percentage);
    if (result->points_filtered > 0) {
        result->quality_after += 5;  /* Bonus for successful filtering */
        if (result->quality_after > 100) result->quality_after = 100;
    }
    
    return 0;
}

/* ============================================================================
 * Flux-Level Functions
 * ============================================================================ */

int uft_crosstalk_detect_flux(
    uft_crosstalk_state_t *state,
    const uint32_t *flux_data,
    size_t flux_count,
    const uint32_t *ref_minus,
    const uint32_t *ref_plus,
    size_t ref_count,
    uft_crosstalk_result_t *result
) {
    if (!state || !flux_data || !result) return -1;
    
    (void)ref_minus;
    (void)ref_plus;
    (void)ref_count;
    
    memset(result, 0, sizeof(*result));
    result->track = state->current_track;
    result->head = state->current_head;
    result->points_analyzed = (uint32_t)flux_count;
    
    /* Flux-level crosstalk detection would analyze timing variations */
    /* For now, just mark as analyzed without detection */
    
    return 0;
}

size_t uft_crosstalk_filter_flux(
    uft_crosstalk_state_t *state,
    uint32_t *flux_data,
    size_t flux_count,
    uft_crosstalk_result_t *result
) {
    if (!state || !flux_data || !result) return 0;
    
    (void)flux_count;
    
    /* Flux-level filtering would adjust timing values */
    /* For now, return 0 (no filtering applied) */
    
    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

void uft_crosstalk_get_stats(
    const uft_crosstalk_state_t *state,
    uint64_t *analyzed,
    uint64_t *detected,
    uint64_t *filtered
) {
    if (!state) return;
    
    if (analyzed) *analyzed = state->total_analyzed;
    if (detected) *detected = state->total_detected;
    if (filtered) *filtered = state->total_filtered;
}

void uft_crosstalk_reset_stats(uft_crosstalk_state_t *state) {
    if (!state) return;
    
    state->total_analyzed = 0;
    state->total_detected = 0;
    state->total_filtered = 0;
}

size_t uft_crosstalk_result_to_json(
    const uft_crosstalk_result_t *result,
    char *buffer,
    size_t size
) {
    if (!result) return 0;
    
    size_t offset = 0;
    
    #define APPEND(...) do { \
        int n = snprintf(buffer ? buffer + offset : NULL, \
                        buffer ? size - offset : 0, __VA_ARGS__); \
        if (n > 0) offset += n; \
    } while(0)
    
    APPEND("{\n");
    APPEND("  \"track\": %u,\n", result->track);
    APPEND("  \"head\": %u,\n", result->head);
    APPEND("  \"points_analyzed\": %u,\n", result->points_analyzed);
    APPEND("  \"points_detected\": %u,\n", result->points_detected);
    APPEND("  \"points_filtered\": %u,\n", result->points_filtered);
    APPEND("  \"max_crosstalk_level\": %.4f,\n", result->max_crosstalk_level);
    APPEND("  \"avg_crosstalk_level\": %.4f,\n", result->avg_crosstalk_level);
    APPEND("  \"crosstalk_percentage\": %.2f,\n", result->crosstalk_percentage);
    APPEND("  \"primary_source_delta\": %d,\n", result->primary_source_delta);
    APPEND("  \"quality_before\": %u,\n", result->quality_before);
    APPEND("  \"quality_after\": %u\n", result->quality_after);
    APPEND("}\n");
    
    #undef APPEND
    
    return offset;
}
