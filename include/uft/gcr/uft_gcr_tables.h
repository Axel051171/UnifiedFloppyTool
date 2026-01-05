/**
 * @file uft_gcr_tables.h
 * @brief Comprehensive GCR Encoding Tables for UFT
 * 
 * Contains all known GCR encoding variants:
 * - Commodore GCR5 (4-to-5, C64/1541/1571)
 * - Apple II GCR6 (6-and-2, Macintosh)
 * - Micropolis GCR (4b/5b, 0,2 RLL)
 * - Victor 9000 GCR
 * 
 * Sources:
 * - MAME flopimg.cpp (BSD-3-Clause)
 * - Wikipedia "Group coded recording"
 * - Commodore 1571 ROM
 * 
 * @copyright UFT Project 2026
 */

#ifndef UFT_GCR_TABLES_H
#define UFT_GCR_TABLES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Common GCR Status Codes
 *============================================================================*/

typedef enum uft_gcr_status {
    UFT_GCR_OK          = 0,
    UFT_GCR_E_INVALID   = 1,    /**< Invalid input */
    UFT_GCR_E_NOMEM     = 2,    /**< Memory allocation failed */
    UFT_GCR_E_BUF       = 3,    /**< Buffer too small */
    UFT_GCR_E_DECODE    = 4     /**< Decoding error (invalid symbol) */
} uft_gcr_status_t;

/*============================================================================
 * Commodore GCR5 (4-to-5 encoding)
 *============================================================================*/

/**
 * @brief Commodore GCR5 forward table (nibble → 5-bit symbol)
 * 
 * Used by: C64, 1541, 1571, 1581, VIC-1540
 * 
 * Nibble | GCR   | Binary
 * -------|-------|--------
 *  0x0   | 0x0A  | 01010
 *  0x1   | 0x0B  | 01011
 *  0x2   | 0x12  | 10010
 *  0x3   | 0x13  | 10011
 *  0x4   | 0x0E  | 01110
 *  0x5   | 0x0F  | 01111
 *  0x6   | 0x16  | 10110
 *  0x7   | 0x17  | 10111
 *  0x8   | 0x09  | 01001
 *  0x9   | 0x19  | 11001
 *  0xA   | 0x1A  | 11010
 *  0xB   | 0x1B  | 11011
 *  0xC   | 0x0D  | 01101
 *  0xD   | 0x1D  | 11101
 *  0xE   | 0x1E  | 11110
 *  0xF   | 0x15  | 10101
 */
extern const uint8_t uft_gcr5_encode[16];

/**
 * @brief Commodore GCR5 reverse table (5-bit symbol → nibble)
 * 
 * Invalid symbols return 0x00 (use uft_gcr5_valid[] to check)
 */
extern const uint8_t uft_gcr5_decode[32];

/**
 * @brief GCR5 validity table
 * 
 * 1 = valid symbol, 0 = invalid
 */
extern const uint8_t uft_gcr5_valid[32];

/*============================================================================
 * Apple II / Macintosh GCR6 (6-and-2 encoding)
 *============================================================================*/

/**
 * @brief Apple GCR6 forward table (6-bit value → disk byte)
 * 
 * Used by: Apple II (16-sector), Macintosh 400K/800K
 * Valid disk bytes range from 0x96 to 0xFF
 */
extern const uint8_t uft_gcr6_encode[64];

/**
 * @brief Apple GCR6 reverse table (disk byte → 6-bit value)
 * 
 * Index: disk byte (0x00-0xFF)
 * Value: 6-bit value or 0xFF for invalid
 */
extern const uint8_t uft_gcr6_decode[256];

/*============================================================================
 * Micropolis / Generic 4b/5b GCR (0,2 RLL)
 *============================================================================*/

/**
 * @brief Micropolis 4b/5b forward table
 * 
 * Different from Commodore GCR5! Used by Micropolis drives.
 * Wikipedia "Group coded recording" table.
 */
extern const uint8_t uft_gcr_4b5b_encode[16];

/**
 * @brief Micropolis 4b/5b reverse table
 */
extern const uint8_t uft_gcr_4b5b_decode[32];

/*============================================================================
 * Commodore Zone Definitions
 *============================================================================*/

/**
 * @brief C64/1541 speed zone by track
 * 
 * Zone | Tracks  | Sectors | Cell (µs) | Cell (ticks@16MHz)
 * -----|---------|---------|-----------|-------------------
 *  3   | 1-17    | 21      | 3.25      | 3250
 *  2   | 18-24   | 19      | 3.50      | 3500
 *  1   | 25-30   | 18      | 3.75      | 3750
 *  0   | 31-42   | 17      | 4.00      | 4000
 */
extern const uint8_t uft_c64_speed_zone[42];

