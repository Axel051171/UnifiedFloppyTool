/**
 * @file uft_const_correct.h
 * @brief Const-Correctness Guidelines and Wrapper Macros
 * 
 * This header provides:
 * 1. Documentation of const-correctness best practices
 * 2. Wrapper macros to help enforce const where legacy APIs don't
 * 3. Static analysis hints
 * 
 * Why const matters:
 * - Prevents accidental modification of data
 * - Enables compiler optimizations
 * - Documents intent (input vs output parameters)
 * - Helps catch bugs at compile time
 * 
 * @version 1.0
 * @date 2026-01-07
 */

#ifndef UFT_CONST_CORRECT_H
#define UFT_CONST_CORRECT_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>  /* memcmp, memchr, strlen, strcmp */

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * COMPILER ATTRIBUTES
 *===========================================================================*/

/**
 * @brief Mark function parameter as input-only (not modified)
 * 
 * Usage: void process(UFT_IN const uint8_t *data, size_t len);
 */
#if defined(__GNUC__) || defined(__clang__)
    #define UFT_IN      /* Input parameter (const implied) */
    #define UFT_OUT     /* Output parameter (modified) */
    #define UFT_INOUT   /* Input/Output parameter */
    #define UFT_PURE    __attribute__((pure))    /* No side effects, depends on args + globals */
    #define UFT_CONST   __attribute__((const))   /* No side effects, depends only on args */
    #define UFT_NONNULL __attribute__((nonnull))
    #define UFT_RETURNS_NONNULL __attribute__((returns_nonnull))
    #define UFT_WARN_UNUSED __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
    #define UFT_IN      _In_
    #define UFT_OUT     _Out_
    #define UFT_INOUT   _Inout_
    #define UFT_PURE
    #define UFT_CONST
    #define UFT_NONNULL
    #define UFT_RETURNS_NONNULL
    #define UFT_WARN_UNUSED _Check_return_
#else
    #define UFT_IN
    #define UFT_OUT
    #define UFT_INOUT
    #define UFT_PURE
    #define UFT_CONST
    #define UFT_NONNULL
    #define UFT_RETURNS_NONNULL
    #define UFT_WARN_UNUSED
#endif

/**
 * @brief Mark pointer as pointing to immutable data
 * 
 * Usage: void read_data(UFT_READONLY uint8_t *data);
 */
#define UFT_READONLY const

/**
 * @brief Mark function as not modifying the object
 * 
 * For C++: Used in method declarations
 */
#ifdef __cplusplus
    #define UFT_CONST_METHOD const
#else
    #define UFT_CONST_METHOD
#endif

/*===========================================================================
 * CONST-CORRECT FUNCTION SIGNATURES
 * 
 * These are the recommended signatures for common operations.
 * Use these patterns when writing new code.
 *===========================================================================*/

/**
 * @example Read-only data processing
 * 
 * BAD:
 *   uint16_t calc_crc(uint8_t *data, size_t len);
 * 
 * GOOD:
 *   uint16_t calc_crc(const uint8_t *data, size_t len);
 */

/**
 * @example Output parameters
 * 
 * BAD:
 *   int get_name(char *buffer, size_t size);
 * 
 * GOOD (C99):
 *   int get_name(char buffer[static size], size_t size);
 * 
 * BETTER (with attributes):
 *   int get_name(UFT_OUT char *buffer, size_t size);
 */

/**
 * @example String parameters
 * 
 * BAD:
 *   int open_file(char *path);
 * 
 * GOOD:
 *   int open_file(const char *path);
 */

/**
 * @example Struct access
 * 
 * BAD:
 *   int get_sector_size(disk_t *disk);
 * 
 * GOOD (if disk is not modified):
 *   int get_sector_size(const disk_t *disk);
 */

/*===========================================================================
 * CONST CASTING HELPERS
 * 
 * Sometimes legacy APIs require non-const pointers even for read-only data.
 * These macros make such casts explicit and auditable.
 *===========================================================================*/

