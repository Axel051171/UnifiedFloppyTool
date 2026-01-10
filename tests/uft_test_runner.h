/**
 * @file uft_test_runner.h
 * @brief UFT Enhanced Test Framework with Structured Logging
 * 
 * Provides structured test output for CI/CD and local development.
 * 
 * Features:
 * - JSON-compatible log output
 * - Test timing
 * - Platform/build info capture
 * - Failure summaries
 * 
 * @copyright UFT Project 2025
 */

#ifndef UFT_TEST_RUNNER_H
#define UFT_TEST_RUNNER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#ifdef _WIN32
    #include <windows.h>
    #define UFT_TEST_PLATFORM "Windows"
#elif defined(__APPLE__)
    #define UFT_TEST_PLATFORM "macOS"
#else
    #define UFT_TEST_PLATFORM "Linux"
#endif

#ifdef __x86_64__
    #define UFT_TEST_ARCH "x64"
#elif defined(__aarch64__)
    #define UFT_TEST_ARCH "ARM64"
#else
    #define UFT_TEST_ARCH "Unknown"
#endif

/*============================================================================
 * Test Runner State
 *============================================================================*/

typedef struct {
    const char *suite_name;
    int total_tests;
    int passed_tests;
    int failed_tests;
    int skipped_tests;
    double total_time_ms;
    FILE *log_file;
    int verbose;
    clock_t suite_start;
    clock_t test_start;
} uft_test_runner_t;

static uft_test_runner_t g_runner = {0};

/*============================================================================
 * Color Output (if terminal supports it)
 *============================================================================*/

#define UFT_COLOR_RESET   "\033[0m"
#define UFT_COLOR_RED     "\033[31m"
#define UFT_COLOR_GREEN   "\033[32m"
#define UFT_COLOR_YELLOW  "\033[33m"
#define UFT_COLOR_BLUE    "\033[34m"
#define UFT_COLOR_BOLD    "\033[1m"

static int uft_use_colors(void) {
#ifdef _WIN32
    return 0;  /* Windows console handling is complex */
#else
    const char *term = getenv("TERM");
    return term && strcmp(term, "dumb") != 0;
#endif
}

#define UFT_C(color) (uft_use_colors() ? (color) : "")

/*============================================================================
 * Test Runner API
 *============================================================================*/

/**
 * @brief Initialize test runner
 */
static inline void uft_test_init(const char *suite_name) {
    memset(&g_runner, 0, sizeof(g_runner));
    g_runner.suite_name = suite_name;
    g_runner.verbose = (getenv("UFT_TEST_VERBOSE") != NULL);
    g_runner.suite_start = clock();
    
    /* Print header */
    printf("\n%s════════════════════════════════════════════════════════════%s\n",
           UFT_C(UFT_COLOR_BOLD), UFT_C(UFT_COLOR_RESET));
    printf("%s UFT Test Suite: %s%s\n", 
           UFT_C(UFT_COLOR_BOLD), suite_name, UFT_C(UFT_COLOR_RESET));
    printf("%s════════════════════════════════════════════════════════════%s\n",
           UFT_C(UFT_COLOR_BOLD), UFT_C(UFT_COLOR_RESET));
    printf(" Platform: %s %s\n", UFT_TEST_PLATFORM, UFT_TEST_ARCH);
    printf(" Build:    %s\n", 
#ifdef NDEBUG
           "Release"
#else
           "Debug"
#endif
    );
    printf("════════════════════════════════════════════════════════════\n\n");
    
    /* Open log file if requested */
    const char *log_path = getenv("UFT_TEST_LOG");
    if (log_path) {
        g_runner.log_file = fopen(log_path, "w");
        if (g_runner.log_file) {
            fprintf(g_runner.log_file, "{\n  \"suite\": \"%s\",\n", suite_name);
            fprintf(g_runner.log_file, "  \"platform\": \"%s\",\n", UFT_TEST_PLATFORM);
            fprintf(g_runner.log_file, "  \"arch\": \"%s\",\n", UFT_TEST_ARCH);
            fprintf(g_runner.log_file, "  \"tests\": [\n");
        }
    }
}

