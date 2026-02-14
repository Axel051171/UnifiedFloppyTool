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
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Platform-specific ssize_t definition */
#if defined(_MSC_VER) || defined(_WIN32)
    /* Windows: Use SSIZE_T from BaseTsd.h */
    #include <BaseTsd.h>
    #ifndef _SSIZE_T_DEFINED
    #define _SSIZE_T_DEFINED
    typedef SSIZE_T ssize_t;
    #endif
#else
    /* POSIX systems (Linux, macOS, BSD) */
    #include <sys/types.h>
#endif

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

// ============================================================================
// Buffered Writer - For efficient byte-by-byte writes
// ============================================================================

#define UFT_BUF_WRITER_SIZE 4096

typedef enum {
    UFT_IO_OK = 0,
    UFT_IO_ERR_NULL_PTR,
    UFT_IO_ERR_WRITE_FAILED,
    UFT_IO_ERR_READ_FAILED,
    UFT_IO_ERR_SEEK_FAILED,
    UFT_IO_ERR_EOF
} uft_io_error_t;

typedef struct {
    FILE* fp;
    uint8_t buffer[UFT_BUF_WRITER_SIZE];
    size_t pos;
    uft_io_error_t last_error;
} uft_buf_writer_t;

/**
 * @brief Initialize buffered writer
 */
static inline uft_io_error_t uft_buf_writer_init(uft_buf_writer_t* w, FILE* fp) {
    if (!w || !fp) return UFT_IO_ERR_NULL_PTR;
    w->fp = fp;
    w->pos = 0;
    w->last_error = UFT_IO_OK;
    return UFT_IO_OK;
}

/**
 * @brief Flush buffer to file
 */
static inline uft_io_error_t uft_buf_writer_flush(uft_buf_writer_t* w) {
    if (!w) return UFT_IO_ERR_NULL_PTR;
    if (w->pos == 0) return UFT_IO_OK;
    
    size_t written = fwrite(w->buffer, 1, w->pos, w->fp);
    if (written != w->pos) {
        w->last_error = UFT_IO_ERR_WRITE_FAILED;
        return w->last_error;
    }
    w->pos = 0;
    return UFT_IO_OK;
}

/**
 * @brief Write single byte (auto-flush when full)
 */
static inline uft_io_error_t uft_buf_writer_u8(uft_buf_writer_t* w, uint8_t val) {
    if (!w) return UFT_IO_ERR_NULL_PTR;
    w->buffer[w->pos++] = val;
    if (w->pos >= UFT_BUF_WRITER_SIZE) {
        return uft_buf_writer_flush(w);
    }
    return UFT_IO_OK;
}

/**
 * @brief Write 16-bit value (little-endian)
 */
static inline uft_io_error_t uft_buf_writer_u16(uft_buf_writer_t* w, uint16_t val) {
    uft_io_error_t err = uft_buf_writer_u8(w, val & 0xFF);
    if (err != UFT_IO_OK) return err;
    return uft_buf_writer_u8(w, (val >> 8) & 0xFF);
}

/**
 * @brief Write 32-bit value (little-endian)
 */
static inline uft_io_error_t uft_buf_writer_u32(uft_buf_writer_t* w, uint32_t val) {
    uft_io_error_t err = uft_buf_writer_u16(w, val & 0xFFFF);
    if (err != UFT_IO_OK) return err;
    return uft_buf_writer_u16(w, (val >> 16) & 0xFFFF);
}

/**
 * @brief Write multiple bytes
 */
static inline uft_io_error_t uft_buf_writer_bytes(uft_buf_writer_t* w, 
                                                   const void* data, size_t len) {
    if (!w || !data) return UFT_IO_ERR_NULL_PTR;
    
    const uint8_t* src = (const uint8_t*)data;
    while (len > 0) {
        size_t space = UFT_BUF_WRITER_SIZE - w->pos;
        size_t copy = (len < space) ? len : space;
        
        memcpy(w->buffer + w->pos, src, copy);
        w->pos += copy;
        src += copy;
        len -= copy;
        
        if (w->pos >= UFT_BUF_WRITER_SIZE) {
            uft_io_error_t err = uft_buf_writer_flush(w);
            if (err != UFT_IO_OK) return err;
        }
    }
    return UFT_IO_OK;
}

// ============================================================================
// Buffered Reader - For efficient byte-by-byte reads
// ============================================================================

#define UFT_BUF_READER_SIZE 4096

typedef struct {
    FILE* fp;
    uint8_t buffer[UFT_BUF_READER_SIZE];
    size_t pos;         /* Current position in buffer */
    size_t valid;       /* Valid bytes in buffer */
    uft_io_error_t last_error;
} uft_buf_reader_t;