/**
 * @brief Cast away const for legacy API compatibility
 * 
 * ONLY use this when calling legacy APIs that don't modify the data
 * but have incorrect signatures.
 * 
 * Usage:
 *   const uint8_t *data = ...;
 *   legacy_read_func(UFT_CONST_CAST(uint8_t*, data), len);
 */
#define UFT_CONST_CAST(type, ptr) ((type)(uintptr_t)(ptr))

/**
 * @brief Add const to pointer (always safe)
 */
#define UFT_ADD_CONST(ptr) ((const typeof(*(ptr)) *)(ptr))

/*===========================================================================
 * CONST-CORRECT WRAPPERS FOR COMMON OPERATIONS
 *===========================================================================*/

/**
 * @brief Const-correct memory comparison
 */
static inline int uft_memcmp_safe(const void *s1, const void *s2, size_t n) {
    return memcmp(s1, s2, n);
}

/**
 * @brief Const-correct byte search
 */
static inline const void *uft_memchr_safe(const void *s, int c, size_t n) {
    return memchr(s, c, n);
}

/**
 * @brief Const-correct string length
 */
static inline size_t uft_strlen_safe(const char *s) {
    return strlen(s);
}

/**
 * @brief Const-correct string comparison
 */
static inline int uft_strcmp_safe(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

/*===========================================================================
 * STATIC ANALYSIS ANNOTATIONS
 *===========================================================================*/

/**
 * @brief Annotate buffer size relationships
 * 
 * Usage:
 *   void copy(UFT_SIZED_BUF(size) char *dst, 
 *             UFT_SIZED_BUF(size) const char *src, 
 *             size_t size);
 */
#if defined(__clang__)
    #define UFT_SIZED_BUF(size_param) __attribute__((pass_object_size(0)))
#else
    #define UFT_SIZED_BUF(size_param)
#endif

/**
 * @brief Mark pointer as potentially NULL
 */
#if defined(__clang__) || defined(__GNUC__)
    #define UFT_NULLABLE _Nullable
    #define UFT_NONNULL_PTR _Nonnull
#else
    #define UFT_NULLABLE
    #define UFT_NONNULL_PTR
#endif

/*===========================================================================
 * BEST PRACTICES CHECKLIST
 * 
 * When writing new code or reviewing existing code, check:
 * 
 * 1. Input parameters:
 *    □ All read-only pointer parameters are const
 *    □ String parameters (char*) are const char*
 *    □ Struct parameters that aren't modified are const
 * 
 * 2. Output parameters:
 *    □ Clearly documented as output (UFT_OUT or comments)
 *    □ Size parameter comes before or after consistently
 * 
 * 3. Return values:
 *    □ Const for pointers to internal data
 *    □ Non-const for allocated data that caller owns
 * 
 * 4. Struct members:
 *    □ Const for immutable configuration
 *    □ Function pointers have const-correct signatures
 * 
 * 5. Casts:
 *    □ No implicit const removal (use UFT_CONST_CAST if needed)
 *    □ Each const cast is commented with justification
 *===========================================================================*/

/*===========================================================================
 * LEGACY API COMPATIBILITY NOTES
 * 
 * The following legacy functions don't use const but should:
 * 
 * fluxStreamAnalyzer.c:
 *   - computehistogram(uint32_t *indata, ...)
 *     Should be: const uint32_t *indata
 * 
 * fm_encoding.h / mfm_encoding.h:
 *   - getFMcode(..., uint8_t *dstbuf)
 *     Should be: uint8_t *dstbuf (output, OK as-is)
 *   - BuildFMCylinder(uint8_t *buffer, ..., uint8_t *track, ...)
 *     track should be: const uint8_t *track
 * 
 * Various loaders:
 *   - char *imgfile parameters
 *     Should be: const char *imgfile
 * 
 * These are documented for future refactoring. Do not change legacy
 * signatures without updating all callers and running full test suite.
 *===========================================================================*/

#ifdef __cplusplus
}
#endif

#endif /* UFT_CONST_CORRECT_H */
