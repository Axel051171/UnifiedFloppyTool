/**
 * @file uft_flux_instability.c
 * @brief Flux Instability Analysis Implementation
 * 
 * EXT4-001: Flux Instability Analysis
 * Based on FloppyAI flux_analyzer.py
 * 
 * Analyzes multi-revolution flux data to detect:
 * - Weak bits (inconsistent across revolutions)
 * - Media degradation
 * - Copy protection signatures
 */

#include "uft/flux/uft_flux_instability.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/*===========================================================================
 * Configuration
 *===========================================================================*/

void uft_instab_config_init(uft_instab_config_t *config)
{
    if (!config) return;
    
    config->angular_bins = UFT_INSTAB_DEFAULT_BINS;
    config->phase_weight = UFT_INSTAB_W_PHASE;
    config->coherence_weight = UFT_INSTAB_W_COHERENCE;
    config->gap_weight = UFT_INSTAB_W_GAP;
    config->outlier_weight = UFT_INSTAB_W_OUTLIER;
    config->outlier_threshold = 3.0;  /* 3 sigma */
    config->gap_threshold_ns = 10000; /* 10µs */
    config->min_revolutions = 2;
}

/*===========================================================================
 * Memory Management
 *===========================================================================*/

uft_instab_result_t *uft_instab_alloc(uint16_t angular_bins)
{
    uft_instab_result_t *result = calloc(1, sizeof(uft_instab_result_t));
    if (!result) return NULL;
    
    result->angular_bins = angular_bins;
    result->phase_histogram = calloc(angular_bins, sizeof(double));
    result->variance_histogram = calloc(angular_bins, sizeof(double));
    
    if (!result->phase_histogram || !result->variance_histogram) {
        uft_instab_free(result);
        return NULL;
    }
    
    return result;
}

void uft_instab_free(uft_instab_result_t *result)
{
    if (!result) return;
    free(result->phase_histogram);
    free(result->variance_histogram);
    free(result);
}

/*===========================================================================
 * Statistical Helpers
 *===========================================================================*/

/**
 * @brief Calculate mean of array
 */
static double calc_mean(const double *data, size_t count)
{
    if (!data || count == 0) return 0.0;
    
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += data[i];
    }
    return sum / (double)count;
}

/**
 * @brief Calculate standard deviation
 */
static double calc_std(const double *data, size_t count, double mean)
{
    if (!data || count < 2) return 0.0;
    
    double var_sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = data[i] - mean;
        var_sum += diff * diff;
    }
    return sqrt(var_sum / (double)(count - 1));
}

/**
 * @brief Calculate Pearson correlation coefficient
 */
static double calc_correlation(const double *x, const double *y, size_t count)
{
    if (!x || !y || count < 2) return 0.0;
    
    double mean_x = calc_mean(x, count);
    double mean_y = calc_mean(y, count);
    
    double cov = 0.0;
    double var_x = 0.0;
    double var_y = 0.0;
    
    for (size_t i = 0; i < count; i++) {
        double dx = x[i] - mean_x;
        double dy = y[i] - mean_y;
        cov += dx * dy;
        var_x += dx * dx;
        var_y += dy * dy;
    }
    
    if (var_x < 1e-10 || var_y < 1e-10) return 0.0;
    return cov / sqrt(var_x * var_y);
}

/*===========================================================================
 * Angular Histogram
 *===========================================================================*/

int uft_instab_angular_histogram(const uint32_t *intervals, size_t count,
                                 uint16_t bins, double *histogram,
                                 double *rev_time)
{
    if (!intervals || !histogram || count == 0 || bins == 0) return -1;
    
    /* Calculate total revolution time */
    uint64_t total = 0;
    for (size_t i = 0; i < count; i++) {
        total += intervals[i];
    }
    
    if (rev_time) *rev_time = (double)total;
    
    /* Clear histogram */
    memset(histogram, 0, bins * sizeof(double));
    
    /* Build histogram */
    double bin_size = (double)total / (double)bins;
    uint64_t pos = 0;
    
    for (size_t i = 0; i < count; i++) {
        /* Determine which bin this interval falls into */
        size_t bin = (size_t)((double)pos / bin_size);
        if (bin >= bins) bin = bins - 1;
        
        /* Add density (normalized interval) */
        histogram[bin] += (double)intervals[i] / bin_size;
        
        pos += intervals[i];
    }
    
    return 0;
}

