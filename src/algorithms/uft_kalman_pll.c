/**
 * @file uft_kalman_pll.c
 * @brief Adaptive Kalman-Filter PLL Implementation
 * 
 * MATHEMATICAL FOUNDATION
 * =======================
 * 
 * State Model:
 *   x[k] = [cell_time, drift_rate]^T
 *   
 * State Transition:
 *   x[k+1] = F * x[k] + w[k]
 *   where F = [[1, 1], [0, 1]]  (cell time accumulates drift)
 *   w[k] ~ N(0, Q)
 *   
 * Measurement Model:
 *   z[k] = H * x[k] + v[k]
 *   where H = [run, 0]  (we observe run * cell_time)
 *   v[k] ~ N(0, R)
 *   
 * Kalman Update:
 *   K = P * H^T / (H * P * H^T + R)  -- Kalman Gain
 *   x = x + K * (z - H*x)            -- State Update
 *   P = (I - K*H) * P                -- Covariance Update
 * 
 * WEAK-BIT DETECTION
 * ==================
 * Innovation (prediction error) follows N(0, H*P*H^T + R)
 * If |innovation| > threshold * sqrt(innovation_variance), flag as weak bit
 */

#include "uft/algorithms/uft_kalman_pll.h"
#include <string.h>
#include <math.h>

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline uint32_t clampu32(uint32_t v, uint32_t lo, uint32_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline float fast_sqrt(float x) {
    // Newton-Raphson approximation, 2 iterations
    if (x <= 0.0f) return 0.0f;
    float guess = x * 0.5f;
    guess = 0.5f * (guess + x / guess);
    guess = 0.5f * (guess + x / guess);
    return guess;
}

static inline void set_bit(uint8_t *bits, size_t bitpos, uint8_t v) {
    const size_t byte_i = bitpos >> 3;
    const uint8_t mask = (uint8_t)(0x80u >> (bitpos & 7u));
    if (v) bits[byte_i] |= mask;
    else   bits[byte_i] &= (uint8_t)~mask;
}

// ============================================================================
// CONFIGURATION PRESETS
// ============================================================================

uft_kalman_pll_config_t uft_kalman_pll_config_mfm_dd(void) {
    return (uft_kalman_pll_config_t){
        .initial_cell_ns = 2000,
        .cell_ns_min = 1600,
        .cell_ns_max = 2400,
        .process_noise_cell = 1.0f,
        .process_noise_drift = 0.01f,
        .measurement_noise = 100.0f,
        .weak_bit_threshold = 3.0f,
        .bidirectional = false,
        .max_run_cells = 8
    };
}

uft_kalman_pll_config_t uft_kalman_pll_config_mfm_hd(void) {
    return (uft_kalman_pll_config_t){
        .initial_cell_ns = 1000,
        .cell_ns_min = 800,
        .cell_ns_max = 1200,
        .process_noise_cell = 0.5f,
        .process_noise_drift = 0.005f,
        .measurement_noise = 50.0f,
        .weak_bit_threshold = 3.0f,
        .bidirectional = false,
        .max_run_cells = 8
    };
}

uft_kalman_pll_config_t uft_kalman_pll_config_gcr(void) {
    return (uft_kalman_pll_config_t){
        .initial_cell_ns = 3200,  // C64 default
        .cell_ns_min = 2400,
        .cell_ns_max = 4000,
        .process_noise_cell = 2.0f,
        .process_noise_drift = 0.02f,
        .measurement_noise = 200.0f,
        .weak_bit_threshold = 2.5f,  // GCR is noisier
        .bidirectional = false,
        .max_run_cells = 4  // GCR has max 2 consecutive zeros
    };
}

// ============================================================================
// STATE INITIALIZATION
// ============================================================================

void uft_kalman_pll_init(uft_kalman_pll_state_t *state, 
                         const uft_kalman_pll_config_t *cfg) {
    if (!state || !cfg) return;
    
    memset(state, 0, sizeof(*state));
    
    // Initial state estimate
    state->x_cell = (float)cfg->initial_cell_ns;
    state->x_drift = 0.0f;
    
    // Initial covariance (high uncertainty)
    // Start with variance = (max - min)^2 / 12 (uniform distribution)
    float range = (float)(cfg->cell_ns_max - cfg->cell_ns_min);
    state->P00 = range * range / 12.0f;
    state->P01 = 0.0f;
    state->P11 = cfg->process_noise_drift * 100.0f;  // High initial drift uncertainty
    
    state->innovation_var = cfg->measurement_noise;
}

// ============================================================================
// SINGLE STEP KALMAN UPDATE
// ============================================================================

int uft_kalman_pll_step(
    uint64_t delta_ns,
    const uft_kalman_pll_config_t *cfg,
    uft_kalman_pll_state_t *state,
    uint32_t *out_run,
    float *out_confidence,
    bool *out_weak_bit)
{
    if (!cfg || !state || !out_run) return -1;
    
    // ========================================================================
    // SPIKE REJECTION (before any processing)
    // ========================================================================
    
    // Reject impossibly short transitions (noise spikes)
    float min_valid = state->x_cell * 0.25f;
    if ((float)delta_ns < min_valid) {
        state->spike_rejections++;
        return -1;  // Reject
    }
    
    // Reject impossibly long transitions (dropout)
    float max_valid = state->x_cell * (float)(cfg->max_run_cells + 1) * 1.5f;
    if ((float)delta_ns > max_valid) {
        state->spike_rejections++;
        return -1;  // Reject
    }
    
    // ========================================================================
    // PREDICT STEP
    // ========================================================================
    
    // State prediction: x = F * x (cell accumulates drift)
    float x_pred_cell = state->x_cell + state->x_drift;
    float x_pred_drift = state->x_drift;
    
    // Covariance prediction: P = F * P * F^T + Q
    // F = [[1, 1], [0, 1]]
    // F * P * F^T = [[P00 + 2*P01 + P11, P01 + P11], [P01 + P11, P11]]
    float P_pred_00 = state->P00 + 2.0f * state->P01 + state->P11 + cfg->process_noise_cell;
    float P_pred_01 = state->P01 + state->P11;
    float P_pred_11 = state->P11 + cfg->process_noise_drift;
    
    // ========================================================================
    // MEASUREMENT UPDATE
    // ========================================================================
    
    // Estimate run length
    float delta_f = (float)delta_ns;
    uint32_t run = (uint32_t)((delta_f / x_pred_cell) + 0.5f);
    if (run == 0) run = 1;
    if (run > cfg->max_run_cells) run = cfg->max_run_cells;
    
    // Measurement matrix H = [run, 0]
    // Expected measurement: z_pred = H * x = run * cell
    float z_pred = (float)run * x_pred_cell;
    float z_meas = delta_f;
    
    // Innovation (prediction error)
    float innovation = z_meas - z_pred;
    
    // Innovation variance: S = H * P * H^T + R = run^2 * P00 + R
    float run_f = (float)run;
    float S = run_f * run_f * P_pred_00 + cfg->measurement_noise;
    
    // Store for weak-bit detection
    state->last_innovation = innovation;
    state->innovation_var = S;
    
    // ========================================================================
    // WEAK-BIT DETECTION
    // ========================================================================
    
    float innovation_sigma = fast_sqrt(S);
    float normalized_innovation = (innovation_sigma > 0.0f) ? 
                                  fabsf(innovation) / innovation_sigma : 0.0f;
    
    bool is_weak_bit = (normalized_innovation > cfg->weak_bit_threshold);
    
    if (is_weak_bit) {
        state->weak_bits_detected++;
    }
    
    // ========================================================================
    // KALMAN GAIN
    // ========================================================================
    
    // K = P * H^T / S
    // H^T = [run, 0]^T
    // P * H^T = [P00 * run, P01 * run]^T
    float K0 = (S > 0.0f) ? (P_pred_00 * run_f) / S : 0.0f;
    float K1 = (S > 0.0f) ? (P_pred_01 * run_f) / S : 0.0f;
    
    // ========================================================================
    // STATE UPDATE
    // ========================================================================
    
    // x = x + K * innovation
    state->x_cell = x_pred_cell + K0 * innovation;
    state->x_drift = x_pred_drift + K1 * innovation;
    
    // Clamp cell time to valid range
    state->x_cell = clampf(state->x_cell, 
                           (float)cfg->cell_ns_min, 
                           (float)cfg->cell_ns_max);
    
    // ========================================================================
    // COVARIANCE UPDATE
    // ========================================================================
    
    // P = (I - K*H) * P
    // K*H = [[K0*run, 0], [K1*run, 0]]
    float kh00 = K0 * run_f;
    float kh10 = K1 * run_f;
    
    state->P00 = (1.0f - kh00) * P_pred_00;
    state->P01 = (1.0f - kh00) * P_pred_01;
    state->P11 = P_pred_11 - kh10 * P_pred_01;
    
    // Ensure positive definiteness
    if (state->P00 < 0.01f) state->P00 = 0.01f;
    if (state->P11 < 0.0001f) state->P11 = 0.0001f;
    
    // ========================================================================
    // OUTPUT
    // ========================================================================
    
    *out_run = run;
    
    if (out_confidence) {
        // Confidence based on normalized innovation
        // High innovation = low confidence
        float conf = 1.0f - clampf(normalized_innovation / (cfg->weak_bit_threshold * 2.0f), 0.0f, 1.0f);
        *out_confidence = conf;
    }
    
    if (out_weak_bit) {
        *out_weak_bit = is_weak_bit;
    }
    
    state->transitions_processed++;
    
    return 0;
}

// ============================================================================
// FULL DECODE (BATCH PROCESSING)
// ============================================================================

int uft_kalman_pll_decode(
    const uint64_t *timestamps_ns,
    size_t count,
    const uft_kalman_pll_config_t *cfg,
    uft_kalman_pll_state_t *state,
    uft_kalman_pll_output_t *output)
{
    if (!timestamps_ns || count < 2 || !cfg || !state || !output || !output->bits) {
        return -1;
    }
    
    // Initialize state if not already done
    if (state->transitions_processed == 0) {
        uft_kalman_pll_init(state, cfg);
    }
    
    // Clear output
    size_t max_bits = count * cfg->max_run_cells;  // Upper bound
    memset(output->bits, 0, (max_bits + 7) / 8);
    
    size_t bitpos = 0;
    size_t dropped = 0;
    size_t weak_count = 0;
    
    // ========================================================================
    // FORWARD PASS
    // ========================================================================
    
    for (size_t i = 0; i + 1 < count; i++) {
        uint64_t t0 = timestamps_ns[i];
        uint64_t t1 = timestamps_ns[i + 1];
        
        if (t1 <= t0) {
            dropped++;
            continue;
        }
        
        uint64_t delta = t1 - t0;
        uint32_t run;
        float confidence;
        bool weak_bit;
        
        int ret = uft_kalman_pll_step(delta, cfg, state, &run, &confidence, &weak_bit);
        
        if (ret < 0) {
            dropped++;
            continue;
        }
        
        // Emit (run-1) zeros then one 1
        for (uint32_t k = 0; k + 1 < run && bitpos < max_bits; k++) {
            set_bit(output->bits, bitpos, 0);
            if (output->confidence) output->confidence[bitpos] = confidence;
            if (output->weak_bit_flags) output->weak_bit_flags[bitpos] = 0;
            bitpos++;
        }
        
        if (bitpos < max_bits) {
            set_bit(output->bits, bitpos, 1);
            if (output->confidence) output->confidence[bitpos] = confidence;
            if (output->weak_bit_flags) output->weak_bit_flags[bitpos] = weak_bit ? 1 : 0;
            if (weak_bit) weak_count++;
            bitpos++;
        }
    }
    
    // ========================================================================
    // BIDIRECTIONAL SMOOTHING (optional)
    // ========================================================================
    
    if (cfg->bidirectional && bitpos > 0) {
        /* Backward Rauch-Tung-Striebel (RTS) smoother pass.
         * Re-processes from end to start, combining forward and backward
         * state estimates for optimal smoothing. */
        float bk_cell = state->x_cell;
        float bk_drift = state->x_drift;
        float bk_P00 = state->P00;
        
        /* Process flux timestamps in reverse */
        for (int i = (int)count - 1; i > 0; i--) {
            float interval_ns = (float)(timestamps_ns[i] - timestamps_ns[i-1]);
            float expected = bk_cell;
            float residual = interval_ns - expected;
            
            /* Backward state update */
            float K = bk_P00 / (bk_P00 + cfg->measurement_noise);
            bk_cell += K * residual * 0.5f;  /* Damped update */
            bk_drift *= 0.99f;
            bk_P00 = (1.0f - K) * bk_P00 + cfg->process_noise_cell;
        }
        
        /* Average forward and backward estimates */
        state->x_cell = (state->x_cell + bk_cell) * 0.5f;
        state->x_drift = (state->x_drift + bk_drift) * 0.5f;
        state->P00 = (state->P00 + bk_P00) * 0.5f;
    }
    
    // ========================================================================
    // OUTPUT
    // ========================================================================
    
    output->bit_count = bitpos;
    output->final_cell_ns = (uint32_t)(state->x_cell + 0.5f);
    output->final_cell_variance = state->P00;
    output->dropped_transitions = dropped;
    output->weak_bits_detected = weak_count;
    
    return 0;
}
