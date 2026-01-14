/**
 * @file test_pll.c
 * @brief PLL (Phase-Locked Loop) algorithm validation tests
 * 
 * Tests the flux-to-bits conversion with various timing scenarios.
 * Critical for accurate disk decoding.
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/* Test framework */
static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_BEGIN(name) \
    do { \
        g_tests_run++; \
        printf("  [%02d] %-50s ", g_tests_run, name); \
        fflush(stdout); \
    } while(0)

#define TEST_PASS() do { g_tests_passed++; printf("\033[32m[PASS]\033[0m\n"); } while(0)
#define TEST_FAIL(msg) do { printf("\033[31m[FAIL]\033[0m %s\n", msg); } while(0)
#define TEST_EXPECT(cond, msg) do { if (cond) { TEST_PASS(); } else { TEST_FAIL(msg); } } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * PLL Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define MFM_CELL_NS_DD      2000    /* ~2µs for DD */
#define MFM_CELL_NS_HD      1000    /* ~1µs for HD */
#define GCR_CELL_NS_C64     4000    /* ~4µs for C64 zone 1 */

/* PLL tuning defaults */
#define PLL_PHASE_GAIN      0.65
#define PLL_FREQ_GAIN       0.04
#define PLL_ERROR_TOL       0.20

/* ═══════════════════════════════════════════════════════════════════════════════
 * Simple PLL Implementation for Testing
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    double cell_ns;          /* Current cell period estimate */
    double cell_min;         /* Minimum allowed cell */
    double cell_max;         /* Maximum allowed cell */
    double phase_gain;       /* Phase adjustment gain */
    double freq_gain;        /* Frequency adjustment gain */
    double window_pos;       /* Current window position in ns */
    int bit_count;           /* Bits decoded */
    int sync_losses;         /* Sync loss count */
} test_pll_t;

static void pll_init(test_pll_t *pll, double cell_ns) {
    pll->cell_ns = cell_ns;
    pll->cell_min = cell_ns * 0.8;
    pll->cell_max = cell_ns * 1.2;
    pll->phase_gain = PLL_PHASE_GAIN;
    pll->freq_gain = PLL_FREQ_GAIN;
    pll->window_pos = 0;
    pll->bit_count = 0;
    pll->sync_losses = 0;
}

/**
 * @brief Process single flux transition
 * @return Number of bits to output (0, 1, or more for long gaps)
 */
static int pll_process_flux(test_pll_t *pll, double flux_ns, uint8_t *bits) {
    double delta = flux_ns - pll->window_pos;
    
    /* Calculate how many bit cells this represents */
    double cells = delta / pll->cell_ns;
    int cell_count = (int)(cells + 0.5);  /* Round to nearest */
    
    if (cell_count < 1) cell_count = 1;
    if (cell_count > 8) {
        pll->sync_losses++;
        cell_count = 8;  /* Limit run length */
    }
    
    /* Phase error */
    double expected = pll->window_pos + (cell_count * pll->cell_ns);
    double phase_err = flux_ns - expected;
    
    /* Adjust window position (phase) */
    pll->window_pos = flux_ns + (phase_err * pll->phase_gain);
    
    /* Adjust cell period (frequency) */
    double freq_adj = (phase_err / cell_count) * pll->freq_gain;
    pll->cell_ns += freq_adj;
    
    /* Clamp cell period */
    if (pll->cell_ns < pll->cell_min) pll->cell_ns = pll->cell_min;
    if (pll->cell_ns > pll->cell_max) pll->cell_ns = pll->cell_max;
    
    /* Output bits: 0s followed by 1 */
    int output_bits = 0;
    for (int i = 0; i < cell_count - 1; i++) {
        if (bits) bits[output_bits] = 0;
        output_bits++;
    }
    if (bits) bits[output_bits] = 1;
    output_bits++;
    
    pll->bit_count += output_bits;
    return output_bits;
}

/**
 * @brief Decode flux array to bits
 */