int uft_instab_phase_align(double *histogram, uint16_t bins,
                           const double *reference, double *shift)
{
    if (!histogram || !reference || bins == 0) return -1;
    
    /* Find best shift using cross-correlation */
    double best_corr = -2.0;
    int best_shift = 0;
    
    for (int s = -(int)(bins/4); s <= (int)(bins/4); s++) {
        double corr = 0.0;
        for (uint16_t i = 0; i < bins; i++) {
            int j = ((int)i + s + bins) % bins;
            corr += histogram[i] * reference[j];
        }
        
        if (corr > best_corr) {
            best_corr = corr;
            best_shift = s;
        }
    }
    
    /* Apply shift */
    if (best_shift != 0) {
        double *temp = calloc(bins, sizeof(double));
        if (temp) {
            for (uint16_t i = 0; i < bins; i++) {
                int j = ((int)i + best_shift + bins) % bins;
                temp[j] = histogram[i];
            }
            memcpy(histogram, temp, bins * sizeof(double));
            free(temp);
        }
    }
    
    if (shift) *shift = (double)best_shift / (double)bins * 360.0;
    return 0;
}

/*===========================================================================
 * Component Scores
 *===========================================================================*/

double uft_instab_phase_variance(const double *const *histograms, 
                                 uint8_t rev_count, uint16_t bins)
{
    if (!histograms || rev_count < 2 || bins == 0) return 0.0;
    
    double total_variance = 0.0;
    
    for (uint16_t bin = 0; bin < bins; bin++) {
        /* Collect values for this bin across revolutions */
        double sum = 0.0;
        for (uint8_t r = 0; r < rev_count; r++) {
            sum += histograms[r][bin];
        }
        double mean = sum / (double)rev_count;
        
        double var = 0.0;
        for (uint8_t r = 0; r < rev_count; r++) {
            double diff = histograms[r][bin] - mean;
            var += diff * diff;
        }
        var /= (double)(rev_count - 1);
        
        total_variance += var;
    }
    
    /* Normalize */
    return total_variance / (double)bins;
}

double uft_instab_coherence(const double *const *histograms,
                            uint8_t rev_count, uint16_t bins,
                            double *mean_profile)
{
    if (!histograms || rev_count < 2 || bins == 0) return 0.0;
    
    /* Calculate mean profile */
    double *profile = mean_profile;
    bool free_profile = false;
    
    if (!profile) {
        profile = calloc(bins, sizeof(double));
        if (!profile) return 0.0;
        free_profile = true;
    }
    
    for (uint16_t bin = 0; bin < bins; bin++) {
        double sum = 0.0;
        for (uint8_t r = 0; r < rev_count; r++) {
            sum += histograms[r][bin];
        }
        profile[bin] = sum / (double)rev_count;
    }
    
    /* Calculate coherence as average correlation to mean */
    double total_corr = 0.0;
    
    for (uint8_t r = 0; r < rev_count; r++) {
        double corr = calc_correlation(histograms[r], profile, bins);
        total_corr += corr;
    }
    
    if (free_profile) free(profile);
    
    return total_corr / (double)rev_count;
}

double uft_instab_outlier_rate(const uint32_t *intervals, size_t count,
                               double threshold_sigma)
{
    if (!intervals || count < 10) return 0.0;
    
    /* Calculate mean and std */
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += (double)intervals[i];
    }
    double mean = sum / (double)count;
    
    double var_sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = (double)intervals[i] - mean;
        var_sum += diff * diff;
    }
    double std = sqrt(var_sum / (double)(count - 1));
    
    if (std < 1e-10) return 0.0;
    
    /* Count outliers */
    size_t outliers = 0;
    double threshold = threshold_sigma * std;
    
    for (size_t i = 0; i < count; i++) {
        if (fabs((double)intervals[i] - mean) > threshold) {
            outliers++;
        }
    }
    
    return (double)outliers / (double)count;
}

double uft_instab_gap_rate(const uint32_t *intervals, size_t count,
                           uint32_t gap_threshold_ns)
{
    if (!intervals || count == 0) return 0.0;
    
    size_t gaps = 0;
    for (size_t i = 0; i < count; i++) {
        if (intervals[i] > gap_threshold_ns) {
            gaps++;
        }
    }
    
    return (double)gaps / (double)count;
}

