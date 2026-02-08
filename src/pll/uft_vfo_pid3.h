/**
 * @file uft_vfo_pid3.h
 * @brief PID-based VFO (Variable Frequency Oscillator) for MFM data separation
 * 
 * Original author: Yasunori Shimura
 * 
 * This VFO implementation uses a PID controller to track the bit cell timing
 * and provides superior data separation compared to simple phase-tracking PLLs.
 * 
 * Features:
 * - PID control with configurable coefficients
 * - Low-pass filtering of pulse positions
 * - Dual gain modes (high for sync, low for data)
 * - Spindle speed variation tolerance (±40%)
 */

#ifndef UFT_VFO_PID3_H
#define UFT_VFO_PID3_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Default gain values */
#define UFT_VFO_GAIN_L_DEFAULT  0.3
#define UFT_VFO_GAIN_H_DEFAULT  1.0

/* VFO type identifiers */
typedef enum {
    UFT_VFO_TYPE_SIMPLE  = 0,
    UFT_VFO_TYPE_FIXED   = 1,
    UFT_VFO_TYPE_PID     = 2,
    UFT_VFO_TYPE_PID2    = 3,
    UFT_VFO_TYPE_SIMPLE2 = 4,
    UFT_VFO_TYPE_PID3    = 5,  /* Recommended */
    UFT_VFO_TYPE_DEFAULT = UFT_VFO_TYPE_PID3
} uft_vfo_type_t;

/* Gain state for VFO */
typedef enum {
    UFT_VFO_GAIN_LOW  = 0,  /* For regular data reading */
    UFT_VFO_GAIN_HIGH = 1   /* For SYNC field (fast lock-in) */
} uft_vfo_gain_state_t;

/* LPF history length (must be power of 2) */
#define UFT_VFO_HISTORY_LEN 4

/**
 * @brief VFO PID3 state structure
 */
typedef struct {
    /* Cell timing parameters */
    double cell_size;           /**< Current data cell size (sampling units) */
    double cell_size_ref;       /**< Reference cell size (standard) */
    double window_ratio;        /**< Data window width ratio (0.75 typical) */
    double window_size;         /**< Current window size */
    double window_ofst;         /**< Window start offset */
    double cell_center;         /**< Center of bit cell */
    
    /* Gain control */
    double gain_l;              /**< Low gain value */
    double gain_h;              /**< High gain value */
    double current_gain;        /**< Currently active gain */
    double gain_used;           /**< Smoothed gain (prevents instability) */
    
    /* Configuration */
    double sampling_rate;       /**< Sampling rate in Hz */
    double fdc_bit_rate;        /**< FDC bit rate in Hz */
    double data_window_ratio;   /**< Configured window ratio */
    
    /* PID state */
    double prev_pulse_pos;      /**< Previous pulse position */
    double prev_phase_err;      /**< Previous phase error */
    double phase_err_I;         /**< Integrated phase error */
    
    /* PID coefficients */
    double phase_err_PC;        /**< Proportional coefficient */
    double phase_err_IC;        /**< Integral coefficient */
    double phase_err_DC;        /**< Derivative coefficient */
    
    /* Low-pass filter history */
    double pulse_history[UFT_VFO_HISTORY_LEN];
    size_t hist_ptr;
    double coeff_sum;           /**< Sum of LPF coefficients */
    
} uft_vfo_pid3_t;

/**
 * @brief Initialize VFO PID3 with default parameters
 * @param vfo Pointer to VFO structure
 */
void uft_vfo_pid3_init(uft_vfo_pid3_t *vfo);

/**
 * @brief Full reset of VFO state
 * @param vfo Pointer to VFO structure
 */
void uft_vfo_pid3_reset(uft_vfo_pid3_t *vfo);

/**
 * @brief Soft reset - keeps cell_size_ref, resets window and PID state
 * @param vfo Pointer to VFO structure
 */
void uft_vfo_pid3_soft_reset(uft_vfo_pid3_t *vfo);

