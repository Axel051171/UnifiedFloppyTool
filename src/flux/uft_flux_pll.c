/**
 * @file uft_flux_pll.c
 * @brief Flux Stream PLL Implementation for UFT
 * 
 * 
 * @copyright UFT Project
 */

#include "uft/flux/uft_flux_pll_v20.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*============================================================================
 * PLL Initialization
 *============================================================================*/

void uft_pll_init(uft_pll_state_t* pll)
{
    if (!pll) return;
    memset(pll, 0, sizeof(*pll));
    
    /* Default configuration */
    pll->tick_freq = UFT_PLL_DEFAULT_TICK_FREQ;
    pll->pll_min_max_percent = UFT_PLL_DEFAULT_MIN_MAX_PERCENT;
    
    /* Default correction ratios */
    pll->fast_correction_ratio_n = UFT_PLL_FAST_CORRECTION_N;
    pll->fast_correction_ratio_d = UFT_PLL_FAST_CORRECTION_D;
    pll->slow_correction_ratio_n = UFT_PLL_SLOW_CORRECTION_N;
    pll->slow_correction_ratio_d = UFT_PLL_SLOW_CORRECTION_D;
    
    /* Configure for default 500 kbps (MFM HD) */
    uft_pll_configure(pll, 500, pll->tick_freq);
}

void uft_pll_reset(uft_pll_state_t* pll)
{
    if (!pll) return;
    
    /* Reset state, keep configuration */
    pll->pump_charge = 0;
    pll->phase = 0;
    pll->last_error = 0;
    pll->last_pulse_phase = 0;
    pll->total_pulses = 0;
    pll->error_pulses = 0;
    pll->sync_losses = 0;
}

void uft_pll_soft_reset(uft_pll_state_t* pll)
{
    if (!pll) return;
    
    /* Partial reset - keep pump_charge for continuity */
    pll->phase = 0;
    pll->last_error = 0;
}

void uft_pll_configure(uft_pll_state_t* pll, uint32_t bitrate_kbps, 
                       uint32_t tick_freq)
{
    if (!pll || bitrate_kbps == 0) return;
    
    pll->tick_freq = tick_freq;
    
    /* Calculate bitcell in ticks */
    /* bitcell_ns = 1000000 / bitrate_kbps */
    /* pivot = bitcell_ns * tick_freq / 1e9 */
    uint64_t bitcell_ns = 1000000ULL / bitrate_kbps;
    pll->pivot = (uint32_t)((bitcell_ns * tick_freq) / 1000000000ULL);
    
    /* Set window limits */
    pll->pll_max = pll->pivot + (pll->pivot * pll->pll_min_max_percent) / 100;
    pll->pll_min = pll->pivot - (pll->pivot * pll->pll_min_max_percent) / 100;
    
    /* Calculate max error in ticks */
    pll->max_pll_error_ticks = (float)((float)tick_freq * 
                                        (float)UFT_PLL_DEFAULT_MAX_ERROR_NS * 1e-9f);
    
    /* Initialize pump charge to center */
    pll->pump_charge = pll->pivot / 2;
}

void uft_pll_set_encoding(uft_pll_state_t* pll, uft_encoding_t encoding)
{
    if (!pll) return;
    pll->encoding = encoding;
}

/*============================================================================
 * Histogram Functions
 *============================================================================*/

void uft_pll_compute_histogram(const uint32_t* pulses, size_t count,
                                uint32_t* histogram)
{
    if (!pulses || !histogram || count == 0) return;
    
    memset(histogram, 0, sizeof(uint32_t) * UFT_PLL_HISTOGRAM_SIZE);
    
    for (size_t i = 0; i < count; i++) {
        uint32_t val = pulses[i];
        if (val < UFT_PLL_HISTOGRAM_SIZE) {
            histogram[val]++;
        }
    }
}

/**
 * @brief Find local maximum in histogram
 */
static int find_peak(const uint32_t* histogram, size_t start, size_t end,
                     uint32_t* center, uint32_t* total)
{
    uint32_t max_val = 0;
    size_t max_idx = start;
    uint32_t sum = 0;
    
    for (size_t i = start; i < end && i < UFT_PLL_HISTOGRAM_SIZE; i++) {
        sum += histogram[i];
        if (histogram[i] > max_val) {
            max_val = histogram[i];
            max_idx = i;
        }
    }
    
    if (max_val == 0) return -1;
    
    *center = (uint32_t)max_idx;
    *total = sum;
    return 0;
}

