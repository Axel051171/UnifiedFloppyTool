/**
 * @file uft_global_mutex.h
 * @brief Global state mutex for thread safety
 */

#ifndef UFT_CORE_GLOBAL_MUTEX_H
#define UFT_CORE_GLOBAL_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

/* Platform-specific mutex type */
#ifdef _WIN32
#include <windows.h>
#include <stddef.h>
typedef CRITICAL_SECTION uft_mutex_t;
#else
#include <pthread.h>
#include <stddef.h>
typedef pthread_mutex_t uft_mutex_t;
#endif

/**
 * @brief Initialize a mutex
 */
static inline int uft_mutex_init(uft_mutex_t* mutex) {
#ifdef _WIN32
    InitializeCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_init(mutex, NULL);
#endif
}

/**
 * @brief Destroy a mutex
 */
static inline void uft_mutex_destroy(uft_mutex_t* mutex) {
#ifdef _WIN32
    DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}

/**
 * @brief Lock a mutex
 */
static inline void uft_mutex_lock(uft_mutex_t* mutex) {
#ifdef _WIN32
    EnterCriticalSection(mutex);
#else
    pthread_mutex_lock(mutex);
#endif
}

/**
 * @brief Unlock a mutex
 */
static inline void uft_mutex_unlock(uft_mutex_t* mutex) {
#ifdef _WIN32
    LeaveCriticalSection(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif
}

/* Global mutex API */
int uft_global_mutex_init(void);
void uft_global_mutex_destroy(void);
void uft_global_lock(void);
void uft_global_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CORE_GLOBAL_MUTEX_H */
