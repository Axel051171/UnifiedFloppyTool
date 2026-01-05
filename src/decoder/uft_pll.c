/**
 * @file uft_pll.c
 * @brief Kalman PLL Implementation
 * 
 * ROADMAP F2.1 - Priority P0
 */

#include "uft/decoder/uft_pll.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// Internal Structure
// ============================================================================

struct uft_pll_s {
    uft_pll_config_t config;
    
    // Phase tracking
    double phase;           // Phase accumulator (ns)
    double frequency;       // Current frequency (Hz)
    double bit_cell;        // Bit cell duration (ns)
    
    // Kalman filter state
    double state_estimate;  // x̂
    double error_cov;       // P
    double process_noise;   // Q
    double measure_noise;   // R
    
    // Lock detection
    int lock_count;
    bool is_locked;
    
    // Statistics
    uint64_t total_bits;
    uint64_t good_bits;
    double jitter_sum;
    int jitter_count;
    
    // History for adaptive
    double phase_errors[16];
    int error_index;
};

// ============================================================================
// Encoding Parameters
// ============================================================================

typedef struct {
    double frequency;
    double bit_cell_ns;
    const char* name;
} encoding_param_t;

static const encoding_param_t* get_encoding_params(uft_encoding_t encoding) {
    static const encoding_param_t MFM_HD    = { 500000.0, 2000.0, "MFM HD" };
    static const encoding_param_t MFM_DD    = { 250000.0, 4000.0, "MFM DD" };
    static const encoding_param_t MFM_SD    = { 125000.0, 8000.0, "MFM SD" };
    static const encoding_param_t FM_SD     = { 125000.0, 8000.0, "FM SD" };
    static const encoding_param_t GCR_CBM   = { 250000.0, 4000.0, "GCR C64" };
    static const encoding_param_t GCR_APPLE = { 250000.0, 4000.0, "GCR Apple" };
    static const encoding_param_t DEFAULT   = { 250000.0, 4000.0, "Unknown" };
    
    switch (encoding) {
        case UFT_ENC_MFM_HD:
            return &MFM_HD;
        case UFT_ENC_MFM_DD:
        case UFT_ENC_MFM:
            return &MFM_DD;
        case UFT_ENC_MFM_SD:
            return &MFM_SD;
        case UFT_ENC_FM_SD:
        case UFT_ENC_FM:
            return &FM_SD;
        case UFT_ENC_GCR_CBM:
        case UFT_ENC_GCR_CBM_V:
            return &GCR_CBM;
        case UFT_ENC_GCR_APPLE_525:
        case UFT_ENC_GCR_APPLE_35:
            return &GCR_APPLE;
        default:
            return &DEFAULT;
    }
}

// ============================================================================
// Config
// ============================================================================

void uft_pll_config_default(uft_pll_config_t* config, int encoding) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    
    const encoding_param_t* params = get_encoding_params((uft_encoding_t)encoding);
    config->initial_frequency = params->frequency;
    
    config->frequency_tolerance = 0.05;
    config->phase_gain = 0.25;
    config->frequency_gain = 0.05;
    config->jitter_tolerance = 0.20;
    config->adaptive_bandwidth = true;
    config->lock_threshold = 10;
}

// ============================================================================
// Create/Destroy
// ============================================================================

uft_pll_t* uft_pll_create(const uft_pll_config_t* config) {
    uft_pll_t* pll = calloc(1, sizeof(uft_pll_t));
    if (!pll) return NULL;
    
    if (config) {
        pll->config = *config;
    } else {
        uft_pll_config_default(&pll->config, UFT_ENC_MFM_DD);
    }
    
    pll->frequency = pll->config.initial_frequency;
    pll->bit_cell = 1e9 / pll->frequency;
    pll->phase = 0;
    
    // Kalman init
    pll->state_estimate = pll->bit_cell;
    pll->error_cov = pll->bit_cell * 0.1;
    pll->process_noise = pll->bit_cell * 0.01;
    pll->measure_noise = pll->bit_cell * 0.1;
    
    return pll;
}

void uft_pll_destroy(uft_pll_t* pll) {
    free(pll);
}

void uft_pll_reset(uft_pll_t* pll) {
    if (!pll) return;
    
    pll->frequency = pll->config.initial_frequency;
    pll->bit_cell = 1e9 / pll->frequency;
    pll->phase = 0;
    pll->lock_count = 0;
    pll->is_locked = false;
    pll->total_bits = 0;
    pll->good_bits = 0;
    pll->jitter_sum = 0;
    pll->jitter_count = 0;
    pll->error_index = 0;
    
    pll->state_estimate = pll->bit_cell;
    pll->error_cov = pll->bit_cell * 0.1;
}

// ============================================================================
// Kalman Update
// ============================================================================

static void kalman_update(uft_pll_t* pll, double measurement) {
    // Predict
    double predicted_state = pll->state_estimate;
    double predicted_cov = pll->error_cov + pll->process_noise;
    
    // Update
    double kalman_gain = predicted_cov / (predicted_cov + pll->measure_noise);
    pll->state_estimate = predicted_state + kalman_gain * (measurement - predicted_state);
    pll->error_cov = (1.0 - kalman_gain) * predicted_cov;
    
    // Clamp to reasonable range
    double min_cell = pll->bit_cell * (1.0 - pll->config.frequency_tolerance);
    double max_cell = pll->bit_cell * (1.0 + pll->config.frequency_tolerance);
    
    if (pll->state_estimate < min_cell) pll->state_estimate = min_cell;
    if (pll->state_estimate > max_cell) pll->state_estimate = max_cell;
}

