/**
 * @file uft_gcr_ops.h
 * @brief C64/1541 GCR Operations for Track Processing
 * 
 * Extended GCR operations based on nibtools gcr.c:
 * - Track comparison and verification
 * - Sync and gap manipulation
 * - Track cycle detection
 * - Error detection and repair
 * 
 * Reference: nibtools by Pete Rittwage (c64preservation.com)
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_GCR_OPS_H
#define UFT_GCR_OPS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** GCR encoded sector header marker (0x08) */
#define GCR_HEADER_MARKER       0x52    /* 01010010 */

/** GCR encoded sector data marker (0x07) */
#define GCR_DATA_MARKER         0x55    /* 01010101 */

/** Sync byte */
#define GCR_SYNC_BYTE           0xFF

/** Gap byte */
#define GCR_GAP_BYTE            0x55

/** Minimum sync bytes for valid sync */
#define GCR_MIN_SYNC            5

/** Maximum sync bytes before killer track */
#define GCR_MAX_SYNC            0x1000

/** Standard sector header size (GCR encoded) */
#define GCR_HEADER_SIZE         10

/** Standard sector data size (GCR encoded) */
#define GCR_DATA_SIZE           325

/** Sector size after GCR decoding */
#define SECTOR_SIZE             256

/* ============================================================================
 * GCR Encoding Tables
 * ============================================================================ */

/**
 * @brief Get GCR encode table
 * @return Pointer to 16-entry nibble-to-GCR table
 */
const uint8_t *gcr_get_encode_table(void);

/**
 * @brief Get GCR decode high nibble table
 * @return Pointer to 32-entry GCR-to-high-nibble table
 */
const uint8_t *gcr_get_decode_high_table(void);

/**
 * @brief Get GCR decode low nibble table
 * @return Pointer to 32-entry GCR-to-low-nibble table
 */
const uint8_t *gcr_get_decode_low_table(void);

/* ============================================================================
 * GCR Encode/Decode
 * ============================================================================ */

/**
 * @brief Encode bytes to GCR
 * @param plain Plain bytes to encode
 * @param plain_size Number of bytes to encode (must be multiple of 4)
 * @param gcr Output GCR buffer (must be 5/4 * plain_size)
 * @return Number of GCR bytes written
 */
size_t gcr_encode(const uint8_t *plain, size_t plain_size, uint8_t *gcr);

/**
 * @brief Decode GCR to bytes
 * @param gcr GCR bytes to decode
 * @param gcr_size Number of GCR bytes (must be multiple of 5)
 * @param plain Output plain buffer (must be 4/5 * gcr_size)
 * @param error_count Output: number of GCR errors (can be NULL)
 * @return Number of plain bytes written
 */
size_t gcr_decode(const uint8_t *gcr, size_t gcr_size, uint8_t *plain, size_t *error_count);

/**
 * @brief Check if GCR data contains errors
 * @param gcr GCR data
 * @param size Data size
 * @return Number of bad GCR bytes
 */
size_t gcr_check_errors(const uint8_t *gcr, size_t size);

/* ============================================================================
 * Sync Operations
 * ============================================================================ */

/**
 * @brief Find sync mark
 * @param gcr GCR data
 * @param size Data size
 * @param start Start position
 * @return Position of sync, or -1 if not found
 */
int gcr_find_sync(const uint8_t *gcr, size_t size, size_t start);

/**
 * @brief Find end of sync mark
 * @param gcr GCR data
 * @param size Data size
 * @param sync_start Start of sync
 * @return Position after sync
 */
size_t gcr_find_sync_end(const uint8_t *gcr, size_t size, size_t sync_start);

/**
 * @brief Count syncs in track
 * @param gcr GCR track data
 * @param size Track size
 * @return Number of syncs found
 */
int gcr_count_syncs(const uint8_t *gcr, size_t size);

/**
 * @brief Get longest sync length
 * @param gcr GCR track data
 * @param size Track size
 * @param position Output: position of longest sync (can be NULL)
 * @return Length of longest sync
 */
size_t gcr_longest_sync(const uint8_t *gcr, size_t size, size_t *position);

/**
 * @brief Lengthen sync marks to specified length
 * @param gcr GCR track data (modified in place)
 * @param size Track size
 * @param min_length Minimum sync length
 * @return Number of syncs lengthened
 */
int gcr_lengthen_syncs(uint8_t *gcr, size_t size, int min_length);

