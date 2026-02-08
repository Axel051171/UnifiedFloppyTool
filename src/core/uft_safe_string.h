#ifndef UFT_SAFE_STRING_H
#define UFT_SAFE_STRING_H
/**
 * @file uft_safe_string.h
 * @brief Safe String Functions for UFT
 * 
 * Replacement for unsafe functions like strcpy, sprintf, strcat.
 * All functions guarantee null-termination and bounds checking.
 */

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Safe string copy with guaranteed null-termination
 * @param dst Destination buffer
 * @param src Source string
 * @param size Size of destination buffer
 * @return Length of src (for truncation detection: if >= size, truncated)
 */
size_t uft_strlcpy(char *dst, const char *src, size_t size);

/**
 * @brief Safe string concatenation with guaranteed null-termination
 * @param dst Destination buffer (must be null-terminated)
 * @param src Source string to append
 * @param size Total size of destination buffer
 * @return Total length attempted (for truncation detection)
 */
size_t uft_strlcat(char *dst, const char *src, size_t size);

/**
 * @brief Safe formatted string print
 * @param buf Destination buffer
 * @param size Size of destination buffer
 * @param fmt Format string
 * @return Number of characters that would have been written (excl. null)
 */
int uft_snprintf(char *buf, size_t size, const char *fmt, ...);

/**
 * @brief Safe formatted string print (va_list version)
 */
int uft_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

/**
 * @brief Safe string duplication
 * @param s String to duplicate
 * @return Newly allocated copy (must be freed) or NULL on error
 */
char *uft_strdup(const char *s);

/**
 * @brief Safe string duplication with length limit
 * @param s String to duplicate
 * @param n Maximum characters to copy
 * @return Newly allocated copy (must be freed) or NULL on error
 */
char *uft_strndup(const char *s, size_t n);

/* Macros for easy migration */
#define UFT_STRCPY(dst, src) uft_strlcpy(dst, src, sizeof(dst))
#define UFT_STRCAT(dst, src) uft_strlcat(dst, src, sizeof(dst))
#define UFT_SPRINTF(buf, ...) uft_snprintf(buf, sizeof(buf), __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* UFT_SAFE_STRING_H */
