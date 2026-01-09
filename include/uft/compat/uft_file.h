/**
 * @file uft_file.h
 * @brief Safe file I/O wrappers with error handling
 * 
 * All functions return clear error status and handle NULL gracefully.
 */
#ifndef UFT_FILE_H
#define UFT_FILE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Safe fopen with error message
 * @return FILE* or NULL with errno set
 */
static inline FILE* uft_fopen(const char* path, const char* mode) {
    if (!path || !mode) {
        errno = EINVAL;
        return NULL;
    }
    return fopen(path, mode);
}

/**
 * @brief Safe fclose (handles NULL)
 */
static inline int uft_fclose(FILE* fp) {
    if (!fp) return 0;
    return fclose(fp);
}

/**
 * @brief Safe fread with complete error handling
 * @return Number of items read, 0 on error
 */
static inline size_t uft_fread(void* ptr, size_t size, size_t count, FILE* fp) {
    if (!ptr || !fp || size == 0 || count == 0) return 0;
    return fread(ptr, size, count, fp);
}

/**
 * @brief Safe fread that reads exactly count bytes or fails
 * @return true if all bytes read, false otherwise
 */
static inline int uft_fread_exact(void* ptr, size_t size, FILE* fp) {
    if (!ptr || !fp || size == 0) return 0;
    return fread(ptr, 1, size, fp) == size;
}

/**
 * @brief Safe fwrite with complete error handling
 * @return Number of items written, 0 on error
 */
static inline size_t uft_fwrite(const void* ptr, size_t size, size_t count, FILE* fp) {
    if (!ptr || !fp || size == 0 || count == 0) return 0;
    return fwrite(ptr, size, count, fp);
}

/**
 * @brief Safe fwrite that writes exactly count bytes or fails
 * @return true if all bytes written, false otherwise
 */
static inline int uft_fwrite_exact(const void* ptr, size_t size, FILE* fp) {
    if (!ptr || !fp || size == 0) return 0;
    return fwrite(ptr, 1, size, fp) == size;
}

/**
 * @brief Safe fseek
 * @return 0 on success, -1 on error
 */
static inline int uft_fseek(FILE* fp, long offset, int whence) {
    if (!fp) return -1;
    return fseek(fp, offset, whence);
}

/**
 * @brief Safe ftell
 * @return Position or -1 on error
 */
static inline long uft_ftell(FILE* fp) {
    if (!fp) return -1;
    return ftell(fp);
}

/**
 * @brief Seek with bounds validation
 * @return 0 on success, -1 on error
 */
static inline int uft_fseek_safe(FILE* fp, long offset, int whence, long file_size) {
    if (!fp) return -1;
    
    /* Calculate target position */
    long target;
    if (whence == SEEK_SET) {
        target = offset;
    } else if (whence == SEEK_CUR) {
        long cur = ftell(fp);
        if (cur < 0) return -1;
        target = cur + offset;
    } else if (whence == SEEK_END) {
        target = file_size + offset;
    } else {
        return -1;  /* Invalid whence */
    }
    
    /* Bounds check */
    if (target < 0 || (file_size > 0 && target > file_size)) {
        return -1;
    }
    
    return fseek(fp, offset, whence);
}

/**
 * @brief Macro for checked fseek with early return
 * Usage: UFT_FSEEK_CHECK(fp, offset, whence, error_return);
 */
#define UFT_FSEEK_CHECK(fp, offset, whence, error_ret) \
    do { \
        if (fseek((fp), (offset), (whence)) != 0) { \
            return (error_ret); \
        } \
    } while(0)

/**
 * @brief Macro for checked ftell with early return
 * Usage: long pos; UFT_FTELL_CHECK(fp, pos, error_return);
 */
#define UFT_FTELL_CHECK(fp, var, error_ret) \
    do { \
        (var) = ftell(fp); \
        if ((var) < 0) { \
            return (error_ret); \
        } \
    } while(0)

/**
 * @brief Get file size safely
 * @return Size or -1 on error, restores original position
 */
static inline long uft_fsize_safe(FILE* fp) {
    if (!fp) return -1;
    
    long saved = ftell(fp);
    if (saved < 0) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) {
        (void)fseek(fp, saved, SEEK_SET);
        return -1;
    }
    
    long size = ftell(fp);
    if (fseek(fp, saved, SEEK_SET) != 0) {
        return -1;  /* Position corrupted */
    }
    
    return size;
}

/**
 * @brief Get file size
 * @return Size in bytes or -1 on error
 */
static inline long uft_fsize(FILE* fp) {
    if (!fp) return -1;
    long pos = ftell(fp);
    if (pos < 0) return -1;
    if (fseek(fp, 0, SEEK_END) != 0) return -1;
    long size = ftell(fp);
    if (fseek(fp, pos, SEEK_SET) != 0) return -1;
    return size;
}

/**
 * @brief Load entire file into memory
 * @param path File path
 * @param size_out Output: file size (can be NULL)
 * @return Allocated buffer or NULL on error (caller must free)
 */
static inline void* uft_load_file(const char* path, size_t* size_out) {
    if (!path) return NULL;
    
    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;
    
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    
    long size = ftell(fp);
    if (size < 0 || size > 1024 * 1024 * 1024) {  /* 1GB limit */
        fclose(fp);
        return NULL;
    }
    
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    
    void* data = malloc((size_t)size);
    if (!data) {
        fclose(fp);
        return NULL;
    }
    
    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return NULL;
    }
    
    fclose(fp);
    
    if (size_out) *size_out = (size_t)size;
    return data;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FILE_H */
