/**
 * @file uft_safe_io.h
 * @brief Safe I/O Wrappers with Error Checking (P1-IO-001)
 * 
 * Provides checked versions of standard I/O functions.
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_SAFE_IO_H
#define UFT_SAFE_IO_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>

/* ssize_t portability */
#ifdef _WIN32
    #include <BaseTsd.h>
    #ifndef _SSIZE_T_DEFINED
    #define _SSIZE_T_DEFINED
    typedef SSIZE_T ssize_t;
    #endif
#else
    #include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Error Reporting
 *===========================================================================*/

/** Last I/O error message (thread-local where supported) */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
    #include <threads.h>
    #include <stdlib.h>
    extern _Thread_local char uft_io_last_error[256];
#else
    #include <stdlib.h>
    extern char uft_io_last_error[256];
#endif

/** Set I/O error message */
static inline void uft_io_set_error(const char *func, const char *msg) {
    snprintf(uft_io_last_error, sizeof(uft_io_last_error), 
             "%s: %s (errno=%d: %s)", func, msg, errno, strerror(errno));
}

/** Get last I/O error message */
static inline const char* uft_io_get_error(void) {
    return uft_io_last_error;
}

/*===========================================================================
 * Safe Read Functions
 *===========================================================================*/

/**
 * @brief Safe fread with full error checking
 * @return true on success, false on error (partial read or EOF)
 */
static inline bool uft_fread(void *ptr, size_t size, size_t count, FILE *fp) {
    if (!ptr || !fp || size == 0 || count == 0) {
        uft_io_set_error("uft_fread", "invalid arguments");
        return false;
    }
    
    size_t n = fread(ptr, size, count, fp);
    if (n != count) {
        if (feof(fp)) {
            uft_io_set_error("uft_fread", "unexpected end of file");
        } else if (ferror(fp)) {
            uft_io_set_error("uft_fread", "read error");
        } else {
            uft_io_set_error("uft_fread", "partial read");
        }
        return false;
    }
    return true;
}

/**
 * @brief Safe read of exact byte count
 * @return Number of bytes read, or -1 on error
 */
static inline ssize_t uft_fread_exact(void *ptr, size_t bytes, FILE *fp) {
    if (!ptr || !fp || bytes == 0) return 0;
    
    size_t total = 0;
    uint8_t *p = (uint8_t *)ptr;
    
    while (total < bytes) {
        size_t n = fread(p + total, 1, bytes - total, fp);
        if (n == 0) {
            if (feof(fp)) {
                uft_io_set_error("uft_fread_exact", "unexpected EOF");
            } else {
                uft_io_set_error("uft_fread_exact", "read error");
            }
            return (ssize_t)total;  /* Return partial count */
        }
        total += n;
    }
    return (ssize_t)total;
}

/**
 * @brief Read uint8_t
 */
static inline bool uft_read_u8(FILE *fp, uint8_t *val) {
    return uft_fread(val, 1, 1, fp);
}

/**
 * @brief Read uint16_t little-endian
 */
static inline bool uft_read_u16_le(FILE *fp, uint16_t *val) {
    uint8_t buf[2];
    if (!uft_fread(buf, 1, 2, fp)) return false;
    *val = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    return true;
}

/**
 * @brief Read uint16_t big-endian
 */
static inline bool uft_read_u16_be(FILE *fp, uint16_t *val) {
    uint8_t buf[2];
    if (!uft_fread(buf, 1, 2, fp)) return false;
    *val = ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
    return true;
}

/**
 * @brief Read uint32_t little-endian
 */
static inline bool uft_read_u32_le(FILE *fp, uint32_t *val) {
    uint8_t buf[4];
    if (!uft_fread(buf, 1, 4, fp)) return false;
    *val = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    return true;
}

/**
 * @brief Read uint32_t big-endian
 */
static inline bool uft_read_u32_be(FILE *fp, uint32_t *val) {
    uint8_t buf[4];
    if (!uft_fread(buf, 1, 4, fp)) return false;
    *val = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
    return true;
}

/**
 * @brief Read uint64_t little-endian
 */
static inline bool uft_read_u64_le(FILE *fp, uint64_t *val) {
    uint8_t buf[8];
    if (!uft_fread(buf, 1, 8, fp)) return false;
    *val = (uint64_t)buf[0] | ((uint64_t)buf[1] << 8) |
           ((uint64_t)buf[2] << 16) | ((uint64_t)buf[3] << 24) |
           ((uint64_t)buf[4] << 32) | ((uint64_t)buf[5] << 40) |
           ((uint64_t)buf[6] << 48) | ((uint64_t)buf[7] << 56);
    return true;
}

/*===========================================================================
 * Safe Write Functions
 *===========================================================================*/

/**
 * @brief Safe fwrite with full error checking
 * @return true on success, false on error
 */
static inline bool uft_fwrite(const void *ptr, size_t size, size_t count, FILE *fp) {
    if (!ptr || !fp || size == 0 || count == 0) {
        uft_io_set_error("uft_fwrite", "invalid arguments");
        return false;
    }
    
    size_t n = fwrite(ptr, size, count, fp);
    if (n != count) {
        uft_io_set_error("uft_fwrite", "write error");
        return false;
    }
    return true;
}

/**
 * @brief Write uint8_t
 */
static inline bool uft_write_u8(FILE *fp, uint8_t val) {
    return uft_fwrite(&val, 1, 1, fp);
}

/**
 * @brief Write uint16_t little-endian
 */
static inline bool uft_write_u16_le(FILE *fp, uint16_t val) {
    uint8_t buf[2] = { (uint8_t)(val & 0xFF), (uint8_t)(val >> 8) };
    return uft_fwrite(buf, 1, 2, fp);
}

