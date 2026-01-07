/**
 * @file uft_histogram.c
 * @brief Unified Histogram Analysis Library - Implementation
 * 
 * @version 1.0
 * @date 2026-01-07
 */

#include "uft_histogram.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

uft_histogram_t* uft_hist_create(uint32_t bin_count) {
    if (bin_count == 0) bin_count = UFT_HIST_BYTE_BINS;
    if (bin_count > UFT_HIST_MAX_BINS) bin_count = UFT_HIST_MAX_BINS;
    
    uft_histogram_t *hist = calloc(1, sizeof(uft_histogram_t));
    if (!hist) return NULL;
    
    hist->bins = calloc(bin_count, sizeof(uint32_t));
    if (!hist->bins) {
        free(hist);
        return NULL;
    }
    
    hist->bin_count = bin_count;
    hist->bin_width = 1;
    hist->offset = 0;
    hist->stats_valid = false;
    hist->peaks_valid = false;
    
    return hist;
}

void uft_hist_destroy(uft_histogram_t *hist) {
    if (hist) {
        free(hist->bins);
        free(hist);
    }
}

void uft_hist_clear(uft_histogram_t *hist) {
    if (hist && hist->bins) {
        memset(hist->bins, 0, hist->bin_count * sizeof(uint32_t));
        memset(&hist->stats, 0, sizeof(hist->stats));
        hist->peak_count = 0;
        hist->stats_valid = false;
        hist->peaks_valid = false;
    }
}

/*===========================================================================
 * Building Histograms
 *===========================================================================*/

void uft_hist_add_u8(uft_histogram_t *hist, const uint8_t *data, size_t len) {
    if (!hist || !data || hist->bin_count < 256) return;
    
    for (size_t i = 0; i < len; i++) {
        hist->bins[data[i]]++;
    }
    
    hist->stats_valid = false;
    hist->peaks_valid = false;
}

void uft_hist_add_u16(uft_histogram_t *hist, const uint16_t *data, size_t len) {
    if (!hist || !data) return;
    
    for (size_t i = 0; i < len; i++) {
        uint32_t bin = data[i];
        if (bin < hist->bin_count) {
            hist->bins[bin]++;
        }
    }
    
    hist->stats_valid = false;
    hist->peaks_valid = false;
}

void uft_hist_add_u32(uft_histogram_t *hist, const uint32_t *data, size_t len) {
    if (!hist || !data) return;
    
    for (size_t i = 0; i < len; i++) {
        uint32_t bin = data[i];
        if (bin < hist->bin_count) {
            hist->bins[bin]++;
        }
    }
    
    hist->stats_valid = false;
    hist->peaks_valid = false;
}

