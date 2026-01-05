/**
 * @file uft_c64_gcr.h
 * @brief Commodore 64/1541 GCR Encoding Support
 * @version 3.1.4.004
 *
 * Commodore-specific GCR (Group Coded Recording) for 1541 and compatible drives.
 *
 * Key differences from Apple GCR:
 * - 4 bits → 5 bits encoding (vs 6-and-2)
 * - Variable speed zones (17-21 sectors per track)
 * - XOR checksum (vs Apple's different algorithm)
 * - Different sync patterns and markers
 *
 * Track Layout:
 * - Tracks 1-17: 21 sectors, 307.69 kbit/s
 * - Tracks 18-24: 19 sectors, 285.71 kbit/s
 * - Tracks 25-30: 18 sectors, 266.67 kbit/s
 * - Tracks 31-35: 17 sectors, 250 kbit/s
 *
 * Based on bbc-fdc implementation and 1541 technical documentation.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_C64_GCR_H
#define UFT_C64_GCR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * C64 GCR Constants
 *============================================================================*/

/** Sector size in bytes */
#define UFT_C64_SECTOR_SIZE     256

/** Maximum tracks on 1541 */
#define UFT_C64_MAX_TRACKS      35

/** Extended tracks (some disks use 40) */
#define UFT_C64_EXT_TRACKS      40

/** Total sectors on standard disk (683) */
#define UFT_C64_TOTAL_SECTORS   683

/** BAM track */
#define UFT_C64_BAM_TRACK       18

/** BAM sector */
#define UFT_C64_BAM_SECTOR      0

/** Directory starts at */
#define UFT_C64_DIR_TRACK       18
#define UFT_C64_DIR_SECTOR      1

/*============================================================================
 * GCR Encoding
 *============================================================================*/

/** Sync byte (NOT GCR encoded) */
#define UFT_C64_SYNC_BYTE       0xFF

/** Number of sync bytes before sector */
#define UFT_C64_SYNC_COUNT      5

/** ID block marker (GCR encoded as 0x52) */
#define UFT_C64_ID_MARKER       0x08

/** Data block marker (GCR encoded as 0x55) */
#define UFT_C64_DATA_MARKER     0x07

/** Off byte (gap filler) */
#define UFT_C64_OFF_BYTE        0x0F

/** Gap byte (between sectors) */
#define UFT_C64_GAP_BYTE        0x55

/*============================================================================
 * Speed Zones
 *============================================================================*/

/**
 * Get sectors per track for given track number
 */
static inline int uft_c64_sectors_per_track(int track) {
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;
}

/**
 * Get speed zone for track
 */
static inline int uft_c64_speed_zone(int track) {
    if (track <= 17) return 3;
    if (track <= 24) return 2;
    if (track <= 30) return 1;
    return 0;
}

/**
 * Get bitrate for track (bits per second)
 */
static inline int uft_c64_track_bitrate(int track) {
    static const int rates[] = { 250000, 266667, 285714, 307692 };
    return rates[uft_c64_speed_zone(track)];
}

/**
 * Get bytes per track (approximately)
 */
static inline int uft_c64_bytes_per_track(int track) {
    static const int bytes[] = { 6250, 6667, 7143, 7692 };
    return bytes[uft_c64_speed_zone(track)];
}

/*============================================================================
 * GCR Encoding Tables
 *============================================================================*/

/**
 * 4-bit value to 5-bit GCR encoding
 *
 * Standard C64 GCR table:
 *   0x0→0x0A, 0x1→0x0B, 0x2→0x12, 0x3→0x13
 *   0x4→0x0E, 0x5→0x0F, 0x6→0x16, 0x7→0x17
 *   0x8→0x09, 0x9→0x19, 0xA→0x1A, 0xB→0x1B
 *   0xC→0x0D, 0xD→0x1D, 0xE→0x1E, 0xF→0x15
 */
extern const uint8_t UFT_C64_GCR_ENCODE[16];

/**
 * 5-bit GCR to 4-bit value decoding
 * Invalid entries are 0xFF
 */
extern const uint8_t UFT_C64_GCR_DECODE[32];

/*============================================================================
 * GCR Structures
 *============================================================================*/

/** Sector header (ID block) - before GCR encoding */
typedef struct {
    uint8_t     marker;         /**< 0x08 */
    uint8_t     checksum;       /**< XOR of sector, track, id2, id1 */
    uint8_t     sector;         /**< Sector number (0-20) */
    uint8_t     track;          /**< Track number (1-35) */
    uint8_t     id2;            /**< Disk ID byte 2 */
    uint8_t     id1;            /**< Disk ID byte 1 */
    uint8_t     off1;           /**< 0x0F */
    uint8_t     off2;           /**< 0x0F */
} uft_c64_sector_header_t;

/** Data block - before GCR encoding */
typedef struct {
    uint8_t     marker;         /**< 0x07 */
    uint8_t     data[UFT_C64_SECTOR_SIZE];
    uint8_t     checksum;       /**< XOR of all 256 data bytes */
    uint8_t     off1;           /**< 0x00 */
    uint8_t     off2;           /**< 0x00 */
} uft_c64_data_block_t;

/** Decoded sector */
typedef struct {
    int         track;
    int         sector;
    uint16_t    disk_id;
    uint8_t     data[UFT_C64_SECTOR_SIZE];
    bool        header_valid;
    bool        data_valid;
    bool        header_checksum_ok;
    bool        data_checksum_ok;
} uft_c64_sector_t;

/*============================================================================
 * GCR Encoding/Decoding Functions
 *============================================================================*/

