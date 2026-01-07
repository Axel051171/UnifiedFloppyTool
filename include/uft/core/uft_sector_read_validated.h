/**
 * @file uft_sector_read_validated.h
 * @brief Validated Sector Read API with CRC Checking (P1-002)
 * 
 * Provides sector reading with automatic CRC validation,
 * retry logic, and statistics collection for forensic analysis.
 * 
 * Usage:
 *   uft_validated_reader_t reader;
 *   uft_validated_reader_init(&reader);
 *   
 *   uft_sector_result_t result;
 *   uft_sector_read_validated(&reader, disk, cyl, head, sector,
 *                             buffer, sizeof(buffer), &result);
 *   
 *   if (result.crc_valid) { ... }
 * 
 * @version 1.0.0
 * @date 2026-01-07
 */

#ifndef UFT_SECTOR_READ_VALIDATED_H
#define UFT_SECTOR_READ_VALIDATED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "uft_crc_validate.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Forward Declarations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Disk type - defined elsewhere */
struct uft_disk;
typedef struct uft_disk uft_disk_t;

/* Error type */
typedef int uft_error_t;

/* Encoding types */
typedef enum {
    UFT_ENC_UNKNOWN = 0,
    UFT_ENC_MFM,
    UFT_ENC_FM,
    UFT_ENC_GCR_CBM,
    UFT_ENC_GCR_APPLE,
    UFT_ENC_AMIGA_MFM
} uft_encoding_id_t;

/* CRC types */
typedef enum {
    UFT_CRC_CCITT = 0,
    UFT_CRC_IBM,
    UFT_CRC_CHECKSUM
} uft_crc_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_MAX_TRACK_ERRORS    64
#define UFT_MAX_SECTOR_SIZE     8192

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sector Status
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_SECTOR_OK           = 0,
    UFT_SECTOR_CRC_ERROR    = 1,
    UFT_SECTOR_READ_ERROR   = 2,
    UFT_SECTOR_NOT_FOUND    = 3,
    UFT_SECTOR_TIMEOUT      = 4,
    UFT_SECTOR_WEAK_BITS    = 5
} uft_sector_status_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Result Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Result of a single sector read with validation
 */
typedef struct {
    /* Location */
    int             cylinder;
    int             head;
    int             sector;
    
    /* Status */
    uft_sector_status_t status;
    uft_error_t     error_code;
    
    /* CRC Information */
    bool            crc_valid;
    uint16_t        crc_expected;
    uint16_t        crc_calculated;
    
    /* Data */
    size_t          data_size;
    int             retries_used;
} uft_sector_result_t;

/**
 * @brief Result of a track read with validation
 */
typedef struct {
    /* Location */
    int             cylinder;
    int             head;
    
    /* Statistics */
    int             sectors_read;
    int             sectors_valid;
    int             sectors_with_errors;
    float           validity_percent;
    
    /* Error Details */
    int             error_count;
    int             error_sectors[UFT_MAX_TRACK_ERRORS];
    uft_error_t     error_codes[UFT_MAX_TRACK_ERRORS];
} uft_track_result_t;

/**
 * @brief Validated reader state with statistics
 */
typedef struct {
    /* Configuration */
    bool            validate_crc;
    bool            collect_stats;
    bool            retry_on_crc_error;
    int             max_retries;
    
    /* Statistics */
    uft_crc_stats_t stats;
} uft_validated_reader_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Reader Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize validated reader with defaults
 */
void uft_validated_reader_init(uft_validated_reader_t *reader);

/**
 * @brief Reset reader statistics
 */
void uft_validated_reader_reset_stats(uft_validated_reader_t *reader);

/**
 * @brief Get current statistics
 */
void uft_validated_reader_get_stats(const uft_validated_reader_t *reader,
                                     uft_crc_stats_t *stats);

/**
 * @brief Get success rate (0.0 - 100.0)
 */
double uft_validated_reader_success_rate(const uft_validated_reader_t *reader);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Validated Read Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read single sector with CRC validation
 * 
 * @param reader    Validated reader state
 * @param disk      Disk to read from
 * @param cylinder  Cylinder number
 * @param head      Head number (0 or 1)
 * @param sector    Sector number
 * @param buffer    Output buffer
 * @param buffer_size Buffer size
 * @param result    Output result details
 * @return UFT_OK on success with valid CRC, error code otherwise
 */
uft_error_t uft_sector_read_validated(
    uft_validated_reader_t *reader,
    uft_disk_t *disk,
    int cylinder,
    int head,
    int sector,
    uint8_t *buffer,
    size_t buffer_size,
    uft_sector_result_t *result);

/**
 * @brief Read entire track with CRC validation
 * 
 * @param reader    Validated reader state
 * @param disk      Disk to read from
 * @param cylinder  Cylinder number
 * @param head      Head number
 * @param result    Output result details
 * @return UFT_OK if all sectors valid, UFT_ERROR_BAD_SECTOR otherwise
 */
uft_error_t uft_track_read_validated(
    uft_validated_reader_t *reader,
    uft_disk_t *disk,
    int cylinder,
    int head,
    uft_track_result_t *result);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Diagnostic Output
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Print sector result to file
 */
void uft_sector_result_print(const uft_sector_result_t *result, FILE *out);

/**
 * @brief Print track result to file
 */
void uft_track_result_print(const uft_track_result_t *result, FILE *out);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Stub for uft_disk_read_raw_sector (implement in disk module)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read raw sector data (to be implemented per disk type)
 */
uft_error_t uft_disk_read_raw_sector(
    uft_disk_t *disk,
    int cylinder,
    int head,
    int sector,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *bytes_read);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SECTOR_READ_VALIDATED_H */
