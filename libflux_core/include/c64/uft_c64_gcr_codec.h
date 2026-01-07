/*
 * uft_c64_gcr_codec.h
 *
 * Complete GCR (Group Code Recording) codec for Commodore 64/1541.
 *
 * GCR Encoding (4-to-5):
 * - Each 4-bit nibble is encoded as 5 bits
 * - Ensures no more than 2 consecutive zeros (RLL constraint)
 * - 4 bytes become 5 bytes (20 data bits → 25 GCR bits, 8 sync bits)
 *
 * Original Source:
 *
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_c64_gcr_codec.c
 */

#ifndef UFT_C64_GCR_CODEC_H
#define UFT_C64_GCR_CODEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR ENCODING TABLES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Get pointer to GCR encode table (nibble → 5-bit GCR).
 * Table has 16 entries for nibbles 0x0-0xF.
 *
 * @return   Pointer to 16-byte encode table
 */
const uint8_t *uft_c64_gcr_encode_table(void);

/**
 * Get pointer to GCR decode table for high nibble.
 * Table has 32 entries, 0xFF = invalid GCR.
 *
 * @return   Pointer to 32-byte decode table (high nibble)
 */
const uint8_t *uft_c64_gcr_decode_high_table(void);

/**
 * Get pointer to GCR decode table for low nibble.
 * Table has 32 entries, 0xFF = invalid GCR.
 *
 * @return   Pointer to 32-byte decode table (low nibble)
 */
const uint8_t *uft_c64_gcr_decode_low_table(void);

/* ═══════════════════════════════════════════════════════════════════════════
 * LOW-LEVEL GCR CONVERSION (4 bytes ↔ 5 GCR bytes)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Encode 4 bytes to 5 GCR bytes.
 *
 * Layout (bits):
 *   Input:  AAAA AAAA | BBBB BBBB | CCCC CCCC | DDDD DDDD  (32 bits)
 *   Output: aaaaa bbb | bb ccccc | dddd eeee | e ffff f | ggg ggggg  (40 bits)
 *
 * Where a=encode(A>>4), b=encode(A&0xF), etc.
 *
 * @param plain      Input: 4 bytes to encode
 * @param gcr_out    Output: 5 GCR bytes (must have space for 5 bytes)
 */
void uft_c64_gcr_encode_4bytes(const uint8_t *plain, uint8_t *gcr_out);

/**
 * Decode 5 GCR bytes to 4 bytes.
 *
 * @param gcr        Input: 5 GCR bytes
 * @param plain_out  Output: 4 decoded bytes
 * @return           Number of bytes successfully decoded (0-4).
 *                   Returns position of first bad GCR if <4.
 */
int uft_c64_gcr_decode_4bytes(const uint8_t *gcr, uint8_t *plain_out);

/**
 * Check if a 5-bit GCR value is valid.
 *
 * @param gcr5       5-bit GCR value (0-31)
 * @return           true if valid GCR, false if invalid
 */
bool uft_c64_gcr_is_valid(uint8_t gcr5);

/**
 * Encode a single nibble to 5-bit GCR.
 *
 * @param nibble     4-bit value (0-15)
 * @return           5-bit GCR value
 */
uint8_t uft_c64_gcr_encode_nibble(uint8_t nibble);

/**
 * Decode a single 5-bit GCR to nibble.
 *
 * @param gcr5       5-bit GCR value (0-31)
 * @return           4-bit nibble, or 0xFF if invalid GCR
 */
uint8_t uft_c64_gcr_decode_nibble(uint8_t gcr5);

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTOR-LEVEL OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * GCR sector decode result structure.
 */
typedef struct uft_c64_sector_result {
    uint8_t  error_code;       /* 1541 error code (SECTOR_OK=1, etc.) */
    uint8_t  header_track;     /* Track from header */
    uint8_t  header_sector;    /* Sector from header */
    uint8_t  header_checksum;  /* Header checksum */
    uint8_t  data_checksum;    /* Data block checksum */
    uint8_t  disk_id[2];       /* Disk ID from header */
    bool     header_ok;        /* Header decoded successfully */
    bool     data_ok;          /* Data decoded successfully */
    uint32_t bad_gcr_count;    /* Number of bad GCR bytes found */
} uft_c64_sector_result_t;

