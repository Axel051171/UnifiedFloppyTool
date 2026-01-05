/**
 * @file test_confidence.c
 * @brief Unit tests for uft_confidence.h
 */

#include "uft/detection/uft_confidence.h"
#include <stdio.h>
#include <string.h>

#define TEST(name) static int test_##name(void)
#define RUN(name) do { \
    printf("  TEST: %-40s ", #name); \
    if (test_##name() == 0) { printf("[PASS]\n"); passed++; } \
    else { printf("[FAIL]\n"); failed++; } \
    total++; \
} while(0)

static int passed = 0, failed = 0, total = 0;

TEST(confidence_names) {
    if (!uft_confidence_name(UFT_CONF_UNKNOWN)) return -1;
    if (!uft_confidence_name(UFT_CONF_GUESS)) return -1;
    if (!uft_confidence_name(UFT_CONF_POSSIBLE)) return -1;
    if (!uft_confidence_name(UFT_CONF_LIKELY)) return -1;
    if (!uft_confidence_name(UFT_CONF_CERTAIN)) return -1;
    if (!uft_confidence_name(UFT_CONF_VERIFIED)) return -1;
    return 0;
}

TEST(score_to_level) {
    if (uft_score_to_level(0.0) != UFT_CONF_UNKNOWN) return -1;
    if (uft_score_to_level(0.1) != UFT_CONF_GUESS) return -1;
    if (uft_score_to_level(0.3) != UFT_CONF_GUESS) return -1;
    if (uft_score_to_level(0.5) != UFT_CONF_POSSIBLE) return -1;
    if (uft_score_to_level(0.75) != UFT_CONF_LIKELY) return -1;
    if (uft_score_to_level(0.9) != UFT_CONF_CERTAIN) return -1;
    if (uft_score_to_level(1.0) != UFT_CONF_VERIFIED) return -1;
    return 0;
}

TEST(detect_options_default) {
    uft_detect_options_t opts;
    uft_detect_options_default(&opts);
    
    if (opts.max_candidates < 1) return -1;
    if (opts.max_candidates > UFT_MAX_CANDIDATES) return -1;
    if (opts.min_confidence < 0 || opts.min_confidence > 1) return -1;
    
    return 0;
}

TEST(evidence_names) {
    if (!uft_evidence_name(UFT_EVIDENCE_MAGIC)) return -1;
    if (!uft_evidence_name(UFT_EVIDENCE_SIZE)) return -1;
    if (!uft_evidence_name(UFT_EVIDENCE_EXTENSION)) return -1;
    if (!uft_evidence_name(UFT_EVIDENCE_HEADER)) return -1;
    if (!uft_evidence_name(UFT_EVIDENCE_CHECKSUM)) return -1;
    return 0;
}

TEST(build_reason) {
    char buf[256];
    
    uft_build_reason(UFT_EVIDENCE_MAGIC | UFT_EVIDENCE_SIZE, buf, sizeof(buf));
    if (strlen(buf) == 0) return -1;
    if (!strstr(buf, "magic") && !strstr(buf, "Magic")) return -1;
    
    return 0;
}

TEST(detect_empty_data) {
    uft_detect_result_t result;
    memset(&result, 0, sizeof(result));
    
    uft_detect_options_t opts;
    uft_detect_options_default(&opts);
    
    // Empty data should not crash, return no candidates
    int ret = uft_detect(NULL, 0, NULL, &opts, &result);
    
    // Either error return or 0 candidates is acceptable
    if (ret != 0 && result.candidate_count != 0) {
        // Only fail if we got invalid results
        if (result.best_index >= result.candidate_count) return -1;
    }
    
    return 0;
}

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         UFT CONFIDENCE API TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    RUN(confidence_names);
    RUN(score_to_level);
    RUN(detect_options_default);
    RUN(evidence_names);
    RUN(build_reason);
    RUN(detect_empty_data);
    
    printf("\n═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         RESULTS: %d/%d passed, %d failed\n", passed, total, failed);
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    return (failed == 0) ? 0 : 1;
}