/**
 * @brief Kill partial sync marks
 * Removes syncs shorter than minimum length
 * @param gcr GCR track data (modified in place)
 * @param size Track size
 * @param min_length Minimum sync length to keep
 * @return Number of syncs removed
 */
int gcr_kill_partial_syncs(uint8_t *gcr, size_t size, int min_length);

/* ============================================================================
 * Gap Operations
 * ============================================================================ */

/**
 * @brief Find gap mark
 * @param gcr GCR data
 * @param size Data size
 * @param start Start position
 * @return Position of gap, or -1 if not found
 */
int gcr_find_gap(const uint8_t *gcr, size_t size, size_t start);

/**
 * @brief Get longest gap
 * @param gcr GCR track data
 * @param size Track size
 * @param position Output: position of longest gap (can be NULL)
 * @param gap_byte Output: gap byte value (can be NULL)
 * @return Length of longest gap
 */
size_t gcr_longest_gap(const uint8_t *gcr, size_t size, size_t *position, uint8_t *gap_byte);

/**
 * @brief Strip sync/gap runs to minimum
 * @param gcr GCR track data (modified in place)
 * @param size Track size
 * @param min_sync Minimum sync bytes to keep
 * @param min_gap Minimum gap bytes to keep
 * @return New track size
 */
size_t gcr_strip_runs(uint8_t *gcr, size_t size, int min_sync, int min_gap);

/**
 * @brief Reduce sync/gap runs
 * @param gcr GCR track data (modified in place)
 * @param size Track size
 * @param max_sync Maximum sync bytes
 * @param max_gap Maximum gap bytes
 * @return New track size
 */
size_t gcr_reduce_runs(uint8_t *gcr, size_t size, int max_sync, int max_gap);

/**
 * @brief Reduce gaps to minimum size
 * @param gcr GCR track data (modified in place)
 * @param size Track size
 * @return New track size
 */
size_t gcr_reduce_gaps(uint8_t *gcr, size_t size);

/* ============================================================================
 * Track Cycle Detection
 * ============================================================================ */

/**
 * @brief Track cycle detection result
 */
typedef struct {
    bool        found;          /**< Cycle found */
    size_t      cycle_start;    /**< Cycle start position */
    size_t      cycle_length;   /**< Cycle length */
    size_t      data_start;     /**< Data start after sync */
    int         quality;        /**< Match quality (0-100) */
} gcr_cycle_t;

/**
 * @brief Detect track cycle
 * @param gcr GCR track data
 * @param size Track size
 * @param min_length Minimum cycle length
 * @param result Output cycle information
 * @return true if cycle found
 */
bool gcr_detect_cycle(const uint8_t *gcr, size_t size, size_t min_length, gcr_cycle_t *result);

/**
 * @brief Extract track data using cycle
 * @param gcr GCR track data
 * @param size Track size
 * @param cycle Cycle information
 * @param output Output buffer (must be at least cycle_length)
 * @return Extracted length
 */
size_t gcr_extract_cycle(const uint8_t *gcr, size_t size, const gcr_cycle_t *cycle, uint8_t *output);

/* ============================================================================
 * Track Comparison
 * ============================================================================ */

/**
 * @brief Track comparison result
 */
typedef struct {
    size_t      byte_diffs;         /**< Number of different bytes */
    size_t      sector_diffs;       /**< Number of different sectors */
    int         missing_sectors1;   /**< Sectors in track1 not in track2 */
    int         missing_sectors2;   /**< Sectors in track2 not in track1 */
    float       similarity;         /**< Similarity percentage (0-100) */
    bool        same_format;        /**< Same format (sector count, density) */
    char        description[256];   /**< Comparison description */
} gcr_compare_result_t;

/**
 * @brief Compare two tracks
 * @param track1 First track data
 * @param len1 First track length
 * @param track2 Second track data
 * @param len2 Second track length
 * @param same_disk true if comparing same disk (allows minor timing diffs)
 * @param result Output comparison result
 * @return Number of differences
 */
size_t gcr_compare_tracks(const uint8_t *track1, size_t len1,
                          const uint8_t *track2, size_t len2,
                          bool same_disk, gcr_compare_result_t *result);

/**
 * @brief Compare sectors between two tracks
 * @param track1 First track data
 * @param len1 First track length
 * @param track2 Second track data
 * @param len2 Second track length
 * @param track Track number (for sector count)
 * @param id1 First disk ID (can be NULL)
 * @param id2 Second disk ID (can be NULL)
 * @param result Output comparison result
 * @return Number of sector differences
 */
