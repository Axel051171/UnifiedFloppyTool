/**
 * @file uft_config.h
 * @brief UnifiedFloppyTool - Master Configuration Header
 * @version 1.6.2
 * @date 2025-01-10
 * 
 * CHANGELOG v1.6.2:
 * - Include uft_macros.h for centralized macro definitions
 * - Eliminates macro duplicates across the codebase
 * 
 * CHANGELOG v1.6.1:
 * - Consolidated all feature flags
 * - Added cache-line alignment for False Sharing prevention
 * - Platform detection macros
 * - Endianness detection
 * - Compiler-specific optimizations
 * 
 * Copyright (c) 2025 UFT Project
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_CONFIG_H
#define UFT_CONFIG_H

/* Central macro definitions - include first to set defaults */
#include "uft_macros.h"

/*============================================================================
 * VERSION INFO
 *============================================================================*/

#define UFT_VERSION_MAJOR   1
#define UFT_VERSION_MINOR   6
#define UFT_VERSION_PATCH   2
#define UFT_VERSION_STRING  "1.6.2"

/*============================================================================
 * PLATFORM DETECTION
 *============================================================================*/

/* Architecture detection */
#if defined(__x86_64__) || defined(_M_X64)
    #define UFT_ARCH_X64        1
    #define UFT_ARCH_NAME       "x86_64"
    #define UFT_CACHE_LINE_SIZE 64
#elif defined(__i386__) || defined(_M_IX86)
    #define UFT_ARCH_X86        1
    #define UFT_ARCH_NAME       "x86"
    #define UFT_CACHE_LINE_SIZE 64
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define UFT_ARCH_ARM64      1
    #define UFT_ARCH_NAME       "arm64"
    #define UFT_CACHE_LINE_SIZE 64
#elif defined(__arm__) || defined(_M_ARM)
    #define UFT_ARCH_ARM32      1
    #define UFT_ARCH_NAME       "arm32"
    #define UFT_CACHE_LINE_SIZE 32
#else
    #define UFT_ARCH_UNKNOWN    1
    #define UFT_ARCH_NAME       "unknown"
    #define UFT_CACHE_LINE_SIZE 64
#endif

/* OS detection */
#if defined(_WIN32) || defined(_WIN64)
    #define UFT_OS_WINDOWS      1
    #define UFT_OS_NAME         "Windows"
#elif defined(__linux__)
    #define UFT_OS_LINUX        1
    #define UFT_OS_NAME         "Linux"
#elif defined(__APPLE__)
    #define UFT_OS_MACOS        1
    #define UFT_OS_NAME         "macOS"
#elif defined(__FreeBSD__)
    #define UFT_OS_FREEBSD      1
    #define UFT_OS_NAME         "FreeBSD"
#else
    #define UFT_OS_UNKNOWN      1
    #define UFT_OS_NAME         "Unknown"
#endif

/*============================================================================
 * ENDIANNESS DETECTION
 *============================================================================*/

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define UFT_LITTLE_ENDIAN   1
    #else
        #define UFT_BIG_ENDIAN      1
    #endif
#elif defined(_WIN32) || defined(UFT_ARCH_X64) || defined(UFT_ARCH_X86)
    #define UFT_LITTLE_ENDIAN   1
#else
    /* Default assumption - can be overridden */
    #define UFT_LITTLE_ENDIAN   1
#endif

/*============================================================================
 * COMPILER DETECTION & ATTRIBUTES
 *============================================================================*/

#if defined(__GNUC__) || defined(__clang__)
    #define UFT_COMPILER_GCC_LIKE   1
#ifndef UFT_LIKELY
    #define UFT_LIKELY(x)           __builtin_expect(!!(x), 1)
#endif
#ifndef UFT_UNLIKELY
    #define UFT_UNLIKELY(x)         __builtin_expect(!!(x), 0)
#endif
#ifndef UFT_INLINE
    #define UFT_INLINE              inline __attribute__((always_inline))
#endif
    #define UFT_NOINLINE            __attribute__((noinline))
    #define UFT_ALIGNED(n)          __attribute__((aligned(n)))
    #define UFT_PACKED              
