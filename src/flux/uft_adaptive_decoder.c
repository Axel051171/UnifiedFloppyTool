/**
 * @file uft_adaptive_decoder.c
 * @brief Adaptive Flux Decoder with K-Means Clustering
 * 
 * EXT4-004: Adaptive Decoder Implementation
 */

#include "uft/flux/uft_adaptive_decoder.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/*===========================================================================
 * Configuration
 *===========================================================================*/

void uft_adec_config_init(uft_adec_config_t *config, uft_adec_mode_t mode)
{
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    config->mode = mode;
    config->nominal_cell_ns = UFT_ADEC_NOMINAL_CELL_NS;
    config->density = 1.0;
    config->rpm = UFT_ADEC_RPM_300;
    config->tolerance = UFT_ADEC_DEFAULT_TOLERANCE;
    config->use_clustering = true;
    config->num_clusters = (mode == UFT_ADEC_VARIABLE_RLL) ? 3 : 2;
    config->normalize_rpm = true;
}

/*===========================================================================
 * State Management
 *===========================================================================*/

int uft_adec_init(uft_adec_state_t *state, const uft_adec_config_t *config)
{
    if (!state || !config) return -1;
    
    memset(state, 0, sizeof(*state));
    state->config = *config;
    
    /* Initialize thresholds based on nominal cell */
    double base = config->nominal_cell_ns / config->density;
    state->thresholds.half_cell_ns = base / 2.0;
    state->thresholds.full_cell_ns = base;
    state->thresholds.short_cell_ns = base * 0.8;
    state->thresholds.long_cell_ns = base * 1.2;
    state->thresholds.half_short_ns = base * 0.4;
    state->thresholds.tolerance = config->tolerance;
    state->thresholds.from_clustering = false;
    
    return 0;
}

void uft_adec_reset(uft_adec_state_t *state)
{
    if (!state) return;
    
    state->mean_interval_ns = 0;
    state->std_interval_ns = 0;
    state->total_intervals = 0;
    state->decoded_bits = 0;
    state->error_count = 0;
    state->cluster_count = 0;
    state->thresholds.from_clustering = false;
}

/*===========================================================================
 * K-Means Clustering
 *===========================================================================*/

/**
 * @brief Find nearest cluster center
 */
static size_t find_nearest_cluster(uint32_t value, const double *centers, uint8_t k)
{
    size_t nearest = 0;
    double min_dist = DBL_MAX;
    
    for (uint8_t i = 0; i < k; i++) {
        double dist = fabs((double)value - centers[i]);
        if (dist < min_dist) {
            min_dist = dist;
            nearest = i;
        }
    }
    
    return nearest;
}

/**
 * @brief Compare function for sorting clusters
 */
static int compare_clusters(const void *a, const void *b)
{
    const uft_adec_cluster_t *ca = (const uft_adec_cluster_t *)a;
    const uft_adec_cluster_t *cb = (const uft_adec_cluster_t *)b;
    
    if (ca->center_ns < cb->center_ns) return -1;
    if (ca->center_ns > cb->center_ns) return 1;
    return 0;
}

