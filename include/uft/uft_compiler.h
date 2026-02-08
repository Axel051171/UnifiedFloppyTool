#ifndef UFT_COMPILER_H
#define UFT_COMPILER_H

/**
 * @file uft_compiler.h
 * @brief ZENTRALE AUTORITATIVE Compiler-Makros für UFT
 * @version 2.0.0 (R19)
 * 
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║ WICHTIG: Dies ist die EINZIGE autoritative Quelle für Compiler-Makros!   ║
 * ║ Alle anderen Header MÜSSEN diesen Header inkludieren statt eigene        ║
 * ║ Definitionen zu erstellen.                                               ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * 
 * Definiert:
 * - UFT_INLINE / UFT_FORCE_INLINE / UFT_NOINLINE
 * - UFT_PACK_BEGIN / UFT_PACK_END / UFT_PACKED
 * - UFT_ALIGNED / UFT_CACHE_ALIGNED
 * - UFT_LIKELY / UFT_UNLIKELY
 * - UFT_UNUSED / UFT_DEPRECATED
 * - UFT_RESTRICT
 * - UFT_API (DLL export/import)
 * - UFT_STATIC_ASSERT
 * - UFT_THREAD_LOCAL
 * 
 * Unterstützte Compiler:
 * - MSVC (_MSC_VER)
 * - GCC (__GNUC__)
 * - Clang (__clang__)
 * 
 * SPDX-License-Identifier: MIT
 */


/* =============================================================================
 * STANDARD HEADERS - Jeder Header muss selbstständig sein!
 * ============================================================================= */

#include <stddef.h>   /* size_t, NULL, ptrdiff_t */
#include <stdint.h>   /* uint8_t, uint16_t, uint32_t, uint64_t, etc. */
#include <stdbool.h>  /* bool, true, false */

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * COMPILER DETECTION
 * ============================================================================= */

#if defined(_MSC_VER)
    #define UFT_COMPILER_MSVC    1
    #define UFT_COMPILER_VERSION _MSC_VER
#elif defined(__clang__)
    #define UFT_COMPILER_CLANG   1
    #define UFT_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100)
#elif defined(__GNUC__)
    #define UFT_COMPILER_GCC     1
    #define UFT_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100)
#else
    #define UFT_COMPILER_UNKNOWN 1
    #define UFT_COMPILER_VERSION 0
#endif

/* Helper für GCC/Clang gemeinsam */
#if defined(__GNUC__) || defined(__clang__)
    #define UFT_GCC_COMPAT 1
#endif

/* =============================================================================
 * INLINE HINTS
 * Zentrale, einmalige Definition - KEINE Duplikate erlaubt!
 * ============================================================================= */

#undef UFT_INLINE
#undef UFT_FORCE_INLINE
#undef UFT_NOINLINE

#if defined(UFT_COMPILER_MSVC)
    #define UFT_INLINE          static __inline
    #define UFT_FORCE_INLINE    static __forceinline
    #define UFT_NOINLINE        __declspec(noinline)
#elif defined(UFT_GCC_COMPAT)
    #define UFT_INLINE          static inline
    #define UFT_FORCE_INLINE    static inline __attribute__((always_inline))
    #define UFT_NOINLINE        __attribute__((noinline))
#else
    #define UFT_INLINE          static inline
    #define UFT_FORCE_INLINE    static inline
    #define UFT_NOINLINE
#endif

/* Legacy alias */
#define UFT_ALWAYS_INLINE UFT_FORCE_INLINE

/* =============================================================================
 * PACKED STRUCTURES
 * 
 * VERWENDUNG:
 *   UFT_PACK_BEGIN
 *   typedef struct {
 *       uint8_t  a;
 *       uint32_t b;
 *   } my_packed_t;
 *   UFT_PACK_END
 * 
 * MSVC:      #pragma pack(push,1) ... #pragma pack(pop)
 * GCC/Clang: Auch pragma pack für Konsistenz
 * ============================================================================= */

#undef UFT_PACK_BEGIN
#undef UFT_PACK_END
#undef UFT_PACKED
#undef UFT_PACKED_STRUCT
#undef UFT_PACKED_BEGIN
#undef UFT_PACKED_END
#undef UFT_PACKED_ATTR

