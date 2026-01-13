/**
 * @file uft_safe_io.h
 * @brief Safe I/O wrappers with error handling
 * 
 * P0-001: All file operations with NULL checks and cleanup
 */

#ifndef UFT_SAFE_IO_H
#define UFT_SAFE_IO_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Codes
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_IO_OK = 0,
    UFT_IO_ERR_NULL_PATH = -1,
    UFT_IO_ERR_OPEN_FAILED = -2,
    UFT_IO_ERR_READ_FAILED = -3,
    UFT_IO_ERR_WRITE_FAILED = -4,
    UFT_IO_ERR_SEEK_FAILED = -5,
    UFT_IO_ERR_ALLOC_FAILED = -6,
    UFT_IO_ERR_FILE_TOO_LARGE = -7,
    UFT_IO_ERR_PARTIAL_READ = -8,
    UFT_IO_ERR_PARTIAL_WRITE = -9,
} uft_io_error_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Safe File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Safe fopen with logging
 * @param path File path
 * @param mode Open mode
 * @param error_out Optional error code output
 * @return FILE pointer or NULL
 */
static inline FILE* uft_fopen_safe(const char *path, const char *mode, 
                                    uft_io_error_t *error_out) {
    if (!path || !mode) {
        if (error_out) *error_out = UFT_IO_ERR_NULL_PATH;
        return NULL;
    }
    
    FILE *f = fopen(path, mode);
    if (!f) {
        if (error_out) *error_out = UFT_IO_ERR_OPEN_FAILED;
        /* Log error - could be extended with callback */
        #ifndef UFT_SILENT_IO
        fprintf(stderr, "[UFT I/O] Failed to open '%s': %s\n", path, strerror(errno));
        #endif
        return NULL;
    }
    
    if (error_out) *error_out = UFT_IO_OK;
    return f;
}

/**
 * @brief Read entire file into buffer
 * @param path File path
 * @param size_out Output file size
 * @param error_out Optional error code output
 * @return Allocated buffer (caller must free) or NULL
 */
static inline uint8_t* uft_read_file(const char *path, size_t *size_out, 
                                      uft_io_error_t *error_out) {
    if (!path) {
        if (error_out) *error_out = UFT_IO_ERR_NULL_PATH;
        if (size_out) *size_out = 0;
        return NULL;
    }
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (error_out) *error_out = UFT_IO_ERR_OPEN_FAILED;
        if (size_out) *size_out = 0;
        return NULL;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size < 0 || file_size > 256 * 1024 * 1024) { /* 256MB limit */
        if (error_out) *error_out = UFT_IO_ERR_FILE_TOO_LARGE;
        if (size_out) *size_out = 0;
        fclose(f);
        return NULL;
    }
    
    uint8_t *buffer = (uint8_t*)malloc((size_t)file_size);
    if (!buffer) {
        if (error_out) *error_out = UFT_IO_ERR_ALLOC_FAILED;
        if (size_out) *size_out = 0;
        fclose(f);
        return NULL;
    }
    
    size_t read = fread(buffer, 1, (size_t)file_size, f);
    fclose(f);
    
    if (read != (size_t)file_size) {
        if (error_out) *error_out = UFT_IO_ERR_PARTIAL_READ;
        /* Still return what we got */
    } else {
        if (error_out) *error_out = UFT_IO_OK;
    }
    
    if (size_out) *size_out = read;
    return buffer;
}

/**
 * @brief Write buffer to file
 * @param path File path
 * @param data Data to write
 * @param size Data size
 * @return UFT_IO_OK on success
 */
static inline uft_io_error_t uft_write_file(const char *path, const void *data, 
                                             size_t size) {
    if (!path || !data) {
        return UFT_IO_ERR_NULL_PATH;
    }
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        #ifndef UFT_SILENT_IO
        fprintf(stderr, "[UFT I/O] Failed to create '%s': %s\n", path, strerror(errno));
        #endif
        return UFT_IO_ERR_OPEN_FAILED;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written != size) {
        return UFT_IO_ERR_PARTIAL_WRITE;
    }
    
    return UFT_IO_OK;
}

/**
 * @brief Safe file read with bounds checking
 * @param f File handle
 * @param buffer Output buffer
 * @param size Bytes to read
 * @param actual_out Actual bytes read
 * @return UFT_IO_OK on success
 */
static inline uft_io_error_t uft_fread_safe(FILE *f, void *buffer, size_t size, 
                                             size_t *actual_out) {
    if (!f || !buffer) {
        if (actual_out) *actual_out = 0;
        return UFT_IO_ERR_NULL_PATH;
    }
    
    size_t read = fread(buffer, 1, size, f);
    if (actual_out) *actual_out = read;
    
    if (read == 0 && ferror(f)) {
        return UFT_IO_ERR_READ_FAILED;
    }
    
    return (read == size) ? UFT_IO_OK : UFT_IO_ERR_PARTIAL_READ;
}

/**
 * @brief Safe file write
 * @param f File handle
 * @param data Data to write
 * @param size Bytes to write
 * @return UFT_IO_OK on success
 */
static inline uft_io_error_t uft_fwrite_safe(FILE *f, const void *data, size_t size) {
    if (!f || !data) {
        return UFT_IO_ERR_NULL_PATH;
    }
    
    size_t written = fwrite(data, 1, size, f);
    return (written == size) ? UFT_IO_OK : UFT_IO_ERR_PARTIAL_WRITE;
}

/**
 * @brief Get error message for error code
 */
static inline const char* uft_io_error_str(uft_io_error_t err) {
    switch (err) {
        case UFT_IO_OK: return "Success";
        case UFT_IO_ERR_NULL_PATH: return "Null path or parameter";
        case UFT_IO_ERR_OPEN_FAILED: return "Failed to open file";
        case UFT_IO_ERR_READ_FAILED: return "Read operation failed";
        case UFT_IO_ERR_WRITE_FAILED: return "Write operation failed";
        case UFT_IO_ERR_SEEK_FAILED: return "Seek operation failed";
        case UFT_IO_ERR_ALLOC_FAILED: return "Memory allocation failed";
        case UFT_IO_ERR_FILE_TOO_LARGE: return "File too large";
        case UFT_IO_ERR_PARTIAL_READ: return "Partial read";
        case UFT_IO_ERR_PARTIAL_WRITE: return "Partial write";
        default: return "Unknown error";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * RAII-style Cleanup Macros (for C)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_CLEANUP_FILE __attribute__((cleanup(uft_cleanup_file_ptr)))

static inline void uft_cleanup_file_ptr(FILE **fp) {
    if (fp && *fp) {
        fclose(*fp);
        *fp = NULL;
    }
}

#define UFT_CLEANUP_BUFFER __attribute__((cleanup(uft_cleanup_buffer_ptr)))

static inline void uft_cleanup_buffer_ptr(void **ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

/* Usage example:
 * 
 * int process_file(const char *path) {
 *     UFT_CLEANUP_FILE FILE *f = uft_fopen_safe(path, "rb", NULL);
 *     if (!f) return -1;
 *     
 *     UFT_CLEANUP_BUFFER uint8_t *buf = malloc(1024);
 *     if (!buf) return -1;  // f automatically closed
 *     
 *     // ... use f and buf ...
 *     return 0;  // f closed, buf freed automatically
 * }
 */

#ifdef __cplusplus
}
#endif

#endif /* UFT_SAFE_IO_H */