uint8_t uft_adec_kmeans(const uint32_t *intervals, size_t count, uint8_t k,
                        uft_adec_cluster_t *clusters, uint16_t max_iterations)
{
    if (!intervals || count < UFT_ADEC_MIN_CLUSTER_SAMPLES || !clusters) return 0;
    if (k < 2) k = 2;
    if (k > UFT_ADEC_MAX_CLUSTERS) k = UFT_ADEC_MAX_CLUSTERS;
    if (max_iterations == 0) max_iterations = 100;
    
    /* Find min/max for initialization */
    uint32_t min_val = UINT32_MAX;
    uint32_t max_val = 0;
    for (size_t i = 0; i < count; i++) {
        if (intervals[i] < min_val) min_val = intervals[i];
        if (intervals[i] > max_val) max_val = intervals[i];
    }
    
    /* Initialize centers evenly spaced */
    double centers[UFT_ADEC_MAX_CLUSTERS];
    for (uint8_t i = 0; i < k; i++) {
        centers[i] = min_val + (double)(max_val - min_val) * (i + 0.5) / k;
    }
    
    /* Allocate assignment array */
    size_t *assignments = calloc(count, sizeof(size_t));
    if (!assignments) return 0;
    
    /* K-means iterations */
    for (uint16_t iter = 0; iter < max_iterations; iter++) {
        bool changed = false;
        
        /* Assign points to nearest cluster */
        for (size_t i = 0; i < count; i++) {
            size_t nearest = find_nearest_cluster(intervals[i], centers, k);
            if (assignments[i] != nearest) {
                assignments[i] = nearest;
                changed = true;
            }
        }
        
        if (!changed && iter > 0) break;
        
        /* Update centers */
        double sums[UFT_ADEC_MAX_CLUSTERS] = {0};
        uint32_t counts[UFT_ADEC_MAX_CLUSTERS] = {0};
        
        for (size_t i = 0; i < count; i++) {
            size_t c = assignments[i];
            sums[c] += intervals[i];
            counts[c]++;
        }
        
        for (uint8_t c = 0; c < k; c++) {
            if (counts[c] > 0) {
                centers[c] = sums[c] / counts[c];
            }
        }
    }
    
    /* Calculate final statistics for each cluster */
    for (uint8_t c = 0; c < k; c++) {
        clusters[c].center_ns = centers[c];
        clusters[c].count = 0;
        clusters[c].variance = 0;
        clusters[c].type = UFT_ADEC_INT_UNKNOWN;
    }
    
    /* Count and calculate variance */
    for (size_t i = 0; i < count; i++) {
        size_t c = assignments[i];
        clusters[c].count++;
    }
    
    /* Calculate variance per cluster */
    for (size_t i = 0; i < count; i++) {
        size_t c = assignments[i];
        double diff = (double)intervals[i] - clusters[c].center_ns;
        clusters[c].variance += diff * diff;
    }
    
    for (uint8_t c = 0; c < k; c++) {
        if (clusters[c].count > 1) {
            clusters[c].variance /= (clusters[c].count - 1);
        }
    }
    
    free(assignments);
    
    /* Sort clusters by center value (ascending) */
    qsort(clusters, k, sizeof(uft_adec_cluster_t), compare_clusters);
    
    return k;
}

void uft_adec_update_thresholds(uft_adec_state_t *state,
                                const uft_adec_cluster_t *clusters,
                                uint8_t cluster_count)
{
    if (!state || !clusters || cluster_count < 2) return;
    
    if (cluster_count == 2) {
        /* Manchester: half-cell and full-cell */
        state->thresholds.half_cell_ns = clusters[0].center_ns;
        state->thresholds.full_cell_ns = clusters[1].center_ns;
        state->config.mode = UFT_ADEC_MANCHESTER;
    } else if (cluster_count >= 3) {
        /* Variable RLL: half-short, short/long */
        state->thresholds.half_short_ns = clusters[0].center_ns;
        state->thresholds.short_cell_ns = clusters[1].center_ns;
        state->thresholds.long_cell_ns = clusters[cluster_count - 1].center_ns;
        state->config.mode = UFT_ADEC_VARIABLE_RLL;
    }
    
    /* Calculate tolerance from cluster variance */
    double total_variance = 0;
    uint32_t total_count = 0;
    for (uint8_t i = 0; i < cluster_count; i++) {
        total_variance += clusters[i].variance * clusters[i].count;
        total_count += clusters[i].count;
    }
    
    if (total_count > 0) {
        double avg_std = sqrt(total_variance / total_count);
        double avg_center = 0;
        for (uint8_t i = 0; i < cluster_count; i++) {
            avg_center += clusters[i].center_ns;
        }
        avg_center /= cluster_count;
        
        state->thresholds.tolerance = (avg_std / avg_center) * 2.0;
        if (state->thresholds.tolerance < 0.05) state->thresholds.tolerance = 0.05;
        if (state->thresholds.tolerance > 0.5) state->thresholds.tolerance = 0.5;
    }
    
    state->thresholds.from_clustering = true;
    state->cluster_count = cluster_count;
    memcpy(state->clusters, clusters, cluster_count * sizeof(uft_adec_cluster_t));
}

