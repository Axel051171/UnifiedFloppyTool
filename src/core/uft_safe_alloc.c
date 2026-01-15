/**
 * @file uft_safe_alloc.c
 * @brief Safe Memory Allocation Implementation (W-P0-002)
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/core/uft_safe_alloc.h"
#include <stdio.h>
#include <string.h>

/*===========================================================================
 * Global Statistics
 *===========================================================================*/

uft_alloc_stats_t uft_alloc_stats = {0};
bool uft_alloc_tracking_enabled = false;

/*===========================================================================
 * Core Allocation Functions
 *===========================================================================*/

void* uft_malloc(size_t size) {
    if (size == 0) return NULL;
    
    void *ptr = malloc(size);
    
    if (uft_alloc_tracking_enabled) {
        if (ptr) {
            uft_alloc_stats.total_allocations++;
            uft_alloc_stats.current_bytes += size;
            if (uft_alloc_stats.current_bytes > uft_alloc_stats.peak_bytes) {
                uft_alloc_stats.peak_bytes = uft_alloc_stats.current_bytes;
            }
        } else {
            uft_alloc_stats.failed_allocations++;
        }
    }
    
    return ptr;
}

void* uft_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;
    
    /* Check for overflow */
    if (count > SIZE_MAX / size) {
        if (uft_alloc_tracking_enabled) {
            uft_alloc_stats.failed_allocations++;
        }
        return NULL;
    }
    
    void *ptr = calloc(count, size);
    
    if (uft_alloc_tracking_enabled) {
        if (ptr) {
            uft_alloc_stats.total_allocations++;
            uft_alloc_stats.current_bytes += count * size;
            if (uft_alloc_stats.current_bytes > uft_alloc_stats.peak_bytes) {
                uft_alloc_stats.peak_bytes = uft_alloc_stats.current_bytes;
            }
        } else {
            uft_alloc_stats.failed_allocations++;
        }
    }
    
    return ptr;
}

void* uft_realloc(void *ptr, size_t new_size) {
    if (new_size == 0) {
        uft_free(ptr);
        return NULL;
    }
    
    void *new_ptr = realloc(ptr, new_size);
    
    if (uft_alloc_tracking_enabled) {
        if (new_ptr) {
            if (ptr == NULL) {
                uft_alloc_stats.total_allocations++;
            }
            /* Note: We can't track size delta without malloc_usable_size */
        } else {
            uft_alloc_stats.failed_allocations++;
        }
    }
    
    return new_ptr;
}

void uft_free(void *ptr) {
    if (!ptr) return;
    
    if (uft_alloc_tracking_enabled) {
        uft_alloc_stats.total_frees++;
        /* Note: We can't track size without malloc_usable_size */
    }
    
    free(ptr);
}

char* uft_strdup(const char *str) {
    if (!str) return NULL;
    
    size_t len = strlen(str) + 1;
    char *dup = (char *)uft_malloc(len);
    
    if (dup) {
        memcpy(dup, str, len);
    }
    
    return dup;
}

/*===========================================================================
 * Tracked Allocation (Debug)
 *===========================================================================*/

void* uft_malloc_tracked(size_t size, const char *file, int line) {
    void *ptr = uft_malloc(size);
    
#ifdef UFT_ALLOC_VERBOSE
    if (ptr) {
        fprintf(stderr, "[ALLOC] malloc(%zu) = %p at %s:%d\n", 
                size, ptr, file, line);
    } else {
        fprintf(stderr, "[ALLOC] malloc(%zu) FAILED at %s:%d\n", 
                size, file, line);
    }
#else
    (void)file;
    (void)line;
#endif
    
    return ptr;
}

void* uft_calloc_tracked(size_t count, size_t size, const char *file, int line) {
    void *ptr = uft_calloc(count, size);
    
#ifdef UFT_ALLOC_VERBOSE
    if (ptr) {
        fprintf(stderr, "[ALLOC] calloc(%zu, %zu) = %p at %s:%d\n", 
                count, size, ptr, file, line);
    } else {
        fprintf(stderr, "[ALLOC] calloc(%zu, %zu) FAILED at %s:%d\n", 
                count, size, file, line);
    }
#else
    (void)file;
    (void)line;
#endif
    
    return ptr;
}

void uft_free_tracked(void *ptr, const char *file, int line) {
#ifdef UFT_ALLOC_VERBOSE
    fprintf(stderr, "[ALLOC] free(%p) at %s:%d\n", ptr, file, line);
#else
    (void)file;
    (void)line;
#endif
    
    uft_free(ptr);
}

/*===========================================================================
 * Statistics Functions
 *===========================================================================*/

const uft_alloc_stats_t* uft_alloc_get_stats(void) {
    return &uft_alloc_stats;
}

void uft_alloc_reset_stats(void) {
    memset(&uft_alloc_stats, 0, sizeof(uft_alloc_stats));
}

void uft_alloc_set_tracking(bool enable) {
    uft_alloc_tracking_enabled = enable;
}

void uft_alloc_print_stats(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  UFT Allocation Statistics\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Total allocations:  %zu\n", uft_alloc_stats.total_allocations);
    printf("  Total frees:        %zu\n", uft_alloc_stats.total_frees);
    printf("  Failed allocations: %zu\n", uft_alloc_stats.failed_allocations);
    printf("  Peak bytes:         %zu\n", uft_alloc_stats.peak_bytes);
    printf("  Outstanding:        %zu\n", 
           uft_alloc_stats.total_allocations - uft_alloc_stats.total_frees);
    printf("═══════════════════════════════════════════════════════════\n");
}
