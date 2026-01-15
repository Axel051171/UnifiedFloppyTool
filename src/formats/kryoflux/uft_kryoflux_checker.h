// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file uft_kryoflux_checker.h
 * @brief KryoFlux Stream Integrity Checker & Analyzer API
 * @version 3.8.0
 */

#ifndef UFT_KRYOFLUX_CHECKER_H
#define UFT_KRYOFLUX_CHECKER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define UFT_KFC_MAX_SECTORS     32
#define UFT_KFC_MAX_REVS        10

/*============================================================================
 * STRUCTURES
 *============================================================================*/

/**
 * @brief Sector info extracted from MFM
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;
    uint16_t header_crc;
    uint16_t data_crc;
    bool     header_ok;
    bool     data_ok;
    bool     deleted;
    uint32_t flux_position;
} uft_kfc_sector_t;

/**
 * @brief Track quality info
 */
typedef struct {
    float    signal_quality;
    float    timing_variance;
    uint32_t weak_bits;
    uint32_t missing_clocks;
    uint32_t extra_clocks;
    bool     index_found;
    double   rotation_time_ms;
    float    rpm;
} uft_kfc_track_quality_t;

/**
 * @brief Stream check result
 */
typedef struct {
    /* Stream integrity */
    bool     stream_valid;
    uint32_t expected_stream_pos;
    uint32_t actual_stream_pos;
    bool     position_match;
    
    /* OOB statistics */
    uint32_t oob_count;
    uint32_t index_count;
    uint32_t overflow_count;
    
    /* Flux statistics */
    uint32_t flux_count;
    uint32_t flux_min;
    uint32_t flux_max;
    double   flux_avg;
    double   flux_stddev;
    
    /* Revolution info */
    uint8_t  revolution_count;
    double   revolution_times_ms[UFT_KFC_MAX_REVS];
    float    revolution_rpm[UFT_KFC_MAX_REVS];
    float    rpm_variance;
    
    /* Sector info */
    uint8_t  sector_count;
    uft_kfc_sector_t sectors[UFT_KFC_MAX_SECTORS];
    uint8_t  sectors_ok;
    uint8_t  sectors_bad_header;
    uint8_t  sectors_bad_data;
    
    /* Track quality */
    uft_kfc_track_quality_t quality;
    
    /* Hardware info */
    char     kf_name[64];
    char     kf_version[32];
    double   sample_clock_hz;
    
    /* Warnings/Errors */
    char     warnings[512];
    char     errors[512];
    
} uft_kfc_result_t;

/*============================================================================
 * API FUNCTIONS
 *============================================================================*/

/**
 * @brief Check KryoFlux stream file integrity
 * 
 * @param data Stream file data
 * @param len Data length
 * @param result Output result structure
 * @return 0 on success, negative on error
 */
int uft_kfc_check_stream(
    const uint8_t* data,
    size_t len,
    uft_kfc_result_t* result);

/**
 * @brief Generate text report
 * 
 * @param result Analysis result
 * @param output Output buffer
 * @param output_size Buffer size
 * @return 0 on success
 */
int uft_kfc_generate_report(
    const uft_kfc_result_t* result,
    char* output,
    size_t output_size);

/**
 * @brief Check stream file and print report
 * 
 * @param data Stream data
 * @param len Data length
 * @param output Output file (e.g., stdout)
 * @return 0 if valid, 1 if invalid, negative on error
 */
int uft_kfc_check_and_report(
    const uint8_t* data,
    size_t len,
    FILE* output);

/**
 * @brief Check if flux timing matches Atari ST DSDD
 * 
 * @param result Analysis result
 * @return true if timing is consistent with Atari ST DSDD
 */
bool uft_kfc_check_atari_st_timing(const uft_kfc_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_KRYOFLUX_CHECKER_H */
