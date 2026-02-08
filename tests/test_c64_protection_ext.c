/**
 * @file test_c64_protection_ext.c
 * @brief Unit tests for Extended C64 Protection Detection
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/protection/uft_c64_protection_ext.h"

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
#define ASSERT_TRUE(x) ASSERT((x))
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/* ============================================================================
 * Unit Tests - TimeWarp
 * ============================================================================ */

TEST(timewarp_v1)
{
    /* Create data with TimeWarp v1 signature */
    uint8_t data[256];
    memset(data, 0x55, sizeof(data));
    
    /* Insert TimeWarp v1 signature */
    data[100] = 0xA9; data[101] = 0x00;
    data[102] = 0x85; data[103] = 0x02;
    data[104] = 0xA9; data[105] = 0x36;
    
    c64_timewarp_result_t result;
    bool detected = c64_detect_timewarp(data, sizeof(data), &result);
    
    ASSERT_TRUE(detected);
    ASSERT_TRUE(result.detected);
    ASSERT_EQ(result.version, 1);
}

TEST(timewarp_v2)
{
    uint8_t data[256];
    memset(data, 0x55, sizeof(data));
    
    /* Insert TimeWarp v2 signature */
    data[50] = 0xA9; data[51] = 0x00;
    data[52] = 0x8D; data[53] = 0x00;
    data[54] = 0xDD; data[55] = 0xA9;
    
    c64_timewarp_result_t result;
    bool detected = c64_detect_timewarp(data, sizeof(data), &result);
    
    ASSERT_TRUE(detected);
    ASSERT_EQ(result.version, 2);
}

TEST(timewarp_v3)
{
    uint8_t data[256];
    memset(data, 0x55, sizeof(data));
    
    /* Insert TimeWarp v3 signature */
    data[0] = 0x78; data[1] = 0xA9;
    data[2] = 0x7F; data[3] = 0x8D;
    data[4] = 0x0D; data[5] = 0xDC;
    
    c64_timewarp_result_t result;
    bool detected = c64_detect_timewarp(data, sizeof(data), &result);
    
    ASSERT_TRUE(detected);
    ASSERT_EQ(result.version, 3);
}

TEST(timewarp_not_present)
{
    uint8_t data[256];
    memset(data, 0x55, sizeof(data));
    
    c64_timewarp_result_t result;
    bool detected = c64_detect_timewarp(data, sizeof(data), &result);
    
    ASSERT_FALSE(detected);
    ASSERT_FALSE(result.detected);
}

TEST(timewarp_track)
{
    uint8_t track_data[7000];
    memset(track_data, 0x55, sizeof(track_data));
    
    /* Insert signature on track 36 */
    track_data[1000] = 0xA9; track_data[1001] = 0x00;
    track_data[1002] = 0x85; track_data[1003] = 0x02;
    track_data[1004] = 0xA9; track_data[1005] = 0x36;
    
    c64_timewarp_result_t result;
    
    /* Track 36 should detect */
    bool detected = c64_detect_timewarp_track(track_data, sizeof(track_data), 36, &result);
    ASSERT_TRUE(detected);
    
    /* Track 10 should not (wrong track range) */
    detected = c64_detect_timewarp_track(track_data, sizeof(track_data), 10, &result);
    ASSERT_FALSE(detected);
}

/* ============================================================================
 * Unit Tests - Densitron
 * ============================================================================ */

TEST(densitron_pattern)
{
    uint8_t pattern1[] = {3, 2, 1, 0};  /* Valid gradient */
    uint8_t pattern2[] = {0, 1, 2, 3};  /* Valid reverse */
    uint8_t pattern3[] = {3, 3, 3, 3};  /* Invalid */
    
    ASSERT_TRUE(c64_is_densitron_pattern(pattern1));
    ASSERT_TRUE(c64_is_densitron_pattern(pattern2));
    ASSERT_FALSE(c64_is_densitron_pattern(pattern3));
}

