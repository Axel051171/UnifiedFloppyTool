/**
 * @file uft_threading.h
 * @brief Cross-platform threading abstraction for UFT
 * @version 1.0.0
 * 
 * Provides portable wrappers for:
 * - Mutexes (pthread_mutex_t / CRITICAL_SECTION)
 * - Condition variables (pthread_cond_t / CONDITION_VARIABLE)
 * - Threads (pthread_t / HANDLE)
 * - Time functions (clock_gettime / QueryPerformanceCounter)
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_THREADING_H
#define UFT_THREADING_H

/* POSIX Feature Test Macros - MUST be before any includes */
#if !defined(_WIN32) && !defined(_WIN64)
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
    #ifndef _DEFAULT_SOURCE
        #define _DEFAULT_SOURCE 1
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * PLATFORM DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

#if defined(_WIN32) || defined(_WIN64)
    #define UFT_PLATFORM_WINDOWS 1
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <time.h>    /* FIXED R19: Include BEFORE our timespec check - SDK may define it */
    #include <stdlib.h>  /* FIXED R19: Required for malloc/free used in this header */
#else
    #define UFT_PLATFORM_POSIX 1
    #include <pthread.h>
    #include <time.h>
    #include <unistd.h>
    #include <stdlib.h>  /* FIXED R19: Required for malloc/free used in this header */
    
    /* FIXED R20: Ensure CLOCK_* constants are available on all POSIX systems */
    #ifndef CLOCK_MONOTONIC
        #ifdef __APPLE__
            /* macOS uses CLOCK_MONOTONIC_RAW or we fall back to CLOCK_REALTIME */
            #ifdef CLOCK_MONOTONIC_RAW
                #define CLOCK_MONOTONIC CLOCK_MONOTONIC_RAW
            #else
                #define CLOCK_MONOTONIC 1
            #endif
        #else
            #define CLOCK_MONOTONIC 1
        #endif
    #endif
    #ifndef CLOCK_REALTIME
        #define CLOCK_REALTIME 0
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * TYPE DEFINITIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_PLATFORM_WINDOWS

/* Windows mutex type */
typedef struct {
    CRITICAL_SECTION cs;
    bool initialized;
} uft_mutex_t;

/* Windows condition variable */
typedef struct {
    CONDITION_VARIABLE cv;
    bool initialized;
} uft_cond_t;

/* Windows thread type */
typedef struct {
    HANDLE handle;
    DWORD id;
} uft_thread_t;

#else /* POSIX */

/* POSIX mutex type */
typedef struct {
    pthread_mutex_t mutex;
    bool initialized;
} uft_mutex_t;

/* POSIX condition variable */
typedef struct {
    pthread_cond_t cond;
    bool initialized;
} uft_cond_t;

/* POSIX thread type */
typedef struct {
    pthread_t thread;
} uft_thread_t;

#endif

/* Thread function type */
typedef void* (*uft_thread_func_t)(void* arg);

/* ═══════════════════════════════════════════════════════════════════════════
 * MUTEX FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize a mutex
 * @param mutex Pointer to mutex
 * @return 0 on success, -1 on failure
 */
static inline int uft_mutex_init(uft_mutex_t *mutex)
{
    if (!mutex) return -1;
#ifdef UFT_PLATFORM_WINDOWS
    InitializeCriticalSection(&mutex->cs);
    mutex->initialized = true;
    return 0;
#else
    int ret = pthread_mutex_init(&mutex->mutex, NULL);
    mutex->initialized = (ret == 0);
    return ret == 0 ? 0 : -1;
#endif
}

/**
 * @brief Lock a mutex
 * @param mutex Pointer to mutex
 * @return 0 on success, -1 on failure
 */
static inline int uft_mutex_lock(uft_mutex_t *mutex)
{
    if (!mutex || !mutex->initialized) return -1;
#ifdef UFT_PLATFORM_WINDOWS
    EnterCriticalSection(&mutex->cs);
    return 0;
#else
    return pthread_mutex_lock(&mutex->mutex) == 0 ? 0 : -1;
#endif
}

/**
 * @brief Unlock a mutex
 * @param mutex Pointer to mutex
 * @return 0 on success, -1 on failure
 */
static inline int uft_mutex_unlock(uft_mutex_t *mutex)
{
    if (!mutex || !mutex->initialized) return -1;
#ifdef UFT_PLATFORM_WINDOWS
    LeaveCriticalSection(&mutex->cs);
    return 0;
#else
    return pthread_mutex_unlock(&mutex->mutex) == 0 ? 0 : -1;
#endif
}

/**
 * @brief Destroy a mutex
 * @param mutex Pointer to mutex
 * @return 0 on success, -1 on failure
 */