#ifndef UFT_UNUSED
    #define UFT_UNUSED              __attribute__((unused))
#endif
    #define UFT_PURE                __attribute__((pure))
    #define UFT_CONST               __attribute__((const))
    #define UFT_HOT                 __attribute__((hot))
    #define UFT_COLD                __attribute__((cold))
    #define UFT_RESTRICT            __restrict__
    #define UFT_PREFETCH(addr)      __builtin_prefetch(addr)
    #define UFT_PREFETCH_W(addr)    __builtin_prefetch(addr, 1)
    
    /* Branch hint for switch statements */
    #define UFT_ASSUME(cond)        do { if (!(cond)) __builtin_unreachable(); } while(0)
    
#elif defined(_MSC_VER)
    #define UFT_COMPILER_MSVC       1
#ifndef UFT_LIKELY
    #define UFT_LIKELY(x)           (x)
#endif
#ifndef UFT_UNLIKELY
    #define UFT_UNLIKELY(x)         (x)
#endif
#ifndef UFT_INLINE
    #define UFT_INLINE              __forceinline
#endif
    #define UFT_NOINLINE            __declspec(noinline)
    #define UFT_ALIGNED(n)          __declspec(align(n))
    #define UFT_PACKED              
#ifndef UFT_UNUSED
    #define UFT_UNUSED              
#endif
    #define UFT_PURE                
    #define UFT_CONST               
    #define UFT_HOT                 
    #define UFT_COLD                
    #define UFT_RESTRICT            __restrict
    #define UFT_PREFETCH(addr)      _mm_prefetch((const char*)(addr), _MM_HINT_T0)
    #define UFT_PREFETCH_W(addr)    _mm_prefetch((const char*)(addr), _MM_HINT_T0)
    #define UFT_ASSUME(cond)        __assume(cond)
    
#else
#ifndef UFT_LIKELY
    #define UFT_LIKELY(x)           (x)
#endif
#ifndef UFT_UNLIKELY
    #define UFT_UNLIKELY(x)         (x)
#endif
#ifndef UFT_INLINE
    #define UFT_INLINE              inline
#endif
    #define UFT_NOINLINE            
    #define UFT_ALIGNED(n)          
    #define UFT_PACKED              
#ifndef UFT_UNUSED
    #define UFT_UNUSED              
#endif
    #define UFT_PURE                
    #define UFT_CONST               
    #define UFT_HOT                 
    #define UFT_COLD                
    #define UFT_RESTRICT            
    #define UFT_PREFETCH(addr)      ((void)0)
    #define UFT_PREFETCH_W(addr)    ((void)0)
    #define UFT_ASSUME(cond)        ((void)0)
#endif

/*============================================================================
 * CACHE-LINE ALIGNMENT (False Sharing Prevention)
 *============================================================================*/

/**
 * @brief Align structure to cache line boundary
 * Use for frequently-accessed shared data in multithreaded code
 */
#define UFT_CACHE_ALIGNED   UFT_ALIGNED(UFT_CACHE_LINE_SIZE)

/**
 * @brief Padding to ensure structure members don't share cache lines
 * Use between hot and cold data in the same structure
 */
#define UFT_CACHE_PAD       char _pad_[UFT_CACHE_LINE_SIZE]

/*============================================================================
 * FEATURE FLAGS (compile-time configuration)
 *============================================================================*/

/* SIMD support (can be disabled with -DUFT_NO_SIMD) */
#ifndef UFT_NO_SIMD
    #if defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
        #define UFT_HAS_SSE2    1
    #endif
    #if defined(__AVX2__)
        #define UFT_HAS_AVX2    1
    #endif
    #if defined(__AVX512F__)
        #define UFT_HAS_AVX512  1
    #endif
    #if defined(__ARM_NEON) || defined(__ARM_NEON__)
        #define UFT_HAS_NEON    1
    #endif
#endif

/* Threading support */
#ifndef UFT_NO_THREADS
    #define UFT_HAS_THREADS     1
#endif

/* Debug mode */
#ifdef NDEBUG
    #define UFT_RELEASE_BUILD   1
#else
    #define UFT_DEBUG_BUILD     1
