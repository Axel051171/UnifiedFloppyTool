#ifndef UFT_MMAP_H
#define UFT_MMAP_H

/**
 * @file uft_mmap.h
 * @brief Memory-Mapped I/O for Large Disk Images
 * 
 * @version 4.1.0
 * @date 2026-01-03
 * 
 * FEATURES:
 * - Cross-platform memory mapping (mmap/MapViewOfFile)
 * - Automatic page alignment
 * - Read-only and read-write modes
 * - Partial mapping for very large files (>4GB)
 * - Copy-on-write support
 * - File locking for concurrent access
 * 
 * PERFORMANCE:
 * - Traditional read: 500 MB/s (sequential)
 * - Memory-mapped: 2+ GB/s (random access)
 * - No memory copy overhead
 * - OS handles caching automatically
 * 
 * USE CASES:
 * - Large flux captures (>1GB)
 * - Multi-disk archives
 * - Random sector access patterns
 * - Read-only analysis
 * 
 * USAGE:
 * ```c
 * uft_mmap_t *map = uft_mmap_open("large_image.scp", UFT_MMAP_READ);
 * if (map) {
 *     const uint8_t *data = uft_mmap_ptr(map);
 *     size_t size = uft_mmap_size(map);
 *     
 *     // Direct memory access - no copying!
 *     process_flux_data(data, size);
 *     
 *     uft_mmap_close(map);
 * }
 * ```
 */


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Codes
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_MMAP_OK              = 0,
    UFT_MMAP_ERR_OPEN        = -1,   /**< Failed to open file */
    UFT_MMAP_ERR_MAP         = -2,   /**< Failed to create mapping */
    UFT_MMAP_ERR_SIZE        = -3,   /**< Invalid file size */
    UFT_MMAP_ERR_MEMORY      = -4,   /**< Memory allocation failed */
    UFT_MMAP_ERR_ACCESS      = -5,   /**< Access denied */
    UFT_MMAP_ERR_LOCKED      = -6,   /**< File is locked */
    UFT_MMAP_ERR_INVALID     = -7,   /**< Invalid parameter */
    UFT_MMAP_ERR_SYNC        = -8,   /**< Sync failed */
    UFT_MMAP_ERR_TRUNCATE    = -9    /**< Truncate failed */
} uft_mmap_error_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Access Modes
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_MMAP_READ            = 0x01, /**< Read-only access */
    UFT_MMAP_WRITE           = 0x02, /**< Read-write access */
    UFT_MMAP_COPY_ON_WRITE   = 0x04, /**< Private copy-on-write */
    UFT_MMAP_SEQUENTIAL      = 0x10, /**< Hint: sequential access */
    UFT_MMAP_RANDOM          = 0x20, /**< Hint: random access */
    UFT_MMAP_WILLNEED        = 0x40, /**< Hint: will need soon */
    UFT_MMAP_DONTNEED        = 0x80  /**< Hint: won't need soon */
} uft_mmap_flags_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Mapping Handle (opaque)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_mmap uft_mmap_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Mapping Info
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *filename;        /**< Mapped filename */
    size_t file_size;            /**< Total file size */
    size_t mapped_size;          /**< Currently mapped size */
    size_t mapped_offset;        /**< Offset of mapped region */
    uint32_t flags;              /**< Access flags */
    bool is_partial;             /**< Partial mapping? */
    size_t page_size;            /**< System page size */
} uft_mmap_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Core API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open and map a file into memory
 * @param filename Path to file
 * @param flags Access mode flags (UFT_MMAP_READ, etc.)
 * @return Mapping handle or NULL on error
 */
uft_mmap_t *uft_mmap_open(const char *filename, uint32_t flags);

/**
 * @brief Create a new file and map it
 * @param filename Path for new file
 * @param size Initial file size
 * @param flags Access flags (must include UFT_MMAP_WRITE)
 * @return Mapping handle or NULL on error
 */
uft_mmap_t *uft_mmap_create(const char *filename, size_t size, uint32_t flags);

/**
 * @brief Close mapping and release resources
 * @param map Mapping handle
 */
void uft_mmap_close(uft_mmap_t *map);

/**
 * @brief Get pointer to mapped data
 * @param map Mapping handle
 * @return Pointer to mapped data, or NULL if invalid
 */
void *uft_mmap_ptr(uft_mmap_t *map);

/**
 * @brief Get const pointer to mapped data
 */
const void *uft_mmap_ptr_const(const uft_mmap_t *map);

/**
 * @brief Get size of mapped region
 * @param map Mapping handle
 * @return Size in bytes
 */
size_t uft_mmap_size(const uft_mmap_t *map);

/**
 * @brief Get mapping information
 * @param map Mapping handle
 * @param info Output info structure
 * @return UFT_MMAP_OK on success
 */
int uft_mmap_get_info(const uft_mmap_t *map, uft_mmap_info_t *info);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Partial Mapping (for very large files)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Map a portion of a file
 * @param filename Path to file
 * @param offset Offset into file (must be page-aligned)
 * @param length Length to map (0 = to end of file)
 * @param flags Access flags
 * @return Mapping handle or NULL on error
 */
