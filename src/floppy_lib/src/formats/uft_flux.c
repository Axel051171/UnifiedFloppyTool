/**
 * @file uft_flux.c
 * @brief Raw flux data analysis implementation
 */

#include "formats/uft_flux.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Flux Track Management
 * ============================================================================ */

#define FLUX_GROWTH_FACTOR 2

uft_flux_track_t* uft_flux_track_create(size_t capacity) {
    uft_flux_track_t *track = calloc(1, sizeof(uft_flux_track_t));
    if (!track) return NULL;
    
    if (capacity > 0) {
        track->samples = calloc(capacity, sizeof(uft_flux_sample_t));
        if (!track->samples) {
            free(track);
            return NULL;
        }
    }
    
    track->sample_clock = UFT_FLUX_CLOCK_GREASEWEAZLE;  /* Default */
    return track;
}

void uft_flux_track_free(uft_flux_track_t *track) {
    if (track) {
        free(track->samples);
        free(track);
    }
}

uft_flux_error_t uft_flux_track_add_sample(uft_flux_track_t *track,
                                            uft_flux_sample_t sample) {
    if (!track) return UFT_FLUX_ERR_INVALID_PARAM;
    
    /* Grow array if needed - track capacity in first element when empty */
    static size_t capacity = 0;
    if (track->sample_count == 0) {
        capacity = 4096;  /* Initial capacity */
    }
    
    if (track->sample_count >= capacity) {
        size_t new_cap = capacity * FLUX_GROWTH_FACTOR;
        uft_flux_sample_t *new_samples = realloc(track->samples,
            new_cap * sizeof(uft_flux_sample_t));
        if (!new_samples) return UFT_FLUX_ERR_BUFFER_TOO_SMALL;
        track->samples = new_samples;
        capacity = new_cap;
    }
    
    track->samples[track->sample_count++] = sample;
    return UFT_FLUX_OK;
}

ssize_t uft_flux_find_index(const uft_flux_track_t *track) {
    if (!track || !track->has_index) return -1;
    return (ssize_t)track->index_offset;
}

uft_flux_error_t uft_flux_rotate_to_index(uft_flux_track_t *track) {
    if (!track || !track->has_index) return UFT_FLUX_ERR_NO_INDEX;
    if (track->index_offset == 0) return UFT_FLUX_OK;
    
    /* Rotate samples so index is at start */
    size_t idx = track->index_offset;
    size_t count = track->sample_count;
    
    uft_flux_sample_t *temp = malloc(idx * sizeof(uft_flux_sample_t));
    if (!temp) return UFT_FLUX_ERR_BUFFER_TOO_SMALL;
    
    memcpy(temp, track->samples, idx * sizeof(uft_flux_sample_t));
    memmove(track->samples, track->samples + idx, 
            (count - idx) * sizeof(uft_flux_sample_t));
    memcpy(track->samples + (count - idx), temp, idx * sizeof(uft_flux_sample_t));
    
    free(temp);
    track->index_offset = 0;
    
    return UFT_FLUX_OK;
}

/* ============================================================================
 * PLL Implementation
 * ============================================================================ */

void uft_pll_init(uft_pll_t *pll, double bit_cell,
                  double freq_gain, double phase_gain) {
    if (!pll) return;
    
    pll->clock = bit_cell;
    pll->phase = 0;
    pll->window = bit_cell * 0.5;  /* 50% window */
    pll->freq_gain = freq_gain;
    pll->phase_gain = phase_gain;
    pll->total_bits = 0;
    pll->errors = 0;
}

void uft_pll_reset(uft_pll_t *pll) {
    if (!pll) return;
    pll->phase = 0;
    pll->total_bits = 0;
    pll->errors = 0;
}

