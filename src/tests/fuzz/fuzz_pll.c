/**
 * @file fuzz_pll.c
 * @brief Fuzz target for PLL robustness testing
 * 
 * Build with: clang -g -fsanitize=fuzzer,address fuzz_pll.c \
 *             ../algorithms/pll/uft_adaptive_pll.c -lm -o fuzz_pll
 */

#include "../algorithms/pll/uft_adaptive_pll.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 16) return 0;
    
    /* Initialize PLL */
    uft_adaptive_pll_t pll;
    uft_pll_init(&pll);
    
    /* Configure with fuzzer-derived parameters */
    double sample_rate = 4e6;
    double bit_rate = 500e3;
    
    /* Extract some config from fuzz data */
    if (data[0] > 200) bit_rate = 250e3;  /* DD */
    if (data[0] > 240) bit_rate = 1e6;    /* HD */
    
    uft_pll_configure(&pll, sample_rate, bit_rate);
    
    /* Optionally change mode based on fuzz input */
    if (data[1] & 0x80) {
        uft_pll_set_mode(&pll, UFT_PLL_MODE_SYNC);
    } else if (data[1] & 0x40) {
        uft_pll_set_mode(&pll, UFT_PLL_MODE_DATA);
    }
    
    /* Process fuzz data as pulse positions */
    for (size_t i = 2; i < size; i++) {
        /* Convert byte to pulse position (0.5 to 2x cell size) */
        double pulse_pos = (double)data[i] / 255.0 * pll.cell_size * 2.0;
        if (pulse_pos < pll.cell_size * 0.5) {
            pulse_pos = pll.cell_size * 0.5;
        }
        
        uint8_t bit, confidence;
        int num_bits = uft_pll_process_pulse(&pll, pulse_pos, &bit, &confidence);
        
        /* Invariant checks */
        if (pll.cell_size <= 0) {
            abort();  /* Bug: cell size went non-positive */
        }
        if (pll.cell_size > pll.cell_ref * 3) {
            abort();  /* Bug: cell size way too large */
        }
        if (pll.cell_size < pll.cell_ref * 0.3) {
            abort();  /* Bug: cell size way too small */
        }
        if (num_bits < 0 || num_bits > 100) {
            abort();  /* Bug: impossible bit count */
        }
        if (bit > 1) {
            abort();  /* Bug: bit value not 0 or 1 */
        }
        
        /* Check gain stays in bounds */
        if (pll.gain_current < 0 || pll.gain_current > 2.0) {
            abort();  /* Bug: gain out of range */
        }
    }
    
    return 0;
}
