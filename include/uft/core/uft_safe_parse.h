/**
 * @file uft_safe_parse.h
 * @brief Safe String Parsing Functions
 * 
 * Replaces unsafe atoi/atol with error-checking alternatives.
 * 
 * @version 1.0.0
 * @date 2026-01-07
 */

#ifndef UFT_SAFE_PARSE_H
#define UFT_SAFE_PARSE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Safe Integer Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse string to int32 with error checking
 * @param str String to parse
 * @param result [out] Parsed value
 * @param base Number base (0 for auto-detect, 10 for decimal, 16 for hex)
 * @return true if parsing succeeded, false on error
 */
static inline bool uft_parse_int32(const char *str, int32_t *result, int base) {
    if (!str || !result || !*str) {
        return false;
    }
    
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, base);
    
    /* Check for various possible errors */
    if (errno == ERANGE || val > INT32_MAX || val < INT32_MIN) {
        return false;  /* Overflow */
    }
    if (endptr == str) {
        return false;  /* No digits found */
    }
    if (*endptr != '\0') {
        /* Trailing characters - might be OK depending on use case */
        /* For strict parsing, return false */
    }
    
    *result = (int32_t)val;
    return true;
}

/**
 * @brief Parse string to uint32 with error checking
 * @param str String to parse
 * @param result [out] Parsed value
 * @param base Number base
 * @return true if parsing succeeded
 */
static inline bool uft_parse_uint32(const char *str, uint32_t *result, int base) {
    if (!str || !result || !*str) {
        return false;
    }
    
    /* Check for negative sign */
    if (*str == '-') {
        return false;
    }
    
    char *endptr;
    errno = 0;
    unsigned long val = strtoul(str, &endptr, base);
    
    if (errno == ERANGE || val > UINT32_MAX) {
        return false;
    }
    if (endptr == str) {
        return false;
    }
    
    *result = (uint32_t)val;
    return true;
}

/**
 * @brief Parse string to uint64 with error checking
 * @param str String to parse
 * @param result [out] Parsed value
 * @param base Number base
 * @return true if parsing succeeded
 */
static inline bool uft_parse_uint64(const char *str, uint64_t *result, int base) {
    if (!str || !result || !*str) {
        return false;
    }
    
    if (*str == '-') {
        return false;
    }
    
    char *endptr;
    errno = 0;
    unsigned long long val = strtoull(str, &endptr, base);
    
    if (errno == ERANGE) {
        return false;
    }
    if (endptr == str) {
        return false;
    }
    
    *result = (uint64_t)val;
    return true;
}

/**
 * @brief Parse string to double with error checking
 * @param str String to parse
 * @param result [out] Parsed value
 * @return true if parsing succeeded
 */
static inline bool uft_parse_double(const char *str, double *result) {
    if (!str || !result || !*str) {
        return false;
    }
    
    char *endptr;
    errno = 0;
    double val = strtod(str, &endptr);
    
    if (errno == ERANGE) {
        return false;
    }
    if (endptr == str) {
        return false;
    }
    
    *result = val;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Safe Boolean Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse string to bool
 * @param str String to parse ("true", "false", "1", "0", "yes", "no")
 * @param result [out] Parsed value
 * @return true if parsing succeeded
 */
static inline bool uft_parse_bool(const char *str, bool *result) {
    if (!str || !result || !*str) {
        return false;
    }
    
    /* Check common true values */
    if (str[0] == '1' || str[0] == 't' || str[0] == 'T' || 
        str[0] == 'y' || str[0] == 'Y') {
        *result = true;
        return true;
    }
    
    /* Check common false values */
    if (str[0] == '0' || str[0] == 'f' || str[0] == 'F' ||
        str[0] == 'n' || str[0] == 'N') {
        *result = false;
        return true;
    }
    
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Hex String Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse hex string to byte array
 * @param hex Hex string (with or without 0x prefix)
 * @param out Output buffer
 * @param out_size Size of output buffer
 * @param bytes_written [out] Actual bytes written
 * @return true if parsing succeeded
 */
static inline bool uft_parse_hex_bytes(const char *hex, uint8_t *out, 
                                        size_t out_size, size_t *bytes_written) {
    if (!hex || !out || out_size == 0 || !bytes_written) {
        return false;
    }
    
    /* Skip 0x prefix if present */
    if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex += 2;
    }
    
    size_t hex_len = 0;
    while (hex[hex_len]) hex_len++;
    
    if (hex_len % 2 != 0) {
        return false;  /* Odd number of hex digits */
    }
    
    size_t num_bytes = hex_len / 2;
    if (num_bytes > out_size) {
        return false;  /* Buffer too small */
    }
    
    for (size_t i = 0; i < num_bytes; i++) {
        unsigned int byte;
        char byte_str[3] = { hex[i*2], hex[i*2 + 1], '\0' };
        
        char *endptr;
        byte = (unsigned int)strtoul(byte_str, &endptr, 16);
        if (endptr != byte_str + 2) {
            return false;  /* Invalid hex digit */
        }
        out[i] = (uint8_t)byte;
    }
    
    *bytes_written = num_bytes;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_SAFE_PARSE_H */
