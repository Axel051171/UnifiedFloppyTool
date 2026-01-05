/**
 * @file uft_2mg_parser.h
 * @brief Enhanced Apple 2MG Parser with Full Geometry Detection
 * @version 3.3.2
 * @date 2025-01-03
 *
 * Supports:
 * - DOS 3.3 sector order
 * - ProDOS sector order
 * - Nibblized data (read-only info)
 * - 5.25" and 3.5" disk geometries
 * - Comment and creator data blocks
 */

#ifndef UFT_2MG_PARSER_H
#define UFT_2MG_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/* Format codes */
#define IMG2_FMT_DOS33      0
#define IMG2_FMT_PRODOS     1
#define IMG2_FMT_NIB        2

/*============================================================================
 * TYPES
 *============================================================================*/

/* Opaque parser context */
typedef struct img2_parser_ctx img2_parser_ctx_t;

/*============================================================================
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Open 2MG file
 * @param path Path to 2MG file
 * @return Parser context or NULL on error
 */
img2_parser_ctx_t* img2_parser_open(const char* path);

/**
 * @brief Close 2MG parser
 * @param ctx Pointer to context pointer (set to NULL)
 */
void img2_parser_close(img2_parser_ctx_t** ctx);

/**
 * @brief Read sector
 * @param ctx Parser context
 * @param track Track number (0-based)
 * @param head Head (0 or 1)
 * @param sector Sector number (0-based)
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes read, or -1 on error
 */
int img2_parser_read_sector(
    img2_parser_ctx_t* ctx,
    int track,
    int head,
    int sector,
    uint8_t* buffer,
    size_t buffer_size
);

/**
 * @brief Write sector
 * @param ctx Parser context
 * @param track Track number
 * @param head Head
 * @param sector Sector number
 * @param buffer Input data
 * @param buffer_size Data size
 * @return Bytes written, or -1 on error
 */
int img2_parser_write_sector(
    img2_parser_ctx_t* ctx,
    int track,
    int head,
    int sector,
    const uint8_t* buffer,
    size_t buffer_size
);

/**
 * @brief Get disk geometry info
 * @param ctx Parser context
 * @param tracks Output: number of tracks (may be NULL)
 * @param sectors Output: sectors per track (may be NULL)
 * @param sector_size Output: sector size (may be NULL)
 * @param heads Output: number of heads (may be NULL)
 * @param disk_type Output: disk type string (may be NULL)
 * @param format_name Output: format name string (may be NULL)
 */
void img2_parser_get_info(
    img2_parser_ctx_t* ctx,
    int* tracks,
    int* sectors,
    int* sector_size,
    int* heads,
    const char** disk_type,
    const char** format_name
);

/**
 * @brief Get header info
 * @param ctx Parser context
 * @param creator Output: creator ID (4 chars + null, buffer >= 5)
 * @param creator_size Size of creator buffer
 * @param format Output: format code (may be NULL)
 * @param prodos_blocks Output: ProDOS block count (may be NULL)
 * @param locked Output: locked flag (may be NULL)
 * @param comment Output: comment string pointer (may be NULL)
 */
void img2_parser_get_header_info(
    img2_parser_ctx_t* ctx,
    char* creator,
    size_t creator_size,
    uint32_t* format,
    uint32_t* prodos_blocks,
    bool* locked,
    const char** comment
);

/**
 * @brief Analyze disk and generate report
 * @param ctx Parser context
 * @param report Output buffer
 * @param report_size Buffer size
 * @return Bytes written, or -1 on error
 */
int img2_parser_analyze(
    img2_parser_ctx_t* ctx,
    char* report,
    size_t report_size
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_2MG_PARSER_H */
