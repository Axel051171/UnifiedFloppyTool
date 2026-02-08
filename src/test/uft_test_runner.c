/**
 * @file uft_test_runner.c
 * @brief UFT Test Runner Implementation
 */

#include "uft/test/uft_test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// TIMING
// ============================================================================

static uint64_t get_time_us(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000000 / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#endif
}

// ============================================================================
// ERROR CLASS NAMES
// ============================================================================

static const char* error_class_name(int bit) {
    static const char* names[] = {
        "CRC_WRONG", "DATA_LOSS", "BIT_FLIP", "ENCODING_ERROR",
        "FORMAT_WRONG", "VARIANT_WRONG", "CONVERT_LOSS", "(reserved)",
        "OOB_READ", "OOB_WRITE", "INT_OVERFLOW", "NULL_DEREF",
        "USE_AFTER_FREE", "DOUBLE_FREE", "DIV_BY_ZERO", "(reserved)",
        "MEMORY_LEAK", "TIMEOUT", "RESOURCE_EXHAUST"
    };
    if (bit < 0 || bit >= (int)(sizeof(names)/sizeof(names[0]))) return "UNKNOWN";
    return names[bit];
}

// ============================================================================
// TEST RUNNER
// ============================================================================

int uft_test_run_suite(uft_test_suite_t *suite, uft_test_category_e filter,
                       uft_test_stats_t *stats) {
    if (!suite || !stats) return -1;
    
    memset(stats, 0, sizeof(*stats));
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("  TEST SUITE: %s\n", suite->name);
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    // Suite setup
    if (suite->suite_setup) {
        if (suite->suite_setup(NULL) != 0) {
            printf("❌ Suite setup failed!\n");
            return -1;
        }
    }
    
    // Run tests
    for (int i = 0; i < suite->test_count; i++) {
        uft_test_case_t *test = &suite->tests[i];
        
        // Filter check
        if (filter != UFT_TEST_CAT_ALL && !(test->category & filter)) {
            continue;
        }
        
        stats->total++;
        
        // Track error class coverage
        for (int b = 0; b < 32; b++) {
            if (test->error_class & (1u << b)) {
                stats->error_class_tested[b]++;
            }
        }
        
        printf("  %-50s ", test->name);
        fflush(stdout);
        
        // Setup
        if (test->setup) {
            if (test->setup(test, NULL) != 0) {
                printf("⚠️  SETUP FAILED\n");
                stats->errors++;
                continue;
            }
        }
        
        // Run with timing
        uint64_t start = get_time_us();
        test->last_result = test->run(test, NULL);
        test->last_duration_us = get_time_us() - start;
        stats->total_duration_us += test->last_duration_us;
        
        // Teardown
        if (test->teardown) {
            test->teardown(test, NULL);
        }
        
        // Report result
        switch (test->last_result) {
            case UFT_TEST_PASS:
                printf("✅ PASS (%lu µs)\n", (unsigned long)test->last_duration_us);
                stats->passed++;
                // Track error class coverage
                for (int b = 0; b < 32; b++) {
                    if (test->error_class & (1u << b)) {
                        stats->error_class_passed[b]++;
                    }
                }
                break;
            case UFT_TEST_FAIL:
                printf("❌ FAIL: %s\n", test->failure_message);
                stats->failed++;
                break;
            case UFT_TEST_SKIP:
                printf("⏭️  SKIP\n");
                stats->skipped++;
                break;
            case UFT_TEST_ERROR:
                printf("⚠️  ERROR: %s\n", test->failure_message);
                stats->errors++;
                break;
            case UFT_TEST_TIMEOUT:
                printf("⏱️  TIMEOUT\n");
                stats->timeouts++;
                break;
        }
    }
    
    // Suite teardown
    if (suite->suite_teardown) {
        suite->suite_teardown(NULL);
    }
    
    return stats->failed + stats->errors + stats->timeouts;
}

// ============================================================================
// RESULTS PRINTER
// ============================================================================

void uft_test_print_results(const uft_test_stats_t *stats) {
    printf("\n");
    printf("───────────────────────────────────────────────────────────────────────────────\n");
    printf("                              TEST RESULTS\n");
    printf("───────────────────────────────────────────────────────────────────────────────\n");
    printf("\n");
    printf("  Total:     %d\n", stats->total);
    printf("  Passed:    %d  (%d%%)\n", stats->passed, 
           stats->total ? (stats->passed * 100 / stats->total) : 0);
    printf("  Failed:    %d\n", stats->failed);
    printf("  Skipped:   %d\n", stats->skipped);
    printf("  Errors:    %d\n", stats->errors);
    printf("  Timeouts:  %d\n", stats->timeouts);
    printf("  Duration:  %.2f ms\n", stats->total_duration_us / 1000.0);
    printf("\n");
    
    if (stats->failed + stats->errors + stats->timeouts == 0) {
        printf("  ╔═══════════════════════════════════════════════════════════════╗\n");
        printf("  ║                      ALL TESTS PASSED!                        ║\n");
        printf("  ╚═══════════════════════════════════════════════════════════════╝\n");
    } else {
        printf("  ╔═══════════════════════════════════════════════════════════════╗\n");
        printf("  ║                      TESTS FAILED!                            ║\n");
        printf("  ╚═══════════════════════════════════════════════════════════════╝\n");
    }
}

// ============================================================================
// ERROR CLASS COVERAGE
// ============================================================================

void uft_test_print_error_class_coverage(const uft_test_stats_t *stats) {
    printf("\n");
    printf("───────────────────────────────────────────────────────────────────────────────\n");
    printf("                         ERROR CLASS COVERAGE\n");
    printf("───────────────────────────────────────────────────────────────────────────────\n");
    printf("\n");
    printf("  %-20s │ %-8s │ %-8s │ %-10s\n", "Error Class", "Tested", "Passed", "Coverage");
    printf("  ────────────────────┼──────────┼──────────┼───────────\n");
    
    for (int b = 0; b < 19; b++) {
        if (stats->error_class_tested[b] > 0) {
            int pct = stats->error_class_passed[b] * 100 / stats->error_class_tested[b];
            const char *bar = (pct == 100) ? "██████████" :
                              (pct >= 80)  ? "████████░░" :
                              (pct >= 60)  ? "██████░░░░" :
                              (pct >= 40)  ? "████░░░░░░" :
                              (pct >= 20)  ? "██░░░░░░░░" : "░░░░░░░░░░";
            printf("  %-20s │ %8d │ %8d │ %s %d%%\n",
                   error_class_name(b),
                   stats->error_class_tested[b],
                   stats->error_class_passed[b],
                   bar, pct);
        }
    }
    printf("\n");
}
