/**
 * @file uft_safe_string.h
 * @brief Safe string functions for UFT
 * 
 * Provides portable, bounds-checked string operations:
 * - uft_strlcpy: Safe strcpy replacement
 * - uft_strlcat: Safe strcat replacement
 * - uft_snprintf: Always null-terminated snprintf
 * 
 * These functions prevent buffer overflows and always null-terminate.
 * 
 * @version 1.0
 * @date 2026-01-07
 */

#ifndef UFT_SAFE_STRING_H
#define UFT_SAFE_STRING_H

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>  /* malloc, free */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Safe string copy (like OpenBSD strlcpy)
 * 
 * Copies up to size-1 characters from src to dst, always null-terminating.
 * 
 * @param dst   Destination buffer
 * @param src   Source string
 * @param size  Size of destination buffer
 * @return      Total length of src (for truncation detection: if >= size, truncated)
 */
static inline size_t uft_strlcpy(char *dst, const char *src, size_t size) {
    size_t src_len = strlen(src);
    
    if (size > 0) {
        size_t copy_len = (src_len >= size) ? (size - 1) : src_len;
        memcpy(dst, src, copy_len);
        dst[copy_len] = '\0';
    }
    
    return src_len;
}

/**
 * @brief Safe string concatenation (like OpenBSD strlcat)
 * 
 * Appends src to dst, ensuring null-termination and not exceeding size.
 * 
 * @param dst   Destination buffer (must be null-terminated)
 * @param src   Source string to append
 * @param size  Total size of destination buffer
 * @return      Total length that would result (for truncation detection)
 */
static inline size_t uft_strlcat(char *dst, const char *src, size_t size) {
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);
    
    if (dst_len >= size) {
        /* dst is already longer than size, nothing to do */
        return size + src_len;
    }
    
    size_t remaining = size - dst_len;
    if (src_len < remaining) {
        memcpy(dst + dst_len, src, src_len + 1);
    } else {
        memcpy(dst + dst_len, src, remaining - 1);
        dst[size - 1] = '\0';
    }
    
    return dst_len + src_len;
}

/**
 * @brief Safe snprintf wrapper (always null-terminates)
 * 
 * Standard snprintf may not null-terminate on some platforms.
 * This wrapper guarantees null-termination.
 * 
 * @param dst   Destination buffer
 * @param size  Size of destination buffer
 * @param fmt   Format string
 * @param ...   Format arguments
 * @return      Number of characters that would be written (excluding null)
 */
static inline int uft_snprintf(char *dst, size_t size, const char *fmt, ...) {
    if (size == 0) return 0;
    
    va_list args;
    va_start(args, fmt);
    int result = vsnprintf(dst, size, fmt, args);
    va_end(args);
    
    /* Ensure null-termination */
    if (result >= 0 && (size_t)result >= size) {
        dst[size - 1] = '\0';
    }
    
    return result;
}

/**
 * @brief Safe vsnprintf wrapper
 */
static inline int uft_vsnprintf(char *dst, size_t size, const char *fmt, va_list args) {
    if (size == 0) return 0;
    
    int result = vsnprintf(dst, size, fmt, args);
    
    if (result >= 0 && (size_t)result >= size) {
        dst[size - 1] = '\0';
    }
    
    return result;
}

/**
 * @brief Check if string was truncated by strlcpy/strlcat
 * 
 * @param result  Return value from uft_strlcpy or uft_strlcat
 * @param size    Size of destination buffer
 * @return        true if truncation occurred
 */
static inline int uft_str_truncated(size_t result, size_t size) {
    return result >= size;
}

/**
 * @brief Safe string duplication with length limit
 * 
 * @param src     Source string
 * @param maxlen  Maximum length to copy (excluding null)
 * @return        Newly allocated string (caller must free) or NULL
 */
static inline char* uft_strndup(const char *src, size_t maxlen) {
    size_t len = strlen(src);
    if (len > maxlen) len = maxlen;
    
    char *dst = (char*)malloc(len + 1);
    if (dst) {
        memcpy(dst, src, len);
        dst[len] = '\0';
    }
    return dst;
}

/**
 * @brief Clear sensitive data from memory
 * 
 * Uses volatile to prevent compiler optimization.
 * 
 * @param ptr   Pointer to memory
 * @param size  Size to clear
 */
static inline void uft_secure_zero(void *ptr, size_t size) {
    volatile unsigned char *p = (volatile unsigned char*)ptr;
    while (size--) {
        *p++ = 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_SAFE_STRING_H */
