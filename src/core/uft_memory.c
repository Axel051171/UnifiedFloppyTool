/**
 * @file uft_memory.c
 * @brief Memory Management Framework - Lock-Free Stats & Pool Allocator
 * @version 1.6.1
 * 
 * Copyright (c) 2025 UFT Project
 * SPDX-License-Identifier: MIT
 */

#include "uft/uft_memory.h"
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

#ifdef UFT_HAS_THREADS
    #include <pthread.h>
#endif

/*============================================================================
 * PLATFORM-SPECIFIC ALIGNED ALLOCATION
 *============================================================================*/

#if defined(_WIN32)
    #include <malloc.h>
    #define platform_aligned_alloc(size, align) _aligned_malloc(size, align)
    #define platform_aligned_free(ptr)          _aligned_free(ptr)
#elif defined(__APPLE__)
    static inline void* platform_aligned_alloc(size_t size, size_t align) {
        void* ptr = NULL;
        return (posix_memalign(&ptr, align, size) == 0) ? ptr : NULL;
    }
    #define platform_aligned_free(ptr)          free(ptr)
#else
    static inline void* platform_aligned_alloc(size_t size, size_t align) {
        size_t aligned_size = (size + align - 1) & ~(align - 1);
        return aligned_alloc(align, aligned_size);
    }
    #define platform_aligned_free(ptr)          free(ptr)
#endif

/*============================================================================
 * GLOBAL STATISTICS
 *============================================================================*/

static atomic_size_t g_total_allocated = 0;
static atomic_size_t g_current_allocated = 0;
static atomic_size_t g_peak_allocated = 0;
static atomic_size_t g_allocation_count = 0;
static atomic_size_t g_free_count = 0;

/*============================================================================
 * PUBLIC API - ALIGNED ALLOCATION
 *============================================================================*/

void* uft_malloc_aligned(size_t size, size_t alignment)
{
    if (size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return NULL;
    }
    
    void* ptr = platform_aligned_alloc(size, alignment);
    if (ptr) {
        atomic_fetch_add(&g_total_allocated, size);
        size_t current = atomic_fetch_add(&g_current_allocated, size) + size;
        atomic_fetch_add(&g_allocation_count, 1);
        
        size_t peak = atomic_load(&g_peak_allocated);
        while (current > peak) {
            if (atomic_compare_exchange_weak(&g_peak_allocated, &peak, current)) break;
        }
    }
    return ptr;
}

void uft_free_aligned(void* ptr)
{
    if (ptr) {
        platform_aligned_free(ptr);
        atomic_fetch_add(&g_free_count, 1);
    }
}

/*============================================================================
 * MEMORY POOL
 *============================================================================*/

typedef struct uft_pool_node { struct uft_pool_node* next; } uft_pool_node_t;
typedef struct uft_pool_chunk { struct uft_pool_chunk* next; size_t size; } uft_pool_chunk_t;

struct uft_pool_t {
    size_t object_size;
    size_t objects_per_chunk;
    uft_pool_node_t* free_list;
    uft_pool_chunk_t* chunks;
    size_t total_objects;
    size_t free_objects;
#ifdef UFT_HAS_THREADS
    pthread_mutex_t mutex;
#endif
};

static bool pool_allocate_chunk(uft_pool_t* pool)
{
    size_t obj_size = pool->object_size < sizeof(uft_pool_node_t) ? sizeof(uft_pool_node_t) : pool->object_size;
    size_t data_size = obj_size * pool->objects_per_chunk;
    
    uft_pool_chunk_t* chunk = (uft_pool_chunk_t*)malloc(sizeof(uft_pool_chunk_t) + data_size);
    if (!chunk) return false;
    
    chunk->next = pool->chunks;
    chunk->size = data_size;
    pool->chunks = chunk;
    
    uint8_t* data = (uint8_t*)(chunk + 1);
    for (size_t i = 0; i < pool->objects_per_chunk; i++) {
        uft_pool_node_t* node = (uft_pool_node_t*)(data + i * obj_size);
        node->next = pool->free_list;
        pool->free_list = node;
    }
    
    pool->total_objects += pool->objects_per_chunk;
    pool->free_objects += pool->objects_per_chunk;
    return true;
}

uft_pool_t* uft_pool_create(size_t object_size, size_t initial_capacity)
{
    if (object_size == 0) return NULL;
    
    uft_pool_t* pool = (uft_pool_t*)calloc(1, sizeof(uft_pool_t));
    if (!pool) return NULL;
    
    pool->object_size = object_size;
    pool->objects_per_chunk = initial_capacity > 0 ? initial_capacity : 128;
    
#ifdef UFT_HAS_THREADS
    pthread_mutex_init(&pool->mutex, NULL);
#endif
    
    if (!pool_allocate_chunk(pool)) {
        uft_pool_destroy(&pool);
        return NULL;
    }
    return pool;
}

void* uft_pool_alloc(uft_pool_t* pool)
{
    if (!pool) return NULL;
    
#ifdef UFT_HAS_THREADS
    pthread_mutex_lock(&pool->mutex);
#endif
    
    if (!pool->free_list && !pool_allocate_chunk(pool)) {
#ifdef UFT_HAS_THREADS
        pthread_mutex_unlock(&pool->mutex);
#endif
        return NULL;
    }
    
    uft_pool_node_t* node = pool->free_list;
    pool->free_list = node->next;
    pool->free_objects--;
    
#ifdef UFT_HAS_THREADS
    pthread_mutex_unlock(&pool->mutex);
#endif
    return node;
}

void uft_pool_free(uft_pool_t* pool, void* obj)
{
    if (!pool || !obj) return;
    
#ifdef UFT_HAS_THREADS
    pthread_mutex_lock(&pool->mutex);
#endif
    
    uft_pool_node_t* node = (uft_pool_node_t*)obj;
    node->next = pool->free_list;
    pool->free_list = node;
    pool->free_objects++;
    
#ifdef UFT_HAS_THREADS
    pthread_mutex_unlock(&pool->mutex);
#endif
}

void uft_pool_destroy(uft_pool_t** pool_ptr)
{
    if (!pool_ptr || !*pool_ptr) return;
    
    uft_pool_t* pool = *pool_ptr;
    
#ifdef UFT_HAS_THREADS
    pthread_mutex_destroy(&pool->mutex);
#endif
    
    uft_pool_chunk_t* chunk = pool->chunks;
    while (chunk) {
        uft_pool_chunk_t* next = chunk->next;
        free(chunk);
        chunk = next;
    }
    
    free(pool);
    *pool_ptr = NULL;
}

/*============================================================================
 * STATISTICS
 *============================================================================*/

void uft_memory_get_stats(uft_memory_stats_t* stats)
{
    if (!stats) return;
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
