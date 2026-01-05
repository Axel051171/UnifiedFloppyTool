/**
 * @file uft_pattern_generator.c
 * @brief Test Pattern Generation Implementation
 * 
 * EXT4-002: Pattern Generator Implementation
 * Based on FloppyAI patterns.py
 */

#include "uft/flux/uft_pattern_generator.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/*===========================================================================
 * LFSR Implementation
 *===========================================================================*/

void uft_lfsr_init(uft_lfsr_t *lfsr, uint8_t order, uint32_t seed)
{
    if (!lfsr) return;
    
    lfsr->order = order;
    
    /* Set polynomial taps based on order */
    switch (order) {
        case 7:
            lfsr->tap1 = 7;
            lfsr->tap2 = 6;
            break;
        case 15:
            lfsr->tap1 = 15;
            lfsr->tap2 = 14;
            break;
        case 23:
            lfsr->tap1 = 23;
            lfsr->tap2 = 18;
            break;
        case 31:
            lfsr->tap1 = 31;
            lfsr->tap2 = 28;
            break;
        default:
            /* Default to 7-bit */
            lfsr->order = 7;
            lfsr->tap1 = 7;
            lfsr->tap2 = 6;
            break;
    }
    
    /* Initialize state */
    if (seed == 0) {
        lfsr->state = 1u << (lfsr->order - 1);
    } else {
        uint32_t mask = (1u << lfsr->order) - 1;
        lfsr->state = (seed & mask) | 1; /* Ensure non-zero */
    }
}

/*===========================================================================
 * Pattern Configuration
 *===========================================================================*/

void uft_pattern_config_init(uft_pattern_config_t *config, uft_pattern_type_t type)
{
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    config->type = type;
    config->base_cell_ns = UFT_PATTERN_CELL_DD_NS;
    config->rpm = 300.0;
    config->seed = (uint32_t)time(NULL);
    
    /* Set defaults for specific patterns */
    switch (type) {
        case UFT_PATTERN_ALT:
            config->params.alt.runlen = 1;
            break;
        case UFT_PATTERN_RUNLEN:
            config->params.runlen.max_len = 8;
            break;
        case UFT_PATTERN_CHIRP:
            config->params.chirp.start_ns = config->base_cell_ns * 6 / 10;
            config->params.chirp.end_ns = config->base_cell_ns * 18 / 10;
            break;
        case UFT_PATTERN_DC_BIAS:
            config->params.dc_bias.bias = 0.1;
            break;
        case UFT_PATTERN_BURST:
            config->params.burst.period = 50;
            config->params.burst.duty = 0.5;
            config->params.burst.noise = 0.25;
            break;
        default:
            break;
    }
}

/*===========================================================================
 * Memory Management
 *===========================================================================*/

uft_pattern_data_t *uft_pattern_alloc(uint16_t revolutions, double rpm,
                                       uint32_t base_cell_ns)
{
    uft_pattern_data_t *data = calloc(1, sizeof(uft_pattern_data_t));
    if (!data) return NULL;
    
    /* Calculate capacity */
    uint32_t bits_per_rev = uft_pattern_bits_per_rev(base_cell_ns, rpm);
    size_t capacity = (size_t)bits_per_rev * revolutions * 2; /* 2x for variable encoding */
    
    data->intervals = calloc(capacity, sizeof(uint32_t));
    if (!data->intervals) {
        free(data);
        return NULL;
    }
    
    data->capacity = capacity;
    data->count = 0;
    data->revolutions = revolutions;
    data->actual_density = 0.0;
    
    return data;
}

void uft_pattern_free(uft_pattern_data_t *data)
{
    if (!data) return;
    free(data->intervals);
    free(data);
}

/*===========================================================================
 * Normalization
 *===========================================================================*/

size_t uft_pattern_normalize_rev(uint32_t *intervals, size_t count,
                                 size_t capacity, double rpm,
                                 uint32_t base_cell_ns)
{
    if (!intervals || count == 0) return 0;
    
    uint64_t rev_time = (rpm == 300.0) ? UFT_PATTERN_REV_NS_300 
                                       : UFT_PATTERN_REV_NS_360;
    
    /* Calculate current total time */
    uint64_t total = 0;
    for (size_t i = 0; i < count; i++) {
        total += intervals[i];
    }
    
    if (total < rev_time && count < capacity) {
        /* Pad with half-cells */
        uint32_t half = base_cell_ns / 2;
        if (half < 1) half = 1;
        
        uint64_t remaining = rev_time - total;
        size_t pairs = (size_t)(remaining / (2 * half));
        
        /* Add pairs of half-cells */
        for (size_t i = 0; i < pairs && count + 1 < capacity; i++) {
            intervals[count++] = half;
            intervals[count++] = half;
        }
    } else if (total > rev_time) {
        /* Trim from end */
        uint64_t cut = total - rev_time;
        while (cut > 0 && count > 0) {
            if (intervals[count - 1] <= cut) {
                cut -= intervals[count - 1];
                count--;
            } else {
                intervals[count - 1] -= (uint32_t)cut;
                cut = 0;
            }
        }
    }
    
    return count;
}