size_t gcr_compare_sectors(const uint8_t *track1, size_t len1,
                           const uint8_t *track2, size_t len2,
                           int track, const uint8_t *id1, const uint8_t *id2,
                           gcr_compare_result_t *result);

/* ============================================================================
 * Track Verification
 * ============================================================================ */

/**
 * @brief Sector verification result
 */
typedef struct {
    int         sector;             /**< Sector number */
    bool        header_ok;          /**< Header checksum valid */
    bool        data_ok;            /**< Data checksum valid */
    uint8_t     header_checksum;    /**< Header checksum */
    uint8_t     data_checksum;      /**< Data checksum */
    uint8_t     track_in_header;    /**< Track number from header */
    uint8_t     sector_in_header;   /**< Sector number from header */
    uint8_t     id[2];              /**< Disk ID from header */
} gcr_sector_verify_t;

/**
 * @brief Track verification result
 */
typedef struct {
    int                 sectors_found;      /**< Number of sectors found */
    int                 sectors_good;       /**< Number of good sectors */
    int                 header_errors;      /**< Header checksum errors */
    int                 data_errors;        /**< Data checksum errors */
    int                 gcr_errors;         /**< GCR encoding errors */
    gcr_sector_verify_t sectors[21];        /**< Per-sector info (max 21) */
} gcr_verify_result_t;

/**
 * @brief Verify track data
 * @param gcr GCR track data
 * @param size Track size
 * @param track Expected track number
 * @param disk_id Expected disk ID (can be NULL)
 * @param result Output verification result
 * @return Number of errors found
 */
int gcr_verify_track(const uint8_t *gcr, size_t size, int track, 
                     const uint8_t *disk_id, gcr_verify_result_t *result);

/**
 * @brief Extract sector data from track
 * @param gcr GCR track data
 * @param size Track size
 * @param sector Sector number to extract
 * @param output Output buffer (256 bytes)
 * @param verify Output verification (can be NULL)
 * @return 0 on success, error code otherwise
 */
int gcr_extract_sector(const uint8_t *gcr, size_t size, int sector,
                       uint8_t *output, gcr_sector_verify_t *verify);

/* ============================================================================
 * Track Utilities
 * ============================================================================ */

/**
 * @brief Check if track is empty/unformatted
 * @param gcr GCR track data
 * @param size Track size
 * @return true if track appears empty
 */
bool gcr_is_empty_track(const uint8_t *gcr, size_t size);

/**
 * @brief Check if track is a killer track (all sync)
 * @param gcr GCR track data
 * @param size Track size
 * @return true if killer track
 */
bool gcr_is_killer_track(const uint8_t *gcr, size_t size);

/**
 * @brief Get track density from data
 * Analyzes track to determine most likely density
 * @param gcr GCR track data
 * @param size Track size
 * @return Density (0-3)
 */
int gcr_detect_density(const uint8_t *gcr, size_t size);

/**
 * @brief Get expected track capacity for track number
 * @param track Track number (1-42)
 * @return Expected capacity in bytes
 */
size_t gcr_expected_capacity(int track);

/**
 * @brief Get number of sectors for track number
 * @param track Track number (1-42)
 * @return Number of sectors (17-21)
 */
int gcr_sectors_per_track(int track);

/**
 * @brief Calculate checksum for sector data
 * @param data Sector data (256 bytes)
 * @return XOR checksum
 */
uint8_t gcr_calc_data_checksum(const uint8_t *data);

/**
 * @brief Calculate checksum for header
 * @param track Track number
 * @param sector Sector number
 * @param id Disk ID (2 bytes)
 * @return XOR checksum
 */
uint8_t gcr_calc_header_checksum(uint8_t track, uint8_t sector, const uint8_t *id);

/* ============================================================================
 * MD5/CRC Hashing
 * ============================================================================ */

/**
 * @brief Calculate CRC32 of track sectors
 * @param gcr GCR track data
 * @param size Track size
 * @param track Track number
 * @return CRC32 of decoded sector data
 */
uint32_t gcr_crc_track(const uint8_t *gcr, size_t size, int track);

/**
 * @brief Calculate CRC32 of directory track
 * @param gcr GCR track data (track 18)
 * @param size Track size
 * @return CRC32 of directory sectors
 */
uint32_t gcr_crc_directory(const uint8_t *gcr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GCR_OPS_H */
