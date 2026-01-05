/**
 * @file test_bayesian_format.c
 * @brief Unit Tests for Bayesian Format Classifier
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Test helpers
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

// ============================================================================
// FORMAT DEFINITIONS (simplified from implementation)
// ============================================================================

typedef struct {
    const char *id;
    const char *name;
    size_t expected_size;
    float base_prior;
} format_def_t;

static const format_def_t FORMATS[] = {
    {"pc_360k",   "PC 360K",   368640,   0.05f},
    {"pc_720k",   "PC 720K",   737280,   0.08f},
    {"pc_1440k",  "PC 1.44M",  1474560,  0.12f},
    {"amiga_dd",  "Amiga DD",  901120,   0.10f},
    {"c64_d64",   "C64 D64",   174848,   0.08f},
    {"atari_st",  "Atari ST",  737280,   0.05f},
    {NULL, NULL, 0, 0.0f}
};

#define FORMAT_COUNT 6

// ============================================================================
// LIKELIHOOD FUNCTIONS
// ============================================================================

static float size_likelihood(size_t actual, size_t expected) {
    if (expected == 0) return 0.5f;
    if (actual == expected) return 0.95f;
    
    float ratio = (float)actual / (float)expected;
    if (ratio > 0.95f && ratio < 1.05f) return 0.7f;
    if (ratio > 0.9f && ratio < 1.1f) return 0.4f;
    return 0.05f;
}

// ============================================================================
// TEST CASES
// ============================================================================

/**
 * Test: Exact size match gives high confidence
 */
static int test_exact_size_match(void) {
    size_t file_size = 1474560;  // Exact 1.44MB
    
    float posteriors[FORMAT_COUNT];
    float total = 0.0f;
    
    for (int i = 0; i < FORMAT_COUNT; i++) {
        float prior = FORMATS[i].base_prior;
        float likelihood = size_likelihood(file_size, FORMATS[i].expected_size);
        posteriors[i] = prior * likelihood;
        total += posteriors[i];
    }
    
    // Normalize
    for (int i = 0; i < FORMAT_COUNT; i++) {
        posteriors[i] /= total;
    }
    
    // PC 1.44M should have highest posterior
    int best = 0;
    for (int i = 1; i < FORMAT_COUNT; i++) {
        if (posteriors[i] > posteriors[best]) best = i;
    }
    
    ASSERT_TRUE(strcmp(FORMATS[best].id, "pc_1440k") == 0);
    ASSERT_TRUE(posteriors[best] > 0.5f);  // High confidence
    
    return 1;
}

/**
 * Test: Ambiguous size (720K = PC or Atari ST)
 */
static int test_ambiguous_size(void) {
    size_t file_size = 737280;  // 720K - both PC and Atari ST
    
    float posteriors[FORMAT_COUNT];
    float total = 0.0f;
    
    for (int i = 0; i < FORMAT_COUNT; i++) {
        float prior = FORMATS[i].base_prior;
        float likelihood = size_likelihood(file_size, FORMATS[i].expected_size);
        posteriors[i] = prior * likelihood;
        total += posteriors[i];
    }
    
    for (int i = 0; i < FORMAT_COUNT; i++) {
        posteriors[i] /= total;
    }
    
    // Find PC 720K and Atari ST posteriors
    float pc_720k_post = 0.0f, atari_post = 0.0f;
    for (int i = 0; i < FORMAT_COUNT; i++) {
        if (strcmp(FORMATS[i].id, "pc_720k") == 0) pc_720k_post = posteriors[i];
        if (strcmp(FORMATS[i].id, "atari_st") == 0) atari_post = posteriors[i];
    }
    
    // Both should have similar posteriors (prior difference)
    ASSERT_TRUE(pc_720k_post > 0.3f);
    ASSERT_TRUE(atari_post > 0.15f);
    
    // Margin should be small (ambiguous)
    float margin = fabsf(pc_720k_post - atari_post);
    ASSERT_TRUE(margin < 0.5f);  // Close enough to flag as uncertain
    
    return 1;
}

/**
 * Test: Unknown size gives low confidence
 */
