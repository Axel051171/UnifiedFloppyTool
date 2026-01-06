/*
 * uft_vfo_pll.h
 *
 * Variable Frequency Oscillator (VFO) and Phase-Locked Loop (PLL)
 * for flux stream decoding.
 *
 * This module provides software VFO implementations for:
 * - MFM (Modified Frequency Modulation) decoding
 * - FM (Frequency Modulation) decoding
 * - GCR (Group Code Recording) decoding
 * - Handling timing-dependent copy protection
 *
 * VFO Types:
 * - SIMPLE: Basic fixed-window sampler
 * - FIXED: Fixed frequency, no tracking
 * - PID: Proportional-Integral-Derivative control
 * - ADAPTIVE: Adaptive gain based on sync detection
 * - DPLL: Digital Phase-Locked Loop
 *
 * Reference: Based on concepts from fdc_bitstream by yas-sim (MIT License)
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_vfo_pll.c
 */

#ifndef UFT_VFO_PLL_H
#define UFT_VFO_PLL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO TYPES AND CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * VFO algorithm types.
 */
typedef enum uft_vfo_type {
    UFT_VFO_SIMPLE    = 0,   /* Simple fixed-window sampler */
    UFT_VFO_FIXED     = 1,   /* Fixed frequency, no tracking */
    UFT_VFO_PID       = 2,   /* PID controller based */
    UFT_VFO_PID2      = 3,   /* PID variant 2 (faster convergence) */
    UFT_VFO_PID3      = 4,   /* PID variant 3 (better stability) */
    UFT_VFO_ADAPTIVE  = 5,   /* Adaptive gain based on sync */
    UFT_VFO_DPLL      = 6    /* Digital Phase-Locked Loop */
} uft_vfo_type_t;

/**
 * Encoding type for VFO configuration.
 */
typedef enum uft_encoding_type {
    UFT_ENC_MFM       = 0,   /* MFM: 500 kbps, 4µs bit cell */
    UFT_ENC_FM        = 1,   /* FM: 250 kbps, 8µs bit cell */
    UFT_ENC_GCR       = 2,   /* GCR: variable bit rate */
    UFT_ENC_M2FM      = 3    /* M2FM: Modified MFM */
} uft_encoding_type_t;

/* Default bit cell times (in nanoseconds) */
#define UFT_MFM_BIT_CELL_NS     2000    /* 2µs = 500 kbps */
#define UFT_FM_BIT_CELL_NS      4000    /* 4µs = 250 kbps */
#define UFT_GCR_BIT_CELL_NS     3200    /* ~3.2µs for C64 zone 3 */

/* Default VFO gain values */
#define UFT_VFO_GAIN_LOW_DEFAULT    0.1     /* Slow tracking */
#define UFT_VFO_GAIN_HIGH_DEFAULT   0.5     /* Fast tracking */

/* Window timing (as fraction of bit cell) */
#define UFT_VFO_WINDOW_EARLY    0.4     /* Early edge of window */
#define UFT_VFO_WINDOW_LATE     0.6     /* Late edge of window */

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO STATE STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * VFO/PLL state structure.
 */
typedef struct uft_vfo_state {
    /* Configuration */
    uft_vfo_type_t      type;           /* VFO algorithm type */
    uft_encoding_type_t encoding;       /* Encoding type */
    
    /* Timing (in sample units, typically 1/4 MHz = 250ns) */
    double              bit_cell;       /* Current bit cell width */
    double              bit_cell_nom;   /* Nominal bit cell width */
    double              sample_rate;    /* Sample rate in Hz */
    
    /* Phase tracking */
    double              phase;          /* Current phase (0.0 - 1.0) */
    double              freq;           /* Current frequency multiplier */
    
    /* Window */
    double              window_start;   /* Window start (fraction) */
    double              window_end;     /* Window end (fraction) */
    
    /* Gain control */
    double              gain_low;       /* Low gain (for tracking) */
    double              gain_high;      /* High gain (for sync acquire) */
    double              gain_current;   /* Current active gain */
    
    /* PID state */
    double              pid_p;          /* Proportional gain */
    double              pid_i;          /* Integral gain */
    double              pid_d;          /* Derivative gain */
    double              pid_integral;   /* Integral accumulator */
    double              pid_prev_error; /* Previous error for derivative */
    
    /* Sync detection */
    int                 sync_count;     /* Consecutive sync pulses */
    int                 sync_threshold; /* Pulses needed for sync */
    bool                synced;         /* Currently synchronized */
    
    /* Statistics */
    uint64_t            pulses_total;   /* Total pulses processed */
    uint64_t            pulses_valid;   /* Pulses within window */
    uint64_t            pulses_early;   /* Pulses early */
    uint64_t            pulses_late;    /* Pulses late */
    double              avg_phase_err;  /* Average phase error */
    
    /* Fluctuator (for copy protection) */
    bool                fluctuator_en;  /* Fluctuator enabled */
    double              fluctuator_amt; /* Fluctuation amount */
    uint32_t            fluctuator_seed;/* Random seed */
    
    /* Output buffer */
    uint8_t            *bit_buffer;     /* Decoded bits output */
    size_t              bit_buffer_size;/* Buffer size in bytes */
    size_t              bit_count;      /* Bits decoded */
} uft_vfo_state_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Initialize VFO state with default parameters.
 *
 * @param state         VFO state to initialize
 * @param type          VFO algorithm type
 * @param encoding      Encoding type (MFM, FM, GCR)
 * @param sample_rate   Sample rate in Hz (e.g., 4000000 for 4 MHz)
 */
