/**
 * @file uft_common.h
 * @brief Common definitions and standard includes
 * 
 * This header provides:
 * - Standard C library includes (stdint, stddef, stdbool)
 * - Platform-independent type definitions
 * - Common macros and utility functions
 * 
 * All UFT source files should include this header first.
 */

#ifndef UFT_COMMON_H
#define UFT_COMMON_H

// ============================================================================
// Standard Library Includes
// ============================================================================

#include <stdint.h>
#include "uft/core/uft_endian.h"  // For endian functions
#include "uft_safe.h"  // For safe math functions
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// POSIX Types (for ssize_t)
// ============================================================================

#ifdef _WIN32
    typedef intptr_t ssize_t;
#else
    #include <sys/types.h>
#endif

// ============================================================================
// Common UFT Headers
// ============================================================================

#include "uft_error.h"
#include "uft_types.h"

// ============================================================================
// Platform Detection
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define UFT_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
    #define UFT_PLATFORM_MACOS 1
#elif defined(__linux__)
    #define UFT_PLATFORM_LINUX 1
#else
    #define UFT_PLATFORM_UNKNOWN 1
#endif

// ============================================================================
// Compiler Attributes
// ============================================================================

#ifdef __GNUC__
    #define UFT_UNUSED      __attribute__((unused))
    #define UFT_PACKED      __attribute__((packed))
    #define UFT_ALIGNED(n)  __attribute__((aligned(n)))
    #define UFT_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UFT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define UFT_UNUSED
    #define UFT_PACKED
    #define UFT_ALIGNED(n)
    #define UFT_LIKELY(x)   (x)
    #define UFT_UNLIKELY(x) (x)
#endif

// ============================================================================
// Safe Math (with guards to prevent redefinition)
// ============================================================================

#ifndef UFT_SAFE_MATH_DEFINED
#define UFT_SAFE_MATH_DEFINED

// Moved to uft_safe.h

// Moved to uft_safe.h

// Moved to uft_safe.h

// Moved to uft_safe.h

#endif // UFT_SAFE_MATH_DEFINED

// ============================================================================
// Byte Order Helpers (with guards)
// ============================================================================

#ifndef UFT_BYTE_ORDER_DEFINED
#define UFT_BYTE_ORDER_DEFINED

// Moved to uft/core/uft_endian.h

// Moved to uft/core/uft_endian.h

// Moved to uft/core/uft_endian.h

// Moved to uft/core/uft_endian.h

// Moved to uft/core/uft_endian.h

// Moved to uft/core/uft_endian.h

static inline void uft_write_be16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

static inline void uft_write_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}

#endif // UFT_BYTE_ORDER_DEFINED

// ============================================================================
// Utility Macros
// ============================================================================

#ifndef UFT_MIN
#define UFT_MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef UFT_MAX
#define UFT_MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef UFT_CLAMP
#define UFT_CLAMP(x, lo, hi) UFT_MIN(UFT_MAX(x, lo), hi)
#endif

#ifndef UFT_ARRAY_SIZE
#define UFT_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef UFT_ALIGN_UP
#define UFT_ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))
#endif

// ============================================================================
// Memory Helpers
// ============================================================================

#define UFT_ZERO(ptr) memset((ptr), 0, sizeof(*(ptr)))
#define UFT_ZERO_ARRAY(arr, count) memset((arr), 0, (count) * sizeof((arr)[0]))

// ============================================================================
// Null Check
// ============================================================================

#define UFT_RETURN_IF_NULL(ptr, ret) do { if (!(ptr)) return (ret); } while(0)
#define UFT_RETURN_ERROR_IF_NULL(ptr) UFT_RETURN_IF_NULL(ptr, UFT_ERROR_NULL_POINTER)

#endif // UFT_COMMON_H
