/**
 * @file test_decode_score.c
 * @brief Unit tests for Unified Decode Scoring System (W-P1-003)
 * 
 * Tests cover:
 * - Score initialization
 * - Component scoring (CRC, ID, timing, protection)
 * - Score comparison
 * - String conversion
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "uft/uft_decode_score.h"

/*============================================================================
 * TEST FRAMEWORK
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;
static int current_test_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s ... ", #name); \
    tests_run++; \
    current_test_failed = 0; \
    test_##name(); \
    if (!current_test_failed) { \
        tests_passed++; \
        printf("PASS\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        current_test_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)

/*============================================================================
 * TESTS: INITIALIZATION
 *============================================================================*/

TEST(score_init) {
    uft_decode_score_t score;
    uft_score_init(&score);
    
    ASSERT_EQ(score.total, 0);
    ASSERT_EQ(score.crc_score, 0);
    ASSERT_EQ(score.id_score, 0);
    ASSERT_FALSE(score.crc_ok);
}

TEST(score_init_null) {
    /* Should not crash */
    uft_score_init(NULL);
}

/*============================================================================
 * TESTS: SCORE CALCULATION
 *============================================================================*/

TEST(score_perfect_sector) {
    uft_decode_score_t score;
    
    uft_score_sector(&score,
                     true,       /* CRC OK */
                     5, 0, 3,    /* Track 5, Head 0, Sector 3 */
                     80, 18,     /* Max cyl 80, max sector 18 */
                     50.0,       /* Timing jitter 50ns */
                     500.0,      /* Threshold 500ns */
                     false,      /* No protection expected */
                     false);     /* No protection found */
    
    ASSERT_TRUE(score.crc_ok);
    ASSERT_EQ(score.crc_score, 40);  /* Full CRC points */
    ASSERT_TRUE(score.id_valid);
    ASSERT_EQ(score.id_score, 15);   /* Full ID points */
    ASSERT_TRUE(score.total >= 80);  /* Should be high */
}

TEST(score_crc_error_sector) {
    uft_decode_score_t score;
    
    uft_score_sector(&score,
                     false,      /* CRC BAD */
                     5, 0, 3,
                     80, 18,
                     50.0, 500.0,
                     false, false);
    
    ASSERT_FALSE(score.crc_ok);
    ASSERT_EQ(score.crc_score, 0);   /* No CRC points */
    ASSERT_TRUE(score.total < 60);   /* Should be lower */
}

TEST(score_invalid_id) {
    uft_decode_score_t score;
    
    /* Invalid cylinder (too high) */
    uft_score_sector(&score,
                     true,
                     100, 0, 3,  /* Cylinder 100 > max 80 */
                     80, 18,
                     50.0, 500.0,
                     false, false);
    
    ASSERT_FALSE(score.id_valid);
    ASSERT_EQ(score.id_score, 0);
}

TEST(score_timing_quality) {
    uft_decode_score_t score1, score2;
    
    /* Good timing (low jitter) */
    uft_score_sector(&score1,
                     true, 0, 0, 1, 80, 18,
                     10.0,       /* Very low jitter */
                     500.0,
                     false, false);
    
    /* Poor timing (high jitter) */
    uft_score_sector(&score2,
                     true, 0, 0, 1, 80, 18,
                     400.0,      /* High jitter */
                     500.0,
                     false, false);
    
    ASSERT_TRUE(score1.timing_score > score2.timing_score);
}

TEST(score_protection_expected) {
    uft_decode_score_t score1, score2;
    
    /* Protection expected and found */
    uft_score_sector(&score1,
                     true, 0, 0, 1, 80, 18,
                     50.0, 500.0,
                     true,       /* Protection expected */
                     true);      /* Protection found */
    
    /* Protection expected but not found */
    uft_score_sector(&score2,
                     true, 0, 0, 1, 80, 18,
                     50.0, 500.0,
                     true,       /* Protection expected */
                     false);     /* Not found */
    
    ASSERT_TRUE(score1.protection_score > score2.protection_score);
}

TEST(score_unexpected_protection) {
    uft_decode_score_t score1, score2;
    
    /* No protection expected, none found */
    uft_score_sector(&score1,
                     true, 0, 0, 1, 80, 18,
                     50.0, 500.0,
                     false,      /* Not expected */
                     false);     /* Not found - good */
    
    /* No protection expected, but found */
    uft_score_sector(&score2,
                     true, 0, 0, 1, 80, 18,
                     50.0, 500.0,
                     false,      /* Not expected */
                     true);      /* Found - unexpected */
    
    /* Finding unexpected protection should give slight penalty */
    ASSERT_TRUE(score1.protection_score >= score2.protection_score);
}

/*============================================================================
 * TESTS: TOTAL CALCULATION
 *============================================================================*/

