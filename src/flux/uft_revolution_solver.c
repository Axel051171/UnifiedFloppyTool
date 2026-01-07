/**
 * @file uft_revolution_solver.c
 * @brief Multi-Revolution Alignment and Analysis Implementation
 * 
 * CLEAN-ROOM implementation based on observable requirements.
 * No proprietary code copied.
 */

#include "uft/flux/uft_revolution_solver.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Find index pulses from flux data
 */
static size_t find_index_from_flux(
    const uint32_t *flux_samples,
    size_t sample_count,
    double sample_rate_hz,
    double nominal_rpm,
    uint64_t *index_positions,
    size_t max_indexes
) {
    /* Expected samples per revolution */
    double expected_duration_s = 60.0 / nominal_rpm;
    double expected_samples = expected_duration_s * sample_rate_hz;
    double tolerance = expected_samples * 0.1;  /* 10% tolerance */
    
    size_t index_count = 0;
    uint64_t current_pos = 0;
    uint64_t last_index = 0;
    
    /* First index at position 0 */
    if (index_positions && max_indexes > 0) {
        index_positions[0] = 0;
        index_count = 1;
    }
    
    /* Accumulate samples and find revolution boundaries */
    for (size_t i = 0; i < sample_count && index_count < max_indexes; i++) {
        current_pos += flux_samples[i];
        
        double samples_since_index = (double)(current_pos - last_index);
        
        /* Check if we've completed approximately one revolution */
        if (samples_since_index >= expected_samples - tolerance) {
            if (samples_since_index <= expected_samples + tolerance) {
                /* Valid revolution boundary */
                if (index_positions) {
                    index_positions[index_count] = current_pos;
                }
                last_index = current_pos;
                index_count++;
            }
        }
    }
    
    return index_count;
}

/**
 * @brief Calculate variance of array
 */