TEST(densitron_detect)
{
    /* Create density array with Densitron pattern on tracks 36-39 */
    uint8_t densities[85];
    memset(densities, 3, sizeof(densities));  /* Default density 3 */
    
    /* Set Densitron pattern at halftracks 72-75 (tracks 36-37.5) */
    densities[72] = 3;
    densities[73] = 2;
    densities[74] = 1;
    densities[75] = 0;
    
    c64_densitron_result_t result;
    bool detected = c64_detect_densitron(densities, 85, &result);
    
    ASSERT_TRUE(detected);
    ASSERT_TRUE(result.detected);
    ASSERT_EQ(result.num_key_tracks, 4);
}

TEST(densitron_not_present)
{
    uint8_t densities[85];
    memset(densities, 3, sizeof(densities));  /* Uniform density */
    
    c64_densitron_result_t result;
    bool detected = c64_detect_densitron(densities, 85, &result);
    
    ASSERT_FALSE(detected);
}

/* ============================================================================
 * Unit Tests - Kracker Jax
 * ============================================================================ */

TEST(kracker_jax_detect)
{
    uint8_t data[256];
    memset(data, 0x00, sizeof(data));
    
    /* Insert "KRACK" signature */
    data[100] = 'K'; data[101] = 'R';
    data[102] = 'A'; data[103] = 'C';
    data[104] = 'K';
    
    c64_kracker_jax_result_t result;
    bool detected = c64_detect_kracker_jax(data, sizeof(data), &result);
    
    ASSERT_TRUE(detected);
    ASSERT_TRUE(result.detected);
}

TEST(kracker_jax_not_present)
{
    uint8_t data[256];
    memset(data, 0x55, sizeof(data));
    
    c64_kracker_jax_result_t result;
    bool detected = c64_detect_kracker_jax(data, sizeof(data), &result);
    
    ASSERT_FALSE(detected);
}

/* ============================================================================
 * Unit Tests - Generic Detection
 * ============================================================================ */

TEST(detect_ext_timewarp)
{
    uint8_t data[256];
    memset(data, 0x55, sizeof(data));
    
    /* Insert TimeWarp v1 signature */
    data[50] = 0xA9; data[51] = 0x00;
    data[52] = 0x85; data[53] = 0x02;
    data[54] = 0xA9; data[55] = 0x36;
    
    c64_prot_ext_result_t result;
    bool detected = c64_detect_protection_ext(C64_PROT_EXT_TIMEWARP, data, sizeof(data), &result);
    
    ASSERT_TRUE(detected);
    ASSERT_EQ(result.type, C64_PROT_EXT_TIMEWARP);
    ASSERT(result.confidence >= 90);
}

TEST(scan_protections)
{
    uint8_t data[512];
    memset(data, 0x55, sizeof(data));
    
    /* Insert TimeWarp signature */
    data[100] = 0xA9; data[101] = 0x00;
    data[102] = 0x85; data[103] = 0x02;
    data[104] = 0xA9; data[105] = 0x36;
    
    /* Insert Kracker Jax signature */
    data[200] = 'K'; data[201] = 'R';
    data[202] = 'A'; data[203] = 'C';
    data[204] = 'K';
    
    c64_prot_ext_scan_t scan;
    int found = c64_scan_protections_ext(data, sizeof(data), &scan);
    
    ASSERT_EQ(found, 2);
    ASSERT_EQ(scan.num_found, 2);
}

TEST(scan_no_protections)
{
    uint8_t data[256];
    memset(data, 0x55, sizeof(data));
    
    c64_prot_ext_scan_t scan;
    int found = c64_scan_protections_ext(data, sizeof(data), &scan);
    
    ASSERT_EQ(found, 0);
    ASSERT_EQ(scan.num_found, 0);
}

/* ============================================================================
 * Unit Tests - Track Analysis
 * ============================================================================ */

TEST(fat_track)
{
    uint8_t track[8000];
    memset(track, 0x55, sizeof(track));
    
    /* Normal capacity for density 3 is ~7692 bytes */
    ASSERT_TRUE(c64_is_fat_track(track, 8000, 7000));  /* 14% larger */
    ASSERT_FALSE(c64_is_fat_track(track, 7000, 7000)); /* Same size */
}