bool uft_pll_process(uft_pll_t *pll, double interval, int *bits) {
    if (!pll || !bits) return false;
    
    /* Add interval to phase */
    pll->phase += interval;
    
    /* Determine number of bit cells */
    double cells = pll->phase / pll->clock;
    int num_bits = (int)(cells + 0.5);  /* Round to nearest */
    
    if (num_bits < 1) num_bits = 1;
    if (num_bits > 3) num_bits = 3;  /* MFM: max 3 cells without transition */
    
    *bits = num_bits;
    pll->total_bits += (uint32_t)num_bits;
    
    /* Calculate phase error */
    double expected = num_bits * pll->clock;
    double error = pll->phase - expected;
    
    /* Check if within window */
    bool in_window = fabs(error) < pll->window;
    if (!in_window) {
        pll->errors++;
    }
    
    /* Adjust clock frequency */
    pll->clock += error * pll->freq_gain;
    
    /* Adjust phase */
    pll->phase = error * pll->phase_gain;
    
    return in_window;
}

/* ============================================================================
 * Decoding Functions
 * ============================================================================ */

uft_flux_error_t uft_flux_decode_mfm(
    const uft_flux_track_t *track,
    uft_decoded_track_t *output,
    double bit_cell
) {
    if (!track || !output || track->sample_count == 0) {
        return UFT_FLUX_ERR_INVALID_PARAM;
    }
    
    /* Initialize PLL */
    uft_pll_t pll;
    uft_pll_init(&pll, bit_cell, 0.01, 0.05);
    
    /* Allocate output buffer (estimate: 1 byte per 16 flux samples) */
    size_t max_bytes = track->sample_count / 8;
    output->data = calloc(max_bytes, 1);
    if (!output->data) return UFT_FLUX_ERR_BUFFER_TOO_SMALL;
    
    output->data_len = 0;
    output->bit_count = 0;
    output->weak_bits = 0;
    
    /* Decode flux transitions */
    uint8_t shift_reg = 0;
    int bit_count = 0;
    double clock_sum = 0;
    int clock_count = 0;
    
    for (size_t i = 0; i < track->sample_count; i++) {
        double interval = (double)track->samples[i];
        int bits;
        
        bool in_window = uft_pll_process(&pll, interval, &bits);
        
        if (!in_window) {
            output->weak_bits++;
        }
        
        clock_sum += pll.clock;
        clock_count++;
        
        /* MFM: Each flux transition represents a '1' bit, 
         * gaps represent '0' bits */
        for (int b = 0; b < bits - 1; b++) {
            shift_reg = (uint8_t)((shift_reg << 1) | 0);  /* Clock bit */
            bit_count++;
            if (bit_count == 8) {
                output->data[output->data_len++] = shift_reg;
                shift_reg = 0;
                bit_count = 0;
            }
        }
        
        /* The transition itself is a '1' */
        shift_reg = (uint8_t)((shift_reg << 1) | 1);
        bit_count++;
        if (bit_count == 8) {
            output->data[output->data_len++] = shift_reg;
            shift_reg = 0;
            bit_count = 0;
        }
    }
    
    output->bit_count = output->data_len * 8 + (size_t)bit_count;
    output->avg_clock = (float)(clock_sum / clock_count);
    
    return UFT_FLUX_OK;
}

uft_flux_error_t uft_flux_decode_fm(
    const uft_flux_track_t *track,
    uft_decoded_track_t *output,
    double bit_cell
) {
    /* FM is similar to MFM but simpler - every bit cell has a clock */
    return uft_flux_decode_mfm(track, output, bit_cell * 2);
}

