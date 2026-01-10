/**
 * @file uft_macros.h
 * @brief Central UFT Macro Definitions
 * @version 1.0.0
 * 
 * This file consolidates all compiler/platform macros to eliminate duplicates.
 * Include this file instead of defining UFT_PACKED, UFT_INLINE, etc. locally.
 * 
 * MIGRATION: Replace local definitions with:
 *   #include "uft/uft_macros.h"
 */

#ifndef UFT_MACROS_H
#define UFT_MACROS_H

/* ═══════════════════════════════════════════════════════════════════════════════
 * Compiler Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(_MSC_VER)
    #define UFT_COMPILER_MSVC       1
    #define UFT_COMPILER_VERSION    _MSC_VER
#elif defined(__clang__)
    #define UFT_COMPILER_CLANG      1
    #define UFT_COMPILER_VERSION    (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
    #define UFT_COMPILER_GCC        1
    #define UFT_COMPILER_VERSION    (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
    #define UFT_COMPILER_UNKNOWN    1
    #define UFT_COMPILER_VERSION    0
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Platform Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(_WIN32) || defined(_WIN64)
    #define UFT_PLATFORM_WINDOWS    1
#elif defined(__APPLE__) && defined(__MACH__)
    #define UFT_PLATFORM_MACOS      1
#elif defined(__linux__)
    #define UFT_PLATFORM_LINUX      1
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    #define UFT_PLATFORM_BSD        1
#else
    #define UFT_PLATFORM_UNKNOWN    1
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_INLINE - Force Inline
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_INLINE
    #if defined(UFT_COMPILER_MSVC)
        #define UFT_INLINE          static __forceinline
    #elif defined(UFT_COMPILER_GCC) || defined(UFT_COMPILER_CLANG)
        #define UFT_INLINE          static inline __attribute__((always_inline))
    #else
        #define UFT_INLINE          static inline
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_NOINLINE - Prevent Inline
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_NOINLINE
    #if defined(UFT_COMPILER_MSVC)
        #define UFT_NOINLINE        __declspec(noinline)
    #elif defined(UFT_COMPILER_GCC) || defined(UFT_COMPILER_CLANG)
        #define UFT_NOINLINE        __attribute__((noinline))
    #else
        #define UFT_NOINLINE
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_PACKED - Structure Packing (Single Attribute Style)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_PACKED
    #if defined(UFT_COMPILER_MSVC)
        /* MSVC uses pragma, not attribute - UFT_PACKED is empty */
        #define UFT_PACKED
    #elif defined(UFT_COMPILER_GCC) || defined(UFT_COMPILER_CLANG)
        #define UFT_PACKED          __attribute__((packed))
    #else
        #define UFT_PACKED
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_PACKED_BEGIN/END - Structure Packing (Pragma Style for MSVC)
 * 
 * Usage:
 *   UFT_PACKED_BEGIN
 *   typedef struct {
 *       uint8_t a;
 *       uint32_t b;
 *   } UFT_PACKED my_packed_struct;
 *   UFT_PACKED_END
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_PACKED_BEGIN
    #if defined(UFT_COMPILER_MSVC)
        #define UFT_PACKED_BEGIN    __pragma(pack(push, 1))
        #define UFT_PACKED_END      __pragma(pack(pop))
    #else
        #define UFT_PACKED_BEGIN
        #define UFT_PACKED_END
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_UNUSED - Mark Variable/Parameter as Intentionally Unused
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_UNUSED
    #if defined(UFT_COMPILER_GCC) || defined(UFT_COMPILER_CLANG)
        #define UFT_UNUSED          __attribute__((unused))
    #else
        #define UFT_UNUSED
    #endif
#endif

