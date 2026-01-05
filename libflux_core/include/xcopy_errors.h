// SPDX-License-Identifier: MIT
/*
 * xcopy_errors.h - X-Copy Error Taxonomy
 * 
 * Based on original X-Copy source code (xio.s)
 * Converted from 68000 assembler to C
 * 
 * Original error system from X-Copy Professional
 * Maps to UFM copy-protection flags
 * 
 * @version 1.0.0
 * @date 2024-12-24
 * @source xio.s lines 2613-2632
 */

#ifndef UFT_XCOPY_ERRORS_H
#define UFT_XCOPY_ERRORS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * X-COPY ERROR CODES
 *============================================================================*/

/**
 * @brief X-Copy error codes (original from X-Copy Professional)
 * 
 * These error codes were used in the original X-Copy to indicate
 * various disk reading problems. Error code 7 (LONG_TRACK) is
 * particularly important as it indicates copy protection.
 */
typedef enum {
    XCOPY_ERR_NONE = 0,          /**< No error */
    XCOPY_ERR_SECTOR_COUNT = 1,  /**< More or less than 11 sectors */
    XCOPY_ERR_NO_SYNC = 2,       /**< No sync mark found */
    XCOPY_ERR_GAP_SYNC = 3,      /**< No sync after gap */
    XCOPY_ERR_HEADER_CRC = 4,    /**< Header checksum error */
    XCOPY_ERR_HEADER_FMT = 5,    /**< Error in header/format long */
    XCOPY_ERR_DATA_CRC = 6,      /**< Data block checksum error */
    XCOPY_ERR_LONG_TRACK = 7,    /**< Long track (copy protection!) */
    XCOPY_ERR_VERIFY = 8         /**< Verify error */
} xcopy_error_t;

/**
 * @brief X-Copy error statistics for a track
 */
typedef struct {
    uint8_t error_code;          /**< X-Copy error code (0-8) */
    uint16_t sector_count;       /**< Actual sectors found */
    uint16_t expected_sectors;   /**< Expected sectors (usually 11) */
    uint32_t track_length;       /**< Track length in bytes */
    uint32_t expected_length;    /**< Expected track length */
    bool sync_found;             /**< Sync mark detected */
    bool gap_valid;              /**< Gap timing valid */
    uint32_t crc_errors;         /**< Number of CRC errors */
    bool is_protected;           /**< Copy protection detected */
} xcopy_track_error_t;

/*============================================================================*
 * ERROR MESSAGES
 *============================================================================*/

/**
 * @brief Get English error message for X-Copy error code
 * 
 * @param error Error code (1-8)
 * @return Const char* Error message string
 */
const char* xcopy_error_message(xcopy_error_t error);

/**
 * @brief Get German error message for X-Copy error code
 * 
 * @param error Error code (1-8)
 * @return Const char* Error message string (German)
 */
const char* xcopy_error_message_de(xcopy_error_t error);

/*============================================================================*
 * ERROR ANALYSIS
 *============================================================================*/

/**
 * @brief Analyze track for X-Copy style errors
 * 
 * Examines a track's data and populates error statistics
 * using X-Copy's error detection methods.
 * 
 * @param track_data Raw track data
 * @param track_length Length of track data in bytes
 * @param error Output error statistics
 * @return int 0 on success, negative on error
 */
int xcopy_analyze_track(
    const uint8_t *track_data,
    size_t track_length,
    xcopy_track_error_t *error
);

/**
 * @brief Check if error indicates copy protection
 * 
 * @param error Error code
 * @return bool true if likely copy protection
 */
static inline bool xcopy_is_protection(xcopy_error_t error)
{
    return (error == XCOPY_ERR_LONG_TRACK ||
            error == XCOPY_ERR_SECTOR_COUNT ||
            error == XCOPY_ERR_GAP_SYNC);
}

/*============================================================================*
 * UFM INTEGRATION
 *============================================================================*/

/**
 * @brief Convert X-Copy error to UFM copy-protection flags
 * 
 * Maps X-Copy error codes to UFM's ufm_cp_flags_t.
 * This allows UFM to understand copy protection indicators
 * from X-Copy's error analysis.
 * 
 * @param error X-Copy error code
 * @return uint32_t UFM copy-protection flags
 */
uint32_t xcopy_error_to_ufm_flags(xcopy_error_t error);

/**
 * @brief Analyze track and set UFM copy-protection flags
 * 
 * @param track_data Raw track data
 * @param track_length Track length in bytes
 * @param cp_flags Output UFM copy-protection flags
 * @return int 0 on success, negative on error
 */
int xcopy_analyze_for_ufm(
    const uint8_t *track_data,
    size_t track_length,
    uint32_t *cp_flags
);

/*============================================================================*
 * ERROR STATISTICS
 *============================================================================*/

/**
 * @brief Track error statistics collection
 */
typedef struct {
    uint32_t total_tracks;       /**< Total tracks analyzed */
    uint32_t error_counts[9];    /**< Count per error type (0-8) */
    uint32_t protected_tracks;   /**< Tracks with protection */
    uint32_t clean_tracks;       /**< Tracks with no errors */
} xcopy_error_stats_t;

/**
 * @brief Initialize error statistics
 * 
 * @param stats Statistics structure to initialize
 */
void xcopy_stats_init(xcopy_error_stats_t *stats);

/**
 * @brief Add track error to statistics
 * 
 * @param stats Statistics structure
 * @param error Track error information
 */
void xcopy_stats_add(xcopy_error_stats_t *stats, 
                     const xcopy_track_error_t *error);

/**
 * @brief Print error statistics
 * 
 * @param stats Statistics structure
 */
void xcopy_stats_print(const xcopy_error_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XCOPY_ERRORS_H */
