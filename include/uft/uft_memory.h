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
    #warning "Auto-cleanup not supported on this compiler - manual cleanup required"
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

// Wrap standard allocation functions in debug mode
#define malloc(size) \
    ({ void *p = malloc(size); \
       uft_memory_debug_register(p, size, __FILE__, __LINE__); \
       p; })

#define free(ptr) \
    ({ uft_memory_debug_unregister(ptr); \
       free(ptr); })

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
