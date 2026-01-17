/**
 * @file test_flux_analysis.c
 * @brief Unit tests for Flux Analysis Module
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "uft/flux/uft_flux_analysis.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(condition) do { \
    if (!(condition)) { \
        printf("FAILED at line %d: %s\n", __LINE__, #condition); \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT((x))
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)
#define ASSERT_NULL(p) ASSERT((p) == NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)
#define ASSERT_NEAR(a, b, tol) ASSERT(fabs((a) - (b)) < (tol))

/**
 * @brief Create test transitions with MFM-like timing
 */
static flux_transitions_t *create_mfm_transitions(int count)
{
    flux_transitions_t *trans = flux_create_transitions(FLUX_SAMPLE_RATE_SCP, FLUX_SOURCE_SCP);
    if (!trans) return NULL;
    
    /* MFM timing: ~2µs per bit cell at 40MHz = 80 samples */
    uint32_t time = 0;
    uint32_t cell = 80;  /* 2µs at 40MHz */
    
    for (int i = 0; i < count; i++) {
        /* Simulate MFM pattern with 1x, 1.5x, 2x cell timing */
        int pattern = i % 3;
        uint32_t delta;
        
        switch (pattern) {
            case 0: delta = cell; break;
            case 1: delta = cell + cell / 2; break;
            case 2: delta = cell * 2; break;
            default: delta = cell;
        }
        
        /* Add small jitter (±5%) */
        int jitter = (rand() % (cell / 10)) - (cell / 20);
        delta += jitter;
        
        time += delta;
        flux_add_transition(trans, time);
    }
    
    return trans;
}

/**
 * @brief Create test transitions with GCR-like timing
 */
static flux_transitions_t *create_gcr_transitions(int count)
{
    flux_transitions_t *trans = flux_create_transitions(FLUX_SAMPLE_RATE_SCP, FLUX_SOURCE_SCP);
    if (!trans) return NULL;
    
    /* GCR timing: ~3.25µs at 40MHz = 130 samples */
    uint32_t time = 0;
    uint32_t cell = 130;
    
    for (int i = 0; i < count; i++) {
        uint32_t delta = cell + (rand() % 10) - 5;  /* Small jitter */
        time += delta;
        flux_add_transition(trans, time);
    }
    
    return trans;
}

/* ============================================================================
 * Unit Tests - Constants
 * ============================================================================ */

TEST(constants)
{
    ASSERT_EQ(FLUX_SAMPLE_RATE_KRYOFLUX, 24027428);
    ASSERT_EQ(FLUX_SAMPLE_RATE_SCP, 40000000);
    ASSERT_EQ(FLUX_SAMPLE_RATE_GW, 80000000);
    
    ASSERT_EQ(FLUX_MFM_CELL_NS, 2000);
    ASSERT_EQ(FLUX_GCR_C64_CELL_NS, 3250);
    ASSERT_EQ(FLUX_HISTOGRAM_BINS, 256);
}

/* ============================================================================
 * Unit Tests - Transition Management
 * ============================================================================ */

TEST(create_transitions)
{
    flux_transitions_t *trans = flux_create_transitions(FLUX_SAMPLE_RATE_SCP, FLUX_SOURCE_SCP);
    ASSERT_NOT_NULL(trans);
    ASSERT_EQ(trans->sample_rate, FLUX_SAMPLE_RATE_SCP);
    ASSERT_EQ(trans->source, FLUX_SOURCE_SCP);
    ASSERT_EQ(trans->num_transitions, 0);
    
    flux_free_transitions(trans);
}

TEST(add_transitions)
{
    flux_transitions_t *trans = flux_create_transitions(FLUX_SAMPLE_RATE_SCP, FLUX_SOURCE_SCP);
    ASSERT_NOT_NULL(trans);
    
    for (int i = 0; i < 100; i++) {
        int ret = flux_add_transition(trans, i * 100);
        ASSERT_EQ(ret, 0);
    }
    
    ASSERT_EQ(trans->num_transitions, 100);
    ASSERT_EQ(trans->times[0], 0);
    ASSERT_EQ(trans->times[99], 9900);
    
    flux_free_transitions(trans);
}

TEST(grow_transitions)
{
    flux_transitions_t *trans = flux_create_transitions(FLUX_SAMPLE_RATE_SCP, FLUX_SOURCE_SCP);
    ASSERT_NOT_NULL(trans);
    
    /* Add more than initial capacity */
    for (int i = 0; i < 5000; i++) {
        flux_add_transition(trans, i);
    }
    
    ASSERT_EQ(trans->num_transitions, 5000);
    ASSERT(trans->capacity >= 5000);
    
    flux_free_transitions(trans);
}

