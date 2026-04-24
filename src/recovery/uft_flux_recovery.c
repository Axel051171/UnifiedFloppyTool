/**
 * @file uft_flux_recovery.c
 * @brief UFT Flux-Level Recovery Module
 * 
 * Advanced flux-level data recovery for damaged or degraded media:
 * - Multi-revolution analysis and voting
 * - Weak bit detection and interpolation
 * - Timing jitter compensation
 * - Signal quality assessment
 * 
 * @version 5.28.0 GOD MODE
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Types
 * ============================================================================ */

typedef struct {
    uint32_t *flux_times;
    size_t    flux_count;
    double    index_time;
    uint8_t   revolution;
} uft_flux_rev_t;

typedef struct {
    double timing_variance;
    double signal_strength;
    double noise_level;
    uint32_t weak_bit_count;
    uint32_t error_count;
    uint8_t  confidence;        /* 0-100 */
} uft_flux_quality_t;

typedef struct {
    uint8_t *recovered_bits;
    size_t   bit_count;
    uint8_t *confidence_map;    /* Per-bit confidence 0-255 */
    uint32_t corrections;
    uft_flux_quality_t quality;
} uft_flux_recovery_result_t;

typedef struct {
    /* Multi-revolution settings */
    size_t   min_revolutions;   /* Minimum revolutions for voting */
    size_t   max_revolutions;   /* Maximum revolutions to use */
    
    /* Timing settings */
    double   timing_tolerance;  /* ns tolerance for matching */
    double   jitter_threshold;  /* Jitter detection threshold */
    
    /* Recovery aggressiveness */
    uint8_t  recovery_level;    /* 0=conservative, 3=aggressive */
    bool     interpolate_weak;  /* Interpolate weak bits */
    bool     use_pll_recovery;  /* Use PLL for recovery */
} uft_flux_recovery_config_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FLUX_WINDOW_SIZE    16
#define MIN_CONFIDENCE      50
#define WEAK_BIT_THRESHOLD  0.3
#define JITTER_WINDOW       8

/* ============================================================================
 * Quality Assessment
 * ============================================================================ */

/**
 * @brief Analyze flux timing histogram
 */
static void analyze_flux_histogram(const uint32_t *flux_times, size_t count,
                                   double *mean, double *stddev)
{
    if (!flux_times || count == 0) {
        *mean = 0;
        *stddev = 0;
        return;
    }
    
    /* Calculate mean */
    double sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += flux_times[i];
    }
    *mean = sum / count;
    
    /* Calculate standard deviation */
    double variance = 0;
    for (size_t i = 0; i < count; i++) {
        double diff = flux_times[i] - *mean;
        variance += diff * diff;
    }
    *stddev = sqrt(variance / count);
}

/**
 * @brief Assess flux quality for a revolution
 */
static void assess_flux_quality(const uft_flux_rev_t *rev, 
                                uft_flux_quality_t *quality)
{
    double mean, stddev;
    analyze_flux_histogram(rev->flux_times, rev->flux_count, &mean, &stddev);
    
    quality->timing_variance = stddev / mean;  /* Coefficient of variation */
    quality->signal_strength = 1.0 / (1.0 + quality->timing_variance);
    quality->noise_level = quality->timing_variance * 100;
    
    /* Count weak bits (high jitter regions) */
    quality->weak_bit_count = 0;
    for (size_t i = JITTER_WINDOW; i < rev->flux_count - JITTER_WINDOW; i++) {
        double local_var = 0;
        for (int j = -JITTER_WINDOW/2; j <= JITTER_WINDOW/2; j++) {
            double diff = rev->flux_times[i+j] - mean;
            local_var += diff * diff;
        }
        local_var /= JITTER_WINDOW;
        
        if (sqrt(local_var) / mean > WEAK_BIT_THRESHOLD) {
            quality->weak_bit_count++;
        }
    }
    
    /* Calculate overall confidence */
    double conf = 100.0 * quality->signal_strength;
    conf -= quality->weak_bit_count * 0.1;
    if (conf < 0) conf = 0;
    if (conf > 100) conf = 100;
    quality->confidence = (uint8_t)conf;
}

/* ============================================================================
 * Multi-Revolution Voting
 * ============================================================================ */

/**
 * @brief Align flux revolutions by index time
 */
