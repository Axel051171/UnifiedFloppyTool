/**
 * @file uft_config.h
 * @brief UnifiedFloppyTool - Master Configuration Header
 * @version 3.8.0
 * @date 2026-01-15
 * 
 * CHANGELOG v3.8.0:
 * - Consolidated all feature flags
 * - Added cache-line alignment for False Sharing prevention
 * - Platform detection macros
 * - Endianness detection
 * - Compiler-specific optimizations
 * 
 * Copyright (c) 2025-2026 UFT Project
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_CONFIG_H
#define UFT_CONFIG_H

/*============================================================================
 * VERSION INFO
 *============================================================================*/

#define UFT_CONFIG_VERSION_MAJOR   3
#define UFT_CONFIG_VERSION_MINOR   8
#define UFT_CONFIG_VERSION_PATCH   0
#define UFT_CONFIG_VERSION_STRING  "3.8.0"

/*============================================================================
 * PLATFORM DETECTION
 *============================================================================*/

/* Architecture detection */
/* MF-456: die zweite Architektur-Erkennung stand hier. Sie widersprach der
 * in uft_platform.h in der Schreibweise ("arm64" gegen "ARM64") und war
 * bei UFT_CACHE_LINE_SIZE die informiertere (32 auf ARM32, wo
 * uft_compiler.h pauschal 64 setzte). Beides steht jetzt in
 * uft/compat/uft_platform_base.h, das weiter unten ohnehin eingebunden
 * wird. */

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

/* MF-455: die dritte Fassung derselben Erkennung stand hier.
 *
 * Sie setzte je nach Zweig UFT_LITTLE_ENDIAN oder UFT_BIG_ENDIAN auf 1 und den
 * jeweils anderen Namen gar nicht — dieselbe "definiert oder abwesend"-Semantik
 * wie in uft_platform.h, gegenlaeufig zu compat, das 0 oder 1 setzte. Der
 * letzte Zweig riet ausserdem ("Default assumption - can be overridden"), statt
 * es zu wissen.
 *
 * Jetzt eine Definition, immer 0 oder 1. */
#include "uft/compat/uft_platform_base.h"

/*============================================================================
 * COMPILER DETECTION & ATTRIBUTES
 *============================================================================*/

/* MF-456: Eigentuemer der Attribut-Makros ist uft_compiler.h.
 *
 * Die Kaskade unten definierte UFT_INLINE, UFT_NOINLINE, UFT_ALIGNED und
 * UFT_RESTRICT ein weiteres Mal, teils ohne #ifndef — mit anderen Ergebnissen:
 * UFT_INLINE hiess hier `inline __attribute__((always_inline))` (ohne
 * `static`, also ein Link-Fehler, wenn der Compiler nicht inlinet), in
 * uft_compiler.h `static inline`. Welche Fassung galt, entschied die
 * Include-Reihenfolge.
 *
 * Die Duplikate sind entfernt, nicht mit #ifndef abgesichert: seit dieser
 * Header den Eigentuemer selbst einbindet, koennte ein Rueckfall nie feuern —
 * er waere toter Code mit einem anderen Wert, also genau die Doppelung, die
 * hier verschwinden soll. */
#include "uft/uft_compiler.h"

#if defined(__GNUC__) || defined(__clang__)
    #define UFT_COMPILER_GCC_LIKE   1
#ifndef UFT_LIKELY
    #define UFT_LIKELY(x)           __builtin_expect(!!(x), 1)
#endif
#ifndef UFT_UNLIKELY
    #define UFT_UNLIKELY(x)         __builtin_expect(!!(x), 0)
#endif
/* MF-451: UFT_PACKED stand hier dreimal (je Compiler-Zweig) und war
 * jedes Mal leer — Packing kommt aus uft_packed.h. */
#include "uft/uft_packed.h"
#ifndef UFT_UNUSED
    #define UFT_UNUSED              __attribute__((unused))
#endif
    #define UFT_PURE                __attribute__((pure))
    #define UFT_CONST               __attribute__((const))
    #define UFT_HOT                 __attribute__((hot))
    #define UFT_COLD                __attribute__((cold))
    /* UFT_PREFETCH / UFT_PREFETCH_W used to be defined in all three branches
     * here as well. uft_compiler.h — included at the bottom of this file —
     * defines UFT_PREFETCH too, with a different replacement list, so gcc
     * warned on every build. Owner is uft_compiler.h (MF-428); the write
     * variant is called UFT_PREFETCH_WRITE there. No caller used either. */
    
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
#ifndef UFT_UNUSED
    #define UFT_UNUSED              
#endif
    #define UFT_PURE                
    #define UFT_CONST               
    #define UFT_HOT                 
    #define UFT_COLD                
    #define UFT_ASSUME(cond)        __assume(cond)
    
#else
#ifndef UFT_LIKELY
    #define UFT_LIKELY(x)           (x)
#endif
#ifndef UFT_UNLIKELY
    #define UFT_UNLIKELY(x)         (x)
#endif
#ifndef UFT_UNUSED
    #define UFT_UNUSED              
#endif
    #define UFT_PURE                
    #define UFT_CONST               
    #define UFT_HOT                 
    #define UFT_COLD                
    #define UFT_ASSUME(cond)        ((void)0)
#endif

/*============================================================================
 * CACHE-LINE ALIGNMENT (False Sharing Prevention)
 *============================================================================*/

/* MF-456: UFT_CACHE_ALIGNED kommt aus uft_compiler.h. Hier stand dieselbe
 * Definition ein zweites Mal — identischer Text, aber sie haette sich mit dem
 * Eigentuemer verschoben, sobald der sich aendert. */

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
 * ERROR CODES — canonical definition in uft_unified_types.h
 *============================================================================*/

#include "uft/core/uft_unified_types.h"

#endif /* UFT_CONFIG_H */
