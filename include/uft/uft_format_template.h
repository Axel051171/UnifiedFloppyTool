/**
 * @file uft_format_template.h
 * @brief Template for migrating formats to unified API v2.10.0
 * 
 * Use this as a starting point for migrating any format to the
 * unified UFT API with standard lifecycle, error handling, and
 * Atari 8-bit compatibility.
 * 
 * @version 2.10.0
 * @date 2024-12-26
 */

#ifndef UFT_FORMAT_TEMPLATE_H
#define UFT_FORMAT_TEMPLATE_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Format context structure
 * 
 * Standard pattern: All formats use this structure template.
 * Customize the format-specific fields as needed.
 */
typedef struct {
    /** File path (owned by context) */
    char* path;
    
    /** File handle (internal, NULL when closed) */
    void* fp_internal;
    
    /** Read-only mode */
    bool read_only;
    
    /** Geometry (if applicable) */
    uint32_t tracks;
    uint32_t heads;
    uint32_t sectors_per_track;
    uint32_t sector_size;
    
    /** Format-specific data */
    uint8_t* image_data;
    size_t image_size;
    
    /** Error context */
    uft_error_ctx_t error;
    
    /* Add format-specific fields here */
} uft_format_ctx_t;

/**
 * @brief Create format context
 * 
 * Standard lifecycle: Always pair with uft_format_destroy().
 * 
 * @param[out] ctx Pointer to receive created context (caller owns)
 * @return UFT_SUCCESS or error code
 * @retval UFT_ERR_INVALID_ARG if ctx is NULL
 * @retval UFT_ERR_MEMORY if allocation fails
 * 
 * @note Caller MUST call uft_format_destroy() when done
 * 
 * @code
 * uft_format_ctx_t* ctx = NULL;
 * uft_rc_t rc = uft_format_create(&ctx);
 * if (uft_success(rc)) {
 *     // Use ctx...
 *     uft_format_destroy(&ctx);
 * }
 * @endcode
 */
uft_rc_t uft_format_create(uft_format_ctx_t** ctx);

/**
 * @brief Destroy format context and free resources
 * 
 * Standard lifecycle: Always call after uft_format_create().
 * Safe to call with NULL or *ctx == NULL.
 * Sets *ctx to NULL after destruction.
 * 
 * @param[in,out] ctx Pointer to context (set to NULL after)
 */
void uft_format_destroy(uft_format_ctx_t** ctx);

/**
 * @brief Detect if buffer contains this format
 * 
 * @param[in] buffer Data buffer to check
 * @param size Buffer size in bytes
 * @return UFT_SUCCESS if format detected, UFT_ERR_FORMAT if not
 * @retval UFT_ERR_INVALID_ARG if buffer is NULL or size too small
 */
uft_rc_t uft_format_detect(const uint8_t* buffer, size_t size);

/**
 * @brief Open format file
 * 
 * @param[in] ctx Format context (must be created first)
 * @param[in] path File path
 * @param read_only Open in read-only mode
 * @return UFT_SUCCESS or error code
 * @retval UFT_ERR_INVALID_ARG if ctx or path is NULL
 * @retval UFT_ERR_FILE_NOT_FOUND if file doesn't exist
 * @retval UFT_ERR_IO if read fails
 * @retval UFT_ERR_FORMAT if invalid format
 */
uft_rc_t uft_format_open(uft_format_ctx_t* ctx, const char* path, bool read_only);

/**
 * @brief Read sector by CHS (Atari compatible)
 * 
 * Standard CHS pattern for all logical formats.
 * 
 * @param[in] ctx Format context
 * @param track Track/cylinder number (0-based)
 * @param head Head/side number (0-based)
 * @param sector Sector number (varies by format, often 1-based or 0-based)
 * @param[out] buffer Buffer to receive sector data
 * @param buffer_size Buffer size (must be >= sector size)
 * @param[out] bytes_read Actual bytes read (can be NULL)
 * @return UFT_SUCCESS or error code
 * @retval UFT_ERR_INVALID_ARG if ctx or buffer is NULL
 * @retval UFT_ERR_BUFFER_TOO_SMALL if buffer_size too small
 * @retval UFT_ERR_INVALID_ARG if track/head/sector out of range
 */
uft_rc_t uft_format_read_sector(
    uft_format_ctx_t* ctx,
    uint32_t track,
    uint32_t head,
    uint32_t sector,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_read
);

/**
 * @brief Write sector by CHS
 * 
 * @param[in] ctx Format context
 * @param track Track number
 * @param head Head number
 * @param sector Sector number
 * @param[in] data Data to write
 * @param data_size Data size (must match sector size)
 * @return UFT_SUCCESS or error code
 * @retval UFT_ERR_INVALID_ARG if ctx or data is NULL
 * @retval UFT_ERR_INVALID_ARG if data_size != sector_size
 * @retval UFT_ERR_NOT_PERMITTED if not opened writable
 */
uft_rc_t uft_format_write_sector(
    uft_format_ctx_t* ctx,
    uint32_t track,
    uint32_t head,
    uint32_t sector,
    const uint8_t* data,
    size_t data_size
);

/**
 * @brief Close format file
 * 
 * Flushes changes and closes file handle.
 * Context remains valid, can open another file.
 * 
 * @param[in] ctx Format context
 * @return UFT_SUCCESS or error code
 * @retval UFT_ERR_INVALID_ARG if ctx is NULL
 * @retval UFT_ERR_IO if write fails
 */
uft_rc_t uft_format_close(uft_format_ctx_t* ctx);

/* Add format-specific functions here */

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_TEMPLATE_H */
