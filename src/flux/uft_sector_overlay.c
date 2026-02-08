/**
 * @file uft_sector_overlay.c
 * @brief Format-Agnostic Sector Detection Implementation
 * 
 * EXT4-003: Sector Overlay Detection
 * 
 * Detects sector boundaries without format knowledge using:
 * - Autocorrelation for periodic patterns
 * - Boundary contrast for GCR media
 * - Local maximum refinement
 */

#include "uft/flux/uft_sector_overlay.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/*===========================================================================
 * Configuration
 *===========================================================================*/

void uft_overlay_config_init(uft_overlay_config_t *config, uft_overlay_method_t method)
{
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    config->method = method;
    config->min_sector_bits = UFT_OVERLAY_MIN_SECTOR_BITS;
    config->max_sector_bits = UFT_OVERLAY_MAX_SECTOR_BITS;
    config->min_sectors = 1;
    config->max_sectors = 30;
    config->confidence_threshold = 0.5;
    config->refine_peaks = true;
}

/*===========================================================================
 * Memory Management
 *===========================================================================*/

uft_overlay_result_t *uft_overlay_alloc(uint8_t max_sectors)
{
    if (max_sectors == 0) max_sectors = 30;
    
    uft_overlay_result_t *result = calloc(1, sizeof(uft_overlay_result_t));
    if (!result) return NULL;
    
    result->sectors = calloc(max_sectors, sizeof(uft_detected_sector_t));
    if (!result->sectors) {
        free(result);
        return NULL;
    }
    
    result->capacity = max_sectors;
    return result;
}

void uft_overlay_free(uft_overlay_result_t *result)
{
    if (!result) return;
    free(result->sectors);
    free(result);
}

/*===========================================================================
 * Autocorrelation (Simple Implementation)
 *===========================================================================*/

/**
 * @brief Calculate autocorrelation at a given lag
 */
static double autocorrelation_at_lag(const double *data, size_t count, size_t lag)
{
    if (lag >= count) return 0.0;
    
    double sum = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;
    
    for (size_t i = 0; i + lag < count; i++) {
        sum += data[i] * data[i + lag];
        norm1 += data[i] * data[i];
        norm2 += data[i + lag] * data[i + lag];
    }
    
    if (norm1 < 1e-10 || norm2 < 1e-10) return 0.0;
    return sum / sqrt(norm1 * norm2);
}

int uft_overlay_autocorrelate(const double *signal, size_t count,
                              double *autocorr, size_t max_lag)
{
    if (!signal || !autocorr || count == 0 || max_lag == 0) return -1;
    
    for (size_t lag = 0; lag < max_lag && lag < count; lag++) {
        autocorr[lag] = autocorrelation_at_lag(signal, count, lag);
    }
    
    return 0;
}

/*===========================================================================
 * Peak Finding
 *===========================================================================*/

size_t uft_overlay_find_peaks(const double *data, size_t count,
                              uint32_t *positions, double *values,
                              size_t max_peaks, double min_height,
                              uint32_t min_distance)
{
    if (!data || !positions || count < 3 || max_peaks == 0) return 0;
    
    size_t peak_count = 0;
    
    for (size_t i = 1; i < count - 1 && peak_count < max_peaks; i++) {
        /* Check if local maximum */
        if (data[i] > data[i - 1] && data[i] > data[i + 1]) {
            /* Check minimum height */
            if (data[i] >= min_height) {
                /* Check minimum distance from previous peak */
                bool far_enough = true;
                if (peak_count > 0 && min_distance > 0) {
                    if (i - positions[peak_count - 1] < min_distance) {
                        /* Too close - check if this is higher */
                        if (data[i] > values[peak_count - 1]) {
                            /* Replace previous peak */
                            positions[peak_count - 1] = (uint32_t)i;
                            values[peak_count - 1] = data[i];
                        }
                        far_enough = false;
                    }
                }
                
                if (far_enough) {
                    positions[peak_count] = (uint32_t)i;
                    if (values) values[peak_count] = data[i];
                    peak_count++;
                }
            }
        }
    }
    
    return peak_count;
}