int uft_pll_detect_peaks(uft_pll_state_t* pll, const uint32_t* histogram)
{
    if (!pll || !histogram) return 0;
    
    /* Calculate total samples */
    uint64_t total = 0;
    for (size_t i = 0; i < UFT_PLL_HISTOGRAM_SIZE; i++) {
        total += histogram[i];
    }
    
    if (total == 0) return 0;
    
    /* Find significant peaks */
    pll->num_peaks = 0;
    
    /* Start search from minimum reasonable timing (~1µs = ~24 ticks @ 24MHz) */
    size_t search_start = 20;
    size_t min_gap = 10;  /* Minimum gap between peaks */
    
    while (pll->num_peaks < UFT_PLL_MAX_PEAKS && search_start < UFT_PLL_HISTOGRAM_SIZE) {
        /* Find next peak */
        uint32_t center, count;
        
        /* Search in a window */
        size_t window_end = search_start + 100;
        if (window_end > UFT_PLL_HISTOGRAM_SIZE) {
            window_end = UFT_PLL_HISTOGRAM_SIZE;
        }
        
        /* Find local maximum */
        uint32_t max_val = 0;
        size_t max_idx = search_start;
        
        for (size_t i = search_start; i < window_end; i++) {
            if (histogram[i] > max_val) {
                max_val = histogram[i];
                max_idx = i;
            }
        }
        
        if (max_val == 0 || max_idx == search_start) {
            search_start = window_end;
            continue;
        }
        
        /* Verify it's a real peak (higher than neighbors) */
        bool is_peak = true;
        uint32_t neighbor_sum = 0;
        size_t neighbor_count = 0;
        
        for (size_t i = max_idx > 5 ? max_idx - 5 : 0; 
             i < max_idx + 5 && i < UFT_PLL_HISTOGRAM_SIZE; i++) {
            if (i != max_idx) {
                neighbor_sum += histogram[i];
                neighbor_count++;
            }
        }
        
        if (neighbor_count > 0) {
            uint32_t avg_neighbor = neighbor_sum / neighbor_count;
            if (max_val < avg_neighbor * 2) {
                is_peak = false;
            }
        }
        
        if (is_peak && max_val > total / 1000) {  /* At least 0.1% of samples */
            /* Find peak boundaries */
            size_t left = max_idx;
            while (left > 0 && histogram[left] > max_val / 4) {
                left--;
            }
            
            size_t right = max_idx;
            while (right < UFT_PLL_HISTOGRAM_SIZE - 1 && histogram[right] > max_val / 4) {
                right++;
            }
            
            /* Calculate total count in peak */
            count = 0;
            for (size_t i = left; i <= right; i++) {
                count += histogram[i];
            }
            
            /* Store peak */
            uft_pll_peak_t* peak = &pll->peaks[pll->num_peaks];
            peak->center = (uint32_t)max_idx;
            peak->left = (uint32_t)left;
            peak->right = (uint32_t)right;
            peak->count = count;
            peak->percent = (float)count * 100.0f / (float)total;
            peak->bit_count = 0;  /* Will be determined later */
            
            pll->num_peaks++;
            
            search_start = right + min_gap;
        } else {
            search_start++;
        }
    }
    
    /* Determine bit counts for each peak */
    if (pll->num_peaks >= 2) {
        /* First peak is typically 1-bit timing */
        pll->peaks[0].bit_count = 1;
        uint32_t base = pll->peaks[0].center;
        
        for (int i = 1; i < pll->num_peaks; i++) {
            /* Estimate bit count from ratio to first peak */
            float ratio = (float)pll->peaks[i].center / (float)base;
            pll->peaks[i].bit_count = (uint8_t)(ratio + 0.5f);
        }
    } else if (pll->num_peaks == 1) {
        pll->peaks[0].bit_count = 1;
    }
    
    return pll->num_peaks;
}