#endif

/* Memory debugging */
#ifdef UFT_DEBUG_MEMORY
    #define UFT_TRACK_ALLOCATIONS   1
#endif

/*============================================================================
 * STANDARD INCLUDES (always needed)
 *============================================================================*/

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

/*============================================================================
 * API EXPORT MACROS (for shared library)
 *============================================================================*/

#ifdef UFT_SHARED_LIBRARY
    #ifdef UFT_BUILDING_DLL
        #if defined(_WIN32)
            #define UFT_API __declspec(dllexport)
        #else
            #define UFT_API __attribute__((visibility("default")))
        #endif
    #else
        #if defined(_WIN32)
            #define UFT_API __declspec(dllimport)
        #else
            #define UFT_API 
        #endif
    #endif
#else
    #define UFT_API
#endif

/*============================================================================
 * DEBUG ASSERTIONS
 *============================================================================*/

#ifdef UFT_DEBUG_BUILD
    #include <assert.h>

#include "uft/uft_compiler.h"
    #define UFT_ASSERT(cond)        assert(cond)
    #define UFT_ASSERT_MSG(cond, m) assert((cond) && (m))
#else
    #define UFT_ASSERT(cond)        UFT_ASSUME(cond)
    #define UFT_ASSERT_MSG(cond, m) UFT_ASSUME(cond)
#endif

/*============================================================================
 * NUMERIC LIMITS
 *============================================================================*/

#define UFT_MAX_TRACKS          168     /* Max cylinders (84 * 2 heads) */
#define UFT_MAX_SECTORS         64      /* Max sectors per track */
#define UFT_MAX_SECTOR_SIZE     8192    /* Max sector size (8K) */
#define UFT_MAX_HEADS           2       /* Max heads */
#define UFT_MAX_REVOLUTIONS     16      /* Max revolutions to capture */

/*============================================================================
 * TIMING CONSTANTS (nanoseconds)
 *============================================================================*/

#define UFT_NS_PER_US           1000ULL
#define UFT_NS_PER_MS           1000000ULL
#define UFT_NS_PER_SEC          1000000000ULL

/* Floppy drive rotation times */
#define UFT_ROTATION_TIME_300RPM    (200 * UFT_NS_PER_MS)   /* 200ms */
#define UFT_ROTATION_TIME_360RPM    (166667 * UFT_NS_PER_US) /* 166.67ms */

/*============================================================================
 * ERROR CODES
 *============================================================================*/

typedef enum uft_error {
    UFT_OK                  = 0,
    UFT_ERR_INVALID_ARG     = -1,
    UFT_ERR_OUT_OF_MEMORY   = -2,
    UFT_ERR_IO_ERROR        = -3,
    UFT_ERR_FORMAT_ERROR    = -4,
    UFT_ERR_CRC_ERROR       = -5,
    UFT_ERR_NOT_FOUND       = -6,
    UFT_ERR_TIMEOUT         = -7,
    UFT_ERR_UNSUPPORTED     = -8,
    UFT_ERR_BUFFER_TOO_SMALL = -9,
    UFT_ERR_INTERNAL        = -99,
} uft_error_t;

/**
 * @brief Get error message for error code
 */
UFT_INLINE const char* uft_error_string(uft_error_t err) {
    switch (err) {
        case UFT_OK:                return "Success";
        case UFT_ERR_INVALID_ARG:   return "Invalid argument";
        case UFT_ERR_OUT_OF_MEMORY: return "Out of memory";
        case UFT_ERR_IO_ERROR:      return "I/O error";
        case UFT_ERR_FORMAT_ERROR:  return "Format error";
        case UFT_ERR_CRC_ERROR:     return "CRC error";
        case UFT_ERR_NOT_FOUND:     return "Not found";
        case UFT_ERR_TIMEOUT:       return "Timeout";
        case UFT_ERR_UNSUPPORTED:   return "Unsupported operation";
        case UFT_ERR_BUFFER_TOO_SMALL: return "Buffer too small";
        case UFT_ERR_INTERNAL:      return "Internal error";
        default:                    return "Unknown error";
    }
}

#endif /* UFT_CONFIG_H */