/*===========================================================================
 * Simple Random Number Generator
 *===========================================================================*/

static uint32_t s_rng_state = 1;

static void rng_seed(uint32_t seed)
{
    s_rng_state = seed ? seed : 1;
}

static uint32_t rng_next(void)
{
    /* xorshift32 */
    uint32_t x = s_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng_state = x;
    return x;
}

static double rng_uniform(void)
{
    return (double)rng_next() / (double)0xFFFFFFFF;
}

static double rng_normal(double mean, double std)
{
    /* Box-Muller transform */
    double u1 = rng_uniform();
    double u2 = rng_uniform();
    if (u1 < 1e-10) u1 = 1e-10;
    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265358979 * u2);
    return mean + std * z;
}

/*===========================================================================
 * Pattern Generation Functions
 *===========================================================================*/

void uft_pattern_gen_random(uft_pattern_data_t *data, uint32_t base_cell_ns,
                            double rpm, uint32_t seed)
{
    if (!data || !data->intervals) return;
    
    rng_seed(seed);
    
    uint32_t bits_per_rev = uft_pattern_bits_per_rev(base_cell_ns, rpm);
    uint32_t lo = base_cell_ns / 2;
    uint32_t hi = base_cell_ns * 2;
    if (lo < 1) lo = 1;
    
    data->count = 0;
    
    for (uint16_t rev = 0; rev < data->revolutions; rev++) {
        size_t rev_start = data->count;
        
        for (uint32_t i = 0; i < bits_per_rev && data->count < data->capacity; i++) {
            uint32_t interval = lo + (rng_next() % (hi - lo + 1));
            data->intervals[data->count++] = interval;
        }
        
        /* Normalize this revolution */
        size_t rev_count = data->count - rev_start;
        rev_count = uft_pattern_normalize_rev(data->intervals + rev_start, 
                                               rev_count,
                                               data->capacity - rev_start,
                                               rpm, base_cell_ns);
        data->count = rev_start + rev_count;
    }
    
    data->actual_density = (double)data->count / (double)data->revolutions;
}

void uft_pattern_gen_prbs(uft_pattern_data_t *data, uint8_t order,
                          uint32_t base_cell_ns, double rpm, uint32_t seed)
{
    if (!data || !data->intervals) return;
    
    uft_lfsr_t lfsr;
    uft_lfsr_init(&lfsr, order, seed);
    
    uint32_t bits_per_rev = uft_pattern_bits_per_rev(base_cell_ns, rpm);
    uint32_t long_cell = base_cell_ns * 12 / 10;  /* 1.2x */
    uint32_t short_cell = base_cell_ns * 8 / 10;  /* 0.8x */
    
    data->count = 0;
    
    for (uint16_t rev = 0; rev < data->revolutions; rev++) {
        size_t rev_start = data->count;
        
        for (uint32_t i = 0; i < bits_per_rev && data->count < data->capacity; i++) {
            uint8_t bit = uft_lfsr_next_bit(&lfsr);
            data->intervals[data->count++] = bit ? long_cell : short_cell;
        }
        
        /* Normalize */
        size_t rev_count = data->count - rev_start;
        rev_count = uft_pattern_normalize_rev(data->intervals + rev_start,
                                               rev_count,
                                               data->capacity - rev_start,
                                               rpm, base_cell_ns);
        data->count = rev_start + rev_count;
    }
    
    data->actual_density = (double)data->count / (double)data->revolutions;
}

