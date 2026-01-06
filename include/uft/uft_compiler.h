/**
 * @file uft_compiler.h
 * @brief Compiler-specific macros and attributes
 * @version 1.0.0
 * 
 * Provides portable definitions for:
 * - Function attributes (hot, cold, inline, pure, const)
 * - Memory alignment
 * - Restrict pointers
 * - Branch prediction hints
 * - Cache line alignment
 */

#ifndef UFT_COMPILER_H
#define UFT_COMPILER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Compiler Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(__GNUC__) || defined(__clang__)
    #define UFT_GCC_COMPAT 1
#endif

#if defined(_MSC_VER)
    #define UFT_MSVC 1
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Function Attributes
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief Mark function as performance-critical (hot path) */
#if defined(UFT_GCC_COMPAT)
    #define UFT_HOT __attribute__((hot))
    #define UFT_COLD __attribute__((cold))
    #define UFT_PURE __attribute__((pure))
    #define UFT_CONST __attribute__((const))
    #define UFT_UNUSED __attribute__((unused))
    #define UFT_DEPRECATED __attribute__((deprecated))
    #define UFT_NOINLINE __attribute__((noinline))
    #define UFT_ALWAYS_INLINE __attribute__((always_inline)) inline
#elif defined(UFT_MSVC)
    #define UFT_HOT
    #define UFT_COLD
    #define UFT_PURE
    #define UFT_CONST
    #define UFT_UNUSED
    #define UFT_DEPRECATED __declspec(deprecated)
    #define UFT_NOINLINE __declspec(noinline)
    #define UFT_ALWAYS_INLINE __forceinline
#else
    #define UFT_HOT
    #define UFT_COLD
    #define UFT_PURE
    #define UFT_CONST
    #define UFT_UNUSED
    #define UFT_DEPRECATED
    #define UFT_NOINLINE
    #define UFT_ALWAYS_INLINE inline
#endif

/** @brief Force inline (stronger than inline) */
#define UFT_INLINE UFT_ALWAYS_INLINE

/* ═══════════════════════════════════════════════════════════════════════════════
 * Pointer Attributes
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief Restrict pointer (no aliasing) */
#if defined(UFT_GCC_COMPAT)
    #define UFT_RESTRICT __restrict__
#elif defined(UFT_MSVC)
    #define UFT_RESTRICT __restrict
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    #define UFT_RESTRICT restrict
#else
    #define UFT_RESTRICT
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Memory Alignment
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief Default cache line size */
#ifndef UFT_CACHE_LINE_SIZE
    #define UFT_CACHE_LINE_SIZE 64
#endif

/** @brief Align to cache line boundary */
#if defined(UFT_GCC_COMPAT)
    #define UFT_CACHE_ALIGNED __attribute__((aligned(UFT_CACHE_LINE_SIZE)))
    #define UFT_ALIGNED(n) __attribute__((aligned(n)))
#elif defined(UFT_MSVC)
    #define UFT_CACHE_ALIGNED __declspec(align(UFT_CACHE_LINE_SIZE))
    #define UFT_ALIGNED(n) __declspec(align(n))
#else
    #define UFT_CACHE_ALIGNED
    #define UFT_ALIGNED(n)
#endif

/** @brief Align to 16-byte boundary (SSE) */
#define UFT_SSE_ALIGNED UFT_ALIGNED(16)

/** @brief Align to 32-byte boundary (AVX) */
#define UFT_AVX_ALIGNED UFT_ALIGNED(32)

/** @brief Align to 64-byte boundary (AVX-512) */
#define UFT_AVX512_ALIGNED UFT_ALIGNED(64)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Branch Prediction Hints
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(UFT_GCC_COMPAT)
    #define UFT_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UFT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define UFT_LIKELY(x)   (x)
    #define UFT_UNLIKELY(x) (x)
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Prefetch Hints
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(UFT_GCC_COMPAT)
    /** @brief Prefetch for read */
    #define UFT_PREFETCH_READ(addr)  __builtin_prefetch((addr), 0, 3)
    /** @brief Prefetch for write */
    #define UFT_PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)
#else
    #define UFT_PREFETCH_READ(addr)
    #define UFT_PREFETCH_WRITE(addr)
#endif

/** @brief Generic prefetch (alias for UFT_PREFETCH_READ) */
#define UFT_PREFETCH(addr) UFT_PREFETCH_READ(addr)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Packed Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief Begin packed structure region */
#if defined(UFT_MSVC)
    #define UFT_PACK_BEGIN __pragma(pack(push, 1))
    #define UFT_PACK_END   __pragma(pack(pop))
    #define UFT_PACKED
#elif defined(UFT_GCC_COMPAT)
    #define UFT_PACK_BEGIN
    #define UFT_PACK_END
    #define UFT_PACKED __attribute__((packed))
#else
    #define UFT_PACK_BEGIN
    #define UFT_PACK_END
    #define UFT_PACKED
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Static Assertions
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define UFT_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#elif defined(__cplusplus) && __cplusplus >= 201103L
    #define UFT_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
    #define UFT_STATIC_ASSERT(cond, msg) \
        typedef char uft_static_assert_##__LINE__[(cond) ? 1 : -1]
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Thread Local Storage
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(UFT_GCC_COMPAT)
    #define UFT_THREAD_LOCAL __thread
#elif defined(UFT_MSVC)
    #define UFT_THREAD_LOCAL __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define UFT_THREAD_LOCAL _Thread_local
#else
    #define UFT_THREAD_LOCAL
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Export/Import for Shared Libraries
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(UFT_MSVC)
    #ifdef UFT_BUILDING_DLL
        #define UFT_API __declspec(dllexport)
    #else
        #define UFT_API __declspec(dllimport)
    #endif
    #define UFT_WEAK  /* MSVC doesn't support weak symbols */
#elif defined(UFT_GCC_COMPAT)
    #define UFT_API __attribute__((visibility("default")))
    #define UFT_WEAK __attribute__((weak))
#else
    #define UFT_API
    #define UFT_WEAK
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_COMPILER_H */