uft_adec_mode_t uft_adec_detect_mode(const uft_adec_cluster_t *clusters,
                                      uint8_t cluster_count)
{
    if (!clusters || cluster_count < 2) return UFT_ADEC_AUTO;
    
    if (cluster_count == 2) {
        /* Check ratio for Manchester vs. MFM */
        double ratio = clusters[1].center_ns / clusters[0].center_ns;
        if (ratio > 1.8 && ratio < 2.2) {
            return UFT_ADEC_MANCHESTER;
        } else if (ratio > 1.4 && ratio < 1.6) {
            return UFT_ADEC_MFM;
        }
    } else if (cluster_count >= 3) {
        /* Variable mode */
        return UFT_ADEC_VARIABLE_RLL;
    }
    
    return UFT_ADEC_AUTO;
}

/*===========================================================================
 * Interval Classification
 *===========================================================================*/

uft_adec_interval_t uft_adec_classify(const uft_adec_state_t *state,
                                       uint32_t interval_ns)
{
    if (!state) return UFT_ADEC_INT_UNKNOWN;
    
    const uft_adec_thresholds_t *t = &state->thresholds;
    double tol = t->tolerance;
    
    if (state->config.mode == UFT_ADEC_VARIABLE_RLL) {
        /* Variable mode thresholds */
        double hs_min = t->half_short_ns * (1.0 - tol);
        double hs_max = t->half_short_ns * (1.0 + tol);
        double long_min = t->long_cell_ns * (1.0 - tol);
        double long_max = t->long_cell_ns * (1.0 + tol);
        
        if (interval_ns >= hs_min && interval_ns <= hs_max) {
            return UFT_ADEC_INT_HALF_SHORT;
        }
        if (interval_ns >= long_min && interval_ns <= long_max) {
            return UFT_ADEC_INT_LONG;
        }
        if (interval_ns >= t->short_cell_ns * (1.0 - tol) &&
            interval_ns <= t->short_cell_ns * (1.0 + tol)) {
            return UFT_ADEC_INT_SHORT;
        }
    } else {
        /* Manchester mode */
        double half_min = t->half_cell_ns * (1.0 - tol);
        double half_max = t->half_cell_ns * (1.0 + tol);
        double full_min = t->full_cell_ns * (1.0 - tol);
        double full_max = t->full_cell_ns * (1.0 + tol);
        
        if (interval_ns >= half_min && interval_ns <= half_max) {
            return UFT_ADEC_INT_HALF;
        }
        if (interval_ns >= full_min && interval_ns <= full_max) {
            return UFT_ADEC_INT_FULL;
        }
    }
    
    /* Fallback: closest match */
    if (state->config.mode == UFT_ADEC_VARIABLE_RLL) {
        double d_hs = fabs((double)interval_ns - t->half_short_ns);
        double d_long = fabs((double)interval_ns - t->long_cell_ns);
        return (d_hs < d_long) ? UFT_ADEC_INT_HALF_SHORT : UFT_ADEC_INT_LONG;
    } else {
        double d_half = fabs((double)interval_ns - t->half_cell_ns);
        double d_full = fabs((double)interval_ns - t->full_cell_ns);
        return (d_half < d_full) ? UFT_ADEC_INT_HALF : UFT_ADEC_INT_FULL;
    }
}

uft_adec_interval_t uft_adec_classify_conf(const uft_adec_state_t *state,
                                            uint32_t interval_ns,
                                            double *confidence)
{
    uft_adec_interval_t type = uft_adec_classify(state, interval_ns);
    
    if (confidence) {
        /* Calculate confidence based on distance to expected */
        double expected = 0;
        switch (type) {
            case UFT_ADEC_INT_HALF:
                expected = state->thresholds.half_cell_ns;
                break;
            case UFT_ADEC_INT_FULL:
                expected = state->thresholds.full_cell_ns;
                break;
            case UFT_ADEC_INT_HALF_SHORT:
                expected = state->thresholds.half_short_ns;
                break;
            case UFT_ADEC_INT_LONG:
                expected = state->thresholds.long_cell_ns;
                break;
            default:
                expected = state->thresholds.full_cell_ns;
                break;
        }
        
        double deviation = fabs((double)interval_ns - expected) / expected;
        *confidence = 1.0 - fmin(1.0, deviation / state->thresholds.tolerance);
    }
    
    return type;
}

/*===========================================================================
 * Learning and Decoding
 *===========================================================================*/