/* Suppress unused parameter warning */
#ifndef UFT_UNUSED_PARAM
    #define UFT_UNUSED_PARAM(x)     (void)(x)
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_DEPRECATED - Mark as Deprecated
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_DEPRECATED
    #if defined(UFT_COMPILER_MSVC)
        #define UFT_DEPRECATED(msg) __declspec(deprecated(msg))
    #elif defined(UFT_COMPILER_GCC) || defined(UFT_COMPILER_CLANG)
        #if UFT_COMPILER_GCC && UFT_COMPILER_VERSION >= 40500
            #define UFT_DEPRECATED(msg) __attribute__((deprecated(msg)))
        #else
            #define UFT_DEPRECATED(msg) __attribute__((deprecated))
        #endif
    #else
        #define UFT_DEPRECATED(msg)
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_ALIGNED - Memory Alignment
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_ALIGNED
    #if defined(UFT_COMPILER_MSVC)
        #define UFT_ALIGNED(n)      __declspec(align(n))
    #elif defined(UFT_COMPILER_GCC) || defined(UFT_COMPILER_CLANG)
        #define UFT_ALIGNED(n)      __attribute__((aligned(n)))
    #else
        #define UFT_ALIGNED(n)
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_LIKELY/UNLIKELY - Branch Prediction Hints
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_LIKELY
    #if defined(UFT_COMPILER_GCC) || defined(UFT_COMPILER_CLANG)
        #define UFT_LIKELY(x)       __builtin_expect(!!(x), 1)
        #define UFT_UNLIKELY(x)     __builtin_expect(!!(x), 0)
    #else
        #define UFT_LIKELY(x)       (x)
        #define UFT_UNLIKELY(x)     (x)
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_RESTRICT - Pointer Aliasing Hint
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_RESTRICT
    #if defined(UFT_COMPILER_MSVC)
        #define UFT_RESTRICT        __restrict
    #elif defined(UFT_COMPILER_GCC) || defined(UFT_COMPILER_CLANG)
        #define UFT_RESTRICT        __restrict__
    #elif __STDC_VERSION__ >= 199901L
        #define UFT_RESTRICT        restrict
    #else
        #define UFT_RESTRICT
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_NORETURN - Function Does Not Return
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_NORETURN
    #if defined(UFT_COMPILER_MSVC)
        #define UFT_NORETURN        __declspec(noreturn)
    #elif defined(UFT_COMPILER_GCC) || defined(UFT_COMPILER_CLANG)
        #define UFT_NORETURN        __attribute__((noreturn))
    #else
        #define UFT_NORETURN
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_EXPORT/UFT_IMPORT - DLL Export/Import (Windows)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_EXPORT
    #if defined(UFT_PLATFORM_WINDOWS)
        #if defined(UFT_BUILD_DLL)
            #define UFT_EXPORT      __declspec(dllexport)
        #elif defined(UFT_USE_DLL)
            #define UFT_EXPORT      __declspec(dllimport)
        #else
            #define UFT_EXPORT
        #endif
    #elif defined(UFT_COMPILER_GCC) || defined(UFT_COMPILER_CLANG)
        #define UFT_EXPORT          __attribute__((visibility("default")))
    #else
        #define UFT_EXPORT
    #endif
#endif

#ifndef UFT_IMPORT
    #if defined(UFT_PLATFORM_WINDOWS) && defined(UFT_USE_DLL)
        #define UFT_IMPORT          __declspec(dllimport)
    #else
        #define UFT_IMPORT
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_API - Public API Marker
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_API
    #if defined(UFT_BUILD_SHARED)
        #define UFT_API             UFT_EXPORT
    #else
        #define UFT_API
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_STATIC_ASSERT - Compile-Time Assertion
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_STATIC_ASSERT
    #if __STDC_VERSION__ >= 201112L
        #define UFT_STATIC_ASSERT(cond, msg)    _Static_assert(cond, msg)
    #elif defined(UFT_COMPILER_MSVC)
        #define UFT_STATIC_ASSERT(cond, msg)    static_assert(cond, msg)
    #else
        #define UFT_STATIC_ASSERT(cond, msg)    typedef char uft_static_assert_##__LINE__[(cond) ? 1 : -1]
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_ARRAY_SIZE - Safe Array Size Macro
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_ARRAY_SIZE
    #define UFT_ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0]))
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_MIN/UFT_MAX - Safe Min/Max Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_MIN
    #define UFT_MIN(a, b)           (((a) < (b)) ? (a) : (b))
#endif

