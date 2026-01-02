// SPDX-License-Identifier: MIT
/*
 * xcopy_protection.h - Copy Protection Pattern Detection Header
 * 
 * @version 2.8.0
 * @date 2024-12-26
 */

#ifndef XCOPY_PROTECTION_H
#define XCOPY_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include "xcopy_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * COPY PROTECTION PATTERNS
 *============================================================================*/

typedef enum {
    CP_PATTERN_NONE,
    CP_PATTERN_ROB_NORTHEN,
    CP_PATTERN_GREMLIN,
    CP_PATTERN_HEXAGON,
    CP_PATTERN_COPS,
    CP_PATTERN_SPEEDLOCK,
    CP_PATTERN_LONG_TRACK,
    CP_PATTERN_NO_SECTORS,
    CP_PATTERN_WEAK_BITS,
    CP_PATTERN_SYNC_SHIFT,
    CP_PATTERN_FUZZY_BITS,
    CP_PATTERN_VARIABLE_DENSITY,
    CP_PATTERN_UNKNOWN
} cp_pattern_t;

typedef struct {
    cp_pattern_t pattern;
    const char *name;
    const char *description;
    uint32_t confidence;
} cp_detection_t;

/*============================================================================*
 * PATTERN DETECTION
 *============================================================================*/

/**
 * @brief Detect copy protection pattern
 * 
 * @param error Track error analysis
 * @param detection_out Detection result (output)
 * @return 0 on success
 */
int xcopy_detect_protection_pattern(
    const xcopy_track_error_t *error,
    cp_detection_t *detection_out
);

/**
 * @brief Check if track has Rob Northen Copylock
 * 
 * @param error Track error
 * @return true if Rob Northen detected
 */
bool xcopy_is_rob_northen(const xcopy_track_error_t *error);

/**
 * @brief Check if track has Gremlin Graphics protection
 * 
 * @param error Track error
 * @return true if Gremlin detected
 */
bool xcopy_is_gremlin_graphics(const xcopy_track_error_t *error);

/**
 * @brief Check if track has Hexagon protection
 * 
 * @param error Track error
 * @return true if Hexagon detected
 */
bool xcopy_is_hexagon(const xcopy_track_error_t *error);

/*============================================================================*
 * DISK ANALYSIS
 *============================================================================*/

typedef struct {
    int total_tracks;
    int protected_tracks;
    int clean_tracks;
    
    cp_pattern_t detected_patterns[10];
    int pattern_count;
    
    cp_pattern_t primary_pattern;
    uint32_t primary_confidence;
    
} disk_protection_t;

/**
 * @brief Analyze entire disk for copy protection
 * 
 * @param track_errors Array of track errors
 * @param track_count Number of tracks
 * @param disk_out Disk analysis (output)
 * @return 0 on success
 */
int xcopy_analyze_disk_protection(
    const xcopy_track_error_t *track_errors,
    int track_count,
    disk_protection_t *disk_out
);

/**
 * @brief Print disk protection analysis
 * 
 * @param disk Disk analysis
 */
void xcopy_print_disk_protection(const disk_protection_t *disk);

#ifdef __cplusplus
}
#endif

#endif /* XCOPY_PROTECTION_H */