/*===========================================================================
 * Main Analysis
 *===========================================================================*/

int uft_instab_analyze(const uft_instab_input_t *input,
                       const uft_instab_config_t *config,
                       uft_instab_result_t *result)
{
    if (!input || !config || !result) return -1;
    if (input->rev_count < config->min_revolutions) return -1;
    
    uint16_t bins = config->angular_bins;
    uint8_t rev_count = input->rev_count;
    
    /* Allocate per-revolution histograms */
    double **histograms = calloc(rev_count, sizeof(double *));
    if (!histograms) return -1;
    
    for (uint8_t r = 0; r < rev_count; r++) {
        histograms[r] = calloc(bins, sizeof(double));
        if (!histograms[r]) {
            for (uint8_t i = 0; i < r; i++) free(histograms[i]);
            free(histograms);
            return -1;
        }
    }
    
    /* Build angular histograms for each revolution */
    double *rev_times = calloc(rev_count, sizeof(double));
    if (!rev_times) {
        for (uint8_t r = 0; r < rev_count; r++) free(histograms[r]);
        free(histograms);
        return -1;
    }
    
    for (uint8_t r = 0; r < rev_count; r++) {
        uft_instab_angular_histogram(input->revolutions[r], input->rev_lengths[r],
                                     bins, histograms[r], &rev_times[r]);
    }
    
    /* Phase-align to first revolution */
    for (uint8_t r = 1; r < rev_count; r++) {
        uft_instab_phase_align(histograms[r], bins, histograms[0], NULL);
    }
    
    /* Calculate component scores */
    double phase_var = uft_instab_phase_variance((const double *const *)histograms, 
                                                  rev_count, bins);
    
    double coherence = uft_instab_coherence((const double *const *)histograms,
                                             rev_count, bins, result->phase_histogram);
    
    /* Calculate outlier and gap rates from all revolutions */
    double total_outlier = 0.0;
    double total_gap = 0.0;
    size_t total_intervals = 0;
    
    for (uint8_t r = 0; r < rev_count; r++) {
        total_outlier += uft_instab_outlier_rate(input->revolutions[r], 
                                                  input->rev_lengths[r],
                                                  config->outlier_threshold) 
                         * input->rev_lengths[r];
        total_gap += uft_instab_gap_rate(input->revolutions[r],
                                          input->rev_lengths[r],
                                          config->gap_threshold_ns)
                     * input->rev_lengths[r];
        total_intervals += input->rev_lengths[r];
    }
    
    double outlier_rate = total_outlier / (double)total_intervals;
    double gap_rate = total_gap / (double)total_intervals;
    
    /* Fill result */
    result->phase_variance = phase_var;
    result->cross_rev_coherence = coherence;
    result->outlier_rate = outlier_rate;
    result->gap_rate = gap_rate;
    result->revolutions_analyzed = rev_count;
    result->angular_bins = bins;
    
    /* Calculate combined instability score */
    /* Lower coherence = more instability, so we invert it */
    double incoherence = 1.0 - coherence;
    if (incoherence < 0.0) incoherence = 0.0;
    
    /* Normalize phase variance (typically 0-1 range) */
    double norm_phase_var = phase_var;
    if (norm_phase_var > 1.0) norm_phase_var = 1.0;
    
    result->instability_score = 
        config->phase_weight * norm_phase_var +
        config->coherence_weight * incoherence +
        config->gap_weight * gap_rate +
        config->outlier_weight * outlier_rate;
    
    /* Clamp to 0-1 */
    if (result->instability_score > 1.0) result->instability_score = 1.0;
    if (result->instability_score < 0.0) result->instability_score = 0.0;
    
    /* Set weak bit flag */
    result->weak_bits_detected = (result->instability_score > UFT_INSTAB_WEAK_THRESHOLD);
    
    /* Cleanup */
    for (uint8_t r = 0; r < rev_count; r++) free(histograms[r]);
    free(histograms);
    free(rev_times);
    
    return 0;
}

/*===========================================================================
 * Weak Bit Detection
 *===========================================================================*/