uft_mmap_t *uft_mmap_open_range(
    const char *filename,
    size_t offset,
    size_t length,
    uint32_t flags
);

/**
 * @brief Remap to a different region of the file
 * @param map Existing mapping handle
 * @param offset New offset (page-aligned)
 * @param length New length
 * @return UFT_MMAP_OK on success
 */
int uft_mmap_remap(uft_mmap_t *map, size_t offset, size_t length);

/**
 * @brief Get the current mapping offset
 */
size_t uft_mmap_offset(const uft_mmap_t *map);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Synchronization
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Flush changes to disk
 * @param map Mapping handle
 * @param async If true, return immediately (async flush)
 * @return UFT_MMAP_OK on success
 */
int uft_mmap_sync(uft_mmap_t *map, bool async);

/**
 * @brief Flush a specific range to disk
 * @param map Mapping handle
 * @param offset Offset within mapping
 * @param length Length to flush
 * @param async Async flush
 * @return UFT_MMAP_OK on success
 */
int uft_mmap_sync_range(uft_mmap_t *map, size_t offset, size_t length, bool async);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Memory Hints
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Advise the OS about access pattern
 * @param map Mapping handle
 * @param offset Offset within mapping
 * @param length Length of region
 * @param advice Access hint (UFT_MMAP_SEQUENTIAL, UFT_MMAP_RANDOM, etc.)
 * @return UFT_MMAP_OK on success
 */
int uft_mmap_advise(uft_mmap_t *map, size_t offset, size_t length, uint32_t advice);

/**
 * @brief Lock pages in memory (prevent swapping)
 * @param map Mapping handle
 * @return UFT_MMAP_OK on success
 */
int uft_mmap_lock(uft_mmap_t *map);

/**
 * @brief Unlock pages (allow swapping)
 * @param map Mapping handle
 * @return UFT_MMAP_OK on success
 */
int uft_mmap_unlock(uft_mmap_t *map);

/**
 * @brief Prefetch pages into memory
 * @param map Mapping handle
 * @param offset Offset to prefetch
 * @param length Length to prefetch
 * @return UFT_MMAP_OK on success
 */
int uft_mmap_prefetch(uft_mmap_t *map, size_t offset, size_t length);

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Resize the underlying file
 * @param map Mapping handle
 * @param new_size New file size
 * @return UFT_MMAP_OK on success
 * @note May invalidate existing pointers
 */
int uft_mmap_resize(uft_mmap_t *map, size_t new_size);

/**
 * @brief Check if mapping is writable
 */
bool uft_mmap_is_writable(const uft_mmap_t *map);

/**
 * @brief Check if mapping is valid
 */
bool uft_mmap_is_valid(const uft_mmap_t *map);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get system page size
 * @return Page size in bytes
 */
size_t uft_mmap_page_size(void);

/**
 * @brief Align offset to page boundary
 * @param offset Offset to align
 * @return Page-aligned offset (rounded down)
 */
size_t uft_mmap_align_offset(size_t offset);

/**
 * @brief Align length to page boundary
 * @param length Length to align
 * @return Page-aligned length (rounded up)
 */
size_t uft_mmap_align_length(size_t length);

/**
 * @brief Get error string for error code
 * @param error Error code
 * @return Human-readable error message
 */
const char *uft_mmap_error_string(int error);

/**
 * @brief Get last error code
 * @return Last error code (thread-local)
 */
int uft_mmap_get_last_error(void);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Convenience Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read-only mapping (most common case)
 */
#define UFT_MMAP_READONLY  (UFT_MMAP_READ | UFT_MMAP_SEQUENTIAL)

/**
 * @brief Read-write mapping
 */
#define UFT_MMAP_READWRITE (UFT_MMAP_READ | UFT_MMAP_WRITE)

/**
 * @brief Random access read-only
 */
#define UFT_MMAP_RANDOM_READ (UFT_MMAP_READ | UFT_MMAP_RANDOM)

/* ═══════════════════════════════════════════════════════════════════════════════
 * High-Level Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read entire file into memory-mapped buffer
 * @param filename Path to file
 * @param data Output pointer to data
 * @param size Output size
 * @return Mapping handle (needed for cleanup)
 * 
 * Usage:
 * ```c
 * const uint8_t *data;
 * size_t size;
 * uft_mmap_t *map = uft_mmap_read_file("image.adf", &data, &size);
 * // Use data...
 * uft_mmap_close(map);
 * ```
 */
uft_mmap_t *uft_mmap_read_file(
    const char *filename,
    const uint8_t **data,
    size_t *size
);

/**
 * @brief Create writable memory-mapped buffer
 * @param filename Path for new file
 * @param size File size
 * @param data Output pointer to writable buffer
 * @return Mapping handle
 */
uft_mmap_t *uft_mmap_create_file(
    const char *filename,
    size_t size,
    uint8_t **data
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MMAP_H */