void uft_overlay_refine_peak(const double *data, size_t count,
                             uint32_t *position, double *value)
{
    if (!data || !position || count < 3) return;
    
    uint32_t pos = *position;
    if (pos == 0 || pos >= count - 1) return;
    
    /* Quadratic interpolation for sub-sample accuracy */
    double y0 = data[pos - 1];
    double y1 = data[pos];
    double y2 = data[pos + 1];
    
    double denom = y0 - 2.0 * y1 + y2;
    if (fabs(denom) < 1e-10) return;
    
    double delta = 0.5 * (y0 - y2) / denom;
    if (delta > 0.5) delta = 0.5;
    if (delta < -0.5) delta = -0.5;
    
    /* Update position (sub-sample) - we round for integer position */
    if (delta > 0.25 && pos < count - 1) {
        *position = pos + 1;
    } else if (delta < -0.25 && pos > 0) {
        *position = pos - 1;
    }
    
    /* Update value (interpolated) */
    if (value) {
        *value = y1 - 0.25 * (y0 - y2) * delta;
    }
}

/*===========================================================================
 * Density Calculation
 *===========================================================================*/

int uft_overlay_calc_density(const uint32_t *intervals, size_t count,
                             double *density, size_t density_len,
                             uint32_t window_size)
{
    if (!intervals || !density || count == 0 || density_len == 0) return -1;
    if (window_size == 0) window_size = 100;
    
    /* Calculate cumulative positions */
    uint64_t *positions = calloc(count + 1, sizeof(uint64_t));
    if (!positions) return -1;
    
    positions[0] = 0;
    for (size_t i = 0; i < count; i++) {
        positions[i + 1] = positions[i] + intervals[i];
    }
    
    uint64_t total_time = positions[count];
    double time_per_bin = (double)total_time / (double)density_len;
    
    /* Calculate density for each bin */
    size_t interval_idx = 0;
    for (size_t bin = 0; bin < density_len; bin++) {
        uint64_t bin_start = (uint64_t)(bin * time_per_bin);
        uint64_t bin_end = (uint64_t)((bin + 1) * time_per_bin);
        
        /* Count intervals in this bin */
        size_t bin_count = 0;
        while (interval_idx < count && positions[interval_idx] < bin_end) {
            if (positions[interval_idx] >= bin_start) {
                bin_count++;
            }
            interval_idx++;
        }
        
        density[bin] = (double)bin_count;
    }
    
    free(positions);
    return 0;
}

/*===========================================================================
 * MFM Detection (Autocorrelation-based)
 *===========================================================================*/

int uft_overlay_detect_mfm(const uint32_t *intervals, size_t count,
                           const uft_overlay_config_t *config,
                           uft_overlay_result_t *result)
{
    if (!intervals || !config || !result) return -1;
    
    /* Calculate density signal */
    size_t density_len = 4096;
    double *density = calloc(density_len, sizeof(double));
    if (!density) return -1;
    
    uft_overlay_calc_density(intervals, count, density, density_len, 100);
    
    /* Calculate autocorrelation */
    size_t max_lag = density_len / 2;
    double *autocorr = calloc(max_lag, sizeof(double));
    if (!autocorr) {
        free(density);
        return -1;
    }
    
    uft_overlay_autocorrelate(density, density_len, autocorr, max_lag);
    
    /* Find peaks in autocorrelation (skip lag 0) */
    size_t min_lag = config->min_sector_bits * density_len / (count * 8);
    if (min_lag < 10) min_lag = 10;
    
    uint32_t *peak_positions = calloc(config->max_sectors, sizeof(uint32_t));
    double *peak_values = calloc(config->max_sectors, sizeof(double));
    
    if (!peak_positions || !peak_values) {
        free(density);
        free(autocorr);
        free(peak_positions);
        free(peak_values);
        return -1;
    }
    
    /* Find first significant peak after min_lag */
    double max_val = 0.0;
    size_t max_pos = 0;
    
    for (size_t i = min_lag; i < max_lag; i++) {
        if (autocorr[i] > max_val) {
            max_val = autocorr[i];
            max_pos = i;
        }
    }
    
    if (max_val < config->confidence_threshold) {
        /* No clear periodicity found */
        result->sector_count = 0;
        result->overall_confidence = 0.0;
        result->detected_method = UFT_OVERLAY_AUTO;
        
        free(density);
        free(autocorr);
        free(peak_positions);
        free(peak_values);
        return 0;
    }
    
    /* The period corresponds to sector size */
    uint64_t total_time = 0;
    for (size_t i = 0; i < count; i++) {
        total_time += intervals[i];
    }
    
    double sector_time = (double)total_time * max_pos / density_len;
    uint32_t sector_bits = (uint32_t)(sector_time / (total_time / count / 8));
    
    /* Estimate number of sectors */
    uint8_t num_sectors = (uint8_t)(density_len / max_pos);
    if (num_sectors < config->min_sectors) num_sectors = config->min_sectors;
    if (num_sectors > config->max_sectors) num_sectors = config->max_sectors;
    
    /* Fill result */
    result->sector_count = num_sectors;
    result->overall_confidence = max_val;
    result->detected_method = UFT_OVERLAY_ACF;
    result->estimated_sector_bits = sector_bits;
    
    /* Generate sector boundaries */
    uint32_t sector_size = density_len / num_sectors;
    for (uint8_t s = 0; s < num_sectors && s < result->capacity; s++) {
        result->sectors[s].start_bit = s * sector_bits;
        result->sectors[s].end_bit = (s + 1) * sector_bits - 1;
        result->sectors[s].confidence = max_val;
        result->sectors[s].sector_id = s;
    }
    
    free(density);
    free(autocorr);
    free(peak_positions);
    free(peak_values);
    
    return 0;
}

