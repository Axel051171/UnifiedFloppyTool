/**
 * @file uft_victor9k_gcr.h
 * @brief Victor 9000 Variable-Density GCR Encoding
 * 
 * Based on HxCFloppyEmulator victor9k_gcr_track.c
 * Copyright (C) 2006-2025 Jean-François DEL NERO
 * License: GPL-2.0+
 * 
 * The Victor 9000 (Sirius 1) uses a unique variable-density GCR encoding
 * where different tracks have different numbers of sectors:
 *   - Outer tracks (0-3):   19 sectors
 *   - Tracks 4-15:          18 sectors
 *   - Tracks 16-26:         17 sectors
 *   - Tracks 27-37:         16 sectors
 *   - Tracks 38-47:         15 sectors
 *   - Tracks 48-59:         14 sectors
 *   - Tracks 60-71:         13 sectors
 *   - Inner tracks (72-79): 12 sectors
 * 
 * Total capacity: 1224 sectors * 512 bytes = 606KB (single-sided)
 */

#ifndef UFT_VICTOR9K_GCR_H
#define UFT_VICTOR9K_GCR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Geometry Constants
 *===========================================================================*/

/** Victor 9000 disk geometry */
#define UFT_V9K_TRACKS          80
#define UFT_V9K_HEADS           2
#define UFT_V9K_SECTOR_SIZE     512
#define UFT_V9K_MAX_SECTORS     19   /**< Maximum sectors per track */
#define UFT_V9K_MIN_SECTORS     12   /**< Minimum sectors per track */

/** Number of zones with different sector counts */
#define UFT_V9K_ZONES           8

/** Total sectors per side */
#define UFT_V9K_SECTORS_PER_SIDE    1224

/** Disk capacity */
#define UFT_V9K_CAPACITY_SS     (UFT_V9K_SECTORS_PER_SIDE * UFT_V9K_SECTOR_SIZE)  /* 606KB */
#define UFT_V9K_CAPACITY_DS     (UFT_V9K_CAPACITY_SS * 2)  /* 1212KB */

/*===========================================================================
 * Zone Definitions
 *===========================================================================*/

/**
 * @brief Victor 9000 zone definition
 */
typedef struct {
    uint8_t start_track;        /**< First track in zone */
    uint8_t end_track;          /**< Last track in zone (exclusive) */
    uint8_t sectors_per_track;  /**< Sectors in this zone */
    uint32_t data_rate;         /**< Data rate in bits/sec */
    double bit_cell_us;         /**< Bit cell time in microseconds */
} uft_v9k_zone_t;

/**
 * Victor 9000 zone table (outer to inner)
 */
static const uft_v9k_zone_t uft_v9k_zones[UFT_V9K_ZONES] = {
    {  0,  4, 19, 394000, 2.538 },  /* Zone 0: 19 sectors, fastest */
    {  4, 16, 18, 373000, 2.681 },  /* Zone 1: 18 sectors */
    { 16, 27, 17, 352000, 2.841 },  /* Zone 2: 17 sectors */
    { 27, 38, 16, 331000, 3.021 },  /* Zone 3: 16 sectors */
    { 38, 48, 15, 310000, 3.226 },  /* Zone 4: 15 sectors */
    { 48, 60, 14, 289000, 3.460 },  /* Zone 5: 14 sectors */
    { 60, 72, 13, 268000, 3.731 },  /* Zone 6: 13 sectors */
    { 72, 80, 12, 247000, 4.049 }   /* Zone 7: 12 sectors, slowest */
};

/*===========================================================================
 * GCR Encoding
 *===========================================================================*/

/** Victor 9000 GCR encoding (different from C64/Apple!) */
static const uint8_t uft_v9k_gcr_encode[16] = {
    0x0A, 0x0B, 0x12, 0x13,  /* 0-3 */
    0x0E, 0x0F, 0x16, 0x17,  /* 4-7 */
    0x09, 0x19, 0x1A, 0x1B,  /* 8-B */
    0x0D, 0x1D, 0x1E, 0x15   /* C-F */
};

/** Victor 9000 GCR decoding */
static const uint8_t uft_v9k_gcr_decode[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 00-07: invalid */
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  /* 08-0F */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  /* 10-17 */
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   /* 18-1F */
};

/** Header/data sync marks */
#define UFT_V9K_SYNC_HEADER     0x3FC   /**< Header sync: 1111111100 */
#define UFT_V9K_SYNC_DATA       0x3FB   /**< Data sync: 1111111011 */

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Victor 9000 sector header
 */
typedef struct {
    uint8_t track;              /**< Track number */
    uint8_t sector;             /**< Sector number */
    uint8_t head;               /**< Head number (0 or 1) */
    uint8_t checksum;           /**< Header checksum */
} uft_v9k_header_t;

/**
 * @brief Victor 9000 sector
 */
typedef struct {
    uft_v9k_header_t header;    /**< Sector header */
    uint8_t data[512];          /**< Sector data */
    uint16_t data_crc;          /**< Data CRC-16 */
    bool header_valid;          /**< Header checksum OK */
    bool data_valid;            /**< Data CRC OK */
} uft_v9k_sector_t;

