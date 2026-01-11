/**
 * @file test_kc_turbo.c
 * @brief Tests for KC Turboloader Format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "uft/tape/uft_kc_turbo.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  Testing: %s... ", #name); \
    tests_run++; \
    if (test_##name()) { \
        printf("PASS\n"); \
        tests_passed++; \
    } else { \
        printf("FAIL\n"); \
    } \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 * Profile Count Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_profile_count(void) {
    int count = uft_kc_turbo_count_profiles();
    return count >= 7;  /* At least 7 profiles */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Profile Lookup Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_find_by_type(void) {
    const uft_kc_turbo_profile_t* p = uft_kc_turbo_find(UFT_KC_TURBO_TURBOTAPE);
    return p != NULL && p->baud_rate == 2400;
}

static int test_find_by_name(void) {
    const uft_kc_turbo_profile_t* p = uft_kc_turbo_find_name("FASTTAPE");
    return p != NULL && p->type == UFT_KC_TURBO_FASTTAPE;
}

static int test_find_by_baud(void) {
    const uft_kc_turbo_profile_t* p = uft_kc_turbo_find_baud(4800);
    return p != NULL && p->type == UFT_KC_TURBO_HYPERTAPE;
}

static int test_find_standard(void) {
    const uft_kc_turbo_profile_t* p = uft_kc_turbo_find(UFT_KC_TURBO_NONE);
    return p != NULL && p->baud_rate == 1200 && p->speed_factor == 1.0f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Profile Data Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_turbotape_profile(void) {
    const uft_kc_turbo_profile_t* p = uft_kc_turbo_find(UFT_KC_TURBO_TURBOTAPE);
    if (!p) return 0;
    
    return p->freq_sync == 2400 &&
           p->freq_bit0 == 4800 &&
           p->freq_bit1 == 2400 &&
           p->speed_factor == 2.0f;
}

static int test_hypertape_profile(void) {
    const uft_kc_turbo_profile_t* p = uft_kc_turbo_find(UFT_KC_TURBO_HYPERTAPE);
    if (!p) return 0;
    
    return p->freq_sync == 4800 &&
           p->freq_bit0 == 9600 &&
           p->freq_bit1 == 4800 &&
           p->speed_factor == 4.0f;
}

static int test_basicode_profile(void) {
    const uft_kc_turbo_profile_t* p = uft_kc_turbo_find(UFT_KC_TURBO_BASICODE);
    if (!p) return 0;
    
    /* BASICODE has inverted bit polarity */
    return p->freq_bit0 == 1200 &&
           p->freq_bit1 == 2400 &&
           p->waves_bit0 == 1 &&
           p->waves_bit1 == 2;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Type Name Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_type_names(void) {
    return strcmp(uft_kc_turbo_type_name(UFT_KC_TURBO_NONE), "Standard") == 0 &&
           strcmp(uft_kc_turbo_type_name(UFT_KC_TURBO_TURBOTAPE), "TURBOTAPE") == 0 &&
           strcmp(uft_kc_turbo_type_name(UFT_KC_TURBO_BASICODE), "BASICODE") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Timing Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_timing_standard(void) {
    const uft_kc_turbo_profile_t* p = uft_kc_turbo_find(UFT_KC_TURBO_NONE);
    uft_kc_turbo_timing_t timing;
    uft_kc_turbo_init_timing(&timing, 44100, p);
    
    /* 44100 / 2400 = 18 samples per bit0 cycle */
    return timing.samples_per_bit0 == 18 &&
           timing.samples_per_bit1 == 36 &&  /* 44100 / 1200 */
           timing.sample_rate == 44100;
}

static int test_timing_turbo2x(void) {
    const uft_kc_turbo_profile_t* p = uft_kc_turbo_find(UFT_KC_TURBO_TURBOTAPE);
    uft_kc_turbo_timing_t timing;
    uft_kc_turbo_init_timing(&timing, 44100, p);
    
    /* 44100 / 4800 = 9 samples per bit0 cycle */
    return timing.samples_per_bit0 == 9 &&
           timing.samples_per_bit1 == 18;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Frequency Detection Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_detect_standard(void) {
    uft_kc_turbo_type_t type = uft_kc_turbo_detect_freq(2400, 1200);
    return type == UFT_KC_TURBO_NONE;
}

static int test_detect_turbotape(void) {
    uft_kc_turbo_type_t type = uft_kc_turbo_detect_freq(4800, 2400);
    return type == UFT_KC_TURBO_TURBOTAPE;
}

static int test_detect_hypertape(void) {
    uft_kc_turbo_type_t type = uft_kc_turbo_detect_freq(9600, 4800);
    return type == UFT_KC_TURBO_HYPERTAPE;
}

static int test_detect_with_tolerance(void) {
    /* 5% off should still detect */
    uft_kc_turbo_type_t type = uft_kc_turbo_detect_freq(4700, 2350);
    return type == UFT_KC_TURBO_TURBOTAPE;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Load Time Calculation Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_calc_time(void) {
    const uft_kc_turbo_profile_t* std = uft_kc_turbo_find(UFT_KC_TURBO_NONE);
    const uft_kc_turbo_profile_t* turbo = uft_kc_turbo_find(UFT_KC_TURBO_TURBOTAPE);
    
    float time_std = uft_kc_turbo_calc_time(std, 10000);
    float time_turbo = uft_kc_turbo_calc_time(turbo, 10000);
    
    /* Turbo should be faster */
    return time_turbo < time_std && time_turbo > 0;
}

static int test_calc_time_ratio(void) {
    const uft_kc_turbo_profile_t* std = uft_kc_turbo_find(UFT_KC_TURBO_NONE);
    const uft_kc_turbo_profile_t* turbo2 = uft_kc_turbo_find(UFT_KC_TURBO_TURBOTAPE);
    
    float time_std = uft_kc_turbo_calc_time(std, 10000);
    float time_turbo = uft_kc_turbo_calc_time(turbo2, 10000);
    
    /* Turbo saves time on both baud rate AND sync sequences
     * So ratio can be higher than 2x for "2x" loader */
    float ratio = time_std / time_turbo;
    return ratio > 2.0f && ratio < 5.0f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== KC Turboloader Tests ===\n\n");
    
    printf("[Profile Count]\n");
    TEST(profile_count);
    
    printf("\n[Profile Lookup]\n");
    TEST(find_by_type);
    TEST(find_by_name);
    TEST(find_by_baud);
    TEST(find_standard);
    
    printf("\n[Profile Data]\n");
    TEST(turbotape_profile);
    TEST(hypertape_profile);
    TEST(basicode_profile);
    
    printf("\n[Type Names]\n");
    TEST(type_names);
    
    printf("\n[Timing]\n");
    TEST(timing_standard);
    TEST(timing_turbo2x);
    
    printf("\n[Frequency Detection]\n");
    TEST(detect_standard);
    TEST(detect_turbotape);
    TEST(detect_hypertape);
    TEST(detect_with_tolerance);
    
    printf("\n[Load Time Calculation]\n");
    TEST(calc_time);
    TEST(calc_time_ratio);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    /* Print all profiles */
    printf("Available Turboloader Profiles:\n");
    uft_kc_turbo_list_profiles();
    
    return (tests_passed == tests_run) ? 0 : 1;
}
