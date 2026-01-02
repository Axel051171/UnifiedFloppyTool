/**
 * @file uft_gcr.h
 * @brief GCR (Group Code Recording) Decoder for Commodore 1541/1571/1581
 * 
 * Implements complete GCR→Sector decoding for C64/C128 disk formats.
 * Handles variable speed zones, sync detection, and error correction.
 * 
 * GCR Encoding:
 * - 4 data bits → 5 GCR bits (expansion encoding)
 * - Self-clocking, no separate clock signal needed
 * - C64 1541: 4 speed zones (tracks 1-35)
 * 
 * Speed Zones (1541):
 * - Zone 0: Tracks 31-35, 17 sectors, 3.25 MHz bitrate
 * - Zone 1: Tracks 25-30, 18 sectors, 3.00 MHz  
 * - Zone 2: Tracks 18-24, 19 sectors, 2.75 MHz
 * - Zone 3: Tracks  1-17, 21 sectors, 2.50 MHz
 * 
 * @version 2.10.0
 * @date 2024-12-26
 */

#ifndef UFT_GCR_H
#define UFT_GCR_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GCR nibble lookup table
 * 
 * 4-bit value → 5-bit GCR code
 */
extern const uint8_t uft_gcr_encode_table[16];

/**
 * @brief GCR decode lookup table
 * 
 * 5-bit GCR code → 4-bit value (0xFF = invalid)
 */
extern const uint8_t uft_gcr_decode_table[32];

/**
 * @brief C64 1541 speed zone configuration
 */
typedef struct {
    uint8_t zone_id;           /**< Zone number (0-3) */
    uint8_t first_track;       /**< First track in zone (1-based) */
    uint8_t last_track;        /**< Last track in zone */
    uint8_t sectors_per_track; /**< Sectors in this zone */
    uint32_t bitrate_hz;       /**< Nominal bitrate */
    uint32_t cell_ns;          /**< Nominal bit cell time (nanoseconds) */
} uft_gcr_speed_zone_t;

/**
 * @brief C64 1541 speed zones (4 zones)
 */
extern const uft_gcr_speed_zone_t uft_c64_speed_zones[4];

/**
 * @brief GCR sector header
 * 
 * Decoded from header block (SYNC + 0x08 + checksum)
 */
typedef struct {
    uint8_t checksum;    /**< Header checksum */
    uint8_t sector;      /**< Sector ID (0-20) */
    uint8_t track;       /**< Track number (1-35) */
    uint8_t id2;         /**< Disk ID byte 2 */
    uint8_t id1;         /**< Disk ID byte 1 */
} uft_gcr_header_t;

/**
 * @brief GCR sector data block
 * 
 * 256 bytes of decoded data + checksum
 */
typedef struct {
    uint8_t data[256];   /**< Sector data */
    uint8_t checksum;    /**< Data checksum */
} uft_gcr_data_block_t;

/**
 * @brief Decoded GCR sector (header + data)
 */
typedef struct {
    uft_gcr_header_t header;      /**< Sector header */
    uft_gcr_data_block_t data;    /**< Sector data */
    bool header_valid;            /**< Header checksum OK */
    bool data_valid;              /**< Data checksum OK */
    uint32_t weak_bits;           /**< Weak bit count detected */
} uft_gcr_sector_t;

/**
 * @brief GCR track decode result
 */
typedef struct {
    uint8_t track_num;            /**< Track number (1-35) */
    uint8_t sectors_found;        /**< Number of sectors decoded */
    uft_gcr_sector_t sectors[21]; /**< Max 21 sectors per track */
    uint32_t sync_marks_found;    /**< Total sync marks detected */
    uint32_t bitstream_length;    /**< Total bits in track */
} uft_gcr_track_t;

/**
 * @brief GCR decoder context
 */
typedef struct {
    /** Current track being decoded */
    uint8_t current_track;
    
    /** Speed zone for current track */
    const uft_gcr_speed_zone_t* speed_zone;
    
    /** Flux timing profile */
    uint32_t nominal_cell_ns;
    uint32_t tolerance_ns;
    
    /** Statistics */
    uint32_t total_flux_reversals;
    uint32_t total_bits_decoded;
    uint32_t sync_marks_found;
    uint32_t sectors_decoded;
    uint32_t checksum_errors;
    
    /** Error context */
    uft_error_ctx_t error;
} uft_gcr_ctx_t;

/**
 * @brief Create GCR decoder context
 * 
 * @param[out] ctx Pointer to receive context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_gcr_create(uft_gcr_ctx_t** ctx);

/**
 * @brief Destroy GCR decoder context
 * 
 * @param[in,out] ctx Context to destroy
 */
void uft_gcr_destroy(uft_gcr_ctx_t** ctx);

/**
 * @brief Encode 4-bit nibble to 5-bit GCR
 * 
 * @param nibble 4-bit value (0-15)
 * @return 5-bit GCR code (0-31), or 0xFF if invalid
 */
