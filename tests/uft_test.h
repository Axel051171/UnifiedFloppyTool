#ifndef UFT_TEST_H
#define UFT_TEST_H
/**
 * @file uft_test.h
 * @brief Minimal Unit Test Framework for UFT
 */

#include <stdio.h>
#include <string.h>

static int _uft_tests_run = 0;
static int _uft_tests_failed = 0;
static const char* _uft_current_test = NULL;

#define UFT_TEST(name) \
    static void name(void); \
    static void _run_##name(void) { \
        _uft_current_test = #name; \
        _uft_tests_run++; \
        name(); \
    } \
    static void name(void)

#define UFT_ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        _uft_tests_failed++; \
        return; \
    } \
} while(0)

#define UFT_ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("  FAIL: %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
        _uft_tests_failed++; \
        return; \
    } \
} while(0)

#define UFT_ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("  FAIL: %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
        _uft_tests_failed++; \
        return; \
    } \
} while(0)

#define UFT_RUN_TEST(name) do { \
    printf("  Running %s...", #name); \
    int _before = _uft_tests_failed; \
    _run_##name(); \
    if (_uft_tests_failed == _before) printf(" OK\n"); \
} while(0)

#define UFT_TEST_MAIN_BEGIN() \
    int main(void) { \
        printf("=== UFT Test Suite ===\n\n");

#define UFT_TEST_MAIN_END() \
        printf("\n=== Results: %d/%d passed ===\n", \
               _uft_tests_run - _uft_tests_failed, _uft_tests_run); \
        return _uft_tests_failed > 0 ? 1 : 0; \
    }

#endif /* UFT_TEST_H */
