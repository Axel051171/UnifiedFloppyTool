/**
 * @file uft_atomics.h
 * @brief Portable Atomic Operations
 * 
 * Provides cross-platform atomic operations for:
 * - C11 (GCC/Clang with stdatomic.h)
 * - MSVC (using Interlocked intrinsics)
 */

#ifndef UFT_ATOMICS_H
#define UFT_ATOMICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Platform Detection
 *===========================================================================*/

#if defined(_MSC_VER)
    /* Microsoft Visual C++ */
    #define UFT_ATOMICS_MSVC 1
    #include <intrin.h>
    #include <windows.h>
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
    /* C11 with atomics support */
    #define UFT_ATOMICS_C11 1
    #include <stdatomic.h>
#elif defined(__GNUC__) || defined(__clang__)
    /* GCC/Clang builtins */
    #define UFT_ATOMICS_GNUC 1
#else
    /* Fallback: no atomics, use volatile */
    #define UFT_ATOMICS_NONE 1
    #warning "No atomic support detected, using volatile (not thread-safe)"
#endif

/*===========================================================================
 * Atomic Types
 *===========================================================================*/

#if defined(UFT_ATOMICS_MSVC)
    typedef volatile long atomic_int;
    typedef volatile long atomic_bool;
    typedef volatile int64_t atomic_int64_t;
    typedef volatile uint64_t atomic_uint_fast64_t;
#elif defined(UFT_ATOMICS_C11)
    /* Use C11 types directly */
    #ifndef __cplusplus
    typedef _Atomic int atomic_int;
    typedef _Atomic _Bool atomic_bool;
    typedef _Atomic int64_t atomic_int64_t;
    typedef _Atomic uint_fast64_t atomic_uint_fast64_t;
    #else
    /* C++ mode */
    typedef volatile int atomic_int;
    typedef volatile bool atomic_bool;
    typedef volatile int64_t atomic_int64_t;
    typedef volatile uint64_t atomic_uint_fast64_t;
    #endif
#elif defined(UFT_ATOMICS_GNUC)
    typedef volatile int atomic_int;
    typedef volatile int atomic_bool;
    typedef volatile int64_t atomic_int64_t;
    typedef volatile uint64_t atomic_uint_fast64_t;
#else
    typedef volatile int atomic_int;
    typedef volatile int atomic_bool;
    typedef volatile int64_t atomic_int64_t;
    typedef volatile uint64_t atomic_uint_fast64_t;
#endif

/*===========================================================================
 * Atomic Operations
 *===========================================================================*/

/* Only define if not already defined (prevents conflicts with stdatomic.h) */
#if defined(UFT_ATOMICS_MSVC)

    #ifndef atomic_load
    #define atomic_load(ptr)            (*(ptr))
    #endif
    #ifndef atomic_store
    #define atomic_store(ptr, val)      (*(ptr) = (val))
    #endif
    #ifndef atomic_fetch_add
    #define atomic_fetch_add(ptr, val)  InterlockedExchangeAdd((ptr), (val))
    #endif
    #ifndef atomic_fetch_sub
    #define atomic_fetch_sub(ptr, val)  InterlockedExchangeAdd((ptr), -(val))
    #endif
    #ifndef atomic_exchange
    #define atomic_exchange(ptr, val)   InterlockedExchange((ptr), (val))
    #endif
    
    static inline int atomic_compare_exchange_strong(atomic_int *ptr, int *expected, int desired) {
        long old = InterlockedCompareExchange((volatile long*)ptr, desired, *expected);
        if (old == *expected) return 1;
        *expected = old;
        return 0;
    }

#elif defined(UFT_ATOMICS_C11)

    #ifndef atomic_load
    #define atomic_load(ptr)            atomic_load_explicit(ptr, memory_order_seq_cst)
    #endif
    #ifndef atomic_store
    #define atomic_store(ptr, val)      atomic_store_explicit(ptr, val, memory_order_seq_cst)
    #endif
    #ifndef atomic_fetch_add
    #define atomic_fetch_add(ptr, val)  atomic_fetch_add_explicit(ptr, val, memory_order_seq_cst)
    #endif
    #ifndef atomic_fetch_sub
    #define atomic_fetch_sub(ptr, val)  atomic_fetch_sub_explicit(ptr, val, memory_order_seq_cst)
    #endif
    #ifndef atomic_exchange
    #define atomic_exchange(ptr, val)   atomic_exchange_explicit(ptr, val, memory_order_seq_cst)
    #endif
    /* atomic_compare_exchange_strong is already available in C11 */

#elif defined(UFT_ATOMICS_GNUC)

    #ifndef atomic_load
    #define atomic_load(ptr)            __atomic_load_n(ptr, __ATOMIC_SEQ_CST)
    #endif
    #ifndef atomic_store
    #define atomic_store(ptr, val)      __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST)
    #endif
    #ifndef atomic_fetch_add
    #define atomic_fetch_add(ptr, val)  __atomic_fetch_add(ptr, val, __ATOMIC_SEQ_CST)
    #endif
    #ifndef atomic_fetch_sub
    #define atomic_fetch_sub(ptr, val)  __atomic_fetch_sub(ptr, val, __ATOMIC_SEQ_CST)
    #endif
    #ifndef atomic_exchange
    #define atomic_exchange(ptr, val)   __atomic_exchange_n(ptr, val, __ATOMIC_SEQ_CST)
    #endif
    
    static inline int atomic_compare_exchange_strong(atomic_int *ptr, int *expected, int desired) {
        return __atomic_compare_exchange_n(ptr, expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }

#else
    /* Fallback - not thread safe! */
    #define atomic_load(ptr)            (*(ptr))
    #define atomic_store(ptr, val)      (*(ptr) = (val))
    #define atomic_fetch_add(ptr, val)  ((*(ptr))++)
    #define atomic_fetch_sub(ptr, val)  ((*(ptr))--)
    #define atomic_exchange(ptr, val)   ({ int _old = *(ptr); *(ptr) = (val); _old; })
#endif

/*===========================================================================
 * Memory Barriers
 *===========================================================================*/

#if defined(UFT_ATOMICS_MSVC)
    #define atomic_thread_fence(order)  MemoryBarrier()
#elif defined(UFT_ATOMICS_C11)
    /* atomic_thread_fence already available */
#elif defined(UFT_ATOMICS_GNUC)
    #define atomic_thread_fence(order)  __atomic_thread_fence(__ATOMIC_SEQ_CST)
#else
    #define atomic_thread_fence(order)  ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATOMICS_H */
