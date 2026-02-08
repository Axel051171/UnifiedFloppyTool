/**
 * @file uft_safe_cast.h
 * @brief Safe Integer Casting Macros
 * 
 * Provides overflow-checked type conversions to prevent
 * integer overflow vulnerabilities.
 * 
 * @version 1.0.0
 * @date 2026-01-07
 */

#ifndef UFT_SAFE_CAST_H
#define UFT_SAFE_CAST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Safe Cast Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Safely cast size_t to int with overflow check
 * @param sz Size value to convert
 * @return int value, or -1 if overflow would occur
 */
static inline int uft_size_to_int(size_t sz) {
    if (sz > (size_t)INT_MAX) {
        return -1;  /* Overflow */
    }
    return (int)sz;
}

/**
 * @brief Safely cast int to size_t with negative check
 * @param i Integer value to convert
 * @return size_t value, or 0 if negative
 */
static inline size_t uft_int_to_size(int i) {
    if (i < 0) {
        return 0;  /* Negative values invalid for size */
    }
    return (size_t)i;
}

/**
 * @brief Safely cast uint64_t to size_t with overflow check
 * @param u64 64-bit value to convert
 * @return size_t value, or SIZE_MAX if overflow
 */
static inline size_t uft_u64_to_size(uint64_t u64) {
    if (u64 > SIZE_MAX) {
        return SIZE_MAX;
    }
    return (size_t)u64;
}

/**
 * @brief Safely cast size_t to uint32_t with overflow check
 * @param sz Size value to convert
 * @return uint32_t value, or UINT32_MAX if overflow
 */
static inline uint32_t uft_size_to_u32(size_t sz) {
    if (sz > UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)sz;
}

/**
 * @brief Check if multiplication would overflow
 * @param a First operand
 * @param b Second operand
 * @return true if a*b would overflow size_t
 */
static inline bool uft_mul_would_overflow(size_t a, size_t b) {
    if (a == 0 || b == 0) return false;
    return a > SIZE_MAX / b;
}

/**
 * @brief Safe multiplication with overflow check
 * @param a First operand
 * @param b Second operand
 * @param result Pointer to store result
 * @return true if successful, false if overflow
 */
static inline bool uft_safe_mul(size_t a, size_t b, size_t *result) {
    if (uft_mul_would_overflow(a, b)) {
        *result = SIZE_MAX;
        return false;
    }
    *result = a * b;
    return true;
}

/**
 * @brief Safe addition with overflow check
 * @param a First operand
 * @param b Second operand
 * @param result Pointer to store result
 * @return true if successful, false if overflow
 */
static inline bool uft_safe_add(size_t a, size_t b, size_t *result) {
    if (a > SIZE_MAX - b) {
        *result = SIZE_MAX;
        return false;
    }
    *result = a + b;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Assertion Macros for Debug Builds
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef NDEBUG
    #define UFT_ASSERT_SIZE_FITS_INT(sz) ((void)0)
    #define UFT_ASSERT_INT_NONNEG(i)     ((void)0)
#else
    #define UFT_ASSERT_SIZE_FITS_INT(sz) assert((sz) <= (size_t)INT_MAX)
    #define UFT_ASSERT_INT_NONNEG(i)     assert((i) >= 0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_SAFE_CAST_H */