#if defined(UFT_COMPILER_MSVC)
    #define UFT_PACK_BEGIN      __pragma(pack(push, 1))
    #define UFT_PACK_END        __pragma(pack(pop))
    #define UFT_PACKED          /* empty - packing via pragma */
#elif defined(UFT_GCC_COMPAT)
    /* GCC/Clang: pragma pack für cross-platform Konsistenz */
    #define UFT_PACK_BEGIN      _Pragma("pack(push, 1)")
    #define UFT_PACK_END        _Pragma("pack(pop)")
    #define UFT_PACKED          /* empty - packing via pragma */
#else
    #define UFT_PACK_BEGIN
    #define UFT_PACK_END
    #define UFT_PACKED
#endif

/* Legacy aliases */
#define UFT_PACKED_BEGIN    UFT_PACK_BEGIN
#define UFT_PACKED_END      UFT_PACK_END
#define UFT_PACKED_STRUCT   UFT_PACKED
#define UFT_PACKED_ATTR     UFT_PACKED

/* =============================================================================
 * ALIGNMENT
 * ============================================================================= */

#undef UFT_ALIGNED
#undef UFT_CACHE_ALIGNED
#undef UFT_ALIGNOF

#ifndef UFT_CACHE_LINE_SIZE
    #define UFT_CACHE_LINE_SIZE 64
#endif

#if defined(UFT_COMPILER_MSVC)
    #define UFT_ALIGNED(n)      __declspec(align(n))
    #define UFT_ALIGNOF(type)   __alignof(type)
#elif defined(UFT_GCC_COMPAT)
    #define UFT_ALIGNED(n)      __attribute__((aligned(n)))
    #define UFT_ALIGNOF(type)   __alignof__(type)
#else
    #define UFT_ALIGNED(n)
    #define UFT_ALIGNOF(type)   sizeof(type)
#endif

#define UFT_CACHE_ALIGNED   UFT_ALIGNED(UFT_CACHE_LINE_SIZE)
#define UFT_SSE_ALIGNED     UFT_ALIGNED(16)
#define UFT_AVX_ALIGNED     UFT_ALIGNED(32)
#define UFT_AVX512_ALIGNED  UFT_ALIGNED(64)

/* =============================================================================
 * BRANCH PREDICTION
 * ============================================================================= */

#undef UFT_LIKELY
#undef UFT_UNLIKELY

#if defined(UFT_GCC_COMPAT)
    #define UFT_LIKELY(x)       __builtin_expect(!!(x), 1)
    #define UFT_UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
    #define UFT_LIKELY(x)       (x)
    #define UFT_UNLIKELY(x)     (x)
#endif

/* =============================================================================
 * UNUSED / DEPRECATED
 * ============================================================================= */

#undef UFT_UNUSED
#undef UFT_DEPRECATED
#undef UFT_MAYBE_UNUSED

#if defined(UFT_GCC_COMPAT)
    #define UFT_UNUSED          __attribute__((unused))
    #define UFT_MAYBE_UNUSED    __attribute__((unused))
    #define UFT_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(UFT_COMPILER_MSVC)
    #define UFT_UNUSED
    #define UFT_MAYBE_UNUSED
    #define UFT_DEPRECATED(msg) __declspec(deprecated(msg))
#else
    #define UFT_UNUSED
    #define UFT_MAYBE_UNUSED
    #define UFT_DEPRECATED(msg)
#endif

/* Macro to silence unused parameter warnings */
#define UFT_UNUSED_PARAM(x)     (void)(x)

/* =============================================================================
 * RESTRICT POINTER
 * ============================================================================= */

#undef UFT_RESTRICT

#if defined(UFT_COMPILER_MSVC)
    #define UFT_RESTRICT        __restrict
#elif defined(UFT_GCC_COMPAT)
    #define UFT_RESTRICT        __restrict__
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    #define UFT_RESTRICT        restrict
#else
    #define UFT_RESTRICT
#endif

/* =============================================================================
 * DLL EXPORT/IMPORT
 * ============================================================================= */

#undef UFT_API
#undef UFT_WEAK

#if defined(_WIN32) || defined(_WIN64)
    #ifdef UFT_BUILD_DLL
        #define UFT_API         __declspec(dllexport)
    #elif defined(UFT_USE_DLL)
        #define UFT_API         __declspec(dllimport)
    #else
        #define UFT_API
    #endif
    #define UFT_WEAK            /* MSVC has no weak symbols */
