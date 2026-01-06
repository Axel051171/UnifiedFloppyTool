/**
 * @file uft_gcr.h
 * @brief GCR (Group Coded Recording) encoding/decoding for C64 and Mac formats
 * 
 * This module provides GCR encoding and decoding support for:
 * - Commodore 64/128 5-bit GCR (used by 1541/1571/1581 drives)
 * - Apple Macintosh 6+2 GCR (used by 400K/800K drives)
 * 
 * GCR encoding ensures there are never more than two consecutive 0-bits
 * in the encoded data stream, which is required for reliable magnetic
 * flux transitions on floppy disk media.
 * 
 * Source: Extracted from disk2disk and mac2dos projects
 * Original: Copyright (c) 1987-1989 Central Coast Software
 */

#ifndef UFT_GCR_H
#define UFT_GCR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** C64 GCR: 4 bits -> 5 bits encoding */
#define UFT_GCR_C64_BITS_IN     4
#define UFT_GCR_C64_BITS_OUT    5

/** Mac GCR: 6 bits -> 8 bits encoding (6+2 scheme) */
#define UFT_GCR_MAC_BITS_IN     6
#define UFT_GCR_MAC_BITS_OUT    8

/** C64 sector sizes */
#define UFT_C64_SECTOR_SIZE     256
#define UFT_C64_GCR_SECTOR_SIZE 325   /* 260 bytes * 10/8 = 325 GCR bytes */

/** Mac sector sizes */
#define UFT_MAC_SECTOR_SIZE     512
#define UFT_MAC_TAG_SIZE        12
#define UFT_MAC_GCR_SECTOR_SIZE 703   /* Raw GCR sector with tags */

/** Sync patterns */
#define UFT_C64_SYNC_BYTE       0xFF
#define UFT_C64_SYNC_COUNT      5
#define UFT_MAC_SYNC_PATTERN_1  0xD5
#define UFT_MAC_SYNC_PATTERN_2  0xAA
#define UFT_MAC_SYNC_DATA       0xAD
#define UFT_MAC_SYNC_HDR        0x96

/** C64 block types */
#define UFT_C64_BLOCK_HEADER    0x08
#define UFT_C64_BLOCK_DATA      0x07

/** Error codes */
typedef enum {
    UFT_GCR_OK = 0,
    UFT_GCR_ERR_INVALID_CODE,     /**< Invalid GCR code encountered */
    UFT_GCR_ERR_CHECKSUM,         /**< Checksum mismatch */
    UFT_GCR_ERR_SYNC_NOT_FOUND,   /**< Sync pattern not found */
    UFT_GCR_ERR_BUFFER_TOO_SMALL, /**< Output buffer too small */
    UFT_GCR_ERR_INVALID_PARAM,    /**< Invalid parameter */
} uft_gcr_error_t;

/* ============================================================================
 * C64 GCR Encoding/Decoding
 * ============================================================================ */

/**
 * C64 GCR lookup tables (from disk2disk DISK.ASM)
 * 
 * The C64 uses 5-bit GCR where each 4-bit nibble maps to a 5-bit code.
 * Valid codes have at most two consecutive 0-bits.
 */
extern const uint8_t uft_c64_gcr_encode[16];  /**< Nibble -> GCR (shifted left 3) */
extern const int8_t  uft_c64_gcr_decode[32];  /**< GCR -> Nibble (-1 = invalid) */

/**
 * @brief Encode a single C64 nibble to GCR
 * @param nibble 4-bit value (0-15)
 * @return 5-bit GCR code
 */
static inline uint8_t uft_c64_gcr_encode_nibble(uint8_t nibble) {
    return uft_c64_gcr_encode[nibble & 0x0F] >> 3;
}

/**
 * @brief Decode a single C64 GCR code to nibble
 * @param gcr 5-bit GCR code (0-31)
 * @return 4-bit nibble or -1 if invalid
 */
static inline int8_t uft_c64_gcr_decode_nibble(uint8_t gcr) {
    return uft_c64_gcr_decode[gcr & 0x1F];
}

/**
 * @brief Encode C64 sector data to GCR format
 * 
 * Encodes 260 bytes (1 ID + 256 data + 1 checksum + 2 nulls) into
 * 325 bytes of GCR data, preceded by sync bytes.
 * 
 * @param data      Input data buffer (256 bytes)
 * @param gcr_out   Output GCR buffer (must be at least 325 + sync bytes)
 * @param block_id  Block type identifier (0x07 for data)
 * @return UFT_GCR_OK on success
 */
uft_gcr_error_t uft_c64_gcr_encode_sector(
    const uint8_t *data,
    uint8_t *gcr_out,
    uint8_t block_id
);

/**
 * @brief Decode C64 GCR sector data
 * 
 * Decodes 325 bytes of GCR data into 256 bytes of sector data.
 * Verifies the checksum.
 * 
 * @param gcr_in    Input GCR buffer
 * @param data_out  Output data buffer (256 bytes)
 * @param block_id  Expected block type (output)
 * @return UFT_GCR_OK on success, UFT_GCR_ERR_CHECKSUM on checksum error
 */
uft_gcr_error_t uft_c64_gcr_decode_sector(
    const uint8_t *gcr_in,
    uint8_t *data_out,
    uint8_t *block_id
);

/**
 * @brief Encode C64 sector header to GCR
 * 
 * @param track     Track number (1-35/40)
 * @param sector    Sector number (0-20)
 * @param disk_id   2-byte disk ID
 * @param gcr_out   Output buffer for GCR header
 * @return UFT_GCR_OK on success
 */
uft_gcr_error_t uft_c64_gcr_encode_header(
    uint8_t track,
    uint8_t sector,
    const uint8_t disk_id[2],
    uint8_t *gcr_out
);

