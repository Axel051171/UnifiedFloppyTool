/**
 * @file uft_safe_alloc.h
 * @brief Safe Memory Allocation with Tracking (W-P0-002)
 * 
 * Provides checked memory allocation with:
 * - NULL return handling
 * - Optional allocation tracking
 * - Cleanup helpers
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_SAFE_ALLOC_H
#define UFT_SAFE_ALLOC_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Allocation Statistics
 *===========================================================================*/

typedef struct {
    size_t total_allocations;
    size_t total_frees;
    size_t current_bytes;
    size_t peak_bytes;
    size_t failed_allocations;
} uft_alloc_stats_t;

/** Global allocation statistics (optional tracking) */
extern uft_alloc_stats_t uft_alloc_stats;

/** Enable/disable allocation tracking */
extern bool uft_alloc_tracking_enabled;

/*===========================================================================
 * Core Allocation Functions
 *===========================================================================*/

/**
 * @brief Safe malloc with NULL check
 * @param size Bytes to allocate
 * @return Allocated memory or NULL
 */
void* uft_malloc(size_t size);

/**
 * @brief Safe calloc with NULL check
 * @param count Number of elements
 * @param size Size per element
 * @return Zero-initialized memory or NULL
 */
void* uft_calloc(size_t count, size_t size);

/**
 * @brief Safe realloc with NULL check
 * @param ptr Existing pointer (may be NULL)
 * @param new_size New size
 * @return Reallocated memory or NULL (original unchanged on failure)
 */
void* uft_realloc(void *ptr, size_t new_size);

/**
 * @brief Safe free (handles NULL)
 * @param ptr Pointer to free (may be NULL)
 */
void uft_free(void *ptr);

/**
 * @brief Safe strdup
 * @param str String to duplicate
 * @return Duplicated string or NULL
 */
char* uft_strdup(const char *str);

/*===========================================================================
 * Tracked Allocation (Debug)
 *===========================================================================*/

/**
 * @brief Allocation with source tracking
 */
void* uft_malloc_tracked(size_t size, const char *file, int line);
void* uft_calloc_tracked(size_t count, size_t size, const char *file, int line);
void uft_free_tracked(void *ptr, const char *file, int line);

#ifdef UFT_ALLOC_TRACKING
    #define UFT_MALLOC(sz)       uft_malloc_tracked((sz), __FILE__, __LINE__)
    #define UFT_CALLOC(n, sz)    uft_calloc_tracked((n), (sz), __FILE__, __LINE__)
    #define UFT_FREE(ptr)        uft_free_tracked((ptr), __FILE__, __LINE__)
#else
    #define UFT_MALLOC(sz)       uft_malloc(sz)
    #define UFT_CALLOC(n, sz)    uft_calloc((n), (sz))
    #define UFT_FREE(ptr)        uft_free(ptr)
#endif

/*===========================================================================
 * Cleanup Helpers
 *===========================================================================*/

/**
 * @brief Auto-cleanup scope guard
 * 
 * Usage:
 *   void *ptr = uft_malloc(100);
 *   UFT_AUTO_FREE(ptr);
 *   ... use ptr ...
 *   // ptr is automatically freed at scope exit
 */
#ifdef __GNUC__
    static inline void uft_auto_free_func(void *p) { 
        uft_free(*(void**)p); 
    }
    #define UFT_AUTO_FREE(ptr) \
        __attribute__((cleanup(uft_auto_free_func))) void *_auto_##ptr = &ptr
#else
    #define UFT_AUTO_FREE(ptr) /* Not supported */
#endif

/**
 * @brief Free and NULL pointer
 */
#define UFT_FREE_NULL(ptr) do { uft_free(ptr); (ptr) = NULL; } while(0)

/**
 * @brief Free array of pointers
 * @param arr Array of pointers
 * @param count Number of elements
 */
static inline void uft_free_array(void **arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; i++) {
        uft_free(arr[i]);
    }
    uft_free(arr);
}

/*===========================================================================
 * Error Handling Patterns
 *===========================================================================*/

/**
 * @brief Allocate or return NULL with cleanup
 * 
 * Usage:
 *   UFT_ALLOC_OR_FAIL(buffer, 1024, cleanup);
 */
#define UFT_ALLOC_OR_FAIL(ptr, size, label) \
    do { \
        (ptr) = uft_malloc(size); \
        if (!(ptr)) goto label; \
    } while(0)

#define UFT_CALLOC_OR_FAIL(ptr, count, size, label) \
    do { \
        (ptr) = uft_calloc((count), (size)); \
        if (!(ptr)) goto label; \
    } while(0)

/**
 * @brief Allocate or return error code
 */
#define UFT_ALLOC_OR_RETURN(ptr, size, errval) \
    do { \
        (ptr) = uft_malloc(size); \
        if (!(ptr)) return (errval); \
    } while(0)

/*===========================================================================
 * Statistics Functions
 *===========================================================================*/

/**
 * @brief Get current allocation statistics
 */
const uft_alloc_stats_t* uft_alloc_get_stats(void);

/**
 * @brief Reset allocation statistics
 */
void uft_alloc_reset_stats(void);

/**
 * @brief Enable/disable allocation tracking
 */
void uft_alloc_set_tracking(bool enable);

/**
 * @brief Print allocation statistics
 */
void uft_alloc_print_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SAFE_ALLOC_H */