TEST(score_calculate_total) {
    uft_decode_score_t score = {0};
    
    score.crc_score = 40;
    score.id_score = 15;
    score.sequence_score = 15;
    score.header_score = 10;
    score.timing_score = 15;
    score.protection_score = 5;
    
    uft_score_calculate_total(&score);
    
    ASSERT_EQ(score.total, 100);  /* Max possible */
}

TEST(score_calculate_total_capped) {
    uft_decode_score_t score = {0};
    
    /* Artificially set components too high */
    score.crc_score = 50;
    score.id_score = 30;
    score.sequence_score = 30;
    score.header_score = 20;
    score.timing_score = 20;
    score.protection_score = 10;
    
    uft_score_calculate_total(&score);
    
    ASSERT_EQ(score.total, 100);  /* Should be capped at 100 */
}

/*============================================================================
 * TESTS: COMPARISON
 *============================================================================*/

TEST(score_compare_total) {
    uft_decode_score_t a = {0}, b = {0};
    
    a.total = 80;
    b.total = 60;
    
    ASSERT_TRUE(uft_score_compare(&a, &b) > 0);  /* a > b */
    ASSERT_TRUE(uft_score_compare(&b, &a) < 0);  /* b < a */
}

TEST(score_compare_crc_tiebreak) {
    uft_decode_score_t a = {0}, b = {0};
    
    a.total = 75;
    a.crc_ok = true;
    
    b.total = 75;
    b.crc_ok = false;
    
    ASSERT_TRUE(uft_score_compare(&a, &b) > 0);  /* CRC OK wins tie */
}

TEST(score_compare_confidence_tiebreak) {
    uft_decode_score_t a = {0}, b = {0};
    
    a.total = 75;
    a.crc_ok = false;
    a.confidence = 90;
    
    b.total = 75;
    b.crc_ok = false;
    b.confidence = 70;
    
    ASSERT_TRUE(uft_score_compare(&a, &b) > 0);  /* Higher confidence wins */
}

TEST(score_compare_null) {
    uft_decode_score_t a = {0};
    
    ASSERT_EQ(uft_score_compare(NULL, &a), 0);
    ASSERT_EQ(uft_score_compare(&a, NULL), 0);
    ASSERT_EQ(uft_score_compare(NULL, NULL), 0);
}

/*============================================================================
 * TESTS: STRING CONVERSION
 *============================================================================*/

TEST(score_to_string) {
    uft_decode_score_t score;
    
    uft_score_sector(&score,
                     true, 5, 0, 3, 80, 18,
                     50.0, 500.0,
                     false, false);
    
    const char *str = uft_score_to_string(&score);
    ASSERT_NOT_NULL(str);
    
    /* Should contain key info */
    ASSERT_TRUE(strstr(str, "Score") != NULL);
    ASSERT_TRUE(strstr(str, "CRC") != NULL);
}

TEST(score_to_string_null) {
    const char *str = uft_score_to_string(NULL);
    ASSERT_NOT_NULL(str);  /* Should return "NULL" string */
}

/*============================================================================
 * TESTS: DEFAULT WEIGHTS
 *============================================================================*/

TEST(default_weights) {
    /* Verify default weights sum to 100 */
    int total = UFT_SCORE_WEIGHTS_DEFAULT.crc_weight +
                UFT_SCORE_WEIGHTS_DEFAULT.id_weight +
                UFT_SCORE_WEIGHTS_DEFAULT.sequence_weight +
                UFT_SCORE_WEIGHTS_DEFAULT.header_weight +
                UFT_SCORE_WEIGHTS_DEFAULT.timing_weight +
                UFT_SCORE_WEIGHTS_DEFAULT.protection_weight;
    
    ASSERT_EQ(total, 100);
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT Decode Score Tests (W-P1-003)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    /* Initialization */
    printf("[SUITE] Initialization\n");
    RUN_TEST(score_init);
    RUN_TEST(score_init_null);
    
    /* Score Calculation */
    printf("\n[SUITE] Score Calculation\n");
    RUN_TEST(score_perfect_sector);
    RUN_TEST(score_crc_error_sector);
    RUN_TEST(score_invalid_id);
    RUN_TEST(score_timing_quality);
    RUN_TEST(score_protection_expected);
    RUN_TEST(score_unexpected_protection);
    
    /* Total Calculation */
    printf("\n[SUITE] Total Calculation\n");
    RUN_TEST(score_calculate_total);
    RUN_TEST(score_calculate_total_capped);
    
    /* Comparison */
    printf("\n[SUITE] Comparison\n");
    RUN_TEST(score_compare_total);
    RUN_TEST(score_compare_crc_tiebreak);
    RUN_TEST(score_compare_confidence_tiebreak);
    RUN_TEST(score_compare_null);
    
    /* String Conversion */
    printf("\n[SUITE] String Conversion\n");
    RUN_TEST(score_to_string);
    RUN_TEST(score_to_string_null);
    
    /* Default Weights */
    printf("\n[SUITE] Default Weights\n");
    RUN_TEST(default_weights);
    
    /* Summary */
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n", 
           tests_passed, tests_run - tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
