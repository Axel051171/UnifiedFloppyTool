/**
 * @file uft_mempool.h
 * @brief Simple Memory Pool Allocator (P3-001)
 * 
 * Reduces malloc overhead for frequently allocated objects
 * of the same size (e.g., sectors, tracks, flux samples).
 * 
 * Usage:
 *   uft_mempool_t *pool = uft_mempool_create(sizeof(sector_t), 1024);
 *   sector_t *s = uft_mempool_alloc(pool);
 *   // ... use s ...
 *   uft_mempool_free(pool, s);
 *   uft_mempool_destroy(pool);
 * 
 * @version 1.0.0
 * @date 2026-01-07
 */

#ifndef UFT_MEMPOOL_H
#define UFT_MEMPOOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_MEMPOOL_DEFAULT_CHUNK_SIZE
#define UFT_MEMPOOL_DEFAULT_CHUNK_SIZE 4096
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_mempool_chunk {
    struct uft_mempool_chunk *next;
    size_t                    used;
    size_t                    capacity;
    uint8_t                   data[];  /* Flexible array member */
} uft_mempool_chunk_t;

typedef struct {
    size_t                  item_size;      /**< Size of each item */
    size_t                  items_per_chunk;/**< Items per chunk */
    uft_mempool_chunk_t    *chunks;         /**< Linked list of chunks */
    void                   *free_list;      /**< Free list for recycling */
    
    /* Statistics */
    size_t                  total_allocs;
    size_t                  total_frees;
    size_t                  chunk_count;
} uft_mempool_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Pool Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a memory pool
 * @param item_size Size of each item
 * @param items_per_chunk How many items per chunk (0 = auto)
 * @return Pool pointer or NULL on failure
 */
static inline uft_mempool_t *uft_mempool_create(size_t item_size, size_t items_per_chunk) {
    if (item_size < sizeof(void*)) {
        item_size = sizeof(void*);  /* Minimum for free list pointer */
    }
    
    /* Align to pointer size */
    item_size = (item_size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
    
    if (items_per_chunk == 0) {
        items_per_chunk = UFT_MEMPOOL_DEFAULT_CHUNK_SIZE / item_size;
        if (items_per_chunk < 16) items_per_chunk = 16;
    }
    
    uft_mempool_t *pool = (uft_mempool_t*)calloc(1, sizeof(uft_mempool_t));
    if (!pool) return NULL;
    
    pool->item_size = item_size;
    pool->items_per_chunk = items_per_chunk;
    
    return pool;
}

/**
 * @brief Destroy a memory pool
 * @param pool Pool to destroy
 */
static inline void uft_mempool_destroy(uft_mempool_t *pool) {
    if (!pool) return;
    
    /* Free all chunks */
    uft_mempool_chunk_t *chunk = pool->chunks;
    while (chunk) {
        uft_mempool_chunk_t *next = chunk->next;
        free(chunk);
        chunk = next;
    }
    
    free(pool);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Allocation
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Allocate an item from the pool
 * @param pool Memory pool
 * @return Pointer to item or NULL on failure
 */
static inline void *uft_mempool_alloc(uft_mempool_t *pool) {
    if (!pool) return NULL;
    
    /* Try free list first */
    if (pool->free_list) {
        void *ptr = pool->free_list;
        pool->free_list = *(void**)ptr;
        pool->total_allocs++;
        return ptr;
    }
    
    /* Find chunk with space or create new one */
    uft_mempool_chunk_t *chunk = pool->chunks;
    while (chunk && chunk->used >= chunk->capacity) {
        chunk = chunk->next;
    }
    
    if (!chunk) {
        /* Allocate new chunk */
        size_t chunk_size = sizeof(uft_mempool_chunk_t) + 
                           pool->item_size * pool->items_per_chunk;
        chunk = (uft_mempool_chunk_t*)malloc(chunk_size);
        if (!chunk) return NULL;
        
        chunk->next = pool->chunks;
        chunk->used = 0;
        chunk->capacity = pool->items_per_chunk;
        pool->chunks = chunk;
        pool->chunk_count++;
    }
    
    /* Allocate from chunk */
    void *ptr = chunk->data + (chunk->used * pool->item_size);
    chunk->used++;
    pool->total_allocs++;
    
    return ptr;
}

/**
 * @brief Allocate and zero-initialize an item
 * @param pool Memory pool
 * @return Pointer to zeroed item or NULL on failure
 */
static inline void *uft_mempool_calloc(uft_mempool_t *pool) {
    void *ptr = uft_mempool_alloc(pool);
    if (ptr) {
        memset(ptr, 0, pool->item_size);
    }
    return ptr;
}

/**
 * @brief Return an item to the pool
 * @param pool Memory pool
 * @param ptr Item to free
 */
static inline void uft_mempool_free(uft_mempool_t *pool, void *ptr) {
    if (!pool || !ptr) return;
    
    /* Add to free list */
    *(void**)ptr = pool->free_list;
    pool->free_list = ptr;
    pool->total_frees++;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Statistics
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get pool statistics
 */
static inline void uft_mempool_stats(const uft_mempool_t *pool,
                                      size_t *total_allocs,
                                      size_t *total_frees,
                                      size_t *chunk_count,
                                      size_t *memory_used) {
    if (!pool) return;
    
    if (total_allocs) *total_allocs = pool->total_allocs;
    if (total_frees) *total_frees = pool->total_frees;
    if (chunk_count) *chunk_count = pool->chunk_count;
    if (memory_used) {
        *memory_used = sizeof(uft_mempool_t) +
                      pool->chunk_count * (sizeof(uft_mempool_chunk_t) +
                                          pool->item_size * pool->items_per_chunk);
    }
}

/**
 * @brief Reset pool (free all items but keep memory)
 */
static inline void uft_mempool_reset(uft_mempool_t *pool) {
    if (!pool) return;
    
    /* Reset all chunks */
    uft_mempool_chunk_t *chunk = pool->chunks;
    while (chunk) {
        chunk->used = 0;
        chunk = chunk->next;
    }
    
    pool->free_list = NULL;
    pool->total_allocs = 0;
    pool->total_frees = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_MEMPOOL_H */
