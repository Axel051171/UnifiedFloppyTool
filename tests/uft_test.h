/**
 * @file uft_test.h
 * @brief Minimal Unit Test Framework für UFT
 */

#ifndef UFT_TEST_H
#define UFT_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Test Counters
static int uft_tests_run = 0;
static int uft_tests_passed = 0;
static int uft_tests_failed = 0;

// Colors
#define UFT_GREEN  "\033[32m"
#define UFT_RED    "\033[31m"
#define UFT_YELLOW "\033[33m"
#define UFT_RESET  "\033[0m"

// Test Macros
#define UFT_TEST(name) static void test_##name(void)

#define UFT_RUN_TEST(name) do { \
    printf("  Testing " #name "..."); \
    fflush(stdout); \
    uft_tests_run++; \
    test_##name(); \
} while(0)

#define UFT_ASSERT(cond) do { \
    if (!(cond)) { \
        printf(UFT_RED " FAIL" UFT_RESET " (%s:%d)\n", __FILE__, __LINE__); \
        uft_tests_failed++; \
        return; \
    } \
} while(0)

#define UFT_ASSERT_EQ(a, b) UFT_ASSERT((a) == (b))
#define UFT_ASSERT_NE(a, b) UFT_ASSERT((a) != (b))
#define UFT_ASSERT_NULL(p)  UFT_ASSERT((p) == NULL)
#define UFT_ASSERT_NOT_NULL(p) UFT_ASSERT((p) != NULL)
#define UFT_ASSERT_STR_EQ(a, b) UFT_ASSERT(strcmp((a), (b)) == 0)

#define UFT_PASS() do { \
    printf(UFT_GREEN " PASS" UFT_RESET "\n"); \
    uft_tests_passed++; \
} while(0)

#define UFT_TEST_SUITE(name) \
    printf("\n" UFT_YELLOW "=== " name " ===" UFT_RESET "\n")

#define UFT_TEST_SUMMARY() do { \
    printf("\n═══════════════════════════════════════════════════════════════\n"); \
    printf("Tests: %d | ", uft_tests_run); \
    printf(UFT_GREEN "Passed: %d" UFT_RESET " | ", uft_tests_passed); \
    if (uft_tests_failed > 0) \
        printf(UFT_RED "Failed: %d" UFT_RESET, uft_tests_failed); \
    else \
        printf("Failed: 0"); \
    printf("\n═══════════════════════════════════════════════════════════════\n"); \
} while(0)

#define UFT_TEST_EXIT() return (uft_tests_failed > 0) ? 1 : 0

#endif // UFT_TEST_H
