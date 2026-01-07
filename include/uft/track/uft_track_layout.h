/**
 * @file uft_track_layout.h
 * @brief Track Layout Generation for UFT
 * 
 * Generates track layouts for various floppy disk formats with
 * proper sync, gap, header, and data block structures.
 * 
 * Supports:
 * - Pattern A: Fixed geometry (21 sectors, constant speed)
 * - Pattern B: Zoned C64 format with header+data blocks
 * - Pattern C: Zoned data-only format
 * - Pattern D: Raw sync stream (for diagnostics)
 * 
 * 
 * @copyright UFT Project 2026
 */

#ifndef UFT_TRACK_LAYOUT_H
#define UFT_TRACK_LAYOUT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Track Layout Pattern Types
 *============================================================================*/

typedef enum uft_track_pattern {
    UFT_PATTERN_FIXED           = 0,    /**< Fixed geometry (21 sectors) */
    UFT_PATTERN_ZONED           = 1,    /**< C64-style zoned with header+data */
    UFT_PATTERN_ZONED_DATA_ONLY = 2,    /**< Zoned, data blocks only */
    UFT_PATTERN_RAW_STREAM      = 3     /**< Raw sync stream (diagnostic) */
} uft_track_pattern_t;

/*============================================================================
 * Track Layout Parameters
 *============================================================================*/

typedef struct uft_track_params {
    uft_track_pattern_t pattern;
    
    uint16_t no_tracks;         /**< Total tracks on disk */
    uint16_t track_size;        /**< Track size in bytes */
    
    uint16_t track_first;       /**< First track (inclusive) */
    uint16_t track_last;        /**< Last track (inclusive) */
    
    /* For fixed pattern */
    uint16_t sector_first;      /**< First sector (inclusive) */
    uint16_t sector_last;       /**< Last sector (inclusive) */
    
    /* Common parameters */
    uint16_t begin_at;          /**< Start offset within track */
    uint16_t sync_len;          /**< Sync mark length */
    
    /* Speed zone (0-3 for C64) */
    uint8_t  speed;
    
    /* For raw stream pattern */
    uint32_t raw_stream_bytes;  /**< Bytes in raw stream */
} uft_track_params_t;

/*============================================================================
 * Sector Maps for Zoned Formats
 *============================================================================*/

/**
 * @brief Map B sector counts by speed zone
 * Zone 0 (31+): 18, Zone 1 (25-30): 19, Zone 2 (18-24): 21, Zone 3 (1-17): 22
 */
extern const uint16_t uft_sector_map_b[4];

/**
 * @brief Map C sector counts by speed zone
 * Zone 0: 19, Zone 1: 20, Zone 2: 21, Zone 3: 23
 */
extern const uint16_t uft_sector_map_c[4];

/*============================================================================
 * Default Parameters
 *============================================================================*/

/**
 * @brief Get default parameters for fixed pattern
 * 
 * - Tracks 1-41, 21 sectors each
 * - Speed 3, sync 40
 * - 84 total tracks, 7928 bytes/track
 */
uft_track_params_t uft_track_params_default(void);

/**
 * @brief Get default parameters for zoned pattern (C64-style)
 * 
 * - Tracks 1-35
 * - Variable sectors by zone
 * - Speed by zone, sync 16
 */
uft_track_params_t uft_track_params_zoned(void);

/**
 * @brief Get default parameters for zoned data-only pattern
 */
uft_track_params_t uft_track_params_zoned_data_only(void);

/**
 * @brief Get default parameters for raw stream pattern
 * 
 * - 24*256 = 6144 bytes of raw data
 */
uft_track_params_t uft_track_params_raw_stream(void);

/*============================================================================
 * Zone Helper Functions
 *============================================================================*/

/**
 * @brief Get speed zone for track
 * @param track Track number (1-based)
 * @return Speed zone (0-3)
 */
uint8_t uft_track_speed_zone(uint16_t track);

/**
 * @brief Get sector count for track using Map B
 * @param track Track number (1-based)
 * @return Sector count
 */
uint16_t uft_track_sectors_map_b(uint16_t track);

/**
 * @brief Get sector count for track using Map C
 * @param track Track number (1-based)
 * @return Sector count
 */
uint16_t uft_track_sectors_map_c(uint16_t track);

/*============================================================================
 * Track Layout Generation
 *============================================================================*/

/**
 * @brief Generate track layout DSL to file
 * 
 * Outputs a human-readable DSL description of the track layout.
 * 
 * @param out Output file (stdout if NULL)
 * @param params Track parameters
 * @return 0 on success, error code on failure
 */
int uft_track_layout_write(FILE *out, const uft_track_params_t *params);

/**
 * @brief Generate raw track data
 * 
 * Generates actual GCR-encoded track data.
 * 
 * @param params Track parameters
 * @param track Track number
 * @param sector_data Array of sector data pointers (256 bytes each)
 * @param num_sectors Number of sectors
 * @param out Output buffer
 * @param out_len Output: bytes written
 * @return 0 on success
 */
int uft_track_generate(const uft_track_params_t *params, uint16_t track,
                       const uint8_t **sector_data, size_t num_sectors,
                       uint8_t *out, size_t *out_len);

/*============================================================================
 * Sector Block Generation
 *============================================================================*/

/**
 * @brief Sector header structure
 */
typedef struct uft_sector_header {
    uint8_t id;         /**< Block ID (0x08) */
    uint8_t checksum;   /**< XOR checksum */
    uint8_t sector;     /**< Sector number */
    uint8_t track;      /**< Track number */
    uint8_t id2;        /**< Disk ID byte 2 */
    uint8_t id1;        /**< Disk ID byte 1 */
} uft_sector_header_t;

/**
 * @brief Generate sector header block (GCR encoded)
 * 
 * @param hdr Header data
 * @param out Output buffer (must be at least 10 bytes for GCR)
 * @return Bytes written
 */
size_t uft_sector_header_encode(const uft_sector_header_t *hdr, uint8_t *out);

/**
 * @brief Generate sector data block (GCR encoded)
 * 
 * @param data Sector data (256 bytes)
 * @param out Output buffer (must be at least 325 bytes for GCR)
 * @return Bytes written
 */
size_t uft_sector_data_encode(const uint8_t data[256], uint8_t *out);

/**
 * @brief Generate sync mark
 * 
 * @param sync_len Number of sync bytes
 * @param out Output buffer
 * @return Bytes written
 */
size_t uft_sync_generate(uint16_t sync_len, uint8_t *out);

/**
 * @brief Generate inter-sector gap
 * 
 * @param gap_len Gap length in bytes
 * @param fill Fill byte (usually 0x55)
 * @param out Output buffer
 * @return Bytes written
 */
size_t uft_gap_generate(uint16_t gap_len, uint8_t fill, uint8_t *out);

/*============================================================================
 * Track Size Calculation
 *============================================================================*/

/**
 * @brief Calculate total track size for given parameters
 * 
 * @param params Track parameters
 * @param track Track number
 * @return Size in bytes (GCR encoded)
 */
size_t uft_track_size_calc(const uft_track_params_t *params, uint16_t track);

/**
 * @brief Calculate sector block size (header + data + gaps)
 * 
 * @param params Track parameters
 * @return Size in bytes
 */
size_t uft_sector_block_size(const uft_track_params_t *params);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_LAYOUT_H */