/**
 * @brief Write uint16_t big-endian
 */
static inline bool uft_write_u16_be(FILE *fp, uint16_t val) {
    uint8_t buf[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
    return uft_fwrite(buf, 1, 2, fp);
}

/**
 * @brief Write uint32_t little-endian
 */
static inline bool uft_write_u32_le(FILE *fp, uint32_t val) {
    uint8_t buf[4] = {
        (uint8_t)(val & 0xFF), (uint8_t)((val >> 8) & 0xFF),
        (uint8_t)((val >> 16) & 0xFF), (uint8_t)(val >> 24)
    };
    return uft_fwrite(buf, 1, 4, fp);
}

/**
 * @brief Write uint32_t big-endian
 */
static inline bool uft_write_u32_be(FILE *fp, uint32_t val) {
    uint8_t buf[4] = {
        (uint8_t)(val >> 24), (uint8_t)((val >> 16) & 0xFF),
        (uint8_t)((val >> 8) & 0xFF), (uint8_t)(val & 0xFF)
    };
    return uft_fwrite(buf, 1, 4, fp);
}

/*===========================================================================
 * Safe Seek/Tell Functions
 *===========================================================================*/

/**
 * @brief Safe fseek with error checking
 */
static inline bool uft_fseek(FILE *fp, long offset, int whence) {
    if (!fp) {
        uft_io_set_error("uft_fseek", "null file pointer");
        return false;
    }
    
    if (fseek(fp, offset, whence) != 0) {
        uft_io_set_error("uft_fseek", "seek failed");
        return false;
    }
    return true;
}

/**
 * @brief Safe ftell with error checking
 */
static inline long uft_ftell(FILE *fp) {
    if (!fp) {
        uft_io_set_error("uft_ftell", "null file pointer");
        return -1;
    }
    
    long pos = ftell(fp);
    if (pos < 0) {
        uft_io_set_error("uft_ftell", "tell failed");
    }
    return pos;
}

/**
 * @brief Get file size
 */
static inline long uft_file_size(FILE *fp) {
    if (!fp) return -1;
    
    long cur = ftell(fp);
    if (cur < 0) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) return -1;
    long size = ftell(fp);
    
    if (fseek(fp, cur, SEEK_SET) != 0) return -1;
    return size;
}

/*===========================================================================
 * Safe Open/Close Functions
 *===========================================================================*/

/**
 * @brief Safe fopen with error message
 */
static inline FILE* uft_fopen(const char *path, const char *mode) {
    if (!path || !mode) {
        uft_io_set_error("uft_fopen", "null arguments");
        return NULL;
    }
    
    FILE *fp = fopen(path, mode);
    if (!fp) {
        uft_io_set_error("uft_fopen", "failed to open file");
    }
    return fp;
}

/**
 * @brief Safe fclose with error checking
 * @return true on success, false on error (still closes the file)
 */
static inline bool uft_fclose(FILE *fp) {
    if (!fp) return true;
    
    if (fclose(fp) != 0) {
        uft_io_set_error("uft_fclose", "close failed");
        return false;
    }
    return true;
}

/*===========================================================================
 * Buffer Read Functions
 *===========================================================================*/

/**
 * @brief Read entire file into buffer
 * @param path File path
 * @param size_out Output: file size
 * @return Allocated buffer (caller must free), or NULL on error
 */
static inline uint8_t* uft_read_file(const char *path, size_t *size_out) {
    FILE *fp = uft_fopen(path, "rb");
    if (!fp) return NULL;
    
    long size = uft_file_size(fp);
    if (size < 0) {
        uft_fclose(fp);
        return NULL;
    }
    
    uint8_t *buf = (uint8_t *)malloc((size_t)size);
    if (!buf) {
        uft_io_set_error("uft_read_file", "out of memory");
        uft_fclose(fp);
        return NULL;
    }
    
    if (!uft_fread(buf, 1, (size_t)size, fp)) {
        free(buf);
        uft_fclose(fp);
        return NULL;
    }
    
    uft_fclose(fp);
    
    if (size_out) *size_out = (size_t)size;
    return buf;
}

/**
 * @brief Write buffer to file
 */
static inline bool uft_write_file(const char *path, const void *data, size_t size) {
    FILE *fp = uft_fopen(path, "wb");
    if (!fp) return false;
    
    bool ok = uft_fwrite(data, 1, size, fp);
    uft_fclose(fp);
    return ok;
}

/*===========================================================================
 * Convenience Macros
 *===========================================================================*/

/** Check fread result and goto cleanup on error */
#define UFT_FREAD_OR_FAIL(ptr, size, count, fp, label) \
    do { \
        if (fread((ptr), (size), (count), (fp)) != (count)) { \
            goto label; \
        } \
    } while(0)

/** Check fwrite result and goto cleanup on error */
#define UFT_FWRITE_OR_FAIL(ptr, size, count, fp, label) \
    do { \
        if (fwrite((ptr), (size), (count), (fp)) != (count)) { \
            goto label; \
        } \
    } while(0)

/** Check fseek result and goto cleanup on error */
#define UFT_FSEEK_OR_FAIL(fp, offset, whence, label) \
    do { \
        if (fseek((fp), (offset), (whence)) != 0) { \
            goto label; \
        } \
    } while(0)

/** Read with size check and return on error */
#define UFT_READ_CHECK(ptr, size, count, fp) \
    do { \
        if (fread((ptr), (size), (count), (fp)) != (count)) { \
            return -1; \
        } \
    } while(0)

/** Write with size check and return on error */
#define UFT_WRITE_CHECK(ptr, size, count, fp) \
    do { \
        if (fwrite((ptr), (size), (count), (fp)) != (count)) { \
            return -1; \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* UFT_SAFE_IO_H */