/**
 * @brief Start a test case
 */
static inline void uft_test_start(const char *test_name) {
    g_runner.total_tests++;
    g_runner.test_start = clock();
    
    if (g_runner.verbose) {
        printf("  ▶ %s ... ", test_name);
        fflush(stdout);
    }
}

/**
 * @brief Mark test as passed
 */
static inline void uft_test_pass(const char *test_name) {
    g_runner.passed_tests++;
    double elapsed = (double)(clock() - g_runner.test_start) / CLOCKS_PER_SEC * 1000;
    g_runner.total_time_ms += elapsed;
    
    if (g_runner.verbose) {
        printf("%s✓ PASS%s (%.1f ms)\n", 
               UFT_C(UFT_COLOR_GREEN), UFT_C(UFT_COLOR_RESET), elapsed);
    } else {
        printf("%s.%s", UFT_C(UFT_COLOR_GREEN), UFT_C(UFT_COLOR_RESET));
        fflush(stdout);
    }
    
    /* Log to file */
    if (g_runner.log_file) {
        fprintf(g_runner.log_file, 
                "    {\"name\": \"%s\", \"status\": \"pass\", \"time_ms\": %.1f}%s\n",
                test_name, elapsed, 
                (g_runner.total_tests == g_runner.passed_tests + g_runner.failed_tests + g_runner.skipped_tests) ? "" : ",");
    }
}

/**
 * @brief Mark test as failed
 */
static inline void uft_test_fail(const char *test_name, const char *reason, ...) {
    g_runner.failed_tests++;
    double elapsed = (double)(clock() - g_runner.test_start) / CLOCKS_PER_SEC * 1000;
    g_runner.total_time_ms += elapsed;
    
    char reason_buf[512] = "";
    if (reason) {
        va_list args;
        va_start(args, reason);
        vsnprintf(reason_buf, sizeof(reason_buf), reason, args);
        va_end(args);
    }
    
    if (g_runner.verbose) {
        printf("%s✗ FAIL%s (%.1f ms)\n", 
               UFT_C(UFT_COLOR_RED), UFT_C(UFT_COLOR_RESET), elapsed);
        if (reason_buf[0]) {
            printf("    → %s\n", reason_buf);
        }
    } else {
        printf("%sF%s", UFT_C(UFT_COLOR_RED), UFT_C(UFT_COLOR_RESET));
        fflush(stdout);
    }
    
    /* Log to file */
    if (g_runner.log_file) {
        fprintf(g_runner.log_file, 
                "    {\"name\": \"%s\", \"status\": \"fail\", \"time_ms\": %.1f, \"reason\": \"%s\"},\n",
                test_name, elapsed, reason_buf);
    }
}

/**
 * @brief Mark test as skipped
 */
static inline void uft_test_skip(const char *test_name, const char *reason) {
    g_runner.skipped_tests++;
    
    if (g_runner.verbose) {
        printf("%s⊘ SKIP%s", UFT_C(UFT_COLOR_YELLOW), UFT_C(UFT_COLOR_RESET));
        if (reason) printf(" (%s)", reason);
        printf("\n");
    } else {
        printf("%sS%s", UFT_C(UFT_COLOR_YELLOW), UFT_C(UFT_COLOR_RESET));
        fflush(stdout);
    }
    
    /* Log to file */
    if (g_runner.log_file) {
        fprintf(g_runner.log_file, 
                "    {\"name\": \"%s\", \"status\": \"skip\", \"reason\": \"%s\"},\n",
                test_name, reason ? reason : "");
    }
}

/**
 * @brief Finalize and print summary
 * @return 0 if all passed, 1 if any failed
 */