uft_encoding_t uft_pll_detect_encoding(uft_pll_state_t* pll, 
                                        const uint32_t* histogram)
{
    if (!pll || !histogram) return UFT_ENCODING_UNKNOWN;
    
    /* Detect peaks first */
    uft_pll_detect_peaks(pll, histogram);
    
    if (pll->num_peaks < 2) {
        return UFT_ENCODING_UNKNOWN;
    }
    
    /* Analyze peak pattern */
    uint32_t p1 = pll->peaks[0].center;
    uint32_t p2 = pll->peaks[1].center;
    float ratio = (float)p2 / (float)p1;
    
    /* MFM: peaks at 1:1.5:2 ratio (1T, 1.5T, 2T) */
    if (ratio > 1.4f && ratio < 1.6f) {
        pll->encoding = UFT_ENCODING_MFM;
        
        /* Update PLL pivot based on detected timing */
        pll->pivot = p1;
        pll->pll_max = pll->pivot + (pll->pivot * pll->pll_min_max_percent) / 100;
        pll->pll_min = pll->pivot - (pll->pivot * pll->pll_min_max_percent) / 100;
        
        return UFT_ENCODING_MFM;
    }
    
    /* FM: peaks at 1:2 ratio (clock, data) */
    if (ratio > 1.9f && ratio < 2.1f) {
        pll->encoding = UFT_ENCODING_FM;
        pll->pivot = p1;
        return UFT_ENCODING_FM;
    }
    
    /* GCR: typically 3 main peaks */
    if (pll->num_peaks >= 3) {
        uint32_t p3 = pll->peaks[2].center;
        float r12 = (float)p2 / (float)p1;
        float r13 = (float)p3 / (float)p1;
        
        /* C64 GCR: roughly 1:1.33:1.67:2 */
        if (r12 > 1.2f && r12 < 1.5f && r13 > 1.5f && r13 < 1.8f) {
            pll->encoding = UFT_ENCODING_C64_GCR;
            pll->pivot = p1;
            return UFT_ENCODING_C64_GCR;
        }
    }
    
    return UFT_ENCODING_UNKNOWN;
}

/*============================================================================
 * PLL Processing Core
 *============================================================================*/

/**
 * @brief Get band (number of bitcells) for pulse value
 */
static int get_band_cells(const uft_pll_state_t* pll, uint32_t pulse)
{
    if (pulse < pll->pll_min) return 0;  /* Too short */
    
    /* Calculate how many bitcells this pulse represents */
    /* Using pump_charge as the current estimated bitcell timing */
    int32_t adjusted = pulse * 16;  /* Scale for precision */
    int32_t bitcell = pll->pump_charge;
    
    if (bitcell <= 0) bitcell = pll->pivot;
    
    int cells = (adjusted + bitcell / 2) / bitcell;
    
    if (cells <= 0) cells = 1;
    if (cells > 4) cells = 4;  /* Cap at 4 bitcells */
    
    return cells;
}

int uft_pll_process_pulse(uft_pll_state_t* pll, uint32_t pulse, 
                          bool* bad_pulse)
{
    if (!pll) return 0;
    if (bad_pulse) *bad_pulse = false;
    
    pll->total_pulses++;
    
    /* Check if pulse is in valid range */
    if (pulse < pll->pll_min / 2 || pulse > pll->pll_max * 3) {
        if (bad_pulse) *bad_pulse = true;
        pll->error_pulses++;
        return 0;
    }
    
    /* Get number of bitcells */
    int cells = get_band_cells(pll, pulse);
    
    /* Calculate expected position for this number of cells */
    int32_t expected = cells * pll->pump_charge;
    int32_t actual = pulse * 16;  /* Scale to match pump_charge */
    
    /* Phase error */
    int32_t phase_error = actual - expected;
    
    /* Apply phase correction */
    if (cells > 0) {
        int32_t correction;
        
        if (phase_error < 0) {
            /* Pulse came early - slow down */
            correction = (pll->pump_charge * pll->slow_correction_ratio_n + phase_error) 
                         / pll->slow_correction_ratio_d;
        } else {
            /* Pulse came late - speed up */
            correction = (pll->pump_charge * pll->fast_correction_ratio_n + phase_error) 
                         / pll->fast_correction_ratio_d;
        }
        
        pll->pump_charge = correction;
        
        /* Clamp pump charge to valid range */
        if (pll->pump_charge < (int32_t)(pll->pll_min / 2)) {
            pll->pump_charge = pll->pll_min / 2;
        }
        if (pll->pump_charge > (int32_t)(pll->pll_max / 2)) {
            pll->pump_charge = pll->pll_max / 2;
        }
    }
    
    /* Track error */
    pll->last_error = phase_error;
    
    /* Check for excessive error */
    if (phase_error > (int32_t)(pll->max_pll_error_ticks * 16) ||
        phase_error < -(int32_t)(pll->max_pll_error_ticks * 16)) {
        if (bad_pulse) *bad_pulse = true;
        pll->sync_losses++;
    }
    
    return cells;
}

/*============================================================================
 * Pre-sync
 *============================================================================*/