size_t uft_instab_find_weak_regions(const uft_instab_input_t *input,
                                    const uft_instab_config_t *config,
                                    uft_weak_region_t *regions,
                                    size_t max_regions)
{
    if (!input || !config || !regions || max_regions == 0) return 0;
    if (input->rev_count < 2) return 0;
    
    size_t region_count = 0;
    uint8_t rev_count = input->rev_count;
    
    /* Find minimum length across revolutions */
    size_t min_len = input->rev_lengths[0];
    for (uint8_t r = 1; r < rev_count; r++) {
        if (input->rev_lengths[r] < min_len) {
            min_len = input->rev_lengths[r];
        }
    }
    
    /* Compare bit-by-bit across revolutions */
    bool in_weak_region = false;
    size_t region_start = 0;
    double region_variance = 0.0;
    
    for (size_t i = 0; i < min_len && region_count < max_regions; i++) {
        /* Calculate variance at this position */
        double sum = 0.0;
        for (uint8_t r = 0; r < rev_count; r++) {
            sum += (double)input->revolutions[r][i];
        }
        double mean = sum / (double)rev_count;
        
        double var = 0.0;
        for (uint8_t r = 0; r < rev_count; r++) {
            double diff = (double)input->revolutions[r][i] - mean;
            var += diff * diff;
        }
        var /= (double)(rev_count - 1);
        
        /* Normalize variance */
        double norm_var = var / (mean * mean);
        bool is_unstable = (norm_var > 0.1);  /* 10% threshold */
        
        if (is_unstable && !in_weak_region) {
            /* Start new region */
            in_weak_region = true;
            region_start = i;
            region_variance = norm_var;
        } else if (is_unstable && in_weak_region) {
            /* Continue region */
            region_variance = (region_variance + norm_var) / 2.0;
        } else if (!is_unstable && in_weak_region) {
            /* End region */
            regions[region_count].start_bit = (uint32_t)region_start;
            regions[region_count].end_bit = (uint32_t)(i - 1);
            regions[region_count].variance = region_variance;
            regions[region_count].confidence = 1.0 - region_variance;
            if (regions[region_count].confidence < 0.0) {
                regions[region_count].confidence = 0.0;
            }
            region_count++;
            in_weak_region = false;
        }
    }
    
    /* Close final region if needed */
    if (in_weak_region && region_count < max_regions) {
        regions[region_count].start_bit = (uint32_t)region_start;
        regions[region_count].end_bit = (uint32_t)(min_len - 1);
        regions[region_count].variance = region_variance;
        regions[region_count].confidence = 1.0 - region_variance;
        region_count++;
    }
    
    return region_count;
}

/*===========================================================================
 * Export
 *===========================================================================*/

int uft_instab_export_json(const uft_instab_result_t *result, const char *path)
{
    if (!result || !path) return -1;
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"instability_score\": %.6f,\n", result->instability_score);
    fprintf(f, "  \"phase_variance\": %.6f,\n", result->phase_variance);
    fprintf(f, "  \"cross_rev_coherence\": %.6f,\n", result->cross_rev_coherence);
    fprintf(f, "  \"outlier_rate\": %.6f,\n", result->outlier_rate);
    fprintf(f, "  \"gap_rate\": %.6f,\n", result->gap_rate);
    fprintf(f, "  \"weak_bits_detected\": %s,\n", result->weak_bits_detected ? "true" : "false");
    fprintf(f, "  \"revolutions_analyzed\": %u,\n", result->revolutions_analyzed);
    fprintf(f, "  \"angular_bins\": %u,\n", result->angular_bins);
    
    /* Phase histogram */
    fprintf(f, "  \"phase_histogram\": [");
    for (uint16_t i = 0; i < result->angular_bins; i++) {
        fprintf(f, "%.4f", result->phase_histogram[i]);
        if (i < result->angular_bins - 1) fprintf(f, ", ");
    }
    fprintf(f, "]\n");
    
    fprintf(f, "}\n");
    
    fclose(f);
    return 0;
}

/*===========================================================================
 * Utilities
 *===========================================================================*/

const char *uft_instab_quality_desc(double score)
{
    if (score < 0.1) return "Excellent";
    if (score < 0.2) return "Good";
    if (score < 0.3) return "Fair";
    if (score < 0.5) return "Poor";
    if (score < 0.7) return "Bad";
    return "Critical";
}