/**
 * @brief Encode 4 bytes to 5 GCR bytes
 *
 * @param input 4 input bytes
 * @param output 5 output GCR bytes
 */
void uft_c64_gcr_encode_4to5(const uint8_t input[4], uint8_t output[5]);

/**
 * @brief Decode 5 GCR bytes to 4 bytes
 *
 * @param input 5 GCR bytes
 * @param output 4 output bytes
 * @return true if all GCR codes were valid
 */
bool uft_c64_gcr_decode_5to4(const uint8_t input[5], uint8_t output[4]);

/**
 * @brief Encode byte buffer to GCR
 *
 * @param data Input bytes
 * @param len Input length (must be multiple of 4)
 * @param gcr Output GCR buffer (len * 5/4 bytes)
 */
void uft_c64_gcr_encode(const uint8_t *data, size_t len, uint8_t *gcr);

/**
 * @brief Decode GCR buffer to bytes
 *
 * @param gcr GCR input
 * @param len GCR length (must be multiple of 5)
 * @param data Output buffer (len * 4/5 bytes)
 * @return true if all valid
 */
bool uft_c64_gcr_decode(const uint8_t *gcr, size_t len, uint8_t *data);

/*============================================================================
 * Sector Operations
 *============================================================================*/

/**
 * @brief Calculate XOR checksum
 * @param data Data buffer
 * @param len Data length
 * @return XOR of all bytes
 */
uint8_t uft_c64_xor_checksum(const uint8_t *data, size_t len);

/**
 * @brief Encode sector header to GCR
 *
 * @param track Track number
 * @param sector Sector number
 * @param disk_id 16-bit disk ID
 * @param gcr Output: 10 GCR bytes
 */
void uft_c64_encode_header(int track, int sector, uint16_t disk_id,
                           uint8_t gcr[10]);

/**
 * @brief Encode data block to GCR
 *
 * @param data 256 bytes of sector data
 * @param gcr Output: 325 GCR bytes
 */
void uft_c64_encode_data(const uint8_t data[256], uint8_t gcr[325]);

/**
 * @brief Decode sector header from GCR
 *
 * @param gcr 10 GCR bytes
 * @param track Output: track number
 * @param sector Output: sector number
 * @param disk_id Output: disk ID
 * @return true if valid
 */
bool uft_c64_decode_header(const uint8_t gcr[10],
                           int *track, int *sector, uint16_t *disk_id);

/**
 * @brief Decode data block from GCR
 *
 * @param gcr 325 GCR bytes
 * @param data Output: 256 bytes
 * @return true if checksum valid
 */
bool uft_c64_decode_data(const uint8_t gcr[325], uint8_t data[256]);

/*============================================================================
 * Bitstream Processing
 *============================================================================*/

/** Parser state */
typedef enum {
    UFT_C64_STATE_IDLE,
    UFT_C64_STATE_ID,
    UFT_C64_STATE_DATA,
} uft_c64_parser_state_t;

/** Parser context */
typedef struct {
    uft_c64_parser_state_t state;
    uint16_t    datacells;      /**< 16-bit sliding window */
    int         bits;           /**< Bits accumulated */
    uint8_t     gcr_buffer[512];
    int         gcr_len;
    uint8_t     byte_buffer[512];
    int         byte_len;
    
    /* Last found sector info */
    int         last_track;
    int         last_sector;
    uint16_t    last_disk_id;
    unsigned long id_position;
    unsigned long data_position;
} uft_c64_parser_t;

/**
 * @brief Initialize parser
 * @param parser Parser context
 * @param track Current track (for bitrate buckets)
 */
void uft_c64_parser_init(uft_c64_parser_t *parser, int track);

/**
 * @brief Add bit to parser
 * @param parser Parser context
 * @param bit Bit value (0 or 1)
 * @param position Bit position for logging
 */
void uft_c64_parser_add_bit(uft_c64_parser_t *parser,
                            uint8_t bit, unsigned long position);

/**
 * @brief Add flux sample to parser
 *
 * Converts sample count to bits and adds them.
 *
 * @param parser Parser context
 * @param samples Sample count since last flux
 * @param position Data position
 * @param track Track number (for timing buckets)
 */
void uft_c64_parser_add_sample(uft_c64_parser_t *parser,
                               unsigned long samples,
                               unsigned long position,
                               int track);

/*============================================================================
 * D64/G64 Format Support
 *============================================================================*/

/** D64 file size (35 tracks, no error info) */
#define UFT_D64_SIZE_35         174848

/** D64 file size (35 tracks, with error info) */
#define UFT_D64_SIZE_35_ERR     175531

/** D64 file size (40 tracks, no error info) */
#define UFT_D64_SIZE_40         196608

/** D64 file size (40 tracks, with error info) */
#define UFT_D64_SIZE_40_ERR     197376

/**
 * @brief Get sector offset in D64 file
 * @param track Track number (1-35 or 1-40)
 * @param sector Sector number
 * @return Byte offset in D64 file
 */
uint32_t uft_d64_sector_offset(int track, int sector);

/**
 * @brief Read sector from D64 data
 * @param d64_data D64 file data
 * @param d64_size D64 file size
 * @param track Track number
 * @param sector Sector number
 * @param data Output: 256 bytes
 * @return true on success
 */
bool uft_d64_read_sector(const uint8_t *d64_data, size_t d64_size,
                         int track, int sector, uint8_t data[256]);

#ifdef __cplusplus
}
#endif

#endif /* UFT_C64_GCR_H */
