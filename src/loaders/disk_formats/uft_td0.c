/**
 * @file uft_td0.c
 * @brief TD0 (Teledisk) Format Loader - Stub/Wrapper
 * 
 * This file wraps the main TD0 implementation.
 * Full implementation is in src/formats/td0/
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* TD0 magic bytes */
#define TD0_MAGIC_NORMAL    0x4454  /* "TD" */
#define TD0_MAGIC_ADVANCED  0x6474  /* "td" (compressed) */

/**
 * @brief Check if file is TD0 format
 */
bool uft_td0_probe(const uint8_t *data, size_t len) {
    if (!data || len < 2) return false;
    uint16_t magic = data[0] | (data[1] << 8);
    return (magic == TD0_MAGIC_NORMAL || magic == TD0_MAGIC_ADVANCED);
}

/**
 * @brief Get TD0 format name
 */
const char *uft_td0_format_name(void) {
    return "Teledisk (TD0)";
}

/**
 * @brief Get file extension
 */
const char *uft_td0_extension(void) {
    return ".td0";
}

/* 
 * Note: Full TD0 implementation with LZSS decompression,
 * track parsing, and sector extraction is in:
 *   - src/formats/td0/uft_td0.c
 *   - src/formats/td0/uft_td0_lzss.c
 *   - src/formats/td0/uft_td0_parser_v2.c
 */