/**
 * @brief Victor 9000 track
 */
typedef struct {
    uint8_t track_num;          /**< Track number */
    uint8_t head;               /**< Head number */
    uint8_t zone;               /**< Zone index (0-7) */
    uint8_t sector_count;       /**< Sectors on this track */
    uft_v9k_sector_t sectors[UFT_V9K_MAX_SECTORS];
    uint8_t valid_count;        /**< Number of valid sectors */
} uft_v9k_track_t;

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Get zone index for track
 * @param track Track number (0-79)
 * @return Zone index (0-7)
 */
static inline uint8_t uft_v9k_get_zone(uint8_t track)
{
    if (track < 4)  return 0;
    if (track < 16) return 1;
    if (track < 27) return 2;
    if (track < 38) return 3;
    if (track < 48) return 4;
    if (track < 60) return 5;
    if (track < 72) return 6;
    return 7;
}

/**
 * @brief Get sectors per track
 * @param track Track number
 * @return Number of sectors
 */
static inline uint8_t uft_v9k_sectors_per_track(uint8_t track)
{
    return uft_v9k_zones[uft_v9k_get_zone(track)].sectors_per_track;
}

/**
 * @brief Get data rate for track
 * @param track Track number
 * @return Data rate in bits/sec
 */
static inline uint32_t uft_v9k_data_rate(uint8_t track)
{
    return uft_v9k_zones[uft_v9k_get_zone(track)].data_rate;
}

/**
 * @brief Get bit cell time for track
 * @param track Track number
 * @return Bit cell time in microseconds
 */
static inline double uft_v9k_bit_cell_us(uint8_t track)
{
    return uft_v9k_zones[uft_v9k_get_zone(track)].bit_cell_us;
}

/**
 * @brief Calculate linear sector number
 * @param track Track number
 * @param head Head number
 * @param sector Sector number
 * @return Linear sector number (0-based)
 */
uint32_t uft_v9k_lba(uint8_t track, uint8_t head, uint8_t sector);

/**
 * @brief Convert LBA to CHS
 * @param lba Linear sector number
 * @param track Output: track number
 * @param head Output: head number
 * @param sector Output: sector number
 */
void uft_v9k_lba_to_chs(uint32_t lba, uint8_t *track, uint8_t *head, uint8_t *sector);

/*===========================================================================
 * GCR Encoding/Decoding
 *===========================================================================*/

/**
 * @brief Encode 4 bits to 5-bit GCR
 * @param nibble 4-bit value
 * @return 5-bit GCR value
 */
static inline uint8_t uft_v9k_encode_nibble(uint8_t nibble)
{
    return uft_v9k_gcr_encode[nibble & 0x0F];
}

/**
 * @brief Decode 5-bit GCR to 4 bits
 * @param gcr 5-bit GCR value
 * @return 4-bit value or 0xFF if invalid
 */
static inline uint8_t uft_v9k_decode_gcr(uint8_t gcr)
{
    return uft_v9k_gcr_decode[gcr & 0x1F];
}

/**
 * @brief Encode 4 bytes to 5 GCR bytes
 * @param in 4-byte input
 * @param out 5-byte output
 */
void uft_v9k_encode_group(const uint8_t in[4], uint8_t out[5]);

/**
 * @brief Decode 5 GCR bytes to 4 bytes
 * @param in 5-byte GCR input
 * @param out 4-byte output
 * @return true if all GCR codes were valid
 */
bool uft_v9k_decode_group(const uint8_t in[5], uint8_t out[4]);

/*===========================================================================
 * Track Operations
 *===========================================================================*/

/**
 * @brief Decode Victor 9000 track from flux data
 * @param flux_data Raw flux timing data
 * @param flux_count Number of flux transitions
 * @param track_num Expected track number
 * @param head Head number
 * @param track Output track structure
 * @return Number of valid sectors decoded
 */
int uft_v9k_decode_track(const uint32_t *flux_data, size_t flux_count,
                         uint8_t track_num, uint8_t head,
                         uft_v9k_track_t *track);

/**
 * @brief Encode Victor 9000 track to flux data
 * @param track Track structure with sector data
 * @param flux_data Output flux timing buffer
 * @param max_flux Maximum flux transitions
 * @return Number of flux transitions written
 */
size_t uft_v9k_encode_track(const uft_v9k_track_t *track,
                            uint32_t *flux_data, size_t max_flux);

/**
 * @brief Calculate header checksum
 * @param header Header data
 * @return Checksum byte
 */
uint8_t uft_v9k_header_checksum(const uft_v9k_header_t *header);

/**
 * @brief Calculate data CRC
 * @param data 512-byte sector data
 * @return CRC-16 value
 */
uint16_t uft_v9k_data_crc(const uint8_t data[512]);

#ifdef __cplusplus
}
#endif

#endif /* UFT_VICTOR9K_GCR_H */