TEST(custom_sync)
{
    uint8_t track[1000];
    memset(track, 0x55, sizeof(track));
    
    /* Add some sync marks with non-standard follow-up bytes */
    track[100] = 0xFF; track[101] = 0xFF;
    track[102] = 0x52;  /* Standard header marker */
    
    track[200] = 0xFF; track[201] = 0xFF;
    track[202] = 0x99;  /* Non-standard (bit 7 set) */
    
    int non_standard = c64_check_custom_sync(track, sizeof(track), 0x52);
    ASSERT(non_standard >= 1);
}

TEST(analyze_gaps)
{
    uint8_t track[1000];
    memset(track, 0x00, sizeof(track));
    
    /* Add some gaps */
    memset(track + 100, 0x55, 10);  /* 10-byte gap */
    memset(track + 300, 0x55, 20);  /* 20-byte gap */
    memset(track + 500, 0x55, 15);  /* 15-byte gap */
    
    int min_gap, max_gap, avg_gap;
    int gaps = c64_analyze_gaps(track, sizeof(track), &min_gap, &max_gap, &avg_gap);
    
    ASSERT(gaps >= 3);
    ASSERT(max_gap >= min_gap);
}

/* ============================================================================
 * Unit Tests - Utilities
 * ============================================================================ */

TEST(prot_names)
{
    ASSERT_STR_EQ(c64_prot_ext_name(C64_PROT_EXT_TIMEWARP), "TimeWarp");
    ASSERT_STR_EQ(c64_prot_ext_name(C64_PROT_EXT_DENSITRON), "Densitron");
    ASSERT_STR_EQ(c64_prot_ext_name(C64_PROT_EXT_KRACKER_JAX), "Kracker Jax");
    ASSERT_NOT_NULL(c64_prot_ext_name(C64_PROT_EXT_NONE));
}

TEST(prot_categories)
{
    ASSERT_STR_EQ(c64_prot_ext_category(C64_PROT_EXT_TIMEWARP), "Track-based");
    ASSERT_STR_EQ(c64_prot_ext_category(C64_PROT_EXT_GMA), "Sector-based");
    ASSERT_STR_EQ(c64_prot_ext_category(C64_PROT_EXT_OCEAN), "Publisher");
}

TEST(prot_type_checks)
{
    ASSERT_TRUE(c64_prot_ext_is_track_based(C64_PROT_EXT_TIMEWARP));
    ASSERT_FALSE(c64_prot_ext_is_track_based(C64_PROT_EXT_GMA));
    
    ASSERT_TRUE(c64_prot_ext_is_density_based(C64_PROT_EXT_DENSITRON));
    ASSERT_FALSE(c64_prot_ext_is_density_based(C64_PROT_EXT_TIMEWARP));
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== Extended C64 Protection Tests ===\n\n");
    
    printf("TimeWarp Detection:\n");
    RUN_TEST(timewarp_v1);
    RUN_TEST(timewarp_v2);
    RUN_TEST(timewarp_v3);
    RUN_TEST(timewarp_not_present);
    RUN_TEST(timewarp_track);
    
    printf("\nDensitron Detection:\n");
    RUN_TEST(densitron_pattern);
    RUN_TEST(densitron_detect);
    RUN_TEST(densitron_not_present);
    
    printf("\nKracker Jax Detection:\n");
    RUN_TEST(kracker_jax_detect);
    RUN_TEST(kracker_jax_not_present);
    
    printf("\nGeneric Detection:\n");
    RUN_TEST(detect_ext_timewarp);
    RUN_TEST(scan_protections);
    RUN_TEST(scan_no_protections);
    
    printf("\nTrack Analysis:\n");
    RUN_TEST(fat_track);
    RUN_TEST(custom_sync);
    RUN_TEST(analyze_gaps);
    
    printf("\nUtilities:\n");
    RUN_TEST(prot_names);
    RUN_TEST(prot_categories);
    RUN_TEST(prot_type_checks);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