#elif defined(UFT_GCC_COMPAT)
    #define UFT_API             __attribute__((visibility("default")))
    #define UFT_WEAK            __attribute__((weak))
#else
    #define UFT_API
    #define UFT_WEAK
#endif

/* =============================================================================
 * STATIC ASSERT
 * ============================================================================= */

#undef UFT_STATIC_ASSERT

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define UFT_STATIC_ASSERT(cond, msg)    _Static_assert(cond, msg)
#elif defined(__cplusplus) && __cplusplus >= 201103L
    #define UFT_STATIC_ASSERT(cond, msg)    static_assert(cond, msg)
#elif defined(UFT_COMPILER_MSVC)
    #define UFT_STATIC_ASSERT(cond, msg)    static_assert(cond, msg)
#else
    /* Fallback für alte Compiler */
    #define UFT_STATIC_ASSERT_IMPL(cond, line) \
        typedef char uft_static_assert_##line[(cond) ? 1 : -1]
    #define UFT_STATIC_ASSERT(cond, msg)    UFT_STATIC_ASSERT_IMPL(cond, __LINE__)
#endif

/* =============================================================================
 * THREAD LOCAL STORAGE
 * ============================================================================= */

#undef UFT_THREAD_LOCAL

#if defined(UFT_COMPILER_MSVC)
    #define UFT_THREAD_LOCAL    __declspec(thread)
#elif defined(UFT_GCC_COMPAT)
    #define UFT_THREAD_LOCAL    __thread
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define UFT_THREAD_LOCAL    _Thread_local
#else
    #define UFT_THREAD_LOCAL    /* no thread local support */
#endif

/* =============================================================================
 * FUNCTION ATTRIBUTES
 * ============================================================================= */

#undef UFT_HOT
#undef UFT_COLD
#undef UFT_PURE
#undef UFT_CONST
#undef UFT_PRINTF_FMT
#undef UFT_WARN_UNUSED_RESULT

#if defined(UFT_GCC_COMPAT)
    #define UFT_HOT                 __attribute__((hot))
    #define UFT_COLD                __attribute__((cold))
    #define UFT_PURE                __attribute__((pure))
    #define UFT_CONST               __attribute__((const))
    #define UFT_PRINTF_FMT(f, a)    __attribute__((format(printf, f, a)))
    #define UFT_WARN_UNUSED_RESULT  __attribute__((warn_unused_result))
#else
    #define UFT_HOT
    #define UFT_COLD
    #define UFT_PURE
    #define UFT_CONST
    #define UFT_PRINTF_FMT(f, a)
    #define UFT_WARN_UNUSED_RESULT
#endif

/* =============================================================================
 * PREFETCH HINTS
 * ============================================================================= */

#if defined(UFT_GCC_COMPAT)
    #define UFT_PREFETCH_READ(addr)     __builtin_prefetch((addr), 0, 3)
    #define UFT_PREFETCH_WRITE(addr)    __builtin_prefetch((addr), 1, 3)
#else
    #define UFT_PREFETCH_READ(addr)     ((void)(addr))
    #define UFT_PREFETCH_WRITE(addr)    ((void)(addr))
#endif

#define UFT_PREFETCH(addr)  UFT_PREFETCH_READ(addr)

/* =============================================================================
 * WARNING SUPPRESSION
 * ============================================================================= */

#if defined(UFT_COMPILER_MSVC)
    #define UFT_DISABLE_WARNING_PUSH    __pragma(warning(push))
    #define UFT_DISABLE_WARNING_POP     __pragma(warning(pop))
    #define UFT_DISABLE_WARNING(num)    __pragma(warning(disable: num))
#elif defined(UFT_GCC_COMPAT)
    #define UFT_DISABLE_WARNING_PUSH    _Pragma("GCC diagnostic push")
    #define UFT_DISABLE_WARNING_POP     _Pragma("GCC diagnostic pop")
    #define UFT_DISABLE_WARNING(warn)   /* per-warning pragma needed */
#else
    #define UFT_DISABLE_WARNING_PUSH
    #define UFT_DISABLE_WARNING_POP
    #define UFT_DISABLE_WARNING(x)
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_COMPILER_H */
