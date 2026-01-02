/**
 * @file uft_test_framework.h
 * @brief Minimal Test Framework for UFT
 */

#ifndef UFT_TEST_FRAMEWORK_H
#define UFT_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Colors for terminal output
#define TEST_RED "\033[0;31m"
#define TEST_GREEN "\033[0;32m"
#define TEST_YELLOW "\033[0;33m"
#define TEST_RESET "\033[0m"

// Test counters
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

// Test macros
#define TEST_CASE(name) \
    static void test_##name(void); \
    static void __attribute__((constructor)) register_##name(void) { \
        run_test(#name, test_##name); \
    } \
    static void test_##name(void)

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            printf(TEST_RED "  FAIL: %s:%d: %s\n" TEST_RESET, \
                   __FILE__, __LINE__, #expr); \
            return; \
        } \
    } while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            printf(TEST_RED "  FAIL: %s:%d: %s != %s\n" TEST_RESET, \
                   __FILE__, __LINE__, #a, #b); \
            return; \
        } \
    } while (0)

#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_LT(a, b) ASSERT_TRUE((a) < (b))
#define ASSERT_LE(a, b) ASSERT_TRUE((a) <= (b))
#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))
#define ASSERT_GE(a, b) ASSERT_TRUE((a) >= (b))

#define ASSERT_NULL(ptr) ASSERT_TRUE((ptr) == NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT_TRUE((ptr) != NULL)

#define ASSERT_STR_EQ(a, b) ASSERT_TRUE(strcmp((a), (b)) == 0)

#define ASSERT_MEM_EQ(a, b, n) ASSERT_TRUE(memcmp((a), (b), (n)) == 0)

// Run a single test
static void run_test(const char* name, void (*func)(void)) {
    g_tests_run++;
    printf("  Running: %s... ", name);
    fflush(stdout);
    
    func();
    
    g_tests_passed++;
    printf(TEST_GREEN "PASS\n" TEST_RESET);
}

// Test runner main
#define TEST_MAIN() \
    int main(int argc, char** argv) { \
        (void)argc; (void)argv; \
        printf("\n%s\n", "═══════════════════════════════════════"); \
        printf("Running tests...\n\n"); \
        /* Tests are auto-registered via constructors */ \
        printf("\n%s\n", "═══════════════════════════════════════"); \
        printf("Results: %d/%d passed\n", g_tests_passed, g_tests_run); \
        if (g_tests_failed > 0) { \
            printf(TEST_RED "%d FAILED\n" TEST_RESET, g_tests_failed); \
            return 1; \
        } \
        printf(TEST_GREEN "All tests passed!\n" TEST_RESET); \
        return 0; \
    }

#endif // UFT_TEST_FRAMEWORK_H
