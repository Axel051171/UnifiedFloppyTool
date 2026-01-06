/*
 * test_vfo_pll.c
 *
 * Unit tests for uft_vfo_pll module.
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -I../libflux_core/include \
 *      test_vfo_pll.c ../libflux_core/src/uft_vfo_pll.c \
 *      -o test_vfo_pll -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "uft_vfo_pll.h"

/* Test counters */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        tests_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s (FAILED at line %d)\n", msg, __LINE__); \
    } \
} while(0)

#define APPROX_EQ(a, b, tol) (fabs((a) - (b)) < (tol))

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO INITIALIZATION TESTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_vfo_init(void)
{
    printf("\n=== VFO Initialization ===\n");
    
    uft_vfo_state_t state;
    
    /* Test MFM init at 4 MHz */
    uft_vfo_init(&state, UFT_VFO_PID, UFT_ENC_MFM, 4000000.0);
    
    TEST_ASSERT(state.type == UFT_VFO_PID, "VFO type is PID");
    TEST_ASSERT(state.encoding == UFT_ENC_MFM, "Encoding is MFM");
    TEST_ASSERT(state.sample_rate == 4000000.0, "Sample rate is 4 MHz");
    
    /* MFM bit cell at 4 MHz: 2µs * 4MHz = 8 samples */
    TEST_ASSERT(APPROX_EQ(state.bit_cell_nom, 8.0, 0.1), "MFM bit cell is ~8 samples");
    
    /* Test FM init */
    uft_vfo_init(&state, UFT_VFO_SIMPLE, UFT_ENC_FM, 4000000.0);
    
    /* FM bit cell at 4 MHz: 4µs * 4MHz = 16 samples */
    TEST_ASSERT(APPROX_EQ(state.bit_cell_nom, 16.0, 0.1), "FM bit cell is ~16 samples");
    
    /* Test GCR init */
    uft_vfo_init(&state, UFT_VFO_DPLL, UFT_ENC_GCR, 4000000.0);
    
    /* GCR bit cell at 4 MHz: 3.2µs * 4MHz = 12.8 samples */
    TEST_ASSERT(APPROX_EQ(state.bit_cell_nom, 12.8, 0.5), "GCR bit cell is ~12.8 samples");
    
    /* Test custom init */
    uft_vfo_init_custom(&state, UFT_VFO_ADAPTIVE, 8000000.0, 1000.0);
    
    /* 1µs bit cell at 8 MHz = 8 samples */
    TEST_ASSERT(APPROX_EQ(state.bit_cell_nom, 8.0, 0.1), "Custom bit cell correct");
    TEST_ASSERT(state.sample_rate == 8000000.0, "Custom sample rate correct");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO CONFIGURATION TESTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_vfo_config(void)
{
    printf("\n=== VFO Configuration ===\n");
    
    uft_vfo_state_t state;
    uft_vfo_init(&state, UFT_VFO_PID, UFT_ENC_MFM, 4000000.0);
    
    /* Test gain setting */
    uft_vfo_set_gain(&state, 0.05, 0.8);
    TEST_ASSERT(APPROX_EQ(state.gain_low, 0.05, 0.001), "Gain low set correctly");
    TEST_ASSERT(APPROX_EQ(state.gain_high, 0.8, 0.001), "Gain high set correctly");
    
    /* Test clamping */
    uft_vfo_set_gain(&state, -0.5, 2.0);
    TEST_ASSERT(state.gain_low >= 0.01, "Gain low clamped to minimum");
    TEST_ASSERT(state.gain_high <= 1.0, "Gain high clamped to maximum");
    
    /* Test PID parameters */
    uft_vfo_set_pid(&state, 0.5, 0.1, 0.2);
    TEST_ASSERT(APPROX_EQ(state.pid_p, 0.5, 0.001), "PID P set correctly");
    TEST_ASSERT(APPROX_EQ(state.pid_i, 0.1, 0.001), "PID I set correctly");
    TEST_ASSERT(APPROX_EQ(state.pid_d, 0.2, 0.001), "PID D set correctly");
    
    /* Test window setting */
    uft_vfo_set_window(&state, 0.3, 0.7);
    TEST_ASSERT(APPROX_EQ(state.window_start, 0.3, 0.001), "Window start set");
    TEST_ASSERT(APPROX_EQ(state.window_end, 0.7, 0.001), "Window end set");
    
    /* Test fluctuator */
    uft_vfo_enable_fluctuator(&state, true, 0.05);
    TEST_ASSERT(state.fluctuator_en == true, "Fluctuator enabled");
    TEST_ASSERT(APPROX_EQ(state.fluctuator_amt, 0.05, 0.001), "Fluctuation amount set");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO PROCESSING TESTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_vfo_processing(void)
{
    printf("\n=== VFO Processing ===\n");
    
    uft_vfo_state_t state;
    uint8_t output[256];
    
    /* Initialize VFO */
    uft_vfo_init(&state, UFT_VFO_SIMPLE, UFT_ENC_MFM, 4000000.0);
    uft_vfo_set_output(&state, output, sizeof(output));
    
    /* Process perfect MFM stream (bit cell = 8 samples) */
    /* 1 bit cell interval -> 1 bit */
    int bits1 = uft_vfo_process_pulse(&state, 8.0);
    TEST_ASSERT(bits1 == 1, "1 bit cell interval -> 1 bit");
    
    /* 2 bit cell interval -> 2 bits (0 then 1) */
    int bits2 = uft_vfo_process_pulse(&state, 16.0);
    TEST_ASSERT(bits2 == 2, "2 bit cell interval -> 2 bits");
    
    /* 3 bit cell interval -> 3 bits */
    int bits3 = uft_vfo_process_pulse(&state, 24.0);
    TEST_ASSERT(bits3 == 3, "3 bit cell interval -> 3 bits");
    
    /* Test statistics */
    uft_vfo_stats_t stats;
    uft_vfo_get_stats(&state, &stats);
    
    TEST_ASSERT(stats.pulses_total == 3, "3 pulses processed");
    TEST_ASSERT(stats.bits_decoded == 6, "6 bits decoded");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO ALGORITHM COMPARISON TESTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_vfo_algorithms(void)
{
    printf("\n=== VFO Algorithm Comparison ===\n");
    
    /* Generate test flux data with slight timing variation */
    double intervals[100];
    double base_cell = 8.0;  /* MFM at 4 MHz */
    
    for (int i = 0; i < 100; i++) {
        /* Simulate 1-cell and 2-cell intervals with ±5% jitter */
        double jitter = (i % 3 == 0) ? 0.95 : ((i % 3 == 1) ? 1.05 : 1.0);
        intervals[i] = base_cell * ((i % 2) + 1) * jitter;
    }
    
    uft_vfo_type_t types[] = {
        UFT_VFO_SIMPLE, UFT_VFO_FIXED, UFT_VFO_PID,
        UFT_VFO_PID2, UFT_VFO_PID3, UFT_VFO_ADAPTIVE, UFT_VFO_DPLL
    };
    const char *names[] = {
        "SIMPLE", "FIXED", "PID", "PID2", "PID3", "ADAPTIVE", "DPLL"
    };
    
    printf("    Testing all VFO algorithms with jittery data:\n");
    
    for (int t = 0; t < 7; t++) {
        uft_vfo_state_t state;
        uint8_t output[256];
        
        uft_vfo_init(&state, types[t], UFT_ENC_MFM, 4000000.0);
        uft_vfo_set_output(&state, output, sizeof(output));
        
        size_t bits = uft_vfo_process_intervals(&state, intervals, 100);
        
        uft_vfo_stats_t stats;
        uft_vfo_get_stats(&state, &stats);
        
        printf("    %s: %zu bits, %.1f%% valid\n", 
               names[t], bits, stats.valid_percent);
        
        /* All algorithms should decode reasonable number of bits */
        TEST_ASSERT(bits >= 100, names[t]);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VFO SYNC TESTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_vfo_sync(void)
{
    printf("\n=== VFO Sync Detection ===\n");
    
    uft_vfo_state_t state;
    uint8_t output[256];
    
    uft_vfo_init(&state, UFT_VFO_PID, UFT_ENC_MFM, 4000000.0);
    uft_vfo_set_output(&state, output, sizeof(output));
    
    /* Initially not synced */
    TEST_ASSERT(!uft_vfo_is_synced(&state), "Initially not synced");
    
    /* Process regular pulses to achieve sync */
    for (int i = 0; i < 20; i++) {
        uft_vfo_process_pulse(&state, 8.0);  /* Perfect bit cell */
    }
    
    /* Should be synced after consistent pulses */
    TEST_ASSERT(uft_vfo_is_synced(&state), "Synced after regular pulses");
    
    /* Force sync */
    uft_vfo_reset(&state);
    TEST_ASSERT(!uft_vfo_is_synced(&state), "Not synced after reset");
    
    uft_vfo_force_sync(&state);
    TEST_ASSERT(uft_vfo_is_synced(&state), "Synced after force_sync");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA SEPARATOR TESTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_data_separator(void)
{
    printf("\n=== Data Separator ===\n");
    
    uft_data_separator_t sep;
    uint8_t output[256];
    
    uft_datasep_init(&sep, UFT_VFO_PID, UFT_ENC_MFM, 4000000.0);
    uft_datasep_set_output(&sep, output, sizeof(output));
    uft_datasep_set_sync(&sep, 0x4489, 0xFFFF);
    
    TEST_ASSERT(!uft_datasep_sync_found(&sep), "Sync not found initially");
    
    /* Generate flux data with sync pattern */
    /* MFM sync 0x4489 = 0100 0100 1000 1001 (with clock bits) */
    /* This is a simplified test - real MFM would be more complex */
    
    uft_datasep_reset(&sep);
    TEST_ASSERT(!uft_datasep_sync_found(&sep), "Sync cleared after reset");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTION TESTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void test_vfo_utilities(void)
{
    printf("\n=== VFO Utilities ===\n");
    
    /* Test type names */
    TEST_ASSERT(strcmp(uft_vfo_type_name(UFT_VFO_SIMPLE), "SIMPLE") == 0, "SIMPLE name");
    TEST_ASSERT(strcmp(uft_vfo_type_name(UFT_VFO_PID), "PID") == 0, "PID name");
    TEST_ASSERT(strcmp(uft_vfo_type_name(UFT_VFO_DPLL), "DPLL") == 0, "DPLL name");
    
    /* Test encoding names */
    TEST_ASSERT(strcmp(uft_encoding_name(UFT_ENC_MFM), "MFM") == 0, "MFM name");
    TEST_ASSERT(strcmp(uft_encoding_name(UFT_ENC_FM), "FM") == 0, "FM name");
    TEST_ASSERT(strcmp(uft_encoding_name(UFT_ENC_GCR), "GCR") == 0, "GCR name");
    
    /* Test bit cell calculation */
    /* 500 kbps at 4 MHz = 8 samples per bit */
    double cell = uft_vfo_calc_bit_cell(500000.0, 4000000.0);
    TEST_ASSERT(APPROX_EQ(cell, 8.0, 0.001), "Bit cell calculation correct");
    
    /* Test rate estimation */
    double intervals[10] = {8, 8, 8, 16, 8, 16, 8, 8, 16, 8};  /* Mix of 1 and 2 cells */
    double rate = uft_vfo_estimate_rate(intervals, 10, 4000000.0);
    /* Should estimate around 500 kbps */
    TEST_ASSERT(rate > 400000.0 && rate < 600000.0, "Rate estimation reasonable");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║  UFT VFO/PLL Unit Tests                                            ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
    
    test_vfo_init();
    test_vfo_config();
    test_vfo_processing();
    test_vfo_algorithms();
    test_vfo_sync();
    test_data_separator();
    test_vfo_utilities();
    
    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("════════════════════════════════════════════════════════════════════\n");
    
    return (tests_failed > 0) ? 1 : 0;
}
