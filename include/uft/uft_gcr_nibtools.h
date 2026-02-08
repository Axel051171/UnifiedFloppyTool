/**
 * 
 * by Markus Brenner and Pete Rittwage.
 * 
 * Features:
 * - GCR encode/decode tables
 * - 1541 disk geometry with variable density zones
 * - Track cycle detection
 * - Sector extraction
 * - Bad GCR detection
 * 
 * @version 1.0.0
 */

#ifndef UFT_GCR_NIBTOOLS_H
#define UFT_GCR_NIBTOOLS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*============================================================================
 * Constants
 *============================================================================*/

/** Maximum tracks on 1541 (including extended) */
#define UFT_1541_MAX_TRACKS         42

/** Maximum half-tracks */
#define UFT_1541_MAX_HALFTRACKS     84

/** Standard track length in NIB format */
#define UFT_NIB_TRACK_LENGTH        8192

/** GCR block length (header + data) */
#define UFT_GCR_BLOCK_LEN           325

/** Minimum formatted GCR run length */
#define UFT_GCR_MIN_FORMATTED       100

/** Maximum sync offset for error conversion */
#define UFT_MAX_SYNC_OFFSET         500

/*============================================================================
 * 1541 Disk Geometry
 *============================================================================*/

/**
 * @brief Sectors per track for 1541 (variable density zones)
 * 
 * Zone 3 (T1-17):  21 sectors, speed 3 (slowest bit rate)
 * Zone 2 (T18-24): 19 sectors, speed 2
 * Zone 1 (T25-30): 18 sectors, speed 1
 * Zone 0 (T31-35): 17 sectors, speed 0 (fastest bit rate)
 * Extended (T36-42): 17 sectors (non-standard)
 */
extern const uint8_t uft_1541_sector_map[UFT_1541_MAX_TRACKS + 1];

/**
 * @brief Speed zone for each track (0=fastest, 3=slowest)
 */
extern const uint8_t uft_1541_speed_map[UFT_1541_MAX_TRACKS + 1];

/**
 * @brief Gap length between sectors
 */
extern const uint8_t uft_1541_gap_map[UFT_1541_MAX_TRACKS + 1];

/**
 * @brief Track capacity in bytes per speed zone
 */
typedef struct {
    size_t min;     /**< Minimum capacity (305 RPM) */
    size_t typical; /**< Typical capacity (300 RPM) */
    size_t max;     /**< Maximum capacity (295 RPM) */
} uft_zone_capacity_t;

extern const uft_zone_capacity_t uft_1541_zone_capacity[4];

/*============================================================================
 * GCR Encoding/Decoding
 *============================================================================*/

/**
 * @brief GCR nibble-to-5bit conversion table
 * 
 * Converts 4-bit nibble (0-15) to 5-bit GCR code
 */
extern const uint8_t uft_gcr_encode_table[16];

/**
 * @brief GCR 5bit-to-nibble conversion table (high nibble)
 */
extern const uint8_t uft_gcr_decode_high[32];

/**
 * @brief GCR 5bit-to-nibble conversion table (low nibble)
 */
extern const uint8_t uft_gcr_decode_low[32];

/**
 * @brief Convert 4 bytes to 5 GCR bytes
 * 
 * @param input     4 input bytes
 * @param output    5 output GCR bytes
 */
void uft_gcr_encode_4bytes(const uint8_t *input, uint8_t *output);

/**
 * @brief Convert 5 GCR bytes to 4 data bytes
 * 
 * @param gcr       5 GCR input bytes
 * @param output    4 output data bytes
 * @return Number of bytes successfully converted (0-4), 
 *         <4 indicates bad GCR code position
 */
int uft_gcr_decode_4bytes(const uint8_t *gcr, uint8_t *output);

/**
 * @brief Check if GCR byte at position is valid
 * 
 * @param data      GCR data buffer
 * @param length    Buffer length
 * @param pos       Position to check
 * @return true if valid GCR, false if bad
 */
bool uft_gcr_is_bad(const uint8_t *data, size_t length, size_t pos);

/*============================================================================
 * Sync Detection
 *============================================================================*/

/**
 * @brief Find next sync mark in GCR data
 * 
 * Sync is at least 10 consecutive '1' bits (0xFF bytes)
 * 
 * @param gcr_ptr   Pointer to current position (updated on return)
 * @param gcr_end   End of buffer
 * @return true if sync found, false if end reached
 */
bool uft_gcr_find_sync(uint8_t **gcr_ptr, const uint8_t *gcr_end);

/**
 * @brief Find sector header after sync
 * 
 * Header starts with 0x52 (GCR-encoded 0x08)
 * 
 * @param gcr_ptr   Pointer to current position (updated on return)
 * @param gcr_end   End of buffer
 * @return true if header found, false if end reached
 */
bool uft_gcr_find_header(uint8_t **gcr_ptr, const uint8_t *gcr_end);

/*============================================================================
 * Track Analysis
 *============================================================================*/

/**
 * @brief Find track cycle (where data repeats)
 * 
 * @param track_data    Raw track data (should be >= 2 revolutions)
 * @param data_length   Length of track data
 * @param cap_min       Minimum expected track capacity
 * @param cap_max       Maximum expected track capacity
 * @param cycle_start   Output: start of cycle
 * @param cycle_end     Output: end of cycle
 * @return Cycle length in bytes, or data_length if no cycle found
 */