// ============================================================================
// Process
// ============================================================================

int uft_pll_process(uft_pll_t* pll, uint32_t flux_time_ns,
                    uint8_t* bits, int max_bits) {
    if (!pll || !bits || max_bits <= 0) return 0;
    
    double flux = (double)flux_time_ns;
    double cell = pll->state_estimate;
    
    // Calculate number of bit cells
    double cells = flux / cell;
    int num_bits = (int)(cells + 0.5);
    
    if (num_bits < 1) num_bits = 1;
    if (num_bits > max_bits) num_bits = max_bits;
    if (num_bits > 4) num_bits = 4;  // MFM max
    
    // Calculate phase error
    double expected = num_bits * cell;
    double error = flux - expected;
    double normalized_error = error / cell;
    
    // Store for adaptive bandwidth
    pll->phase_errors[pll->error_index++ & 15] = fabs(normalized_error);
    
    // Update Kalman
    double measured_cell = flux / num_bits;
    kalman_update(pll, measured_cell);
    
    // Track jitter
    pll->jitter_sum += fabs(normalized_error);
    pll->jitter_count++;
    
    // Lock detection
    if (fabs(normalized_error) < pll->config.jitter_tolerance) {
        pll->lock_count++;
        pll->good_bits++;
        if (pll->lock_count >= pll->config.lock_threshold) {
            pll->is_locked = true;
        }
    } else {
        pll->lock_count = pll->lock_count > 0 ? pll->lock_count - 1 : 0;
        if (pll->lock_count < pll->config.lock_threshold / 2) {
            pll->is_locked = false;
        }
    }
    
    // Adaptive bandwidth
    if (pll->config.adaptive_bandwidth && pll->jitter_count >= 16) {
        double avg_error = 0;
        for (int i = 0; i < 16; i++) {
            avg_error += pll->phase_errors[i];
        }
        avg_error /= 16.0;
        
        if (avg_error > 0.15) {
            pll->measure_noise *= 1.1;
        } else if (avg_error < 0.05) {
            pll->measure_noise *= 0.9;
        }
        
        if (pll->measure_noise < pll->bit_cell * 0.01) {
            pll->measure_noise = pll->bit_cell * 0.01;
        }
        if (pll->measure_noise > pll->bit_cell * 0.5) {
            pll->measure_noise = pll->bit_cell * 0.5;
        }
    }
    
    // Output bits (MFM: first n-1 are 0, last is 1)
    for (int i = 0; i < num_bits - 1 && i < max_bits; i++) {
        bits[i] = 0;
    }
    if (num_bits > 0 && num_bits <= max_bits) {
        bits[num_bits - 1] = 1;
    }
    
    pll->total_bits += num_bits;
    
    return num_bits;
}

int uft_pll_decode(uft_pll_t* pll, const uint32_t* flux,
                   size_t flux_count, uint8_t* bits, size_t max_bits) {
    if (!pll || !flux || !bits) return 0;
    
    size_t bit_pos = 0;
    uint8_t temp_bits[8];
    
    for (size_t i = 0; i < flux_count && bit_pos < max_bits; i++) {
        int n = uft_pll_process(pll, flux[i], temp_bits, 8);
        
        for (int j = 0; j < n && bit_pos < max_bits; j++) {
            bits[bit_pos++] = temp_bits[j];
        }
    }
    
    return (int)bit_pos;
}

// ============================================================================
// State
// ============================================================================

void uft_pll_get_state(const uft_pll_t* pll, uft_pll_state_t* state) {
    if (!pll || !state) return;
    
    memset(state, 0, sizeof(*state));
    
    state->current_frequency = 1e9 / pll->state_estimate;
    state->current_phase = pll->phase;
    state->lock_count = pll->lock_count;
    state->is_locked = pll->is_locked;
    
    state->kalman_gain = pll->error_cov / (pll->error_cov + pll->measure_noise);
    state->error_covariance = pll->error_cov;
    
    state->total_bits = pll->total_bits;
    state->good_bits = pll->good_bits;
    
    if (pll->jitter_count > 0) {
        state->avg_jitter = pll->jitter_sum / pll->jitter_count;
    }
    
    if (pll->total_bits > 0) {
        state->confidence = (double)pll->good_bits / pll->total_bits;
    }
}

bool uft_pll_is_locked(const uft_pll_t* pll) {
    return pll && pll->is_locked;
}

double uft_pll_get_ber(const uft_pll_t* pll) {
    if (!pll || pll->total_bits == 0) return 1.0;
    return 1.0 - (double)pll->good_bits / pll->total_bits;
}

// ============================================================================
// Utility
// ============================================================================

double uft_pll_encoding_frequency(int encoding) {
    const encoding_param_t* params = get_encoding_params((uft_encoding_t)encoding);
    return params->frequency;
}

const char* uft_pll_encoding_name(int encoding) {
    const encoding_param_t* params = get_encoding_params((uft_encoding_t)encoding);
    return params->name;
}