static inline int uft_test_finish(void) {
    double suite_time = (double)(clock() - g_runner.suite_start) / CLOCKS_PER_SEC * 1000;
    
    /* Print newline if not verbose (dots mode) */
    if (!g_runner.verbose) {
        printf("\n\n");
    }
    
    /* Print summary */
    printf("════════════════════════════════════════════════════════════\n");
    printf(" %s%s: %d tests%s\n",
           UFT_C(UFT_COLOR_BOLD), g_runner.suite_name, 
           g_runner.total_tests, UFT_C(UFT_COLOR_RESET));
    printf("────────────────────────────────────────────────────────────\n");
    printf(" %sPassed:%s  %d\n", UFT_C(UFT_COLOR_GREEN), UFT_C(UFT_COLOR_RESET), g_runner.passed_tests);
    if (g_runner.failed_tests > 0) {
        printf(" %sFailed:%s  %d\n", UFT_C(UFT_COLOR_RED), UFT_C(UFT_COLOR_RESET), g_runner.failed_tests);
    }
    if (g_runner.skipped_tests > 0) {
        printf(" %sSkipped:%s %d\n", UFT_C(UFT_COLOR_YELLOW), UFT_C(UFT_COLOR_RESET), g_runner.skipped_tests);
    }
    printf(" Time:    %.1f ms\n", suite_time);
    printf("════════════════════════════════════════════════════════════\n");
    
    /* Final status */
    if (g_runner.failed_tests == 0) {
        printf(" %s✓ ALL TESTS PASSED%s\n", 
               UFT_C(UFT_COLOR_GREEN), UFT_C(UFT_COLOR_RESET));
    } else {
        printf(" %s✗ %d TEST(S) FAILED%s\n", 
               UFT_C(UFT_COLOR_RED), g_runner.failed_tests, UFT_C(UFT_COLOR_RESET));
    }
    printf("════════════════════════════════════════════════════════════\n\n");
    
    /* Close log file */
    if (g_runner.log_file) {
        fprintf(g_runner.log_file, "  ],\n");
        fprintf(g_runner.log_file, "  \"summary\": {\n");
        fprintf(g_runner.log_file, "    \"total\": %d,\n", g_runner.total_tests);
        fprintf(g_runner.log_file, "    \"passed\": %d,\n", g_runner.passed_tests);
        fprintf(g_runner.log_file, "    \"failed\": %d,\n", g_runner.failed_tests);
        fprintf(g_runner.log_file, "    \"skipped\": %d,\n", g_runner.skipped_tests);
        fprintf(g_runner.log_file, "    \"time_ms\": %.1f\n", suite_time);
        fprintf(g_runner.log_file, "  }\n}\n");
        fclose(g_runner.log_file);
    }
    
    return (g_runner.failed_tests > 0) ? 1 : 0;
}

/*============================================================================
 * Assertion Macros
 *============================================================================*/

#define UFT_TEST_BEGIN(suite) uft_test_init(suite)
#define UFT_TEST_END()        return uft_test_finish()

#define UFT_RUN_TEST(test_func) do { \
    uft_test_start(#test_func); \
    if (test_func()) { \
        uft_test_pass(#test_func); \
    } \
} while(0)

#define UFT_ASSERT(cond) do { \
    if (!(cond)) { \
        uft_test_fail(__func__, "Assertion failed: %s (line %d)", #cond, __LINE__); \
        return 0; \
    } \
} while(0)

#define UFT_ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        uft_test_fail(__func__, "Expected %s == %s (line %d)", #a, #b, __LINE__); \
        return 0; \
    } \
} while(0)

#define UFT_ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        uft_test_fail(__func__, "Expected \"%s\" == \"%s\" (line %d)", (a), (b), __LINE__); \
        return 0; \
    } \
} while(0)

#define UFT_ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        uft_test_fail(__func__, "%s is NULL (line %d)", #ptr, __LINE__); \
        return 0; \
    } \
} while(0)

#endif /* UFT_TEST_RUNNER_H */