void uft_pattern_gen_alt(uft_pattern_data_t *data, uint32_t base_cell_ns,
                         double rpm, uint8_t runlen)
{
    if (!data || !data->intervals || runlen == 0) return;
    
    uint32_t bits_per_rev = uft_pattern_bits_per_rev(base_cell_ns, rpm);
    uint32_t long_cell = base_cell_ns * 15 / 10;  /* 1.5x */
    uint32_t short_cell = base_cell_ns;
    
    data->count = 0;
    int toggle = 0;
    
    for (uint16_t rev = 0; rev < data->revolutions; rev++) {
        size_t rev_start = data->count;
        uint8_t count = 0;
        
        for (uint32_t i = 0; i < bits_per_rev && data->count < data->capacity; i++) {
            data->intervals[data->count++] = toggle ? long_cell : short_cell;
            count++;
            if (count >= runlen) {
                toggle = !toggle;
                count = 0;
            }
        }
        
        /* Normalize */
        size_t rev_count = data->count - rev_start;
        rev_count = uft_pattern_normalize_rev(data->intervals + rev_start,
                                               rev_count,
                                               data->capacity - rev_start,
                                               rpm, base_cell_ns);
        data->count = rev_start + rev_count;
    }
    
    data->actual_density = (double)data->count / (double)data->revolutions;
}

void uft_pattern_gen_chirp(uft_pattern_data_t *data, uint32_t start_ns,
                           uint32_t end_ns, double rpm)
{
    if (!data || !data->intervals) return;
    if (start_ns == 0) start_ns = 1;
    if (end_ns <= start_ns) end_ns = start_ns + 1000;
    
    uint32_t mid_cell = (start_ns + end_ns) / 2;
    uint32_t bits_per_rev = uft_pattern_bits_per_rev(mid_cell, rpm);
    
    data->count = 0;
    
    for (uint16_t rev = 0; rev < data->revolutions; rev++) {
        size_t rev_start = data->count;
        
        for (uint32_t i = 0; i < bits_per_rev && data->count < data->capacity; i++) {
            /* Linear interpolation from start to end */
            double t = (double)i / (double)(bits_per_rev - 1);
            uint32_t cell = start_ns + (uint32_t)(t * (end_ns - start_ns));
            data->intervals[data->count++] = cell;
        }
        
        /* Normalize */
        size_t rev_count = data->count - rev_start;
        rev_count = uft_pattern_normalize_rev(data->intervals + rev_start,
                                               rev_count,
                                               data->capacity - rev_start,
                                               rpm, mid_cell);
        data->count = rev_start + rev_count;
    }
    
    data->actual_density = (double)data->count / (double)data->revolutions;
}

void uft_pattern_gen_burst(uft_pattern_data_t *data, uint32_t base_cell_ns,
                           double rpm, uint16_t period, double duty,
                           double noise)
{
    if (!data || !data->intervals) return;
    if (period == 0) period = 50;
    if (duty <= 0.0) duty = 0.5;
    if (duty > 1.0) duty = 1.0;
    
    rng_seed((uint32_t)time(NULL));
    
    uint32_t bits_per_rev = uft_pattern_bits_per_rev(base_cell_ns, rpm);
    uint16_t burst_len = (uint16_t)(period * duty);
    
    data->count = 0;
    
    for (uint16_t rev = 0; rev < data->revolutions; rev++) {
        size_t rev_start = data->count;
        
        for (uint32_t i = 0; i < bits_per_rev && data->count < data->capacity; i++) {
            int in_burst = ((i % period) < burst_len);
            double base = base_cell_ns * (in_burst ? 0.9 : 1.2);
            double jitter = rng_normal(0, base_cell_ns * noise);
            
            int32_t cell = (int32_t)(base + jitter);
            if (cell < 1) cell = 1;
            
            data->intervals[data->count++] = (uint32_t)cell;
        }
        
        /* Normalize */
        size_t rev_count = data->count - rev_start;
        rev_count = uft_pattern_normalize_rev(data->intervals + rev_start,
                                               rev_count,
                                               data->capacity - rev_start,
                                               rpm, base_cell_ns);
        data->count = rev_start + rev_count;
    }
    
    data->actual_density = (double)data->count / (double)data->revolutions;
}

/*===========================================================================
 * Main Generation Function
 *===========================================================================*/

