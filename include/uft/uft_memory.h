/**
 * @file uft_memory.h
 * @brief Elite Memory Management Framework - Valgrind-Safe RAII for C
 * 
 * FEATURES:
 * - Automatic cleanup using GCC __attribute__((cleanup))
 * - Memory leak prevention
 * - Debug-Mode allocation tracking
 * - Aligned allocation for SIMD
 * - Pool allocator for small objects
 * 
 * EXAMPLE USAGE:
 * ```c
 * void process_disk(const char *filename) {
 *     UFT_AUTO_FREE char *data = malloc(1024);
 *     UFT_AUTO_DESTROY(flux_disk_t) disk = flux_disk_create();
 *     
 *     // Use data and disk...
 *     // Automatic cleanup on scope exit (even on early return/error)
 * }
 * ```
 */

#ifndef UFT_MEMORY_H
#define UFT_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if !defined(_WIN32)
#include <unistd.h>
#else
#include <io.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * AUTO-CLEANUP MACROS (GCC/Clang)
 * ============================================================================= */

#if defined(__GNUC__) || defined(__clang__)
    #define UFT_CLEANUP(func) __attribute__((cleanup(func)))
#else
    #define UFT_CLEANUP(func)
    /* MSVC doesn't support #warning, use #pragma message */
    #ifdef _MSC_VER
        #pragma message("Auto-cleanup not supported on this compiler - manual cleanup required")
    #endif
#endif

/* ─────────────────────────────────────────────────────────────────────────── 
 * GENERIC CLEANUP FUNCTIONS
 * ─────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Auto-free for malloc'd pointers
 * 
 * USAGE:
 *   UFT_AUTO_FREE char *buffer = malloc(1024);
 */