/*===========================================================================
 * GCR Detection (Boundary Contrast)
 *===========================================================================*/

int uft_overlay_detect_gcr(const uint32_t *intervals, size_t count,
                           const uft_overlay_config_t *config,
                           uft_overlay_result_t *result)
{
    if (!intervals || !config || !result) return -1;
    
    /* Calculate density signal with finer resolution */
    size_t density_len = 8192;
    double *density = calloc(density_len, sizeof(double));
    if (!density) return -1;
    
    uft_overlay_calc_density(intervals, count, density, density_len, 50);
    
    /* Calculate gradient (boundary contrast) */
    double *gradient = calloc(density_len, sizeof(double));
    if (!gradient) {
        free(density);
        return -1;
    }
    
    for (size_t i = 1; i + 1 < density_len; i++) {
        gradient[i] = fabs(density[i + 1] - density[i - 1]);
    }
    
    /* Find peaks in gradient (sector boundaries) */
    uint32_t *boundaries = calloc(config->max_sectors + 1, sizeof(uint32_t));
    double *boundary_strength = calloc(config->max_sectors + 1, sizeof(double));
    
    if (!boundaries || !boundary_strength) {
        free(density);
        free(gradient);
        free(boundaries);
        free(boundary_strength);
        return -1;
    }
    
    size_t min_distance = config->min_sector_bits * density_len / (count * 8);
    if (min_distance < 100) min_distance = 100;
    
    /* Find maximum gradient value for threshold */
    double max_gradient = 0.0;
    for (size_t i = 0; i < density_len; i++) {
        if (gradient[i] > max_gradient) max_gradient = gradient[i];
    }
    
    double threshold = max_gradient * 0.3;
    
    size_t num_boundaries = uft_overlay_find_peaks(gradient, density_len,
                                                    boundaries, boundary_strength,
                                                    config->max_sectors + 1,
                                                    threshold, (uint32_t)min_distance);
    
    if (num_boundaries < 2) {
        result->sector_count = 0;
        result->overall_confidence = 0.0;
        result->detected_method = UFT_OVERLAY_BOUNDARY;
        
        free(density);
        free(gradient);
        free(boundaries);
        free(boundary_strength);
        return 0;
    }
    
    /* Convert boundaries to sectors */
    result->sector_count = (uint8_t)(num_boundaries - 1);
    if (result->sector_count > result->capacity) {
        result->sector_count = result->capacity;
    }
    
    /* Calculate total bits */
    uint64_t total_time = 0;
    for (size_t i = 0; i < count; i++) {
        total_time += intervals[i];
    }
    double bits_per_density = (double)(count * 8) / density_len;
    
    double total_confidence = 0.0;
    for (uint8_t s = 0; s < result->sector_count; s++) {
        result->sectors[s].start_bit = (uint32_t)(boundaries[s] * bits_per_density);
        result->sectors[s].end_bit = (uint32_t)(boundaries[s + 1] * bits_per_density) - 1;
        result->sectors[s].confidence = boundary_strength[s] / max_gradient;
        result->sectors[s].sector_id = s;
        total_confidence += result->sectors[s].confidence;
    }
    
    result->overall_confidence = total_confidence / result->sector_count;
    result->detected_method = UFT_OVERLAY_BOUNDARY;
    result->estimated_sector_bits = (result->sectors[0].end_bit - result->sectors[0].start_bit);
    
    free(density);
    free(gradient);
    free(boundaries);
    free(boundary_strength);
    
    return 0;
}

