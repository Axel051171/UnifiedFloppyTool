/**
 * @file uft_adaptive_pll.h
 * @brief Adaptive PID-based PLL for robust MFM/GCR data separation
 * 
 * This PLL implementation addresses the #1 algorithmic weakness:
 * Sync loss under phase jitter conditions.
 * 
 * Features:
 * - PID control with anti-windup
 * - Adaptive gain based on jitter detection
 * - Dual-mode operation (sync vs data)
 * - Sub-sample precision
 * 
 * @author UFT Team
 * @date 2026-01-07
 */

#ifndef UFT_ADAPTIVE_PLL_H
#define UFT_ADAPTIVE_PLL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration constants */
#define UFT_PLL_HISTORY_SIZE    16
#define UFT_PLL_JITTER_WINDOW   8

/* PLL operating modes */
typedef enum {
    UFT_PLL_MODE_SYNC,      /* High gain for sync field lock-in */
    UFT_PLL_MODE_DATA,      /* Low gain for stable data reading */
    UFT_PLL_MODE_ADAPTIVE   /* Auto-switch based on conditions */
} uft_pll_mode_t;

/* PLL statistics for diagnostics */
typedef struct {
    size_t   total_pulses;
    size_t   lock_count;
    size_t   unlock_count;
    double   avg_phase_error;
    double   max_phase_error;
    double   avg_jitter;
    double   current_cell_size;
    uint32_t mode_switches;
} uft_pll_stats_t;

/**
 * @brief Adaptive PLL state structure
 */
typedef struct {
    /* Timing parameters */
    double cell_size;           /**< Current cell size (samples) */
    double cell_ref;            /**< Reference cell size */
    double window_ratio;        /**< Data window ratio (0.5-0.9) */
    double window_size;         /**< Current window size */
    double window_start;        /**< Window start offset */
    double cell_center;         /**< Cell center position */
    
    /* PID state */
    double phase_err_P;         /**< Proportional term */
    double phase_err_I;         /**< Integral term */
    double phase_err_D;         /**< Derivative term */
    double prev_phase_err;      /**< Previous error for D term */
    
    /* PID coefficients */
    double Kp;                  /**< Proportional gain (default: 0.25) */
    double Ki;                  /**< Integral gain (default: 0.015625) */
    double Kd;                  /**< Derivative gain (default: 0.0625) */
    
    /* Adaptive gain control */
    double gain_sync;           /**< Gain for sync mode (1.0) */
    double gain_data;           /**< Gain for data mode (0.3) */
    double gain_current;        /**< Current active gain */
    double gain_target;         /**< Target gain (for smooth transitions) */
    
    /* Jitter detection */
    double jitter_history[UFT_PLL_JITTER_WINDOW];
    size_t jitter_idx;
    double jitter_avg;
    double jitter_threshold;    /**< Threshold for gain reduction */
    
    /* Phase history for analysis */
    double phase_history[UFT_PLL_HISTORY_SIZE];
    size_t phase_idx;
    
    /* Operating mode */
    uft_pll_mode_t mode;
    bool   is_locked;
    size_t lock_counter;        /**< Consecutive good pulses */
    size_t unlock_threshold;    /**< Pulses needed for lock */
    
    /* Configuration */
    double sample_rate;
    double bit_rate;
    double tolerance;           /**< Max cell size deviation (0.4 = ±40%) */
    
    /* Statistics */
    uft_pll_stats_t stats;
    
} uft_adaptive_pll_t;

/**
 * @brief Initialize PLL with default parameters
 */
void uft_pll_init(uft_adaptive_pll_t *pll);

/**
 * @brief Configure PLL for specific bit rate
 * @param pll PLL instance
 * @param sample_rate Sample rate in Hz (e.g., 4000000)
 * @param bit_rate Bit rate in Hz (e.g., 500000 for MFM DD)
 */
void uft_pll_configure(uft_adaptive_pll_t *pll,
                       double sample_rate,
                       double bit_rate);

/**
 * @brief Set PID coefficients
 */
void uft_pll_set_pid(uft_adaptive_pll_t *pll,
                     double Kp, double Ki, double Kd);

/**
 * @brief Set operating mode
 */
void uft_pll_set_mode(uft_adaptive_pll_t *pll, uft_pll_mode_t mode);

/**
 * @brief Reset PLL state (soft reset, keeps configuration)
 */
void uft_pll_reset(uft_adaptive_pll_t *pll);

/**
 * @brief Hard reset (resets everything including config)
 */
void uft_pll_hard_reset(uft_adaptive_pll_t *pll);

/**
 * @brief Process a pulse and update PLL
 * @param pll PLL instance
 * @param pulse_pos Position of pulse in samples from last pulse
 * @param out_bit Output: decoded bit value (0 or 1)
 * @param out_confidence Output: confidence (0-255)
 * @return Number of bits decoded (0, 1, or more for long gaps)
 */
int uft_pll_process_pulse(uft_adaptive_pll_t *pll,
                          double pulse_pos,
                          uint8_t *out_bit,
                          uint8_t *out_confidence);

/**
 * @brief Check if PLL is locked
 */
static inline bool uft_pll_is_locked(const uft_adaptive_pll_t *pll) {
    return pll->is_locked;
}

/**
 * @brief Get current cell size
 */
static inline double uft_pll_get_cell_size(const uft_adaptive_pll_t *pll) {
    return pll->cell_size;
}

/**
 * @brief Get PLL statistics
 */
void uft_pll_get_stats(const uft_adaptive_pll_t *pll, uft_pll_stats_t *stats);

/**
 * @brief Print PLL status for debugging
 */
void uft_pll_dump_status(const uft_adaptive_pll_t *pll);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ADAPTIVE_PLL_H */
