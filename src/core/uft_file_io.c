/**
 * @file uft_file_io.c
 * @brief Safe File I/O Implementation
 * 
 * Provides secure file operations with:
 * - Path traversal protection
 * - Comprehensive error handling
 * - EOF/ferror checking
 * 
 * @version 2.0.0 - Added security hardening
 */

#include "uft_file_io.h"
#include "uft/core/uft_path_safe.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <io.h>
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

/* Global flag for path security checks (can be disabled for trusted paths) */
static int g_uft_path_security_enabled = 1;

void uft_file_set_security(int enabled) {
    g_uft_path_security_enabled = enabled;
}

int uft_file_get_security(void) {
    return g_uft_path_security_enabled;
}

uft_error_t uft_file_open(const char *path, const char *mode, FILE **out_file) {
    if (!path || !mode || !out_file) return UFT_ERROR_NULL_POINTER;
    
    *out_file = NULL;
    
    /* Security check: validate path if enabled */
    if (g_uft_path_security_enabled) {
        /* Check for directory traversal attempts */
        if (strstr(path, "..") != NULL) {
            return UFT_ERROR_SECURITY;
        }
    }
    
    FILE *f = fopen(path, mode);
    if (!f) {
        switch (errno) {
            case ENOENT: return UFT_ERROR_FILE_NOT_FOUND;
            case EACCES: return UFT_ERROR_PERMISSION_DENIED;
            default: return UFT_ERROR_FILE_OPEN;
        }
    }
    
    *out_file = f;
    return UFT_OK;
}

uft_error_t uft_file_open_safe(const char *path, const char *mode, 
                               const char *base_dir, FILE **out_file) {
    if (!path || !mode || !out_file) return UFT_ERROR_NULL_POINTER;
    
    *out_file = NULL;
    
    /* If base_dir specified, verify path is within it */
    if (base_dir) {
        if (!uft_path_within_base(path, base_dir)) {
            return UFT_ERROR_SECURITY;
        }
    } else {
        /* No base_dir - at least check for traversal */
        if (!uft_path_is_safe(path)) {
            return UFT_ERROR_SECURITY;
        }
    }
    
    FILE *f = fopen(path, mode);
    if (!f) {
        switch (errno) {
            case ENOENT: return UFT_ERROR_FILE_NOT_FOUND;
            case EACCES: return UFT_ERROR_PERMISSION_DENIED;
            default: return UFT_ERROR_FILE_OPEN;
        }
    }
    
    *out_file = f;
    return UFT_OK;
}

void uft_file_close(FILE *f) {
    if (f) fclose(f);
}

uft_error_t uft_file_read(FILE *f, void *buf, size_t size) {
    if (!f || !buf) return UFT_ERROR_NULL_POINTER;
    if (size == 0) return UFT_OK;
    
    size_t n = fread(buf, 1, size, f);
    if (n != size) {
        if (feof(f)) return UFT_ERROR_FILE_TRUNCATED;
        return UFT_ERROR_FILE_READ;
    }
    
    return UFT_OK;
}

uft_error_t uft_file_read_partial(FILE *f, void *buf, size_t size, size_t *out_read) {
    if (!f || !buf) return UFT_ERROR_NULL_POINTER;
    if (out_read) *out_read = 0;
    if (size == 0) return UFT_OK;
    
    size_t n = fread(buf, 1, size, f);
    if (out_read) *out_read = n;
    
    if (n == 0 && ferror(f)) {
        return UFT_ERROR_FILE_READ;
    }
    
    return UFT_OK;
}

uft_error_t uft_file_read_all(const char *path, uint8_t **out_data, size_t *out_size) {
    if (!path || !out_data || !out_size) return UFT_ERROR_NULL_POINTER;
    
    *out_data = NULL;
    *out_size = 0;
    
    FILE *f = NULL;
    uft_error_t err = uft_file_open(path, "rb", &f);
    if (err != UFT_OK) return err;
    
    /* Get file size */
    size_t file_size;
    err = uft_file_size(f, &file_size);
    if (err != UFT_OK) {
        uft_file_close(f);
        return err;
    }
    
    if (file_size == 0) {
        uft_file_close(f);
        *out_data = NULL;
        *out_size = 0;
        return UFT_OK;
    }
    
    /* Allocate buffer */
    uint8_t *data = malloc(file_size);
    if (!data) {
        uft_file_close(f);
        return UFT_ERROR_OUT_OF_MEMORY;
    }
    
    /* Read file */
    err = uft_file_read(f, data, file_size);
    uft_file_close(f);
    
    if (err != UFT_OK) {
        free(data);
        return err;
    }
    
    *out_data = data;
    *out_size = file_size;
    return UFT_OK;
}

uft_error_t uft_file_write(FILE *f, const void *buf, size_t size) {
    if (!f || !buf) return UFT_ERROR_NULL_POINTER;
    if (size == 0) return UFT_OK;
    
    size_t n = fwrite(buf, 1, size, f);
    if (n != size) {
        /* Check for specific error condition */
        if (ferror(f)) {
            return UFT_ERROR_FILE_WRITE;
        }
        /* Partial write without error flag - disk full? */
        return UFT_ERROR_FILE_WRITE;
    }
    
    /* Flush to detect write errors immediately */
    if (fflush(f) != 0) {
        return UFT_ERROR_FILE_WRITE;
    }
    
    return UFT_OK;
}

uft_error_t uft_file_write_all(const char *path, const void *data, size_t size) {
    if (!path || (!data && size > 0)) return UFT_ERROR_NULL_POINTER;
    
    FILE *f = NULL;
    uft_error_t err = uft_file_open(path, "wb", &f);
    if (err != UFT_OK) return err;
    
    if (size > 0) {
        err = uft_file_write(f, data, size);
    }
    
    uft_file_close(f);
    return err;
}

uft_error_t uft_file_seek(FILE *f, long offset, int whence) {
    if (!f) return UFT_ERROR_NULL_POINTER;
    
    if (fseek(f, offset, whence) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    return UFT_OK;
}

uft_error_t uft_file_tell(FILE *f, long *out_pos) {
    if (!f || !out_pos) return UFT_ERROR_NULL_POINTER;
    
    long pos = ftell(f);
    if (pos < 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    *out_pos = pos;
    return UFT_OK;
}

uft_error_t uft_file_size(FILE *f, size_t *out_size) {
    if (!f || !out_size) return UFT_ERROR_NULL_POINTER;
    
    /* Save current position */
    long saved_pos = ftell(f);
    if (saved_pos < 0) return UFT_ERROR_FILE_SEEK;
    
    /* Seek to end */
    if (fseek(f, 0, SEEK_END) != 0) return UFT_ERROR_FILE_SEEK;
    
    /* Get size */
    long size = ftell(f);
    if (size < 0) return UFT_ERROR_FILE_SEEK;
    
    /* Restore position */
    if (fseek(f, saved_pos, SEEK_SET) != 0) return UFT_ERROR_FILE_SEEK;
    
    *out_size = (size_t)size;
    return UFT_OK;
}

int uft_file_exists(const char *path) {
    if (!path) return 0;
    return access(path, F_OK) == 0;
}

uft_error_t uft_file_size_path(const char *path, size_t *out_size) {
    if (!path || !out_size) return UFT_ERROR_NULL_POINTER;
    
    FILE *f = NULL;
    uft_error_t err = uft_file_open(path, "rb", &f);
    if (err != UFT_OK) return err;
    
    err = uft_file_size(f, out_size);
    uft_file_close(f);
    return err;
}
