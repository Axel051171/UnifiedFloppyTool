/*
 * uft_msa.h - Atari ST MSA (Magic Shadow Archiver) Format Parser
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on msa-to-zip (Olivier Bruchez) - Algorithm extraction
 *
 * MSA is a compressed disk image format for Atari ST.
 * Uses simple RLE compression with $E5 marker byte.
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
#define UFT_MSA_HEADER_SIZE     10
#define UFT_MSA_SECTOR_SIZE     512
#define UFT_MSA_RLE_MARKER      0xE5

/* Maximum supported geometry */
#define UFT_MSA_MAX_TRACKS      86
#define UFT_MSA_MAX_SIDES       2
#define UFT_MSA_MAX_SECTORS     11

/* ═══════════════════════════════════════════════════════════════════════════
 * Error Codes
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_msa_error {
    UFT_MSA_OK = 0,
    UFT_MSA_ERR_NULL_PTR,
    UFT_MSA_ERR_INVALID_SIGNATURE,
    UFT_MSA_ERR_INVALID_GEOMETRY,
    UFT_MSA_ERR_BUFFER_TOO_SMALL,
    UFT_MSA_ERR_TRUNCATED_DATA,
    UFT_MSA_ERR_RLE_OVERFLOW,
    UFT_MSA_ERR_COMPRESSION_FAILED,
} uft_msa_error_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * MSA Header Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_msa_header {
    uint16_t signature;         /* Must be 0x0E0F */
    uint16_t sectors_per_track; /* 9, 10, or 11 */
    uint16_t sides;             /* 0 = single, 1 = double (add 1 for count) */
    uint16_t start_track;       /* Usually 0 */
    uint16_t end_track;         /* Usually 79 or 81 */
} uft_msa_header_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * MSA Track Header
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_msa_track_header {
    uint16_t data_length;       /* Compressed length (or uncompressed if equal) */
} uft_msa_track_header_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * MSA Image Info (parsed from header)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_msa_info {
    uint16_t sectors_per_track;
    uint8_t  side_count;        /* 1 or 2 */
    uint16_t start_track;
    uint16_t end_track;
    uint16_t track_count;       /* end_track - start_track + 1 */
    uint32_t raw_size;          /* Uncompressed size in bytes */
} uft_msa_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * RLE Compression Statistics
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_msa_stats {
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t tracks_compressed;
    uint32_t tracks_uncompressed;
    float    compression_ratio;
} uft_msa_stats_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions - Header Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Parse MSA header from buffer
 */
uft_msa_error_t uft_msa_parse_header(const uint8_t *data, size_t len,
                                      uft_msa_header_t *header);

/**
 * Validate MSA header and extract info
 */
uft_msa_error_t uft_msa_validate_header(const uft_msa_header_t *header,
                                         uft_msa_info_t *info);

/**
 * Quick probe - check if data is valid MSA
 */
int uft_msa_probe(const uint8_t *data, size_t len);

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions - RLE Decompression
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Decompress MSA RLE data
 * 
 * RLE format:
 *   - $E5 <data_byte> <run_length_hi> <run_length_lo>
 *   - All other bytes are literal
 */
uft_msa_error_t uft_msa_rle_decode(const uint8_t *src, size_t src_len,
                                    uint8_t *dst, size_t dst_len,
                                    size_t *out_written);

/**
 * Compress data using MSA RLE
 */
uft_msa_error_t uft_msa_rle_encode(const uint8_t *src, size_t src_len,
                                    uint8_t *dst, size_t dst_len,
                                    size_t *out_written);

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions - Full Image Conversion
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Convert MSA image to raw ST format
 */
uft_msa_error_t uft_msa_to_st(const uint8_t *msa_data, size_t msa_len,
                               uint8_t *st_data, size_t st_len,
                               size_t *out_written,
                               uft_msa_stats_t *stats);

/**
 * Convert raw ST image to MSA format
 */
uft_msa_error_t uft_st_to_msa(const uint8_t *st_data, size_t st_len,
                               const uft_msa_info_t *info,
                               uint8_t *msa_data, size_t msa_len,
                               size_t *out_written,
                               uft_msa_stats_t *stats);

/**
 * Calculate required buffer size for MSA to ST conversion
 */
size_t uft_msa_get_st_size(const uint8_t *msa_data, size_t msa_len);

/**
 * Get error string
 */
const char *uft_msa_strerror(uft_msa_error_t err);

#ifdef __cplusplus
}
#endif
#endif /* UFT_MSA_H */