size_t uft_gcr_find_track_cycle(const uint8_t *track_data,
                                size_t data_length,
                                size_t cap_min,
                                size_t cap_max,
                                const uint8_t **cycle_start,
                                const uint8_t **cycle_end);

/**
 * @brief Find sector 0 position
 * 
 * @param track_data    Track data buffer
 * @param track_len     Track length
 * @param sector_len    Output: sector length (if found)
 * @return Pointer to sector 0 sync, or NULL if not found
 */
const uint8_t *uft_gcr_find_sector0(const uint8_t *track_data,
                                    size_t track_len,
                                    size_t *sector_len);

/**
 * @brief Find largest sector gap (for track alignment)
 * 
 * @param track_data    Track data buffer
 * @param track_len     Track length
 * @param gap_len       Output: gap length
 * @return Pointer to gap position, or NULL if not found
 */
const uint8_t *uft_gcr_find_sector_gap(const uint8_t *track_data,
                                       size_t track_len,
                                       size_t *gap_len);

/**
 * @brief Check if track contains formatted data
 * 
 * @param track_data    Track data buffer
 * @param length        Data length
 * @return true if formatted GCR data found
 */
bool uft_gcr_is_formatted(const uint8_t *track_data, size_t length);

/*============================================================================
 * Sector Operations
 *============================================================================*/

/** Sector extraction result codes */
typedef enum {
    UFT_SECTOR_OK = 0,
    UFT_SECTOR_SYNC_NOT_FOUND,
    UFT_SECTOR_HEADER_NOT_FOUND,
    UFT_SECTOR_DATA_NOT_FOUND,
    UFT_SECTOR_HEADER_CHECKSUM_ERROR,
    UFT_SECTOR_DATA_CHECKSUM_ERROR,
    UFT_SECTOR_ID_MISMATCH,
    UFT_SECTOR_BAD_GCR
} uft_sector_error_t;

/**
 * @brief 1541 sector header structure
 */
typedef struct {
    uint8_t type;           /**< Header type (0x08) */
    uint8_t checksum;       /**< Header checksum */
    uint8_t sector;         /**< Sector number */
    uint8_t track;          /**< Track number */
    uint8_t id2;            /**< Disk ID byte 2 */
    uint8_t id1;            /**< Disk ID byte 1 */
    uint8_t gap_byte1;      /**< Gap byte (0x0F) */
    uint8_t gap_byte2;      /**< Gap byte (0x0F) */
} uft_1541_header_t;

/**
 * @brief Extract sector from GCR track data
 * 
 * @param gcr_start     Start of GCR track data
 * @param gcr_cycle     End of track cycle
 * @param sector_buf    Output buffer (260 bytes: type + data + checksum)
 * @param track         Expected track number
 * @param sector        Expected sector number
 * @param disk_id       Expected disk ID (2 bytes)
 * @return Error code
 */
uft_sector_error_t uft_gcr_extract_sector(const uint8_t *gcr_start,
                                          const uint8_t *gcr_cycle,
                                          uint8_t *sector_buf,
                                          int track,
                                          int sector,
                                          const uint8_t *disk_id);

/**
 * @brief Extract disk ID from track 18
 * 
 * @param gcr_track     Track 18 GCR data
 * @param id            Output: 2-byte disk ID
 * @return true if ID found
 */
bool uft_gcr_extract_id(const uint8_t *gcr_track, uint8_t *id);

/*============================================================================
 * Bitshift Alignment
 *============================================================================*/

/**
 * @brief Check if track is bitshifted (not byte-aligned)
 * 
 * @param track_data    Track data
 * @param track_len     Track length
 * @return true if bitshifted
 */
bool uft_gcr_is_bitshifted(const uint8_t *track_data, size_t track_len);

/**
 * @brief Align bitshifted track to byte boundaries
 * 
 * Inserts padding bits before sync marks to align data sectors
 * 
 * @param track_data    Track data (modified in place)
 * @param track_len     Track length
 * @param aligned_start Output: aligned data start
 * @param aligned_len   Output: aligned data length
 * @return 1 = aligned, 0 = no sync found, -1 = empty track
 */
int uft_gcr_align_bitshifted(uint8_t *track_data,
                             size_t track_len,
                             uint8_t **aligned_start,
                             size_t *aligned_len);

/**
 * @brief Shift buffer left by N bits
 * 
 * @param buffer    Buffer to shift
 * @param length    Buffer length
 * @param bits      Number of bits (1-7)
 */
void uft_gcr_shift_left(uint8_t *buffer, size_t length, int bits);

/**
 * @brief Shift buffer right by N bits
 * 
 * @param buffer    Buffer to shift
 * @param length    Buffer length
 * @param bits      Number of bits (1-7)
 */
void uft_gcr_shift_right(uint8_t *buffer, size_t length, int bits);

/*============================================================================
 * Initialization
 *============================================================================*/

/**
 * @brief Initialize GCR codec tables
 * 
 * Call once at startup
 */
void uft_gcr_init(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GCR_NIBTOOLS_H */