static inline uint8_t uft_gcr_encode_nibble(uint8_t nibble) {
    return (nibble < 16) ? uft_gcr_encode_table[nibble] : 0xFF;
}

/**
 * @brief Decode 5-bit GCR to 4-bit nibble
 * 
 * @param gcr 5-bit GCR code (0-31)
 * @return 4-bit value (0-15), or 0xFF if invalid code
 */
static inline uint8_t uft_gcr_decode_nibble(uint8_t gcr) {
    return (gcr < 32) ? uft_gcr_decode_table[gcr] : 0xFF;
}

/**
 * @brief Encode byte to GCR (2 bytes output)
 * 
 * Encodes one byte as two GCR bytes (high nibble first).
 * 
 * @param byte Input byte
 * @param[out] gcr_out 2-byte GCR output
 * @return UFT_SUCCESS or UFT_ERR_INVALID_ARG
 */
uft_rc_t uft_gcr_encode_byte(uint8_t byte, uint8_t gcr_out[2]);

/**
 * @brief Decode 2 GCR bytes to 1 data byte
 * 
 * @param gcr_in 2-byte GCR input
 * @param[out] byte_out Decoded byte
 * @return UFT_SUCCESS or UFT_ERR_CORRUPTED if invalid GCR
 */
uft_rc_t uft_gcr_decode_byte(const uint8_t gcr_in[2], uint8_t* byte_out);

/**
 * @brief Get speed zone for track number
 * 
 * @param track Track number (1-35 for 1541)
 * @return Speed zone configuration, or NULL if invalid track
 */
const uft_gcr_speed_zone_t* uft_gcr_get_speed_zone(uint8_t track);

/**
 * @brief Find sync mark in bitstream
 * 
 * Searches for 10+ consecutive '1' bits (0x3FF+).
 * 
 * @param bitstream Bitstream to search
 * @param bit_count Total bits available
 * @param start_bit Start search position
 * @param[out] sync_pos Position of sync mark (if found)
 * @return UFT_SUCCESS if found, UFT_ERR_NOT_FOUND if not
 */
uft_rc_t uft_gcr_find_sync(
    const uint8_t* bitstream,
    uint32_t bit_count,
    uint32_t start_bit,
    uint32_t* sync_pos
);

/**
 * @brief Decode GCR sector header (after sync)
 * 
 * Expects: 0x08 + header_data[10] (5 GCR bytes → header)
 * 
 * @param bitstream Bitstream positioned after sync
 * @param[out] header Decoded header
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_gcr_decode_header(
    const uint8_t* bitstream,
    uft_gcr_header_t* header
);

/**
 * @brief Decode GCR sector data block (after sync)
 * 
 * Expects: 0x07 + data[325] (256 bytes + checksum in GCR)
 * 
 * @param bitstream Bitstream positioned after sync
 * @param[out] data Decoded data block
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_gcr_decode_data(
    const uint8_t* bitstream,
    uft_gcr_data_block_t* data
);

/**
 * @brief Decode entire GCR track from bitstream
 * 
 * Primary decode function - extracts all sectors from track.
 * 
 * @param[in] ctx GCR context
 * @param track_num Track number (1-35)
 * @param bitstream Raw GCR bitstream
 * @param bit_count Bitstream length in bits
 * @param[out] track Decoded track with sectors
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_gcr_decode_track(
    uft_gcr_ctx_t* ctx,
    uint8_t track_num,
    const uint8_t* bitstream,
    uint32_t bit_count,
    uft_gcr_track_t* track
);

/**
 * @brief Decode flux transitions to GCR bitstream
 * 
 * Converts raw flux timing data to GCR bits.
 * 
 * @param[in] ctx GCR context
 * @param flux_ns Flux transition times (nanoseconds)
 * @param flux_count Number of transitions
 * @param[out] bitstream Output bitstream buffer
 * @param bitstream_size Buffer size in bytes
 * @param[out] bits_decoded Number of bits decoded
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_gcr_flux_to_bitstream(
    uft_gcr_ctx_t* ctx,
    const uint32_t* flux_ns,
    uint32_t flux_count,
    uint8_t* bitstream,
    size_t bitstream_size,
    uint32_t* bits_decoded
);

/**
 * @brief Complete pipeline: Flux → D64 sectors
 * 
 * High-level function combining flux decode + GCR decode.
 * 
 * @param[in] ctx GCR context
 * @param track_num Track number
 * @param flux_ns Flux timing data
 * @param flux_count Flux transition count
 * @param[out] track Decoded track
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_gcr_decode_track_from_flux(
    uft_gcr_ctx_t* ctx,
    uint8_t track_num,
    const uint32_t* flux_ns,
    uint32_t flux_count,
    uft_gcr_track_t* track
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GCR_H */
