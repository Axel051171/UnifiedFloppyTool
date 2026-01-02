#ifndef UFT_D64_V2_H
#define UFT_D64_V2_H

/**
 * @file uft_d64_v2.h
 * @brief D64 format - Migrated to unified API (v2.10.0)
 * 
 * D64 is a logical format for Commodore 1541 disk images.
 * 
 * Migration from v2.8.7:
 * - Uses unified uft_rc_t return codes
 * - Standardized _create()/_destroy() lifecycle
 * - NULL-safe with UFT_CHECK_NULL
 * - Error context in struct
 * 
 * @version 2.10.0
 * @date 2024-12-26
 */

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief D64 context structure
 * 
 * Caller must create with uft_d64_create() and destroy with uft_d64_destroy().
 */
typedef struct uft_d64_ctx {
    /** Image data (owned by context) */
    uint8_t* image;
    
    /** Image size in bytes */
    size_t image_size;
    
    /** Error information table present */
    bool has_error_table;
    
    /** File path (owned by context) */
    char* path;
    
    /** Writable mode */
    bool writable;
    
    /** Extended error context */
    uft_error_ctx_t error;
} uft_d64_ctx_t;

/**
 * @brief Create D64 context
 * 
 * @param[out] ctx Pointer to receive created context (caller owns)
 * @return UFT_SUCCESS or error code
 * @retval UFT_ERR_INVALID_ARG if ctx is NULL
 * @retval UFT_ERR_MEMORY if allocation fails
 * 
 * @note Caller must call uft_d64_destroy() when done
 */
uft_rc_t uft_d64_create(uft_d64_ctx_t** ctx);

/**
 * @brief Destroy D64 context and free resources
 * 
 * @param[in,out] ctx Pointer to context (set to NULL after destroy)
 * 
 * @note Safe to call with NULL or *ctx == NULL
 */
void uft_d64_destroy(uft_d64_ctx_t** ctx);

/**
 * @brief Detect if buffer contains D64 format
 * 
 * @param[in] buffer Data buffer to check
 * @param size Buffer size in bytes
 * @param[out] has_error_table Set to true if error table detected (can be NULL)
 * @return UFT_SUCCESS if D64 detected, UFT_ERR_FORMAT if not
 * @retval UFT_ERR_INVALID_ARG if buffer is NULL
 */
uft_rc_t uft_d64_detect(const uint8_t* buffer, size_t size, bool* has_error_table);

/**
 * @brief Open D64 image file
 * 
 * @param[in] ctx D64 context
 * @param[in] path File path
 * @param writable Open for writing
 * @return UFT_SUCCESS or error code
 * @retval UFT_ERR_INVALID_ARG if ctx or path is NULL
 * @retval UFT_ERR_FILE_NOT_FOUND if file doesn't exist
 * @retval UFT_ERR_IO if read fails
 * @retval UFT_ERR_FORMAT if invalid D64 format
 * 
 * @note Context must be created with uft_d64_create() first
 */
uft_rc_t uft_d64_open(uft_d64_ctx_t* ctx, const char* path, bool writable);

/**
 * @brief Read logical sector
 * 
 * @param[in] ctx D64 context
 * @param track Track number (1-35)
 * @param sector Sector number (0-20, varies by track)
 * @param[out] out_data Buffer to receive sector data
 * @param out_len Buffer size (must be >= 256)
 * @return UFT_SUCCESS or error code
 * @retval UFT_ERR_INVALID_ARG if ctx or out_data is NULL
 * @retval UFT_ERR_BUFFER_TOO_SMALL if out_len < 256
 * @retval UFT_ERR_INVALID_ARG if track/sector out of range
 */
uft_rc_t uft_d64_read_sector(
    uft_d64_ctx_t* ctx,
    uint8_t track,
    uint8_t sector,
    uint8_t* out_data,
    size_t out_len
);

/**
 * @brief Write logical sector
 * 
 * @param[in] ctx D64 context
 * @param track Track number (1-35)
 * @param sector Sector number (0-20, varies by track)
 * @param[in] in_data Sector data to write
 * @param in_len Data size (must be 256)
 * @return UFT_SUCCESS or error code
 * @retval UFT_ERR_INVALID_ARG if ctx or in_data is NULL
 * @retval UFT_ERR_INVALID_ARG if in_len != 256
 * @retval UFT_ERR_INVALID_ARG if track/sector out of range
 * @retval UFT_ERR_NOT_PERMITTED if not opened writable
 */
uft_rc_t uft_d64_write_sector(
    uft_d64_ctx_t* ctx,
    uint8_t track,
    uint8_t sector,
    const uint8_t* in_data,
    size_t in_len
);

/**
 * @brief Get total sectors on a track
 * 
 * D64 has variable sectors per track:
 * - Tracks 1-17: 21 sectors
 * - Tracks 18-24: 19 sectors  
 * - Tracks 25-30: 18 sectors
 * - Tracks 31-35: 17 sectors
 * 
 * @param track Track number (1-35)
 * @param[out] sectors Number of sectors (can be NULL)
 * @return UFT_SUCCESS or UFT_ERR_INVALID_ARG if track out of range
 */
uft_rc_t uft_d64_sectors_per_track(uint8_t track, uint8_t* sectors);

/**
 * @brief Close D64 image
 * 
 * Writes any pending changes and releases file handle.
 * Context remains valid, can open another file.
 * 
 * @param[in] ctx D64 context
 * @return UFT_SUCCESS or error code
 * @retval UFT_ERR_INVALID_ARG if ctx is NULL
 * @retval UFT_ERR_IO if write fails
 */
uft_rc_t uft_d64_close(uft_d64_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D64_V2_H */
