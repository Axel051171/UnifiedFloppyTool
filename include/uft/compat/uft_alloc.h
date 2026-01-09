/**
 * @file uft_alloc.h
 * @brief Safe memory allocation wrappers with NULL checks
 * 
 * Usage:
 *   ptr = uft_malloc(size);       // Returns NULL on failure (caller must check)
 *   ptr = uft_malloc_or_die(size); // Aborts on failure (for critical paths)
 *   ptr = uft_calloc(n, size);    // Zero-initialized
 *   ptr = uft_realloc(ptr, size); // Realloc with NULL check
 *   uft_free(ptr);                // Safe free (handles NULL)
 */
#ifndef UFT_ALLOC_H
#define UFT_ALLOC_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Safe malloc - returns NULL on failure
 * Caller MUST check return value
 */
static inline void* uft_malloc(size_t size) {
    if (size == 0) return NULL;
    return malloc(size);
}

/**
 * @brief Safe calloc - zero-initialized, returns NULL on failure
 */
static inline void* uft_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;
    return calloc(count, size);
}

/**
 * @brief Safe realloc - returns NULL on failure (original ptr unchanged)
 */
static inline void* uft_realloc(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

/**
 * @brief Safe free - handles NULL pointer
 */
static inline void uft_free(void* ptr) {
    if (ptr) free(ptr);
}

/**
 * @brief Malloc that aborts on failure - for critical allocations
 * Use sparingly, only where allocation failure is unrecoverable
 */
static inline void* uft_malloc_or_die(size_t size, const char* file, int line) {
    void* ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "FATAL: Out of memory allocating %zu bytes at %s:%d\n", 
                size, file, line);
        abort();
    }
    return ptr;
}

/**
 * @brief Calloc that aborts on failure
 */
static inline void* uft_calloc_or_die(size_t count, size_t size, const char* file, int line) {
    void* ptr = calloc(count, size);
    if (!ptr && count > 0 && size > 0) {
        fprintf(stderr, "FATAL: Out of memory allocating %zu*%zu bytes at %s:%d\n",
                count, size, file, line);
        abort();
    }
    return ptr;
}

/* Convenience macros with file/line info */
#define UFT_MALLOC_OR_DIE(size) uft_malloc_or_die((size), __FILE__, __LINE__)
#define UFT_CALLOC_OR_DIE(count, size) uft_calloc_or_die((count), (size), __FILE__, __LINE__)

/**
 * @brief Duplicate memory block (like strdup but for binary data)
 */
static inline void* uft_memdup(const void* src, size_t size) {
    if (!src || size == 0) return NULL;
    void* dst = malloc(size);
    if (dst) memcpy(dst, src, size);
    return dst;
}

/**
 * @brief Safe strdup
 */
static inline char* uft_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char* dup = (char*)malloc(len);
    if (dup) memcpy(dup, str, len);
    return dup;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_ALLOC_H */