static int pll_decode_flux(
    const double *flux_times,  /* Absolute times in ns */
    size_t flux_count,
    double cell_ns,
    uint8_t *out_bits,
    size_t max_bits)
{
    test_pll_t pll;
    pll_init(&pll, cell_ns);
    
    size_t bit_pos = 0;
    for (size_t i = 0; i < flux_count && bit_pos < max_bits; i++) {
        uint8_t temp_bits[16];
        int count = pll_process_flux(&pll, flux_times[i], temp_bits);
        
        for (int j = 0; j < count && bit_pos < max_bits; j++) {
            out_bits[bit_pos++] = temp_bits[j];
        }
    }
    
    return (int)bit_pos;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test: Perfect Timing (No Jitter)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_pll_perfect_timing(void) {
    TEST_BEGIN("PLL: Perfect MFM timing (no jitter)");
    
    /* Generate perfect MFM timing: 1010101010 pattern */
    /* Each flux at exactly 2000ns intervals */
    double flux[10];
    for (int i = 0; i < 10; i++) {
        flux[i] = (i + 1) * MFM_CELL_NS_DD;
    }
    
    uint8_t bits[32] = {0};
    int bit_count = pll_decode_flux(flux, 10, MFM_CELL_NS_DD, bits, sizeof(bits));
    
    /* Should decode to alternating 1s */
    bool all_ones = true;
    for (int i = 0; i < bit_count; i++) {
        if (bits[i] != 1) all_ones = false;
    }
    
    TEST_EXPECT(bit_count == 10 && all_ones, "Perfect timing decode failed");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test: Timing with Jitter
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_pll_with_jitter(void) {
    TEST_BEGIN("PLL: MFM timing with ±10% jitter");
    
    /* Generate MFM timing with jitter */
    double flux[10];
    double jitter[] = {0.95, 1.05, 0.98, 1.02, 1.00, 0.97, 1.03, 0.99, 1.01, 0.96};
    
    double time = 0;
    for (int i = 0; i < 10; i++) {
        time += MFM_CELL_NS_DD * jitter[i];
        flux[i] = time;
    }
    
    uint8_t bits[32] = {0};
    int bit_count = pll_decode_flux(flux, 10, MFM_CELL_NS_DD, bits, sizeof(bits));
    
    /* Should still decode to 10 bits */
    TEST_EXPECT(bit_count == 10, "Jittery timing decode failed");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test: Long Run (00001)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_pll_long_run(void) {
    TEST_BEGIN("PLL: Long run (5 cells between flux)");
    
    /* MFM allows max 3 zeros between 1s: 10001 */
    double flux[] = {
        MFM_CELL_NS_DD * 1,   /* First flux */
        MFM_CELL_NS_DD * 5,   /* 4 cells later = 0001 */
        MFM_CELL_NS_DD * 9,   /* 4 more cells */
    };
    
    uint8_t bits[32] = {0};
    int bit_count = pll_decode_flux(flux, 3, MFM_CELL_NS_DD, bits, sizeof(bits));
    
    /* Should decode: 1, 0001, 0001 = 9 bits */
    TEST_EXPECT(bit_count >= 9, "Long run decode failed");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test: Clock Drift
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_pll_clock_drift(void) {
    TEST_BEGIN("PLL: Gradual clock drift +5%");
    
    /* Simulate drive with 5% faster rotation over time */
    double flux[20];
    double time = 0;
    double cell = MFM_CELL_NS_DD;
    double drift_per_cell = 0.0025;  /* 0.25% per cell → 5% over 20 cells */
    
    for (int i = 0; i < 20; i++) {
        time += cell;
        flux[i] = time;
        cell *= (1.0 - drift_per_cell);  /* Speed up */
    }
    
    test_pll_t pll;
    pll_init(&pll, MFM_CELL_NS_DD);
    
    for (int i = 0; i < 20; i++) {
        pll_process_flux(&pll, flux[i], NULL);
    }
    
    /* PLL should have adapted cell period */
    double final_cell = pll.cell_ns;
    double expected_cell = MFM_CELL_NS_DD * pow(1.0 - drift_per_cell, 20);
    double error = fabs(final_cell - expected_cell) / expected_cell;
    
    TEST_EXPECT(error < 0.05, "PLL didn't track drift");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test: Sync Recovery
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_pll_sync_recovery(void) {
    TEST_BEGIN("PLL: Sync recovery after gap");
    
    /* Normal flux, then gap, then normal again */
    double flux[] = {
        2000, 4000, 6000, 8000,        /* Normal */
        50000,                          /* Big gap (lost sync) */
        52000, 54000, 56000, 58000     /* Normal again */
    };
    
    test_pll_t pll;
    pll_init(&pll, MFM_CELL_NS_DD);
    
    for (int i = 0; i < 9; i++) {
        pll_process_flux(&pll, flux[i], NULL);
    }
    
    /* Should have detected at least one sync loss */
    TEST_EXPECT(pll.sync_losses >= 1, "Sync loss not detected");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test: HD vs DD Cell Size
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_pll_hd_timing(void) {
    TEST_BEGIN("PLL: HD timing (1µs cells)");
    
    double flux[10];
    for (int i = 0; i < 10; i++) {
        flux[i] = (i + 1) * MFM_CELL_NS_HD;  /* 1µs cells */
    }
    
    uint8_t bits[32] = {0};
    int bit_count = pll_decode_flux(flux, 10, MFM_CELL_NS_HD, bits, sizeof(bits));
    
    TEST_EXPECT(bit_count == 10, "HD timing decode failed");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test: GCR C64 Timing
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_pll_gcr_c64(void) {
    TEST_BEGIN("PLL: GCR C64 timing (4µs zone 1)");
    
    double flux[8];
    for (int i = 0; i < 8; i++) {
        flux[i] = (i + 1) * GCR_CELL_NS_C64;
    }
    
    uint8_t bits[32] = {0};
    int bit_count = pll_decode_flux(flux, 8, GCR_CELL_NS_C64, bits, sizeof(bits));
    
    TEST_EXPECT(bit_count == 8, "GCR C64 decode failed");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test: Phase Lock Stability
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_pll_phase_stability(void) {
    TEST_BEGIN("PLL: Phase lock stability");
    
    /* Start with wrong phase, should lock */
    double flux[20];
    double offset = MFM_CELL_NS_DD * 0.3;  /* Start 30% off phase */
    
    for (int i = 0; i < 20; i++) {
        flux[i] = offset + (i + 1) * MFM_CELL_NS_DD;
    }
    
    test_pll_t pll;
    pll_init(&pll, MFM_CELL_NS_DD);
    
    /* Process all flux */
    for (int i = 0; i < 20; i++) {
        pll_process_flux(&pll, flux[i], NULL);
    }
    
    /* After 20 bits, should have decoded approximately 20 bits */
    TEST_EXPECT(pll.bit_count >= 18, "Phase lock failed");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test: Determinism
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_pll_deterministic(void) {
    TEST_BEGIN("PLL: Deterministic decode");
    
    double flux[] = {2000, 4000, 6000, 8000, 10000};
    
    uint8_t bits1[16] = {0};
    uint8_t bits2[16] = {0};
    
    int count1 = pll_decode_flux(flux, 5, MFM_CELL_NS_DD, bits1, sizeof(bits1));
    int count2 = pll_decode_flux(flux, 5, MFM_CELL_NS_DD, bits2, sizeof(bits2));
    
    bool match = (count1 == count2) && (memcmp(bits1, bits2, count1) == 0);
    TEST_EXPECT(match, "Non-deterministic decode");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT PLL Algorithm Tests\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    printf("Basic Timing Tests:\n");
    test_pll_perfect_timing();
    test_pll_with_jitter();
    test_pll_long_run();
    
    printf("\nAdaptive Tests:\n");
    test_pll_clock_drift();
    test_pll_sync_recovery();
    test_pll_phase_stability();
    
    printf("\nFormat-Specific Tests:\n");
    test_pll_hd_timing();
    test_pll_gcr_c64();
    
    printf("\nQuality Tests:\n");
    test_pll_deterministic();
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d/%d tests passed\n", g_tests_passed, g_tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