/**
 * Decode a GCR sector from track data.
 *
 * Searches for sector header, verifies checksums, and decodes data.
 *
 * @param gcr_start    Start of track GCR data
 * @param gcr_end      End of track GCR data (or cycle point)
 * @param track        Expected track number
 * @param sector       Expected sector number
 * @param disk_id      Expected disk ID (2 bytes)
 * @param data_out     Output: 256 bytes of decoded sector data
 * @param result       Output: Decode result details (can be NULL)
 * @return             Error code (UFT_C64_ERR_SECTOR_OK on success)
 */
uint8_t uft_c64_gcr_decode_sector(const uint8_t *gcr_start,
                                  const uint8_t *gcr_end,
                                  int track, int sector,
                                  const uint8_t *disk_id,
                                  uint8_t *data_out,
                                  uft_c64_sector_result_t *result);

/**
 * Encode a sector to GCR format.
 *
 * Creates complete sector: sync + header + gap + sync + data + tail gap.
 *
 * @param data         Input: 256 bytes of sector data
 * @param track        Track number (1-42)
 * @param sector       Sector number
 * @param disk_id      Disk ID (2 bytes)
 * @param error        Error to simulate (SECTOR_OK for normal)
 * @param gcr_out      Output: GCR sector data
 * @param gcr_size     Size of output buffer (should be >= 360 bytes)
 * @return             Number of GCR bytes written
 */
size_t uft_c64_gcr_encode_sector(const uint8_t *data,
                                 int track, int sector,
                                 const uint8_t *disk_id,
                                 uint8_t error,
                                 uint8_t *gcr_out,
                                 size_t gcr_size);

/* ═══════════════════════════════════════════════════════════════════════════
 * HEADER OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Extract disk ID from track data.
 *
 * Searches for first valid sector header and extracts disk ID.
 *
 * @param gcr_track    Track GCR data
 * @param length       Track length in bytes
 * @param id_out       Output: 2-byte disk ID
 * @return             true if ID found, false otherwise
 */
bool uft_c64_gcr_extract_id(const uint8_t *gcr_track, size_t length,
                            uint8_t *id_out);

/**
 * Extract "cosmetic" disk ID from track 18 sector 0 (BAM).
 *
 * This reads the user-visible disk ID from the BAM, which may
 * differ from the physical header ID on protected disks.
 *
 * @param gcr_track    Track 18 GCR data
 * @param length       Track length
 * @param id_out       Output: 2-byte cosmetic ID
 * @return             true if found, false otherwise
 */
bool uft_c64_gcr_extract_cosmetic_id(const uint8_t *gcr_track, size_t length,
                                     uint8_t *id_out);

/**
 * Decode a sector header from GCR.
 *
 * @param gcr          Pointer to header GCR data (10 bytes after sync)
 * @param header_out   Output: 8-byte decoded header
 * @return             true if header valid, false if bad GCR
 */
bool uft_c64_gcr_decode_header(const uint8_t *gcr, uint8_t *header_out);

/**
 * Encode a sector header to GCR.
 *
 * @param track        Track number
 * @param sector       Sector number
 * @param disk_id      2-byte disk ID
 * @param gcr_out      Output: 10-byte GCR header
 */
void uft_c64_gcr_encode_header(int track, int sector,
                               const uint8_t *disk_id,
                               uint8_t *gcr_out);

/* ═══════════════════════════════════════════════════════════════════════════
 * SYNC DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Find next sync mark in GCR data.
 *
 * Sync is 10+ consecutive '1' bits, typically $FF $FF... followed
 * by a byte with MSB set.
 *
 * @param gcr          Pointer to current position (updated on return)
 * @param gcr_end      End of buffer
 * @return             true if sync found, false if end reached
 */
bool uft_c64_gcr_find_sync(const uint8_t **gcr, const uint8_t *gcr_end);

/**
 * Find next sector header in GCR data.
 *
 * Looks for sync followed by header marker ($52 in GCR = $08 decoded).
 *
 * @param gcr          Pointer to current position (updated on return)
 * @param gcr_end      End of buffer
 * @return             true if header found, false if end reached
 */