/**
 * @brief C64/1541 sectors per track
 */
extern const uint8_t uft_c64_sectors_per_track[42];

/**
 * @brief C64/1541 cell size per zone (in 1/10 µs)
 */
extern const uint16_t uft_c64_cell_size[4];

/*============================================================================
 * Encoding/Decoding Functions
 *============================================================================*/

/**
 * @brief Encode nibble using Commodore GCR5
 */
static inline uint8_t uft_gcr5_encode_nibble(uint8_t nibble) {
    return uft_gcr5_encode[nibble & 0x0F];
}

/**
 * @brief Decode GCR5 symbol to nibble
 * @return Nibble value or 0xFF if invalid
 */
static inline uint8_t uft_gcr5_decode_symbol(uint8_t symbol) {
    return uft_gcr5_valid[symbol & 0x1F] ? uft_gcr5_decode[symbol & 0x1F] : 0xFF;
}

/**
 * @brief Check if GCR5 symbol is valid
 */
static inline bool uft_gcr5_is_valid(uint8_t symbol) {
    return uft_gcr5_valid[symbol & 0x1F] != 0;
}

/**
 * @brief Encode byte using GCR5 (produces 10 bits)
 * @param byte Input byte
 * @param out Output: two 5-bit symbols packed as uint16_t
 */
static inline uint16_t uft_gcr5_encode_byte(uint8_t byte) {
    uint16_t hi = uft_gcr5_encode[(byte >> 4) & 0x0F];
    uint16_t lo = uft_gcr5_encode[byte & 0x0F];
    return (hi << 5) | lo;
}

/**
 * @brief Encode 4 bytes to 5 GCR bytes (Commodore format)
 */
void uft_gcr5_encode_4to5(const uint8_t in[4], uint8_t out[5]);

/**
 * @brief Decode 5 GCR bytes to 4 data bytes
 * @return Number of invalid symbols encountered
 */
int uft_gcr5_decode_5to4(const uint8_t in[5], uint8_t out[4]);

/**
 * @brief Encode using Apple GCR6
 */
static inline uint8_t uft_gcr6_encode_value(uint8_t val) {
    return uft_gcr6_encode[val & 0x3F];
}

/**
 * @brief Decode Apple GCR6 disk byte
 * @return 6-bit value or 0xFF if invalid
 */
static inline uint8_t uft_gcr6_decode_byte(uint8_t disk_byte) {
    return uft_gcr6_decode[disk_byte];
}

/**
 * @brief Encode 3 bytes using Apple GCR6 (produces 4 disk bytes)
 */
void uft_gcr6_encode_3to4(uint8_t a, uint8_t b, uint8_t c, uint8_t out[4]);

/**
 * @brief Decode 4 Apple GCR6 disk bytes to 3 data bytes
 */
void uft_gcr6_decode_4to3(const uint8_t in[4], uint8_t *a, uint8_t *b, uint8_t *c);

/**
 * @brief Encode using Micropolis 4b/5b GCR
 */
uft_gcr_status_t uft_gcr_4b5b_encode_bytes(const uint8_t *in, size_t in_len,
                                            uint8_t *out, size_t *out_len);

/**
 * @brief Decode Micropolis 4b/5b GCR
 */
uft_gcr_status_t uft_gcr_4b5b_decode_bytes(const uint8_t *in, size_t in_len,
                                            uint8_t *out, size_t *out_len);

/**
 * @brief Calculate encoded size for 4b/5b
 */
static inline size_t uft_gcr_4b5b_encoded_size(size_t in_len) {
    return (in_len * 10 + 7) / 8;
}

/*============================================================================
 * Track Utility Functions
 *============================================================================*/

/**
 * @brief Get speed zone for C64 track
 * @param track Track number (1-42)
 * @return Zone (0-3) or 0 for invalid
 */
static inline uint8_t uft_c64_get_zone(uint8_t track) {
    if (track < 1 || track > 42) return 0;
    return uft_c64_speed_zone[track - 1];
}

/**
 * @brief Get sectors per track for C64
 * @param track Track number (1-42)
 * @return Sectors (17-21) or 0 for invalid
 */
static inline uint8_t uft_c64_get_sectors(uint8_t track) {
    if (track < 1 || track > 42) return 0;
    return uft_c64_sectors_per_track[track - 1];
}

/**
 * @brief Get cell size for C64 track (in 1/10 µs)
 */
static inline uint16_t uft_c64_get_cell_size(uint8_t track) {
    return uft_c64_cell_size[uft_c64_get_zone(track)];
}

/**
 * @brief Calculate XOR checksum (Commodore style)
 */
uint8_t uft_gcr_checksum_xor(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GCR_TABLES_H */
