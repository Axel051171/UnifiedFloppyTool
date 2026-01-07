/**
 * @file libflux_json_diagnostics.h
 * @brief JSON Diagnostic Export Library
 * 
 * Machine-readable diagnostic output in JSON format
 * for integration with external tools and automation.
 * 
 * CLEAN-ROOM IMPLEMENTATION
 * 
 * Copyright (C) 2025 UFT Project Contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LIBFLUX_JSON_DIAGNOSTICS_H
#define LIBFLUX_JSON_DIAGNOSTICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * DIAGNOSTIC STRUCTURES
 * ============================================================================ */

/**
 * @brief Sector diagnostic info
 */
typedef struct {
    int track;
    int head;
    int sector;
    int size;
    bool header_ok;
    bool data_ok;
    uint32_t header_crc;
    uint32_t data_crc;
    uint8_t confidence;
} libflux_sector_diag_t;

/**
 * @brief Track diagnostic info
 */
typedef struct {
    int track;
    int head;
    int bitrate;
    int encoding;
    int sectors_found;
    int sectors_ok;
    int sectors_bad;
    double rpm;
    uint8_t quality;
} libflux_track_diag_t;

/**
 * @brief Full disk diagnostic report
 */
typedef struct {
    char filename[256];
    char format[64];
    int tracks;
    int sides;
    int sectors_per_track;
    int sector_size;
    
    int total_sectors_ok;
    int total_sectors_bad;
    double overall_quality;
    
    uint32_t crc32;
    char md5[33];
    
    libflux_track_diag_t *track_diags;
    int track_count;
    
    libflux_sector_diag_t *sector_diags;
    int sector_count;
    
} libflux_disk_diag_t;

/* ============================================================================
 * API FUNCTIONS
 * ============================================================================ */

/**
 * @brief Initialize disk diagnostics
 */
void libflux_diag_init(libflux_disk_diag_t *diag);

/**
 * @brief Free disk diagnostics resources
 */
void libflux_diag_free(libflux_disk_diag_t *diag);

/**
 * @brief Allocate track diagnostics array
 */
int libflux_diag_alloc_tracks(libflux_disk_diag_t *diag, int count);

/**
 * @brief Allocate sector diagnostics array
 */
int libflux_diag_alloc_sectors(libflux_disk_diag_t *diag, int count);

/**
 * @brief Export disk diagnostics to JSON file
 */
int libflux_export_json(const libflux_disk_diag_t *diag, const char *filename);

/**
 * @brief Export disk diagnostics to JSON string
 */
int libflux_export_json_string(const libflux_disk_diag_t *diag, char *buffer, int size);

/**
 * @brief Export track diagnostics only
 */
int libflux_export_track_json(const libflux_track_diag_t *track, FILE *f);

/**
 * @brief Export sector diagnostics only
 */
int libflux_export_sector_json(const libflux_sector_diag_t *sector, FILE *f);

/**
 * @brief Quick export: error report
 */
int libflux_json_error(char *buffer, int size, int code, const char *message);

/**
 * @brief Quick export: progress report  
 */
int libflux_json_progress(char *buffer, int size, int current, int total, const char *op);

/**
 * @brief Quick export: completion report
 */
int libflux_json_complete(char *buffer, int size, bool success, int processed, int failed);

#ifdef __cplusplus
}
#endif

#endif /* LIBFLUX_JSON_DIAGNOSTICS_H */