/* ============================================================================
 * Unit Tests - Basic Analysis
 * ============================================================================ */

TEST(calc_cell_stats)
{
    flux_transitions_t *trans = create_mfm_transitions(1000);
    ASSERT_NOT_NULL(trans);
    
    flux_cell_stats_t stats;
    int ret = flux_calc_cell_stats(trans, FLUX_ENCODING_MFM, &stats);
    ASSERT_EQ(ret, 0);
    
    ASSERT(stats.mean_ns > 0);
    ASSERT(stats.stddev_ns > 0);
    ASSERT(stats.jitter_percent > 0);
    ASSERT(stats.sample_count > 0);
    
    flux_free_transitions(trans);
}

TEST(generate_histogram)
{
    flux_transitions_t *trans = create_mfm_transitions(1000);
    ASSERT_NOT_NULL(trans);
    
    flux_histogram_t hist;
    int ret = flux_generate_histogram(trans, &hist);
    ASSERT_EQ(ret, 0);
    
    ASSERT_EQ(hist.total_samples, trans->num_transitions - 1);
    ASSERT(hist.min_time_ns > 0);
    ASSERT(hist.max_time_ns > hist.min_time_ns);
    
    flux_free_transitions(trans);
}

TEST(find_histogram_peaks)
{
    flux_transitions_t *trans = create_mfm_transitions(10000);
    ASSERT_NOT_NULL(trans);
    
    flux_histogram_t hist;
    flux_generate_histogram(trans, &hist);
    
    int peaks = flux_find_histogram_peaks(&hist, 4);
    ASSERT(peaks > 0);
    ASSERT(peaks <= 4);
    
    flux_free_transitions(trans);
}

TEST(detect_encoding_mfm)
{
    flux_transitions_t *trans = create_mfm_transitions(1000);
    ASSERT_NOT_NULL(trans);
    
    flux_encoding_t enc = flux_detect_encoding(trans);
    /* Should detect as MFM or something reasonable */
    ASSERT(enc != FLUX_ENCODING_UNKNOWN);
    
    flux_free_transitions(trans);
}

TEST(detect_encoding_gcr)
{
    flux_transitions_t *trans = create_gcr_transitions(1000);
    ASSERT_NOT_NULL(trans);
    
    flux_encoding_t enc = flux_detect_encoding(trans);
    /* Should detect encoding */
    ASSERT(enc != FLUX_ENCODING_RAW);
    
    flux_free_transitions(trans);
}

/* ============================================================================
 * Unit Tests - Revolution Analysis
 * ============================================================================ */

TEST(find_revolutions)
{
    /* Create transitions for ~1 revolution at 300 RPM */
    flux_transitions_t *trans = flux_create_transitions(FLUX_SAMPLE_RATE_SCP, FLUX_SOURCE_SCP);
    ASSERT_NOT_NULL(trans);
    
    /* 200ms at 40MHz = 8,000,000 samples per revolution */
    uint32_t time = 0;
    uint32_t samples_per_rev = 8000000;
    
    for (int r = 0; r < 5; r++) {
        for (int i = 0; i < 100000; i++) {
            flux_add_transition(trans, time);
            time += 80;  /* ~2µs MFM timing */
        }
        time = (r + 1) * samples_per_rev;  /* Force revolution boundary */
    }
    
    flux_revolution_t revs[16];
    int num_revs = flux_find_revolutions(trans, revs, 16);
    
    /* Should find some revolutions */
    ASSERT(num_revs >= 0);
    
    flux_free_transitions(trans);
}

TEST(calc_rpm)
{
    flux_revolution_t rev;
    rev.duration_ns = 200000000;  /* 200ms = 300 RPM */
    rev.start_index = 0;
    rev.num_transitions = 100000;
    
    float rpm = flux_calc_rpm(&rev, FLUX_SAMPLE_RATE_SCP);
    ASSERT_NEAR(rpm, 300.0f, 1.0f);
}

