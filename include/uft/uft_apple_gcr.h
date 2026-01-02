/**
 * @file uft_apple_gcr.h
 * @brief Apple GCR (Group Coded Recording) Encoding/Decoding
 * @version 3.1.4.003
 *
 * Apple-specific GCR encoding used in:
 * - Apple II 5.25" disks (6-and-2 encoding)
 * - Macintosh 3.5" disks (modified GCR)
 * - Lisa/Twiggy disks
 *
 * Key characteristics:
 * - Self-clocking (no separate clock bits like MFM)
 * - 6-and-2 encoding: 6 bits per disk byte + 2 bits checksum
 * - Nibble encoding: 8-bit bytes → encoded nibbles
 * - No consecutive zero bits (max 2 zeros between ones)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_APPLE_GCR_H
#define UFT_APPLE_GCR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Apple II 5.25" GCR Constants
 *============================================================================*/

/** Sync byte (self-sync) */
#define UFT_APPLE_SYNC_BYTE     0xFF

/** Address field prologue */
#define UFT_APPLE_ADDR_PROLOGUE_D5  0xD5
#define UFT_APPLE_ADDR_PROLOGUE_AA  0xAA
#define UFT_APPLE_ADDR_PROLOGUE_96  0x96

/** Data field prologue */
#define UFT_APPLE_DATA_PROLOGUE_D5  0xD5
#define UFT_APPLE_DATA_PROLOGUE_AA  0xAA
#define UFT_APPLE_DATA_PROLOGUE_AD  0xAD

/** Field epilogue */
#define UFT_APPLE_EPILOGUE_DE      0xDE
#define UFT_APPLE_EPILOGUE_AA      0xAA
#define UFT_APPLE_EPILOGUE_EB      0xEB

/** Sector size */
#define UFT_APPLE_SECTOR_SIZE      256

/** Number of sectors per track (DOS 3.3 / ProDOS) */
#define UFT_APPLE_SECTORS_16       16

/** Number of sectors per track (DOS 3.2) */
#define UFT_APPLE_SECTORS_13       13

/*============================================================================
 * 6-and-2 Encoding Tables
 *============================================================================*/

/**
 * @brief Disk byte to 6-bit value
 *
 * Valid disk bytes have exactly one of each bit pattern.
 * Invalid entries are 0xFF.
 */
extern const uint8_t UFT_APPLE_DECODE_62[256];

/**
 * @brief 6-bit value to disk byte
 *
 * 64 entries mapping 0-63 to valid disk bytes.
 */
extern const uint8_t UFT_APPLE_ENCODE_62[64];

/*============================================================================
 * 5-and-3 Encoding Tables (DOS 3.2)
 *============================================================================*/

/**
 * @brief Disk byte to 5-bit value (DOS 3.2)
 */
extern const uint8_t UFT_APPLE_DECODE_53[256];

/**
 * @brief 5-bit value to disk byte (DOS 3.2)
 */
extern const uint8_t UFT_APPLE_ENCODE_53[32];

/*============================================================================
 * Macintosh 3.5" GCR Constants
 *============================================================================*/

/** Mac GCR sync pattern */
#define UFT_MAC_SYNC_BYTE       0xFF

/** Mac address mark */
#define UFT_MAC_ADDR_MARK_1     0xD5
#define UFT_MAC_ADDR_MARK_2     0xAA
#define UFT_MAC_ADDR_MARK_3     0x96

/** Mac data mark */
#define UFT_MAC_DATA_MARK_1     0xD5
#define UFT_MAC_DATA_MARK_2     0xAA
#define UFT_MAC_DATA_MARK_3     0xAD

/** Mac sector sizes by zone */
#define UFT_MAC_ZONE_0_SPT      12  /**< Tracks 0-15: 12 sectors */
#define UFT_MAC_ZONE_1_SPT      11  /**< Tracks 16-31: 11 sectors */
#define UFT_MAC_ZONE_2_SPT      10  /**< Tracks 32-47: 10 sectors */
#define UFT_MAC_ZONE_3_SPT       9  /**< Tracks 48-63: 9 sectors */
#define UFT_MAC_ZONE_4_SPT       8  /**< Tracks 64-79: 8 sectors */

/** Mac sector data size */
#define UFT_MAC_SECTOR_SIZE     512

/** Mac tag bytes per sector */
#define UFT_MAC_TAG_SIZE        12

/*============================================================================
 * Address Field Structure
 *============================================================================*/

/** Apple II address field */
typedef struct {
    uint8_t     volume;     /**< Volume number (odd-even encoded) */
    uint8_t     track;      /**< Track number */
    uint8_t     sector;     /**< Sector number */
    uint8_t     checksum;   /**< XOR checksum */
} uft_apple_address_t;

/** Macintosh address field */
typedef struct {
    uint8_t     track;      /**< Track number */
    uint8_t     sector;     /**< Sector number */
    uint8_t     side;       /**< Side (0 or 1) */
    uint8_t     format;     /**< Format byte (interleave info) */
    uint8_t     checksum;   /**< Checksum */
} uft_mac_address_t;

/*============================================================================
 * Sector Data Structures
 *============================================================================*/

/** Apple II decoded sector */
typedef struct {
    uft_apple_address_t address;
    uint8_t     data[UFT_APPLE_SECTOR_SIZE];
    bool        address_valid;
    bool        data_valid;
    bool        checksum_ok;
} uft_apple_sector_t;

/** Macintosh decoded sector */
typedef struct {
    uft_mac_address_t address;
    uint8_t     tag[UFT_MAC_TAG_SIZE];
    uint8_t     data[UFT_MAC_SECTOR_SIZE];
    bool        address_valid;
    bool        data_valid;
    bool        checksum_ok;
} uft_mac_sector_t;

