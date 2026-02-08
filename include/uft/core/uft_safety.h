/**
 * @file uft_safety.h
 * @brief Master Include for All Safety Functions
 * 
 * This header provides a single include for all UFT safety modules:
 * - Safe I/O operations
 * - Safe integer casts
 * - Safe path handling
 * - Safe string parsing
 * - CRC validation
 * 
 * Usage:
 *   #include "uft/core/uft_safety.h"
 * 
 * @version 1.0.0
 * @date 2026-01-07
 */

#ifndef UFT_SAFETY_H
#define UFT_SAFETY_H

/* Core safety modules */
#include "uft_safe_io.h"        /* Safe fread/fwrite */
#include "uft_safe_cast.h"      /* Safe integer casts */
#include "uft_safe_parse.h"     /* Safe atoi/strtol replacements */
#include "uft_path_safe.h"      /* Path traversal protection */
#include "uft_crc_validate.h"   /* CRC validation */

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Safety Assertion Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

#include <assert.h>

/** Assert pointer is non-NULL */
#define UFT_ASSERT_NONNULL(ptr) assert((ptr) != NULL)

/** Assert size fits in target type */
#define UFT_ASSERT_SIZE_OK(sz, max) assert((sz) <= (max))

/** Mark code as unreachable (for switch defaults) */
#ifdef __GNUC__
    #define UFT_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
    #define UFT_UNREACHABLE() __assume(0)
#else
    #define UFT_UNREACHABLE() assert(0 && "Unreachable code")
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Bounds Checking Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Check if index is within bounds */
#define UFT_IN_BOUNDS(idx, len) ((size_t)(idx) < (size_t)(len))

/** Check if range is valid */
#define UFT_RANGE_VALID(start, count, max) \
    ((start) <= (max) && (count) <= (max) - (start))

/* ═══════════════════════════════════════════════════════════════════════════════
 * Safe Memory Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

#include <string.h>
#include <stdlib.h>

/**
 * @brief Safe memcpy with NULL and overlap check
 */
static inline void *uft_safe_memcpy(void *dest, const void *src, size_t n) {
    if (!dest || !src || n == 0) return dest;
    
    /* Check for overlap - use memmove if overlapping */
    const char *s = (const char *)src;
    char *d = (char *)dest;
    if ((s < d && s + n > d) || (d < s && d + n > s)) {
        return memmove(dest, src, n);
    }
    return memcpy(dest, src, n);
}

/**
 * @brief Safe strncpy that always null-terminates
 */
static inline char *uft_safe_strncpy(char *dest, const char *src, size_t n) {
    if (!dest || n == 0) return dest;
    if (!src) {
        dest[0] = '\0';
        return dest;
    }
    
    strncpy(dest, src, n - 1);
    dest[n - 1] = '\0';
    return dest;
}

/**
 * @brief Safe malloc with overflow check
 */
static inline void *uft_safe_malloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return NULL;
    
    /* Check for multiplication overflow */
    if (nmemb > SIZE_MAX / size) return NULL;
    
    return malloc(nmemb * size);
}

/**
 * @brief Safe calloc (uses calloc's built-in overflow check)
 */
static inline void *uft_safe_calloc(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

/**
 * @brief Safe realloc with overflow check
 */
static inline void *uft_safe_realloc(void *ptr, size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        free(ptr);
        return NULL;
    }
    
    if (nmemb > SIZE_MAX / size) return NULL;
    
    return realloc(ptr, nmemb * size);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_SAFETY_H */