/**
 * @brief Decode C64 GCR sector header
 * 
 * @param gcr_in    Input GCR header buffer
 * @param track     Decoded track number (output)
 * @param sector    Decoded sector number (output)
 * @param disk_id   Decoded disk ID (output, 2 bytes)
 * @return UFT_GCR_OK on success
 */
uft_gcr_error_t uft_c64_gcr_decode_header(
    const uint8_t *gcr_in,
    uint8_t *track,
    uint8_t *sector,
    uint8_t disk_id[2]
);

/* ============================================================================
 * Mac GCR Encoding/Decoding (6+2 scheme)
 * ============================================================================ */

/**
 * Mac GCR lookup tables (from mac2dos mfmac.asm)
 * 
 * The Mac uses 6+2 GCR where 6-bit values map to valid 8-bit disk bytes.
 * Valid codes have the high bit set and no more than one pair of 0-bits.
 */
extern const uint8_t uft_mac_gcr_encode[64];   /**< 6-bit -> GCR byte */
extern const int8_t  uft_mac_gcr_decode[256];  /**< GCR byte -> 6-bit (-1 = invalid) */

/**
 * @brief Encode a single Mac 6-bit value to GCR
 * @param value 6-bit value (0-63)
 * @return 8-bit GCR disk byte
 */
static inline uint8_t uft_mac_gcr_encode_byte(uint8_t value) {
    return uft_mac_gcr_encode[value & 0x3F];
}

/**
 * @brief Decode a single Mac GCR disk byte
 * @param gcr 8-bit GCR disk byte
 * @return 6-bit value or -1 if invalid
 */
static inline int8_t uft_mac_gcr_decode_byte(uint8_t gcr) {
    return uft_mac_gcr_decode[gcr];
}

/**
 * @brief Encode Mac sector data to GCR format
 * 
 * Encodes 512 bytes of data plus 12 bytes of tag data into
 * GCR format using the Mac's 6+2 encoding with checksums.
 * 
 * @param data      Input data buffer (512 bytes)
 * @param tags      Tag data buffer (12 bytes, can be NULL)
 * @param gcr_out   Output GCR buffer
 * @param sector    Sector number (for tag byte 0)
 * @return UFT_GCR_OK on success
 */
uft_gcr_error_t uft_mac_gcr_encode_sector(
    const uint8_t *data,
    const uint8_t *tags,
    uint8_t *gcr_out,
    uint8_t sector
);

/**
 * @brief Decode Mac GCR sector data
 * 
 * Decodes GCR sector data, verifies checksums, extracts tag data.
 * 
 * @param gcr_in    Input GCR buffer
 * @param data_out  Output data buffer (512 bytes)
 * @param tags_out  Output tag buffer (12 bytes, can be NULL)
 * @return UFT_GCR_OK on success, UFT_GCR_ERR_CHECKSUM on error
 */
uft_gcr_error_t uft_mac_gcr_decode_sector(
    const uint8_t *gcr_in,
    uint8_t *data_out,
    uint8_t *tags_out
);

/**
 * @brief Encode Mac sector header
 * 
 * @param track     Track number (0-79)
 * @param sector    Sector number (0-11)
 * @param side      Side (0 or 1)
 * @param format    Format byte (0x22 = double-sided)
 * @param gcr_out   Output buffer for header
 * @return UFT_GCR_OK on success
 */
uft_gcr_error_t uft_mac_gcr_encode_header(
    uint8_t track,
    uint8_t sector,
    uint8_t side,
    uint8_t format,
    uint8_t *gcr_out
);

/**
 * @brief Decode Mac sector header
 * 
 * @param gcr_in    Input GCR header buffer
 * @param track     Decoded track (output)
 * @param sector    Decoded sector (output)
 * @param side      Decoded side (output)
 * @param format    Decoded format byte (output)
 * @return UFT_GCR_OK on success
 */
uft_gcr_error_t uft_mac_gcr_decode_header(
    const uint8_t *gcr_in,
    uint8_t *track,
    uint8_t *sector,
    uint8_t *side,
    uint8_t *format
);

/* ============================================================================
 * Track-Level Operations
 * ============================================================================ */

/**
 * @brief Find sync pattern in raw track data
 * 
 * Searches for the specified sync pattern in raw track data.
 * 
 * @param track_data    Raw track data buffer
 * @param track_len     Length of track data
 * @param pattern       Sync pattern to find
 * @param pattern_len   Length of pattern
 * @param start_offset  Starting offset for search
 * @return Offset of sync pattern, or -1 if not found
 */
ssize_t uft_gcr_find_sync(
    const uint8_t *track_data,
    size_t track_len,
    const uint8_t *pattern,
    size_t pattern_len,
    size_t start_offset
);

/**
 * @brief C64 track parameters
 */
typedef struct {
    uint8_t sectors_per_track;
    uint16_t raw_track_size;     /**< Bytes per track (raw) */
    uint16_t gap_size;           /**< Gap between sectors */
    uint16_t bit_rate;           /**< Bit timing */
} uft_c64_track_params_t;

/**
 * @brief Get C64 track parameters
 * @param track Track number (1-35 or 1-40)
 * @param params Output parameters
 * @return UFT_GCR_OK on success
 */
uft_gcr_error_t uft_c64_get_track_params(uint8_t track, uft_c64_track_params_t *params);

/**
 * @brief C64 track zones and sectors per track
 * 
 * Track 1-17:  21 sectors, speed zone 3
 * Track 18-24: 19 sectors, speed zone 2
 * Track 25-30: 18 sectors, speed zone 1
 * Track 31-35: 17 sectors, speed zone 0
 */
extern const uft_c64_track_params_t uft_c64_track_table[4];

#ifdef __cplusplus
}
#endif

#endif /* UFT_GCR_H */