uft_flux_error_t uft_flux_decode_gcr(
    const uft_flux_track_t *track,
    uft_decoded_track_t *output,
    double bit_cell,
    int gcr_type
) {
    if (!track || !output) return UFT_FLUX_ERR_INVALID_PARAM;
    
    /* GCR decoding - simplified implementation */
    /* Full implementation would need proper GCR tables */
    
    uft_pll_t pll;
    uft_pll_init(&pll, bit_cell, 0.01, 0.05);
    
    size_t max_bytes = track->sample_count / 4;
    output->data = calloc(max_bytes, 1);
    if (!output->data) return UFT_FLUX_ERR_BUFFER_TOO_SMALL;
    
    output->data_len = 0;
    output->bit_count = 0;
    
    uint8_t shift_reg = 0;
    int bit_count = 0;
    
    for (size_t i = 0; i < track->sample_count; i++) {
        double interval = (double)track->samples[i];
        int bits;
        
        uft_pll_process(&pll, interval, &bits);
        
        /* GCR: Direct bit representation */
        for (int b = 0; b < bits - 1; b++) {
            shift_reg = (uint8_t)(shift_reg << 1);
            bit_count++;
            if (bit_count == 8) {
                output->data[output->data_len++] = shift_reg;
                shift_reg = 0;
                bit_count = 0;
            }
        }
        
        shift_reg = (uint8_t)((shift_reg << 1) | 1);
        bit_count++;
        if (bit_count == 8) {
            output->data[output->data_len++] = shift_reg;
            shift_reg = 0;
            bit_count = 0;
        }
    }
    
    output->bit_count = output->data_len * 8 + (size_t)bit_count;
    
    (void)gcr_type;  /* TODO: Use GCR type for proper decoding */
    
    return UFT_FLUX_OK;
}

uft_flux_error_t uft_flux_decode_auto(
    const uft_flux_track_t *track,
    uft_decoded_track_t *output
) {
    if (!track || !output) return UFT_FLUX_ERR_INVALID_PARAM;
    
    /* Estimate bit cell to determine encoding */
    double bit_cell = uft_flux_estimate_bitcell(track, 0);
    if (bit_cell == 0) {
        bit_cell = uft_flux_estimate_bitcell(track, 2);
    }
    
    if (bit_cell == 0) return UFT_FLUX_ERR_PLL_FAIL;
    
    /* Try MFM first (most common) */
    uft_flux_error_t err = uft_flux_decode_mfm(track, output, bit_cell);
    
    return err;
}

void uft_decoded_track_free(uft_decoded_track_t *decoded) {
    if (decoded) {
        free(decoded->data);
        decoded->data = NULL;
        decoded->data_len = 0;
    }
}

/* ============================================================================
 * Analysis Functions
 * ============================================================================ */

uft_flux_error_t uft_flux_histogram(
    const uft_flux_track_t *track,
    uft_flux_hist_bin_t *bins,
    size_t bin_count,
    uint32_t min_time,
    uint32_t max_time
) {
    if (!track || !bins || bin_count == 0) {
        return UFT_FLUX_ERR_INVALID_PARAM;
    }
    
    /* Initialize bins */
    uint32_t range = max_time - min_time;
    uint32_t bin_size = range / (uint32_t)bin_count;
    
    for (size_t i = 0; i < bin_count; i++) {
        bins[i].min_time = min_time + (uint32_t)i * bin_size;
        bins[i].max_time = bins[i].min_time + bin_size;
        bins[i].count = 0;
    }
    
    /* Count samples into bins */
    for (size_t i = 0; i < track->sample_count; i++) {
        uint32_t sample = track->samples[i];
        if (sample >= min_time && sample < max_time) {
            size_t bin = (sample - min_time) / bin_size;
            if (bin >= bin_count) bin = bin_count - 1;
            bins[bin].count++;
        }
    }
    
    return UFT_FLUX_OK;
}

double uft_flux_estimate_bitcell(
    const uft_flux_track_t *track,
    int encoding
) {
    if (!track || track->sample_count < 100) return 0;
    
    /* Create histogram */
    uft_flux_hist_bin_t bins[100];
    uint32_t min_time = 10;   /* ~400ns at 24MHz */
    uint32_t max_time = 200;  /* ~8us at 24MHz */
    
    uft_flux_histogram(track, bins, 100, min_time, max_time);
    
    /* Find peaks in histogram */
    /* For MFM, expect peaks at 1T, 1.5T, 2T */
    /* For GCR, expect peaks at 1T, 2T */
    
    uint32_t max_count = 0;
    size_t max_bin = 0;
    
    for (size_t i = 0; i < 100; i++) {
        if (bins[i].count > max_count) {
            max_count = bins[i].count;
            max_bin = i;
        }
    }
    
    if (max_count == 0) return 0;
    
    /* First peak should be at bit cell time */
    double first_peak = (double)(bins[max_bin].min_time + bins[max_bin].max_time) / 2;
    
    /* For MFM, the first peak is typically at 1.5T or 2T */
    if (encoding == 0) {  /* MFM */
        /* Assume first peak is 2T, so T = peak/2 */
        return first_peak / 2.0;
    } else {  /* GCR or unknown */
        return first_peak;
    }
}

