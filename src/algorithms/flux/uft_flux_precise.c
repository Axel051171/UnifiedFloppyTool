/**
 * @file uft_flux_precise.c
 * @brief High-precision flux timing implementation
 */

#include "uft_flux_precise.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Buffer Management
 * ============================================================================ */

int uft_flux_buffer_init(uft_flux_buffer_t *buf,
                         size_t capacity,
                         double sample_rate) {
    if (!buf || capacity == 0 || sample_rate <= 0) return -1;
    
    memset(buf, 0, sizeof(*buf));
    
    buf->samples = calloc(capacity, sizeof(uft_flux_sample_t));
    if (!buf->samples) return -2;
    
    buf->capacity = capacity;
    buf->sample_rate = sample_rate;
    buf->ns_per_sample = 1e9 / sample_rate;
    
    return 0;
}

void uft_flux_buffer_free(uft_flux_buffer_t *buf) {
    if (!buf) return;
    free(buf->samples);
    memset(buf, 0, sizeof(*buf));
}

void uft_flux_buffer_clear(uft_flux_buffer_t *buf) {
    if (!buf) return;
    buf->count = 0;
    buf->index_count = 0;
}

int uft_flux_buffer_add(uft_flux_buffer_t *buf,
                        uint64_t timestamp_ns,
                        double fractional,
                        uint8_t flags) {
    if (!buf) return -1;
    
    /* Grow if needed */
    if (buf->count >= buf->capacity) {
        size_t new_cap = buf->capacity * 2;
        uft_flux_sample_t *new_samples = realloc(buf->samples,
            new_cap * sizeof(uft_flux_sample_t));
        if (!new_samples) return -2;
        buf->samples = new_samples;
        buf->capacity = new_cap;
    }
    
    uft_flux_sample_t *s = &buf->samples[buf->count++];
    s->timestamp_ns = timestamp_ns;
    s->fractional = fractional;
    s->flags = flags;
    
    if (flags & UFT_FLUX_FLAG_INDEX) {
        buf->index_count++;
    }
    
    return 0;
}

int uft_flux_buffer_add_sample(uft_flux_buffer_t *buf,
                               double sample_position,
                               uint8_t flags) {
    if (!buf) return -1;
    
    /* Convert sample position to nanoseconds */
    double time_ns = sample_position * buf->ns_per_sample;
    uint64_t int_ns = (uint64_t)time_ns;
    double frac = time_ns - int_ns;
    
    return uft_flux_buffer_add(buf, int_ns, frac, flags);
}

/* ============================================================================
 * Precision Timing
 * ============================================================================ */

double uft_flux_get_time(const uft_flux_buffer_t *buf, double index) {
    if (!buf || buf->count == 0) return 0;
    
    size_t idx = (size_t)index;
    double frac = index - idx;
    
    if (idx >= buf->count) {
        idx = buf->count - 1;
        frac = 0;
    }
    
    double t0 = buf->samples[idx].timestamp_ns + buf->samples[idx].fractional;
    
    /* Interpolate if fractional and next sample exists */
    if (frac > 0 && idx + 1 < buf->count) {
        double t1 = buf->samples[idx + 1].timestamp_ns + 
                    buf->samples[idx + 1].fractional;
        return t0 + (t1 - t0) * frac;
    }
    
    return t0;
}

double uft_flux_interpolate_position(const uft_flux_buffer_t *buf,
                                     double time_ns) {
    if (!buf || buf->count == 0) return 0;
    
    /* Binary search for position */
    size_t lo = 0, hi = buf->count - 1;
    
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        double mid_time = buf->samples[mid].timestamp_ns + 
                         buf->samples[mid].fractional;
        
        if (mid_time < time_ns) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    
    if (lo == 0) return 0;
    
    /* Interpolate between lo-1 and lo */
    double t0 = buf->samples[lo - 1].timestamp_ns + buf->samples[lo - 1].fractional;
    double t1 = buf->samples[lo].timestamp_ns + buf->samples[lo].fractional;
    
    if (t1 == t0) return (double)lo;
    
    double frac = (time_ns - t0) / (t1 - t0);
    return (lo - 1) + frac;
}

