/*
 * uft_msa.h - Atari ST MSA Image Format
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 *
 * MSA (Magic Shadow Archiver) is a compressed disk image format for Atari ST.
 * Uses RLE compression with $E5 marker byte.
 *
 * Reference: msa-to-zip (Scala), Hatari emulator
 */

#ifndef UFT_MSA_H
#define UFT_MSA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_MSA_SIGNATURE       0x0E0F
#define UFT_MSA_RLE_MARKER      0xE5
#define UFT_MSA_SECTOR_SIZE     512
#define UFT_MSA_HEADER_SIZE     10

/* ═══════════════════════════════════════════════════════════════════════════
 * Error Codes
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_msa_error {
    UFT_MSA_OK = 0,
    UFT_MSA_ERR_NULL_PTR,
    UFT_MSA_ERR_INVALID_SIGNATURE,
    UFT_MSA_ERR_INVALID_GEOMETRY,
    UFT_MSA_ERR_BUFFER_TOO_SMALL,
    UFT_MSA_ERR_DECOMPRESSION_FAILED,
    UFT_MSA_ERR_TRUNCATED,
    UFT_MSA_ERR_COMPRESSION_FAILED,
} uft_msa_error_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * MSA Header Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_msa_header {
    uint16_t signature;         /* Must be 0x0E0F */
    uint16_t sectors_per_track; /* Sectors per track (9-11) */
    uint16_t sides;             /* 0 = single, 1 = double (add 1!) */
    uint16_t start_track;       /* First track (usually 0) */
    uint16_t end_track;         /* Last track (usually 79 or 81) */
} uft_msa_header_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * MSA Image Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_msa_image {
    uft_msa_header_t header;
    
    /* Calculated values */
    uint8_t  side_count;        /* 1 or 2 */
    uint8_t  track_count;       /* end_track - start_track + 1 */
    uint32_t raw_size;          /* Uncompressed disk size */
    
    /* Decompressed data (caller-provided buffer) */
    uint8_t *data;
    size_t   data_size;
} uft_msa_image_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Parse MSA header from buffer
 *
 * @param src       Source buffer (at least 10 bytes)
 * @param src_len   Source buffer length
 * @param header    Output header structure
 * @return          UFT_MSA_OK or error code
 */
uft_msa_error_t uft_msa_parse_header(const uint8_t *src, size_t src_len,
                                      uft_msa_header_t *header);

/**
 * Validate MSA header
 *
 * @param header    Header to validate
 * @return          UFT_MSA_OK if valid
 */
uft_msa_error_t uft_msa_validate_header(const uft_msa_header_t *header);

/**
 * Calculate raw (uncompressed) disk size from header
 *
 * @param header    Parsed header
 * @return          Size in bytes, or 0 on error
 */
uint32_t uft_msa_calc_raw_size(const uft_msa_header_t *header);

/**
 * Decompress MSA image to raw sector data
 *
 * @param src       Source MSA data (including header)
 * @param src_len   Source data length
 * @param dst       Destination buffer for raw sectors
 * @param dst_len   Destination buffer size (must be >= raw_size)
 * @param out_len   Actual bytes written (optional, can be NULL)
 * @return          UFT_MSA_OK or error code
 */
uft_msa_error_t uft_msa_decompress(const uint8_t *src, size_t src_len,
                                    uint8_t *dst, size_t dst_len,
                                    size_t *out_len);

/**
 * Decompress single track
 *
 * @param src           Compressed track data (after 2-byte length)
 * @param src_len       Compressed data length
 * @param dst           Destination buffer
 * @param expected_len  Expected uncompressed length
 * @param out_len       Actual bytes written
 * @return              UFT_MSA_OK or error code
 */
uft_msa_error_t uft_msa_decompress_track(const uint8_t *src, size_t src_len,
                                          uint8_t *dst, size_t expected_len,
                                          size_t *out_len);

/**
 * Compress raw sector data to MSA format
 *
 * @param header    Header describing geometry
 * @param src       Raw sector data
 * @param src_len   Source length (must match header geometry)
 * @param dst       Destination buffer for MSA data
 * @param dst_len   Destination buffer size
 * @param out_len   Actual bytes written
 * @return          UFT_MSA_OK or error code
 */
uft_msa_error_t uft_msa_compress(const uft_msa_header_t *header,
                                  const uint8_t *src, size_t src_len,
                                  uint8_t *dst, size_t dst_len,
                                  size_t *out_len);

/**
 * Get error message string
 */
const char *uft_msa_strerror(uft_msa_error_t err);

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Macros
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Read big-endian 16-bit value (MSA uses big-endian!) */
#define UFT_MSA_READ_BE16(p) \
    (((uint16_t)(p)[0] << 8) | (uint16_t)(p)[1])

/* Write big-endian 16-bit value */
#define UFT_MSA_WRITE_BE16(p, v) do { \
    (p)[0] = (uint8_t)((v) >> 8); \
    (p)[1] = (uint8_t)((v) & 0xFF); \
} while(0)

#ifdef __cplusplus
}
#endif
#endif /* UFT_MSA_H */