bool uft_c64_gcr_find_header(const uint8_t **gcr, const uint8_t *gcr_end);

/**
 * Count sync bytes at current position.
 *
 * @param gcr          Start position
 * @param gcr_end      End of buffer
 * @return             Number of consecutive $FF bytes
 */
size_t uft_c64_gcr_count_sync(const uint8_t *gcr, const uint8_t *gcr_end);

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK CYCLE DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Find track cycle using sector headers.
 *
 * Looks for repeating sector header pattern to determine true track length.
 *
 * @param gcr_data     Track GCR data
 * @param max_length   Maximum search length
 * @param cap_min      Minimum expected capacity
 * @param cap_max      Maximum expected capacity
 * @param cycle_start  Output: Start of cycle
 * @param cycle_end    Output: End of cycle
 * @return             Cycle length in bytes, or 0 if not found
 */
size_t uft_c64_gcr_find_cycle_headers(const uint8_t *gcr_data,
                                      size_t max_length,
                                      size_t cap_min, size_t cap_max,
                                      const uint8_t **cycle_start,
                                      const uint8_t **cycle_end);

/**
 * Find track cycle using sync marks.
 *
 * @param gcr_data     Track GCR data
 * @param max_length   Maximum search length
 * @param cap_min      Minimum expected capacity
 * @param cap_max      Maximum expected capacity
 * @param cycle_start  Output: Start of cycle
 * @param cycle_end    Output: End of cycle
 * @return             Cycle length in bytes, or 0 if not found
 */
size_t uft_c64_gcr_find_cycle_syncs(const uint8_t *gcr_data,
                                    size_t max_length,
                                    size_t cap_min, size_t cap_max,
                                    const uint8_t **cycle_start,
                                    const uint8_t **cycle_end);

/**
 * Find track cycle using raw byte comparison.
 *
 * Fallback method when headers/syncs aren't usable.
 *
 * @param gcr_data     Track GCR data
 * @param max_length   Maximum search length
 * @param cap_min      Minimum expected capacity
 * @param cap_max      Maximum expected capacity
 * @param cycle_start  Output: Start of cycle
 * @param cycle_end    Output: End of cycle
 * @return             Cycle length in bytes, or 0 if not found
 */
size_t uft_c64_gcr_find_cycle_raw(const uint8_t *gcr_data,
                                  size_t max_length,
                                  size_t cap_min, size_t cap_max,
                                  const uint8_t **cycle_start,
                                  const uint8_t **cycle_end);

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK-LEVEL OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Extract and process a GCR track.
 *
 * Finds cycle, aligns, and copies clean track data.
 *
 * @param dest         Output buffer
 * @param source       Source track data
 * @param source_len   Source length
 * @param track        Track number (for capacity lookup)
 * @param align_out    Output: Alignment method used (can be NULL)
 * @return             Extracted track length
 */
size_t uft_c64_gcr_extract_track(uint8_t *dest,
                                 const uint8_t *source,
                                 size_t source_len,
                                 int track,
                                 uint8_t *align_out);

/**
 * Check if track appears to be formatted.
 *
 * @param gcr_data     Track GCR data
 * @param length       Track length
 * @return             true if track has valid GCR structure
 */
bool uft_c64_gcr_is_formatted(const uint8_t *gcr_data, size_t length);

/**
 * Find sector 0 in track data.
 *
 * @param gcr_data       Track GCR data
 * @param length         Track length
 * @param sector_len_out Output: Length of sector 0 (can be NULL)
 * @return               Pointer to sector 0, or NULL if not found
 */
const uint8_t *uft_c64_gcr_find_sector0(const uint8_t *gcr_data,
                                        size_t length,
                                        size_t *sector_len_out);

/**
 * Find sector gap (inter-sector region).
 *
 * @param gcr_data       Track GCR data
 * @param length         Track length
 * @param gap_len_out    Output: Length of gap (can be NULL)
 * @return               Pointer to gap, or NULL if not found
 */
const uint8_t *uft_c64_gcr_find_sector_gap(const uint8_t *gcr_data,
                                           size_t length,
                                           size_t *gap_len_out);

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR VALIDATION AND REPAIR
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Check for bad GCR at position.
 *
 * @param gcr_data     Track GCR data
 * @param length       Track length
 * @param pos          Position to check
 * @return             true if bad GCR at position
 */