double uft_flux_delta(const uft_flux_sample_t *a,
                      const uft_flux_sample_t *b) {
    if (!a || !b) return 0;
    
    double ta = a->timestamp_ns + a->fractional;
    double tb = b->timestamp_ns + b->fractional;
    
    return tb - ta;
}

/* ============================================================================
 * Flux-to-Bit Conversion
 * ============================================================================ */

void uft_flux_converter_init(uft_flux_converter_t *conv,
                             double cell_time_ns,
                             double tolerance) {
    if (!conv) return;
    
    memset(conv, 0, sizeof(*conv));
    conv->cell_time_ns = cell_time_ns;
    conv->cell_tolerance = tolerance;
    conv->error_threshold = cell_time_ns * 0.1;  /* 10% threshold */
}

void uft_flux_converter_reset(uft_flux_converter_t *conv) {
    if (!conv) return;
    
    conv->accumulated_error = 0;
    conv->max_error = 0;
    conv->drift_compensation = 0;
    conv->pulses_processed = 0;
    conv->bits_generated = 0;
    conv->corrections_applied = 0;
}

int uft_flux_delta_to_bits(uft_flux_converter_t *conv,
                           double delta_ns,
                           uint8_t *out_bits) {
    if (!conv || delta_ns <= 0) return 0;
    
    /* Add accumulated error */
    double adj_delta = delta_ns + conv->accumulated_error;
    
    /* Calculate number of cells */
    double cells_f = adj_delta / conv->cell_time_ns;
    int cells = (int)(cells_f + 0.5);  /* Round */
    
    if (cells < 1) cells = 1;
    if (cells > 8) cells = 8;  /* Limit to reasonable value */
    
    /* Update accumulated error */
    double used_time = cells * conv->cell_time_ns;
    conv->accumulated_error = adj_delta - used_time;
    
    /* Track max error */
    if (fabs(conv->accumulated_error) > conv->max_error) {
        conv->max_error = fabs(conv->accumulated_error);
    }
    
    /* Apply correction if error too large */
    if (fabs(conv->accumulated_error) > conv->error_threshold) {
        /* Snap to nearest cell boundary */
        conv->accumulated_error = 0;
        conv->corrections_applied++;
    }
    
    /* Generate bits: (cells-1) zeros, then one 1 */
    if (out_bits) {
        *out_bits = 0;
        for (int i = 0; i < cells - 1; i++) {
            /* Zeros - nothing to set in byte */
        }
        /* Set the final 1 bit at position (cells-1) from MSB */
        *out_bits = 1 << (8 - cells);
    }
    
    conv->pulses_processed++;
    conv->bits_generated += cells;
    
    return cells;
}

size_t uft_flux_to_bits(uft_flux_converter_t *conv,
                        const uft_flux_buffer_t *flux,
                        uint8_t *out_bits,
                        size_t out_capacity,
                        double *out_residual) {
    if (!conv || !flux || !out_bits || flux->count < 2) return 0;
    
    memset(out_bits, 0, out_capacity);
    
    size_t bit_pos = 0;
    size_t max_bits = out_capacity * 8;
    
    for (size_t i = 1; i < flux->count && bit_pos < max_bits; i++) {
        double delta = uft_flux_delta(&flux->samples[i-1], &flux->samples[i]);
        
        uint8_t bits;
        int num_bits = uft_flux_delta_to_bits(conv, delta, &bits);
        
        /* Insert bits into output */
        for (int b = 0; b < num_bits && bit_pos < max_bits; b++) {
            int bit_val = (b == num_bits - 1) ? 1 : 0;  /* Last bit is 1 */
            
            size_t byte_idx = bit_pos / 8;
            int bit_idx = 7 - (bit_pos % 8);
            
            if (bit_val) {
                out_bits[byte_idx] |= (1 << bit_idx);
            }
            bit_pos++;
        }
    }
    
    if (out_residual) {
        *out_residual = conv->accumulated_error;
    }
    
    return bit_pos;
}

/* ============================================================================
 * Drift Analysis
 * ============================================================================ */

