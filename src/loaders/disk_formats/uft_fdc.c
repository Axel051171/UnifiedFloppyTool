/**
 * @file uft_fdc.c
 * @brief FDC Raw Format Loader - Stub/Wrapper
 * 
 * This file provides basic FDC raw format support.
 * Full FDC bitstream handling is in src/fdc/
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Check if file is FDC raw format
 */
bool uft_fdc_probe(const uint8_t *data, size_t len) {
    (void)data;
    (void)len;
    /* FDC raw format has no specific magic - 
       detection is based on file extension or context */
    return false;
}

/**
 * @brief Get FDC format name
 */
const char *uft_fdc_format_name(void) {
    return "FDC Raw Bitstream";
}

/**
 * @brief Get file extension
 */
const char *uft_fdc_extension(void) {
    return ".fdc";
}

/* 
 * Note: Full FDC bitstream implementation is in:
 *   - src/fdc/uft_fdc.c
 *   - src/flux/fdc_bitstream/
 */
