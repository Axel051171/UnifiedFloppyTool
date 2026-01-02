/**
 * @file uft_kalman_pll_v2.c
 * @brief GOD MODE: Adaptive Kalman-Filter PLL
 * 
 * Replaces naive fixed-window PLL with adaptive timing recovery.
 * 
 * Key improvements:
 * - Dynamic timing window based on measured variance
 * - Drift compensation
 * - Outlier rejection
 * 
 * @author Superman QA - GOD MODE
 * @date 2026
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Constants
// ============================================================================

#define KALMAN_MIN_CELL_NS      1500.0   // 1.5µs minimum (HD)
#define KALMAN_MAX_CELL_NS      5000.0   // 5µs maximum (DD slow)
#define KALMAN_OUTLIER_SIGMA    3.0      // Reject > 3 sigma
#define KALMAN_HISTORY_SIZE     16       // For variance estimation

// ============================================================================
// Types
// ============================================================================

typedef struct {
    // Kalman state
    double cell_time;           // Current bitcell estimate (ns)
    double variance;            // Estimation uncertainty
    
    // Kalman parameters
    double process_noise;       // Q: Expected drift per sample
    double measurement_noise;   // R: Measurement uncertainty
    
    // Running statistics
    double history[KALMAN_HISTORY_SIZE];
    int history_idx;
    int history_count;
    double running_mean;
    double running_var;
    
    // Diagnostics
    uint32_t total_samples;
    uint32_t outliers_rejected;
    double max_deviation;
    
} kalman_pll_state_t;

typedef struct {
    double initial_cell_time;
    double initial_variance;
    double process_noise_factor;    // Multiplied by cell_time
    double measurement_noise_factor;
} kalman_pll_config_t;

// ============================================================================
// Default Configurations
// ============================================================================

static const kalman_pll_config_t KALMAN_CONFIG_MFM_DD = {
    .initial_cell_time = 2000.0,        // 2µs for DD MFM
    .initial_variance = 200.0,          // 10% initial uncertainty
    .process_noise_factor = 0.0001,     // 0.01% drift per sample
    .measurement_noise_factor = 0.05    // 5% measurement noise
};

static const kalman_pll_config_t KALMAN_CONFIG_GCR_C64 = {
    .initial_cell_time = 3692.0,        // Zone 0 timing
    .initial_variance = 400.0,
    .process_noise_factor = 0.0002,
    .measurement_noise_factor = 0.08
};

// ============================================================================
// Initialization
// ============================================================================

void kalman_pll_init(kalman_pll_state_t* state, const kalman_pll_config_t* config) {
    memset(state, 0, sizeof(*state));
    
    state->cell_time = config->initial_cell_time;
    state->variance = config->initial_variance;
    state->process_noise = config->initial_cell_time * config->process_noise_factor;
    state->measurement_noise = config->initial_cell_time * config->measurement_noise_factor;
    
    state->running_mean = config->initial_cell_time;
}

void kalman_pll_init_mfm_dd(kalman_pll_state_t* state) {
    kalman_pll_init(state, &KALMAN_CONFIG_MFM_DD);
}

void kalman_pll_init_gcr_c64(kalman_pll_state_t* state, int zone) {
    kalman_pll_config_t config = KALMAN_CONFIG_GCR_C64;
    
    // Zone-specific timing
    static const double zone_timings[] = {3692, 3768, 3846, 4000};
    if (zone >= 0 && zone < 4) {
        config.initial_cell_time = zone_timings[zone];
    }
    
    kalman_pll_init(state, &config);
}

// ============================================================================
// Statistics Helpers
// ============================================================================

static void update_running_stats(kalman_pll_state_t* state, double value) {
    // Add to circular buffer
    state->history[state->history_idx] = value;
    state->history_idx = (state->history_idx + 1) % KALMAN_HISTORY_SIZE;
    if (state->history_count < KALMAN_HISTORY_SIZE) {
        state->history_count++;
    }
    
    // Calculate mean and variance
    double sum = 0, sum_sq = 0;
    for (int i = 0; i < state->history_count; i++) {
        sum += state->history[i];
        sum_sq += state->history[i] * state->history[i];
    }
    
    state->running_mean = sum / state->history_count;
    if (state->history_count > 1) {
        state->running_var = (sum_sq - sum * sum / state->history_count) / 
                             (state->history_count - 1);
    }
}

static bool is_outlier(kalman_pll_state_t* state, double value) {
    if (state->history_count < 4) return false;  // Not enough data
    
    double sigma = sqrt(state->running_var);
    if (sigma < 1.0) sigma = 1.0;  // Avoid division issues
    
    double z_score = fabs(value - state->running_mean) / sigma;
    return z_score > KALMAN_OUTLIER_SIGMA;
}

// ============================================================================
// Core Update Function
// ============================================================================

/**
 * @brief Update PLL with new flux measurement
 * 
 * @param state PLL state
 * @param flux_time_ns Measured time between flux transitions (nanoseconds)
 * @param bit_count Expected number of bits (1-5 typical)
 * @return Decoded bit pattern, or -1 on error
 */
