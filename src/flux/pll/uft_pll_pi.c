/**
 * @file uft_pll_pi.c
 * @brief PI Loop Filter PLL Implementation
 * 
 * Based on raszpl/sigrok-disk modern PLL implementation.
 * 
 * @copyright GPL-3.0 (derived from sigrok-disk)
 */

#include "uft_pll_pi.h"
#include <math.h>
#include <string.h>

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/* Default PI constants (from sigrok-disk) */
#define DEFAULT_KP          0.5
#define DEFAULT_KI          0.0005
#define DEFAULT_SYNC_TOL    0.25
#define DEFAULT_LOCK_THRESH 0.1

/* Sync requirements */
#define SYNC_TRANSITIONS_REQUIRED 16
#define SYNC_QUALITY_THRESHOLD    0.8

/* MFM sync pattern: A1 = 0100010010001001 with missing clock */
#define MFM_SYNC_A1_PATTERN     0x4489
#define MFM_SYNC_A1_CLOCK_MASK  0x0000  /* Missing clock at position 4 */

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static inline double clamp(double val, double min, double max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

static inline int count_bits_to_cells(double delta, double period, int encoding) {
    /* How many bit cells does this transition span? */
    double cells = delta / period;
    
    switch (encoding) {
        case UFT_ENC_FM:
            /* FM: cells can be 1 or 2 */
            if (cells < 1.5) return 1;
            if (cells < 2.5) return 2;
            return -1;  /* Error */
            
        case UFT_ENC_MFM:
            /* MFM: cells can be 1, 1.5, or 2 (representing 01, 001, 0001) */
            if (cells < 1.25) return 2;       /* Short: 1 cell */
            if (cells < 1.75) return 3;       /* Medium: 1.5 cells */
            if (cells < 2.25) return 4;       /* Long: 2 cells */
            return -1;
            
        case UFT_ENC_RLL_27:
            /* RLL 2,7: cells can be 2, 3, 4, or more */
            if (cells < 2.5) return 2;
            if (cells < 3.5) return 3;
            if (cells < 4.5) return 4;
            if (cells < 5.5) return 5;
            if (cells < 6.5) return 6;
            if (cells < 7.5) return 7;
            if (cells < 8.5) return 8;
            return -1;
            
        default:
            /* Default to MFM behavior */
            if (cells < 1.25) return 2;
            if (cells < 1.75) return 3;
            if (cells < 2.25) return 4;
            return -1;
    }
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

void uft_pll_config_default(uft_pll_config_t *config,
                            uft_encoding_t encoding,
                            uint32_t data_rate) {
    if (!config) return;
    
    config->kp = DEFAULT_KP;
    config->ki = DEFAULT_KI;
    config->sync_tolerance = DEFAULT_SYNC_TOL;
    config->lock_threshold = DEFAULT_LOCK_THRESH;
    config->encoding = encoding;
    config->data_rate = data_rate;
}

void uft_pll_config_aggressive(uft_pll_config_t *config) {
    if (!config) return;
    
    /* More aggressive tracking for problematic media */
    config->kp = 1.0;           /* Faster response */
    config->ki = 0.001;         /* Faster integration */
    config->sync_tolerance = 0.33;  /* Looser sync */
    config->lock_threshold = 0.15;  /* Easier lock */
}

int uft_pll_init(uft_pll_t *pll, const uft_pll_config_t *config) {
    if (!pll) return -1;
    
    memset(pll, 0, sizeof(uft_pll_t));
    
    if (config) {
        pll->config = *config;
    } else {
        uft_pll_config_default(&pll->config, UFT_ENC_MFM, UFT_RATE_MFM_DD);
    }
    
    /* Calculate nominal period */
    pll->nominal_period = uft_pll_nominal_period(
        pll->config.data_rate, pll->config.encoding);
    pll->current_period = pll->nominal_period;
    pll->tolerance = pll->nominal_period * pll->config.sync_tolerance;
    
    /* Initialize state */
    pll->state = UFT_PLL_SEEKING;
    pll->sync_required = SYNC_TRANSITIONS_REQUIRED;
    
    /* Statistics initialization */
    pll->min_period_seen = pll->nominal_period * 2;
    pll->max_period_seen = 0;
    
    return 0;
}

void uft_pll_reset(uft_pll_t *pll) {
    if (!pll) return;
    
    pll->current_period = pll->nominal_period;
    pll->tolerance = pll->nominal_period * pll->config.sync_tolerance;
    pll->integral = 0;
    pll->last_error = 0;
    pll->state = UFT_PLL_SEEKING;
    pll->sync_count = 0;
    pll->accumulated = 0;
    pll->shift_reg = 0;
    pll->bits_pending = 0;
}

/* ============================================================================
 * Core Processing
 * ============================================================================ */

int uft_pll_process_transition(uft_pll_t *pll,
                               double delta_ns,
                               uft_pll_bit_t *bits,
                               int *bit_count) {
    if (!pll || !bits || !bit_count) return -1;
    
    *bit_count = 0;
    pll->total_transitions++;
    
    /* Update statistics */
    if (delta_ns < pll->min_period_seen) pll->min_period_seen = delta_ns;
    if (delta_ns > pll->max_period_seen) pll->max_period_seen = delta_ns;
    
    /* Calculate error from expected period */
    double expected = pll->current_period;
    double error = 0;
    int num_cells;
    
    /* Determine how many bit cells this transition spans */
    num_cells = count_bits_to_cells(delta_ns, pll->current_period, 
                                    pll->config.encoding);
    
    if (num_cells < 0) {
        /* Out of tolerance */
        pll->out_of_tolerance++;
        
        if (pll->state == UFT_PLL_LOCKED || pll->state == UFT_PLL_TRACKING) {
            /* Lost lock, need to resync */
            pll->state = UFT_PLL_SEEKING;
            pll->sync_count = 0;
        }
        
        return 0;  /* Skip this transition */
    }
    
    /* Calculate expected time for this number of cells */
    double expected_time = num_cells * (pll->current_period / 2.0);
    error = delta_ns - expected_time;
    
    /* PI filter update */
    pll->integral += error;
    
    /* Limit integral windup */
    double max_integral = pll->nominal_period * 0.5;
    pll->integral = clamp(pll->integral, -max_integral, max_integral);
    
    /* Calculate adjustment */
    double adjustment = (pll->config.kp * error) + 
                       (pll->config.ki * pll->integral);
    
    /* Apply adjustment to period (per cell) */
    pll->current_period += adjustment / num_cells;
    
    /* Keep period within reasonable bounds */
    double min_period = pll->nominal_period * 0.8;
    double max_period = pll->nominal_period * 1.2;
    pll->current_period = clamp(pll->current_period, min_period, max_period);
    
    /* Update tolerance based on lock state */
    if (pll->state >= UFT_PLL_LOCKED) {
        /* Tighten tolerance when locked */
        pll->tolerance = pll->current_period * pll->config.lock_threshold;
    }
    
    /* Check lock quality */
    double quality = 1.0 - fabs(error / pll->current_period);
    if (quality > 0.9) {
        pll->good_transitions++;
    }
    
    /* Update sync state */
    if (quality > SYNC_QUALITY_THRESHOLD) {
        pll->sync_count++;
        if (pll->sync_count >= pll->sync_required && 
            pll->state < UFT_PLL_LOCKED) {
            pll->state = UFT_PLL_LOCKED;
        }
    } else {
        pll->sync_count = 0;
        if (pll->state > UFT_PLL_SYNCING) {
            pll->clock_errors++;
        }
    }
    
    /* Generate bits based on encoding and cell count */
    switch (pll->config.encoding) {
        case UFT_ENC_MFM:
            /* MFM: 2 cells = "01", 3 cells = "001", 4 cells = "0001" */
            for (int i = 0; i < num_cells - 1; i++) {
                bits[*bit_count].value = 0;
                bits[*bit_count].is_clock = (i % 2 == 0);
                bits[*bit_count].timing = delta_ns / num_cells;
                bits[*bit_count].deviation = error / num_cells;
                (*bit_count)++;
            }
            bits[*bit_count].value = 1;
            bits[*bit_count].is_clock = false;
            bits[*bit_count].timing = delta_ns / num_cells;
            bits[*bit_count].deviation = error / num_cells;
            (*bit_count)++;
            break;
            
        case UFT_ENC_FM:
            /* FM: clock + data pairs */
            bits[0].value = 1;  /* Clock */
            bits[0].is_clock = true;
            bits[0].timing = delta_ns / num_cells;
            (*bit_count) = 1;
            
            if (num_cells == 2) {
                bits[1].value = 1;  /* Data 1 */
                bits[1].is_clock = false;
                bits[1].timing = delta_ns / 2;
                (*bit_count) = 2;
            }
            /* num_cells == 1 means data 0 */
            break;
            
        case UFT_ENC_RLL_27:
            /* RLL 2,7: direct bit output */
            for (int i = 0; i < num_cells - 1; i++) {
                bits[*bit_count].value = 0;
                bits[*bit_count].is_clock = false;
                (*bit_count)++;
            }
            bits[*bit_count].value = 1;
            bits[*bit_count].is_clock = false;
            (*bit_count)++;
            break;
            
        default:
            /* Treat as MFM */
            for (int i = 0; i < num_cells - 1; i++) {
                bits[*bit_count].value = 0;
                bits[*bit_count].is_clock = (i % 2 == 0);
                (*bit_count)++;
            }
            bits[*bit_count].value = 1;
            bits[*bit_count].is_clock = false;
            (*bit_count)++;
            break;
    }
    
    /* Update shift register for sync detection */
    for (int i = 0; i < *bit_count; i++) {
        pll->shift_reg = (pll->shift_reg << 1) | bits[i].value;
    }
    
    /* Check for sync mark (A1 pattern for MFM) */
    if (pll->config.encoding == UFT_ENC_MFM) {
        if ((pll->shift_reg & 0xFFFF) == MFM_SYNC_A1_PATTERN) {
            for (int i = 0; i < *bit_count; i++) {
                bits[i].is_sync = true;
            }
            pll->state = UFT_PLL_TRACKING;
        }
    }
    
    return 0;
}

int uft_pll_process_to_byte(uft_pll_t *pll,
                            double delta_ns,
                            uft_pll_byte_t *byte_out,
                            bool *byte_ready) {
    if (!pll || !byte_out || !byte_ready) return -1;
    
    *byte_ready = false;
    
    uft_pll_bit_t bits[8];
    int bit_count;
    
    int rc = uft_pll_process_transition(pll, delta_ns, bits, &bit_count);
    if (rc != 0) return rc;
    
    /* Accumulate bits (MFM: need 16 raw bits for 8 data bits) */
    for (int i = 0; i < bit_count; i++) {
        if (!bits[i].is_clock) {  /* Skip clock bits for byte assembly */
            pll->shift_reg = (pll->shift_reg << 1) | bits[i].value;
            pll->bits_pending++;
        }
    }
    
    /* Check if we have a complete byte */
    if (pll->bits_pending >= 8) {
        byte_out->value = pll->shift_reg & 0xFF;
        byte_out->clock_pattern = 0;  /* TODO: track clock pattern */
        byte_out->has_clock_error = false;
        byte_out->is_sync_mark = (byte_out->value == 0xA1 || 
                                  byte_out->value == 0xC2);
        
        pll->bits_pending -= 8;
        *byte_ready = true;
    }
    
    return 0;
}

int uft_pll_process_batch(uft_pll_t *pll,
                          const double *deltas,
                          size_t count,
                          uft_pll_byte_t *bytes,
                          size_t max_bytes,
                          size_t *bytes_decoded) {
    if (!pll || !deltas || !bytes || !bytes_decoded) return -1;
    
    *bytes_decoded = 0;
    
    for (size_t i = 0; i < count && *bytes_decoded < max_bytes; i++) {
        bool ready;
        int rc = uft_pll_process_to_byte(pll, deltas[i], 
                                         &bytes[*bytes_decoded], &ready);
        if (rc != 0) return rc;
        
        if (ready) {
            (*bytes_decoded)++;
        }
    }
    
    return 0;
}

/* ============================================================================
 * Sync Detection
 * ============================================================================ */

bool uft_pll_is_locked(const uft_pll_t *pll) {
    if (!pll) return false;
    return pll->state >= UFT_PLL_LOCKED;
}

double uft_pll_sync_quality(const uft_pll_t *pll) {
    if (!pll || pll->total_transitions == 0) return 0.0;
    return (double)pll->good_transitions / pll->total_transitions;
}

int uft_pll_force_sync(uft_pll_t *pll,
                       const double *preamble_deltas,
                       size_t count) {
    if (!pll || !preamble_deltas || count < 2) return -1;
    
    /* Average the preamble periods to establish timing */
    double sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += preamble_deltas[i];
    }
    
    /* Preamble should be regular pattern */
    double avg_period = sum / count;
    
    /* Use this as our reference period */
    pll->current_period = avg_period;
    pll->integral = 0;
    pll->state = UFT_PLL_LOCKED;
    pll->sync_count = pll->sync_required;
    
    return 0;
}

/* ============================================================================
 * Address Mark Detection
 * ============================================================================ */

bool uft_pll_is_sync_mark(const uft_pll_byte_t *byte) {
    if (!byte) return false;
    return byte->is_sync_mark;
}

int uft_pll_address_mark_type(uint8_t byte) {
    switch (byte) {
        case UFT_MARK_IDAM: return UFT_MARK_IDAM;
        case UFT_MARK_DAM:  return UFT_MARK_DAM;
        case UFT_MARK_DDAM: return UFT_MARK_DDAM;
        case UFT_MARK_IAM:  return UFT_MARK_IAM;
        default: return 0;
    }
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

void uft_pll_get_stats(const uft_pll_t *pll, uft_pll_stats_t *stats) {
    if (!pll || !stats) return;
    
    memset(stats, 0, sizeof(uft_pll_stats_t));
    
    stats->total_transitions = pll->total_transitions;
    stats->good_transitions = pll->good_transitions;
    stats->clock_errors = pll->clock_errors;
    stats->out_of_tolerance = pll->out_of_tolerance;
    stats->min_period_ns = pll->min_period_seen;
    stats->max_period_ns = pll->max_period_seen;
    stats->avg_period_ns = pll->current_period;
    stats->lock_quality = uft_pll_sync_quality(pll);
}

void uft_pll_reset_stats(uft_pll_t *pll) {
    if (!pll) return;
    
    pll->total_transitions = 0;
    pll->good_transitions = 0;
    pll->clock_errors = 0;
    pll->out_of_tolerance = 0;
    pll->min_period_seen = pll->nominal_period * 2;
    pll->max_period_seen = 0;
}

/* ============================================================================
 * Utility
 * ============================================================================ */

double uft_pll_nominal_period(uint32_t data_rate, uft_encoding_t encoding) {
    /* Period in nanoseconds */
    double base_period = 1e9 / data_rate;
    
    switch (encoding) {
        case UFT_ENC_FM:
            /* FM: one clock + one data per bit */
            return base_period;
            
        case UFT_ENC_MFM:
            /* MFM: variable, but base period is the bit cell */
            return base_period;
            
        case UFT_ENC_RLL_27:
            /* RLL 2,7: same as MFM at higher rate */
            return base_period;
            
        default:
            return base_period;
    }
}

const char *uft_pll_encoding_name(uft_encoding_t encoding) {
    switch (encoding) {
        case UFT_ENC_FM:          return "FM";
        case UFT_ENC_MFM:         return "MFM";
        case UFT_ENC_RLL_27:      return "RLL 2,7";
        case UFT_ENC_RLL_17:      return "RLL 1,7";
        case UFT_ENC_RLL_SEAGATE: return "RLL Seagate";
        case UFT_ENC_RLL_WD:      return "RLL Western Digital";
        case UFT_ENC_RLL_OMTI:    return "RLL OMTI";
        case UFT_ENC_RLL_ADAPTEC: return "RLL Adaptec";
        case UFT_ENC_CUSTOM:      return "Custom";
        default:                  return "Unknown";
    }
}
