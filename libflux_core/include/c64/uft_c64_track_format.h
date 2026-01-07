/*
 * uft_c64_track_format.h
 *
 * C64/1541 track format tables and constants.
 *
 * This module provides:
 * - Sector counts per track (sector_map)
 * - Speed zones (speed_map)
 * - Gap lengths between sectors
 * - Track capacity calculations
 * - Density zone data rates
 *
 * Original Source:
 *
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_c64_track_format.c
 */

#ifndef UFT_C64_TRACK_FORMAT_H
#define UFT_C64_TRACK_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK/DISK CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Track limits */
#define UFT_C64_MAX_TRACKS_1541      42   /* Tracks 1-42 (includes non-standard) */
#define UFT_C64_MAX_TRACKS_1571      84   /* Double-sided */
#define UFT_C64_MAX_HALFTRACKS_1541  84
#define UFT_C64_MAX_HALFTRACKS_1571  168
#define UFT_C64_STANDARD_TRACKS      35   /* Standard DOS tracks */

/* D64 constants */
#define UFT_C64_BLOCKS_ON_DISK       683  /* Standard 35-track disk */
#define UFT_C64_BLOCKS_EXTRA         85   /* Tracks 36-40 */
#define UFT_C64_MAX_BLOCKS           (UFT_C64_BLOCKS_ON_DISK + UFT_C64_BLOCKS_EXTRA)

/* NIB format */
#define UFT_C64_NIB_TRACK_LENGTH     0x2000   /* 8192 bytes per track */
#define UFT_C64_NIB_HEADER_SIZE      0xFF

/* Sector structure lengths (in GCR bytes) */
#define UFT_C64_SYNC_LENGTH          5
#define UFT_C64_HEADER_LENGTH        10
#define UFT_C64_HEADER_GAP_LENGTH    9    /* Must be 9 or 1541 corrupts on write */
#define UFT_C64_DATA_LENGTH          325  /* 65 * 5 GCR bytes = 256 data + 4 overhead */

#define UFT_C64_SECTOR_SIZE          (UFT_C64_SYNC_LENGTH + UFT_C64_HEADER_LENGTH + \
                                      UFT_C64_HEADER_GAP_LENGTH + UFT_C64_SYNC_LENGTH + \
                                      UFT_C64_DATA_LENGTH)

/* GCR block sizes */
#define UFT_C64_GCR_BLOCK_HEADER_LEN 24
#define UFT_C64_GCR_BLOCK_DATA_LEN   337
#define UFT_C64_GCR_BLOCK_LEN        (UFT_C64_GCR_BLOCK_HEADER_LEN + UFT_C64_GCR_BLOCK_DATA_LEN)

/* Maximum sync offset before error (approx 800 GCR bytes = 1/10 rotation) */
#define UFT_C64_MAX_SYNC_OFFSET      0x1500

/* ═══════════════════════════════════════════════════════════════════════════
 * DENSITY ZONES
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * 1541 Speed Zones:
 *
 * Zone  Tracks    Sectors  Clock Divisor  Data Rate    Bytes/Track
 * ─────────────────────────────────────────────────────────────────
 *  3    1-17      21       13             307692 bps   ~7692
 *  2    18-24     19       14             285714 bps   ~7143
 *  1    25-30     18       15             266667 bps   ~6667
 *  0    31-35+    17       16             250000 bps   ~6250
 *
 * Data rate calculation:
 *   rate = 4,000,000 / (divisor * 8) = 500000 / divisor  bits/sec
 *   bytes/minute = rate * 60 / 8
 */

typedef enum uft_c64_speed_zone {
    UFT_C64_SPEED_ZONE_0 = 0,   /* Tracks 31-42, 17 sectors, slowest */
    UFT_C64_SPEED_ZONE_1 = 1,   /* Tracks 25-30, 18 sectors */
    UFT_C64_SPEED_ZONE_2 = 2,   /* Tracks 18-24, 19 sectors */
    UFT_C64_SPEED_ZONE_3 = 3    /* Tracks  1-17, 21 sectors, fastest */
} uft_c64_speed_zone_t;

/* Bytes per minute at each density zone */
#define UFT_C64_DENSITY_3    2307692   /* Zone 3: 307692 bps */
#define UFT_C64_DENSITY_2    2142857   /* Zone 2: 285714 bps */
#define UFT_C64_DENSITY_1    2000000   /* Zone 1: 266667 bps */
#define UFT_C64_DENSITY_0    1875000   /* Zone 0: 250000 bps */

/* ═══════════════════════════════════════════════════════════════════════════
 * DISK ERROR CODES (1541 DOS)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_c64_error_code {
    UFT_C64_ERR_SECTOR_OK        = 0x01,   /* 00,OK */
    UFT_C64_ERR_HEADER_NOT_FOUND = 0x02,   /* 20,READ ERROR */
    UFT_C64_ERR_SYNC_NOT_FOUND   = 0x03,   /* 21,READ ERROR */
    UFT_C64_ERR_DATA_NOT_FOUND   = 0x04,   /* 22,READ ERROR */
    UFT_C64_ERR_BAD_DATA_CHKSUM  = 0x05,   /* 23,READ ERROR */
    UFT_C64_ERR_BAD_GCR          = 0x06,   /* 24,READ ERROR */
    UFT_C64_ERR_BAD_HDR_CHKSUM   = 0x09,   /* 27,READ ERROR */
    UFT_C64_ERR_ID_MISMATCH      = 0x0B,   /* 29,DISK ID MISMATCH */
    UFT_C64_ERR_DRIVE_NOT_READY  = 0x0F    /* 74,DRIVE NOT READY */
} uft_c64_error_code_t;