double uft_flux_rotation_time(const uft_flux_track_t *track) {
    if (!track || !track->has_index || track->sample_count == 0) {
        return 0;
    }
    
    /* Sum all samples to get total track time */
    uint64_t total = 0;
    for (size_t i = 0; i < track->sample_count; i++) {
        total += track->samples[i];
    }
    
    /* Convert to microseconds */
    return (double)total * 1e6 / (double)track->sample_clock;
}

uint32_t uft_flux_data_rate(const uft_flux_track_t *track) {
    if (!track) return 0;
    
    double rot_time = uft_flux_rotation_time(track);
    if (rot_time == 0) return 0;
    
    /* Estimate bits from sample count (rough estimate) */
    size_t estimated_bits = track->sample_count * 2;  /* MFM average */
    
    /* bits per second */
    return (uint32_t)(estimated_bits * 1e6 / rot_time);
}

/* ============================================================================
 * Multi-Revolution Analysis
 * ============================================================================ */

size_t uft_flux_compare_revolutions(
    const uft_flux_track_t **revs,
    size_t rev_count,
    double tolerance,
    uint8_t *diff_map,
    size_t diff_map_len
) {
    if (!revs || rev_count < 2 || !diff_map) return 0;
    
    /* Find minimum sample count */
    size_t min_samples = revs[0]->sample_count;
    for (size_t r = 1; r < rev_count; r++) {
        if (revs[r]->sample_count < min_samples) {
            min_samples = revs[r]->sample_count;
        }
    }
    
    memset(diff_map, 0, diff_map_len);
    size_t diff_count = 0;
    
    /* Compare sample by sample */
    for (size_t i = 0; i < min_samples && i / 8 < diff_map_len; i++) {
        uint32_t ref = revs[0]->samples[i];
        bool differs = false;
        
        for (size_t r = 1; r < rev_count; r++) {
            uint32_t sample = revs[r]->samples[i];
            double diff = fabs((double)sample - (double)ref);
            if (diff > tolerance * ref) {
                differs = true;
                break;
            }
        }
        
        if (differs) {
            diff_map[i / 8] |= (uint8_t)(1 << (i % 8));
            diff_count++;
        }
    }
    
    return diff_count;
}

uft_flux_error_t uft_flux_merge_revolutions(
    const uft_flux_track_t **revs,
    size_t rev_count,
    uft_flux_track_t *output
) {
    if (!revs || rev_count == 0 || !output) {
        return UFT_FLUX_ERR_INVALID_PARAM;
    }
    
    /* Find minimum sample count */
    size_t min_samples = revs[0]->sample_count;
    for (size_t r = 1; r < rev_count; r++) {
        if (revs[r]->sample_count < min_samples) {
            min_samples = revs[r]->sample_count;
        }
    }
    
    /* Allocate output */
    output->samples = calloc(min_samples, sizeof(uft_flux_sample_t));
    if (!output->samples) return UFT_FLUX_ERR_BUFFER_TOO_SMALL;
    
    output->sample_count = min_samples;
    output->sample_clock = revs[0]->sample_clock;
    
    /* Merge by taking median value */
    for (size_t i = 0; i < min_samples; i++) {
        /* Collect all values */
        uint32_t values[16];  /* Assume max 16 revolutions */
        size_t count = 0;
        
        for (size_t r = 0; r < rev_count && count < 16; r++) {
            values[count++] = revs[r]->samples[i];
        }
        
        /* Simple median: sort and take middle */
        for (size_t j = 0; j < count - 1; j++) {
            for (size_t k = j + 1; k < count; k++) {
                if (values[k] < values[j]) {
                    uint32_t tmp = values[j];
                    values[j] = values[k];
                    values[k] = tmp;
                }
            }
        }
        
        output->samples[i] = values[count / 2];
    }
    
    return UFT_FLUX_OK;
}
