/**
 * @file uft_rtc_decompress.h
 * @brief Rob Northen Computing (RTC) Decompressor
 * 
 * Handles decompression of RTC/CTX compressed data found in
 * copy-protected Amiga and Atari ST software.
 * 
 * RTC copy protection was widely used in the late 1980s/early 1990s.
 * The compression uses an LZAR (Lempel-Ziv with Arithmetic coding)
 * variant with:
 * - 4KB sliding window (circular buffer)
 * - Adaptive arithmetic coding
 * - Literal/Length symbol model (315 symbols)
 * - Distance model (4096 symbols)
 * 
 * Based on reverse-engineered RTCExtractor by various authors.
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#ifndef UFT_RTC_DECOMPRESS_H
#define UFT_RTC_DECOMPRESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Sliding window size (4KB) */
#define UFT_RTC_WINDOW_SIZE     4096
#define UFT_RTC_WINDOW_MASK     0xFFF

/** Number of literal/length symbols (256 literals + 59 lengths) */
#define UFT_RTC_LITLEN_SYMBOLS  315

/** Number of distance symbols */
#define UFT_RTC_DIST_SYMBOLS    4096

/** Workspace size in bytes */
#define UFT_RTC_WORKSPACE_SIZE  66344

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    UFT_RTC_OK = 0,             /**< Success */
    UFT_RTC_ERR_NULLPTR,        /**< Null pointer argument */
    UFT_RTC_ERR_ALLOC,          /**< Memory allocation failed */
    UFT_RTC_ERR_INVALID_SIZE,   /**< Invalid output size in header */
    UFT_RTC_ERR_TRUNCATED,      /**< Input data truncated */
    UFT_RTC_ERR_OVERFLOW,       /**< Output buffer overflow */
    UFT_RTC_ERR_CORRUPT         /**< Compressed data corrupt */
} uft_rtc_error_t;

/*===========================================================================
 * Context Structure
 *===========================================================================*/

/**
 * @brief RTC decompression context
 * 
 * Opaque structure containing all state needed for decompression.
 * Use uft_rtc_ctx_create() and uft_rtc_ctx_destroy() to manage.
 */
typedef struct uft_rtc_ctx uft_rtc_ctx_t;

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Create decompression context
 * @return New context or NULL on allocation failure
 */
uft_rtc_ctx_t *uft_rtc_ctx_create(void);

/**
 * @brief Destroy decompression context
 * @param ctx Context to destroy (NULL-safe)
 */
void uft_rtc_ctx_destroy(uft_rtc_ctx_t *ctx);

/**
 * @brief Reset context for new decompression
 * @param ctx Context to reset
 */
void uft_rtc_ctx_reset(uft_rtc_ctx_t *ctx);

/**
 * @brief Decompress RTC/CTX data
 * 
 * @param ctx Decompression context
 * @param src Source (compressed) data
 * @param src_len Source data length
 * @param dst Destination buffer (or NULL to query size)
 * @param dst_len Destination buffer size (updated with actual size)
 * @return UFT_RTC_OK on success, error code otherwise
 * 
 * @note If dst is NULL, only reads header and returns expected
 *       output size in dst_len without decompressing.
 */
uft_rtc_error_t uft_rtc_decompress(uft_rtc_ctx_t *ctx,
                                    const uint8_t *src, size_t src_len,
                                    uint8_t *dst, size_t *dst_len);

/**
 * @brief Decompress with automatic allocation
 * 
 * @param src Source (compressed) data
 * @param src_len Source data length
 * @param dst_out Output pointer to decompressed data
 * @param dst_len_out Output length of decompressed data
 * @return UFT_RTC_OK on success, error code otherwise
 * 
 * @note Caller must free() the returned buffer.
 */
uft_rtc_error_t uft_rtc_decompress_alloc(const uint8_t *src, size_t src_len,
                                          uint8_t **dst_out, size_t *dst_len_out);

/**
 * @brief Get expected output size from compressed header
 * 
 * @param src Source data (minimum 4 bytes)
 * @param src_len Source data length
 * @return Expected decompressed size, or 0 on error
 */
uint32_t uft_rtc_get_output_size(const uint8_t *src, size_t src_len);

/**
 * @brief Check if data appears to be RTC compressed
 * 
 * @param src Source data
 * @param src_len Source data length
 * @return true if likely RTC compressed
 * 
 * @note This is heuristic-based, not definitive.
 */
bool uft_rtc_is_compressed(const uint8_t *src, size_t src_len);

/**
 * @brief Get error message for error code
 * @param err Error code
 * @return Human-readable error message
 */
const char *uft_rtc_error_string(uft_rtc_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RTC_DECOMPRESS_H */
