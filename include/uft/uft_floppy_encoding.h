/**
 * @file uft_floppy_encoding.h
 * @brief Floppy Disk Data Encoding (FM, MFM, GCR)
 * 
 * various platform documentation.
 * 
 * Implements:
 * - FM (Frequency Modulation) - Single Density
 * - MFM (Modified FM) - Double Density and above
 * - M2FM (Modified MFM) - Intel ISIS, HP
 * - GCR (Group Coded Recording) - Apple, Commodore
 */

#ifndef UFT_FLOPPY_ENCODING_H
#define UFT_FLOPPY_ENCODING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * FM (Frequency Modulation) Encoding
 *===========================================================================
 * 
 * FM uses one clock bit before each data bit.
 * Cell structure: [C][D] where C=clock (always 1), D=data
 * 
 * Data rate: 125 kbit/s (effective 62.5 kbit/s data)
 * Cell time: 4µs at 250 kHz bit rate
 * 
 * Sync patterns:
 * - Address mark: Missing clock before data
 * - Index mark: 0xFC with missing clock
 * - ID mark: 0xFE with missing clock  
 * - Data mark: 0xFB with missing clock
 * - Deleted data: 0xF8 with missing clock
 */

/** FM sync byte values */
#define UFT_FM_SYNC_INDEX       0xFC    /**< Index address mark */
#define UFT_FM_SYNC_ID          0xFE    /**< ID address mark */
#define UFT_FM_SYNC_DATA        0xFB    /**< Data address mark */
#define UFT_FM_SYNC_DELETED     0xF8    /**< Deleted data mark */

/** FM gap fill byte */
#define UFT_FM_GAP_FILL         0xFF

/**
 * @brief Encode byte using FM
 * @param data Data byte to encode
 * @return 16-bit encoded value (clock+data interleaved)
 */
static inline uint16_t uft_fm_encode_byte(uint8_t data)
{
    uint16_t result = 0;
    for (int i = 7; i >= 0; i--) {
        result <<= 2;
        result |= 0x02;  /* Clock bit always 1 */
        result |= (data >> i) & 1;  /* Data bit */
    }
    return result;
}

/**
 * @brief Decode FM to data byte
 * @param fm 16-bit FM encoded value
 * @return Decoded data byte
 */
static inline uint8_t uft_fm_decode_byte(uint16_t fm)
{
    uint8_t result = 0;
    for (int i = 0; i < 8; i++) {
        result <<= 1;
        result |= (fm >> (14 - i * 2)) & 1;
    }
    return result;
}

/*===========================================================================
 * MFM (Modified Frequency Modulation) Encoding
 *===========================================================================
 * 
 * MFM removes redundant clock bits - clock only between consecutive 0s.
 * Cell structure: [C][D] where C depends on surrounding data
 * Clock rule: C=1 if previous D=0 AND current D=0, else C=0
 * 
 * Data rate: 250 kbit/s (effective 250 kbit/s data, 2x FM)
 * Cell time: 2µs at 500 kHz bit rate
 * 
 * Sync patterns:
 * - A1 sync: 0x4489 (special pattern with missing clock)
 * - C2 sync: 0x5224 (alternative sync)
 */

/** MFM special sync patterns */
#define UFT_MFM_SYNC_A1         0x4489  /**< A1 sync (missing clock) */
#define UFT_MFM_SYNC_C2         0x5224  /**< C2 sync (alternative) */

/** MFM address marks (follow 3x A1 sync) */
#define UFT_MFM_MARK_INDEX      0xFC    /**< Index address mark */
#define UFT_MFM_MARK_ID         0xFE    /**< ID address mark (IDAM) */
#define UFT_MFM_MARK_DATA       0xFB    /**< Data address mark (DAM) */
#define UFT_MFM_MARK_DELETED    0xF8    /**< Deleted data mark */

/** MFM gap fill byte */
#define UFT_MFM_GAP_FILL        0x4E

/**
 * @brief MFM encoder state
 */
typedef struct {
    uint8_t last_bit;   /**< Last data bit written */
} uft_mfm_state_t;

/**
 * @brief Initialize MFM encoder state
 */
static inline void uft_mfm_init(uft_mfm_state_t *state)
{
    state->last_bit = 0;
}

/**
 * @brief Encode byte using MFM
 * @param state Encoder state
 * @param data Data byte to encode
 * @return 16-bit encoded value
 */
static inline uint16_t uft_mfm_encode_byte(uft_mfm_state_t *state, uint8_t data)
{
    uint16_t result = 0;
    
    for (int i = 7; i >= 0; i--) {
        uint8_t bit = (data >> i) & 1;
        uint8_t clock = (!state->last_bit && !bit) ? 1 : 0;
        
        result <<= 2;
        result |= (clock << 1) | bit;
        state->last_bit = bit;
    }
    return result;
}

/**
 * @brief Decode MFM to data byte
 * @param mfm 16-bit MFM encoded value
 * @return Decoded data byte
 */
