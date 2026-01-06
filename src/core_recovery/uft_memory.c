/**
 * @file uft_memory.c
 * @brief Memory Management Framework Implementation
 */

#include "uft/uft_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

/* =============================================================================
 * PLATFORM-SPECIFIC ALIGNED ALLOCATION
 * ============================================================================= */

#if defined(_WIN32)
    #include <malloc.h>
    #define uft_aligned_alloc(size, align) _aligned_malloc(size, align)
    #define uft_aligned_free(ptr) _aligned_free(ptr)
#elif defined(__APPLE__)
    static inline void* uft_aligned_alloc(size_t size, size_t align) {
        void *ptr;
        return (posix_memalign(&ptr, align, size) == 0) ? ptr : NULL;
    }
    #define uft_aligned_free(ptr) free(ptr)
#else
    #define uft_aligned_alloc(size, align) aligned_alloc(align, size)
    #define uft_aligned_free(ptr) free(ptr)
#endif

void* uft_malloc_aligned(size_t size, size_t alignment)
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        fprintf(stderr, "UFT ERROR: Invalid alignment %zu (must be power of 2)\n",
                alignment);
        return NULL;
    }
    
    // Round size up to multiple of alignment
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
    
    void *ptr = uft_aligned_alloc(aligned_size, alignment);
    if (!ptr) {
        fprintf(stderr, "UFT ERROR: Failed to allocate %zu bytes (aligned to %zu)\n",
                size, alignment);
    }
    
    return ptr;
}

void uft_free_aligned(void *ptr)
{
    if (ptr) {
        uft_aligned_free(ptr);
    }
}

/* =============================================================================
 * MEMORY POOL IMPLEMENTATION
 * ============================================================================= */

typedef struct uft_pool_block_t {
    struct uft_pool_block_t *next;
} uft_pool_block_t;

struct uft_pool_t {
    size_t object_size;      /* Size of each object */
    size_t objects_per_chunk; /* Objects in each chunk */
    
    void *chunks;            /* List of allocated chunks */
    uft_pool_block_t *free_list; /* Free objects */
    
    size_t total_allocated;  /* Total objects allocated */
    size_t total_free;       /* Free objects available */
    
    pthread_mutex_t mutex;   /* Thread safety */
};

uft_pool_t* uft_pool_create(size_t object_size, size_t initial_capacity)
{
    if (object_size < sizeof(uft_pool_block_t)) {
        object_size = sizeof(uft_pool_block_t);
    }
    
    uft_pool_t *pool = calloc(1, sizeof(uft_pool_t));
    if (!pool) {
        return NULL;
    }
    
    pool->object_size = object_size;
    pool->objects_per_chunk = initial_capacity > 0 ? initial_capacity : 128;
    
    pthread_mutex_init(&pool->mutex, NULL);
    
    // Pre-allocate first chunk
    size_t chunk_size = pool->object_size * pool->objects_per_chunk;
    void *chunk = malloc(chunk_size);
    if (!chunk) {
        free(pool);
        return NULL;
    }
    
    pool->chunks = chunk;
    
    // Build free list
    for (size_t i = 0; i < pool->objects_per_chunk; i++) {
        uft_pool_block_t *block = (uft_pool_block_t *)
            ((char *)chunk + i * pool->object_size);
        block->next = pool->free_list;
        pool->free_list = block;
    }
    
    pool->total_allocated = pool->objects_per_chunk;
    pool->total_free = pool->objects_per_chunk;
    
    return pool;
}

