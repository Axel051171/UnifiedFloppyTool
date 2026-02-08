/**
 * @file uft_strbuf.h
 * @brief Dynamic String Buffer (P3-001)
 * 
 * Efficient string building without repeated realloc.
 * Useful for log messages, reports, and path construction.
 * 
 * Usage:
 *   uft_strbuf_t sb;
 *   uft_strbuf_init(&sb, 256);
 *   uft_strbuf_append(&sb, "Track ");
 *   uft_strbuf_appendf(&sb, "%d: ", track);
 *   uft_strbuf_append(&sb, status);
 *   printf("%s\n", uft_strbuf_get(&sb));
 *   uft_strbuf_free(&sb);
 * 
 * @version 1.0.0
 * @date 2026-01-07
 */

#ifndef UFT_STRBUF_H
#define UFT_STRBUF_H

#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char   *data;       /**< String buffer */
    size_t  len;        /**< Current length (excluding null) */
    size_t  capacity;   /**< Allocated capacity */
} uft_strbuf_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize string buffer
 * @param sb String buffer
 * @param initial_capacity Initial capacity (0 for default 64)
 * @return true on success
 */
static inline bool uft_strbuf_init(uft_strbuf_t *sb, size_t initial_capacity) {
    if (!sb) return false;
    
    if (initial_capacity == 0) initial_capacity = 64;
    
    sb->data = (char*)malloc(initial_capacity);
    if (!sb->data) return false;
    
    sb->data[0] = '\0';
    sb->len = 0;
    sb->capacity = initial_capacity;
    
    return true;
}

/**
 * @brief Free string buffer
 */
static inline void uft_strbuf_free(uft_strbuf_t *sb) {
    if (sb && sb->data) {
        free(sb->data);
        sb->data = NULL;
        sb->len = 0;
        sb->capacity = 0;
    }
}

/**
 * @brief Clear buffer (keep capacity)
 */
static inline void uft_strbuf_clear(uft_strbuf_t *sb) {
    if (sb && sb->data) {
        sb->data[0] = '\0';
        sb->len = 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Growth
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Ensure capacity for additional bytes
 */
static inline bool uft_strbuf_grow(uft_strbuf_t *sb, size_t additional) {
    if (!sb) return false;
    
    size_t needed = sb->len + additional + 1;
    if (needed <= sb->capacity) return true;
    
    /* Grow by 1.5x or to needed, whichever is larger */
    size_t new_cap = sb->capacity + sb->capacity / 2;
    if (new_cap < needed) new_cap = needed;
    
    char *new_data = (char*)realloc(sb->data, new_cap);
    if (!new_data) return false;
    
    sb->data = new_data;
    sb->capacity = new_cap;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Append Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Append string
 */
static inline bool uft_strbuf_append(uft_strbuf_t *sb, const char *str) {
    if (!sb || !str) return false;
    
    size_t len = strlen(str);
    if (!uft_strbuf_grow(sb, len)) return false;
    
    memcpy(sb->data + sb->len, str, len + 1);
    sb->len += len;
    return true;
}

/**
 * @brief Append n bytes
 */
static inline bool uft_strbuf_append_n(uft_strbuf_t *sb, const char *str, size_t n) {
    if (!sb || !str) return false;
    if (!uft_strbuf_grow(sb, n)) return false;
    
    memcpy(sb->data + sb->len, str, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
    return true;
}

/**
 * @brief Append single character
 */
static inline bool uft_strbuf_append_char(uft_strbuf_t *sb, char c) {
    if (!sb) return false;
    if (!uft_strbuf_grow(sb, 1)) return false;
    
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
    return true;
}

/**
 * @brief Append formatted string
 */
static inline bool uft_strbuf_appendf(uft_strbuf_t *sb, const char *fmt, ...) {
    if (!sb || !fmt) return false;
    
    va_list args, args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);
    
    /* Calculate needed size */
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    
    if (needed < 0) {
        va_end(args_copy);
        return false;
    }
    
    if (!uft_strbuf_grow(sb, (size_t)needed)) {
        va_end(args_copy);
        return false;
    }
    
    vsnprintf(sb->data + sb->len, (size_t)needed + 1, fmt, args_copy);
    sb->len += (size_t)needed;
    va_end(args_copy);
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Access
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get string pointer (read-only)
 */
static inline const char *uft_strbuf_get(const uft_strbuf_t *sb) {
    return sb ? sb->data : "";
}

/**
 * @brief Get current length
 */
static inline size_t uft_strbuf_len(const uft_strbuf_t *sb) {
    return sb ? sb->len : 0;
}

/**
 * @brief Detach buffer (caller owns memory)
 */
static inline char *uft_strbuf_detach(uft_strbuf_t *sb) {
    if (!sb) return NULL;
    
    char *data = sb->data;
    sb->data = NULL;
    sb->len = 0;
    sb->capacity = 0;
    return data;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_STRBUF_H */
