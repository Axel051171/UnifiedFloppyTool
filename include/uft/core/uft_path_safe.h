/**
 * @file uft_path_safe.h
 * @brief Safe Path Handling with Traversal Protection
 * 
 * Prevents directory traversal attacks and validates file paths.
 * 
 * @version 1.0.0
 * @date 2026-01-07
 */

#ifndef UFT_PATH_SAFE_H
#define UFT_PATH_SAFE_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
    #include <direct.h>
    #include <windows.h>
    #define UFT_PATH_SEP '\\'
    #define UFT_PATH_SEP_STR "\\"
    #ifndef PATH_MAX
        #define PATH_MAX MAX_PATH
    #endif
    /* Windows: use _fullpath */
    static inline char *uft_realpath_impl(const char *path, char *resolved) {
        return _fullpath(resolved, path, PATH_MAX);
    }
#else
    #include <unistd.h>
    #include <limits.h>
    #define UFT_PATH_SEP '/'
    #define UFT_PATH_SEP_STR "/"
    #ifndef PATH_MAX
        #define PATH_MAX 4096
    #endif
    /* POSIX realpath declaration */
    extern char *realpath(const char *path, char *resolved_path);
    /* POSIX: use realpath */
    static inline char *uft_realpath_impl(const char *path, char *resolved) {
        return realpath(path, resolved);
    }
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Path Validation
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if path contains directory traversal sequences
 * @param path Path to check
 * @return true if path is safe (no traversal), false if dangerous
 */
static inline bool uft_path_is_safe(const char *path) {
    if (!path || !*path) {
        return false;
    }
    
    /* Check for obvious traversal patterns */
    if (strstr(path, "..") != NULL) {
        return false;
    }
    
#ifdef _WIN32
    if (path[0] == '\\' && path[1] == '\\') {
        return false;  /* UNC path */
    }
    if (path[1] == ':') {
        return false;  /* Drive letter */
    }
#else
    if (path[0] == '/') {
        return false;  /* Absolute path */
    }
#endif
    
    return true;
}

/**
 * @brief Check if path stays within a base directory
 * @param path Path to check
 * @param base_dir Base directory to stay within
 * @return true if path resolves within base_dir
 */
static inline bool uft_path_within_base(const char *path, const char *base_dir) {
    if (!path || !base_dir) {
        return false;
    }
    
    char resolved_path[PATH_MAX];
    char resolved_base[PATH_MAX];
    
    if (!uft_realpath_impl(path, resolved_path)) {
        /* Path doesn't exist - try checking parent */
        return false;
    }
    if (!uft_realpath_impl(base_dir, resolved_base)) {
        return false;
    }
    
    size_t base_len = strlen(resolved_base);
    if (strncmp(resolved_path, resolved_base, base_len) != 0) {
        return false;
    }
    
    if (resolved_path[base_len] != '\0' && 
        resolved_path[base_len] != UFT_PATH_SEP) {
        return false;
    }
    
    return true;
}

/**
 * @brief Sanitize a filename (remove path separators)
 * @param filename Input filename
 * @param out Output buffer
 * @param out_size Size of output buffer
 * @return true if successful
 */
static inline bool uft_sanitize_filename(const char *filename, char *out, size_t out_size) {
    if (!filename || !out || out_size == 0) {
        return false;
    }
    
    size_t j = 0;
    for (size_t i = 0; filename[i] && j < out_size - 1; i++) {
        char c = filename[i];
        if (c == '/' || c == '\\' || c == ':' || c < 32) {
            continue;
        }
        out[j++] = c;
    }
    out[j] = '\0';
    
    if (j == 0 || (j == 1 && out[0] == '.') || 
        (j == 2 && out[0] == '.' && out[1] == '.')) {
        out[0] = '_';
        out[1] = '\0';
    }
    
    return true;
}

/**
 * @brief Build safe path from base + filename
 */
static inline bool uft_build_safe_path(const char *base_dir, const char *filename,
                                        char *out, size_t out_size) {
    if (!base_dir || !filename || !out || out_size == 0) {
        return false;
    }
    
    char safe_name[256];
    if (!uft_sanitize_filename(filename, safe_name, sizeof(safe_name))) {
        return false;
    }
    
    int written = snprintf(out, out_size, "%s%s%s", 
                           base_dir, UFT_PATH_SEP_STR, safe_name);
    
    return (written > 0 && (size_t)written < out_size);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_PATH_SAFE_H */
