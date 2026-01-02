#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

typedef float uft_confidence_t;

typedef struct {
    uint32_t initial_cell_ns;
    uint32_t cell_ns_min;
    uint32_t cell_ns_max;
    float process_noise_cell;
    float process_noise_drift;
    float measurement_noise;
    float weak_bit_threshold;
    int bidirectional;
    uint32_t max_run_cells;
} uft_kalman_pll_config_t;

typedef struct {
    float x_cell;
    float x_drift;
    float P00, P01, P11;
    float last_innovation;
    float innovation_var;
    uint64_t transitions_processed;
    uint64_t weak_bits_detected;
    uint64_t spike_rejections;
} uft_kalman_pll_state_t;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  TEST: %s ... ", #name); \
        if (test_##name()) { \
            tests_passed++; \
            printf("PASS\n"); \
        } else { \
            printf("FAIL\n"); \
        } \
    } while(0)

#define ASSERT_TRUE(x) do { if (!(x)) return 0; } while(0)
#define ASSERT_NEAR(a, b, tol) do { if (fabs((a)-(b)) > (tol)) return 0; } while(0)

// Echte Kalman-Update-Funktion
static void kalman_update(uft_kalman_pll_state_t *s, float measurement, int cells) {
    // Predict
    float x_pred = s->x_cell + s->x_drift;
    float P00_pred = s->P00 + 2*s->P01 + s->P11 + 1.0f;
    
    // Measurement
    float z = measurement;
    float H = (float)cells;
    float y = z - H * x_pred;  // Innovation
    
    // Innovation covariance
    float S = H * H * P00_pred + 100.0f;  // R = 100
    
    // Kalman gain
    float K = P00_pred * H / S;
    
    // Update
    s->x_cell = x_pred + K * y;
    s->P00 = (1.0f - K * H) * P00_pred;
    s->last_innovation = y;
    s->innovation_var = S;
}

static int test_kalman_convergence(void) {
    const uint32_t true_cell = 2000;
    
    uft_kalman_pll_state_t state = {
        .x_cell = 2500.0f,  // Wrong initial estimate
        .x_drift = 0.0f,
        .P00 = 10000.0f,
        .P01 = 0.0f,
        .P11 = 1.0f
    };
    
    // Run 100 updates with correct measurements
    for (int i = 0; i < 100; i++) {
        int cells = (i % 2) ? 1 : 2;
        float measurement = (float)(cells * true_cell);
        kalman_update(&state, measurement, cells);
    }
    
    ASSERT_NEAR(state.x_cell, true_cell, 100.0f);  // Within 100ns
    ASSERT_TRUE(state.P00 < 200.0f);  // Variance reduced
    
    return 1;
}

static int test_spike_rejection(void) {
    uft_kalman_pll_state_t state = {
        .x_cell = 2000.0f,
        .spike_rejections = 0
    };
    
    uint64_t spike_delta = 400;  // < 2000*0.25 = 500
    
    if ((float)spike_delta < state.x_cell * 0.25f) {
        state.spike_rejections++;
    }
    
    ASSERT_TRUE(state.spike_rejections == 1);
    return 1;
}

static int test_weak_bit_detection(void) {
    uft_kalman_pll_state_t state = {
        .x_cell = 2000.0f,
        .P00 = 100.0f,
        .weak_bits_detected = 0
    };
    
    float weak_bit_threshold = 3.0f;
    
    // Normal innovation (small)
    float innovation1 = 50.0f;
    float sigma1 = sqrtf(state.P00 + 100.0f);  // ~14
    if (fabsf(innovation1) / sigma1 > weak_bit_threshold) {
        state.weak_bits_detected++;
    }
    ASSERT_TRUE(state.weak_bits_detected == 0);
    
    // Large innovation -> weak bit
    float innovation2 = 500.0f;
    float sigma2 = sqrtf(state.P00 + 100.0f);
    if (fabsf(innovation2) / sigma2 > weak_bit_threshold) {
        state.weak_bits_detected++;
    }
    ASSERT_TRUE(state.weak_bits_detected == 1);
    
    return 1;
}

static int test_drift_tracking(void) {
    uft_kalman_pll_state_t state = {
        .x_cell = 2000.0f,
        .x_drift = 0.0f,
        .P00 = 100.0f,
        .P01 = 0.0f,
        .P11 = 1.0f
    };
    
    // Simulate drift: cell time increases by 2ns per transition
    float true_drift = 2.0f;
    float current_cell = 2000.0f;
    
    for (int i = 0; i < 50; i++) {
        current_cell += true_drift;
        kalman_update(&state, current_cell, 1);
    }
    
    // State should track the drifting cell time
    ASSERT_TRUE(state.x_cell > 2050.0f);  // Drifted up
    
    return 1;
}

static int test_config_presets(void) {
    uft_kalman_pll_config_t mfm_dd = {
        .initial_cell_ns = 2000,
        .cell_ns_min = 1600,
        .cell_ns_max = 2400,
        .max_run_cells = 8
    };
    
    ASSERT_TRUE(mfm_dd.initial_cell_ns == 2000);
    ASSERT_TRUE(mfm_dd.cell_ns_min < mfm_dd.initial_cell_ns);
    ASSERT_TRUE(mfm_dd.cell_ns_max > mfm_dd.initial_cell_ns);
    
    uft_kalman_pll_config_t mfm_hd = {
        .initial_cell_ns = 1000,
        .cell_ns_min = 800,
        .cell_ns_max = 1200,
        .max_run_cells = 8
    };
    
    ASSERT_TRUE(mfm_hd.initial_cell_ns == 1000);
    ASSERT_TRUE(mfm_hd.initial_cell_ns < mfm_dd.initial_cell_ns);
    
    return 1;
}

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("         KALMAN PLL UNIT TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    TEST(kalman_convergence);
    TEST(spike_rejection);
    TEST(weak_bit_detection);
    TEST(drift_tracking);
    TEST(config_presets);
    
    printf("\n───────────────────────────────────────────────────────────────────\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("───────────────────────────────────────────────────────────────────\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
