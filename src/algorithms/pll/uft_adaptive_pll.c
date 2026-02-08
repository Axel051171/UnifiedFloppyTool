/**
 * @file uft_adaptive_pll.c
 * @brief Adaptive PID-based PLL implementation
 * 
 * Fixes algorithmic weakness #1: PLL sync loss under jitter
 */

#include "uft_adaptive_pll.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Helper Macros
 * ============================================================================ */

#define CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#define ABS(x) ((x) < 0 ? -(x) : (x))

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

static void update_cell_params(uft_adaptive_pll_t *pll) {
    pll->window_size = pll->cell_size * pll->window_ratio;
    pll->window_start = (pll->cell_size - pll->window_size) / 2.0;
    pll->cell_center = pll->cell_size / 2.0;
}

static void update_jitter_average(uft_adaptive_pll_t *pll, double phase_err) {
    pll->jitter_history[pll->jitter_idx] = ABS(phase_err);
    pll->jitter_idx = (pll->jitter_idx + 1) % UFT_PLL_JITTER_WINDOW;
    
    /* Calculate running average */
    double sum = 0;
    for (int i = 0; i < UFT_PLL_JITTER_WINDOW; i++) {
        sum += pll->jitter_history[i];
    }
    pll->jitter_avg = sum / UFT_PLL_JITTER_WINDOW;
}

static void adjust_adaptive_gain(uft_adaptive_pll_t *pll) {
    if (pll->mode != UFT_PLL_MODE_ADAPTIVE) return;
    
    /* High jitter: reduce gain for stability */
    if (pll->jitter_avg > pll->jitter_threshold) {
        pll->gain_target = pll->gain_data * 0.5;
    } 
    /* Low jitter in sync mode: can use higher gain */
    else if (pll->jitter_avg < pll->jitter_threshold * 0.3) {
        pll->gain_target = pll->is_locked ? pll->gain_data : pll->gain_sync;
    }
    /* Normal operation */
    else {
        pll->gain_target = pll->is_locked ? pll->gain_data : pll->gain_sync;
    }
    
    /* Smooth gain transition to prevent instability */
    const double transition_rate = 0.05;
    if (ABS(pll->gain_current - pll->gain_target) < transition_rate) {
        pll->gain_current = pll->gain_target;
    } else if (pll->gain_current < pll->gain_target) {
        pll->gain_current += transition_rate;
    } else {
        pll->gain_current -= transition_rate;
    }
}