void uft_pll_presync(uft_pll_state_t* pll, const uint32_t* pulses,
                     size_t count, size_t sync_pulses)
{
    if (!pll || !pulses || count == 0) return;
    
    if (sync_pulses > count) sync_pulses = count;
    if (sync_pulses < 10) sync_pulses = 10;
    
    /* Reset PLL */
    uft_pll_reset(pll);
    
    /* Process sync pulses without using output */
    for (size_t i = 0; i < sync_pulses; i++) {
        bool bad;
        uft_pll_process_pulse(pll, pulses[i], &bad);
    }
}

/*============================================================================
 * Stream Decoding
 *============================================================================*/

/**
 * @brief Set bit in packed array
 */
static inline void set_bit(uint8_t* data, size_t offset, int value)
{
    if (value) {
        data[offset >> 3] |= (0x80 >> (offset & 7));
    } else {
        data[offset >> 3] &= ~(0x80 >> (offset & 7));
    }
}

int uft_pll_decode_stream(uft_pll_state_t* pll, 
                          const uft_flux_stream_t* stream,
                          uft_decoded_track_t* output)
{
    if (!pll || !stream || !output || !stream->pulses) return -1;
    
    /* Estimate output size (each pulse = ~1-4 bits, use 2x for safety) */
    size_t est_bits = stream->num_pulses * 2;
    size_t est_bytes = (est_bits + 7) / 8;
    
    output->data = calloc(est_bytes, 1);
    if (!output->data) return -1;
    
    output->byte_length = est_bytes;
    output->bit_length = 0;
    output->timing = NULL;
    output->weak_mask = NULL;
    
    /* Pre-sync */
    uft_pll_presync(pll, stream->pulses, stream->num_pulses, 
                    stream->num_pulses > 1000 ? 1000 : stream->num_pulses / 2);
    
    /* Decode */
    size_t bit_pos = 0;
    
    for (size_t i = 0; i < stream->num_pulses; i++) {
        bool bad;
        int cells = uft_pll_process_pulse(pll, stream->pulses[i], &bad);
        
        if (cells > 0) {
            /* Output '1' followed by (cells-1) zeros */
            if (bit_pos < est_bits) {
                set_bit(output->data, bit_pos++, 1);
            }
            
            for (int c = 1; c < cells && bit_pos < est_bits; c++) {
                set_bit(output->data, bit_pos++, 0);
            }
        }
    }
    
    output->bit_length = bit_pos;
    output->byte_length = (bit_pos + 7) / 8;
    
    return 0;
}

/*============================================================================
 * Multi-Revolution Processing
 *============================================================================*/

int uft_pll_multi_revolution(uft_pll_state_t* pll,
                             const uft_flux_stream_t* stream,
                             uft_decoded_track_t* output,
                             uft_revolution_t* revolutions,
                             size_t max_revolutions)
{
    if (!pll || !stream || !output || !revolutions) return -1;
    if (stream->num_indices < 2) return -1;
    
    size_t num_revs = stream->num_indices - 1;
    if (num_revs > max_revolutions) num_revs = max_revolutions;
    
    /* Decode each revolution */
    uft_decoded_track_t* rev_tracks = calloc(num_revs, sizeof(uft_decoded_track_t));
    if (!rev_tracks) return -1;
    
    for (size_t r = 0; r < num_revs; r++) {
        uft_flux_stream_t rev_stream = *stream;
        rev_stream.pulses = stream->pulses + stream->index_offsets[r];
        rev_stream.num_pulses = stream->index_offsets[r + 1] - stream->index_offsets[r];
        
        revolutions[r].start_pulse = stream->index_offsets[r];
        revolutions[r].end_pulse = stream->index_offsets[r + 1];
        
        uft_pll_soft_reset(pll);
        int result = uft_pll_decode_stream(pll, &rev_stream, &rev_tracks[r]);
        
        if (result == 0) {
            revolutions[r].bit_length = rev_tracks[r].bit_length;
            
            /* Calculate confidence based on error rate */
            float error_rate = (float)pll->error_pulses / 
                               (float)(pll->total_pulses > 0 ? pll->total_pulses : 1);
            revolutions[r].confidence = 1.0f - error_rate;
        }
    }
    
    /* Fuse revolutions */
    int result = uft_pll_fuse_revolutions(rev_tracks, num_revs, output);
    
    /* Cleanup */
    for (size_t r = 0; r < num_revs; r++) {
        uft_decoded_track_free(&rev_tracks[r]);
    }
    free(rev_tracks);
    
    return result == 0 ? (int)num_revs : -1;
}