/**
 * @brief Set VFO parameters from sampling rate and bit rate
 * @param vfo Pointer to VFO structure
 * @param sampling_rate Sampling rate in Hz (e.g., 4000000 for 4MHz)
 * @param fdc_bit_rate FDC bit rate in Hz (e.g., 500000 for MFM DD)
 * @param window_ratio Data window ratio (0.5-0.8, default 0.75)
 */
void uft_vfo_pid3_set_params(uft_vfo_pid3_t *vfo,
                              size_t sampling_rate,
                              size_t fdc_bit_rate,
                              double window_ratio);

/**
 * @brief Set cell size directly
 * @param vfo Pointer to VFO structure
 * @param cell_size Cell size in sampling units
 */
void uft_vfo_pid3_set_cell_size(uft_vfo_pid3_t *vfo, double cell_size);

/**
 * @brief Set gain values
 * @param vfo Pointer to VFO structure
 * @param gain_l Low gain (for data reading, e.g., 0.3)
 * @param gain_h High gain (for sync field, e.g., 1.0)
 */
void uft_vfo_pid3_set_gain_val(uft_vfo_pid3_t *vfo, double gain_l, double gain_h);

/**
 * @brief Set current gain mode
 * @param vfo Pointer to VFO structure
 * @param state UFT_VFO_GAIN_LOW or UFT_VFO_GAIN_HIGH
 */
void uft_vfo_pid3_set_gain_mode(uft_vfo_pid3_t *vfo, uft_vfo_gain_state_t state);

/**
 * @brief Set PID coefficients
 * @param vfo Pointer to VFO structure
 * @param p_coeff Proportional coefficient (default 0.25)
 * @param i_coeff Integral coefficient (default 0.015625)
 * @param d_coeff Derivative coefficient (default 0.0625)
 */
void uft_vfo_pid3_set_pid_coeff(uft_vfo_pid3_t *vfo,
                                 double p_coeff,
                                 double i_coeff,
                                 double d_coeff);

/**
 * @brief Calculate new cell timing from pulse position
 * 
 * This is the core PID algorithm:
 * 1. Adjust pulse position for phase jumps
 * 2. Apply LPF to smooth pulse positions
 * 3. Calculate phase error from cell center
 * 4. Apply PID control to adjust cell size
 * 5. Clamp cell size within tolerance (±40%)
 * 
 * @param vfo Pointer to VFO structure
 * @param pulse_pos Distance to next pulse in sampling units
 * @return Adjusted pulse position
 */
double uft_vfo_pid3_calc(uft_vfo_pid3_t *vfo, double pulse_pos);

/**
 * @brief Get current cell size
 * @param vfo Pointer to VFO structure
 * @return Current cell size in sampling units
 */
static inline double uft_vfo_pid3_get_cell_size(const uft_vfo_pid3_t *vfo) {
    return vfo->cell_size;
}

/**
 * @brief Get cell center position
 * @param vfo Pointer to VFO structure
 * @return Cell center in sampling units
 */
static inline double uft_vfo_pid3_get_cell_center(const uft_vfo_pid3_t *vfo) {
    return vfo->cell_center;
}

/**
 * @brief Get window boundaries
 * @param vfo Pointer to VFO structure
 * @param start Output: window start offset
 * @param end Output: window end offset
 */
void uft_vfo_pid3_get_window(const uft_vfo_pid3_t *vfo, 
                              double *start, 
                              double *end);

/**
 * @brief Check if pulse is within data window
 * @param vfo Pointer to VFO structure
 * @param pulse_pos Pulse position
 * @return true if within window, false otherwise
 */
bool uft_vfo_pid3_is_in_window(const uft_vfo_pid3_t *vfo, double pulse_pos);

/**
 * @brief Print VFO status (for debugging)
 * @param vfo Pointer to VFO structure
 */
void uft_vfo_pid3_dump_status(const uft_vfo_pid3_t *vfo);

#ifdef __cplusplus
}
#endif

#endif /* UFT_VFO_PID3_H */