void* uft_pool_alloc(uft_pool_t *pool)
{
    if (!pool) {
        return NULL;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    // Expand pool if empty
    if (!pool->free_list) {
        // TODO: Allocate new chunk
        pthread_mutex_unlock(&pool->mutex);
        fprintf(stderr, "UFT WARNING: Pool exhausted, falling back to malloc\n");
        return malloc(pool->object_size);
    }
    
    uft_pool_block_t *block = pool->free_list;
    pool->free_list = block->next;
    pool->total_free--;
    
    pthread_mutex_unlock(&pool->mutex);
    
    return (void *)block;
}

void uft_pool_free(uft_pool_t *pool, void *obj)
{
    if (!pool || !obj) {
        return;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    uft_pool_block_t *block = (uft_pool_block_t *)obj;
    block->next = pool->free_list;
    pool->free_list = block;
    pool->total_free++;
    
    pthread_mutex_unlock(&pool->mutex);
}

void uft_pool_destroy(uft_pool_t **pool)
{
    if (!pool || !*pool) {
        return;
    }
    
    uft_pool_t *p = *pool;
    
    pthread_mutex_destroy(&p->mutex);
    
    // Free all chunks (simple version - assumes single chunk)
    if (p->chunks) {
        free(p->chunks);
    }
    
    free(p);
    *pool = NULL;
}

/* =============================================================================
 * MEMORY STATISTICS
 * ============================================================================= */

static atomic_size_t g_total_allocated = 0;
static atomic_size_t g_current_allocated = 0;
static atomic_size_t g_peak_allocated = 0;
static atomic_size_t g_allocation_count = 0;
static atomic_size_t g_free_count = 0;

void uft_memory_get_stats(uft_memory_stats_t *stats)
{
    if (!stats) {
        return;
    }
    
    stats->total_allocated = atomic_load(&g_total_allocated);
    stats->current_allocated = atomic_load(&g_current_allocated);
    stats->peak_allocated = atomic_load(&g_peak_allocated);
    stats->allocation_count = atomic_load(&g_allocation_count);
    stats->free_count = atomic_load(&g_free_count);
}

void uft_memory_reset_stats(void)
{
    atomic_store(&g_total_allocated, 0);
    atomic_store(&g_current_allocated, 0);
    atomic_store(&g_peak_allocated, 0);
    atomic_store(&g_allocation_count, 0);
    atomic_store(&g_free_count, 0);
}

/* =============================================================================
 * DEBUG MODE - LEAK DETECTION
 * ============================================================================= */

#ifdef UFT_DEBUG_MEMORY

#define MAX_ALLOCATIONS 100000

typedef struct {
    void *ptr;
    size_t size;
    const char *file;
    int line;
} allocation_info_t;

static allocation_info_t g_allocations[MAX_ALLOCATIONS];
static size_t g_allocation_index = 0;
static pthread_mutex_t g_debug_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_debug_enabled = false;

void uft_memory_debug_enable(void)
{
    g_debug_enabled = true;
}

void uft_memory_debug_register(void *ptr, size_t size,
                                const char *file, int line)
{
    if (!g_debug_enabled || !ptr) {
        return;
    }
    
    pthread_mutex_lock(&g_debug_mutex);
    
    if (g_allocation_index < MAX_ALLOCATIONS) {
        g_allocations[g_allocation_index].ptr = ptr;
        g_allocations[g_allocation_index].size = size;
        g_allocations[g_allocation_index].file = file;
        g_allocations[g_allocation_index].line = line;
        g_allocation_index++;
    }
    
    pthread_mutex_unlock(&g_debug_mutex);
}

void uft_memory_debug_unregister(void *ptr)
{
    if (!g_debug_enabled || !ptr) {
        return;
    }
    
    pthread_mutex_lock(&g_debug_mutex);
    
    for (size_t i = 0; i < g_allocation_index; i++) {
        if (g_allocations[i].ptr == ptr) {
            // Remove by shifting down
            memmove(&g_allocations[i], &g_allocations[i + 1],
                   (g_allocation_index - i - 1) * sizeof(allocation_info_t));
            g_allocation_index--;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_debug_mutex);
}

void uft_memory_debug_report(void)
{
    pthread_mutex_lock(&g_debug_mutex);
    
    if (g_allocation_index == 0) {
        fprintf(stderr, "\n✓ No memory leaks detected!\n\n");
    } else {
        fprintf(stderr, "\n⚠ MEMORY LEAKS DETECTED: %zu allocations\n\n",
                g_allocation_index);
        
        size_t total_leaked = 0;
        for (size_t i = 0; i < g_allocation_index; i++) {
            fprintf(stderr, "  Leak #%zu: %zu bytes at %p (%s:%d)\n",
                   i + 1,
                   g_allocations[i].size,
                   g_allocations[i].ptr,
                   g_allocations[i].file,
                   g_allocations[i].line);
            total_leaked += g_allocations[i].size;
        }
        
        fprintf(stderr, "\n  Total leaked: %zu bytes\n\n", total_leaked);
    }
    
    pthread_mutex_unlock(&g_debug_mutex);
}

#endif /* UFT_DEBUG_MEMORY */

/* =============================================================================
 * AUTO-DESTROY IMPLEMENTATIONS (Forward References)
 * ============================================================================= */

// These are implemented in flux_core.c
void uft_auto_destroy_flux_disk_t_impl_(void *p)
{
    // Will be implemented in flux_core.c
    extern void flux_disk_destroy(void **disk);
    flux_disk_destroy((void **)p);
}

void uft_auto_destroy_flux_track_t_impl_(void *p)
{
    extern void flux_track_destroy(void **track);
    flux_track_destroy((void **)p);
}

void uft_auto_destroy_flux_bitstream_t_impl_(void *p)
{
    extern void flux_bitstream_destroy(void *bitstream);
    void **ptr = (void **)p;
    if (ptr && *ptr) {
        flux_bitstream_destroy(*ptr);
        *ptr = NULL;
    }
}