/* Bitflags for track analysis */
#define UFT_C64_BM_MATCH           0x10
#define UFT_C64_BM_NO_CYCLE        0x20
#define UFT_C64_BM_NO_SYNC         0x40
#define UFT_C64_BM_FF_TRACK        0x80

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK FORMAT TABLES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Get number of sectors for a track.
 *
 * Standard 1541 track layout:
 * - Tracks  1-17: 21 sectors
 * - Tracks 18-24: 19 sectors
 * - Tracks 25-30: 18 sectors
 * - Tracks 31-42: 17 sectors
 *
 * @param track   Track number (1-42)
 * @return        Number of sectors (0 for invalid track)
 */
uint8_t uft_c64_sectors_per_track(int track);

/**
 * Get speed zone for a track.
 *
 * @param track   Track number (1-42)
 * @return        Speed zone (0-3)
 */
uft_c64_speed_zone_t uft_c64_speed_zone(int track);

/**
 * Get inter-sector gap length for a track.
 *
 * Gap lengths vary by track density:
 * - Zone 3 (tracks  1-17): 10 bytes
 * - Zone 2 (tracks 18-24): 17 bytes
 * - Zone 1 (tracks 25-30): 11 bytes
 * - Zone 0 (tracks 31-42):  8 bytes
 *
 * @param track   Track number (1-42)
 * @return        Gap length in bytes
 */
uint8_t uft_c64_sector_gap_length(int track);

/**
 * Get data rate for a speed zone.
 *
 * @param zone    Speed zone (0-3)
 * @return        Data rate in bytes per minute
 */
uint32_t uft_c64_zone_data_rate(uft_c64_speed_zone_t zone);

/**
 * Get typical track capacity for a speed zone at given RPM.
 *
 * @param zone    Speed zone (0-3)
 * @param rpm     Drive RPM (typically 300)
 * @return        Track capacity in bytes
 */
size_t uft_c64_track_capacity(uft_c64_speed_zone_t zone, int rpm);

/**
 * Get minimum track capacity for a speed zone.
 *
 * @param zone    Speed zone (0-3)
 * @return        Minimum expected capacity
 */
size_t uft_c64_track_capacity_min(uft_c64_speed_zone_t zone);

/**
 * Get maximum track capacity for a speed zone.
 *
 * @param zone    Speed zone (0-3)
 * @return        Maximum expected capacity
 */
size_t uft_c64_track_capacity_max(uft_c64_speed_zone_t zone);

/* ═══════════════════════════════════════════════════════════════════════════
 * D64 BLOCK ADDRESSING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Convert track/sector to linear block number.
 *
 * @param track   Track number (1-40)
 * @param sector  Sector number (0-20)
 * @return        Linear block number (0-767), or -1 for invalid
 */
int uft_c64_ts_to_block(int track, int sector);

/**
 * Convert linear block number to track/sector.
 *
 * @param block      Linear block number
 * @param track_out  Output: track number
 * @param sector_out Output: sector number
 * @return           true if valid, false otherwise
 */
bool uft_c64_block_to_ts(int block, int *track_out, int *sector_out);

/**
 * Get offset into D64 file for a track/sector.
 *
 * @param track   Track number (1-40)
 * @param sector  Sector number
 * @return        Byte offset into D64, or -1 for invalid
 */
int uft_c64_d64_offset(int track, int sector);

/* ═══════════════════════════════════════════════════════════════════════════
 * DIRECTORY TRACK
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_C64_DIR_TRACK            18
#define UFT_C64_BAM_SECTOR           0
#define UFT_C64_DIR_FIRST_SECTOR     1

/**
 * Check if track/sector is in the directory area.
 *
 * @param track   Track number
 * @param sector  Sector number
 * @return        true if this is a directory/BAM sector
 */
bool uft_c64_is_dir_sector(int track, int sector);

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK VALIDATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Check if track number is valid for standard 1541.
 *
 * @param track   Track number
 * @return        true if track 1-35
 */
bool uft_c64_is_standard_track(int track);

/**
 * Check if track number is valid for extended 1541.
 *
 * @param track   Track number
 * @return        true if track 1-40
 */
bool uft_c64_is_extended_track(int track);

/**
 * Check if halftrack has potential data (non-standard preservation).
 *
 * @param halftrack   Halftrack number (track * 2, 0-indexed)
 * @return            true if this halftrack could contain protection data
 */
bool uft_c64_is_halftrack_data(int halftrack);

/* ═══════════════════════════════════════════════════════════════════════════
 * RAW TABLE ACCESS (for advanced use)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Get pointer to sector map table.
 * Table is indexed [0..42], track 0 unused.
 *
 * @return   Pointer to sector_map array
 */
const uint8_t *uft_c64_get_sector_map(void);

/**
 * Get pointer to speed zone map table.
 * Table is indexed [0..42], track 0 unused.
 *
 * @return   Pointer to speed_map array
 */
const uint8_t *uft_c64_get_speed_map(void);

/**
 * Get pointer to gap length table.
 * Table is indexed [0..42], track 0 unused.
 *
 * @return   Pointer to gap_length array
 */
const uint8_t *uft_c64_get_gap_map(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_C64_TRACK_FORMAT_H */