double uft_flux_estimate_drift(const uft_flux_buffer_t *flux,
                               double expected_rotation_ns) {
    if (!flux || flux->index_count < 2 || expected_rotation_ns <= 0) return 1.0;
    
    /* Find index markers */
    size_t first_idx = 0, last_idx = 0;
    bool found_first = false;
    
    for (size_t i = 0; i < flux->count; i++) {
        if (flux->samples[i].flags & UFT_FLUX_FLAG_INDEX) {
            if (!found_first) {
                first_idx = i;
                found_first = true;
            }
            last_idx = i;
        }
    }
    
    if (first_idx == last_idx) return 1.0;
    
    /* Calculate actual rotation time */
    double actual_ns = uft_flux_delta(&flux->samples[first_idx],
                                      &flux->samples[last_idx]);
    
    int rotations = (int)(flux->index_count - 1);
    if (rotations < 1) rotations = 1;
    
    double actual_per_rotation = actual_ns / rotations;
    
    return actual_per_rotation / expected_rotation_ns;
}

void uft_flux_compensate_drift(uft_flux_buffer_t *flux,
                               double drift_rate) {
    if (!flux || drift_rate <= 0 || drift_rate == 1.0) return;
    
    /* Scale all timestamps */
    double scale = 1.0 / drift_rate;
    
    for (size_t i = 0; i < flux->count; i++) {
        double time = flux->samples[i].timestamp_ns + flux->samples[i].fractional;
        time *= scale;
        
        flux->samples[i].timestamp_ns = (uint64_t)time;
        flux->samples[i].fractional = time - flux->samples[i].timestamp_ns;
    }
}

double uft_flux_analyze_timing(const uft_flux_buffer_t *flux,
                               double cell_time_ns,
                               double *out_variance) {
    if (!flux || flux->count < 2 || cell_time_ns <= 0) return 0;
    
    double sum_error = 0;
    double sum_sq_error = 0;
    size_t count = 0;
    
    for (size_t i = 1; i < flux->count; i++) {
        double delta = uft_flux_delta(&flux->samples[i-1], &flux->samples[i]);
        
        /* Find nearest integer cell count */
        double cells = delta / cell_time_ns;
        int int_cells = (int)(cells + 0.5);
        if (int_cells < 1) int_cells = 1;
        
        double expected = int_cells * cell_time_ns;
        double error = delta - expected;
        
        sum_error += error;
        sum_sq_error += error * error;
        count++;
    }
    
    double mean_error = sum_error / count;
    
    if (out_variance) {
        *out_variance = (sum_sq_error / count) - (mean_error * mean_error);
    }
    
    return mean_error;
}

/* ============================================================================
 * Debug
 * ============================================================================ */

void uft_flux_buffer_dump(const uft_flux_buffer_t *buf) {
    if (!buf) {
        printf("Flux Buffer: NULL\n");
        return;
    }
    
    printf("=== Flux Buffer ===\n");
    printf("Samples: %zu / %zu\n", buf->count, buf->capacity);
    printf("Sample rate: %.0f Hz (%.3f ns/sample)\n",
           buf->sample_rate, buf->ns_per_sample);
    printf("Index pulses: %zu\n", buf->index_count);
    
    if (buf->count > 0) {
        double first = buf->samples[0].timestamp_ns + buf->samples[0].fractional;
        double last = buf->samples[buf->count-1].timestamp_ns + 
                     buf->samples[buf->count-1].fractional;
        printf("Time span: %.3f ms\n", (last - first) / 1e6);
    }
}

void uft_flux_converter_dump(const uft_flux_converter_t *conv) {
    if (!conv) {
        printf("Flux Converter: NULL\n");
        return;
    }
    
    printf("=== Flux Converter ===\n");
    printf("Cell time: %.3f ns (tolerance: %.0f%%)\n",
           conv->cell_time_ns, conv->cell_tolerance * 100);
    printf("Accumulated error: %.3f ns\n", conv->accumulated_error);
    printf("Max error seen: %.3f ns\n", conv->max_error);
    printf("Pulses processed: %zu\n", conv->pulses_processed);
    printf("Bits generated: %zu\n", conv->bits_generated);
    printf("Corrections applied: %zu\n", conv->corrections_applied);
}
