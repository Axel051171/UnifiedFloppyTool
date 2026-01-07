/**
 * @file uft_safe_string.c
 * @brief Safe String Functions Implementation
 */

#include "uft_safe_string.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* strnlen is not always available (POSIX.1-2008) */
static size_t uft_strnlen(const char *s, size_t maxlen) {
    const char *end = memchr(s, '\0', maxlen);
    return end ? (size_t)(end - s) : maxlen;
}

size_t uft_strlcpy(char *dst, const char *src, size_t size) {
    if (!dst || !src) return 0;
    
    size_t src_len = strlen(src);
    
    if (size > 0) {
        size_t copy_len = (src_len >= size) ? size - 1 : src_len;
        memcpy(dst, src, copy_len);
        dst[copy_len] = '\0';
    }
    
    return src_len;
}

size_t uft_strlcat(char *dst, const char *src, size_t size) {
    if (!dst || !src) return 0;
    
    size_t dst_len = uft_strnlen(dst, size);
    size_t src_len = strlen(src);
    
    if (dst_len >= size) {
        return size + src_len;
    }
    
    size_t remaining = size - dst_len;
    if (src_len >= remaining) {
        memcpy(dst + dst_len, src, remaining - 1);
        dst[size - 1] = '\0';
    } else {
        memcpy(dst + dst_len, src, src_len + 1);
    }
    
    return dst_len + src_len;
}

int uft_snprintf(char *buf, size_t size, const char *fmt, ...) {
    if (!buf || size == 0) return 0;
    
    va_list ap;
    va_start(ap, fmt);
    int result = uft_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    
    return result;
}

int uft_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    if (!buf || size == 0 || !fmt) return 0;
    
    int result = vsnprintf(buf, size, fmt, ap);
    
    /* Ensure null-termination even if vsnprintf failed */
    if (result < 0) {
        buf[0] = '\0';
        return 0;
    }
    
    /* Ensure null-termination on truncation */
    if ((size_t)result >= size) {
        buf[size - 1] = '\0';
    }
    
    return result;
}

char *uft_strdup(const char *s) {
    if (!s) return NULL;
    
    size_t len = strlen(s) + 1;
    char *dup = malloc(len);
    if (dup) {
        memcpy(dup, s, len);
    }
    return dup;
}

char *uft_strndup(const char *s, size_t n) {
    if (!s) return NULL;
    
    size_t len = uft_strnlen(s, n);
    char *dup = malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len);
        dup[len] = '\0';
    }
    return dup;
}