static void update_lock_status(uft_adaptive_pll_t *pll, double phase_err) {
    double threshold = pll->cell_size * 0.25;  /* 25% of cell = locked */
    
    if (ABS(phase_err) < threshold) {
        pll->lock_counter++;
        if (pll->lock_counter >= pll->unlock_threshold) {
            if (!pll->is_locked) {
                pll->is_locked = true;
                pll->stats.lock_count++;
            }
        }
    } else {
        if (pll->is_locked && pll->lock_counter > 0) {
            pll->lock_counter--;
            if (pll->lock_counter == 0) {
                pll->is_locked = false;
                pll->stats.unlock_count++;
            }
        }
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void uft_pll_init(uft_adaptive_pll_t *pll) {
    if (!pll) return;
    memset(pll, 0, sizeof(*pll));
    
    /* Default PID coefficients (empirically tuned) */
    pll->Kp = 0.25;       /* 1/4 */
    pll->Ki = 0.015625;   /* 1/64 */
    pll->Kd = 0.0625;     /* 1/16 */
    
    /* Default gains */
    pll->gain_sync = 1.0;
    pll->gain_data = 0.3;
    pll->gain_current = pll->gain_sync;
    pll->gain_target = pll->gain_sync;
    
    /* Default window and tolerance */
    pll->window_ratio = 0.75;
    pll->tolerance = 0.4;  /* ±40% */
    
    /* Jitter detection */
    pll->jitter_threshold = 0.15;  /* 15% of cell */
    
    /* Lock detection */
    pll->unlock_threshold = 8;  /* 8 good pulses to lock */
    
    /* Mode */
    pll->mode = UFT_PLL_MODE_ADAPTIVE;
}

void uft_pll_configure(uft_adaptive_pll_t *pll,
                       double sample_rate,
                       double bit_rate) {
    if (!pll || bit_rate <= 0) return;
    
    pll->sample_rate = sample_rate;
    pll->bit_rate = bit_rate;
    
    /* Calculate cell size: samples per bit */
    pll->cell_ref = sample_rate / bit_rate;
    pll->cell_size = pll->cell_ref;
    
    update_cell_params(pll);
    
    /* Scale jitter threshold to absolute value */
    pll->jitter_threshold = pll->cell_ref * 0.15;
    
    /* Initialize history with cell center */
    for (int i = 0; i < UFT_PLL_JITTER_WINDOW; i++) {
        pll->jitter_history[i] = 0;
    }
    for (int i = 0; i < UFT_PLL_HISTORY_SIZE; i++) {
        pll->phase_history[i] = pll->cell_center;
    }
}

void uft_pll_set_pid(uft_adaptive_pll_t *pll,
                     double Kp, double Ki, double Kd) {
    if (!pll) return;
    pll->Kp = Kp;
    pll->Ki = Ki;
    pll->Kd = Kd;
}

void uft_pll_set_mode(uft_adaptive_pll_t *pll, uft_pll_mode_t mode) {
    if (!pll) return;
    pll->mode = mode;
    
    switch (mode) {
        case UFT_PLL_MODE_SYNC:
            pll->gain_current = pll->gain_sync;
            pll->gain_target = pll->gain_sync;
            break;
        case UFT_PLL_MODE_DATA:
            pll->gain_current = pll->gain_data;
            pll->gain_target = pll->gain_data;
            break;
        case UFT_PLL_MODE_ADAPTIVE:
            /* Will adjust automatically */
            break;
    }
    pll->stats.mode_switches++;
}

void uft_pll_reset(uft_adaptive_pll_t *pll) {
    if (!pll) return;
    
    /* Reset PID state */
    pll->phase_err_P = 0;
    pll->phase_err_I = 0;
    pll->phase_err_D = 0;
    pll->prev_phase_err = 0;
    
    /* Reset cell size to reference */
    pll->cell_size = pll->cell_ref;
    update_cell_params(pll);
    
    /* Reset lock status */
    pll->is_locked = false;
    pll->lock_counter = 0;
    
    /* Reset gain */
    pll->gain_current = pll->gain_sync;
    pll->gain_target = pll->gain_sync;
    
    /* Clear histories */
    for (int i = 0; i < UFT_PLL_JITTER_WINDOW; i++) {
        pll->jitter_history[i] = 0;
    }
    pll->jitter_idx = 0;
    pll->jitter_avg = 0;
}

void uft_pll_hard_reset(uft_adaptive_pll_t *pll) {
    if (!pll) return;
    
    /* Save config */
    double sample_rate = pll->sample_rate;
    double bit_rate = pll->bit_rate;
    double Kp = pll->Kp, Ki = pll->Ki, Kd = pll->Kd;
    
    /* Full reset */
    uft_pll_init(pll);
    
    /* Restore and reconfigure */
    pll->Kp = Kp;
    pll->Ki = Ki;
    pll->Kd = Kd;
    uft_pll_configure(pll, sample_rate, bit_rate);
}

int uft_pll_process_pulse(uft_adaptive_pll_t *pll,
                          double pulse_pos,
                          uint8_t *out_bit,
                          uint8_t *out_confidence) {
    if (!pll) return 0;
    
    pll->stats.total_pulses++;
    
    /* =========================================
     * Step 1: Calculate phase error
     * Error = how far pulse is from cell center
     * ========================================= */
    double phase_err = pll->cell_center - pulse_pos;
    
    /* Store for history */
    pll->phase_history[pll->phase_idx] = pulse_pos;
    pll->phase_idx = (pll->phase_idx + 1) % UFT_PLL_HISTORY_SIZE;
    
    /* Update stats */
    pll->stats.avg_phase_error = (pll->stats.avg_phase_error * 0.99) + (ABS(phase_err) * 0.01);
    if (ABS(phase_err) > pll->stats.max_phase_error) {
        pll->stats.max_phase_error = ABS(phase_err);
    }
    
    /* =========================================
     * Step 2: Update jitter tracking
     * ========================================= */
    update_jitter_average(pll, phase_err);
    pll->stats.avg_jitter = pll->jitter_avg;
    
    /* =========================================
     * Step 3: Adjust gain based on conditions
     * ========================================= */
    adjust_adaptive_gain(pll);
    
    /* =========================================
     * Step 4: PID calculation
     * ========================================= */
    
    /* Proportional */
    pll->phase_err_P = phase_err * pll->Kp;
    
    /* Integral with anti-windup */
    pll->phase_err_I += phase_err;
    double max_integral = (pll->cell_ref * 0.2) / pll->Ki;  /* Limit to 20% */
    pll->phase_err_I = CLAMP(pll->phase_err_I, -max_integral, max_integral);
    
    /* Derivative */
    pll->phase_err_D = (phase_err - pll->prev_phase_err) * pll->Kd;
    pll->prev_phase_err = phase_err;
    
    /* =========================================
     * Step 5: Calculate new cell size
     * ========================================= */
    double pid_output = pll->phase_err_P + 
                       (pll->phase_err_I * pll->Ki) - 
                       pll->phase_err_D;
    
    double new_cell = pll->cell_ref - (pid_output * pll->gain_current);
    
    /* Clamp to tolerance */
    double min_cell = pll->cell_ref / (1.0 + pll->tolerance);
    double max_cell = pll->cell_ref * (1.0 + pll->tolerance);
    pll->cell_size = CLAMP(new_cell, min_cell, max_cell);
    
    update_cell_params(pll);
    pll->stats.current_cell_size = pll->cell_size;
    
    /* =========================================
     * Step 6: Update lock status
     * ========================================= */
    update_lock_status(pll, phase_err);
    
    /* =========================================
     * Step 7: Determine bit value and confidence
     * ========================================= */
    int num_bits = 0;
    
    /* How many cells did this pulse span? */
    double cells = pulse_pos / pll->cell_size;
    int cell_count = (int)(cells + 0.5);
    if (cell_count < 1) cell_count = 1;
    
    /* Confidence based on position within window */
    uint8_t confidence = 255;
    double dist_from_center = ABS(pulse_pos - (cell_count * pll->cell_center));
    double half_window = pll->window_size / 2.0;
    
    if (dist_from_center > half_window) {
        /* Outside window - low confidence */
        confidence = 64;
    } else if (dist_from_center > half_window * 0.7) {
        /* Edge of window */
        confidence = 128;
    } else if (dist_from_center > half_window * 0.3) {
        /* Good position */
        confidence = 200;
    }
    /* else: very good, keep 255 */
    
    /* Output bits: (cell_count - 1) zeros, then one 1 */
    if (out_bit && out_confidence) {
        /* For simplicity, return last bit info */
        *out_bit = 1;  /* Pulse = 1 */
        *out_confidence = confidence;
        num_bits = cell_count;
    }
    
    return num_bits;
}

void uft_pll_get_stats(const uft_adaptive_pll_t *pll, uft_pll_stats_t *stats) {
    if (!pll || !stats) return;
    *stats = pll->stats;
}

void uft_pll_dump_status(const uft_adaptive_pll_t *pll) {
    if (!pll) {
        printf("PLL: NULL\n");
        return;
    }
    
    printf("=== Adaptive PLL Status ===\n");
    printf("Cell size: %.4f (ref: %.4f, deviation: %.1f%%)\n",
           pll->cell_size, pll->cell_ref,
           (pll->cell_size / pll->cell_ref - 1.0) * 100.0);
    printf("Window: [%.2f - %.2f] (ratio: %.2f)\n",
           pll->window_start, pll->window_start + pll->window_size,
           pll->window_ratio);
    printf("Lock status: %s (counter: %zu/%zu)\n",
           pll->is_locked ? "LOCKED" : "UNLOCKED",
           pll->lock_counter, pll->unlock_threshold);
    printf("Gain: %.3f (target: %.3f, mode: %s)\n",
           pll->gain_current, pll->gain_target,
           pll->mode == UFT_PLL_MODE_SYNC ? "SYNC" :
           pll->mode == UFT_PLL_MODE_DATA ? "DATA" : "ADAPTIVE");
    printf("Jitter: %.4f (threshold: %.4f)\n",
           pll->jitter_avg, pll->jitter_threshold);
    printf("PID: P=%.4f I=%.4f D=%.4f\n",
           pll->phase_err_P, pll->phase_err_I * pll->Ki, pll->phase_err_D);
    printf("Stats: %zu pulses, %zu locks, %zu unlocks, %u mode switches\n",
           pll->stats.total_pulses, pll->stats.lock_count,
           pll->stats.unlock_count, pll->stats.mode_switches);
}
