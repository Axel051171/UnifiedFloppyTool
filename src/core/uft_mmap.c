/**
 * @file uft_mmap.c
 * @brief Memory-Mapped I/O Implementation
 * 
 * @version 4.1.0
 * @date 2026-01-03
 * 
 * Cross-platform implementation using:
 * - POSIX: mmap/munmap
 * - Windows: CreateFileMapping/MapViewOfFile
 */

#include "uft/uft_mmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <io.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <errno.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct uft_mmap {
    char *filename;              /* Mapped filename (owned) */
    void *data;                  /* Mapped data pointer */
    size_t size;                 /* Mapped size */
    size_t file_size;            /* Total file size */
    size_t offset;               /* Offset into file */
    uint32_t flags;              /* Access flags */
    bool is_valid;               /* Mapping valid? */
    
#ifdef _WIN32
    HANDLE file_handle;          /* File handle */
    HANDLE map_handle;           /* Mapping handle */
#else
    int fd;                      /* File descriptor */
#endif
};

/* Thread-local error storage */
#ifdef _WIN32
    static __declspec(thread) int g_last_error = UFT_MMAP_OK;
#else
    static __thread int g_last_error = UFT_MMAP_OK;
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Platform Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

size_t uft_mmap_page_size(void)
{
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwAllocationGranularity;
#else
    return (size_t)sysconf(_SC_PAGESIZE);
#endif
}

size_t uft_mmap_align_offset(size_t offset)
{
    size_t page = uft_mmap_page_size();
    return (offset / page) * page;
}