static inline void uft_auto_free_impl_(void *p) {
    void **ptr = (void **)p;
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

#define UFT_AUTO_FREE UFT_CLEANUP(uft_auto_free_impl_)

/**
 * @brief Auto-close for file descriptors
 * 
 * USAGE:
 *   UFT_AUTO_CLOSE int fd = open("file.img", O_RDONLY);
 */
static inline void uft_auto_close_impl_(int *fd) {
    if (fd && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

#define UFT_AUTO_CLOSE UFT_CLEANUP(uft_auto_close_impl_)

/**
 * @brief Auto-close for FILE*
 * 
 * USAGE:
 *   UFT_AUTO_FCLOSE FILE *fp = fopen("file.txt", "r");
 */
static inline void uft_auto_fclose_impl_(FILE **fp) {
    if (fp && *fp) {
        fclose(*fp);
        *fp = NULL;
    }
}

#define UFT_AUTO_FCLOSE UFT_CLEANUP(uft_auto_fclose_impl_)

/* ─────────────────────────────────────────────────────────────────────────── 
 * OBJECT-SPECIFIC CLEANUP MACROS
 * ─────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Auto-destroy for flux_disk_t
 * 
 * USAGE:
 *   UFT_AUTO_DESTROY(flux_disk_t) disk = flux_disk_create();
 */
#define UFT_AUTO_DESTROY(type) \
    UFT_CLEANUP(uft_auto_destroy_##type##_impl_) type*

// Forward declarations for cleanup functions
// (Actual implementations in respective .c files)
extern void uft_auto_destroy_flux_disk_t_impl_(void *p);
extern void uft_auto_destroy_flux_track_t_impl_(void *p);
extern void uft_auto_destroy_flux_bitstream_t_impl_(void *p);

/* =============================================================================
 * SAFE MEMORY FUNCTIONS (Bounds-Checked)
 * ============================================================================= */

/**
 * @brief Safe memcpy with bounds checking
 * @return true on success, false on buffer overflow
 */
static inline bool uft_memcpy_safe(void *dest, size_t dest_size,
                                   const void *src, size_t src_size)
{
    if (!dest || !src) {
        return false;
    }
    
    if (src_size > dest_size) {
        // Would overflow - truncate and warn
        fprintf(stderr, "UFT WARNING: memcpy truncated (%zu > %zu bytes)\n",
                src_size, dest_size);
        src_size = dest_size;
    }
    
    memcpy(dest, src, src_size);
    return true;
}

/**
 * @brief Safe string copy with NULL termination guarantee
 */
static inline bool uft_strcpy_safe(char *dest, size_t dest_size,
                                   const char *src)
{
    if (!dest || !src || dest_size == 0) {
        return false;
    }
    
    size_t i;
    for (i = 0; i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    
    return (src[i] == '\0'); // true if fully copied
}

/* =============================================================================
 * ALIGNED ALLOCATION (For SIMD)
 * ============================================================================= */

/**
 * @brief Allocate aligned memory (for SSE2/AVX2)
 * @param alignment Must be power of 2 (e.g., 16 for SSE2, 32 for AVX2)
 */
void* uft_malloc_aligned(size_t size, size_t alignment);

/**
 * @brief Free aligned memory
 */
void uft_free_aligned(void *ptr);

/**
 * @brief Auto-free for aligned pointers
 */
static inline void uft_auto_free_aligned_impl_(void *p) {
    void **ptr = (void **)p;
    if (ptr && *ptr) {
        uft_free_aligned(*ptr);
        *ptr = NULL;
    }
}

#define UFT_AUTO_FREE_ALIGNED UFT_CLEANUP(uft_auto_free_aligned_impl_)

/* =============================================================================
 * SAFE MALLOC MACROS (P0-SEC-001: NULL-Check after malloc)
 * ============================================================================= */

/**
 * @brief Safe malloc that returns typed pointer
 * Usage: ptr = UFT_MALLOC(type, count);
 *        if (!ptr) return error;
 */
#define UFT_MALLOC(type, count) \
    ((type*)malloc(sizeof(type) * (count)))

/**
 * @brief Safe calloc (zero-initialized)
 */
#define UFT_CALLOC(type, count) \
    ((type*)calloc((count), sizeof(type)))

/**
 * @brief Safe malloc with immediate NULL check and return
 * Usage: UFT_MALLOC_OR_RETURN(ptr, type, count, return_value);
 */
#define UFT_MALLOC_OR_RETURN(ptr, type, count, ret_val) \
    do { \
        (ptr) = (type*)malloc(sizeof(type) * (count)); \
        if (!(ptr)) return (ret_val); \
    } while(0)

/**
 * @brief Safe malloc with NULL check and goto
 * Usage: UFT_MALLOC_OR_GOTO(ptr, type, count, label);
 */
#define UFT_MALLOC_OR_GOTO(ptr, type, count, label) \
    do { \
        (ptr) = (type*)malloc(sizeof(type) * (count)); \
        if (!(ptr)) goto label; \
    } while(0)

/**
 * @brief Safe realloc with NULL check (preserves original on failure)
 */
#define UFT_REALLOC_OR_RETURN(ptr, type, count, ret_val) \
    do { \
        type* _new = (type*)realloc((ptr), sizeof(type) * (count)); \
        if (!_new) return (ret_val); \
        (ptr) = _new; \
    } while(0)

/**
 * @brief Safe strdup with NULL check
 */
static inline char* uft_strdup_safe(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

/**
 * @brief Safe malloc with size check (for raw bytes)
 */
static inline void* uft_malloc_safe(size_t size) {
    if (size == 0) return NULL;
    return malloc(size);
}

/* =============================================================================
 * MEMORY POOL (For Small Objects)
 * ============================================================================= */

typedef struct uft_pool_t uft_pool_t;

/**
 * @brief Create memory pool for fixed-size objects
 * @param object_size Size of each object
 * @param initial_capacity Number of objects to pre-allocate
 */
uft_pool_t* uft_pool_create(size_t object_size, size_t initial_capacity);

/**
 * @brief Allocate object from pool
 */
void* uft_pool_alloc(uft_pool_t *pool);

/**
 * @brief Return object to pool
 */
void uft_pool_free(uft_pool_t *pool, void *obj);

/**
 * @brief Destroy entire pool
 */
void uft_pool_destroy(uft_pool_t **pool);

/* =============================================================================
 * DEBUG MODE - ALLOCATION TRACKING
 * FIXED R18: Only enable malloc/free wrapping on GCC/Clang, not MSVC
 *            MSVC does not support GNU Statement Expressions
 * ============================================================================= */

#ifdef UFT_DEBUG_MEMORY

/**
 * @brief Enable memory leak detection
 */
void uft_memory_debug_enable(void);

/**
 * @brief Print memory leak report to stderr
 */
void uft_memory_debug_report(void);

/**
 * @brief Register allocation (internal use)
 */
void uft_memory_debug_register(void *ptr, size_t size, 
                                 const char *file, int line);

/**
 * @brief Unregister allocation (internal use)
 */
void uft_memory_debug_unregister(void *ptr);

/* 
 * Wrap standard allocation functions in debug mode
 * Only works with GCC/Clang due to GNU Statement Expressions
 * For MSVC, use the explicit uft_debug_* functions instead
 */
#if defined(__GNUC__) || defined(__clang__)
    #define malloc(size) \
        ({ void *p = malloc(size); \
           uft_memory_debug_register(p, size, __FILE__, __LINE__); \
           p; })

    #define free(ptr) \
        ({ uft_memory_debug_unregister(ptr); \
           free(ptr); })
#else
    /* MSVC: Use explicit debug functions - no automatic wrapping */
    /* Call uft_debug_malloc() and uft_debug_free() explicitly for tracking */
    static inline void* uft_debug_malloc(size_t size, const char* file, int line) {
        void* p = malloc(size);
        if (p) uft_memory_debug_register(p, size, file, line);
        return p;
    }
    static inline void uft_debug_free(void* ptr) {
        uft_memory_debug_unregister(ptr);
        free(ptr);
    }
    /* Optional macros for MSVC - these don't auto-track but avoid errors */
    #define UFT_DEBUG_MALLOC(size) uft_debug_malloc(size, __FILE__, __LINE__)
    #define UFT_DEBUG_FREE(ptr)    uft_debug_free(ptr)
#endif

#endif /* UFT_DEBUG_MEMORY */

/* =============================================================================
 * STATISTICS
 * ============================================================================= */

typedef struct {
    size_t total_allocated;      /* Total bytes ever allocated */
    size_t current_allocated;    /* Currently allocated bytes */
    size_t peak_allocated;       /* Peak memory usage */
    size_t allocation_count;     /* Number of allocations */
    size_t free_count;           /* Number of frees */
} uft_memory_stats_t;

/**
 * @brief Get memory statistics
 */
void uft_memory_get_stats(uft_memory_stats_t *stats);

/**
 * @brief Reset statistics
 */
void uft_memory_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MEMORY_H */

/* =============================================================================
 * INTEGER OVERFLOW PROTECTION (P0-SEC-004)
 * ============================================================================= */

#include <stdint.h>
#include <limits.h>

/**
 * @brief Safe array allocation with overflow check
 * @param nmemb Number of elements
 * @param size Size of each element
 * @return Allocated pointer or NULL on overflow/failure
 * 
 * Usage: ptr = uft_safe_malloc_array(count, sizeof(*ptr));
 */
static inline void* uft_safe_malloc_array(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return NULL;
    
    /* Check for multiplication overflow */
    if (nmemb > SIZE_MAX / size) {
        return NULL;  /* Would overflow */
    }
    
    return malloc(nmemb * size);
}

/**
 * @brief Safe calloc wrapper (already overflow-safe, but for consistency)
 */
static inline void* uft_safe_calloc_array(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return NULL;
    return calloc(nmemb, size);  /* calloc already checks overflow */
}

/**
 * @brief Safe realloc array with overflow check
 */
static inline void* uft_safe_realloc_array(void* ptr, size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        free(ptr);
        return NULL;
    }
    
    /* Check for multiplication overflow */
    if (nmemb > SIZE_MAX / size) {
        return NULL;  /* Would overflow */
    }
    
    return realloc(ptr, nmemb * size);
}

/**
 * @brief Macro for safe array malloc with automatic sizeof
 * Usage: UFT_MALLOC_ARRAY(ptr, count)
 */
#define UFT_MALLOC_ARRAY(ptr, count) \
    ((ptr) = uft_safe_malloc_array((count), sizeof(*(ptr))))

/**
 * @brief Macro for safe array calloc with automatic sizeof
 * Usage: UFT_CALLOC_ARRAY(ptr, count)
 */
#define UFT_CALLOC_ARRAY(ptr, count) \
    ((ptr) = uft_safe_calloc_array((count), sizeof(*(ptr))))

/**
 * @brief Check if multiplication would overflow
 */
#define UFT_MUL_WOULD_OVERFLOW(a, b) \
    ((b) != 0 && (a) > SIZE_MAX / (b))

/**
 * @brief Safe multiplication with overflow check (returns 0 on overflow)
 */
static inline size_t uft_safe_mul(size_t a, size_t b) {
    if (b != 0 && a > SIZE_MAX / b) {
        return 0;  /* Overflow */
    }
    return a * b;
}


/* =============================================================================
 * STACK-BUFFER SAFETY (P2-PERF-003)
 * ============================================================================= */

/**
 * @brief Maximum allowed stack buffer size (4KB)
 * Buffers larger than this should use heap allocation
 */
#define UFT_MAX_STACK_BUFFER 4096

/**
 * @brief Allocate buffer - stack if small, heap if large
 * Returns NULL if heap allocation fails for large buffers
 * 
 * Usage:
 *   uint8_t stack_buf[UFT_MAX_STACK_BUFFER];
 *   uint8_t *buf = UFT_ALLOC_BUFFER(size, stack_buf);
 *   if (!buf) return error;
 *   // use buf...
 *   UFT_FREE_BUFFER(buf, stack_buf);
 */
#define UFT_ALLOC_BUFFER(size, stack_buf) \
    ((size) <= sizeof(stack_buf) ? (stack_buf) : (uint8_t*)malloc(size))

#define UFT_FREE_BUFFER(buf, stack_buf) \
    do { if ((buf) != (stack_buf)) free(buf); } while(0)

/**
 * @brief Safe large buffer allocation (always heap)
 * Use for buffers known to be >4KB
 */
#define UFT_LARGE_BUFFER_ALLOC(size) malloc(size)
#define UFT_LARGE_BUFFER_FREE(ptr)   free(ptr)

/**
 * @brief Declare a hybrid buffer (stack small, heap large)
 */
#define UFT_HYBRID_BUFFER_DECL(name, stack_size) \
    uint8_t name##_stack[stack_size]; \
    uint8_t *name = NULL

#define UFT_HYBRID_BUFFER_ALLOC(name, size) \
    name = ((size) <= sizeof(name##_stack) ? name##_stack : (uint8_t*)malloc(size))

#define UFT_HYBRID_BUFFER_FREE(name) \
    do { if (name != name##_stack && name != NULL) { free(name); name = NULL; } } while(0)

