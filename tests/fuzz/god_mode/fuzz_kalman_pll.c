/**
 * @file fuzz_kalman_pll.c
 * @brief Fuzz target for Kalman PLL
 * 
 * Ensures PLL doesn't crash or produce NaN/Inf on any input.
 */

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <stdbool.h>

// Inline PLL for fuzzing (no external deps)
typedef struct {
    double cell_time;
    double variance;
    double process_noise;
    double measurement_noise;
} kalman_state_t;

void kalman_init(kalman_state_t* s) {
    s->cell_time = 2000.0;
    s->variance = 200.0;
    s->process_noise = 0.2;
    s->measurement_noise = 100.0;
}

int kalman_update(kalman_state_t* s, double flux_ns, int* bits) {
    if (flux_ns <= 0) flux_ns = 1;
    
    double ratio = flux_ns / s->cell_time;
    int bit_count = (int)(ratio + 0.5);
    if (bit_count < 1) bit_count = 1;
    if (bit_count > 5) bit_count = 5;
    
    double predicted_var = s->variance + s->process_noise;
    double expected = s->cell_time * bit_count;
    double residual = flux_ns - expected;
    
    double S = predicted_var + s->measurement_noise;
    if (S < 0.001) S = 0.001;
    
    double K = predicted_var / S;
    if (K < 0) K = 0;
    if (K > 1) K = 1;
    
    s->cell_time += K * (residual / bit_count);
    s->variance = (1.0 - K) * predicted_var;
    
    // Bounds
    if (s->cell_time < 500) s->cell_time = 500;
    if (s->cell_time > 10000) s->cell_time = 10000;
    if (s->variance < 0.1) s->variance = 0.1;
    if (s->variance > 1e9) s->variance = 1e9;
    
    *bits = bit_count;
    return bit_count;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 4) return 0;
    
    kalman_state_t pll;
    kalman_init(&pll);
    
    size_t i = 0;
    while (i + 2 <= size) {
        uint16_t flux_raw = (data[i] << 8) | data[i+1];
        double flux_ns = flux_raw * 20.0;  // 0 to ~1.3ms
        
        int bits;
        kalman_update(&pll, flux_ns, &bits);
        
        // Invariant checks
        if (isnan(pll.cell_time) || isinf(pll.cell_time)) {
            __builtin_trap();
        }
        if (isnan(pll.variance) || isinf(pll.variance)) {
            __builtin_trap();
        }
        if (pll.cell_time < 500 || pll.cell_time > 10000) {
            __builtin_trap();
        }
        if (pll.variance < 0) {
            __builtin_trap();
        }
        
        i += 2;
    }
    
    return 0;
}