int uft_adec_learn(uft_adec_state_t *state, const uint32_t *intervals,
                   size_t count)
{
    if (!state || !intervals || count < UFT_ADEC_MIN_CLUSTER_SAMPLES) return -1;
    
    /* Calculate statistics */
    uft_adec_statistics(intervals, count,
                        &state->mean_interval_ns, &state->std_interval_ns,
                        NULL, NULL);
    state->total_intervals = (uint32_t)count;
    
    /* Perform K-means clustering */
    if (state->config.use_clustering) {
        uft_adec_cluster_t clusters[UFT_ADEC_MAX_CLUSTERS];
        uint8_t k = uft_adec_kmeans(intervals, count, state->config.num_clusters,
                                     clusters, 100);
        
        if (k >= 2) {
            uft_adec_update_thresholds(state, clusters, k);
        }
    }
    
    return 0;
}

size_t uft_adec_decode_manchester(const uft_adec_state_t *state,
                                  const uint32_t *intervals, size_t count,
                                  uint8_t *bits, size_t max_bits)
{
    if (!state || !intervals || !bits || count == 0 || max_bits == 0) return 0;
    
    size_t bit_count = 0;
    size_t i = 0;
    
    while (i < count && bit_count < max_bits) {
        uft_adec_interval_t type = uft_adec_classify(state, intervals[i]);
        
        if (type == UFT_ADEC_INT_FULL) {
            /* Full cell = 1 */
            bits[bit_count++] = 1;
            i++;
        } else if (type == UFT_ADEC_INT_HALF) {
            /* Half cell - check for pair */
            if (i + 1 < count) {
                uft_adec_interval_t next = uft_adec_classify(state, intervals[i + 1]);
                if (next == UFT_ADEC_INT_HALF) {
                    /* Two half cells = 0 */
                    bits[bit_count++] = 0;
                    i += 2;
                } else {
                    /* Mismatch - assume 1 */
                    bits[bit_count++] = 1;
                    i++;
                }
            } else {
                bits[bit_count++] = 1;
                i++;
            }
        } else {
            /* Unknown - assume 1 */
            bits[bit_count++] = 1;
            i++;
        }
    }
    
    return bit_count;
}

size_t uft_adec_decode_variable(const uft_adec_state_t *state,
                                const uint32_t *intervals, size_t count,
                                uint8_t *bits, size_t max_bits)
{
    if (!state || !intervals || !bits || count == 0 || max_bits == 0) return 0;
    
    size_t bit_count = 0;
    size_t i = 0;
    
    while (i < count && bit_count < max_bits) {
        uft_adec_interval_t type = uft_adec_classify(state, intervals[i]);
        
        if (type == UFT_ADEC_INT_LONG) {
            /* Long = 1 */
            bits[bit_count++] = 1;
            i++;
        } else if (type == UFT_ADEC_INT_HALF_SHORT) {
            /* Half-short - check for pair */
            if (i + 1 < count) {
                uft_adec_interval_t next = uft_adec_classify(state, intervals[i + 1]);
                if (next == UFT_ADEC_INT_HALF_SHORT) {
                    /* Two half-shorts = 0 */
                    bits[bit_count++] = 0;
                    i += 2;
                } else {
                    /* Mismatch */
                    bits[bit_count++] = 0;
                    i++;
                }
            } else {
                bits[bit_count++] = 0;
                i++;
            }
        } else {
            /* Unknown */
            bits[bit_count++] = 0;
            i++;
        }
    }
    
    return bit_count;
}

