/**
 * @file uft_atomics.h
 * @brief Cross-platform atomic operations
 * @version 1.0.0
 * 
 * Provides portable atomic operations for:
 * - GCC/Clang (C11 stdatomic)
 * - MSVC (Interlocked functions)
 */

#ifndef UFT_ATOMICS_H
#define UFT_ATOMICS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Platform Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(_MSC_VER)
    /* MSVC - use Interlocked functions */
    #define UFT_ATOMICS_MSVC 1
    #include <windows.h>
    #include <intrin.h>
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
    /* C11 with atomics support */
    #define UFT_ATOMICS_C11 1
    #include <stdatomic.h>
#elif defined(__GNUC__) || defined(__clang__)
    /* GCC/Clang builtin atomics */
    #define UFT_ATOMICS_GCC 1
#else
    /* Fallback - no atomics, use volatile */
    #define UFT_ATOMICS_NONE 1
    #warning "No atomic support - using volatile fallback (not thread-safe)"
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Atomic Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(UFT_ATOMICS_C11)
    typedef atomic_int uft_atomic_int;
    typedef atomic_bool uft_atomic_bool;
    typedef atomic_size_t uft_atomic_size;
    typedef _Atomic(void*) uft_atomic_ptr;
#elif defined(UFT_ATOMICS_MSVC)
    typedef volatile LONG uft_atomic_int;
    typedef volatile LONG uft_atomic_bool;
    typedef volatile LONG64 uft_atomic_size;
    typedef void* volatile uft_atomic_ptr;
#elif defined(UFT_ATOMICS_GCC)
    typedef int uft_atomic_int;
    typedef int uft_atomic_bool;
    typedef size_t uft_atomic_size;
    typedef void* uft_atomic_ptr;
#else
    typedef volatile int uft_atomic_int;
    typedef volatile int uft_atomic_bool;
    typedef volatile size_t uft_atomic_size;
    typedef void* volatile uft_atomic_ptr;
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Atomic Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Initialize */
#if defined(UFT_ATOMICS_C11)
    #define UFT_ATOMIC_INIT(val) ATOMIC_VAR_INIT(val)
#else
    #define UFT_ATOMIC_INIT(val) (val)
#endif

/* Load */
#if defined(UFT_ATOMICS_C11)
    #define uft_atomic_load(ptr) atomic_load(ptr)
#elif defined(UFT_ATOMICS_MSVC)
    #define uft_atomic_load(ptr) (*(ptr))
#elif defined(UFT_ATOMICS_GCC)
    #define uft_atomic_load(ptr) __atomic_load_n(ptr, __ATOMIC_SEQ_CST)
#else
    #define uft_atomic_load(ptr) (*(ptr))
#endif

/* Store */
#if defined(UFT_ATOMICS_C11)
    #define uft_atomic_store(ptr, val) atomic_store(ptr, val)
#elif defined(UFT_ATOMICS_MSVC)
    #define uft_atomic_store(ptr, val) (*(ptr) = (val))
#elif defined(UFT_ATOMICS_GCC)
    #define uft_atomic_store(ptr, val) __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST)
#else
    #define uft_atomic_store(ptr, val) (*(ptr) = (val))
#endif

/* Fetch and Add */
#if defined(UFT_ATOMICS_C11)
    #define uft_atomic_fetch_add(ptr, val) atomic_fetch_add(ptr, val)
#elif defined(UFT_ATOMICS_MSVC)
    #define uft_atomic_fetch_add(ptr, val) InterlockedExchangeAdd((LONG*)(ptr), (LONG)(val))
#elif defined(UFT_ATOMICS_GCC)
    #define uft_atomic_fetch_add(ptr, val) __atomic_fetch_add(ptr, val, __ATOMIC_SEQ_CST)
#else
    static inline int uft_atomic_fetch_add_fallback(volatile int* ptr, int val) {
        int old = *ptr; *ptr += val; return old;
    }
    #define uft_atomic_fetch_add(ptr, val) uft_atomic_fetch_add_fallback(ptr, val)
#endif

/* Fetch and Sub */
#if defined(UFT_ATOMICS_C11)
    #define uft_atomic_fetch_sub(ptr, val) atomic_fetch_sub(ptr, val)
#elif defined(UFT_ATOMICS_MSVC)
    #define uft_atomic_fetch_sub(ptr, val) InterlockedExchangeAdd((LONG*)(ptr), -(LONG)(val))
#elif defined(UFT_ATOMICS_GCC)
    #define uft_atomic_fetch_sub(ptr, val) __atomic_fetch_sub(ptr, val, __ATOMIC_SEQ_CST)
#else
    #define uft_atomic_fetch_sub(ptr, val) uft_atomic_fetch_add_fallback(ptr, -(val))
#endif

/* Compare and Exchange */
#if defined(UFT_ATOMICS_C11)
    #define uft_atomic_compare_exchange(ptr, expected, desired) \
        atomic_compare_exchange_strong(ptr, expected, desired)
#elif defined(UFT_ATOMICS_MSVC)
    static inline bool uft_atomic_cas_msvc(volatile LONG* ptr, LONG* expected, LONG desired) {
        LONG old = InterlockedCompareExchange(ptr, desired, *expected);
        if (old == *expected) return true;
        *expected = old;
        return false;
    }
    #define uft_atomic_compare_exchange(ptr, expected, desired) \
        uft_atomic_cas_msvc((volatile LONG*)(ptr), (LONG*)(expected), (LONG)(desired))
#elif defined(UFT_ATOMICS_GCC)
    #define uft_atomic_compare_exchange(ptr, expected, desired) \
        __atomic_compare_exchange_n(ptr, expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#else
    static inline bool uft_atomic_cas_fallback(volatile int* ptr, int* expected, int desired) {
        if (*ptr == *expected) { *ptr = desired; return true; }
        *expected = *ptr; return false;
    }
    #define uft_atomic_compare_exchange(ptr, expected, desired) \
        uft_atomic_cas_fallback(ptr, expected, desired)
#endif

/* Memory Fence */
#if defined(UFT_ATOMICS_C11)
    #define uft_atomic_fence() atomic_thread_fence(memory_order_seq_cst)
#elif defined(UFT_ATOMICS_MSVC)
    #define uft_atomic_fence() MemoryBarrier()
#elif defined(UFT_ATOMICS_GCC)
    #define uft_atomic_fence() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#else
    #define uft_atomic_fence() ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATOMICS_H */
