/**
 * @file uft_assert.h
 * @brief Snowhouse-inspired Fluent Assertion Framework for C
 * 
 * Inspired by banditcpp/snowhouse but implemented in pure C99.
 * 
 * Usage:
 *   UFT_ASSERT_THAT(value, IS_EQUAL_TO(expected));
 *   UFT_ASSERT_THAT(ptr, IS_NOT_NULL);
 *   UFT_ASSERT_THAT(str, CONTAINS("hello"));
 *   UFT_ASSERT_THAT(arr, HAS_LENGTH(10));
 */

#ifndef UFT_ASSERT_H
#define UFT_ASSERT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Context
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    int         passed;
    int         failed;
    int         skipped;
    const char  *current_test;
    const char  *current_file;
    int         current_line;
    bool        verbose;
} uft_test_ctx_t;

/* Global test context */
static uft_test_ctx_t _uft_test_ctx = {0, 0, 0, NULL, NULL, 0, true};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Result Reporting
 * ═══════════════════════════════════════════════════════════════════════════════ */

static inline void _uft_assert_pass(const char *expr) {
    _uft_test_ctx.passed++;
    if (_uft_test_ctx.verbose) {
        printf("    ✓ %s\n", expr);
    }
}

static inline void _uft_assert_fail(const char *expr, const char *expected, 
                                     const char *actual, const char *file, int line) {
    _uft_test_ctx.failed++;
    printf("    ✗ %s\n", expr);
    printf("      Expected: %s\n", expected);
    printf("      Actual:   %s\n", actual);
    printf("      Location: %s:%d\n", file, line);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Core Assertion Macro
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_ASSERT_THAT(actual, matcher) \
    do { \
        _uft_test_ctx.current_file = __FILE__; \
        _uft_test_ctx.current_line = __LINE__; \
        matcher(actual, #actual, __FILE__, __LINE__); \
    } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Equality Matchers
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define IS_EQUAL_TO(expected) _uft_is_equal_to_int, (expected)
#define EQUALS(expected) IS_EQUAL_TO(expected)

static inline void _uft_is_equal_to_int(int actual, int expected, 
                                         const char *expr, const char *file, int line) {
    if (actual == expected) {
        _uft_assert_pass(expr);
    } else {
        char exp_str[64], act_str[64];
        snprintf(exp_str, sizeof(exp_str), "%d", expected);
        snprintf(act_str, sizeof(act_str), "%d", actual);
        _uft_assert_fail(expr, exp_str, act_str, file, line);
    }
}

/* Overloaded for different types using _Generic (C11) or macros */
#define IS_EQUAL_TO_INT(expected) \
    [](int a, const char *e, const char *f, int l) { \
        _uft_is_equal_to_int(a, expected, e, f, l); \
    }

#define UFT_ASSERT_EQ(actual, expected) \
    do { \
        if ((actual) == (expected)) { \
            _uft_assert_pass(#actual " == " #expected); \
        } else { \
            char _exp[64], _act[64]; \
            snprintf(_exp, 64, "%lld", (long long)(expected)); \
            snprintf(_act, 64, "%lld", (long long)(actual)); \
            _uft_assert_fail(#actual " == " #expected, _exp, _act, __FILE__, __LINE__); \
        } \
    } while(0)

#define UFT_ASSERT_NE(actual, expected) \
    do { \
        if ((actual) != (expected)) { \
            _uft_assert_pass(#actual " != " #expected); \
        } else { \
            char _val[64]; \
            snprintf(_val, 64, "%lld", (long long)(actual)); \
            _uft_assert_fail(#actual " != " #expected, "not equal", _val, __FILE__, __LINE__); \
        } \
    } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Comparison Matchers
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_ASSERT_GT(actual, expected) \
    do { \
        if ((actual) > (expected)) { \
            _uft_assert_pass(#actual " > " #expected); \
        } else { \
            char _exp[64], _act[64]; \
            snprintf(_exp, 64, "> %lld", (long long)(expected)); \
            snprintf(_act, 64, "%lld", (long long)(actual)); \
            _uft_assert_fail(#actual " > " #expected, _exp, _act, __FILE__, __LINE__); \
        } \
    } while(0)

#define UFT_ASSERT_GE(actual, expected) \
    do { \
        if ((actual) >= (expected)) { \
            _uft_assert_pass(#actual " >= " #expected); \
        } else { \
            char _exp[64], _act[64]; \
            snprintf(_exp, 64, ">= %lld", (long long)(expected)); \
            snprintf(_act, 64, "%lld", (long long)(actual)); \
            _uft_assert_fail(#actual " >= " #expected, _exp, _act, __FILE__, __LINE__); \
        } \
    } while(0)

#define UFT_ASSERT_LT(actual, expected) \
    do { \
        if ((actual) < (expected)) { \
            _uft_assert_pass(#actual " < " #expected); \
        } else { \
            char _exp[64], _act[64]; \
            snprintf(_exp, 64, "< %lld", (long long)(expected)); \
            snprintf(_act, 64, "%lld", (long long)(actual)); \
            _uft_assert_fail(#actual " < " #expected, _exp, _act, __FILE__, __LINE__); \
        } \
    } while(0)

#define UFT_ASSERT_LE(actual, expected) \
    do { \
        if ((actual) <= (expected)) { \
            _uft_assert_pass(#actual " <= " #expected); \
        } else { \
            char _exp[64], _act[64]; \
            snprintf(_exp, 64, "<= %lld", (long long)(expected)); \
            snprintf(_act, 64, "%lld", (long long)(actual)); \
            _uft_assert_fail(#actual " <= " #expected, _exp, _act, __FILE__, __LINE__); \
        } \
    } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Boolean Matchers
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_ASSERT_TRUE(expr) \
    do { \
        if ((expr)) { \
            _uft_assert_pass(#expr " is true"); \
        } else { \
            _uft_assert_fail(#expr " is true", "true", "false", __FILE__, __LINE__); \
        } \
    } while(0)

#define UFT_ASSERT_FALSE(expr) \
    do { \
        if (!(expr)) { \
            _uft_assert_pass(#expr " is false"); \
        } else { \
            _uft_assert_fail(#expr " is false", "false", "true", __FILE__, __LINE__); \
        } \
    } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Pointer Matchers
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            _uft_assert_pass(#ptr " is NULL"); \
        } else { \
            char _act[64]; \
            snprintf(_act, 64, "%p", (void*)(ptr)); \
            _uft_assert_fail(#ptr " is NULL", "NULL", _act, __FILE__, __LINE__); \
        } \
    } while(0)

#define UFT_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            _uft_assert_pass(#ptr " is not NULL"); \
        } else { \
            _uft_assert_fail(#ptr " is not NULL", "not NULL", "NULL", __FILE__, __LINE__); \
        } \
    } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * String Matchers
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_ASSERT_STR_EQ(actual, expected) \
    do { \
        if ((actual) && (expected) && strcmp((actual), (expected)) == 0) { \
            _uft_assert_pass(#actual " equals \"" #expected "\""); \
        } else { \
            _uft_assert_fail(#actual " equals \"" #expected "\"", \
                            (expected) ? (expected) : "(null)", \
                            (actual) ? (actual) : "(null)", __FILE__, __LINE__); \
        } \
    } while(0)

#define UFT_ASSERT_STR_CONTAINS(haystack, needle) \
    do { \
        if ((haystack) && (needle) && strstr((haystack), (needle)) != NULL) { \
            _uft_assert_pass(#haystack " contains \"" #needle "\""); \
        } else { \
            _uft_assert_fail(#haystack " contains \"" #needle "\"", \
                            (needle) ? (needle) : "(null)", \
                            (haystack) ? (haystack) : "(null)", __FILE__, __LINE__); \
        } \
    } while(0)

#define UFT_ASSERT_STR_STARTS_WITH(str, prefix) \
    do { \
        if ((str) && (prefix) && strncmp((str), (prefix), strlen(prefix)) == 0) { \
            _uft_assert_pass(#str " starts with \"" #prefix "\""); \
        } else { \
            _uft_assert_fail(#str " starts with \"" #prefix "\"", \
                            (prefix) ? (prefix) : "(null)", \
                            (str) ? (str) : "(null)", __FILE__, __LINE__); \
        } \
    } while(0)

#define UFT_ASSERT_STR_ENDS_WITH(str, suffix) \
    do { \
        size_t _slen = (str) ? strlen(str) : 0; \
        size_t _xlen = (suffix) ? strlen(suffix) : 0; \
        if (_slen >= _xlen && strcmp((str) + _slen - _xlen, (suffix)) == 0) { \
            _uft_assert_pass(#str " ends with \"" #suffix "\""); \
        } else { \
            _uft_assert_fail(#str " ends with \"" #suffix "\"", \
                            (suffix) ? (suffix) : "(null)", \
                            (str) ? (str) : "(null)", __FILE__, __LINE__); \
        } \
    } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Float Matchers
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_ASSERT_FLOAT_EQ(actual, expected, epsilon) \
    do { \
        double _diff = fabs((double)(actual) - (double)(expected)); \
        if (_diff <= (epsilon)) { \
            _uft_assert_pass(#actual " ≈ " #expected); \
        } else { \
            char _exp[64], _act[64]; \
            snprintf(_exp, 64, "%.6f (±%.6f)", (double)(expected), (double)(epsilon)); \
            snprintf(_act, 64, "%.6f", (double)(actual)); \
            _uft_assert_fail(#actual " ≈ " #expected, _exp, _act, __FILE__, __LINE__); \
        } \
    } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Memory Matchers
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_ASSERT_MEM_EQ(actual, expected, size) \
    do { \
        if (memcmp((actual), (expected), (size)) == 0) { \
            _uft_assert_pass(#actual " equals " #expected " (memcmp)"); \
        } else { \
            _uft_assert_fail(#actual " equals " #expected " (memcmp)", \
                            "equal bytes", "different bytes", __FILE__, __LINE__); \
        } \
    } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Array/Length Matchers
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_ASSERT_LENGTH(arr, expected_len) \
    do { \
        size_t _len = sizeof(arr) / sizeof((arr)[0]); \
        if (_len == (size_t)(expected_len)) { \
            _uft_assert_pass(#arr " has length " #expected_len); \
        } else { \
            char _exp[64], _act[64]; \
            snprintf(_exp, 64, "%zu", (size_t)(expected_len)); \
            snprintf(_act, 64, "%zu", _len); \
            _uft_assert_fail(#arr " has length " #expected_len, _exp, _act, __FILE__, __LINE__); \
        } \
    } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Suite Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_DESCRIBE(name) \
    printf("\n  %s\n", name); \
    _uft_test_ctx.current_test = name

#define UFT_IT(description, block) \
    do { \
        printf("  → %s\n", description); \
        _uft_test_ctx.current_test = description; \
        block \
    } while(0)

#define UFT_TEST(name) static void test_##name(void)

#define UFT_RUN_TEST(name) \
    do { \
        printf("  [TEST] %s\n", #name); \
        _uft_test_ctx.current_test = #name; \
        test_##name(); \
    } while(0)

#define UFT_TEST_SUITE_BEGIN(name) \
    printf("═══════════════════════════════════════════════════════════════\n"); \
    printf("  %s\n", name); \
    printf("═══════════════════════════════════════════════════════════════\n"); \
    _uft_test_ctx.passed = 0; \
    _uft_test_ctx.failed = 0; \
    _uft_test_ctx.verbose = false

#define UFT_TEST_SUITE_END() \
    printf("\n═══════════════════════════════════════════════════════════════\n"); \
    printf("  Results: %d passed, %d failed\n", _uft_test_ctx.passed, _uft_test_ctx.failed); \
    printf("═══════════════════════════════════════════════════════════════\n"); \
    return _uft_test_ctx.failed > 0 ? 1 : 0

/* ═══════════════════════════════════════════════════════════════════════════════
 * Snowhouse-style Fluent Aliases
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Snowhouse: AssertThat(x, Equals(5)) */
#define AssertThat(actual, matcher)      UFT_ASSERT_##matcher(actual)
#define Equals(expected)                 EQ, expected
#define IsTrue()                         TRUE
#define IsFalse()                        FALSE
#define IsNull()                         NULL
#define IsNotNull()                      NOT_NULL
#define IsGreaterThan(expected)          GT, expected
#define IsLessThan(expected)             LT, expected
#define Contains(needle)                 STR_CONTAINS, needle

#ifdef __cplusplus
}
#endif

#endif /* UFT_ASSERT_H */