void uft_vfo_init(uft_vfo_state_t *state,
                  uft_vfo_type_t type,
                  uft_encoding_type_t encoding,
                  double sample_rate);

/**
 * Initialize VFO with custom bit cell timing.
 *
 * @param state         VFO state to initialize
 * @param type          VFO algorithm type
 * @param sample_rate   Sample rate in Hz
 * @param bit_cell_ns   Bit cell width in nanoseconds
 */
void uft_vfo_init_custom(uft_vfo_state_t *state,
                         uft_vfo_type_t type,
                         double sample_rate,
                         double bit_cell_ns);

/**
 * Reset VFO to initial state (keep configuration).
 *
 * @param state         VFO state to reset
 */
void uft_vfo_reset(uft_vfo_state_t *state);

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Set VFO gain values.
 *
 * @param state         VFO state
 * @param gain_low      Low gain (0.0-1.0, slow tracking)
 * @param gain_high     High gain (0.0-1.0, fast acquire)
 */
void uft_vfo_set_gain(uft_vfo_state_t *state,
                      double gain_low, double gain_high);

/**
 * Set PID controller parameters.
 *
 * @param state         VFO state
 * @param p             Proportional gain
 * @param i             Integral gain
 * @param d             Derivative gain
 */
void uft_vfo_set_pid(uft_vfo_state_t *state,
                     double p, double i, double d);

/**
 * Set data window timing.
 *
 * @param state         VFO state
 * @param early         Window early edge (0.0-0.5)
 * @param late          Window late edge (0.5-1.0)
 */
void uft_vfo_set_window(uft_vfo_state_t *state,
                        double early, double late);

/**
 * Enable/disable fluctuator for copy protection handling.
 *
 * @param state         VFO state
 * @param enable        Enable fluctuator
 * @param amount        Fluctuation amount (0.0-0.1)
 */
void uft_vfo_enable_fluctuator(uft_vfo_state_t *state,
                               bool enable, double amount);

/**
 * Set output bit buffer.
 *
 * @param state         VFO state
 * @param buffer        Output buffer for decoded bits
 * @param size          Buffer size in bytes
 */
void uft_vfo_set_output(uft_vfo_state_t *state,
                        uint8_t *buffer, size_t size);

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO PROCESSING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Process a single flux pulse.
 *
 * @param state         VFO state
 * @param interval      Interval since last pulse (in samples)
 * @return              Number of bits decoded (0, 1, 2, or more)
 */
int uft_vfo_process_pulse(uft_vfo_state_t *state, double interval);

/**
 * Process an array of flux intervals.
 *
 * @param state         VFO state
 * @param intervals     Array of intervals (in samples)
 * @param count         Number of intervals
 * @return              Total bits decoded
 */
size_t uft_vfo_process_intervals(uft_vfo_state_t *state,
                                 const double *intervals,
                                 size_t count);

/**
 * Process raw flux timing data.
 *
 * @param state         VFO state
 * @param flux_times    Array of absolute flux times (in samples)
 * @param count         Number of flux transitions
 * @return              Total bits decoded
 */
size_t uft_vfo_process_flux(uft_vfo_state_t *state,
                            const uint32_t *flux_times,
                            size_t count);

/**
 * Force sync acquisition.
 *
 * Call this when you know a sync pattern is starting.
 *
 * @param state         VFO state
 */
void uft_vfo_force_sync(uft_vfo_state_t *state);

/**
 * Check if VFO is currently synchronized.
 *
 * @param state         VFO state
 * @return              true if synchronized
 */