int uft_pll_fuse_revolutions(const uft_decoded_track_t* revs, size_t num_revs,
                             uft_decoded_track_t* output)
{
    if (!revs || !output || num_revs == 0) return -1;
    
    /* Find longest revolution */
    size_t max_bits = 0;
    for (size_t r = 0; r < num_revs; r++) {
        if (revs[r].bit_length > max_bits) {
            max_bits = revs[r].bit_length;
        }
    }
    
    if (max_bits == 0) return -1;
    
    /* Allocate output */
    size_t bytes = (max_bits + 7) / 8;
    output->data = calloc(bytes, 1);
    output->weak_mask = calloc(bytes, 1);
    if (!output->data || !output->weak_mask) {
        free(output->data);
        free(output->weak_mask);
        return -1;
    }
    
    output->bit_length = max_bits;
    output->byte_length = bytes;
    
    /* Majority voting for each bit */
    for (size_t bit = 0; bit < max_bits; bit++) {
        int ones = 0;
        int zeros = 0;
        int valid_revs = 0;
        
        for (size_t r = 0; r < num_revs; r++) {
            if (bit < revs[r].bit_length) {
                int val = (revs[r].data[bit >> 3] >> (7 - (bit & 7))) & 1;
                if (val) ones++;
                else zeros++;
                valid_revs++;
            }
        }
        
        if (valid_revs > 0) {
            /* Set bit based on majority */
            if (ones > zeros) {
                output->data[bit >> 3] |= (0x80 >> (bit & 7));
            }
            
            /* Mark as weak if not unanimous */
            if (ones > 0 && zeros > 0) {
                output->weak_mask[bit >> 3] |= (0x80 >> (bit & 7));
            }
        }
    }
    
    return 0;
}

/*============================================================================
 * Jitter Filter
 *============================================================================*/

void uft_pll_jitter_filter(uint32_t* pulses, size_t count, size_t window)
{
    if (!pulses || count < window) return;
    
    /* Simple moving average filter */
    uint32_t* temp = malloc(count * sizeof(uint32_t));
    if (!temp) return;
    
    memcpy(temp, pulses, count * sizeof(uint32_t));
    
    for (size_t i = window / 2; i < count - window / 2; i++) {
        uint64_t sum = 0;
        for (size_t j = i - window / 2; j <= i + window / 2; j++) {
            sum += temp[j];
        }
        pulses[i] = (uint32_t)(sum / window);
    }
    
    free(temp);
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

void uft_pll_print_stats(const uft_pll_state_t* pll)
{
    if (!pll) return;
    
    printf("PLL Statistics:\n");
    printf("  Tick Frequency: %u Hz\n", pll->tick_freq);
    printf("  Pivot (bitcell): %u ticks (%.2f ns)\n", 
           pll->pivot, uft_ticks_to_ns(pll->pivot, pll->tick_freq) / 1.0f);
    printf("  Window: %u - %u ticks (±%u%%)\n", 
           pll->pll_min, pll->pll_max, pll->pll_min_max_percent);
    printf("  Current Pump Charge: %d\n", pll->pump_charge);
    printf("  Encoding: %d\n", pll->encoding);
    printf("  Peaks Detected: %d\n", pll->num_peaks);
    
    for (int i = 0; i < pll->num_peaks && i < 4; i++) {
        printf("    Peak %d: center=%u, count=%u (%.1f%%), bits=%u\n",
               i, pll->peaks[i].center, pll->peaks[i].count,
               pll->peaks[i].percent, pll->peaks[i].bit_count);
    }
    
    printf("  Total Pulses: %llu\n", (unsigned long long)pll->total_pulses);
    printf("  Error Pulses: %llu (%.2f%%)\n", 
           (unsigned long long)pll->error_pulses,
           pll->total_pulses > 0 ? 
           (float)pll->error_pulses * 100.0f / pll->total_pulses : 0.0f);
    printf("  Sync Losses: %llu\n", (unsigned long long)pll->sync_losses);
}

void uft_decoded_track_free(uft_decoded_track_t* track)
{
    if (!track) return;
    
    if (track->data) {
        free(track->data);
        track->data = NULL;
    }
    if (track->timing) {
        free(track->timing);
        track->timing = NULL;
    }
    if (track->weak_mask) {
        free(track->weak_mask);
        track->weak_mask = NULL;
    }
    
    track->bit_length = 0;
    track->byte_length = 0;
}
