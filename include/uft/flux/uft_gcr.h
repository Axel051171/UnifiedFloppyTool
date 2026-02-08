/**
 * @file uft_gcr.h
 * @brief Group Coded Recording (GCR) Support
 * 
 * Supports Commodore and Apple II GCR encoding/decoding.
 * 
 * Based on SuperDiskIndex by Henrik Akesson (MIT)
 */

#ifndef UFT_GCR_H
#define UFT_GCR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Commodore GCR (4-to-5 bit encoding)
 *===========================================================================*/

/**
 * @brief Commodore GCR 4-to-5 encoding table
 * 
 * Maps 4-bit nibble to 5-bit GCR value.
 * Design ensures:
 * - No more than 2 consecutive zeros
 * - No more than 8 consecutive ones
 */
static const uint8_t uft_gcr_cbm_encode[16] = {
    0x0A,  /* 0x0 -> 01010 */
    0x0B,  /* 0x1 -> 01011 */
    0x12,  /* 0x2 -> 10010 */
    0x13,  /* 0x3 -> 10011 */
    0x0E,  /* 0x4 -> 01110 */
    0x0F,  /* 0x5 -> 01111 */
    0x16,  /* 0x6 -> 10110 */
    0x17,  /* 0x7 -> 10111 */
    0x09,  /* 0x8 -> 01001 */
    0x19,  /* 0x9 -> 11001 */
    0x1A,  /* 0xA -> 11010 */
    0x1B,  /* 0xB -> 11011 */
    0x0D,  /* 0xC -> 01101 */
    0x1D,  /* 0xD -> 11101 */
    0x1E,  /* 0xE -> 11110 */
    0x15   /* 0xF -> 10101 */
};

/**
 * @brief Commodore GCR 5-to-4 decoding table
 * 
 * 0xFF indicates invalid GCR value.
 */
static const uint8_t uft_gcr_cbm_decode[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 00-07: invalid */
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  /* 08-0F */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  /* 10-17 */
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   /* 18-1F */
};

/**
 * @brief Encode Commodore GCR nibble
 */
static inline uint8_t uft_gcr_cbm_encode_nibble(uint8_t nibble) {
    return (nibble <= 0x0F) ? uft_gcr_cbm_encode[nibble] : 0;
}

/**
 * @brief Decode Commodore GCR value
 * @return Decoded nibble, or 0xFF if invalid
 */
static inline uint8_t uft_gcr_cbm_decode_value(uint8_t gcr) {
    return (gcr <= 0x1F) ? uft_gcr_cbm_decode[gcr] : 0xFF;
}

/**
 * @brief Encode 4 bytes to 5 GCR bytes (Commodore)
 * @param input 4 input bytes
 * @param output 5 output bytes
 */
void uft_gcr_cbm_encode_block(const uint8_t *input, uint8_t *output);

/**
 * @brief Decode 5 GCR bytes to 4 bytes (Commodore)
 * @param input 5 input bytes
 * @param output 4 output bytes
 * @return true if all GCR values valid
 */
bool uft_gcr_cbm_decode_block(const uint8_t *input, uint8_t *output);

/*===========================================================================
 * Apple II GCR (6-and-2 encoding)
 *===========================================================================*/

/**
 * @brief Apple II 6-and-2 encoding table
 * 
 * Maps 6-bit value to disk byte.
 * All disk bytes have high bit set and no more than
 * one pair of consecutive zero bits.
 */