bool uft_c64_gcr_is_bad_at(const uint8_t *gcr_data, size_t length, size_t pos);

/**
 * Count total bad GCR bytes in track.
 *
 * @param gcr_data     Track GCR data
 * @param length       Track length
 * @return             Number of bad GCR positions
 */
size_t uft_c64_gcr_count_bad(const uint8_t *gcr_data, size_t length);

/**
 * Check track for various errors.
 *
 * @param gcr_data     Track GCR data
 * @param length       Track length
 * @param track        Track number
 * @param disk_id      Expected disk ID
 * @param error_str    Output: Error description string (can be NULL)
 * @param error_size   Size of error string buffer
 * @return             Number of errors found
 */
size_t uft_c64_gcr_check_errors(const uint8_t *gcr_data, size_t length,
                                int track, const uint8_t *disk_id,
                                char *error_str, size_t error_size);

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK MANIPULATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Replace all occurrences of a byte.
 *
 * @param buffer       Track data (modified in place)
 * @param length       Track length
 * @param old_byte     Byte to replace
 * @param new_byte     Replacement byte
 * @return             Number of replacements made
 */
size_t uft_c64_gcr_replace_bytes(uint8_t *buffer, size_t length,
                                 uint8_t old_byte, uint8_t new_byte);

/**
 * Strip runs of a target byte.
 *
 * Removes sequences of 'target' longer than 'min_run'.
 *
 * @param buffer       Track data (modified in place)
 * @param length       Current length
 * @param max_length   Maximum allowed length
 * @param min_run      Minimum run length to strip
 * @param target       Target byte to strip
 * @return             New length after stripping
 */
size_t uft_c64_gcr_strip_runs(uint8_t *buffer, size_t length,
                              size_t max_length, size_t min_run,
                              uint8_t target);

/**
 * Reduce runs of a target byte.
 *
 * Shortens sequences of 'target' to 'min_run' length.
 *
 * @param buffer       Track data (modified in place)
 * @param length       Current length
 * @param max_length   Maximum allowed length
 * @param min_run      Target run length
 * @param target       Target byte to reduce
 * @return             New length after reducing
 */
size_t uft_c64_gcr_reduce_runs(uint8_t *buffer, size_t length,
                               size_t max_length, size_t min_run,
                               uint8_t target);

/**
 * Strip inter-sector gaps.
 *
 * @param buffer       Track data (modified in place)
 * @param length       Current length
 * @return             New length after stripping
 */
size_t uft_c64_gcr_strip_gaps(uint8_t *buffer, size_t length);

/**
 * Reduce inter-sector gaps.
 *
 * @param buffer       Track data (modified in place)
 * @param length       Current length
 * @param max_length   Maximum allowed length
 * @return             New length after reducing
 */
size_t uft_c64_gcr_reduce_gaps(uint8_t *buffer, size_t length,
                               size_t max_length);

/**
 * Lengthen sync marks.
 *
 * Extends short sync marks to improve read reliability.
 *
 * @param buffer       Track data (modified in place)
 * @param length       Current length
 * @param max_length   Maximum allowed length
 * @return             New length after lengthening
 */
size_t uft_c64_gcr_lengthen_sync(uint8_t *buffer, size_t length,
                                 size_t max_length);

/* ═══════════════════════════════════════════════════════════════════════════
 * PETSCII CONVERSION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Convert ASCII character to PETSCII.
 *
 * @param ascii        ASCII character
 * @return             PETSCII character
 */
char uft_c64_to_petscii(char ascii);

/**
 * Convert PETSCII character to ASCII.
 *
 * @param petscii      PETSCII character
 * @return             ASCII character
 */
char uft_c64_from_petscii(char petscii);

/**
 * Convert ASCII string to PETSCII in place.
 *
 * @param str          String to convert
 * @param len          String length
 */
void uft_c64_str_to_petscii(char *str, size_t len);

/**
 * Convert PETSCII string to ASCII in place.
 *
 * @param str          String to convert
 * @param len          String length
 */
void uft_c64_str_from_petscii(char *str, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_C64_GCR_CODEC_H */