/**
 * @brief Initialize buffered reader
 */
static inline uft_io_error_t uft_buf_reader_init(uft_buf_reader_t* r, FILE* fp) {
    if (!r || !fp) return UFT_IO_ERR_NULL_PTR;
    r->fp = fp;
    r->pos = 0;
    r->valid = 0;
    r->last_error = UFT_IO_OK;
    return UFT_IO_OK;
}

/**
 * @brief Refill buffer from file
 */
static inline uft_io_error_t uft_buf_reader_refill(uft_buf_reader_t* r) {
    if (!r) return UFT_IO_ERR_NULL_PTR;
    
    r->valid = fread(r->buffer, 1, UFT_BUF_READER_SIZE, r->fp);
    r->pos = 0;
    
    if (r->valid == 0) {
        if (feof(r->fp)) return UFT_IO_ERR_EOF;
        return UFT_IO_ERR_READ_FAILED;
    }
    return UFT_IO_OK;
}

/**
 * @brief Read single byte (auto-refill when empty)
 */
static inline uft_io_error_t uft_buf_reader_u8(uft_buf_reader_t* r, uint8_t* val) {
    if (!r || !val) return UFT_IO_ERR_NULL_PTR;
    
    if (r->pos >= r->valid) {
        uft_io_error_t err = uft_buf_reader_refill(r);
        if (err != UFT_IO_OK) return err;
    }
    
    *val = r->buffer[r->pos++];
    return UFT_IO_OK;
}

/**
 * @brief Read 16-bit value (little-endian)
 */
static inline uft_io_error_t uft_buf_reader_u16(uft_buf_reader_t* r, uint16_t* val) {
    uint8_t lo, hi;
    uft_io_error_t err = uft_buf_reader_u8(r, &lo);
    if (err != UFT_IO_OK) return err;
    err = uft_buf_reader_u8(r, &hi);
    if (err != UFT_IO_OK) return err;
    *val = (uint16_t)lo | ((uint16_t)hi << 8);
    return UFT_IO_OK;
}

/**
 * @brief Read 32-bit value (little-endian)
 */
static inline uft_io_error_t uft_buf_reader_u32(uft_buf_reader_t* r, uint32_t* val) {
    uint16_t lo, hi;
    uft_io_error_t err = uft_buf_reader_u16(r, &lo);
    if (err != UFT_IO_OK) return err;
    err = uft_buf_reader_u16(r, &hi);
    if (err != UFT_IO_OK) return err;
    *val = (uint32_t)lo | ((uint32_t)hi << 16);
    return UFT_IO_OK;
}

/**
 * @brief Read multiple bytes
 */
static inline uft_io_error_t uft_buf_reader_bytes(uft_buf_reader_t* r, 
                                                   void* data, size_t len) {
    if (!r || !data) return UFT_IO_ERR_NULL_PTR;
    
    uint8_t* dst = (uint8_t*)data;
    while (len > 0) {
        if (r->pos >= r->valid) {
            uft_io_error_t err = uft_buf_reader_refill(r);
            if (err != UFT_IO_OK) return err;
        }
        
        size_t avail = r->valid - r->pos;
        size_t copy = (len < avail) ? len : avail;
        
        memcpy(dst, r->buffer + r->pos, copy);
        r->pos += copy;
        dst += copy;
        len -= copy;
    }
    return UFT_IO_OK;
}

#ifdef __cplusplus
}
#endif

#endif // UFT_SAFE_IO_H

/* =============================================================================
 * SAFE STRING FORMATTING (P0-SEC-002: sprintf→snprintf)
 * ============================================================================= */

/**
 * @brief Safe sprintf replacement
 * Usage: UFT_SNPRINTF(buf, "format %d", value);
 */
#define UFT_SNPRINTF(buf, ...) \
    snprintf((buf), sizeof(buf), __VA_ARGS__)

/**
 * @brief Safe sprintf append (for building strings incrementally)
 * Usage: UFT_SNPRINTF_APPEND(buf, "more %s", str);
 */
