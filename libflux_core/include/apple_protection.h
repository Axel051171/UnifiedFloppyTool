// SPDX-License-Identifier: MIT
/*
 * apple_protection.h - Apple II Copy Protection Patterns Header
 * 
 * @version 2.8.2
 * @date 2024-12-26
 */

#ifndef APPLE_PROTECTION_H
#define APPLE_PROTECTION_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * PROTECTION TYPES
 *============================================================================*/

typedef enum {
    PROTECTION_TYPE_TIMING,         /* Timing-based */
    PROTECTION_TYPE_NIBBLE_COUNT,   /* Nibble count variations */
    PROTECTION_TYPE_SYNC_PATTERN,   /* Modified sync patterns */
    PROTECTION_TYPE_TRACK_LAYOUT,   /* Non-standard track layout */
    PROTECTION_TYPE_SECTOR_EDITOR,  /* Sector editor tricks */
    PROTECTION_TYPE_HALF_TRACK,     /* Half-track positioning */
    PROTECTION_TYPE_CUSTOM_DOS,     /* Custom DOS modifications */
    PROTECTION_TYPE_CUSTOM_RWTS     /* Custom RWTS */
} apple_protection_type_t;

/*============================================================================*
 * STRUCTURES
 *============================================================================*/

/**
 * @brief Apple II protection pattern
 */
typedef struct {
    const char *name;               /* Protection name */
    apple_protection_type_t type;   /* Protection type */
    
    uint8_t signature[16];          /* Signature bytes */
    size_t signature_len;           /* Signature length */
    
    uint8_t track_pattern[35];      /* Affected tracks */
    size_t track_pattern_len;       /* Number of tracks */
    
    const char *description;        /* Description */
} apple_protection_pattern_t;

/*============================================================================*
 * FUNCTIONS
 *============================================================================*/

/**
 * @brief Detect protection by signature
 * 
 * @param data Data to scan
 * @param len Data length
 * @return Protection pattern or NULL
 */
const apple_protection_pattern_t* apple_protection_detect_signature(
    const uint8_t *data, size_t len);

/**
 * @brief Detect protection by track analysis
 * 
 * @param track Track number
 * @param track_data Track data
 * @param len Data length
 * @return Protection pattern or NULL
 */
const apple_protection_pattern_t* apple_protection_detect_track(
    uint8_t track, const uint8_t *track_data, size_t len);

/**
 * @brief Get all known protection patterns
 * 
 * @param patterns_out Array of patterns (output)
 * @return Number of patterns
 */
size_t apple_protection_get_all(const apple_protection_pattern_t **patterns_out);

/**
 * @brief Print protection info
 * 
 * @param pattern Protection pattern
 */
void apple_protection_print_info(const apple_protection_pattern_t *pattern);

#ifdef __cplusplus
}
#endif

#endif /* APPLE_PROTECTION_H */
