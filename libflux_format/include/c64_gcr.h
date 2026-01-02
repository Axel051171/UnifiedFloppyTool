// SPDX-License-Identifier: MIT
/*
 * c64_gcr.h - Commodore 64 GCR Encoding/Decoding Header
 * 
 * @version 2.7.3
 * @date 2024-12-26
 */

#ifndef C64_GCR_H
#define C64_GCR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * CONSTANTS
 *============================================================================*/

#define C64_MAX_TRACKS_1541     42
#define C64_MAX_TRACKS_1571     70  /* Double-sided */
#define C64_SECTORS_PER_TRACK   21
#define C64_SECTOR_SIZE         256

/*============================================================================*
 * STRUCTURES
 *============================================================================*/

/**
 * @brief C64 sector
 */
typedef struct {
    uint8_t track;                  /* Track number */
    uint8_t sector;                 /* Sector number */
    uint8_t data[C64_SECTOR_SIZE];  /* Sector data */
    uint8_t disk_id[2];             /* Disk ID */
    bool valid;                     /* Sector valid flag */
    bool crc_ok;                    /* CRC verification */
} c64_sector_t;

/**
 * @brief C64 track
 */
typedef struct {
    uint8_t track_num;
    uint8_t *gcr_data;
    size_t gcr_length;
    c64_sector_t *sectors;
    uint8_t sector_count;
} c64_track_t;

/*============================================================================*
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Decode complete C64 sector from GCR track
 * 
 * @param gcr_track GCR track data
 * @param track_len Track length
 * @param start_pos Start position
 * @param sector_out Decoded sector (output)
 * @return Position after sector, or -1 on error
 */
int c64_decode_sector(
    const uint8_t *gcr_track,
    size_t track_len,
    size_t start_pos,
    c64_sector_t *sector_out
);

/**
 * @brief Get number of sectors for track
 * 
 * @param track Track number (1-42)
 * @return Number of sectors
 */
uint8_t c64_get_sectors_per_track(uint8_t track);

/**
 * @brief Get track capacity in bytes
 * 
 * @param track Track number (1-42)
 * @return Capacity in bytes
 */
size_t c64_get_track_capacity(uint8_t track);

#ifdef __cplusplus
}
#endif

#endif /* C64_GCR_H */