static inline int uft_mutex_destroy(uft_mutex_t *mutex)
{
    if (!mutex || !mutex->initialized) return -1;
#ifdef UFT_PLATFORM_WINDOWS
    DeleteCriticalSection(&mutex->cs);
    mutex->initialized = false;
    return 0;
#else
    int ret = pthread_mutex_destroy(&mutex->mutex);
    mutex->initialized = false;
    return ret == 0 ? 0 : -1;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CONDITION VARIABLE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize a condition variable
 * @param cond Pointer to condition variable
 * @return 0 on success, -1 on failure
 */
static inline int uft_cond_init(uft_cond_t *cond)
{
    if (!cond) return -1;
#ifdef UFT_PLATFORM_WINDOWS
    InitializeConditionVariable(&cond->cv);
    cond->initialized = true;
    return 0;
#else
    int ret = pthread_cond_init(&cond->cond, NULL);
    cond->initialized = (ret == 0);
    return ret == 0 ? 0 : -1;
#endif
}

/**
 * @brief Wait on a condition variable
 * @param cond Pointer to condition variable
 * @param mutex Pointer to associated mutex
 * @return 0 on success, -1 on failure
 */
static inline int uft_cond_wait(uft_cond_t *cond, uft_mutex_t *mutex)
{
    if (!cond || !mutex || !cond->initialized || !mutex->initialized) return -1;
#ifdef UFT_PLATFORM_WINDOWS
    return SleepConditionVariableCS(&cond->cv, &mutex->cs, INFINITE) ? 0 : -1;
#else
    return pthread_cond_wait(&cond->cond, &mutex->mutex) == 0 ? 0 : -1;
#endif
}

/**
 * @brief Signal a condition variable (wake one waiter)
 * @param cond Pointer to condition variable
 * @return 0 on success, -1 on failure
 */
static inline int uft_cond_signal(uft_cond_t *cond)
{
    if (!cond || !cond->initialized) return -1;
#ifdef UFT_PLATFORM_WINDOWS
    WakeConditionVariable(&cond->cv);
    return 0;
#else
    return pthread_cond_signal(&cond->cond) == 0 ? 0 : -1;
#endif
}

/**
 * @brief Broadcast to a condition variable (wake all waiters)
 * @param cond Pointer to condition variable
 * @return 0 on success, -1 on failure
 */
static inline int uft_cond_broadcast(uft_cond_t *cond)
{
    if (!cond || !cond->initialized) return -1;
#ifdef UFT_PLATFORM_WINDOWS
    WakeAllConditionVariable(&cond->cv);
    return 0;
#else
    return pthread_cond_broadcast(&cond->cond) == 0 ? 0 : -1;
#endif
}

/**
 * @brief Destroy a condition variable
 * @param cond Pointer to condition variable
 * @return 0 on success, -1 on failure
 */
static inline int uft_cond_destroy(uft_cond_t *cond)
{
    if (!cond || !cond->initialized) return -1;
#ifdef UFT_PLATFORM_WINDOWS
    /* Windows condition variables don't need explicit destruction */
    cond->initialized = false;
    return 0;
#else
    int ret = pthread_cond_destroy(&cond->cond);
    cond->initialized = false;
    return ret == 0 ? 0 : -1;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════
 * THREAD FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_PLATFORM_WINDOWS
/* Windows thread wrapper structure */
typedef struct {
    uft_thread_func_t func;
    void *arg;
} uft_thread_wrapper_t;

/* Windows thread entry point */
static DWORD WINAPI uft_thread_wrapper_func(LPVOID lpParam)
{
    uft_thread_wrapper_t *wrapper = (uft_thread_wrapper_t *)lpParam;
    uft_thread_func_t func = wrapper->func;
    void *arg = wrapper->arg;
    free(wrapper);
    func(arg);
    return 0;
}
#endif

/**
 * @brief Create a new thread
 * @param thread Pointer to thread handle
 * @param func Thread function
 * @param arg Argument to pass to thread function
 * @return 0 on success, -1 on failure
 */
static inline int uft_thread_create(uft_thread_t *thread, uft_thread_func_t func, void *arg)
{
    if (!thread || !func) return -1;
#ifdef UFT_PLATFORM_WINDOWS
    uft_thread_wrapper_t *wrapper = (uft_thread_wrapper_t *)malloc(sizeof(uft_thread_wrapper_t));
    if (!wrapper) return -1;
    wrapper->func = func;
    wrapper->arg = arg;
    thread->handle = CreateThread(NULL, 0, uft_thread_wrapper_func, wrapper, 0, &thread->id);
    if (!thread->handle) {
        free(wrapper);
        return -1;
    }
    return 0;
#else
    return pthread_create(&thread->thread, NULL, func, arg) == 0 ? 0 : -1;
#endif
}

/**
 * @brief Wait for a thread to complete
 * @param thread Pointer to thread handle
 * @param retval Optional pointer to store return value (unused on Windows)
 * @return 0 on success, -1 on failure
 */
static inline int uft_thread_join(uft_thread_t *thread, void **retval)
{
    if (!thread) return -1;
    (void)retval; /* Unused on Windows */
#ifdef UFT_PLATFORM_WINDOWS
    if (WaitForSingleObject(thread->handle, INFINITE) == WAIT_FAILED) {
        return -1;
    }
    CloseHandle(thread->handle);
    thread->handle = NULL;
    return 0;
#else
    return pthread_join(thread->thread, retval) == 0 ? 0 : -1;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TIME FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get current timestamp in microseconds (monotonic clock)
 * @return Timestamp in microseconds
 */
static inline uint64_t uft_time_get_us(void)
{
#ifdef UFT_PLATFORM_WINDOWS
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000ULL) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
#endif
}

/**
 * @brief Get current time in microseconds (wall clock)
 * @return Wall clock time in microseconds since epoch
 */
static inline uint64_t uft_time_get_realtime_us(void)
{
#ifdef UFT_PLATFORM_WINDOWS
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    /* Convert from 100-ns intervals since 1601 to microseconds since 1970 */
    return (uli.QuadPart - 116444736000000000ULL) / 10ULL;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
#endif
}

/**
 * @brief Generate a unique session ID based on current time
 * @return Unique session ID
 */
static inline uint64_t uft_generate_session_id(void)
{
#ifdef UFT_PLATFORM_WINDOWS
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return ((uli.QuadPart >> 20) ^ GetCurrentProcessId()) | (GetTickCount64() & 0xFFFFF);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ((uint64_t)ts.tv_sec << 20) | ((uint64_t)ts.tv_nsec & 0xFFFFF);
#endif
}

/**
 * @brief Sleep for specified milliseconds
 * @param ms Milliseconds to sleep
 */
static inline void uft_sleep_ms(uint32_t ms)
{
#ifdef UFT_PLATFORM_WINDOWS
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════
 * COMPATIBILITY MACROS (for legacy code migration)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* These macros allow gradual migration from pthread to uft_threading */
#ifndef UFT_NO_PTHREAD_COMPAT

#ifdef UFT_PLATFORM_WINDOWS
    /* Provide pthread-like macros for Windows */
    #define pthread_mutex_t         uft_mutex_t
    #define pthread_mutex_init(m,a) uft_mutex_init(m)
    #define pthread_mutex_lock      uft_mutex_lock
    #define pthread_mutex_unlock    uft_mutex_unlock
    #define pthread_mutex_destroy   uft_mutex_destroy
    
    #define pthread_cond_t          uft_cond_t
    #define pthread_cond_init(c,a)  uft_cond_init(c)
    #define pthread_cond_wait       uft_cond_wait
    #define pthread_cond_signal     uft_cond_signal
    #define pthread_cond_broadcast  uft_cond_broadcast
    #define pthread_cond_destroy    uft_cond_destroy
    
    #define pthread_t               uft_thread_t
    #define pthread_create(t,a,f,p) uft_thread_create(t, f, p)
    #define pthread_join(t,r)       uft_thread_join(&(t), r)
    
    /* Time compatibility */
    #ifndef CLOCK_MONOTONIC
        #define CLOCK_MONOTONIC 1
    #endif
    #ifndef CLOCK_REALTIME
        #define CLOCK_REALTIME 0
    #endif
    
    /* 
     * FIXED R20: Windows SDK 10.0.26100+ defines struct timespec in <time.h>
     * We DO NOT define our own - just use the SDK's definition.
     * The <time.h> include above ensures it's available.
     */
    
    /* clock_gettime implementation for Windows */
    static inline int clock_gettime(int clk_id, struct timespec *ts)
    {
        if (!ts) return -1;
        if (clk_id == CLOCK_MONOTONIC) {
            uint64_t us = uft_time_get_us();
            ts->tv_sec = (long)(us / 1000000ULL);
            ts->tv_nsec = (long)((us % 1000000ULL) * 1000ULL);
        } else {
            uint64_t us = uft_time_get_realtime_us();
            ts->tv_sec = (long)(us / 1000000ULL);
            ts->tv_nsec = (long)((us % 1000000ULL) * 1000ULL);
        }
        return 0;
    }
#endif /* UFT_PLATFORM_WINDOWS */

#endif /* UFT_NO_PTHREAD_COMPAT */

#ifdef __cplusplus
}
#endif

#endif /* UFT_THREADING_H */
