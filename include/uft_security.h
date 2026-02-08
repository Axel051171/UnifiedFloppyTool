/**
 * @file uft_security.h
 * @brief Secure helper functions for UFT project
 * @date 2026-01-06
 * 
 * This header provides safe alternatives to dangerous C functions
 * to prevent buffer overflows, command injection, and other vulnerabilities.
 */

#ifndef UFT_SECURITY_H
#define UFT_SECURITY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Safe string copy with guaranteed NUL termination
 * @param dst Destination buffer
 * @param src Source string
 * @param dst_size Size of destination buffer
 * @return Number of characters copied (excluding NUL), or dst_size if truncated
 */
static inline size_t uft_safe_strcpy(char *dst, const char *src, size_t dst_size) {
    if (dst == NULL || dst_size == 0) {
        return 0;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return 0;
    }
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dst_size - 1) ? src_len : dst_size - 1;
    
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
    
    return copy_len;
}

/**
 * @brief Safe string concatenation with guaranteed NUL termination
 * @param dst Destination buffer (must be NUL-terminated)
 * @param src Source string to append
 * @param dst_size Total size of destination buffer
 * @return Total length of resulting string, or dst_size if truncated
 */
static inline size_t uft_safe_strcat(char *dst, const char *src, size_t dst_size) {
    if (dst == NULL || dst_size == 0) {
        return 0;
    }
    
    size_t dst_len = strlen(dst);
    if (dst_len >= dst_size - 1) {
        return dst_len;
    }
    
    if (src == NULL) {
        return dst_len;
    }
    
    size_t remaining = dst_size - dst_len - 1;
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < remaining) ? src_len : remaining;
    
    memcpy(dst + dst_len, src, copy_len);
    dst[dst_len + copy_len] = '\0';
    
    return dst_len + copy_len;
}

/**
 * @brief Escape shell metacharacters in a string
 * @param dst Destination buffer for escaped string
 * @param src Source string to escape
 * @param dst_size Size of destination buffer
 * @return true if successful, false if buffer too small
 * 
 * Escapes: $ ` " \ ! and newlines
 */
static inline bool uft_shell_escape(char *dst, const char *src, size_t dst_size) {
    if (dst == NULL || src == NULL || dst_size < 2) {
        return false;
    }
    
    size_t j = 0;
    const char *dangerous = "$`\"\\!\n\r";
    
    for (size_t i = 0; src[i] != '\0' && j < dst_size - 2; i++) {
        if (strchr(dangerous, src[i]) != NULL) {
            if (j + 2 >= dst_size - 1) {
                dst[j] = '\0';
                return false;
            }
            dst[j++] = '\\';
        }
        dst[j++] = src[i];
    }
    dst[j] = '\0';
    
    return true;
}

/**
 * @brief Validate filename for safe shell use
 * @param filename Filename to validate
 * @return true if safe, false if contains dangerous characters
 */
static inline bool uft_is_safe_filename(const char *filename) {
    if (filename == NULL || filename[0] == '\0') {
        return false;
    }
    
    /* Reject if starts with dash (could be interpreted as option) */
    if (filename[0] == '-') {
        return false;
    }
    
    /* Check for dangerous characters */
    const char *dangerous = ";|&$`\"'\\<>(){}[]!#~*?\n\r";
    for (size_t i = 0; filename[i] != '\0'; i++) {
        if (strchr(dangerous, filename[i]) != NULL) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Safe multiplication with overflow check
 * @param a First operand
 * @param b Second operand
 * @param result Pointer to store result
 * @return true if no overflow, false if overflow would occur
 */
static inline bool uft_safe_mul(size_t a, size_t b, size_t *result) {
    if (a == 0 || b == 0) {
        *result = 0;
        return true;
    }
    if (a > SIZE_MAX / b) {
        return false;  /* Overflow would occur */
    }
    *result = a * b;
    return true;
}

/**
 * @brief Safe malloc with overflow-checked size calculation
 * @param count Number of elements
 * @param size Size of each element
 * @return Allocated memory or NULL on failure/overflow
 */
static inline void *uft_safe_malloc(size_t count, size_t size) {
    size_t total;
    if (!uft_safe_mul(count, size, &total)) {
        return NULL;  /* Overflow */
    }
    return malloc(total);
}

/**
 * @brief Safe calloc wrapper (already overflow-safe but adds consistency)
 */
static inline void *uft_safe_calloc(size_t count, size_t size) {
    return calloc(count, size);  /* calloc checks for overflow */
}

/**
 * @brief Securely zero memory before free
 * @param ptr Pointer to memory
 * @param size Size of memory region
 */
static inline void uft_secure_free(void *ptr, size_t size) {
    if (ptr != NULL) {
        volatile unsigned char *p = (volatile unsigned char *)ptr;
        while (size--) {
            *p++ = 0;
        }
        free(ptr);
    }
}

/**
 * @brief Constant-time memory comparison (prevents timing attacks)
 * @param a First buffer
 * @param b Second buffer
 * @param size Size to compare
 * @return 0 if equal, non-zero otherwise
 */
static inline int uft_secure_compare(const void *a, const void *b, size_t size) {
    const volatile unsigned char *pa = (const volatile unsigned char *)a;
    const volatile unsigned char *pb = (const volatile unsigned char *)b;
    unsigned char result = 0;
    
    for (size_t i = 0; i < size; i++) {
        result |= pa[i] ^ pb[i];
    }
    
    return result;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_SECURITY_H */