int uft_adec_decode(uft_adec_state_t *state, const uint32_t *intervals,
                    size_t count, uft_adec_result_t *result)
{
    if (!state || !intervals || !result || count == 0) return -1;
    
    /* Allocate bit buffer */
    size_t max_bits = count * 2;
    uint8_t *bits = calloc(max_bits, sizeof(uint8_t));
    if (!bits) return -1;
    
    /* Decode based on mode */
    size_t bit_count;
    if (state->config.mode == UFT_ADEC_VARIABLE_RLL) {
        bit_count = uft_adec_decode_variable(state, intervals, count, bits, max_bits);
    } else {
        bit_count = uft_adec_decode_manchester(state, intervals, count, bits, max_bits);
    }
    
    /* Pack bits to bytes */
    size_t byte_count = bit_count / 8;
    if (result->data && byte_count > 0) {
        uft_adec_pack_bits(bits, bit_count, result->data);
    }
    
    result->bit_count = bit_count;
    result->byte_count = byte_count;
    result->error_count = state->error_count;
    result->confidence = state->thresholds.from_clustering ? 0.9 : 0.7;
    
    state->decoded_bits = (uint32_t)bit_count;
    
    free(bits);
    return 0;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

void uft_adec_statistics(const uint32_t *intervals, size_t count,
                         double *mean, double *std,
                         uint32_t *min_val, uint32_t *max_val)
{
    if (!intervals || count == 0) return;
    
    /* First pass: mean, min, max */
    uint64_t sum = 0;
    uint32_t min_v = UINT32_MAX;
    uint32_t max_v = 0;
    
    for (size_t i = 0; i < count; i++) {
        sum += intervals[i];
        if (intervals[i] < min_v) min_v = intervals[i];
        if (intervals[i] > max_v) max_v = intervals[i];
    }
    
    double m = (double)sum / (double)count;
    if (mean) *mean = m;
    if (min_val) *min_val = min_v;
    if (max_val) *max_val = max_v;
    
    /* Second pass: standard deviation */
    if (std) {
        double var_sum = 0;
        for (size_t i = 0; i < count; i++) {
            double diff = (double)intervals[i] - m;
            var_sum += diff * diff;
        }
        *std = sqrt(var_sum / (double)count);
    }
}

void uft_adec_get_stats(const uft_adec_state_t *state,
                        uint32_t *total_bits, double *error_rate,
                        double *mean_confidence)
{
    if (!state) return;
    
    if (total_bits) *total_bits = state->decoded_bits;
    
    if (error_rate) {
        if (state->decoded_bits > 0) {
            *error_rate = (double)state->error_count / (double)state->decoded_bits;
        } else {
            *error_rate = 0.0;
        }
    }
    
    if (mean_confidence) {
        *mean_confidence = state->thresholds.from_clustering ? 0.9 : 0.7;
    }
}

/*===========================================================================
 * RPM Normalization
 *===========================================================================*/

double uft_adec_normalize_rpm(uint32_t *intervals, size_t count,
                              double measured_rpm, double target_rpm)
{
    if (!intervals || count == 0 || measured_rpm <= 0 || target_rpm <= 0) return 1.0;
    
    double scale = target_rpm / measured_rpm;
    
    for (size_t i = 0; i < count; i++) {
        double new_val = (double)intervals[i] * scale;
        intervals[i] = (new_val > 0) ? (uint32_t)new_val : 1;
    }
    
    return scale;
}

double uft_adec_estimate_rpm(const uint64_t *rev_intervals, size_t rev_count)
{
    if (!rev_intervals || rev_count == 0) return 0.0;
    
    uint64_t sum = 0;
    for (size_t i = 0; i < rev_count; i++) {
        sum += rev_intervals[i];
    }
    
    double mean_rev_ns = (double)sum / (double)rev_count;
    
    /* RPM = 60e9 / revolution_time_ns */
    return 60000000000.0 / mean_rev_ns;
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

const char *uft_adec_mode_name(uft_adec_mode_t mode)
{
    switch (mode) {
        case UFT_ADEC_MANCHESTER:    return "Manchester";
        case UFT_ADEC_VARIABLE_RLL:  return "Variable RLL";
        case UFT_ADEC_MFM:           return "MFM";
        case UFT_ADEC_FM:            return "FM";
        case UFT_ADEC_GCR:           return "GCR";
        case UFT_ADEC_AUTO:          return "Auto-Detect";
        default:                     return "Unknown";
    }
}

const char *uft_adec_interval_name(uft_adec_interval_t type)
{
    switch (type) {
        case UFT_ADEC_INT_HALF:       return "Half";
        case UFT_ADEC_INT_FULL:       return "Full";
        case UFT_ADEC_INT_SHORT:      return "Short";
        case UFT_ADEC_INT_LONG:       return "Long";
        case UFT_ADEC_INT_HALF_SHORT: return "Half-Short";
        case UFT_ADEC_INT_UNKNOWN:    return "Unknown";
        default:                      return "Invalid";
    }
}