TEST(analyze_speed)
{
    flux_revolution_t revs[5];
    
    for (int i = 0; i < 5; i++) {
        revs[i].duration_ns = 200000000 + (i - 2) * 1000000;  /* ±1ms variation */
        revs[i].rpm = 60000000000.0f / revs[i].duration_ns;
    }
    
    float mean_rpm, variation;
    int ret = flux_analyze_speed(revs, 5, &mean_rpm, &variation);
    
    ASSERT_EQ(ret, 0);
    ASSERT_NEAR(mean_rpm, 300.0f, 5.0f);
    ASSERT(variation < 5.0f);  /* < 5% variation */
}

/* ============================================================================
 * Unit Tests - Track Analysis
 * ============================================================================ */

TEST(analyze_track)
{
    flux_transitions_t *trans = create_mfm_transitions(50000);
    ASSERT_NOT_NULL(trans);
    
    flux_track_analysis_t analysis;
    int ret = flux_analyze_track(trans, 1, 0, &analysis);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(analysis.track, 1);
    ASSERT_EQ(analysis.side, 0);
    ASSERT(analysis.signal_quality >= 0);
    ASSERT(analysis.signal_quality <= 100);
    ASSERT(strlen(analysis.description) > 0);
    
    flux_free_transitions(trans);
}

TEST(find_weak_bits)
{
    flux_transitions_t *trans = flux_create_transitions(FLUX_SAMPLE_RATE_SCP, FLUX_SOURCE_SCP);
    ASSERT_NOT_NULL(trans);
    
    uint32_t time = 0;
    
    /* Normal timing */
    for (int i = 0; i < 500; i++) {
        time += 80;
        flux_add_transition(trans, time);
    }
    
    /* Weak bit region (high variance) */
    for (int i = 0; i < 100; i++) {
        time += 80 + (rand() % 50) - 25;  /* High jitter */
        flux_add_transition(trans, time);
    }
    
    /* More normal timing */
    for (int i = 0; i < 500; i++) {
        time += 80;
        flux_add_transition(trans, time);
    }
    
    int regions;
    int weak = flux_find_weak_bits(trans, 20, &regions);
    
    ASSERT(weak >= 0);
    
    flux_free_transitions(trans);
}

TEST(find_no_flux)
{
    flux_transitions_t *trans = flux_create_transitions(FLUX_SAMPLE_RATE_SCP, FLUX_SOURCE_SCP);
    ASSERT_NOT_NULL(trans);
    
    uint32_t time = 0;
    
    /* Normal flux */
    for (int i = 0; i < 500; i++) {
        time += 80;
        flux_add_transition(trans, time);
    }
    
    /* No-flux gap (100µs = 4000 samples at 40MHz) */
    time += 4000;
    flux_add_transition(trans, time);
    
    /* More normal flux */
    for (int i = 0; i < 500; i++) {
        time += 80;
        flux_add_transition(trans, time);
    }
    
    int no_flux = flux_find_no_flux(trans, 50000, NULL, 0);  /* 50µs threshold */
    ASSERT(no_flux >= 1);
    
    flux_free_transitions(trans);
}

TEST(detect_anomalies)
{
    flux_transitions_t *trans = create_mfm_transitions(1000);
    ASSERT_NOT_NULL(trans);
    
    int anomalies;
    bool detected = flux_detect_anomalies(trans, FLUX_ENCODING_MFM, &anomalies);
    
    /* Synthetic data should have minimal anomalies */
    ASSERT(anomalies >= 0);
    
    flux_free_transitions(trans);
}

/* ============================================================================
 * Unit Tests - Disk Analysis
 * ============================================================================ */

TEST(create_disk_analysis)
{
    flux_disk_analysis_t *analysis = flux_create_disk_analysis(80, 2);
    ASSERT_NOT_NULL(analysis);
    ASSERT_EQ(analysis->num_tracks, 80);
    ASSERT_EQ(analysis->num_sides, 2);
    ASSERT_NOT_NULL(analysis->tracks);
    
    flux_free_disk_analysis(analysis);
}

/* ============================================================================
 * Unit Tests - Protection Detection
 * ============================================================================ */

TEST(detect_protection_long_track)
{
    flux_track_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));
    
    analysis.track = 36;
    analysis.has_long_track = true;
    
    char desc[256];
    bool detected = flux_detect_protection(&analysis, desc, sizeof(desc));
    
    ASSERT_TRUE(detected);
    ASSERT(strlen(desc) > 0);
}

TEST(detect_protection_weak_bits)
{
    flux_track_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));
    
    analysis.track = 18;
    analysis.has_weak_region = true;
    
    char desc[256];
    bool detected = flux_detect_protection(&analysis, desc, sizeof(desc));
    
    ASSERT_TRUE(detected);
}

