/**
 * @file uft_vfo_pid3.c
 * @brief PID-based VFO implementation for MFM data separation
 * 
 * Original author: Yasunori Shimura
 * 
 * This implementation provides superior data separation for:
 * - Damaged/degraded floppy media
 * - Copy protection analysis
 * - Weak bit handling
 * - Spindle speed variation tolerance
 */

#include "uft_vfo_pid3.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static inline double limit(double val, double lower, double upper) {
    if (val < lower) return lower;
    if (val > upper) return upper;
    return val;
}

static void update_cell_params(uft_vfo_pid3_t *vfo) {
    vfo->window_size = vfo->cell_size * vfo->window_ratio;
    vfo->window_ofst = (vfo->cell_size - vfo->window_size) / 2.0;
    vfo->cell_center = vfo->cell_size / 2.0;
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

void uft_vfo_pid3_init(uft_vfo_pid3_t *vfo) {
    if (!vfo) return;
    
    memset(vfo, 0, sizeof(*vfo));
    
    /* Default gain values */
    vfo->gain_l = UFT_VFO_GAIN_L_DEFAULT;
    vfo->gain_h = UFT_VFO_GAIN_H_DEFAULT;
    vfo->current_gain = vfo->gain_l;
    vfo->gain_used = 1.0;
    
    /* Calculate LPF coefficient sum: 1+2+3+4 = 10 */
    vfo->coeff_sum = 0.0;
    for (double i = 1.0; i <= UFT_VFO_HISTORY_LEN; i += 1.0) {
        vfo->coeff_sum += i;
    }
    
    uft_vfo_pid3_reset(vfo);
}

void uft_vfo_pid3_reset(uft_vfo_pid3_t *vfo) {
    if (!vfo) return;
    
    vfo->cell_size = 0.0;
    vfo->cell_size_ref = 0.0;
    vfo->window_ratio = 0.75;  /* Default */
    vfo->window_size = 0.0;
    vfo->window_ofst = 0.0;
    vfo->cell_center = 0.0;
    
    uft_vfo_pid3_soft_reset(vfo);
}

void uft_vfo_pid3_soft_reset(uft_vfo_pid3_t *vfo) {
    if (!vfo) return;
    
    /* Reset PID state */
    vfo->prev_pulse_pos = 0.0;
    vfo->prev_phase_err = 0.0;
    vfo->phase_err_I = 0.0;
    
    /* Default PID coefficients (empirically tuned) */
    vfo->phase_err_PC = 1.0 / 4.0;    /* 0.25 */
    vfo->phase_err_IC = 1.0 / 64.0;   /* 0.015625 */
    vfo->phase_err_DC = 1.0 / 16.0;   /* 0.0625 */
    
    /* Initialize LPF history with cell center */
    double center = vfo->cell_size_ref / 2.0;
    for (size_t i = 0; i < UFT_VFO_HISTORY_LEN; i++) {
        vfo->pulse_history[i] = center;
    }
    vfo->hist_ptr = 0;
    
    /* Update derived parameters */
    if (vfo->cell_size_ref > 0) {
        vfo->cell_size = vfo->cell_size_ref;
        update_cell_params(vfo);
    }
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

void uft_vfo_pid3_set_params(uft_vfo_pid3_t *vfo,
                              size_t sampling_rate,
                              size_t fdc_bit_rate,
                              double window_ratio) {
    if (!vfo) return;
    
    vfo->sampling_rate = (double)sampling_rate;
    vfo->fdc_bit_rate = (double)fdc_bit_rate;
    vfo->data_window_ratio = window_ratio;
    
    /* Calculate cell size: samples per bit */
    if (fdc_bit_rate > 0) {
        vfo->cell_size_ref = (double)sampling_rate / (double)fdc_bit_rate;
        vfo->cell_size = vfo->cell_size_ref;
    }
    
    /* Set window ratio */
    if (window_ratio >= 0.2 && window_ratio <= 0.9) {
        vfo->window_ratio = window_ratio;
    } else {
        vfo->window_ratio = 0.75;  /* Default */
    }
    
    update_cell_params(vfo);
    
    /* Initialize history with new cell center */
    double center = vfo->cell_size_ref / 2.0;
    for (size_t i = 0; i < UFT_VFO_HISTORY_LEN; i++) {
        vfo->pulse_history[i] = center;
    }
}

void uft_vfo_pid3_set_cell_size(uft_vfo_pid3_t *vfo, double cell_size) {
    if (!vfo || cell_size <= 0) return;
    
    vfo->cell_size = cell_size;
    update_cell_params(vfo);
}

void uft_vfo_pid3_set_gain_val(uft_vfo_pid3_t *vfo, double gain_l, double gain_h) {
    if (!vfo) return;
    
    vfo->gain_l = gain_l;
    vfo->gain_h = gain_h;
}

void uft_vfo_pid3_set_gain_mode(uft_vfo_pid3_t *vfo, uft_vfo_gain_state_t state) {
    if (!vfo) return;
    
    switch (state) {
        case UFT_VFO_GAIN_LOW:
            vfo->current_gain = vfo->gain_l;
            break;
        case UFT_VFO_GAIN_HIGH:
            vfo->current_gain = vfo->gain_h;
            break;
    }
}

void uft_vfo_pid3_set_pid_coeff(uft_vfo_pid3_t *vfo,
                                 double p_coeff,
                                 double i_coeff,
                                 double d_coeff) {
    if (!vfo) return;
    
    vfo->phase_err_PC = p_coeff;
    vfo->phase_err_IC = i_coeff;
    vfo->phase_err_DC = d_coeff;
}

/* ============================================================================
 * Core PID Algorithm
 * ============================================================================ */

double uft_vfo_pid3_calc(uft_vfo_pid3_t *vfo, double pulse_pos) {
    if (!vfo) return pulse_pos;
    
    double pulse_pos_backup = pulse_pos;
    
    /* 
     * Phase jump detection and correction
     * If phase shift > 180°, assume it wrapped around the cell boundary
     */
    if (vfo->prev_pulse_pos > pulse_pos) {
        /* Pulse shifted left */
        if (vfo->prev_pulse_pos - pulse_pos > vfo->cell_size - 1.1) {
            /* Jumped over left boundary - add cell size */
            pulse_pos += vfo->cell_size;
        }
    } else if (vfo->prev_pulse_pos < pulse_pos) {
        /* Pulse shifted right */
        if (pulse_pos - vfo->prev_pulse_pos > vfo->cell_size - 1.1) {
            /* Jumped over right boundary - subtract cell size */
            pulse_pos -= vfo->cell_size;
            /* Note: pulse_pos might be negative */
        }
    }
    
    /*
     * Smooth gain changes to prevent controller instability
     * Gradual transition instead of sudden jumps
     */
    const double gain_change_speed = 0.05;
    if (fabs(vfo->current_gain - vfo->gain_used) < gain_change_speed) {
        vfo->gain_used = vfo->current_gain;
    } else if (vfo->gain_used < vfo->current_gain) {
        vfo->gain_used += gain_change_speed;
    } else {
        vfo->gain_used -= gain_change_speed;
    }
    
    /*
     * Low-pass filter with weighted history
     * Newer samples have higher weight: 1, 2, 3, 4
     */
    vfo->hist_ptr = (vfo->hist_ptr + 1) & (UFT_VFO_HISTORY_LEN - 1);
    vfo->pulse_history[vfo->hist_ptr] = pulse_pos;
    
    double sum = 0.0;
    for (size_t i = 1; i <= UFT_VFO_HISTORY_LEN; i++) {
        size_t idx = (vfo->hist_ptr + i) & (UFT_VFO_HISTORY_LEN - 1);
        sum += vfo->pulse_history[idx] * (double)i;
    }
    double avg = sum / vfo->coeff_sum;
    
    /*
     * PID Controller
     * 
     * Error = cell_center - filtered_pulse_pos
     * 
     *  Bit cell  |               |
     *  Window    |   WWWWWWWW    |
     *  Center    |       ^       |
     *  Pulse     |     |         |
     *  Error     |     |-|       |
     */
    double phase_err_P = vfo->cell_center - avg;
    double phase_err_D = phase_err_P - vfo->prev_phase_err;
    vfo->phase_err_I += phase_err_P;
    vfo->prev_phase_err = phase_err_P;
    
    /*
     * Limit integral term to prevent windup
     * Max ±20% of cell_size_ref through IC coefficient
     */
    double ic_limit = vfo->cell_size_ref * 0.4;
    if (vfo->phase_err_IC > 0) {
        double max_integral = ic_limit / vfo->phase_err_IC;
        vfo->phase_err_I = limit(vfo->phase_err_I, -max_integral, max_integral);
    }
    
    /*
     * Calculate new cell size using PID output
     * 
     * new_cell = ref - (P*Kp - D*Kd + I*Ki) * gain
     * 
     * Note: Signs are arranged so that:
     * - Positive phase error (pulse early) → decrease cell size
     * - Negative phase error (pulse late) → increase cell size
     */
    double pid_output = phase_err_P * vfo->phase_err_PC 
                      - phase_err_D * vfo->phase_err_DC 
                      + vfo->phase_err_I * vfo->phase_err_IC;
    
    double new_cell_size = vfo->cell_size_ref - pid_output * vfo->gain_used;
    
    /*
     * Clamp cell size within tolerance
     * 
     * Typical variations:
     * - FDD spindle: 2-2.5%
     * - Wow/flutter: ±2-2.5%
     * - VFO drift: 5%
     * - Total: ~15%, use 40% for margin
     */
    const double tolerance = 0.4;
    double min_cell = vfo->cell_size_ref / (1.0 + tolerance);  /* ~0.71x */
    double max_cell = vfo->cell_size_ref * (1.0 + tolerance);  /* ~1.4x */
    new_cell_size = limit(new_cell_size, min_cell, max_cell);
    
    /* Apply new cell size */
    uft_vfo_pid3_set_cell_size(vfo, new_cell_size);
    
    /* Save for next iteration */
    vfo->prev_pulse_pos = pulse_pos_backup;
    
    return pulse_pos_backup;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

void uft_vfo_pid3_get_window(const uft_vfo_pid3_t *vfo, 
                              double *start, 
                              double *end) {
    if (!vfo) {
        if (start) *start = 0;
        if (end) *end = 0;
        return;
    }
    
    if (start) *start = vfo->window_ofst;
    if (end) *end = vfo->window_ofst + vfo->window_size;
}

bool uft_vfo_pid3_is_in_window(const uft_vfo_pid3_t *vfo, double pulse_pos) {
    if (!vfo) return false;
    
    double win_start = vfo->window_ofst;
    double win_end = vfo->window_ofst + vfo->window_size;
    
    return (pulse_pos >= win_start && pulse_pos <= win_end);
}

void uft_vfo_pid3_dump_status(const uft_vfo_pid3_t *vfo) {
    if (!vfo) {
        printf("VFO PID3: NULL\n");
        return;
    }
    
    printf("=== VFO PID3 Status ===\n");
    printf("Cell size      : %.4f (ref: %.4f)\n", vfo->cell_size, vfo->cell_size_ref);
    printf("Window         : %.4f - %.4f (ratio: %.2f)\n", 
           vfo->window_ofst, vfo->window_ofst + vfo->window_size, vfo->window_ratio);
    printf("Cell center    : %.4f\n", vfo->cell_center);
    printf("Gain           : L=%.3f H=%.3f (current=%.3f, used=%.3f)\n",
           vfo->gain_l, vfo->gain_h, vfo->current_gain, vfo->gain_used);
    printf("PID coefficients:\n");
    printf("  P=%.6f  I=%.6f  D=%.6f\n", 
           vfo->phase_err_PC, vfo->phase_err_IC, vfo->phase_err_DC);
    printf("PID state:\n");
    printf("  Prev pulse pos: %.4f\n", vfo->prev_pulse_pos);
    printf("  Prev phase err: %.4f\n", vfo->prev_phase_err);
    printf("  Integral      : %.4f\n", vfo->phase_err_I);
}