/*===========================================================================
 * Main Detection
 *===========================================================================*/

int uft_overlay_detect(const uint32_t *intervals, size_t count,
                       const uft_overlay_config_t *config,
                       uft_overlay_result_t *result)
{
    if (!intervals || !config || !result) return -1;
    
    switch (config->method) {
        case UFT_OVERLAY_ACF:
            return uft_overlay_detect_mfm(intervals, count, config, result);
            
        case UFT_OVERLAY_BOUNDARY:
            return uft_overlay_detect_gcr(intervals, count, config, result);
            
        case UFT_OVERLAY_AUTO:
        default:
            /* Try ACF first, fall back to boundary */
            if (uft_overlay_detect_mfm(intervals, count, config, result) == 0 &&
                result->overall_confidence > 0.5) {
                return 0;
            }
            return uft_overlay_detect_gcr(intervals, count, config, result);
    }
}

/*===========================================================================
 * Multi-Track Analysis
 *===========================================================================*/

int uft_overlay_multi_track(const uft_overlay_input_t *inputs, size_t track_count,
                            const uft_overlay_config_t *config,
                            uft_overlay_result_t *result)
{
    if (!inputs || !config || !result || track_count == 0) return -1;
    
    /* Analyze each track */
    double best_confidence = 0.0;
    uint32_t best_sector_bits = 0;
    uint8_t best_sector_count = 0;
    
    for (size_t t = 0; t < track_count; t++) {
        uft_overlay_result_t *track_result = uft_overlay_alloc(config->max_sectors);
        if (!track_result) continue;
        
        if (uft_overlay_detect(inputs[t].intervals, inputs[t].count, 
                               config, track_result) == 0) {
            if (track_result->overall_confidence > best_confidence) {
                best_confidence = track_result->overall_confidence;
                best_sector_bits = track_result->estimated_sector_bits;
                best_sector_count = track_result->sector_count;
            }
        }
        
        uft_overlay_free(track_result);
    }
    
    /* Use best result as template */
    result->sector_count = best_sector_count;
    result->overall_confidence = best_confidence;
    result->estimated_sector_bits = best_sector_bits;
    
    /* Generate uniform sectors based on average */
    for (uint8_t s = 0; s < result->sector_count && s < result->capacity; s++) {
        result->sectors[s].start_bit = s * best_sector_bits;
        result->sectors[s].end_bit = (s + 1) * best_sector_bits - 1;
        result->sectors[s].confidence = best_confidence;
        result->sectors[s].sector_id = s;
    }
    
    return 0;
}

/*===========================================================================
 * Export
 *===========================================================================*/

int uft_overlay_export_json(const uft_overlay_result_t *result, const char *path)
{
    if (!result || !path) return -1;
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    const char *method_name = "unknown";
    switch (result->detected_method) {
        case UFT_OVERLAY_FFT: method_name = "fft"; break;
        case UFT_OVERLAY_ACF: method_name = "acf"; break;
        case UFT_OVERLAY_BOUNDARY: method_name = "boundary"; break;
        case UFT_OVERLAY_AUTO: method_name = "auto"; break;
    }
    
    fprintf(f, "{\n");
    fprintf(f, "  \"sector_count\": %u,\n", result->sector_count);
    fprintf(f, "  \"overall_confidence\": %.4f,\n", result->overall_confidence);
    fprintf(f, "  \"detected_method\": \"%s\",\n", method_name);
    fprintf(f, "  \"estimated_sector_bits\": %u,\n", result->estimated_sector_bits);
    
    fprintf(f, "  \"sectors\": [\n");
    for (uint8_t s = 0; s < result->sector_count; s++) {
        fprintf(f, "    {\n");
        fprintf(f, "      \"id\": %u,\n", result->sectors[s].sector_id);
        fprintf(f, "      \"start_bit\": %u,\n", result->sectors[s].start_bit);
        fprintf(f, "      \"end_bit\": %u,\n", result->sectors[s].end_bit);
        fprintf(f, "      \"confidence\": %.4f\n", result->sectors[s].confidence);
        fprintf(f, "    }%s\n", (s < result->sector_count - 1) ? "," : "");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    
    fclose(f);
    return 0;
}