int uft_pattern_generate(const uft_pattern_config_t *config,
                         uint16_t revolutions, uft_pattern_data_t *data)
{
    if (!config || !data) return -1;
    
    data->revolutions = revolutions;
    
    switch (config->type) {
        case UFT_PATTERN_RANDOM:
            uft_pattern_gen_random(data, config->base_cell_ns, config->rpm, config->seed);
            break;
            
        case UFT_PATTERN_PRBS7:
            uft_pattern_gen_prbs(data, 7, config->base_cell_ns, config->rpm, config->seed);
            break;
            
        case UFT_PATTERN_PRBS15:
            uft_pattern_gen_prbs(data, 15, config->base_cell_ns, config->rpm, config->seed);
            break;
            
        case UFT_PATTERN_PRBS23:
            uft_pattern_gen_prbs(data, 23, config->base_cell_ns, config->rpm, config->seed);
            break;
            
        case UFT_PATTERN_PRBS31:
            uft_pattern_gen_prbs(data, 31, config->base_cell_ns, config->rpm, config->seed);
            break;
            
        case UFT_PATTERN_ALT:
            uft_pattern_gen_alt(data, config->base_cell_ns, config->rpm, 
                                config->params.alt.runlen);
            break;
            
        case UFT_PATTERN_CHIRP:
            uft_pattern_gen_chirp(data, config->params.chirp.start_ns,
                                  config->params.chirp.end_ns, config->rpm);
            break;
            
        case UFT_PATTERN_BURST:
            uft_pattern_gen_burst(data, config->base_cell_ns, config->rpm,
                                  config->params.burst.period,
                                  config->params.burst.duty,
                                  config->params.burst.noise);
            break;
            
        default:
            uft_pattern_gen_random(data, config->base_cell_ns, config->rpm, config->seed);
            break;
    }
    
    return 0;
}

/*===========================================================================
 * Jitter Addition
 *===========================================================================*/

void uft_pattern_add_jitter(uint32_t *intervals, size_t count,
                            double jitter_percent, uint32_t seed)
{
    if (!intervals || count == 0 || jitter_percent <= 0.0) return;
    
    rng_seed(seed);
    
    /* Calculate mean */
    uint64_t sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += intervals[i];
    }
    double mean = (double)sum / (double)count;
    double std = mean * jitter_percent / 100.0;
    
    /* Add jitter */
    for (size_t i = 0; i < count; i++) {
        double jitter = rng_normal(0, std);
        int32_t new_val = (int32_t)intervals[i] + (int32_t)jitter;
        intervals[i] = (new_val > 0) ? (uint32_t)new_val : 1;
    }
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

const char *uft_pattern_type_name(uft_pattern_type_t type)
{
    switch (type) {
        case UFT_PATTERN_RANDOM:    return "Random";
        case UFT_PATTERN_PRBS7:     return "PRBS-7";
        case UFT_PATTERN_PRBS15:    return "PRBS-15";
        case UFT_PATTERN_PRBS23:    return "PRBS-23";
        case UFT_PATTERN_PRBS31:    return "PRBS-31";
        case UFT_PATTERN_ALT:       return "Alternating";
        case UFT_PATTERN_RUNLEN:    return "Run-Length";
        case UFT_PATTERN_CHIRP:     return "Chirp";
        case UFT_PATTERN_DC_BIAS:   return "DC Bias";
        case UFT_PATTERN_BURST:     return "Burst";
        case UFT_PATTERN_MFM_CLOCK: return "MFM Clock";
        case UFT_PATTERN_MFM_SYNC:  return "MFM Sync";
        case UFT_PATTERN_GCR_SYNC:  return "GCR Sync";
        case UFT_PATTERN_CUSTOM:    return "Custom";
        default:                    return "Unknown";
    }
}

int uft_pattern_verify(const uft_pattern_data_t *data,
                       const uft_pattern_config_t *config)
{
    if (!data || !config || data->count == 0) return 0;
    
    /* Calculate statistics */
    uint64_t sum = 0;
    uint32_t min_val = UINT32_MAX;
    uint32_t max_val = 0;
    
    for (size_t i = 0; i < data->count; i++) {
        sum += data->intervals[i];
        if (data->intervals[i] < min_val) min_val = data->intervals[i];
        if (data->intervals[i] > max_val) max_val = data->intervals[i];
    }
    
    double mean = (double)sum / (double)data->count;
    
    /* Calculate variance */
    double var_sum = 0;
    for (size_t i = 0; i < data->count; i++) {
        double diff = (double)data->intervals[i] - mean;
        var_sum += diff * diff;
    }
    double std = sqrt(var_sum / (double)data->count);
    
    /* Score based on expected behavior */
    int score = 100;
    
    /* Check mean is reasonable */
    double expected_mean = config->base_cell_ns;
    double mean_error = fabs(mean - expected_mean) / expected_mean;
    if (mean_error > 0.5) score -= 30;
    else if (mean_error > 0.3) score -= 15;
    
    /* Check variance is reasonable */
    double cv = std / mean;  /* Coefficient of variation */
    if (cv > 0.5) score -= 20;
    else if (cv > 0.3) score -= 10;
    
    /* Check min/max are reasonable */
    if (min_val < config->base_cell_ns / 4) score -= 10;
    if (max_val > config->base_cell_ns * 4) score -= 10;
    
    if (score < 0) score = 0;
    return score;
}