static const uint8_t uft_gcr_apple_encode[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/**
 * @brief Apple II 6-and-2 decoding table (256 entries)
 * 
 * Maps disk byte to 6-bit value. Invalid = 0xFF.
 */
extern const uint8_t uft_gcr_apple_decode[256];

/**
 * @brief Encode Apple II 6-and-2 value
 */
static inline uint8_t uft_gcr_apple_encode_value(uint8_t value) {
    return (value <= 0x3F) ? uft_gcr_apple_encode[value] : 0;
}

/**
 * @brief Decode Apple II 6-and-2 disk byte
 * @return Decoded value (0-63), or 0xFF if invalid
 */
static inline uint8_t uft_gcr_apple_decode_value(uint8_t disk_byte) {
    return uft_gcr_apple_decode[disk_byte];
}

/*===========================================================================
 * Commodore 1541 Sector Handling
 *===========================================================================*/

/** Sync mark value (minimum 10 bytes of 0xFF) */
#define UFT_C64_SYNC_BYTE       0xFF
#define UFT_C64_SYNC_MIN_LEN    10

/** Block type markers */
#define UFT_C64_BLOCK_HEADER    0x08
#define UFT_C64_BLOCK_DATA      0x07

/** Sector sizes */
#define UFT_C64_HEADER_RAW_SIZE  10   /**< GCR bytes (decoded: 8) */
#define UFT_C64_DATA_RAW_SIZE   325   /**< GCR bytes (decoded: 260) */
#define UFT_C64_SECTOR_SIZE     256   /**< Data bytes per sector */

/**
 * @brief 1541 zone speed table
 * 
 * Commodore drives vary bit rate by track zone.
 */
typedef struct {
    int first_track;      /**< First track in zone (1-based) */
    int last_track;       /**< Last track in zone (1-based) */
    int sectors;          /**< Sectors per track */
    float bit_time_us;    /**< Bit cell time in microseconds */
} uft_c64_zone_t;

static const uft_c64_zone_t uft_c64_zones[4] = {
    {  1, 17, 21, 3.25f },  /* Zone 1: tracks 1-17 */
    { 18, 24, 19, 3.50f },  /* Zone 2: tracks 18-24 */
    { 25, 30, 18, 3.75f },  /* Zone 3: tracks 25-30 */
    { 31, 35, 17, 4.00f }   /* Zone 4: tracks 31-35 */
};

/**
 * @brief Get zone for track
 * @param track Track number (1-based)
 * @return Zone (0-3), or -1 if invalid
 */
static inline int uft_c64_get_zone(int track) {
    if (track >= 1 && track <= 17) return 0;
    if (track >= 18 && track <= 24) return 1;
    if (track >= 25 && track <= 30) return 2;
    if (track >= 31 && track <= 35) return 3;
    return -1;
}

/**
 * @brief Get sectors per track
 * @param track Track number (1-based)
 * @return Sectors per track (17-21), or 0 if invalid
 */
static inline int uft_c64_sectors_per_track(int track) {
    int zone = uft_c64_get_zone(track);
    return (zone >= 0) ? uft_c64_zones[zone].sectors : 0;
}

/**
 * @brief Calculate D64 offset for sector
 * @param track Track number (1-based)
 * @param sector Sector number (0-based)
 * @return Byte offset in D64 image, or -1 if invalid
 */
int uft_c64_d64_offset(int track, int sector);

/**
 * @brief Commodore sector header (decoded)
 */
typedef struct {
    uint8_t block_type;   /**< 0x08 for header */
    uint8_t checksum;     /**< XOR of track, sector, id1, id2 */
    uint8_t sector;       /**< Sector number */
    uint8_t track;        /**< Track number */
    uint8_t id2;          /**< Disk ID byte 2 */
    uint8_t id1;          /**< Disk ID byte 1 */
} uft_c64_header_t;

/**
 * @brief Parse sector header from decoded GCR
 * @param data 8 decoded bytes
 * @param header Output header structure
 * @return true if checksum valid
 */
bool uft_c64_parse_header(const uint8_t *data, uft_c64_header_t *header);

/**
 * @brief Verify sector data checksum
 * @param data 257 decoded bytes (data + checksum)
 * @return true if checksum valid
 */
bool uft_c64_verify_data(const uint8_t *data);

/**
 * @brief Calculate XOR checksum
 */
static inline uint8_t uft_c64_checksum(const uint8_t *data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

/*===========================================================================
 * Apple II Sector Handling
 *===========================================================================*/

/** Apple II sector sizes */
#define UFT_APPLE_SECTOR_SIZE    256    /**< DOS 3.3 sector size */
#define UFT_APPLE_PRODOS_BLOCK   512    /**< ProDOS block size */

/** Address field markers */
#define UFT_APPLE_ADDR_PROLOGUE_D5  0xD5
#define UFT_APPLE_ADDR_PROLOGUE_AA  0xAA
#define UFT_APPLE_ADDR_PROLOGUE_96  0x96
#define UFT_APPLE_ADDR_EPILOGUE_DE  0xDE
#define UFT_APPLE_ADDR_EPILOGUE_AA  0xAA

/** Data field markers */
#define UFT_APPLE_DATA_PROLOGUE_D5  0xD5
#define UFT_APPLE_DATA_PROLOGUE_AA  0xAA
#define UFT_APPLE_DATA_PROLOGUE_AD  0xAD

/**
 * @brief Apple II address field (4-and-4 encoded)
 */
typedef struct {
    uint8_t volume;       /**< Volume number */
    uint8_t track;        /**< Track number */
    uint8_t sector;       /**< Sector number */
    uint8_t checksum;     /**< XOR of above three */
} uft_apple_addr_t;

/**
 * @brief Decode Apple II 4-and-4 byte pair
 * @param odd Odd bits (bits 7,5,3,1 in bit positions 6,4,2,0)
 * @param even Even bits (bits 6,4,2,0 in bit positions 6,4,2,0)
 * @return Decoded byte
 */
static inline uint8_t uft_apple_decode_44(uint8_t odd, uint8_t even) {
    return ((odd << 1) | 0x01) & even;
}

/**
 * @brief Encode byte as Apple II 4-and-4 pair
 * @param value Byte to encode
 * @param odd Output: odd byte
 * @param even Output: even byte
 */
static inline void uft_apple_encode_44(uint8_t value, uint8_t *odd, uint8_t *even) {
    *odd = (value >> 1) | 0xAA;
    *even = value | 0xAA;
}

/**
 * @brief Decode 6-and-2 sector data
 * @param encoded 343 encoded bytes (342 data + checksum)
 * @param decoded 256 output bytes
 * @return true if checksum valid
 */
bool uft_apple_decode_62(const uint8_t *encoded, uint8_t *decoded);

/**
 * @brief Encode 6-and-2 sector data
 * @param data 256 input bytes
 * @param encoded 343 output bytes
 */
void uft_apple_encode_62(const uint8_t *data, uint8_t *encoded);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GCR_H */
