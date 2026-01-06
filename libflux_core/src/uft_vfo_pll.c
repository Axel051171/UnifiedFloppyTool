/*
 * uft_vfo_pll.c
 *
 * Variable Frequency Oscillator (VFO) and Phase-Locked Loop (PLL)
 * implementation for flux stream decoding.
 *
 * Algorithms:
 * - SIMPLE: Fixed window sampler with no frequency tracking
 * - FIXED: Fixed frequency reference
 * - PID: Proportional-Integral-Derivative control
 * - ADAPTIVE: Adaptive gain based on sync detection
 * - DPLL: Digital Phase-Locked Loop
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_vfo_pll.c -lm
 */

#include "uft_vfo_pll.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Simple PRNG for fluctuator */
static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* Get random fluctuation value (-amount to +amount) */
static double get_fluctuation(uft_vfo_state_t *state)
{
    if (!state->fluctuator_en || state->fluctuator_amt <= 0.0) {
        return 0.0;
    }
    uint32_t r = xorshift32(&state->fluctuator_seed);
    double normalized = (double)r / (double)UINT32_MAX;  /* 0.0 to 1.0 */
    return (normalized * 2.0 - 1.0) * state->fluctuator_amt;
}

/* Clamp value to range */
static double clamp(double val, double min_val, double max_val)
{
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

/* Output a decoded bit */
static void output_bit(uft_vfo_state_t *state, int bit)
{
    if (!state->bit_buffer || state->bit_count >= state->bit_buffer_size * 8) {
        return;
    }
    
    size_t byte_idx = state->bit_count / 8;
    size_t bit_idx = 7 - (state->bit_count % 8);  /* MSB first */
    
    if (bit) {
        state->bit_buffer[byte_idx] |= (1 << bit_idx);
    } else {
        state->bit_buffer[byte_idx] &= ~(1 << bit_idx);
    }
    
    state->bit_count++;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO ALGORITHM IMPLEMENTATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Simple VFO: Fixed window, no tracking */
static int vfo_simple_process(uft_vfo_state_t *state, double interval)
{
    int bits_decoded = 0;
    double bit_cell = state->bit_cell_nom;
    
    /* Count how many bit cells fit in this interval */
    double cells = interval / bit_cell;
    int num_cells = (int)round(cells);
    
    if (num_cells < 1) num_cells = 1;
    if (num_cells > 4) num_cells = 4;  /* Sanity limit */
    
    /* Output bits: 1 for the pulse, 0s for empty cells */
    for (int i = 0; i < num_cells - 1; i++) {
        output_bit(state, 0);
        bits_decoded++;
    }
    output_bit(state, 1);
    bits_decoded++;
    
    state->pulses_total++;
    state->pulses_valid++;
    
    return bits_decoded;
}

/* Fixed VFO: No frequency tracking at all */
static int vfo_fixed_process(uft_vfo_state_t *state, double interval)
{
    return vfo_simple_process(state, interval);
}

/* PID VFO: Proportional-Integral-Derivative control */
static int vfo_pid_process(uft_vfo_state_t *state, double interval)
{
    int bits_decoded = 0;
    double bit_cell = state->bit_cell;
    
    /* Add fluctuation if enabled */
    bit_cell += get_fluctuation(state) * state->bit_cell_nom;
    
    /* Count bit cells and calculate phase error */
    double cells = interval / bit_cell;
    int num_cells = (int)round(cells);
    
    if (num_cells < 1) num_cells = 1;
    if (num_cells > 4) num_cells = 4;
    
    /* Phase error: difference from center of window */
    double expected = num_cells * bit_cell;
    double error = (interval - expected) / bit_cell;
    
    /* Update statistics */
    state->pulses_total++;
    if (fabs(error) < 0.3) {
        state->pulses_valid++;
    } else if (error < 0) {
        state->pulses_early++;
    } else {
        state->pulses_late++;
    }
    
    /* Running average of phase error */
    state->avg_phase_err = state->avg_phase_err * 0.99 + error * 0.01;
    
    /* PID calculation */
    double p_term = state->pid_p * error;
    state->pid_integral += error;
    state->pid_integral = clamp(state->pid_integral, -10.0, 10.0);
    double i_term = state->pid_i * state->pid_integral;
    double d_term = state->pid_d * (error - state->pid_prev_error);
    state->pid_prev_error = error;
    
    /* Adjust bit cell */
    double adjustment = p_term + i_term + d_term;
    double gain = state->synced ? state->gain_low : state->gain_high;
    state->bit_cell += adjustment * gain * state->bit_cell_nom * 0.01;
    
    /* Clamp bit cell to reasonable range (±20% of nominal) */
    state->bit_cell = clamp(state->bit_cell,
                            state->bit_cell_nom * 0.8,
                            state->bit_cell_nom * 1.2);
    
    /* Update sync state */
    if (fabs(error) < 0.2) {
        state->sync_count++;
        if (state->sync_count >= state->sync_threshold) {
            state->synced = true;
        }
    } else {
        state->sync_count = 0;
        if (fabs(error) > 0.4) {
            state->synced = false;
        }
    }
    
    /* Output bits */
    for (int i = 0; i < num_cells - 1; i++) {
        output_bit(state, 0);
        bits_decoded++;
    }
    output_bit(state, 1);
    bits_decoded++;
    
    return bits_decoded;
}

/* PID2 VFO: Faster convergence variant */
static int vfo_pid2_process(uft_vfo_state_t *state, double interval)
{
    /* Use PID with modified gains for faster convergence */
    double orig_p = state->pid_p;
    double orig_i = state->pid_i;
    
    state->pid_p = 0.5;
    state->pid_i = 0.1;
    
    int result = vfo_pid_process(state, interval);
    
    state->pid_p = orig_p;
    state->pid_i = orig_i;
    
    return result;
}

/* PID3 VFO: Better stability variant */
static int vfo_pid3_process(uft_vfo_state_t *state, double interval)
{
    /* Use PID with modified gains for stability */
    double orig_p = state->pid_p;
    double orig_d = state->pid_d;
    
    state->pid_p = 0.2;
    state->pid_d = 0.1;
    
    int result = vfo_pid_process(state, interval);
    
    state->pid_p = orig_p;
    state->pid_d = orig_d;
    
    return result;
}

/* Adaptive VFO: Gain adjusts based on sync state */
static int vfo_adaptive_process(uft_vfo_state_t *state, double interval)
{
    /* Adaptive gain selection */
    if (state->synced) {
        state->gain_current = state->gain_low;
    } else {
        /* Gradually increase gain when not synced */
        state->gain_current = state->gain_current * 1.01;
        if (state->gain_current > state->gain_high) {
            state->gain_current = state->gain_high;
        }
    }
    
    return vfo_pid_process(state, interval);
}

/* DPLL VFO: Digital Phase-Locked Loop */
static int vfo_dpll_process(uft_vfo_state_t *state, double interval)
{
    int bits_decoded = 0;
    double bit_cell = state->bit_cell;
    
    /* Advance phase by interval */
    state->phase += interval / bit_cell;
    
    /* Each time phase crosses 1.0, output a bit cell */
    while (state->phase >= 1.0) {
        state->phase -= 1.0;
        
        /* Determine if this cell contains a pulse */
        /* Pulse should be near phase 0.5 (center of cell) */
        output_bit(state, 0);  /* Default: no pulse */
        bits_decoded++;
    }
    
    /* The current pulse - adjust phase */
    double phase_error = state->phase - 0.5;
    
    /* Phase detector: adjust to center pulse in window */
    double gain = state->synced ? state->gain_low : state->gain_high;
    double freq_adj = phase_error * gain;
    
    state->freq = 1.0 - freq_adj * 0.1;
    state->freq = clamp(state->freq, 0.8, 1.2);
    
    state->bit_cell = state->bit_cell_nom / state->freq;
    
    /* Output the pulse bit */
    output_bit(state, 1);
    bits_decoded++;
    
    /* Update sync */
    state->pulses_total++;
    if (fabs(phase_error) < 0.2) {
        state->pulses_valid++;
        state->sync_count++;
        if (state->sync_count >= state->sync_threshold) {
            state->synced = true;
        }
    } else {
        if (phase_error < 0) state->pulses_early++;
        else state->pulses_late++;
        state->sync_count = 0;
    }
    
    /* Reset phase after pulse */
    state->phase = 0.0;
    
    return bits_decoded;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════════ */

void uft_vfo_init(uft_vfo_state_t *state,
                  uft_vfo_type_t type,
                  uft_encoding_type_t encoding,
                  double sample_rate)
{
    double bit_cell_ns;
    
    switch (encoding) {
        case UFT_ENC_FM:
            bit_cell_ns = UFT_FM_BIT_CELL_NS;
            break;
        case UFT_ENC_GCR:
            bit_cell_ns = UFT_GCR_BIT_CELL_NS;
            break;
        case UFT_ENC_MFM:
        default:
            bit_cell_ns = UFT_MFM_BIT_CELL_NS;
            break;
    }
    
    uft_vfo_init_custom(state, type, sample_rate, bit_cell_ns);
    state->encoding = encoding;
}

void uft_vfo_init_custom(uft_vfo_state_t *state,
                         uft_vfo_type_t type,
                         double sample_rate,
                         double bit_cell_ns)
{
    memset(state, 0, sizeof(*state));
    
    state->type = type;
    state->sample_rate = sample_rate;
    
    /* Convert bit cell from nanoseconds to samples */
    state->bit_cell_nom = (bit_cell_ns / 1e9) * sample_rate;
    state->bit_cell = state->bit_cell_nom;
    
    /* Default window */
    state->window_start = UFT_VFO_WINDOW_EARLY;
    state->window_end = UFT_VFO_WINDOW_LATE;
    
    /* Default gains */
    state->gain_low = UFT_VFO_GAIN_LOW_DEFAULT;
    state->gain_high = UFT_VFO_GAIN_HIGH_DEFAULT;
    state->gain_current = state->gain_high;
    
    /* Default PID parameters */
    state->pid_p = 0.3;
    state->pid_i = 0.05;
    state->pid_d = 0.05;
    
    /* Sync detection */
    state->sync_threshold = 8;
    state->synced = false;
    
    /* Phase */
    state->phase = 0.0;
    state->freq = 1.0;
    
    /* Fluctuator */
    state->fluctuator_en = false;
    state->fluctuator_amt = 0.0;
    state->fluctuator_seed = 12345;
}

void uft_vfo_reset(uft_vfo_state_t *state)
{
    state->bit_cell = state->bit_cell_nom;
    state->phase = 0.0;
    state->freq = 1.0;
    state->synced = false;
    state->sync_count = 0;
    state->pid_integral = 0.0;
    state->pid_prev_error = 0.0;
    state->gain_current = state->gain_high;
    state->bit_count = 0;
    
    uft_vfo_reset_stats(state);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════ */

void uft_vfo_set_gain(uft_vfo_state_t *state,
                      double gain_low, double gain_high)
{
    state->gain_low = clamp(gain_low, 0.01, 1.0);
    state->gain_high = clamp(gain_high, 0.01, 1.0);
}

void uft_vfo_set_pid(uft_vfo_state_t *state,
                     double p, double i, double d)
{
    state->pid_p = clamp(p, 0.0, 2.0);
    state->pid_i = clamp(i, 0.0, 1.0);
    state->pid_d = clamp(d, 0.0, 1.0);
}

void uft_vfo_set_window(uft_vfo_state_t *state,
                        double early, double late)
{
    state->window_start = clamp(early, 0.1, 0.4);
    state->window_end = clamp(late, 0.6, 0.9);
}

void uft_vfo_enable_fluctuator(uft_vfo_state_t *state,
                               bool enable, double amount)
{
    state->fluctuator_en = enable;
    state->fluctuator_amt = clamp(amount, 0.0, 0.2);
}

void uft_vfo_set_output(uft_vfo_state_t *state,
                        uint8_t *buffer, size_t size)
{
    state->bit_buffer = buffer;
    state->bit_buffer_size = size;
    state->bit_count = 0;
    
    if (buffer && size > 0) {
        memset(buffer, 0, size);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO PROCESSING
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_vfo_process_pulse(uft_vfo_state_t *state, double interval)
{
    if (interval <= 0.0) return 0;
    
    switch (state->type) {
        case UFT_VFO_SIMPLE:
            return vfo_simple_process(state, interval);
        case UFT_VFO_FIXED:
            return vfo_fixed_process(state, interval);
        case UFT_VFO_PID:
            return vfo_pid_process(state, interval);
        case UFT_VFO_PID2:
            return vfo_pid2_process(state, interval);
        case UFT_VFO_PID3:
            return vfo_pid3_process(state, interval);
        case UFT_VFO_ADAPTIVE:
            return vfo_adaptive_process(state, interval);
        case UFT_VFO_DPLL:
            return vfo_dpll_process(state, interval);
        default:
            return vfo_simple_process(state, interval);
    }
}

size_t uft_vfo_process_intervals(uft_vfo_state_t *state,
                                 const double *intervals,
                                 size_t count)
{
    size_t total_bits = 0;
    
    for (size_t i = 0; i < count; i++) {
        total_bits += uft_vfo_process_pulse(state, intervals[i]);
    }
    
    return total_bits;
}

size_t uft_vfo_process_flux(uft_vfo_state_t *state,
                            const uint32_t *flux_times,
                            size_t count)
{
    if (count < 2) return 0;
    
    size_t total_bits = 0;
    
    for (size_t i = 1; i < count; i++) {
        double interval = (double)(flux_times[i] - flux_times[i-1]);
        total_bits += uft_vfo_process_pulse(state, interval);
    }
    
    return total_bits;
}

void uft_vfo_force_sync(uft_vfo_state_t *state)
{
    state->synced = true;
    state->sync_count = state->sync_threshold;
    state->gain_current = state->gain_low;
    state->pid_integral = 0.0;
}

bool uft_vfo_is_synced(const uft_vfo_state_t *state)
{
    return state->synced;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO STATISTICS
 * ═══════════════════════════════════════════════════════════════════════════ */

void uft_vfo_get_stats(const uft_vfo_state_t *state, uft_vfo_stats_t *stats)
{
    stats->pulses_total = state->pulses_total;
    stats->pulses_valid = state->pulses_valid;
    stats->pulses_early = state->pulses_early;
    stats->pulses_late = state->pulses_late;
    
    if (state->pulses_total > 0) {
        stats->valid_percent = 100.0 * state->pulses_valid / state->pulses_total;
    } else {
        stats->valid_percent = 0.0;
    }
    
    stats->avg_phase_error = state->avg_phase_err;
    stats->current_freq = state->freq;
    stats->current_bit_cell = state->bit_cell;
    stats->bits_decoded = state->bit_count;
}

void uft_vfo_reset_stats(uft_vfo_state_t *state)
{
    state->pulses_total = 0;
    state->pulses_valid = 0;
    state->pulses_early = 0;
    state->pulses_late = 0;
    state->avg_phase_err = 0.0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA SEPARATOR
 * ═══════════════════════════════════════════════════════════════════════════ */

void uft_datasep_init(uft_data_separator_t *sep,
                      uft_vfo_type_t vfo_type,
                      uft_encoding_type_t encoding,
                      double sample_rate)
{
    memset(sep, 0, sizeof(*sep));
    uft_vfo_init(&sep->vfo, vfo_type, encoding, sample_rate);
    
    /* Default sync pattern for MFM: 0x4489 (A1 with missing clock) */
    if (encoding == UFT_ENC_MFM) {
        sep->sync_pattern = 0x4489;
        sep->sync_mask = 0xFFFF;
    } else if (encoding == UFT_ENC_FM) {
        sep->sync_pattern = 0xF57E;  /* FM sync */
        sep->sync_mask = 0xFFFF;
    }
}

void uft_datasep_reset(uft_data_separator_t *sep)
{
    uft_vfo_reset(&sep->vfo);
    sep->shift_reg = 0;
    sep->bit_counter = 0;
    sep->clock_bit = true;
    sep->sync_found = false;
    sep->data_count = 0;
}

void uft_datasep_set_output(uft_data_separator_t *sep,
                            uint8_t *buffer, size_t size)
{
    sep->data_buffer = buffer;
    sep->data_size = size;
    sep->data_count = 0;
}

void uft_datasep_set_sync(uft_data_separator_t *sep,
                          uint16_t pattern, uint16_t mask)
{
    sep->sync_pattern = pattern;
    sep->sync_mask = mask;
}

size_t uft_datasep_process(uft_data_separator_t *sep,
                           const uint32_t *flux_times,
                           size_t count)
{
    if (count < 2) return 0;
    
    uint16_t shift16 = 0;
    size_t bytes_decoded = 0;
    
    for (size_t i = 1; i < count; i++) {
        double interval = (double)(flux_times[i] - flux_times[i-1]);
        
        /* Estimate number of bit cells */
        double cells = interval / sep->vfo.bit_cell;
        int num_cells = (int)round(cells);
        if (num_cells < 1) num_cells = 1;
        if (num_cells > 4) num_cells = 4;
        
        /* Process VFO for tracking */
        uft_vfo_process_pulse(&sep->vfo, interval);
        
        /* Shift in bits: 0s for empty cells, 1 for the pulse */
        for (int j = 0; j < num_cells - 1; j++) {
            shift16 = (shift16 << 1) | 0;
        }
        shift16 = (shift16 << 1) | 1;
        
        /* Check for sync pattern */
        if ((shift16 & sep->sync_mask) == sep->sync_pattern) {
            sep->sync_found = true;
            sep->bit_counter = 0;
            sep->shift_reg = 0;
            continue;
        }
        
        /* If synced, decode MFM data */
        if (sep->sync_found && sep->data_buffer && 
            sep->data_count < sep->data_size) {
            
            /* MFM: alternate clock and data bits */
            for (int j = 0; j < num_cells; j++) {
                int bit = (j == num_cells - 1) ? 1 : 0;
                
                sep->clock_bit = !sep->clock_bit;
                if (!sep->clock_bit) {
                    /* Data bit */
                    sep->shift_reg = (sep->shift_reg << 1) | bit;
                    sep->bit_counter++;
                    
                    if (sep->bit_counter >= 8) {
                        sep->data_buffer[sep->data_count++] = sep->shift_reg;
                        bytes_decoded++;
                        sep->bit_counter = 0;
                        sep->shift_reg = 0;
                    }
                }
            }
        }
    }
    
    return bytes_decoded;
}

bool uft_datasep_sync_found(const uft_data_separator_t *sep)
{
    return sep->sync_found;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char *vfo_type_names[] = {
    "SIMPLE", "FIXED", "PID", "PID2", "PID3", "ADAPTIVE", "DPLL"
};

static const char *encoding_names[] = {
    "MFM", "FM", "GCR", "M2FM"
};

const char *uft_vfo_type_name(uft_vfo_type_t type)
{
    if (type <= UFT_VFO_DPLL) {
        return vfo_type_names[type];
    }
    return "UNKNOWN";
}

const char *uft_encoding_name(uft_encoding_type_t encoding)
{
    if (encoding <= UFT_ENC_M2FM) {
        return encoding_names[encoding];
    }
    return "UNKNOWN";
}

double uft_vfo_calc_bit_cell(double data_rate, double sample_rate)
{
    if (data_rate <= 0.0) return 0.0;
    return sample_rate / data_rate;
}

double uft_vfo_estimate_rate(const double *intervals,
                             size_t count,
                             double sample_rate)
{
    if (count < 10) return 0.0;
    
    /* Find median interval (approximation) */
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += intervals[i];
    }
    double avg_interval = sum / count;
    
    /* Assume most intervals are 1 or 2 bit cells */
    /* The shortest common interval is likely 1 bit cell */
    double min_interval = avg_interval;
    for (size_t i = 0; i < count; i++) {
        if (intervals[i] > 0.5 * avg_interval && 
            intervals[i] < min_interval) {
            min_interval = intervals[i];
        }
    }
    
    /* Estimate data rate */
    double bit_cell_time = min_interval / sample_rate;
    if (bit_cell_time <= 0.0) return 0.0;
    
    return 1.0 / bit_cell_time;
}