#ifndef UFT_MAX
    #define UFT_MAX(a, b)           (((a) > (b)) ? (a) : (b))
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_STRINGIFY - Macro Stringification
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_STRINGIFY
    #define UFT_STRINGIFY_IMPL(x)   #x
    #define UFT_STRINGIFY(x)        UFT_STRINGIFY_IMPL(x)
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_CONCAT - Macro Token Concatenation
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_CONCAT
    #define UFT_CONCAT_IMPL(a, b)   a##b
    #define UFT_CONCAT(a, b)        UFT_CONCAT_IMPL(a, b)
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Byte Order Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_BYTE_ORDER
    #if defined(__BYTE_ORDER__)
        #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            #define UFT_LITTLE_ENDIAN   1
        #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            #define UFT_BIG_ENDIAN      1
        #endif
    #elif defined(UFT_PLATFORM_WINDOWS)
        /* Windows is always little-endian on supported architectures */
        #define UFT_LITTLE_ENDIAN       1
    #elif defined(__LITTLE_ENDIAN__) || defined(__ARMEL__) || defined(__THUMBEL__) || \
          defined(__AARCH64EL__) || defined(_MIPSEL) || defined(__MIPSEL__) || defined(__i386__) || \
          defined(__x86_64__) || defined(__amd64__)
        #define UFT_LITTLE_ENDIAN       1
    #elif defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__THUMBEB__) || \
          defined(__AARCH64EB__) || defined(_MIPSEB) || defined(__MIPSEB__)
        #define UFT_BIG_ENDIAN          1
    #else
        /* Default to little-endian */
        #define UFT_LITTLE_ENDIAN       1
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Byte Swap Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_BSWAP16
    #if defined(UFT_COMPILER_GCC) || defined(UFT_COMPILER_CLANG)
        #define UFT_BSWAP16(x)      __builtin_bswap16(x)
        #define UFT_BSWAP32(x)      __builtin_bswap32(x)
        #define UFT_BSWAP64(x)      __builtin_bswap64(x)
    #elif defined(UFT_COMPILER_MSVC)
        #include <stdlib.h>
        #define UFT_BSWAP16(x)      _byteswap_ushort(x)
        #define UFT_BSWAP32(x)      _byteswap_ulong(x)
        #define UFT_BSWAP64(x)      _byteswap_uint64(x)
    #else
        #define UFT_BSWAP16(x)      ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))
        #define UFT_BSWAP32(x)      ((((x) & 0xFF000000) >> 24) | (((x) & 0x00FF0000) >> 8) | \
                                     (((x) & 0x0000FF00) << 8) | (((x) & 0x000000FF) << 24))
        #define UFT_BSWAP64(x)      ((UFT_BSWAP32((x) & 0xFFFFFFFF) << 32) | UFT_BSWAP32((x) >> 32))
    #endif
#endif

/* Little/Big Endian conversion */
#if defined(UFT_LITTLE_ENDIAN)
    #define UFT_LE16(x)     (x)
    #define UFT_LE32(x)     (x)
    #define UFT_LE64(x)     (x)
    #define UFT_BE16(x)     UFT_BSWAP16(x)
    #define UFT_BE32(x)     UFT_BSWAP32(x)
    #define UFT_BE64(x)     UFT_BSWAP64(x)
#else
    #define UFT_LE16(x)     UFT_BSWAP16(x)
    #define UFT_LE32(x)     UFT_BSWAP32(x)
    #define UFT_LE64(x)     UFT_BSWAP64(x)
    #define UFT_BE16(x)     (x)
    #define UFT_BE32(x)     (x)
    #define UFT_BE64(x)     (x)
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Debug Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_DEBUG
    #if defined(DEBUG) || defined(_DEBUG) || !defined(NDEBUG)
        #define UFT_DEBUG           1
    #endif
#endif

#ifndef UFT_ASSERT
    #if defined(UFT_DEBUG)
        #include <assert.h>
        #define UFT_ASSERT(cond)    assert(cond)
    #else
        #define UFT_ASSERT(cond)    ((void)0)
    #endif
#endif

#endif /* UFT_MACROS_H */
