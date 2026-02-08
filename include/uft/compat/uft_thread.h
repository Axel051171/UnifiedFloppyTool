/**
 * @file uft_thread.h
 * @brief Cross-platform threading primitives (mutex, once)
 * 
 * Provides pthread-like interface on Windows using Win32 API.
 */
#ifndef UFT_THREAD_H
#define UFT_THREAD_H

#ifdef _WIN32

#include <windows.h>

/* Mutex type */
typedef CRITICAL_SECTION pthread_mutex_t;

/* Mutex attribute (unused on Windows) */
typedef int pthread_mutexattr_t;

/* Initialize mutex */
static inline int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    (void)attr;
    InitializeCriticalSection(mutex);
    return 0;
}

/* Destroy mutex */
static inline int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    DeleteCriticalSection(mutex);
    return 0;
}

/* Lock mutex */
static inline int pthread_mutex_lock(pthread_mutex_t* mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

/* Unlock mutex */
static inline int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

/* Try lock mutex */
static inline int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    return TryEnterCriticalSection(mutex) ? 0 : EBUSY;
}

/* Once control */
typedef INIT_ONCE pthread_once_t;
#define PTHREAD_ONCE_INIT INIT_ONCE_STATIC_INIT

static inline int pthread_once(pthread_once_t* once_control, void (*init_routine)(void)) {
    BOOL pending = FALSE;
    if (InitOnceBeginInitialize(once_control, 0, &pending, NULL)) {
        if (pending) {
            init_routine();
            InitOnceComplete(once_control, 0, NULL);
        }
        return 0;
    }
    return -1;
}

/* EBUSY for trylock */
#ifndef EBUSY
#define EBUSY 16
#endif

#else
/* POSIX - use native pthread.h */
#include <pthread.h>
#endif /* _WIN32 */

#endif /* UFT_THREAD_H */
