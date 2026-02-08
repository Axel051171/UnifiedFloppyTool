/**
 * @file uft_cell_analyzer.c
 * @brief Enhanced Cell-Level Flux Analysis Implementation
 * 
 * CLEAN-ROOM implementation based on observable requirements.
 */

#include "uft/decoder/uft_cell_analyzer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static double samples_to_ns(uint32_t samples, double sample_rate_hz) {
    return (double)samples / sample_rate_hz * 1e9;
}

static uint32_t ns_to_samples(double ns, double sample_rate_hz) {
    return (uint32_t)(ns * sample_rate_hz / 1e9 + 0.5);
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void uft_cell_options_init(uft_cell_options_t *options) {
    if (!options) return;
    
    options->cell_time_ns = UFT_CELL_MFM_DD_NS;
    options->tolerance = UFT_CELL_DEFAULT_TOLERANCE;
    options->search_mode = UFT_CELL_SEARCH_NORMAL;
    options->pll_window = 2;
    options->pll_gain = 0.05;
    options->detect_weak_bits = true;
    options->weak_threshold = 0.3;
    options->auto_detect_rate = false;
}

void uft_cell_options_for_encoding(
    uft_cell_options_t *options,
    const char *encoding
) {
    uft_cell_options_init(options);
    
    if (!encoding) return;
    
    if (strcmp(encoding, "MFM_DD") == 0) {
        options->cell_time_ns = UFT_CELL_MFM_DD_NS;
    } else if (strcmp(encoding, "MFM_HD") == 0) {
        options->cell_time_ns = UFT_CELL_MFM_HD_NS;
    } else if (strcmp(encoding, "MFM_ED") == 0) {
        options->cell_time_ns = UFT_CELL_MFM_ED_NS;
    } else if (strcmp(encoding, "FM") == 0) {
        options->cell_time_ns = UFT_CELL_FM_DD_NS;
    } else if (strcmp(encoding, "GCR_C64") == 0) {
        options->cell_time_ns = UFT_CELL_GCR_C64_NS;
    } else if (strcmp(encoding, "GCR_APPLE") == 0) {
        options->cell_time_ns = UFT_CELL_GCR_APPLE_NS;
    }
}

uft_error_t uft_cell_analyze(
    const uint32_t *flux_times,
    size_t flux_count,
    const uft_cell_options_t *options,
    uft_cell_result_t *result
) {
    if (!flux_times || !options || !result || flux_count == 0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Determine cell time */
    double cell_time_ns = options->cell_time_ns;
    if (options->auto_detect_rate) {
        uint8_t confidence;
        uft_cell_auto_detect(flux_times, flux_count, options->sample_rate_hz,
                            &cell_time_ns, &confidence);
        result->detected_cell_time = cell_time_ns;
        result->detected_bitrate = (cell_time_ns > 0) ? 1e9 / cell_time_ns : 0;
    }
    
    /* Convert to samples */
    double cell_samples = cell_time_ns * options->sample_rate_hz / 1e9;
    
    /* Initialize PLL */
    uft_pll_state_t pll;
    uft_pll_init(&pll, cell_samples, options->pll_gain);
    
    /* Estimate max output bits */
    size_t est_bits = flux_count * 3;  /* Conservative estimate */
    
    result->decoded_data = (uint8_t *)calloc((est_bits + 7) / 8, 1);
    result->cells = (uft_cell_info_t *)calloc(est_bits, sizeof(uft_cell_info_t));
    result->sync_positions = (uint64_t *)calloc(1024, sizeof(uint64_t));
    
    if (!result->decoded_data || !result->cells || !result->sync_positions) {
        uft_cell_result_free(result);
        return UFT_ERR_MEMORY;
    }
    
    /* Decode flux transitions */
    size_t bit_count = 0;
    double time_sum = 0;
    double time_sum_sq = 0;
    result->min_cell_time = 1e9;
    result->max_cell_time = 0;
    result->weak_bit_count = 0;
    
    for (size_t i = 0; i < flux_count && bit_count < est_bits; i++) {
        uint8_t out_bit;
        int bits = uft_pll_step(&pll, flux_times[i], &out_bit);
        
        double actual_ns = samples_to_ns(flux_times[i], options->sample_rate_hz);
        
        for (size_t b = 0; b < bits && bit_count < est_bits; b++) {
            uft_cell_info_t *cell = &result->cells[bit_count];
            
            cell->position = bit_count;
            cell->actual_time_ns = actual_ns / bits;  /* Distribute time */
            cell->deviation_ns = cell->actual_time_ns - cell_time_ns;
            cell->deviation_pct = cell->deviation_ns / cell_time_ns * 100.0;
            cell->value = out_bit;
            
            /* Confidence based on deviation */
            double abs_dev = fabs(cell->deviation_pct);
            if (abs_dev < options->tolerance * 100) {
                cell->confidence = (uint8_t)(100 - abs_dev / (options->tolerance * 100) * 50);
            } else {
                cell->confidence = (uint8_t)(50 - (abs_dev - options->tolerance * 100) * 2);
                if (cell->confidence > 50) cell->confidence = 0;  /* Overflow */
            }
            
            /* Weak bit detection */
            if (options->detect_weak_bits) {
                cell->is_weak = (abs_dev > options->weak_threshold * 100);
                if (cell->is_weak) result->weak_bit_count++;
            }
            
            /* Store decoded bit */
            if (out_bit) {
                result->decoded_data[bit_count / 8] |= (1 << (7 - (bit_count % 8)));
            }
            
            /* Statistics */
            time_sum += cell->actual_time_ns;
            time_sum_sq += cell->actual_time_ns * cell->actual_time_ns;
            if (cell->actual_time_ns < result->min_cell_time) {
                result->min_cell_time = cell->actual_time_ns;
            }
            if (cell->actual_time_ns > result->max_cell_time) {
                result->max_cell_time = cell->actual_time_ns;
            }
            
            bit_count++;
        }
    }
    
    result->cell_count = bit_count;
    result->bit_count = bit_count;
    
    /* Calculate statistics */
    if (bit_count > 0) {
        result->average_cell_time = time_sum / bit_count;
        double variance = (time_sum_sq / bit_count) - 
                         (result->average_cell_time * result->average_cell_time);
        result->cell_time_stddev = (variance > 0) ? sqrt(variance) : 0;
    }
    
    /* PLL final state */
    result->final_pll_phase = pll.phase;
    result->final_pll_freq = pll.frequency;
    
    /* Calculate overall quality */
    uft_cell_calc_quality(result);
    
    return UFT_OK;
}

uft_error_t uft_cell_auto_detect(
    const uint32_t *flux_times,
    size_t flux_count,
    double sample_rate_hz,
    double *detected_ns,
    uint8_t *confidence
) {
    if (!flux_times || !detected_ns || flux_count < 100) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Build histogram of flux times */
    #define HIST_BINS 100
    #define HIST_MAX_NS 10000
    
    uint32_t histogram[HIST_BINS] = {0};
    double bin_width = HIST_MAX_NS / HIST_BINS;
    
    for (size_t i = 0; i < flux_count; i++) {
        double ns = samples_to_ns(flux_times[i], sample_rate_hz);
        int bin = (int)(ns / bin_width);
        if (bin >= 0 && bin < HIST_BINS) {
            histogram[bin]++;
        }
    }
    
    /* Find peaks (looking for 1T, 2T, 3T pattern for MFM) */
    uint32_t max_count = 0;
    int max_bin = 0;
    
    for (int i = 0; i < HIST_BINS; i++) {
        if (histogram[i] > max_count) {
            max_count = histogram[i];
            max_bin = i;
        }
    }
    
    /* First peak is typically 2T for MFM */
    double peak_ns = (max_bin + 0.5) * bin_width;
    
    /* For MFM, cell time is peak / 2 */
    /* For GCR/FM, need different heuristics */
    *detected_ns = peak_ns / 2.0;
    
    /* Confidence based on peak prominence */
    double peak_ratio = (double)max_count / flux_count;
    if (confidence) {
        *confidence = (uint8_t)(peak_ratio * 300);  /* Scale to 0-100 */
        if (*confidence > 100) *confidence = 100;
    }
    
    return UFT_OK;
}

uft_error_t uft_cell_detect_bands(
    const uint32_t *flux_times,
    size_t flux_count,
    const uft_cell_options_t *options,
    uft_cell_band_result_t *band_result
) {
    if (!flux_times || !options || !band_result) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    memset(band_result, 0, sizeof(*band_result));
    
    /* C64 speed zones */
    double zone_times[] = {
        3250,  /* Zone 0: tracks 31-35 */
        3500,  /* Zone 1: tracks 25-30 */
        3750,  /* Zone 2: tracks 18-24 */
        4000   /* Zone 3: tracks 1-17 */
    };
    
    /* Allocate bands */
    band_result->bands = (uft_cell_band_t *)calloc(4, sizeof(uft_cell_band_t));
    if (!band_result->bands) return UFT_ERR_MEMORY;
    
    /* Analyze flux in windows to detect zone changes */
    size_t window = 5000;
    uint64_t current_pos = 0;
    int current_zone = -1;
    
    for (size_t start = 0; start < flux_count; start += window) {
        size_t end = (start + window < flux_count) ? start + window : flux_count;
        
        /* Calculate average cell time in window */
        uint64_t sum = 0;
        for (size_t i = start; i < end; i++) {
            sum += flux_times[i];
            current_pos += flux_times[i];
        }
        
        double avg_samples = (double)sum / (end - start);
        double avg_ns = samples_to_ns((uint32_t)avg_samples, options->sample_rate_hz) / 2;
        
        /* Find closest zone */
        int best_zone = 0;
        double best_diff = fabs(avg_ns - zone_times[0]);
        
        for (int z = 1; z < 4; z++) {
            double diff = fabs(avg_ns - zone_times[z]);
            if (diff < best_diff) {
                best_diff = diff;
                best_zone = z;
            }
        }
        
        if (best_zone != current_zone) {
            if (current_zone >= 0 && band_result->band_count > 0) {
                /* End previous band */
                band_result->bands[band_result->band_count - 1].end_position = current_pos;
            }
            
            /* Start new band */
            if (band_result->band_count < 4) {
                uft_cell_band_t *band = &band_result->bands[band_result->band_count];
                band->zone = best_zone;
                band->cell_time_ns = zone_times[best_zone];
                band->start_position = current_pos;
                band_result->band_count++;
            }
            
            current_zone = best_zone;
        }
    }
    
    /* Close last band */
    if (band_result->band_count > 0) {
        band_result->bands[band_result->band_count - 1].end_position = current_pos;
    }
    
    band_result->is_multi_rate = (band_result->band_count > 1);
    
    return UFT_OK;
}

uft_error_t uft_cell_build_histogram(
    const uint32_t *flux_times,
    size_t flux_count,
    double sample_rate_hz,
    double bin_width_ns,
    uft_cell_histogram_t *histogram
) {
    if (!flux_times || !histogram || flux_count == 0 || bin_width_ns <= 0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Find range */
    double min_ns = 1e9, max_ns = 0;
    for (size_t i = 0; i < flux_count; i++) {
        double ns = samples_to_ns(flux_times[i], sample_rate_hz);
        if (ns < min_ns) min_ns = ns;
        if (ns > max_ns) max_ns = ns;
    }
    
    /* Create bins */
    size_t bin_count = (size_t)((max_ns - min_ns) / bin_width_ns) + 1;
    if (bin_count > 1000) bin_count = 1000;  /* Limit */
    
    histogram->bins = (uft_cell_histogram_bin_t *)calloc(bin_count, sizeof(uft_cell_histogram_bin_t));
    if (!histogram->bins) return UFT_ERR_MEMORY;
    
    histogram->bin_count = bin_count;
    histogram->min_time_ns = min_ns;
    histogram->max_time_ns = max_ns;
    histogram->total_cells = (uint32_t)flux_count;
    
    /* Initialize bins */
    for (size_t i = 0; i < bin_count; i++) {
        histogram->bins[i].center_ns = min_ns + (i + 0.5) * bin_width_ns;
        histogram->bins[i].count = 0;
    }
    
    /* Fill histogram */
    uint32_t max_count = 0;
    size_t max_bin = 0;
    
    for (size_t i = 0; i < flux_count; i++) {
        double ns = samples_to_ns(flux_times[i], sample_rate_hz);
        size_t bin = (size_t)((ns - min_ns) / bin_width_ns);
        if (bin >= bin_count) bin = bin_count - 1;
        
        histogram->bins[bin].count++;
        if (histogram->bins[bin].count > max_count) {
            max_count = histogram->bins[bin].count;
            max_bin = bin;
        }
    }
    
    /* Calculate percentages and find peak */
    for (size_t i = 0; i < bin_count; i++) {
        histogram->bins[i].percentage = 100.0 * histogram->bins[i].count / flux_count;
    }
    
    histogram->peak_time_ns = histogram->bins[max_bin].center_ns;
    
    return UFT_OK;
}

void uft_cell_calc_quality(uft_cell_result_t *result) {
    if (!result || result->cell_count == 0) return;
    
    /* Quality factors:
     * - Low deviation from nominal (weight: 40%)
     * - Few weak bits (weight: 30%)
     * - Consistent timing (weight: 30%)
     */
    
    double avg_confidence = 0;
    for (size_t i = 0; i < result->cell_count; i++) {
        avg_confidence += result->cells[i].confidence;
    }
    avg_confidence /= result->cell_count;
    
    double weak_ratio = (double)result->weak_bit_count / result->cell_count;
    double weak_score = 100 * (1.0 - weak_ratio);
    
    double timing_score = 100;
    if (result->average_cell_time > 0) {
        double cv = result->cell_time_stddev / result->average_cell_time;
        timing_score = 100 * (1.0 - cv * 10);  /* 10% CV = 0 score */
        if (timing_score < 0) timing_score = 0;
    }
    
    result->overall_quality = (uint8_t)(
        avg_confidence * 0.4 +
        weak_score * 0.3 +
        timing_score * 0.3
    );
}

void uft_cell_result_free(uft_cell_result_t *result) {
    if (!result) return;
    
    free(result->cells);
    free(result->decoded_data);
    free(result->sync_positions);
    uft_cell_histogram_free(&result->histogram);
    
    memset(result, 0, sizeof(*result));
}

void uft_cell_band_free(uft_cell_band_result_t *result) {
    if (!result) return;
    free(result->bands);
    memset(result, 0, sizeof(*result));
}

void uft_cell_histogram_free(uft_cell_histogram_t *histogram) {
    if (!histogram) return;
    free(histogram->bins);
    memset(histogram, 0, sizeof(*histogram));
}

/* ============================================================================
 * PLL Implementation
 * ============================================================================ */

void uft_pll_init(
    uft_pll_state_t *pll,
    double cell_time_samples,
    double gain
) {
    if (!pll) return;
    
    pll->phase = 0;
    pll->frequency = cell_time_samples;
    pll->nominal_freq = cell_time_samples;
    pll->gain = gain;
    pll->window = 2;
    pll->locked = false;
    pll->bit_position = 0;
}

int uft_pll_step(
    uft_pll_state_t *pll,
    uint32_t flux_interval,
    uint8_t *output_bit
) {
    if (!pll || !output_bit) return 0;
    
    int bits_out = 0;
    double remaining = flux_interval;
    
    while (remaining >= pll->frequency * 0.5) {
        /* Time to next bit boundary */
        double to_boundary = pll->frequency - pll->phase;
        
        if (remaining >= to_boundary) {
            /* Crossed a bit boundary */
            remaining -= to_boundary;
            pll->phase = 0;
            
            /* Determine bit value based on transition position */
            if (remaining < pll->frequency * 0.25) {
                *output_bit = 1;  /* Transition near boundary = 1 */
            } else {
                *output_bit = 0;
            }
            
            bits_out++;
            pll->bit_position++;
        } else {
            pll->phase += remaining;
            remaining = 0;
        }
    }
    
    /* Phase error adjustment */
    if (bits_out > 0) {
        double error = pll->phase;
        pll->frequency += error * pll->gain;
        
        /* Clamp frequency to reasonable range */
        if (pll->frequency < pll->nominal_freq * 0.8) {
            pll->frequency = pll->nominal_freq * 0.8;
        }
        if (pll->frequency > pll->nominal_freq * 1.2) {
            pll->frequency = pll->nominal_freq * 1.2;
        }
    }
    
    return bits_out;
}

void uft_pll_reset(uft_pll_state_t *pll) {
    if (!pll) return;
    
    double nominal = pll->nominal_freq;
    double gain = pll->gain;
    uft_pll_init(pll, nominal, gain);
}

/* ============================================================================
 * JSON Export
 * ============================================================================ */

size_t uft_cell_result_to_json(
    const uft_cell_result_t *result,
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
    APPEND("  \"cell_count\": %zu,\n", result->cell_count);
    APPEND("  \"bit_count\": %zu,\n", result->bit_count);
    APPEND("  \"average_cell_time_ns\": %.3f,\n", result->average_cell_time);
    APPEND("  \"cell_time_stddev_ns\": %.3f,\n", result->cell_time_stddev);
    APPEND("  \"min_cell_time_ns\": %.3f,\n", result->min_cell_time);
    APPEND("  \"max_cell_time_ns\": %.3f,\n", result->max_cell_time);
    APPEND("  \"weak_bit_count\": %u,\n", result->weak_bit_count);
    APPEND("  \"error_count\": %u,\n", result->error_count);
    APPEND("  \"overall_quality\": %u,\n", result->overall_quality);
    APPEND("  \"detected_cell_time_ns\": %.3f,\n", result->detected_cell_time);
    APPEND("  \"detected_bitrate_bps\": %.1f,\n", result->detected_bitrate);
    APPEND("  \"final_pll_phase\": %.3f,\n", result->final_pll_phase);
    APPEND("  \"final_pll_freq\": %.3f\n", result->final_pll_freq);
    APPEND("}\n");
    
    #undef APPEND
    
    return offset;
}