void uft_hist_add_scaled(uft_histogram_t *hist, 
                         const uint32_t *data, size_t len,
                         uint32_t scale) {
    if (!hist || !data || scale == 0) return;
    
    hist->bin_width = scale;
    
    for (size_t i = 0; i < len; i++) {
        uint32_t bin = data[i] / scale;
        if (bin < hist->bin_count) {
            hist->bins[bin]++;
        }
    }
    
    hist->stats_valid = false;
    hist->peaks_valid = false;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

const uft_hist_stats_t* uft_hist_stats(uft_histogram_t *hist) {
    if (!hist) return NULL;
    if (hist->stats_valid) return &hist->stats;
    
    uft_hist_stats_t *s = &hist->stats;
    memset(s, 0, sizeof(*s));
    
    /* First pass: find range and totals */
    s->min_bin = hist->bin_count;
    s->max_bin = 0;
    s->max_count = 0;
    
    uint64_t sum = 0;
    uint64_t sum_sq = 0;
    
    for (uint32_t i = 0; i < hist->bin_count; i++) {
        uint32_t count = hist->bins[i];
        if (count > 0) {
            if (i < s->min_bin) s->min_bin = i;
            if (i > s->max_bin) s->max_bin = i;
            if (count > s->max_count) {
                s->max_count = count;
                s->max_bin_pos = i;
            }
            s->total_samples += count;
            sum += (uint64_t)i * count;
            sum_sq += (uint64_t)i * i * count;
        }
    }
    
    if (s->total_samples > 0) {
        s->mean = (double)sum / s->total_samples;
        double variance = (double)sum_sq / s->total_samples - s->mean * s->mean;
        s->stddev = sqrt(variance > 0 ? variance : 0);
        
        /* Find median */
        uint64_t half = s->total_samples / 2;
        uint64_t cumulative = 0;
        for (uint32_t i = 0; i < hist->bin_count; i++) {
            cumulative += hist->bins[i];
            if (cumulative >= half) {
                s->median = (double)i;
                break;
            }
        }
    }
    
    hist->stats_valid = true;
    return s;
}

/*===========================================================================
 * Peak Detection
 *===========================================================================*/

uint32_t uft_hist_find_peaks(uft_histogram_t *hist,
                             uint32_t min_height,
                             uint32_t min_distance) {
    if (!hist) return 0;
    if (hist->peaks_valid) return hist->peak_count;
    
    /* Ensure stats are computed */
    const uft_hist_stats_t *s = uft_hist_stats(hist);
    
    /* Auto-calculate thresholds if not specified */
    if (min_height == 0) {
        min_height = s->max_count / 10;  /* 10% of max */
        if (min_height < 2) min_height = 2;
    }
    
    if (min_distance == 0) {
        min_distance = (s->max_bin - s->min_bin) / 20;  /* 5% of range */
        if (min_distance < 3) min_distance = 3;
    }
    
    hist->peak_count = 0;
    
    /* Find local maxima */
    for (uint32_t i = 1; i < hist->bin_count - 1; i++) {
        uint32_t curr = hist->bins[i];
        
        /* Must be above minimum height */
        if (curr < min_height) continue;
        
        /* Must be local maximum */
        uint32_t prev = hist->bins[i - 1];
        uint32_t next = hist->bins[i + 1];
        
        if (curr > prev && curr >= next) {
            /* Check distance from previous peak */
            bool too_close = false;
            for (uint32_t p = 0; p < hist->peak_count; p++) {
                uint32_t dist = (i > hist->peaks[p].position) ?
                               (i - hist->peaks[p].position) :
                               (hist->peaks[p].position - i);
                if (dist < min_distance) {
                    /* Keep the higher peak */
                    if (curr > hist->peaks[p].count) {
                        hist->peaks[p].position = i;
                        hist->peaks[p].count = curr;
                    }
                    too_close = true;
                    break;
                }
            }
            
            if (!too_close && hist->peak_count < UFT_HIST_MAX_PEAKS) {
                uft_hist_peak_t *peak = &hist->peaks[hist->peak_count++];
                peak->position = i;
                peak->count = curr;
                
                /* Calculate width at half maximum */
                uint32_t half = curr / 2;
                uint32_t left = i, right = i;
                while (left > 0 && hist->bins[left] > half) left--;
                while (right < hist->bin_count - 1 && hist->bins[right] > half) right++;
                peak->width = right - left;
                
                /* Calculate weighted center */
                uint64_t weighted_sum = 0;
                uint64_t weight_total = 0;
                for (uint32_t j = left; j <= right; j++) {
                    weighted_sum += (uint64_t)j * hist->bins[j];
                    weight_total += hist->bins[j];
                }
                peak->center = (weight_total > 0) ? 
                              (float)weighted_sum / weight_total : (float)i;
            }
        }
    }
    
    /* Sort peaks by position */
    for (uint32_t i = 0; i < hist->peak_count; i++) {
        for (uint32_t j = i + 1; j < hist->peak_count; j++) {
            if (hist->peaks[j].position < hist->peaks[i].position) {
                uft_hist_peak_t tmp = hist->peaks[i];
                hist->peaks[i] = hist->peaks[j];
                hist->peaks[j] = tmp;
            }
        }
    }
    
    hist->peaks_valid = true;
    return hist->peak_count;
}

const uft_hist_peak_t* uft_hist_get_peaks(const uft_histogram_t *hist,
                                          uint32_t *count) {
    if (!hist) {
        if (count) *count = 0;
        return NULL;
    }
    if (count) *count = hist->peak_count;
    return hist->peaks;
}

/*===========================================================================
 * MFM/FM Timing Analysis
 *===========================================================================*/

bool uft_hist_detect_mfm_timing(const uft_histogram_t *hist,
                                 uint32_t *cell_time) {
    if (!hist || !cell_time) return false;
    
    /* MFM should have exactly 3 peaks at 1T, 1.5T, 2T ratios */
    if (hist->peak_count < 3) return false;
    
    /* Find best 3 peaks that match MFM pattern */
    for (uint32_t i = 0; i < hist->peak_count - 2; i++) {
        float p1 = hist->peaks[i].center;
        float p2 = hist->peaks[i + 1].center;
        float p3 = hist->peaks[i + 2].center;
        
        /* Check ratios: p2/p1 ≈ 1.5, p3/p1 ≈ 2.0 */
        float ratio1 = p2 / p1;
        float ratio2 = p3 / p1;
        
        if (ratio1 >= 1.4f && ratio1 <= 1.6f &&
            ratio2 >= 1.9f && ratio2 <= 2.1f) {
            /* Found MFM pattern! Cell time is first peak position */
            *cell_time = (uint32_t)(p1 * hist->bin_width);
            return true;
        }
    }
    
    return false;
}

bool uft_hist_detect_fm_timing(const uft_histogram_t *hist,
                                uint32_t *cell_time) {
    if (!hist || !cell_time) return false;
    
    /* FM should have 2 peaks at 1T, 2T ratios */
    if (hist->peak_count < 2) return false;
    
    for (uint32_t i = 0; i < hist->peak_count - 1; i++) {
        float p1 = hist->peaks[i].center;
        float p2 = hist->peaks[i + 1].center;
        
        /* Check ratio: p2/p1 ≈ 2.0 */
        float ratio = p2 / p1;
        
        if (ratio >= 1.9f && ratio <= 2.1f) {
            *cell_time = (uint32_t)(p1 * hist->bin_width);
            return true;
        }
    }
    
    return false;
}

/*===========================================================================
 * Utility
 *===========================================================================*/

void uft_hist_smooth(uft_histogram_t *hist, uint32_t window) {
    if (!hist || window < 3) return;
    if (window % 2 == 0) window++;  /* Make odd */
    
    uint32_t half = window / 2;
    uint32_t *temp = malloc(hist->bin_count * sizeof(uint32_t));
    if (!temp) return;
    
    for (uint32_t i = 0; i < hist->bin_count; i++) {
        uint64_t sum = 0;
        uint32_t count = 0;
        
        for (uint32_t j = (i > half ? i - half : 0);
             j <= i + half && j < hist->bin_count;
             j++) {
            sum += hist->bins[j];
            count++;
        }
        
        temp[i] = (uint32_t)(sum / count);
    }
    
    memcpy(hist->bins, temp, hist->bin_count * sizeof(uint32_t));
    free(temp);
    
    hist->stats_valid = false;
    hist->peaks_valid = false;
}

void uft_hist_normalize(uft_histogram_t *hist, uint32_t scale) {
    if (!hist || scale == 0) return;
    
    const uft_hist_stats_t *s = uft_hist_stats(hist);
    if (s->max_count == 0) return;
    
    for (uint32_t i = 0; i < hist->bin_count; i++) {
        hist->bins[i] = (uint32_t)((uint64_t)hist->bins[i] * scale / s->max_count);
    }
    
    hist->stats_valid = false;
}

void uft_hist_print(const uft_histogram_t *hist, 
                    FILE *out, 
                    uint32_t width,
                    uint32_t height) {
    if (!hist || !out) return;
    
    const uft_hist_stats_t *s = uft_hist_stats((uft_histogram_t*)hist);
    if (s->total_samples == 0) {
        fprintf(out, "(empty histogram)\n");
        return;
    }
    
    /* Print header */
    fprintf(out, "Histogram: %u bins, %lu samples\n", 
            hist->bin_count, (unsigned long)s->total_samples);
    fprintf(out, "Range: [%u - %u], Mean: %.2f, StdDev: %.2f\n",
            s->min_bin, s->max_bin, s->mean, s->stddev);
    
    /* Print ASCII bars */
    uint32_t range = s->max_bin - s->min_bin + 1;
    uint32_t step = (range > width) ? (range / width) : 1;
    
    for (uint32_t row = 0; row < height; row++) {
        uint32_t threshold = s->max_count * (height - row) / height;
        
        fprintf(out, "%6u |", threshold);
        
        for (uint32_t col = 0; col < width && (s->min_bin + col * step) < hist->bin_count; col++) {
            uint32_t bin_start = s->min_bin + col * step;
            uint32_t max_in_range = 0;
            
            for (uint32_t b = bin_start; b < bin_start + step && b < hist->bin_count; b++) {
                if (hist->bins[b] > max_in_range) {
                    max_in_range = hist->bins[b];
                }
            }
            
            fprintf(out, "%c", (max_in_range >= threshold) ? '#' : ' ');
        }
        
        fprintf(out, "|\n");
    }
    
    /* Print axis */
    fprintf(out, "       +");
    for (uint32_t i = 0; i < width; i++) fprintf(out, "-");
    fprintf(out, "+\n");
    
    fprintf(out, "        %-*u%*u\n", 
            (int)(width/2), s->min_bin, 
            (int)(width/2), s->max_bin);
}

/*===========================================================================
 * Unit Tests
 *===========================================================================*/

#ifdef UFT_HISTOGRAM_TEST

#include <assert.h>

int main(void) {
    printf("=== UFT Histogram Unit Tests ===\n\n");
    
    /* Test 1: Create/Destroy */
    printf("Test 1: Create/Destroy... ");
    {
        uft_histogram_t *h = uft_hist_create(256);
        assert(h != NULL);
        assert(h->bin_count == 256);
        uft_hist_destroy(h);
    }
    printf("OK\n");
    
    /* Test 2: Add data */
    printf("Test 2: Add data... ");
    {
        uft_histogram_t *h = uft_hist_create(256);
        uint8_t data[] = { 10, 20, 30, 20, 20, 10 };
        uft_hist_add_u8(h, data, 6);
        
        assert(uft_hist_get(h, 10) == 2);
        assert(uft_hist_get(h, 20) == 3);
        assert(uft_hist_get(h, 30) == 1);
        uft_hist_destroy(h);
    }
    printf("OK\n");
    
    /* Test 3: Statistics */
    printf("Test 3: Statistics... ");
    {
        uft_histogram_t *h = uft_hist_create(256);
        uint8_t data[] = { 10, 20, 30, 40, 50 };
        uft_hist_add_u8(h, data, 5);
        
        const uft_hist_stats_t *s = uft_hist_stats(h);
        assert(s->total_samples == 5);
        assert(s->min_bin == 10);
        assert(s->max_bin == 50);
        assert(fabs(s->mean - 30.0) < 0.1);
        
        uft_hist_destroy(h);
    }
    printf("OK\n");
    
    /* Test 4: Peak detection */
    printf("Test 4: Peak detection... ");
    {
        uft_histogram_t *h = uft_hist_create(256);
        
        /* Create artificial peaks at 50, 100, 150 */
        for (int i = 45; i < 55; i++) h->bins[i] = 100 - abs(i - 50) * 10;
        for (int i = 95; i < 105; i++) h->bins[i] = 200 - abs(i - 100) * 20;
        for (int i = 145; i < 155; i++) h->bins[i] = 150 - abs(i - 150) * 15;
        
        uint32_t count = uft_hist_find_peaks(h, 50, 10);
        assert(count == 3);
        
        uft_hist_destroy(h);
    }
    printf("OK\n");
    
    /* Test 5: Smoothing */
    printf("Test 5: Smoothing... ");
    {
        uft_histogram_t *h = uft_hist_create(256);
        h->bins[50] = 100;
        h->bins[51] = 0;
        h->bins[52] = 100;
        
        uft_hist_smooth(h, 3);
        
        /* After smoothing, center should have some value */
        assert(uft_hist_get(h, 51) > 0);
        
        uft_hist_destroy(h);
    }
    printf("OK\n");
    
    /* Test 6: Print */
    printf("Test 6: Print... ");
    {
        uft_histogram_t *h = uft_hist_create(100);
        for (int i = 20; i < 80; i++) {
            h->bins[i] = 50 - abs(i - 50);
        }
        
        printf("\n");
        uft_hist_print(h, stdout, 40, 10);
        
        uft_hist_destroy(h);
    }
    printf("OK\n");
    
    printf("\n=== All 6 tests passed! ===\n");
    return 0;
}

#endif /* UFT_HISTOGRAM_TEST */