static inline uint8_t uft_mfm_decode_byte(uint16_t mfm)
{
    uint8_t result = 0;
    for (int i = 0; i < 8; i++) {
        result <<= 1;
        result |= (mfm >> (14 - i * 2)) & 1;
    }
    return result;
}

/*===========================================================================
 * GCR (Group Coded Recording) - Apple II
 *===========================================================================
 * 
 * Apple used several GCR variants:
 * - 5-and-3 (DOS 3.2): 5 bits encoded to 8 bits, max 1 consecutive 0
 * - 6-and-2 (DOS 3.3): 6 bits encoded to 8 bits, max 2 consecutive 0s
 * 
 * Rules for valid GCR bytes:
 * - Must start with a 1 bit (MSB = 1)
 * - No more than 2 consecutive 0 bits (6-and-2)
 * - No more than 1 consecutive 0 bit (5-and-3)
 */

/** Apple 6-and-2 GCR encoding table (64 entries) */
extern const uint8_t UFT_GCR_APPLE_6AND2_ENC[64];

/** Apple 6-and-2 GCR decoding table (256 entries, 0xFF = invalid) */
extern const uint8_t UFT_GCR_APPLE_6AND2_DEC[256];

/** Apple disk markers */
#define UFT_GCR_APPLE_PROLOG1   0xD5    /**< Address/data prolog byte 1 */
#define UFT_GCR_APPLE_PROLOG2   0xAA    /**< Address/data prolog byte 2 */
#define UFT_GCR_APPLE_ADDR3     0x96    /**< Address field marker */
#define UFT_GCR_APPLE_DATA3     0xAD    /**< Data field marker */
#define UFT_GCR_APPLE_EPILOG1   0xDE    /**< Epilog byte 1 */
#define UFT_GCR_APPLE_EPILOG2   0xAA    /**< Epilog byte 2 */

/**
 * @brief Encode 6 bits using Apple 6-and-2 GCR
 */
static inline uint8_t uft_gcr_apple_encode(uint8_t data6)
{
    extern const uint8_t UFT_GCR_APPLE_6AND2_ENC[64];
    return UFT_GCR_APPLE_6AND2_ENC[data6 & 0x3F];
}

/**
 * @brief Decode Apple 6-and-2 GCR byte
 * @return 6-bit value or 0xFF if invalid
 */
static inline uint8_t uft_gcr_apple_decode(uint8_t gcr)
{
    extern const uint8_t UFT_GCR_APPLE_6AND2_DEC[256];
    return UFT_GCR_APPLE_6AND2_DEC[gcr];
}

/*===========================================================================
 * GCR (Group Coded Recording) - Commodore
 *===========================================================================
 * 
 * Commodore GCR: 4 bits encoded to 5 bits (nibble-based)
 * Rules: No more than 2 consecutive 0 bits, no more than 8 consecutive 1s
 * 
 * Two nibbles are encoded to produce 10 bits of GCR data.
 */

/** Commodore GCR encoding table (16 entries) */
extern const uint8_t UFT_GCR_C64_ENC[16];

/** Commodore GCR decoding table (32 entries) */
extern const uint8_t UFT_GCR_C64_DEC[32];

/** Commodore sync byte (10x '1' bits) */
#define UFT_GCR_C64_SYNC        0xFF

/**
 * @brief Encode nibble using Commodore GCR
 */
static inline uint8_t uft_gcr_c64_encode_nibble(uint8_t nibble)
{
    extern const uint8_t UFT_GCR_C64_ENC[16];
    return UFT_GCR_C64_ENC[nibble & 0x0F];
}

/**
 * @brief Decode Commodore GCR nibble
 * @return 4-bit value or 0xFF if invalid
 */
static inline uint8_t uft_gcr_c64_decode_nibble(uint8_t gcr5)
{
    extern const uint8_t UFT_GCR_C64_DEC[32];
    return (gcr5 < 32) ? UFT_GCR_C64_DEC[gcr5] : 0xFF;
}

/**
 * @brief Encode byte pair to Commodore GCR (produces 10 bits)
 * @param high High nibble
 * @param low Low nibble
 * @return 10-bit GCR value
 */
static inline uint16_t uft_gcr_c64_encode_byte(uint8_t byte)
{
    uint8_t hi = uft_gcr_c64_encode_nibble(byte >> 4);
    uint8_t lo = uft_gcr_c64_encode_nibble(byte & 0x0F);
    return ((uint16_t)hi << 5) | lo;
}

/*===========================================================================
 * Commodore Zone Bit Recording
 *===========================================================================
 * 
 * The 1541 uses different numbers of sectors per track:
 * 
 * Zone 1: Tracks  1-17: 21 sectors (speed zone 3)
 * Zone 2: Tracks 18-24: 19 sectors (speed zone 2)
 * Zone 3: Tracks 25-30: 18 sectors (speed zone 1)
 * Zone 4: Tracks 31-35: 17 sectors (speed zone 0)
 * 
 * Total: 683 sectors = 174,848 bytes (256 bytes/sector - 2 header)
 */

