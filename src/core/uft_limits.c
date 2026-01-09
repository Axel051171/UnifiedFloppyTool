#include "uft/compat/uft_platform.h"
/**
 * @file uft_limits.c
 * @brief Resource limits implementation
 */

#include "uft/core/uft_limits.h"
#include "uft/uft_compiler.h"
#include <string.h>
#include <stdlib.h>

// Thread-local storage for limits
static UFT_THREAD_LOCAL uft_limits_t g_limits;
static UFT_THREAD_LOCAL bool g_limits_initialized = false;
static UFT_THREAD_LOCAL size_t g_total_allocated = 0;

static const uft_limits_t DEFAULT_LIMITS = {
    .max_file_size = UFT_DEFAULT_MAX_FILE_SIZE,
    .max_single_alloc = UFT_DEFAULT_MAX_SINGLE_ALLOC,
    .max_total_alloc = UFT_DEFAULT_MAX_TOTAL_ALLOC,
    .max_tracks = UFT_DEFAULT_MAX_TRACKS,
    .max_sides = 2,
    .max_sectors = UFT_DEFAULT_MAX_SECTORS,
    .max_sector_size = UFT_DEFAULT_MAX_SECTOR_SIZE,
    .max_revolutions = UFT_DEFAULT_MAX_REVOLUTIONS,
    .max_flux_per_rev = UFT_DEFAULT_MAX_FLUX_PER_REV,
    .max_string_length = UFT_DEFAULT_MAX_STRING_LENGTH,
    .max_path_length = UFT_DEFAULT_MAX_PATH_LENGTH,
    .io_timeout_ms = UFT_DEFAULT_IO_TIMEOUT_MS,
    .usb_timeout_ms = 5000,
    .scp = { .max_file_size = 512 * 1024 * 1024, .max_track_size = 2 * 1024 * 1024 },
    .d64 = { .max_file_size = 300 * 1024 },
    .g64 = { .max_file_size = 50 * 1024 * 1024, .max_track_size = 8192 },
    .hfe = { .max_file_size = 100 * 1024 * 1024, .max_track_size = 100 * 1024 },
    .adf = { .max_file_size = 10 * 1024 * 1024 },
};

void uft_limits_get_defaults(uft_limits_t* limits) {
    if (limits) *limits = DEFAULT_LIMITS;
}

const uft_limits_t* uft_limits_set(const uft_limits_t* limits) {
    const uft_limits_t* old = &g_limits;
    if (limits) {
        g_limits = *limits;
        g_limits_initialized = true;
    }
    return old;
}

const uft_limits_t* uft_limits_get(void) {
    if (!g_limits_initialized) {
        g_limits = DEFAULT_LIMITS;
        g_limits_initialized = true;
    }
    return &g_limits;
}

void uft_limits_reset(void) {
    g_limits = DEFAULT_LIMITS;
    g_limits_initialized = true;
    g_total_allocated = 0;
}

bool uft_check_file_size(size_t size) {
    return size <= uft_limits_get()->max_file_size;
}

bool uft_check_alloc_size(size_t size) {
    const uft_limits_t* lim = uft_limits_get();
    return size <= lim->max_single_alloc && 
           g_total_allocated + size <= lim->max_total_alloc;
}

void uft_track_allocation(ssize_t size) {
    if (size > 0) {
        g_total_allocated += (size_t)size;
    } else if (size < 0 && g_total_allocated >= (size_t)(-size)) {
        g_total_allocated -= (size_t)(-size);
    }
}

size_t uft_get_total_allocation(void) {
    return g_total_allocated;
}

void* uft_malloc_limited(size_t size) {
    if (!uft_check_alloc_size(size)) return NULL;
    void* ptr = malloc(size);
    if (ptr) uft_track_allocation((ssize_t)size);
    return ptr;
}

void* uft_realloc_limited(void* ptr, size_t old_size, size_t new_size) {
    if (new_size > old_size) {
        size_t diff = new_size - old_size;
        if (!uft_check_alloc_size(diff)) return NULL;
    }
    void* new_ptr = realloc(ptr, new_size);
    if (new_ptr) {
        if (new_size > old_size) {
            uft_track_allocation((ssize_t)(new_size - old_size));
        } else {
            uft_track_allocation(-(ssize_t)(old_size - new_size));
        }
    }
    return new_ptr;
}

void uft_free_limited(void* ptr, size_t size) {
    if (ptr) {
        free(ptr);
        uft_track_allocation(-(ssize_t)size);
    }
}

void uft_limits_get_embedded(uft_limits_t* limits) {
    if (!limits) return;
    *limits = DEFAULT_LIMITS;
    limits->max_file_size = 8 * 1024 * 1024;      // 8 MB
    limits->max_single_alloc = 1 * 1024 * 1024;   // 1 MB
    limits->max_total_alloc = 4 * 1024 * 1024;    // 4 MB
    limits->max_revolutions = 5;
    limits->max_flux_per_rev = 100000;
}

void uft_limits_get_server(uft_limits_t* limits) {
    if (!limits) return;
    *limits = DEFAULT_LIMITS;
    limits->max_file_size = 2UL * 1024 * 1024 * 1024;   // 2 GB
    limits->max_single_alloc = 256 * 1024 * 1024;       // 256 MB
    limits->max_total_alloc = 1024UL * 1024 * 1024;     // 1 GB
}

void uft_limits_get_paranoid(uft_limits_t* limits) {
    if (!limits) return;
    *limits = DEFAULT_LIMITS;
    limits->max_file_size = 1 * 1024 * 1024;      // 1 MB
    limits->max_single_alloc = 64 * 1024;         // 64 KB
    limits->max_total_alloc = 256 * 1024;         // 256 KB
    limits->max_tracks = 84;
    limits->max_sectors = 32;
    limits->max_revolutions = 5;
    limits->io_timeout_ms = 1000;
}
