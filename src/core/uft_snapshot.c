/**
 * @file uft_snapshot.c
 * @brief Recovery Snapshot System Implementation
 * @version 1.0.0
 */

#include "uft/core/uft_snapshot.h"
#include "uft/core/uft_sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define PATH_SEP '\\'
#else
#include <sys/time.h>
#define PATH_SEP '/'
#endif

/* Get current time in milliseconds */
static uint64_t now_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return t / 10000 - 11644473600000ULL;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
#endif
}

/* Read entire file into buffer */
static uint8_t *slurp_file(const char *path, size_t *out_len) {
    *out_len = 0;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    
    uint8_t *buf = (uint8_t *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    
    if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        free(buf);
        return NULL;
    }
    
    fclose(f);
    *out_len = (size_t)sz;
    return buf;
}

uft_error_t uft_snapshot_create(const char *snapshot_dir,
                                const char *prefix,
                                const uint8_t *buf,
                                size_t len,
                                const uft_snapshot_opts_t *opts,
                                uft_snapshot_t *out) {
    if (!snapshot_dir || !prefix || !buf || !out) {
        return UFT_ERR_INVALID_ARG;
    }
    
    memset(out, 0, sizeof(*out));
    
    /* Generate unique filename with timestamp */
    uint64_t ts = now_ms();
    snprintf(out->path, sizeof(out->path), "%s%c%s_%llu.snap",
             snapshot_dir, PATH_SEP, prefix, (unsigned long long)ts);
    
    /* Calculate SHA-256 hash */
    uft_sha256(buf, len, out->sha256);
    
    /* Write snapshot file */
    FILE *f = fopen(out->path, "wb");
    if (!f) {
        return UFT_ERR_IO;
    }
    
    if (len > 0 && fwrite(buf, 1, len, f) != len) {
        fclose(f);
        return UFT_ERR_IO;
    }
    
    if (fclose(f) != 0) {
        return UFT_ERR_IO;
    }
    
    out->size_bytes = (uint64_t)len;
    out->timestamp_ms = ts;
    
    /* Verify if requested */
    if (opts && opts->verify_after_write) {
        uft_error_t err = uft_snapshot_verify(out);
        if (err != UFT_SUCCESS) {
            return err;
        }
    }
    
    return UFT_SUCCESS;
}

uft_error_t uft_snapshot_create_from_file(const char *snapshot_dir,
                                          const char *prefix,
                                          const char *src_path,
                                          const uft_snapshot_opts_t *opts,
                                          uft_snapshot_t *out) {
    if (!snapshot_dir || !prefix || !src_path || !out) {
        return UFT_ERR_INVALID_ARG;
    }
    
    size_t len = 0;
    uint8_t *buf = slurp_file(src_path, &len);
    if (!buf && len == 0) {
        /* Could be empty file - check if file exists */
        FILE *f = fopen(src_path, "rb");
        if (!f) return UFT_ERR_FILE_NOT_FOUND;
        fclose(f);
        /* Empty file is valid */
        buf = (uint8_t *)malloc(1);
        if (!buf) return UFT_ERR_MEMORY;
        len = 0;
    }
    
    uft_error_t err = uft_snapshot_create(snapshot_dir, prefix, buf, len, opts, out);
    free(buf);
    return err;
}

uft_error_t uft_snapshot_verify(const uft_snapshot_t *snapshot) {
    if (!snapshot || !snapshot->path[0]) {
        return UFT_ERR_INVALID_ARG;
    }
    
    size_t len = 0;
    uint8_t *buf = slurp_file(snapshot->path, &len);
    if (!buf && len == 0) {
        /* Check for empty file */
        FILE *f = fopen(snapshot->path, "rb");
        if (!f) return UFT_ERR_FILE_NOT_FOUND;
        fclose(f);
        if (snapshot->size_bytes != 0) {
            return UFT_ERR_CORRUPTED;
        }
        /* Empty file - verify hash of empty data */
        uint8_t hash[32];
        uft_sha256(NULL, 0, hash);
        if (uft_sha256_compare(hash, snapshot->sha256) != 0) {
            return UFT_ERR_CRC;
        }
        return UFT_SUCCESS;
    }
    
    /* Check size */
    if ((uint64_t)len != snapshot->size_bytes) {
        free(buf);
        return UFT_ERR_CORRUPTED;
    }
    
    /* Verify hash */
    uint8_t hash[32];
    uft_sha256(buf, len, hash);
    free(buf);
    
    if (uft_sha256_compare(hash, snapshot->sha256) != 0) {
        return UFT_ERR_CRC;
    }
    
    return UFT_SUCCESS;
}

uft_error_t uft_snapshot_restore(const uft_snapshot_t *snapshot,
                                 uint8_t *buf,
                                 size_t buf_size) {
    if (!snapshot || !buf) {
        return UFT_ERR_INVALID_ARG;
    }
    
    if (buf_size < snapshot->size_bytes) {
        return UFT_ERR_BUFFER_TOO_SMALL;
    }
    
    /* Verify first */
    uft_error_t err = uft_snapshot_verify(snapshot);
    if (err != UFT_SUCCESS) {
        return err;
    }
    
    /* Read snapshot */
    FILE *f = fopen(snapshot->path, "rb");
    if (!f) {
        return UFT_ERR_FILE_NOT_FOUND;
    }
    
    if (snapshot->size_bytes > 0) {
        if (fread(buf, 1, (size_t)snapshot->size_bytes, f) != (size_t)snapshot->size_bytes) {
            fclose(f);
            return UFT_ERR_IO;
        }
    }
    
    fclose(f);
    return UFT_SUCCESS;
}

uft_error_t uft_snapshot_restore_to_file(const uft_snapshot_t *snapshot,
                                         const char *dest_path) {
    if (!snapshot || !dest_path) {
        return UFT_ERR_INVALID_ARG;
    }
    
    /* Verify first */
    uft_error_t err = uft_snapshot_verify(snapshot);
    if (err != UFT_SUCCESS) {
        return err;
    }
    
    /* Read snapshot */
    size_t len = 0;
    uint8_t *buf = slurp_file(snapshot->path, &len);
    if (!buf && snapshot->size_bytes > 0) {
        return UFT_ERR_IO;
    }
    
    /* Write to destination */
    FILE *f = fopen(dest_path, "wb");
    if (!f) {
        free(buf);
        return UFT_ERR_IO;
    }
    
    if (len > 0 && fwrite(buf, 1, len, f) != len) {
        fclose(f);
        free(buf);
        return UFT_ERR_IO;
    }
    
    fclose(f);
    free(buf);
    return UFT_SUCCESS;
}

uft_error_t uft_snapshot_delete(const uft_snapshot_t *snapshot) {
    if (!snapshot || !snapshot->path[0]) {
        return UFT_ERR_INVALID_ARG;
    }
    
    if (remove(snapshot->path) != 0) {
        return UFT_ERR_IO;
    }
    
    return UFT_SUCCESS;
}

void uft_snapshot_get_hash_str(const uft_snapshot_t *snapshot, char out[65]) {
    if (!snapshot || !out) {
        if (out) out[0] = '\0';
        return;
    }
    uft_sha256_to_hex(snapshot->sha256, out);
}