/** Get sectors per track for C64/1541 */
static inline int uft_c64_sectors_per_track(int track)
{
    if (track < 1 || track > 35) return 0;
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;
}

/** Get speed zone for C64/1541 track */
static inline int uft_c64_speed_zone(int track)
{
    if (track < 1 || track > 35) return -1;
    if (track <= 17) return 3;
    if (track <= 24) return 2;
    if (track <= 30) return 1;
    return 0;
}

/*===========================================================================
 * Macintosh Variable Speed Zones
 *===========================================================================
 * 
 * Mac 400K/800K disks use CLV (Constant Linear Velocity) with variable
 * RPM. More sectors on outer tracks:
 * 
 * Tracks  0-15: 12 sectors @ 394 RPM
 * Tracks 16-31: 11 sectors @ 429 RPM
 * Tracks 32-47: 10 sectors @ 472 RPM
 * Tracks 48-63:  9 sectors @ 524 RPM
 * Tracks 64-79:  8 sectors @ 590 RPM
 */

/** Get sectors per track for Mac 400K/800K */
static inline int uft_mac_sectors_per_track(int track)
{
    if (track < 0 || track > 79) return 0;
    if (track < 16) return 12;
    if (track < 32) return 11;
    if (track < 48) return 10;
    if (track < 64) return 9;
    return 8;
}

/** Get approximate RPM for Mac track */
static inline int uft_mac_rpm_for_track(int track)
{
    if (track < 0 || track > 79) return 0;
    if (track < 16) return 394;
    if (track < 32) return 429;
    if (track < 48) return 472;
    if (track < 64) return 524;
    return 590;
}

/*===========================================================================
 * CRC Calculations
 *===========================================================================*/

/** CRC-16-CCITT polynomial (used by IBM MFM) */
#define UFT_CRC16_POLY          0x1021

/** Initial CRC value for IBM formats */
#define UFT_CRC16_INIT_IBM      0xFFFF

/** Initial CRC value for some other formats */
#define UFT_CRC16_INIT_ZERO     0x0000

/**
 * @brief Calculate CRC-16-CCITT
 * @param data Data buffer
 * @param length Data length
 * @param init Initial CRC value
 * @return CRC-16 value
 */
static inline uint16_t uft_crc16_ccitt(const uint8_t *data, size_t length,
                                        uint16_t init)
{
    uint16_t crc = init;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ UFT_CRC16_POLY;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Calculate IBM-style sector CRC (starts with 0xFFFF, includes mark)
 */
static inline uint16_t uft_crc16_ibm(const uint8_t *data, size_t length)
{
    return uft_crc16_ccitt(data, length, UFT_CRC16_INIT_IBM);
}

/*===========================================================================
 * Track Layout Constants
 *===========================================================================*/

/**
 * IBM MFM Track Layout (example for 512-byte sectors):
 * 
 * Gap 4a: 80 bytes of 0x4E
 * Sync:   12 bytes of 0x00
 * IAM:    3x 0xC2 + 0xFC
 * Gap 1:  50 bytes of 0x4E
 * 
 * For each sector:
 *   Sync:   12 bytes of 0x00
 *   IDAM:   3x 0xA1 + 0xFE
 *   ID:     Track, Head, Sector, Size (4 bytes)
 *   CRC:    2 bytes
 *   Gap 2:  22 bytes of 0x4E
 *   Sync:   12 bytes of 0x00
 *   DAM:    3x 0xA1 + 0xFB
 *   Data:   512 bytes
 *   CRC:    2 bytes
 *   Gap 3:  54 bytes of 0x4E
 * 
 * Gap 4b: Fill to end with 0x4E
 */

#define UFT_MFM_GAP4A_SIZE      80      /**< Pre-index gap */
#define UFT_MFM_SYNC_SIZE       12      /**< Sync field (0x00) */
#define UFT_MFM_GAP1_SIZE       50      /**< Post-index gap */
#define UFT_MFM_GAP2_SIZE       22      /**< ID to data gap */
#define UFT_MFM_GAP3_SIZE_512   54      /**< Inter-sector gap (512-byte) */
#define UFT_MFM_GAP3_SIZE_256   32      /**< Inter-sector gap (256-byte) */

/*===========================================================================
 * Sector Header Structure
 *===========================================================================*/

/**
 * @brief IBM-format sector ID field
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  track;         /**< Track number (cylinder) */
    uint8_t  head;          /**< Head/side number */
    uint8_t  sector;        /**< Sector number */
    uint8_t  size_code;     /**< Sector size code (0=128, 1=256, 2=512...) */
} uft_sector_id_t;

/**
 * @brief Complete sector header with CRC
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  sync[3];       /**< 0xA1 sync bytes */
    uint8_t  mark;          /**< Address mark (0xFE) */
    uft_sector_id_t id;     /**< Sector ID */
    uint16_t crc;           /**< CRC-16 */
} uft_mfm_sector_header_t;

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLOPPY_ENCODING_H */