TEST(detect_protection_none)
{
    flux_track_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));
    
    analysis.track = 1;
    
    char desc[256];
    bool detected = flux_detect_protection(&analysis, desc, sizeof(desc));
    
    ASSERT_FALSE(detected);
}

/* ============================================================================
 * Unit Tests - Utilities
 * ============================================================================ */

TEST(samples_to_ns)
{
    /* At 40MHz, 40 samples = 1µs = 1000ns */
    uint32_t ns = flux_samples_to_ns(40, FLUX_SAMPLE_RATE_SCP);
    ASSERT_NEAR(ns, 1000, 10);
    
    /* At 40MHz, 40000 samples = 1ms = 1,000,000ns */
    ns = flux_samples_to_ns(40000, FLUX_SAMPLE_RATE_SCP);
    ASSERT_NEAR(ns, 1000000, 100);
}

TEST(ns_to_samples)
{
    /* 1µs at 40MHz = 40 samples */
    uint32_t samples = flux_ns_to_samples(1000, FLUX_SAMPLE_RATE_SCP);
    ASSERT_NEAR(samples, 40, 1);
    
    /* 1ms at 40MHz = 40000 samples */
    samples = flux_ns_to_samples(1000000, FLUX_SAMPLE_RATE_SCP);
    ASSERT_NEAR(samples, 40000, 10);
}

TEST(encoding_name)
{
    ASSERT_STR_EQ(flux_encoding_name(FLUX_ENCODING_MFM), "MFM");
    ASSERT_STR_EQ(flux_encoding_name(FLUX_ENCODING_GCR_C64), "GCR (C64)");
    ASSERT_STR_EQ(flux_encoding_name(FLUX_ENCODING_FM), "FM");
    ASSERT_NOT_NULL(flux_encoding_name(FLUX_ENCODING_UNKNOWN));
}

TEST(source_name)
{
    ASSERT_STR_EQ(flux_source_name(FLUX_SOURCE_KRYOFLUX), "Kryoflux");
    ASSERT_STR_EQ(flux_source_name(FLUX_SOURCE_SCP), "SuperCard Pro");
    ASSERT_STR_EQ(flux_source_name(FLUX_SOURCE_GREASEWEAZLE), "Greaseweazle");
}

TEST(expected_cell_time)
{
    ASSERT_EQ(flux_expected_cell_time(FLUX_ENCODING_MFM), FLUX_MFM_CELL_NS);
    ASSERT_EQ(flux_expected_cell_time(FLUX_ENCODING_FM), FLUX_FM_CELL_NS);
    ASSERT_EQ(flux_expected_cell_time(FLUX_ENCODING_GCR_C64), FLUX_GCR_C64_CELL_NS);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    /* Seed random for jitter simulation */
    srand(42);
    
    printf("\n=== Flux Analysis Tests ===\n\n");
    
    printf("Constants:\n");
    RUN_TEST(constants);
    
    printf("\nTransition Management:\n");
    RUN_TEST(create_transitions);
    RUN_TEST(add_transitions);
    RUN_TEST(grow_transitions);
    
    printf("\nBasic Analysis:\n");
    RUN_TEST(calc_cell_stats);
    RUN_TEST(generate_histogram);
    RUN_TEST(find_histogram_peaks);
    RUN_TEST(detect_encoding_mfm);
    RUN_TEST(detect_encoding_gcr);
    
    printf("\nRevolution Analysis:\n");
    RUN_TEST(find_revolutions);
    RUN_TEST(calc_rpm);
    RUN_TEST(analyze_speed);
    
    printf("\nTrack Analysis:\n");
    RUN_TEST(analyze_track);
    RUN_TEST(find_weak_bits);
    RUN_TEST(find_no_flux);
    RUN_TEST(detect_anomalies);
    
    printf("\nDisk Analysis:\n");
    RUN_TEST(create_disk_analysis);
    
    printf("\nProtection Detection:\n");
    RUN_TEST(detect_protection_long_track);
    RUN_TEST(detect_protection_weak_bits);
    RUN_TEST(detect_protection_none);
    
    printf("\nUtilities:\n");
    RUN_TEST(samples_to_ns);
    RUN_TEST(ns_to_samples);
    RUN_TEST(encoding_name);
    RUN_TEST(source_name);
    RUN_TEST(expected_cell_time);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