static double calc_variance(const double *values, size_t count, double mean) {
    if (count < 2) return 0;
    
    double sum_sq = 0;
    for (size_t i = 0; i < count; i++) {
        double diff = values[i] - mean;
        sum_sq += diff * diff;
    }
    
    return sum_sq / (count - 1);
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void uft_revolution_options_init(uft_revolution_options_t *options) {
    if (!options) return;
    
    options->nominal_rpm = UFT_REV_NOMINAL_RPM_300;
    options->tolerance = UFT_REV_DEFAULT_TOLERANCE;
    options->use_index_pulse = true;
    options->allow_missing_index = true;
    options->min_revolutions = UFT_REV_MIN_REVOLUTIONS;
    options->max_revolutions = UFT_REV_MAX_REVOLUTIONS;
}

uft_error_t uft_revolution_solve(
    const uint32_t *flux_samples,
    size_t sample_count,
    const uft_index_data_t *index_data,
    const uft_revolution_options_t *options,
    uft_revolution_result_t *result
) {
    if (!flux_samples || !result || !options) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (sample_count == 0) {
        return UFT_ERR_NO_DATA;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Get index positions */
    uint64_t index_positions[UFT_REV_MAX_REVOLUTIONS + 1];
    size_t index_count = 0;
    
    if (index_data && index_data->positions && index_data->count > 0) {
        /* Use provided index data */
        index_count = (index_data->count > UFT_REV_MAX_REVOLUTIONS + 1) 
                    ? UFT_REV_MAX_REVOLUTIONS + 1 
                    : index_data->count;
        memcpy(index_positions, index_data->positions, 
               index_count * sizeof(uint64_t));
    } else if (options->use_index_pulse) {
        /* Try to find index from flux data */
        index_count = find_index_from_flux(
            flux_samples, sample_count,
            options->sample_rate_hz, options->nominal_rpm,
            index_positions, UFT_REV_MAX_REVOLUTIONS + 1
        );
    }
    
    if (index_count < options->min_revolutions + 1) {
        if (!options->allow_missing_index) {
            return UFT_ERR_NO_INDEX;
        }
        /* Fall back to estimated revolution boundaries */
        double expected_duration_s = 60.0 / options->nominal_rpm;
        uint64_t expected_samples = (uint64_t)(expected_duration_s * options->sample_rate_hz);
        
        uint64_t total_samples = 0;
        for (size_t i = 0; i < sample_count; i++) {
            total_samples += flux_samples[i];
        }
        
        index_count = 0;
        for (uint64_t pos = 0; pos <= total_samples && index_count <= UFT_REV_MAX_REVOLUTIONS; 
             pos += expected_samples) {
            index_positions[index_count++] = pos;
        }
    }
    
    /* Build revolution info */
    size_t rev_count = 0;
    double rpm_values[UFT_REV_MAX_REVOLUTIONS];
    
    for (size_t i = 0; i < index_count - 1 && rev_count < options->max_revolutions; i++) {
        uft_revolution_info_t *rev = &result->revolutions[rev_count];
        
        rev->revolution = (uint32_t)rev_count;
        rev->index_position = index_positions[i];
        rev->start_sample = index_positions[i];
        rev->end_sample = index_positions[i + 1];
        rev->sample_count = rev->end_sample - rev->start_sample;
        
        /* Calculate timing */
        rev->duration_us = (double)rev->sample_count / options->sample_rate_hz * 1000000.0;
        rev->rpm = uft_duration_to_rpm(rev->duration_us);
        
        double nominal_duration = uft_rpm_to_duration(options->nominal_rpm);
        rev->drift_us = rev->duration_us - nominal_duration;
        
        /* Quality based on RPM stability */
        if (uft_rpm_in_tolerance(rev->rpm, options->nominal_rpm, options->tolerance)) {
            double deviation = fabs(rev->rpm - options->nominal_rpm) / options->nominal_rpm;
            rev->quality = (uint8_t)(100.0 * (1.0 - deviation / options->tolerance));
        } else {
            rev->quality = 0;
        }
        
        rev->index_valid = true;
        rpm_values[rev_count] = rev->rpm;
        rev_count++;
    }
    
    result->count = rev_count;
    
    /* Calculate statistics */
    uft_revolution_calc_stats(result, options->nominal_rpm);
    
    /* Find best revolution */
    result->best_revolution = uft_revolution_find_best(result);
    
    return (rev_count >= options->min_revolutions) ? UFT_OK : UFT_ERR_INSUFFICIENT_DATA;
}

uft_error_t uft_revolution_extract(
    const uint32_t *flux_samples,
    size_t sample_count,
    const uft_revolution_result_t *revs,
    uint32_t revolution_idx,
    uint32_t *output,
    size_t output_size,
    size_t *actual_size
) {
    if (!flux_samples || !revs || !output || !actual_size) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (revolution_idx >= revs->count) {
        return UFT_ERR_OUT_OF_RANGE;
    }
    
    const uft_revolution_info_t *rev = &revs->revolutions[revolution_idx];
    
    /* Find sample range for this revolution */
    uint64_t current_pos = 0;
    size_t start_idx = 0;
    size_t end_idx = 0;
    bool found_start = false;
    
    for (size_t i = 0; i < sample_count; i++) {
        if (!found_start && current_pos >= rev->start_sample) {
            start_idx = i;
            found_start = true;
        }
        
        current_pos += flux_samples[i];
        
        if (found_start && current_pos >= rev->end_sample) {
            end_idx = i + 1;
            break;
        }
    }
    
    if (!found_start) {
        return UFT_ERR_NOT_FOUND;
    }
    
    size_t samples_to_copy = end_idx - start_idx;
    if (samples_to_copy > output_size) {
        *actual_size = samples_to_copy;
        return UFT_ERR_BUFFER_TOO_SMALL;
    }
    
    memcpy(output, &flux_samples[start_idx], samples_to_copy * sizeof(uint32_t));
    *actual_size = samples_to_copy;
    
    return UFT_OK;
}

uft_error_t uft_revolution_merge(
    const uint8_t **decoded_revs,
    size_t rev_count,
    size_t bit_count,
    uft_merged_revolution_t *merged
) {
    if (!decoded_revs || !merged || rev_count < 2) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t byte_count = (bit_count + 7) / 8;
    
    /* Allocate output */
    merged->data = (uint8_t *)calloc(byte_count, 1);
    merged->confidence = (uint8_t *)calloc(byte_count, 1);
    merged->weak_bits = (uint8_t *)calloc(byte_count, 1);
    
    if (!merged->data || !merged->confidence || !merged->weak_bits) {
        uft_merged_revolution_free(merged);
        return UFT_ERR_MEMORY;
    }
    
    merged->bit_count = bit_count;
    merged->weak_count = 0;
    
    /* Process each bit position */
    for (size_t bit_pos = 0; bit_pos < bit_count; bit_pos++) {
        size_t byte_idx = bit_pos / 8;
        size_t bit_idx = 7 - (bit_pos % 8);
        
        /* Count votes for 0 and 1 */
        size_t ones = 0;
        size_t zeros = 0;
        
        for (size_t r = 0; r < rev_count; r++) {
            if (decoded_revs[r]) {
                uint8_t bit = (decoded_revs[r][byte_idx] >> bit_idx) & 1;
                if (bit) ones++; else zeros++;
            }
        }
        
        /* Determine output bit */
        uint8_t output_bit;
        uint8_t confidence;
        bool is_weak = false;
        
        if (ones > zeros) {
            output_bit = 1;
            confidence = (uint8_t)(100 * ones / rev_count);
        } else if (zeros > ones) {
            output_bit = 0;
            confidence = (uint8_t)(100 * zeros / rev_count);
        } else {
            /* Tie - mark as weak, default to 0 */
            output_bit = 0;
            confidence = 50;
            is_weak = true;
        }
        
        /* Check for weak bit (significant disagreement) */
        if (ones > 0 && zeros > 0) {
            double agreement = (double)(ones > zeros ? ones : zeros) / rev_count;
            if (agreement < 0.75) {
                is_weak = true;
            }
        }
        
        /* Store results */
        if (output_bit) {
            merged->data[byte_idx] |= (1 << bit_idx);
        }
        merged->confidence[byte_idx] = confidence;  /* Simplified: per-byte */
        
        if (is_weak) {
            merged->weak_bits[byte_idx] |= (1 << bit_idx);
            merged->weak_count++;
        }
    }
    
    return UFT_OK;
}

uft_error_t uft_revolution_detect_weak(
    const uint8_t **decoded_revs,
    size_t rev_count,
    size_t bit_count,
    uint8_t *weak_mask,
    size_t *weak_count
) {
    if (!decoded_revs || !weak_mask || !weak_count || rev_count < 2) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t byte_count = (bit_count + 7) / 8;
    memset(weak_mask, 0, byte_count);
    *weak_count = 0;
    
    /* Compare all pairs of revolutions */
    for (size_t bit_pos = 0; bit_pos < bit_count; bit_pos++) {
        size_t byte_idx = bit_pos / 8;
        size_t bit_idx = 7 - (bit_pos % 8);
        
        /* Get first non-null revolution's bit */
        int reference_bit = -1;
        bool inconsistent = false;
        
        for (size_t r = 0; r < rev_count; r++) {
            if (!decoded_revs[r]) continue;
            
            int bit = (decoded_revs[r][byte_idx] >> bit_idx) & 1;
            
            if (reference_bit < 0) {
                reference_bit = bit;
            } else if (bit != reference_bit) {
                inconsistent = true;
                break;
            }
        }
        
        if (inconsistent) {
            weak_mask[byte_idx] |= (1 << bit_idx);
            (*weak_count)++;
        }
    }
    
    return UFT_OK;
}

void uft_revolution_calc_stats(
    uft_revolution_result_t *result,
    double nominal_rpm
) {
    if (!result || result->count == 0) return;
    
    double rpm_sum = 0;
    double duration_sum = 0;
    result->rpm_min = result->revolutions[0].rpm;
    result->rpm_max = result->revolutions[0].rpm;
    
    for (size_t i = 0; i < result->count; i++) {
        rpm_sum += result->revolutions[i].rpm;
        duration_sum += result->revolutions[i].duration_us;
        
        if (result->revolutions[i].rpm < result->rpm_min) {
            result->rpm_min = result->revolutions[i].rpm;
        }
        if (result->revolutions[i].rpm > result->rpm_max) {
            result->rpm_max = result->revolutions[i].rpm;
        }
    }
    
    result->average_rpm = rpm_sum / result->count;
    result->average_duration_us = duration_sum / result->count;
    
    /* Calculate variance */
    double rpm_values[UFT_REV_MAX_REVOLUTIONS];
    double duration_values[UFT_REV_MAX_REVOLUTIONS];
    
    for (size_t i = 0; i < result->count; i++) {
        rpm_values[i] = result->revolutions[i].rpm;
        duration_values[i] = result->revolutions[i].duration_us;
    }
    
    result->rpm_variance = calc_variance(rpm_values, result->count, result->average_rpm);
    result->duration_variance = calc_variance(duration_values, result->count, 
                                              result->average_duration_us);
    
    /* Check consistency */
    result->index_consistent = true;
    for (size_t i = 0; i < result->count; i++) {
        if (!result->revolutions[i].index_valid) {
            result->index_consistent = false;
            break;
        }
    }
    
    /* Check timing stability (variance < 0.5% of nominal) */
    double nominal_duration = uft_rpm_to_duration(nominal_rpm);
    double max_variance = nominal_duration * 0.005;
    result->timing_stable = (sqrt(result->duration_variance) < max_variance);
    
    /* Overall quality */
    uint32_t quality_sum = 0;
    for (size_t i = 0; i < result->count; i++) {
        quality_sum += result->revolutions[i].quality;
    }
    result->overall_quality = (uint8_t)(quality_sum / result->count);
}

uint32_t uft_revolution_find_best(
    const uft_revolution_result_t *result
) {
    if (!result || result->count == 0) return 0;
    
    uint32_t best_idx = 0;
    uint8_t best_quality = result->revolutions[0].quality;
    
    for (size_t i = 1; i < result->count; i++) {
        if (result->revolutions[i].quality > best_quality) {
            best_quality = result->revolutions[i].quality;
            best_idx = (uint32_t)i;
        }
    }
    
    return best_idx;
}

void uft_merged_revolution_free(uft_merged_revolution_t *merged) {
    if (!merged) return;
    
    free(merged->data);
    free(merged->confidence);
    free(merged->weak_bits);
    
    merged->data = NULL;
    merged->confidence = NULL;
    merged->weak_bits = NULL;
    merged->bit_count = 0;
    merged->weak_count = 0;
}

size_t uft_revolution_to_json(
    const uft_revolution_result_t *result,
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
    APPEND("  \"count\": %zu,\n", result->count);
    APPEND("  \"average_rpm\": %.3f,\n", result->average_rpm);
    APPEND("  \"rpm_variance\": %.6f,\n", result->rpm_variance);
    APPEND("  \"rpm_min\": %.3f,\n", result->rpm_min);
    APPEND("  \"rpm_max\": %.3f,\n", result->rpm_max);
    APPEND("  \"average_duration_us\": %.3f,\n", result->average_duration_us);
    APPEND("  \"index_consistent\": %s,\n", result->index_consistent ? "true" : "false");
    APPEND("  \"timing_stable\": %s,\n", result->timing_stable ? "true" : "false");
    APPEND("  \"overall_quality\": %u,\n", result->overall_quality);
    APPEND("  \"best_revolution\": %u,\n", result->best_revolution);
    APPEND("  \"revolutions\": [\n");
    
    for (size_t i = 0; i < result->count; i++) {
        const uft_revolution_info_t *rev = &result->revolutions[i];
        APPEND("    {\n");
        APPEND("      \"revolution\": %u,\n", rev->revolution);
        APPEND("      \"index_position\": %lu,\n", (unsigned long)rev->index_position);
        APPEND("      \"sample_count\": %lu,\n", (unsigned long)rev->sample_count);
        APPEND("      \"duration_us\": %.3f,\n", rev->duration_us);
        APPEND("      \"rpm\": %.3f,\n", rev->rpm);
        APPEND("      \"drift_us\": %.3f,\n", rev->drift_us);
        APPEND("      \"quality\": %u,\n", rev->quality);
        APPEND("      \"index_valid\": %s\n", rev->index_valid ? "true" : "false");
        APPEND("    }%s\n", (i < result->count - 1) ? "," : "");
    }
    
    APPEND("  ]\n");
    APPEND("}\n");
    
    #undef APPEND
    
    return offset;
}