static int align_revolutions(const uft_flux_rev_t *revs, size_t rev_count,
                             size_t **alignment_offsets)
{
    *alignment_offsets = calloc(rev_count, sizeof(size_t));
    if (!*alignment_offsets) return -1;
    
    /* Use first revolution as reference */
    (*alignment_offsets)[0] = 0;
    
    for (size_t r = 1; r < rev_count; r++) {
        /* Find best alignment based on correlation */
        double best_corr = 0;
        size_t best_offset = 0;
        
        size_t max_offset = revs[r].flux_count / 10;  /* Max 10% shift */
        
        for (size_t off = 0; off < max_offset; off++) {
            double corr = 0;
            size_t samples = 0;
            
            for (size_t i = 0; i < revs[0].flux_count && i + off < revs[r].flux_count; i++) {
                double diff = (double)revs[0].flux_times[i] - revs[r].flux_times[i + off];
                corr += 1.0 / (1.0 + fabs(diff) / 100.0);
                samples++;
            }
            
            if (samples > 0) {
                corr /= samples;
                if (corr > best_corr) {
                    best_corr = corr;
                    best_offset = off;
                }
            }
        }
        
        (*alignment_offsets)[r] = best_offset;
    }
    
    return 0;
}

/**
 * @brief Vote on flux timing across revolutions
 */
static uint32_t vote_flux_timing(const uft_flux_rev_t *revs, size_t rev_count,
                                 const size_t *offsets, size_t position,
                                 uint8_t *confidence)
{
    uint32_t values[16];  /* Max 16 revolutions */
    size_t valid_count = 0;
    
    for (size_t r = 0; r < rev_count && r < 16; r++) {
        size_t pos = position + offsets[r];
        if (pos < revs[r].flux_count) {
            values[valid_count++] = revs[r].flux_times[pos];
        }
    }
    
    if (valid_count == 0) {
        *confidence = 0;
        return 0;
    }
    
    if (valid_count == 1) {
        *confidence = 50;
        return values[0];
    }
    
    /* Sort and take median */
    for (size_t i = 0; i < valid_count - 1; i++) {
        for (size_t j = i + 1; j < valid_count; j++) {
            if (values[j] < values[i]) {
                uint32_t tmp = values[i];
                values[i] = values[j];
                values[j] = tmp;
            }
        }
    }
    
    uint32_t median = values[valid_count / 2];
    
    /* Calculate confidence based on agreement */
    double agreement = 0;
    for (size_t i = 0; i < valid_count; i++) {
        double diff = fabs((double)values[i] - median);
        agreement += 1.0 / (1.0 + diff / 50.0);
    }
    agreement /= valid_count;
    
    *confidence = (uint8_t)(agreement * 100);
    
    return median;
}

/* ============================================================================
 * Weak Bit Recovery
 * ============================================================================ */

/**
 * @brief Detect weak bits in flux stream
 */
static void detect_weak_bits(const uint32_t *flux_times, size_t count,
                             uint8_t *weak_map, double clock_ns)
{
    memset(weak_map, 0, count);
    
    double expected_2T = clock_ns * 2.0;
    double expected_3T = clock_ns * 3.0;
    double expected_4T = clock_ns * 4.0;
    
    for (size_t i = 0; i < count; i++) {
        double t = flux_times[i];
        
        /* Check if timing is between expected values */
        double dist_2T = fabs(t - expected_2T);
        double dist_3T = fabs(t - expected_3T);
        double dist_4T = fabs(t - expected_4T);
        
        double min_dist = dist_2T;
        if (dist_3T < min_dist) min_dist = dist_3T;
        if (dist_4T < min_dist) min_dist = dist_4T;
        
        /* If far from all expected values, it's weak */
        double threshold = clock_ns * 0.4;  /* 40% of cell */
        if (min_dist > threshold) {
            weak_map[i] = 255;  /* Definitely weak */
        } else if (min_dist > threshold * 0.5) {
            weak_map[i] = 128;  /* Possibly weak */
        }
    }
}

/**
 * @brief Interpolate weak bits using context
 */
