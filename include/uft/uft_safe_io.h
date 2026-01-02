/**
 * @file uft_safe_io.h
 * @brief Safe I/O Macros with Error Handling
 * 
 * Diese Macros beheben die 59 fread und 73 fseek Instanzen
 * ohne Return-Value Prüfung.
 * 
 * ELITE QA FIX: P1 Priority Items
 */

#ifndef UFT_SAFE_IO_H
#define UFT_SAFE_IO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Safe fread with complete error handling
// ============================================================================

/**
 * @brief Safe fread that checks return value
 * @param ptr Destination buffer
 * @param size Element size
 * @param count Number of elements
 * @param stream File stream
 * @param bytes_read Output: actual bytes read (can be NULL)
 * @return true on success (all bytes read), false on error/partial
 */
static inline bool uft_safe_fread(void* ptr, size_t size, size_t count,
                                   FILE* stream, size_t* bytes_read) {
    if (!ptr || !stream) {
        if (bytes_read) *bytes_read = 0;
        return false;
    }
    
    size_t expected = size * count;
    if (expected == 0) {
        if (bytes_read) *bytes_read = 0;
        return true;
    }
    
    size_t actual = fread(ptr, 1, expected, stream);
    if (bytes_read) *bytes_read = actual;
    
    if (actual != expected) {
        // Check if EOF or error
        if (ferror(stream)) {
            return false;
        }
        // EOF before expected bytes - partial read
        return false;
    }
    
    return true;
}

/**
 * @brief Safe fread macro with goto on error
 * Usage: UFT_SAFE_FREAD(buf, 1, 512, fp, fail_label);
 */
#define UFT_SAFE_FREAD(ptr, size, count, stream, fail_label) \
    do { \
        size_t _expected = (size_t)(size) * (size_t)(count); \
        size_t _actual = fread((ptr), 1, _expected, (stream)); \
        if (_actual != _expected) { \
            goto fail_label; \
        } \
    } while (0)

/**
 * @brief Safe fread with return on error
 */
#define UFT_SAFE_FREAD_RET(ptr, size, count, stream, retval) \
    do { \
        size_t _expected = (size_t)(size) * (size_t)(count); \
        size_t _actual = fread((ptr), 1, _expected, (stream)); \
        if (_actual != _expected) { \
            return (retval); \
        } \
    } while (0)

// ============================================================================
// Safe fseek with error handling
// ============================================================================

/**
 * @brief Safe fseek that checks return value
 */
static inline bool uft_safe_fseek(FILE* stream, long offset, int whence) {
    if (!stream) return false;
    
    if (fseek(stream, offset, whence) != 0) {
        return false;
    }
    
    return true;
}

/**
 * @brief Safe fseek macro with goto on error
 */
#define UFT_SAFE_FSEEK(stream, offset, whence, fail_label) \
    do { \
        if (fseek((stream), (offset), (whence)) != 0) { \
            goto fail_label; \
        } \
    } while (0)

/**
 * @brief Safe fseek with return on error
 */
#define UFT_SAFE_FSEEK_RET(stream, offset, whence, retval) \
    do { \
        if (fseek((stream), (offset), (whence)) != 0) { \
            return (retval); \
        } \
    } while (0)

// ============================================================================
// Safe fwrite with error handling
// ============================================================================

/**
 * @brief Safe fwrite that checks return value
 */
static inline bool uft_safe_fwrite(const void* ptr, size_t size, size_t count,
                                    FILE* stream, size_t* bytes_written) {
    if (!ptr || !stream) {
        if (bytes_written) *bytes_written = 0;
        return false;
    }
    
    size_t expected = size * count;
    if (expected == 0) {
        if (bytes_written) *bytes_written = 0;
        return true;
    }
    
    size_t actual = fwrite(ptr, 1, expected, stream);
    if (bytes_written) *bytes_written = actual;
    
    return (actual == expected);
}

#define UFT_SAFE_FWRITE(ptr, size, count, stream, fail_label) \
    do { \
        size_t _expected = (size_t)(size) * (size_t)(count); \
        size_t _actual = fwrite((ptr), 1, _expected, (stream)); \
        if (_actual != _expected) { \
            goto fail_label; \
        } \
    } while (0)

// ============================================================================
// Safe malloc with NULL check
// ============================================================================

/**
 * @brief Safe malloc that clears memory and handles NULL
 */
static inline void* uft_safe_malloc(size_t size) {
    if (size == 0) return NULL;
    
    void* ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/**
 * @brief Safe malloc macro with goto on NULL
 */
#define UFT_SAFE_MALLOC(ptr, size, fail_label) \
    do { \
        (ptr) = uft_safe_malloc(size); \
        if (!(ptr)) { \
            goto fail_label; \
        } \
    } while (0)

/**
 * @brief Safe malloc with return on NULL
 */
#define UFT_SAFE_MALLOC_RET(ptr, size, retval) \
    do { \
        (ptr) = uft_safe_malloc(size); \
        if (!(ptr)) { \
            return (retval); \
        } \
    } while (0)

// ============================================================================
// Safe calloc
// ============================================================================

#define UFT_SAFE_CALLOC(ptr, count, size, fail_label) \
    do { \
        (ptr) = calloc((count), (size)); \
        if (!(ptr)) { \
            goto fail_label; \
        } \
    } while (0)

// ============================================================================
// Safe file open
// ============================================================================

#define UFT_SAFE_FOPEN(fp, path, mode, fail_label) \
    do { \
        (fp) = fopen((path), (mode)); \
        if (!(fp)) { \
            goto fail_label; \
        } \
    } while (0)

// ============================================================================
// Safe string operations
// ============================================================================

/**
 * @brief Safe snprintf that always null-terminates
 */
#define UFT_SAFE_SNPRINTF(buf, size, ...) \
    do { \
        int _n = snprintf((buf), (size), __VA_ARGS__); \
        if ((size) > 0) (buf)[(size) - 1] = '\0'; \
        (void)_n; \
    } while (0)

/**
 * @brief Safe string append (replacement for strcat)
 */
static inline int uft_safe_strcat(char* dest, size_t dest_size, 
                                   const char* src) {
    if (!dest || !src || dest_size == 0) return -1;
    
    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);
    
    if (dest_len + src_len >= dest_size) {
        // Would overflow - truncate
        size_t copy_len = dest_size - dest_len - 1;
        if (copy_len > 0) {
            memcpy(dest + dest_len, src, copy_len);
            dest[dest_size - 1] = '\0';
        }
        return -1;  // Truncated
    }
    
    memcpy(dest + dest_len, src, src_len + 1);
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif // UFT_SAFE_IO_H