size_t uft_mmap_align_length(size_t length)
{
    size_t page = uft_mmap_page_size();
    return ((length + page - 1) / page) * page;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Handling
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void set_error(int err)
{
    g_last_error = err;
}

int uft_mmap_get_last_error(void)
{
    return g_last_error;
}

const char *uft_mmap_error_string(int error)
{
    switch (error) {
        case UFT_MMAP_OK:           return "Success";
        case UFT_MMAP_ERR_OPEN:     return "Failed to open file";
        case UFT_MMAP_ERR_MAP:      return "Failed to create mapping";
        case UFT_MMAP_ERR_SIZE:     return "Invalid file size";
        case UFT_MMAP_ERR_MEMORY:   return "Memory allocation failed";
        case UFT_MMAP_ERR_ACCESS:   return "Access denied";
        case UFT_MMAP_ERR_LOCKED:   return "File is locked";
        case UFT_MMAP_ERR_INVALID:  return "Invalid parameter";
        case UFT_MMAP_ERR_SYNC:     return "Sync failed";
        case UFT_MMAP_ERR_TRUNCATE: return "Truncate failed";
        default:                    return "Unknown error";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Core Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_mmap_t *uft_mmap_open(const char *filename, uint32_t flags)
{
    return uft_mmap_open_range(filename, 0, 0, flags);
}

uft_mmap_t *uft_mmap_open_range(
    const char *filename,
    size_t offset,
    size_t length,
    uint32_t flags)
{
    if (!filename) {
        set_error(UFT_MMAP_ERR_INVALID);
        return NULL;
    }
    
    /* Allocate handle */
    uft_mmap_t *map = (uft_mmap_t*)calloc(1, sizeof(uft_mmap_t));
    if (!map) {
        set_error(UFT_MMAP_ERR_MEMORY);
        return NULL;
    }
    
    map->filename = strdup(filename);
    if (!map->filename) {
        free(map);
        set_error(UFT_MMAP_ERR_MEMORY);
        return NULL;
    }
    
    map->flags = flags;
    
#ifdef _WIN32
    /* Windows implementation */
    DWORD access = GENERIC_READ;
    DWORD share = FILE_SHARE_READ;
    DWORD protect = PAGE_READONLY;
    DWORD map_access = FILE_MAP_READ;
    
    if (flags & UFT_MMAP_WRITE) {
        access |= GENERIC_WRITE;
        share |= FILE_SHARE_WRITE;
        protect = PAGE_READWRITE;
        map_access = FILE_MAP_WRITE;
    }
    if (flags & UFT_MMAP_COPY_ON_WRITE) {
        protect = PAGE_WRITECOPY;
        map_access = FILE_MAP_COPY;
    }
    
    /* Open file */
    map->file_handle = CreateFileA(
        filename,
        access,
        share,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (map->file_handle == INVALID_HANDLE_VALUE) {
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_OPEN);
        return NULL;
    }
    
    /* Get file size */
    LARGE_INTEGER li;
    if (!GetFileSizeEx(map->file_handle, &li)) {
        CloseHandle(map->file_handle);
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_SIZE);
        return NULL;
    }
    map->file_size = (size_t)li.QuadPart;
    
    /* Calculate mapping range */
    size_t aligned_offset = uft_mmap_align_offset(offset);
    size_t offset_adjust = offset - aligned_offset;
    
    if (length == 0) {
        length = map->file_size - offset;
    }
    
    size_t map_length = length + offset_adjust;
    
    /* Create file mapping */
    map->map_handle = CreateFileMappingA(
        map->file_handle,
        NULL,
        protect,
        0, 0,  /* Map entire file */
        NULL
    );
    
    if (!map->map_handle) {
        CloseHandle(map->file_handle);
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_MAP);
        return NULL;
    }
    
    /* Map view */
    DWORD offset_high = (DWORD)(aligned_offset >> 32);
    DWORD offset_low = (DWORD)(aligned_offset & 0xFFFFFFFF);
    
    map->data = MapViewOfFile(
        map->map_handle,
        map_access,
        offset_high,
        offset_low,
        map_length
    );
    
    if (!map->data) {
        CloseHandle(map->map_handle);
        CloseHandle(map->file_handle);
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_MAP);
        return NULL;
    }
    
    /* Adjust pointer for unaligned offset */
    map->data = (uint8_t*)map->data + offset_adjust;
    map->size = length;
    map->offset = offset;
    
#else
    /* POSIX implementation */
    int open_flags = O_RDONLY;
    int prot = PROT_READ;
    int map_flags = MAP_PRIVATE;
    
    if (flags & UFT_MMAP_WRITE) {
        open_flags = O_RDWR;
        prot |= PROT_WRITE;
        map_flags = MAP_SHARED;
    }
    if (flags & UFT_MMAP_COPY_ON_WRITE) {
        map_flags = MAP_PRIVATE;
    }
    
    /* Open file */
    map->fd = open(filename, open_flags);
    if (map->fd < 0) {
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_OPEN);
        return NULL;
    }
    
    /* Get file size */
    struct stat st;
    if (fstat(map->fd, &st) < 0) {
        close(map->fd);
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_SIZE);
        return NULL;
    }
    map->file_size = (size_t)st.st_size;
    
    /* Calculate mapping range */
    size_t aligned_offset = uft_mmap_align_offset(offset);
    size_t offset_adjust = offset - aligned_offset;
    
    if (length == 0) {
        length = map->file_size - offset;
    }
    
    size_t map_length = length + offset_adjust;
    
    /* Create mapping */
    void *ptr = mmap(NULL, map_length, prot, map_flags, map->fd, (off_t)aligned_offset);
    if (ptr == MAP_FAILED) {
        close(map->fd);
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_MAP);
        return NULL;
    }
    
    /* Apply access hints */
    if (flags & UFT_MMAP_SEQUENTIAL) {
        madvise(ptr, map_length, MADV_SEQUENTIAL);
    }
    if (flags & UFT_MMAP_RANDOM) {
        madvise(ptr, map_length, MADV_RANDOM);
    }
    if (flags & UFT_MMAP_WILLNEED) {
        madvise(ptr, map_length, MADV_WILLNEED);
    }
    
    map->data = (uint8_t*)ptr + offset_adjust;
    map->size = length;
    map->offset = offset;
#endif
    
    map->is_valid = true;
    set_error(UFT_MMAP_OK);
    return map;
}