/*============================================================================
 * Encoding/Decoding Functions
 *============================================================================*/

/**
 * @brief Decode 6-and-2 encoded sector data
 *
 * @param encoded Encoded nibbles (342 + 1 checksum)
 * @param decoded Output: 256 decoded bytes
 * @return true if checksum valid
 */
bool uft_apple_decode_62_sector(const uint8_t *encoded, uint8_t *decoded);

/**
 * @brief Encode sector data to 6-and-2 format
 *
 * @param data 256 bytes of sector data
 * @param encoded Output: 343 encoded nibbles
 */
void uft_apple_encode_62_sector(const uint8_t *data, uint8_t *encoded);

/**
 * @brief Decode 5-and-3 encoded sector data (DOS 3.2)
 *
 * @param encoded Encoded nibbles
 * @param decoded Output: 256 decoded bytes
 * @return true if checksum valid
 */
bool uft_apple_decode_53_sector(const uint8_t *encoded, uint8_t *decoded);

/**
 * @brief Decode odd-even encoded byte pair
 *
 * Apple II address fields use odd-even encoding.
 *
 * @param odd Odd bits byte
 * @param even Even bits byte
 * @return Decoded value
 */
static inline uint8_t uft_apple_decode_odd_even(uint8_t odd, uint8_t even) {
    return ((odd & 0x55) << 1) | (even & 0x55);
}

/**
 * @brief Encode byte to odd-even format
 *
 * @param value Byte to encode
 * @param odd Output: odd bits
 * @param even Output: even bits
 */
static inline void uft_apple_encode_odd_even(uint8_t value, uint8_t *odd, uint8_t *even) {
    *odd  = ((value >> 1) & 0x55) | 0xAA;
    *even = (value & 0x55) | 0xAA;
}

/*============================================================================
 * Bitstream Parsing
 *============================================================================*/

/**
 * @brief Find next sync pattern in bitstream
 *
 * @param bits Bitstream data
 * @param bit_count Total bits
 * @param start_bit Starting position
 * @param sync_count Minimum consecutive FF bytes (typically 5)
 * @return Bit position after sync, or -1 if not found
 */
int32_t uft_apple_find_sync(const uint8_t *bits, uint32_t bit_count,
                            uint32_t start_bit, uint8_t sync_count);

/**
 * @brief Read nibble from bitstream
 *
 * Reads 8 bits starting at bit position, MSB first.
 *
 * @param bits Bitstream data
 * @param bit_pos Bit position
 * @return Nibble value
 */
uint8_t uft_apple_read_nibble(const uint8_t *bits, uint32_t bit_pos);

/**
 * @brief Decode address field from bitstream
 *
 * @param bits Bitstream data
 * @param bit_pos Starting position (after D5 AA 96)
 * @param addr Output: decoded address
 * @return true if valid
 */
bool uft_apple_decode_address(const uint8_t *bits, uint32_t bit_pos,
                               uft_apple_address_t *addr);

/**
 * @brief Find and decode all sectors in track
 *
 * @param bits Track bitstream
 * @param bit_count Number of bits
 * @param sectors Output array (16 or 13 elements)
 * @param max_sectors Maximum sectors to find
 * @return Number of sectors found
 */
int uft_apple_decode_track(const uint8_t *bits, uint32_t bit_count,
                           uft_apple_sector_t *sectors, int max_sectors);

/*============================================================================
 * Track Encoding
 *============================================================================*/

/**
 * @brief Encode sectors to track bitstream
 *
 * Creates a complete track with sync, address fields, and data fields.
 *
 * @param sectors Array of sectors (must have valid data)
 * @param sector_count Number of sectors (13 or 16)
 * @param volume Volume number
 * @param track Track number
 * @param bits Output bitstream buffer
 * @param max_bits Maximum bits in buffer
 * @return Actual bits written
 */
uint32_t uft_apple_encode_track(const uft_apple_sector_t *sectors,
                                int sector_count, uint8_t volume,
                                uint8_t track, uint8_t *bits,
                                uint32_t max_bits);

/*============================================================================
 * Macintosh GCR Functions
 *============================================================================*/

/**
 * @brief Get sectors per track for Mac zone
 *
 * @param track Track number (0-79)
 * @return Sectors per track
 */
int uft_mac_sectors_for_track(int track);

/**
 * @brief Decode Macintosh GCR sector
 *
 * @param encoded Encoded data
 * @param tag Output: 12-byte tag
 * @param data Output: 512-byte data
 * @return true if valid
 */
bool uft_mac_decode_sector(const uint8_t *encoded,
                           uint8_t *tag, uint8_t *data);

/**
 * @brief Encode Macintosh GCR sector
 *
 * @param tag 12-byte tag data
 * @param data 512-byte sector data
 * @param encoded Output: encoded nibbles
 */
void uft_mac_encode_sector(const uint8_t *tag, const uint8_t *data,
                           uint8_t *encoded);

/*============================================================================
 * Interleave Tables
 *============================================================================*/

/** DOS 3.3 physical-to-logical sector mapping */
extern const uint8_t UFT_APPLE_INTERLEAVE_DOS33[16];

/** ProDOS physical-to-logical sector mapping */
extern const uint8_t UFT_APPLE_INTERLEAVE_PRODOS[16];

/** DOS 3.2 physical-to-logical sector mapping */
extern const uint8_t UFT_APPLE_INTERLEAVE_DOS32[13];

/** CP/M physical-to-logical sector mapping */
extern const uint8_t UFT_APPLE_INTERLEAVE_CPM[16];

#ifdef __cplusplus
}
#endif

#endif /* UFT_APPLE_GCR_H */