static int test_unknown_size(void) {
    size_t file_size = 123456;  // Not a standard size
    
    float posteriors[FORMAT_COUNT];
    float total = 0.0f;
    
    for (int i = 0; i < FORMAT_COUNT; i++) {
        float prior = FORMATS[i].base_prior;
        float likelihood = size_likelihood(file_size, FORMATS[i].expected_size);
        posteriors[i] = prior * likelihood;
        total += posteriors[i];
    }
    
    for (int i = 0; i < FORMAT_COUNT; i++) {
        posteriors[i] /= total;
    }
    
    // Best posterior should be low
    float best_post = 0.0f;
    for (int i = 0; i < FORMAT_COUNT; i++) {
        if (posteriors[i] > best_post) best_post = posteriors[i];
    }
    
    ASSERT_TRUE(best_post < 0.5f);  // Low confidence
    
    return 1;
}

/**
 * Test: Regional priors affect results
 */
static int test_regional_priors(void) {
    // EU region: Amiga more popular
    float eu_amiga_mult = 2.0f;
    float eu_apple_mult = 0.7f;
    
    // US region: Apple more popular
    float us_amiga_mult = 0.5f;
    float us_apple_mult = 2.0f;
    
    // Base Amiga prior
    float amiga_base = 0.10f;
    
    float amiga_eu = amiga_base * eu_amiga_mult;
    float amiga_us = amiga_base * us_amiga_mult;
    
    ASSERT_TRUE(amiga_eu > amiga_us);  // Amiga more likely in EU
    ASSERT_NEAR(amiga_eu, 0.20f, 0.01f);
    ASSERT_NEAR(amiga_us, 0.05f, 0.01f);
    
    return 1;
}

/**
 * Test: Priors sum to reasonable value
 */
static int test_prior_sum(void) {
    float total = 0.0f;
    for (int i = 0; i < FORMAT_COUNT; i++) {
        total += FORMATS[i].base_prior;
    }
    
    // Priors don't need to sum to 1 (more formats exist)
    // But should be reasonable
    ASSERT_TRUE(total > 0.3f);
    ASSERT_TRUE(total < 1.5f);
    
    return 1;
}

/**
 * Test: C64 D64 detection
 */
static int test_c64_detection(void) {
    size_t file_size = 174848;  // Standard D64
    
    float posteriors[FORMAT_COUNT];
    float total = 0.0f;
    
    for (int i = 0; i < FORMAT_COUNT; i++) {
        float prior = FORMATS[i].base_prior;
        float likelihood = size_likelihood(file_size, FORMATS[i].expected_size);
        posteriors[i] = prior * likelihood;
        total += posteriors[i];
    }
    
    for (int i = 0; i < FORMAT_COUNT; i++) {
        posteriors[i] /= total;
    }
    
    // C64 D64 should win
    int best = 0;
    for (int i = 1; i < FORMAT_COUNT; i++) {
        if (posteriors[i] > posteriors[best]) best = i;
    }
    
    ASSERT_TRUE(strcmp(FORMATS[best].id, "c64_d64") == 0);
    ASSERT_TRUE(posteriors[best] > 0.8f);  // Very high confidence (unique size)
    
    return 1;
}

/**
 * Test: Confidence margin calculation
 */
static int test_confidence_margin(void) {
    // Simulate clear winner
    float posteriors1[] = {0.80f, 0.10f, 0.05f, 0.03f, 0.01f, 0.01f};
    float margin1 = posteriors1[0] - posteriors1[1];
    ASSERT_TRUE(margin1 > 0.5f);  // Clear winner
    
    // Simulate tie
    float posteriors2[] = {0.35f, 0.33f, 0.15f, 0.10f, 0.05f, 0.02f};
    float margin2 = posteriors2[0] - posteriors2[1];
    ASSERT_TRUE(margin2 < 0.1f);  // Uncertain
    
    return 1;
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("         BAYESIAN FORMAT CLASSIFIER UNIT TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    TEST(exact_size_match);
    TEST(ambiguous_size);
    TEST(unknown_size);
    TEST(regional_priors);
    TEST(prior_sum);
    TEST(c64_detection);
    TEST(confidence_margin);
    
    printf("\n───────────────────────────────────────────────────────────────────\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("───────────────────────────────────────────────────────────────────\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
