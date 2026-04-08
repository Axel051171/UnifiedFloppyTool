/**
 * @file uft_path_safe.h
 * @brief Safe Path Handling with Traversal Protection
 * 
 * Prevents directory traversal attacks and validates file paths.
 * 
 * @version 2.0.0
 * @date 2026-04-08
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
 * @brief Check if a path component equals ".."
 *
 * Walks each component separated by '/' or '\\' and rejects any
 * component that is exactly "..".  This avoids false positives from
 * filenames that legitimately contain ".." as a substring (e.g.
 * "data..old") while still catching all standard traversal attempts.
 *
 * @param path Path to check
 * @return true if no component is "..", false if traversal detected
 */
static inline bool uft_path_no_dotdot_component(const char *path) {
    if (!path) return false;

    const char *p = path;
    while (*p) {
        /* Find start of next component (skip leading separators) */
        while (*p == '/' || *p == '\\') p++;
        if (!*p) break;

        /* Find end of component */
        const char *start = p;
        while (*p && *p != '/' && *p != '\\') p++;
        size_t len = (size_t)(p - start);

        /* Check if this component is exactly ".." */
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if path contains directory traversal sequences
 * @param path Path to check
 * @return true if path is safe (no traversal), false if dangerous
 *
 * Rejects paths with ".." components, null bytes, UNC paths (Windows),
 * drive letters (Windows), or absolute paths (POSIX).
 */
static inline bool uft_path_is_safe(const char *path) {
    if (!path || !*path) {
        return false;
    }

    /* Reject embedded null bytes (strlen mismatch would indicate truncation) */
    /* Note: C strings terminate at first null, so we check for the patterns
     * that callers can actually pass.  The real defense is canonicalization
     * in uft_path_within_base(). */

    /* Reject any component that is exactly ".." */
    if (!uft_path_no_dotdot_component(path)) {
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
 * @brief Check if path stays within a base directory using canonicalization
 * @param path Path to check (may or may not exist)
 * @param base_dir Base directory to stay within (must exist)
 * @return true if path resolves within base_dir, false otherwise
 *
 * Uses realpath/_fullpath to resolve symlinks and ".." sequences,
 * then verifies the resolved path starts with the resolved base.
 * If the target file does not exist yet (common for write operations),
 * the parent directory is resolved instead and the filename is appended.
 */
static inline bool uft_path_within_base(const char *path, const char *base_dir) {
    if (!path || !base_dir || !*path || !*base_dir) {
        return false;
    }

    /* Quick reject: ".." components before expensive syscalls */
    if (!uft_path_no_dotdot_component(path)) {
        return false;
    }

    char resolved_path[PATH_MAX];
    char resolved_base[PATH_MAX];

    /* Resolve base directory — must exist */
    if (!uft_realpath_impl(base_dir, resolved_base)) {
        return false;
    }

    /* Try resolving path directly (works if it exists) */
    if (!uft_realpath_impl(path, resolved_path)) {
        /*
         * File may not exist yet (write operations).
         * Resolve the parent directory and append the basename.
         */
        char path_copy[PATH_MAX];
        size_t path_len = strlen(path);
        if (path_len == 0 || path_len >= PATH_MAX) return false;
        memcpy(path_copy, path, path_len + 1);

        /* Find last separator to split into parent + basename */
        char *last_sep = NULL;
        for (char *p = path_copy; *p; p++) {
            if (*p == '/' || *p == '\\') last_sep = p;
        }

        const char *basename_part;
        char parent_resolved[PATH_MAX];

        if (last_sep) {
            *last_sep = '\0';
            basename_part = last_sep + 1;
            if (!uft_realpath_impl(path_copy, parent_resolved)) {
                return false;
            }
        } else {
            /* No separator — file is in current directory */
            basename_part = path_copy;
            if (!uft_realpath_impl(".", parent_resolved)) {
                return false;
            }
        }

        /* Reject empty or dotdot basenames */
        if (!basename_part || !*basename_part) return false;
        if (basename_part[0] == '.' && basename_part[1] == '.' &&
            basename_part[2] == '\0') {
            return false;
        }

        /* Build the resolved path: parent + separator + basename */
        int written = snprintf(resolved_path, PATH_MAX, "%s%c%s",
                               parent_resolved, UFT_PATH_SEP, basename_part);
        if (written < 0 || (size_t)written >= PATH_MAX) {
            return false;
        }
    }

    /* Case-sensitive comparison (case-insensitive would be needed for
     * full Windows correctness, but this is conservative/safe) */
    size_t base_len = strlen(resolved_base);

#ifdef _WIN32
    /* Windows paths are case-insensitive */
    if (_strnicmp(resolved_path, resolved_base, base_len) != 0) {
        return false;
    }
#else
    if (strncmp(resolved_path, resolved_base, base_len) != 0) {
        return false;
    }
#endif

    /* Ensure the match is at a directory boundary */
    if (resolved_path[base_len] != '\0' &&
        resolved_path[base_len] != '/' &&
        resolved_path[base_len] != '\\') {
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