#define UFT_SNPRINTF_APPEND(buf, ...) \
    do { \
        size_t _len = strlen(buf); \
        if (_len < sizeof(buf) - 1) { \
            snprintf((buf) + _len, sizeof(buf) - _len, __VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief Safe sprintf with explicit size (for pointer buffers)
 * Usage: uft_snprintf_safe(ptr, size, "format %d", val);
 */
#define uft_snprintf_safe(buf, size, ...) \
    snprintf((buf), (size), __VA_ARGS__)

/**
 * @brief Safe sprintf append with explicit size
 */
static inline int uft_snprintf_append(char* buf, size_t bufsize, const char* fmt, ...) {
    size_t len = strlen(buf);
    if (len >= bufsize - 1) return 0;
    
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf + len, bufsize - len, fmt, args);
    va_end(args);
    return ret;
}


/* =============================================================================
 * SAFE STRING COPY/CONCAT (P0-SEC-003: strcpy/strcat→strncpy/strncat)
 * ============================================================================= */

/**
 * @brief Safe strcpy replacement for arrays
 * Always null-terminates, prevents buffer overflow
 * Usage: UFT_STRCPY(dest_array, src);
 */
#define UFT_STRCPY(dest, src) \
    do { \
        strncpy((dest), (src), sizeof(dest) - 1); \
        (dest)[sizeof(dest) - 1] = '\0'; \
    } while(0)

/**
 * @brief Safe strcat replacement for arrays
 * Always null-terminates, prevents buffer overflow
 * Usage: UFT_STRCAT(dest_array, src);
 */
#define UFT_STRCAT(dest, src) \
    do { \
        size_t _dlen = strlen(dest); \
        if (_dlen < sizeof(dest) - 1) { \
            strncat((dest), (src), sizeof(dest) - _dlen - 1); \
        } \
    } while(0)

/**
 * @brief Safe strcpy with explicit size (for pointer buffers)
 */
static inline void uft_strcpy_safe(char* dest, size_t dest_size, const char* src) {
    if (!dest || dest_size == 0) return;
    if (!src) { dest[0] = '\0'; return; }
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/**
 * @brief Safe strcat with explicit size (for pointer buffers)
 */
static inline void uft_strcat_safe(char* dest, size_t dest_size, const char* src) {
    if (!dest || !src || dest_size == 0) return;
    size_t dlen = strlen(dest);
    if (dlen >= dest_size - 1) return;
    strncat(dest, src, dest_size - dlen - 1);
}

/**
 * @brief Safe strcpy for struct members with known size
 * Usage: UFT_STRCPY_N(dest, src, sizeof(struct.member));
 */
#define UFT_STRCPY_N(dest, src, size) \
    do { \
        strncpy((dest), (src), (size) - 1); \
        (dest)[(size) - 1] = '\0'; \
    } while(0)

/**
 * @brief Safe strcat for struct members with known size
 */
#define UFT_STRCAT_N(dest, src, size) \
    do { \
        size_t _dlen = strlen(dest); \
        if (_dlen < (size) - 1) { \
            strncat((dest), (src), (size) - _dlen - 1); \
        } \
    } while(0)


/**
 * @brief Macro: fread with goto on failure
 */
#define UFT_FREAD_OR_GOTO(ptr, size, count, stream, label) \
    do { \
        size_t _exp = (size_t)(size) * (size_t)(count); \
        if (fread((ptr), 1, _exp, (stream)) != _exp) goto label; \
    } while (0)

/**
 * @brief Macro: fwrite with goto on failure
 */
#define UFT_FWRITE_OR_GOTO(ptr, size, count, stream, label) \
    do { \
        size_t _exp = (size_t)(size) * (size_t)(count); \
        if (fwrite((ptr), 1, _exp, (stream)) != _exp) goto label; \
    } while (0)

/**
 * @brief Macro: fseek with goto on failure
 */
#define UFT_FSEEK_OR_GOTO(stream, offset, whence, label) \
    do { \
        if (fseek((stream), (offset), (whence)) != 0) goto label; \
    } while (0)

/**
 * @brief Macro: fread with return on failure
 */
#define UFT_FREAD_OR_RETURN(ptr, size, count, stream, retval) \
    do { \
        size_t _exp = (size_t)(size) * (size_t)(count); \
        if (fread((ptr), 1, _exp, (stream)) != _exp) return (retval); \
    } while (0)

/**
 * @brief Macro: fwrite with return on failure
 */
#define UFT_FWRITE_OR_RETURN(ptr, size, count, stream, retval) \
    do { \
        size_t _exp = (size_t)(size) * (size_t)(count); \
        if (fwrite((ptr), 1, _exp, (stream)) != _exp) return (retval); \
    } while (0)

/**
 * @brief Macro: fseek with return on failure
 */
#define UFT_FSEEK_OR_RETURN(stream, offset, whence, retval) \
    do { \
        if (fseek((stream), (offset), (whence)) != 0) return (retval); \
    } while (0)

/**
 * @brief Read exact number of bytes or fail
 * @return Number of bytes read, or -1 on error
 */
static inline ssize_t uft_read_exact(FILE* fp, void* buf, size_t size) {
    if (!fp || !buf || size == 0) return -1;
    size_t total = 0;
    uint8_t* p = (uint8_t*)buf;
    while (total < size) {
        size_t n = fread(p + total, 1, size - total, fp);
        if (n == 0) {
            if (feof(fp)) return (ssize_t)total;
            return -1;
        }
        total += n;
    }
    return (ssize_t)total;
}

/**
 * @brief Write exact number of bytes or fail
 * @return Number of bytes written, or -1 on error
 */
static inline ssize_t uft_write_exact(FILE* fp, const void* buf, size_t size) {
    if (!fp || !buf || size == 0) return -1;
    size_t total = 0;
    const uint8_t* p = (const uint8_t*)buf;
    while (total < size) {
        size_t n = fwrite(p + total, 1, size - total, fp);
        if (n == 0) return -1;
        total += n;
    }
    return (ssize_t)total;
}


/* =============================================================================
 * P1-ARR-001: ARRAY BOUNDS CHECKING MACROS
 * ============================================================================= */

/**
 * @brief Check if index is within array bounds
 * @return true if index < size, false otherwise
 */
#define UFT_INDEX_VALID(index, size) ((size_t)(index) < (size_t)(size))

/**
 * @brief Check if index + offset is within array bounds
 * @return true if index + offset < size, false otherwise
 */
#define UFT_OFFSET_VALID(index, offset, size) \
    ((size_t)(index) + (size_t)(offset) < (size_t)(size))

/**
 * @brief Safe array access with bounds check and goto on failure
 */
#define UFT_ARRAY_GET_OR_GOTO(arr, index, size, result, fail_label) \
    do { \
        if (!UFT_INDEX_VALID(index, size)) goto fail_label; \
        (result) = (arr)[index]; \
    } while (0)

/**
 * @brief Safe array access with bounds check and return on failure
 */
#define UFT_ARRAY_GET_OR_RETURN(arr, index, size, result, retval) \
    do { \
        if (!UFT_INDEX_VALID(index, size)) return (retval); \
        (result) = (arr)[index]; \
    } while (0)

/**
 * @brief Check range [start, start+len) is within buffer [0, size)
 */
static inline bool uft_range_valid(size_t start, size_t len, size_t size) {
    if (start > size) return false;
    if (len > size - start) return false;  /* Overflow-safe check */
    return true;
}

/**
 * @brief Safe buffer read with bounds checking
 * @return Number of bytes read, or -1 on bounds error
 */
static inline ssize_t uft_safe_memcpy(void* dst, const void* src, 
                                       size_t offset, size_t len, size_t src_size) {
    if (!dst || !src) return -1;
    if (!uft_range_valid(offset, len, src_size)) return -1;
    memcpy(dst, (const uint8_t*)src + offset, len);
    return (ssize_t)len;
}

/**
 * @brief Read uint16 little-endian with bounds check
 */
static inline int uft_read_u16le(const uint8_t* data, size_t offset, 
                                  size_t size, uint16_t* out) {
    if (!data || !out || offset + 2 > size) return -1;
    *out = (uint16_t)(data[offset] | (data[offset + 1] << 8));
    return 0;
}

/**
 * @brief Read uint32 little-endian with bounds check
 */
static inline int uft_read_u32le(const uint8_t* data, size_t offset, 
                                  size_t size, uint32_t* out) {
    if (!data || !out || offset + 4 > size) return -1;
    *out = (uint32_t)(data[offset] | 
                      (data[offset + 1] << 8) | 
                      (data[offset + 2] << 16) | 
                      (data[offset + 3] << 24));
    return 0;
}

/**
 * @brief Read uint16 big-endian with bounds check
 */
static inline int uft_read_u16be(const uint8_t* data, size_t offset, 
                                  size_t size, uint16_t* out) {
    if (!data || !out || offset + 2 > size) return -1;
    *out = (uint16_t)((data[offset] << 8) | data[offset + 1]);
    return 0;
}

/**
 * @brief Read uint32 big-endian with bounds check
 */
static inline int uft_read_u32be(const uint8_t* data, size_t offset, 
                                  size_t size, uint32_t* out) {
    if (!data || !out || offset + 4 > size) return -1;
    *out = (uint32_t)((data[offset] << 24) | 
                      (data[offset + 1] << 16) | 
                      (data[offset + 2] << 8) | 
                      data[offset + 3]);
    return 0;
}