uft_mmap_t *uft_mmap_create(const char *filename, size_t size, uint32_t flags)
{
    if (!filename || size == 0) {
        set_error(UFT_MMAP_ERR_INVALID);
        return NULL;
    }
    
    if (!(flags & UFT_MMAP_WRITE)) {
        flags |= UFT_MMAP_WRITE;
    }
    
    uft_mmap_t *map = (uft_mmap_t*)calloc(1, sizeof(uft_mmap_t));
    if (!map) {
        set_error(UFT_MMAP_ERR_MEMORY);
        return NULL;
    }
    
    map->filename = strdup(filename);
    map->flags = flags;
    map->file_size = size;
    map->size = size;
    map->offset = 0;
    
#ifdef _WIN32
    /* Create file */
    map->file_handle = CreateFileA(
        filename,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (map->file_handle == INVALID_HANDLE_VALUE) {
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_OPEN);
        return NULL;
    }
    
    /* Set file size */
    LARGE_INTEGER li;
    li.QuadPart = (LONGLONG)size;
    if (!SetFilePointerEx(map->file_handle, li, NULL, FILE_BEGIN) ||
        !SetEndOfFile(map->file_handle)) {
        CloseHandle(map->file_handle);
        DeleteFileA(filename);
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_TRUNCATE);
        return NULL;
    }
    
    /* Create mapping */
    map->map_handle = CreateFileMappingA(
        map->file_handle,
        NULL,
        PAGE_READWRITE,
        (DWORD)(size >> 32),
        (DWORD)(size & 0xFFFFFFFF),
        NULL
    );
    
    if (!map->map_handle) {
        CloseHandle(map->file_handle);
        DeleteFileA(filename);
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_MAP);
        return NULL;
    }
    
    map->data = MapViewOfFile(map->map_handle, FILE_MAP_WRITE, 0, 0, size);
    if (!map->data) {
        CloseHandle(map->map_handle);
        CloseHandle(map->file_handle);
        DeleteFileA(filename);
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_MAP);
        return NULL;
    }
    
#else
    /* Create file */
    map->fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (map->fd < 0) {
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_OPEN);
        return NULL;
    }
    
    /* Set file size */
    if (ftruncate(map->fd, (off_t)size) < 0) {
        close(map->fd);
        unlink(filename);
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_TRUNCATE);
        return NULL;
    }
    
    /* Create mapping */
    map->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, map->fd, 0);
    if (map->data == MAP_FAILED) {
        close(map->fd);
        unlink(filename);
        free(map->filename);
        free(map);
        set_error(UFT_MMAP_ERR_MAP);
        return NULL;
    }
#endif
    
    /* Zero-fill */
    memset(map->data, 0, size);
    
    map->is_valid = true;
    set_error(UFT_MMAP_OK);
    return map;
}

