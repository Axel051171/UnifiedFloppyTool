/**
 * @file uft_opencbm_gcr.h
 * @version 1.0.0
 * @date 2026-01-06
 *
 *
 * Commodore GCR (Group Code Recording) converts 4 data bits into 5 GCR bits,
 * ensuring no more than 2 consecutive zeros (important for magnetic recording).
 *
 * This is used by:
 * - Commodore 1541/1571/1581 drives
 * - D64, G64, NIB formats
 * - C64, C128, VIC-20, PET systems
 */

#ifndef UFT_OPENCBM_GCR_H
#define UFT_OPENCBM_GCR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Standard Commodore block size (256 bytes) */
#define UFT_GCR_BLOCK_SIZE      256

/** GCR encoded block size (325 bytes = 256 * 10/8 + overhead) */
#define UFT_GCR_ENCODED_SIZE    325

/** Sector header size (10 GCR bytes) */
#define UFT_GCR_HEADER_SIZE     10

/*============================================================================
 * Error Codes
 *============================================================================*/

#define UFT_GCR_OK              0       /**< Success */
#define UFT_GCR_ERR_NULLPTR     (-1)    /**< Null pointer */
#define UFT_GCR_ERR_DECODE      (-2)    /**< Decode error (invalid GCR) */
#define UFT_GCR_ERR_HEADER      (-3)    /**< Invalid header marker */
#define UFT_GCR_ERR_CHECKSUM    (-4)    /**< Checksum mismatch */

/*============================================================================
 * Structures
 *============================================================================*/

/**
 * @brief Decoded sector header
 */
typedef struct {
    uint8_t header_checksum;    /**< Header checksum byte */
    uint8_t sector;             /**< Sector number (0-20) */
    uint8_t track;              /**< Track number (1-40) */
    uint8_t id2;                /**< Disk ID byte 2 */
    uint8_t id1;                /**< Disk ID byte 1 */
    bool    valid;              /**< Checksum valid */
} uft_gcr_sector_header_t;

/*============================================================================
 * Low-Level GCR Conversion
 *============================================================================*/

/**
 * @brief Decode 5 GCR bytes to 4 data bytes
 * @param source Input GCR bytes (5 bytes)
 * @param dest Output data bytes (4 bytes)
 * @param source_len Source buffer length
 * @param dest_len Destination buffer length
 * @return 0 on success, -1 on error, or bitmask of invalid GCR codes
 *
 * @note Supports partial conversion if buffer lengths are smaller
 * @note Allows overlapping buffers (dest < source+2 or dest > source+4)
 */
int uft_gcr_5_to_4_decode(const uint8_t* source, uint8_t* dest,
                           size_t source_len, size_t dest_len);

/**
 * @brief Encode 4 data bytes to 5 GCR bytes
 * @param source Input data bytes (4 bytes)
 * @param dest Output GCR bytes (5 bytes)
 * @param source_len Source buffer length
 * @param dest_len Destination buffer length
 * @return 0 on success, -1 on error
 *
 * @note Supports partial conversion
 * @note Allows overlapping buffers (dest <= source or dest > source+3)
 */
int uft_gcr_4_to_5_encode(const uint8_t* source, uint8_t* dest,
                           size_t source_len, size_t dest_len);

/*============================================================================
 * Block Operations
 *============================================================================*/

/**
 * @brief Decode a full GCR-encoded data block
 * @param gcr_data Input GCR data (325 bytes)
 * @param decoded Output decoded data (256 bytes)
 * @param header_out Optional: decoded header (4 bytes)
 * @return UFT_GCR_OK on success, error code on failure
 *
 * Expects format:
 * - 5 bytes: header (0x07 marker + 3 data bytes)
 * - 315 bytes: main data (63 groups of 5 GCR -> 4 bytes)
 * - 5 bytes: trailer (last byte + checksum)
 */
int uft_gcr_decode_block(const uint8_t* gcr_data, uint8_t* decoded,
                          uint8_t* header_out);

/**
 * @brief Encode a data block to GCR
 * @param data Input data (256 bytes)
 * @param gcr_out Output GCR data (325 bytes)
 * @return UFT_GCR_OK on success, error code on failure
 */
int uft_gcr_encode_block(const uint8_t* data, uint8_t* gcr_out);

/*============================================================================
 * Sector Header Operations
 *============================================================================*/

/**
 * @brief Decode a GCR sector header
 * @param gcr_header Input GCR header (10 bytes)
 * @param header Output decoded header
 * @return UFT_GCR_OK on success, error code on failure
 */
int uft_gcr_decode_sector_header(const uint8_t* gcr_header, 
                                  uft_gcr_sector_header_t* header);

/**
 * @brief Encode a sector header to GCR
 * @param header Input header data
 * @param gcr_out Output GCR header (10 bytes)
 * @return UFT_GCR_OK on success, error code on failure
 */
int uft_gcr_encode_sector_header(const uft_gcr_sector_header_t* header,
                                  uint8_t* gcr_out);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Check if a 5-bit GCR code is valid
 * @param gcr_value GCR value (0-31)
 * @return true if valid GCR code
 */
bool uft_gcr_is_valid_code(uint8_t gcr_value);

/**
 * @brief Encode a single 4-bit nybble to 5-bit GCR
 * @param nybble Input nybble (0-15)
 * @return GCR code (0-31)
 */
uint8_t uft_gcr_encode_nybble(uint8_t nybble);

/**
 * @brief Decode a single 5-bit GCR code to 4-bit nybble
 * @param gcr_value GCR code (0-31)
 * @param valid Output: true if valid GCR code
 * @return Decoded nybble (0-15), or 0 if invalid
 */
uint8_t uft_gcr_decode_nybble(uint8_t gcr_value, bool* valid);

#ifdef __cplusplus
}
#endif

#endif /* UFT_OPENCBM_GCR_H */