bool uft_vfo_is_synced(const uft_vfo_state_t *state);

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO STATISTICS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * VFO statistics structure.
 */
typedef struct uft_vfo_stats {
    uint64_t    pulses_total;       /* Total pulses processed */
    uint64_t    pulses_valid;       /* Pulses within window */
    uint64_t    pulses_early;       /* Pulses early */
    uint64_t    pulses_late;        /* Pulses late */
    double      valid_percent;      /* Percentage valid */
    double      avg_phase_error;    /* Average phase error */
    double      current_freq;       /* Current frequency */
    double      current_bit_cell;   /* Current bit cell width */
    size_t      bits_decoded;       /* Total bits decoded */
} uft_vfo_stats_t;

/**
 * Get VFO statistics.
 *
 * @param state         VFO state
 * @param stats         Output statistics structure
 */
void uft_vfo_get_stats(const uft_vfo_state_t *state, uft_vfo_stats_t *stats);

/**
 * Reset VFO statistics counters.
 *
 * @param state         VFO state
 */
void uft_vfo_reset_stats(uft_vfo_state_t *state);

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA SEPARATOR
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Data separator state (combines VFO with MFM/FM decoder).
 */
typedef struct uft_data_separator {
    uft_vfo_state_t     vfo;            /* VFO state */
    
    /* MFM/FM decoder state */
    uint8_t             shift_reg;      /* Shift register */
    int                 bit_counter;    /* Bits in shift reg */
    bool                clock_bit;      /* Last bit was clock */
    
    /* Sync detection */
    uint16_t            sync_pattern;   /* Expected sync pattern */
    uint16_t            sync_mask;      /* Sync mask */
    bool                sync_found;     /* Sync detected */
    
    /* Output */
    uint8_t            *data_buffer;    /* Decoded data output */
    size_t              data_size;      /* Buffer size */
    size_t              data_count;     /* Bytes decoded */
} uft_data_separator_t;

/**
 * Initialize data separator.
 *
 * @param sep           Data separator state
 * @param vfo_type      VFO algorithm type
 * @param encoding      Encoding type
 * @param sample_rate   Sample rate in Hz
 */
void uft_datasep_init(uft_data_separator_t *sep,
                      uft_vfo_type_t vfo_type,
                      uft_encoding_type_t encoding,
                      double sample_rate);

/**
 * Reset data separator state.
 *
 * @param sep           Data separator state
 */
void uft_datasep_reset(uft_data_separator_t *sep);

/**
 * Set data output buffer.
 *
 * @param sep           Data separator state
 * @param buffer        Output buffer
 * @param size          Buffer size in bytes
 */
void uft_datasep_set_output(uft_data_separator_t *sep,
                            uint8_t *buffer, size_t size);

/**
 * Set sync pattern to detect.
 *
 * @param sep           Data separator state
 * @param pattern       Sync pattern (e.g., 0x4489 for MFM)
 * @param mask          Mask for pattern matching
 */
void uft_datasep_set_sync(uft_data_separator_t *sep,
                          uint16_t pattern, uint16_t mask);

/**
 * Process flux data through data separator.
 *
 * @param sep           Data separator state
 * @param flux_times    Flux transition times
 * @param count         Number of transitions
 * @return              Bytes of data decoded
 */
size_t uft_datasep_process(uft_data_separator_t *sep,
                           const uint32_t *flux_times,
                           size_t count);

/**
 * Check if sync was found.
 *
 * @param sep           Data separator state
 * @return              true if sync detected
 */
bool uft_datasep_sync_found(const uft_data_separator_t *sep);

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Get VFO type name string.
 *
 * @param type          VFO type
 * @return              Static string with type name
 */
const char *uft_vfo_type_name(uft_vfo_type_t type);

/**
 * Get encoding type name string.
 *
 * @param encoding      Encoding type
 * @return              Static string with encoding name
 */
const char *uft_encoding_name(uft_encoding_type_t encoding);

/**
 * Calculate bit cell width from data rate.
 *
 * @param data_rate     Data rate in bits per second
 * @param sample_rate   Sample rate in Hz
 * @return              Bit cell width in samples
 */
double uft_vfo_calc_bit_cell(double data_rate, double sample_rate);

/**
 * Estimate data rate from flux intervals.
 *
 * @param intervals     Flux intervals
 * @param count         Number of intervals
 * @param sample_rate   Sample rate in Hz
 * @return              Estimated data rate in bps
 */
double uft_vfo_estimate_rate(const double *intervals,
                             size_t count,
                             double sample_rate);

#ifdef __cplusplus
}
#endif

#endif /* UFT_VFO_PLL_H */