static uint32_t interpolate_weak_bit(const uint32_t *flux_times, size_t count,
                                     size_t position, double clock_ns)
{
    /* Look at surrounding flux timings */
    double sum = 0;
    int samples = 0;
    
    /* Look back */
    for (int i = 1; i <= 4 && (int)position - i >= 0; i++) {
        sum += flux_times[position - i];
        samples++;
    }
    
    /* Look forward */
    for (int i = 1; i <= 4 && position + i < count; i++) {
        sum += flux_times[position + i];
        samples++;
    }
    
    if (samples == 0) {
        return flux_times[position];  /* Can't interpolate */
    }
    
    double avg = sum / samples;
    
    /* Quantize to nearest expected value */
    double expected_2T = clock_ns * 2.0;
    double expected_3T = clock_ns * 3.0;
    double expected_4T = clock_ns * 4.0;
    
    double dist_2T = fabs(avg - expected_2T);
    double dist_3T = fabs(avg - expected_3T);
    double dist_4T = fabs(avg - expected_4T);
    
    if (dist_2T <= dist_3T && dist_2T <= dist_4T) {
        return (uint32_t)expected_2T;
    } else if (dist_3T <= dist_4T) {
        return (uint32_t)expected_3T;
    } else {
        return (uint32_t)expected_4T;
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize flux recovery configuration with defaults
 */
void uft_flux_recovery_config_init(uft_flux_recovery_config_t *config)
{
    config->min_revolutions = 2;
    config->max_revolutions = 5;
    config->timing_tolerance = 100.0;  /* 100ns */
    config->jitter_threshold = 0.2;
    config->recovery_level = 1;  /* Moderate */
    config->interpolate_weak = true;
    config->use_pll_recovery = true;
}

/**
 * @brief Recover flux data using multi-revolution voting
 */
int uft_flux_recover_multi_rev(const uft_flux_rev_t *revs, size_t rev_count,
                               const uft_flux_recovery_config_t *config,
                               uint32_t **recovered_flux, size_t *recovered_count,
                               uft_flux_quality_t *quality)
{
    if (!revs || rev_count == 0 || !config || !recovered_flux || !recovered_count) {
        return -1;
    }
    
    /* Align revolutions */
    size_t *offsets;
    if (align_revolutions(revs, rev_count, &offsets) != 0) {
        return -1;
    }
    
    /* Use shortest revolution as output size */
    size_t out_count = revs[0].flux_count;
    for (size_t r = 1; r < rev_count; r++) {
        size_t available = revs[r].flux_count - offsets[r];
        if (available < out_count) {
            out_count = available;
        }
    }
    
    /* Allocate output */
    *recovered_flux = malloc(out_count * sizeof(uint32_t));
    uint8_t *conf_map = malloc(out_count);
    if (!*recovered_flux || !conf_map) {
        free(offsets);
        free(*recovered_flux);
        free(conf_map);
        return -1;
    }
    
    /* Vote on each flux transition */
    uint32_t low_conf_count = 0;
    for (size_t i = 0; i < out_count; i++) {
        (*recovered_flux)[i] = vote_flux_timing(revs, rev_count, offsets, i, &conf_map[i]);
        if (conf_map[i] < MIN_CONFIDENCE) {
            low_conf_count++;
        }
    }
    
    *recovered_count = out_count;
    
    /* Calculate quality */
    if (quality) {
        uft_flux_rev_t recovered_rev = {
            .flux_times = *recovered_flux,
            .flux_count = out_count,
            .index_time = revs[0].index_time,
            .revolution = 0
        };
        assess_flux_quality(&recovered_rev, quality);
        quality->error_count = low_conf_count;
    }
    
    free(offsets);
    free(conf_map);
    
    return 0;
}

/**
 * @brief Recover weak bits in flux stream
 */
int uft_flux_recover_weak_bits(uint32_t *flux_times, size_t count,
                               double clock_ns, uint32_t *corrections)
{
    if (!flux_times || count == 0) return -1;
    
    uint8_t *weak_map = malloc(count);
    if (!weak_map) return -1;
    
    /* Detect weak bits */
    detect_weak_bits(flux_times, count, weak_map, clock_ns);
    
    /* Interpolate weak bits */
    uint32_t fixed = 0;
    for (size_t i = 0; i < count; i++) {
        if (weak_map[i] > 128) {
            uint32_t orig = flux_times[i];
            flux_times[i] = interpolate_weak_bit(flux_times, count, i, clock_ns);
            if (flux_times[i] != orig) {
                fixed++;
            }
        }
    }
    
    if (corrections) {
        *corrections = fixed;
    }
    
    free(weak_map);
    return 0;
}

/**
 * @brief Get quality assessment for flux stream
 */
int uft_flux_assess_quality(const uint32_t *flux_times, size_t count,
                            double index_time, uft_flux_quality_t *quality)
{
    if (!flux_times || count == 0 || !quality) return -1;
    
    uft_flux_rev_t rev = {
        .flux_times = (uint32_t*)flux_times,
        .flux_count = count,
        .index_time = index_time,
        .revolution = 0
    };
    
    assess_flux_quality(&rev, quality);
    return 0;
}