int kalman_pll_update(kalman_pll_state_t* state, double flux_time_ns, int* bit_count_out) {
    state->total_samples++;
    
    // === PREDICT ===
    // State prediction (cell_time stays same, variance increases)
    double predicted_var = state->variance + state->process_noise;
    
    // === DETERMINE BIT COUNT ===
    // How many bitcells fit in this flux time?
    double ratio = flux_time_ns / state->cell_time;
    int bit_count = (int)round(ratio);
    
    // Clamp to valid range
    if (bit_count < 1) bit_count = 1;
    if (bit_count > 5) bit_count = 5;  // MFM max is typically 4, GCR up to 5
    
    // Expected time for this bit count
    double expected_time = state->cell_time * bit_count;
    
    // === OUTLIER CHECK ===
    double per_bit_time = flux_time_ns / bit_count;
    if (is_outlier(state, per_bit_time)) {
        state->outliers_rejected++;
        // Still return decoded bits but don't update PLL
        if (bit_count_out) *bit_count_out = bit_count;
        return bit_count;
    }
    
    // === MEASUREMENT UPDATE ===
    double residual = flux_time_ns - expected_time;
    
    // Kalman gain
    double S = predicted_var + state->measurement_noise;
    double K = predicted_var / S;
    
    // State update
    double correction = K * (residual / bit_count);
    state->cell_time += correction;
    state->variance = (1.0 - K) * predicted_var;
    
    // Track max deviation for diagnostics
    double deviation = fabs(correction);
    if (deviation > state->max_deviation) {
        state->max_deviation = deviation;
    }
    
    // === BOUNDS ENFORCEMENT ===
    if (state->cell_time < KALMAN_MIN_CELL_NS) {
        state->cell_time = KALMAN_MIN_CELL_NS;
    }
    if (state->cell_time > KALMAN_MAX_CELL_NS) {
        state->cell_time = KALMAN_MAX_CELL_NS;
    }
    
    // Variance floor (prevent over-convergence)
    if (state->variance < state->process_noise) {
        state->variance = state->process_noise;
    }
    
    // === UPDATE STATISTICS ===
    update_running_stats(state, per_bit_time);
    
    if (bit_count_out) *bit_count_out = bit_count;
    return bit_count;
}

// ============================================================================
// Batch Processing
// ============================================================================

/**
 * @brief Decode entire flux stream to bitstream
 * 
 * @param flux_times Array of flux transition times (ns)
 * @param num_flux Number of flux values
 * @param output Output bitstream (caller allocates)
 * @param max_bits Maximum bits to output
 * @param config PLL configuration (NULL for MFM defaults)
 * @return Number of bits decoded
 */
size_t kalman_pll_decode_flux(const uint32_t* flux_times, size_t num_flux,
                              uint8_t* output, size_t max_bits,
                              const kalman_pll_config_t* config) {
    if (!flux_times || !output || num_flux == 0) return 0;
    
    kalman_pll_state_t state;
    if (config) {
        kalman_pll_init(&state, config);
    } else {
        kalman_pll_init_mfm_dd(&state);
    }
    
    size_t bit_idx = 0;
    memset(output, 0, (max_bits + 7) / 8);
    
    for (size_t i = 0; i < num_flux && bit_idx < max_bits; i++) {
        int bit_count;
        kalman_pll_update(&state, (double)flux_times[i], &bit_count);
        
        // Output bits: 0s followed by 1
        // MFM: bit_count-1 zeros, then 1
        for (int b = 0; b < bit_count - 1 && bit_idx < max_bits; b++) {
            // output[bit_idx/8] already 0
            bit_idx++;
        }
        
        if (bit_idx < max_bits) {
            output[bit_idx / 8] |= (1 << (7 - (bit_idx % 8)));
            bit_idx++;
        }
    }
    
    return bit_idx;
}

// ============================================================================
// Diagnostics
// ============================================================================

typedef struct {
    double final_cell_time;
    double final_variance;
    double drift_from_initial;
    uint32_t total_samples;
    uint32_t outliers_rejected;
    double outlier_rate;
    double max_deviation;
    double timing_confidence;   // 0-100
} kalman_pll_stats_t;

void kalman_pll_get_stats(const kalman_pll_state_t* state, 
                          double initial_cell_time,
                          kalman_pll_stats_t* stats) {
    stats->final_cell_time = state->cell_time;
    stats->final_variance = state->variance;
    stats->drift_from_initial = (state->cell_time - initial_cell_time) / initial_cell_time * 100;
    stats->total_samples = state->total_samples;
    stats->outliers_rejected = state->outliers_rejected;
    stats->outlier_rate = (state->total_samples > 0) ?
                          (double)state->outliers_rejected / state->total_samples * 100 : 0;
    stats->max_deviation = state->max_deviation;
    
    // Confidence based on variance and outlier rate
    double var_factor = 100.0 - sqrt(state->variance) / state->cell_time * 100;
    double outlier_factor = 100.0 - stats->outlier_rate * 10;
    
    if (var_factor < 0) var_factor = 0;
    if (outlier_factor < 0) outlier_factor = 0;
    
    stats->timing_confidence = (var_factor * 0.6 + outlier_factor * 0.4);
}