void uft_mmap_close(uft_mmap_t *map)
{
    if (!map) return;
    
#ifdef _WIN32
    if (map->data) {
        /* Adjust for offset */
        size_t offset_adjust = map->offset - uft_mmap_align_offset(map->offset);
        UnmapViewOfFile((uint8_t*)map->data - offset_adjust);
    }
    if (map->map_handle) CloseHandle(map->map_handle);
    if (map->file_handle) CloseHandle(map->file_handle);
#else
    if (map->data && map->data != MAP_FAILED) {
        size_t offset_adjust = map->offset - uft_mmap_align_offset(map->offset);
        size_t map_length = map->size + offset_adjust;
        munmap((uint8_t*)map->data - offset_adjust, map_length);
    }
    if (map->fd >= 0) close(map->fd);
#endif
    
    free(map->filename);
    free(map);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Accessors
 * ═══════════════════════════════════════════════════════════════════════════════ */

void *uft_mmap_ptr(uft_mmap_t *map)
{
    return map ? map->data : NULL;
}

const void *uft_mmap_ptr_const(const uft_mmap_t *map)
{
    return map ? map->data : NULL;
}

size_t uft_mmap_size(const uft_mmap_t *map)
{
    return map ? map->size : 0;
}

size_t uft_mmap_offset(const uft_mmap_t *map)
{
    return map ? map->offset : 0;
}

bool uft_mmap_is_writable(const uft_mmap_t *map)
{
    return map && (map->flags & UFT_MMAP_WRITE);
}

bool uft_mmap_is_valid(const uft_mmap_t *map)
{
    return map && map->is_valid && map->data;
}

int uft_mmap_get_info(const uft_mmap_t *map, uft_mmap_info_t *info)
{
    if (!map || !info) return UFT_MMAP_ERR_INVALID;
    
    info->filename = map->filename;
    info->file_size = map->file_size;
    info->mapped_size = map->size;
    info->mapped_offset = map->offset;
    info->flags = map->flags;
    info->is_partial = (map->offset > 0 || map->size < map->file_size);
    info->page_size = uft_mmap_page_size();
    
    return UFT_MMAP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Synchronization
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_mmap_sync(uft_mmap_t *map, bool async)
{
    return uft_mmap_sync_range(map, 0, map ? map->size : 0, async);
}

int uft_mmap_sync_range(uft_mmap_t *map, size_t offset, size_t length, bool async)
{
    if (!map || !map->data) return UFT_MMAP_ERR_INVALID;
    if (!(map->flags & UFT_MMAP_WRITE)) return UFT_MMAP_OK;  /* Nothing to sync */
    
    if (offset + length > map->size) {
        length = map->size - offset;
    }
    
#ifdef _WIN32
    if (!FlushViewOfFile((uint8_t*)map->data + offset, length)) {
        set_error(UFT_MMAP_ERR_SYNC);
        return UFT_MMAP_ERR_SYNC;
    }
    if (!async) {
        FlushFileBuffers(map->file_handle);
    }
#else
    int flags = async ? MS_ASYNC : MS_SYNC;
    
    /* Align to page boundary */
    size_t page = uft_mmap_page_size();
    size_t aligned_offset = (offset / page) * page;
    size_t adjust = offset - aligned_offset;
    
    if (msync((uint8_t*)map->data + aligned_offset, length + adjust, flags) < 0) {
        set_error(UFT_MMAP_ERR_SYNC);
        return UFT_MMAP_ERR_SYNC;
    }
#endif
    
    return UFT_MMAP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Memory Hints
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_mmap_advise(uft_mmap_t *map, size_t offset, size_t length, uint32_t advice)
{
    if (!map || !map->data) return UFT_MMAP_ERR_INVALID;
    
#ifdef _WIN32
    /* Windows doesn't have madvise, but we can use prefetch hints */
    (void)offset;
    (void)length;
    (void)advice;
    return UFT_MMAP_OK;
#else
    int hint = MADV_NORMAL;
    if (advice & UFT_MMAP_SEQUENTIAL) hint = MADV_SEQUENTIAL;
    if (advice & UFT_MMAP_RANDOM)     hint = MADV_RANDOM;
    if (advice & UFT_MMAP_WILLNEED)   hint = MADV_WILLNEED;
    if (advice & UFT_MMAP_DONTNEED)   hint = MADV_DONTNEED;
    
    if (madvise((uint8_t*)map->data + offset, length, hint) < 0) {
        return UFT_MMAP_ERR_INVALID;
    }
    return UFT_MMAP_OK;
#endif
}

int uft_mmap_lock(uft_mmap_t *map)
{
    if (!map || !map->data) return UFT_MMAP_ERR_INVALID;
    
#ifdef _WIN32
    if (!VirtualLock(map->data, map->size)) {
        return UFT_MMAP_ERR_LOCKED;
    }
#else
    if (mlock(map->data, map->size) < 0) {
        return UFT_MMAP_ERR_LOCKED;
    }
#endif
    return UFT_MMAP_OK;
}

int uft_mmap_unlock(uft_mmap_t *map)
{
    if (!map || !map->data) return UFT_MMAP_ERR_INVALID;
    
#ifdef _WIN32
    VirtualUnlock(map->data, map->size);
#else
    munlock(map->data, map->size);
#endif
    return UFT_MMAP_OK;
}

int uft_mmap_prefetch(uft_mmap_t *map, size_t offset, size_t length)
{
    return uft_mmap_advise(map, offset, length, UFT_MMAP_WILLNEED);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_mmap_resize(uft_mmap_t *map, size_t new_size)
{
    if (!map || !(map->flags & UFT_MMAP_WRITE)) {
        return UFT_MMAP_ERR_INVALID;
    }
    
    /* This is complex - need to unmap, resize, remap */
    /* For now, return unsupported */
    return UFT_MMAP_ERR_INVALID;
}

int uft_mmap_remap(uft_mmap_t *map, size_t offset, size_t length)
{
    if (!map) return UFT_MMAP_ERR_INVALID;
    
    /* Store info */
    char *filename = strdup(map->filename);
    uint32_t flags = map->flags;
    
    /* Close existing */
    uft_mmap_close(map);
    
    /* Reopen with new range - but this invalidates the pointer */
    /* This function should update the existing map in-place */
    /* For simplicity, return error - caller should use open_range */
    
    free(filename);
    return UFT_MMAP_ERR_INVALID;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * High-Level Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_mmap_t *uft_mmap_read_file(
    const char *filename,
    const uint8_t **data,
    size_t *size)
{
    uft_mmap_t *map = uft_mmap_open(filename, UFT_MMAP_READONLY);
    if (map) {
        if (data) *data = (const uint8_t*)map->data;
        if (size) *size = map->size;
    }
    return map;
}

uft_mmap_t *uft_mmap_create_file(
    const char *filename,
    size_t size,
    uint8_t **data)
{
    uft_mmap_t *map = uft_mmap_create(filename, size, UFT_MMAP_READWRITE);
    if (map && data) {
        *data = (uint8_t*)map->data;
    }
    return map;
}
