// SPDX-License-Identifier: MIT
/*
 * woz_analyzer.h - WOZ Analysis and Protection Detection
 * 
 * Advanced WOZ analysis inspired by wozardry (4am team)
 * 
 * @version 2.8.6
 * @date 2024-12-26
 */

#ifndef WOZ_ANALYZER_H
#define WOZ_ANALYZER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * TRACK QUALITY METRICS
 *============================================================================*/

typedef struct {
    float timing_quality;       /* 0.0-1.0: Bit timing consistency */
    float sync_quality;         /* 0.0-1.0: Sync byte quality */
    float data_quality;         /* 0.0-1.0: Overall data integrity */
    uint32_t sync_count;        /* Number of sync bytes found */
    uint32_t error_count;       /* Decoding errors detected */
    bool has_weak_bits;         /* Weak bit areas detected */
    bool has_long_sync;         /* Extended sync detected (protection?) */
} woz_track_quality_t;

/*============================================================================*
 * PROTECTION DETECTION
 *============================================================================*/

typedef enum {
    WOZ_PROTECTION_NONE = 0,
    WOZ_PROTECTION_HALF_TRACK,      /* Half-track stepping */
    WOZ_PROTECTION_SPIRAL,          /* Spiral track */
    WOZ_PROTECTION_BIT_SLIP,        /* Intentional bit timing errors */
    WOZ_PROTECTION_LONG_SYNC,       /* Extended sync bytes */
    WOZ_PROTECTION_WEAK_BITS,       /* Weak bit areas */
    WOZ_PROTECTION_CUSTOM_FORMAT,   /* Non-standard sector format */
    WOZ_PROTECTION_CROSS_TRACK,     /* Cross-track dependencies */
    WOZ_PROTECTION_EA,              /* Electronic Arts */
    WOZ_PROTECTION_OPTIMUM,         /* Optimum Resource */
    WOZ_PROTECTION_PROLOK,          /* ProLok */
    WOZ_PROTECTION_UNKNOWN          /* Unknown protection */
} woz_protection_type_t;

typedef struct {
    woz_protection_type_t type;
    char description[256];
    float confidence;           /* 0.0-1.0 */
    uint8_t track;             /* Track where detected */
    uint32_t offset;           /* Bit offset in track */
} woz_protection_info_t;

/*============================================================================*
 * NIBBLE ANALYSIS
 *============================================================================*/

typedef struct {
    uint8_t *nibbles;           /* Decoded nibbles */
    size_t count;               /* Number of nibbles */
    uint32_t *bit_positions;    /* Bit position for each nibble */
    bool *valid;                /* Valid nibble flags */
} woz_nibble_data_t;

/*============================================================================*
 * ANALYSIS RESULTS
 *============================================================================*/

typedef struct {
    /* Track quality */
    woz_track_quality_t *track_quality;  /* Per-track quality */
    uint8_t num_tracks;
    
    /* Protection detection */
    woz_protection_info_t *protections;  /* Detected protections */
    size_t num_protections;
    
    /* Statistics */
    uint32_t total_bits;
    uint32_t total_sync_bytes;
    uint32_t total_errors;
    float overall_quality;
    
    /* Metadata */
    char format_name[64];       /* Detected disk format */
    bool is_copy_protected;
    bool is_flux_normalized;
} woz_analysis_t;

/*============================================================================*
 * ANALYZER API
 *============================================================================*/

/**
 * @brief Analyze WOZ image for quality and protections
 * 
 * @param woz_filename Path to WOZ file
 * @param analysis Output analysis results
 * @return true on success
 */
bool woz_analyze(const char *woz_filename, woz_analysis_t *analysis);

/**
 * @brief Analyze single track quality
 * 
 * @param track_data Track data
 * @param bit_count Number of bits in track
 * @param quality Output quality metrics
 * @return true on success
 */
bool woz_analyze_track_quality(const uint8_t *track_data, uint32_t bit_count,
                                woz_track_quality_t *quality);

/**
 * @brief Detect protection patterns in track
 * 
 * @param track_data Track data
 * @param bit_count Number of bits
 * @param protections Output protection list
 * @param max_protections Maximum protections to detect
 * @return Number of protections detected
 */
size_t woz_detect_protections(const uint8_t *track_data, uint32_t bit_count,
                              woz_protection_info_t *protections,
                              size_t max_protections);

/**
 * @brief Decode track to nibbles
 * 
 * @param track_data Track data
 * @param bit_count Number of bits
 * @param nibbles Output nibble data
 * @return true on success
 */
bool woz_decode_nibbles(const uint8_t *track_data, uint32_t bit_count,
                        woz_nibble_data_t *nibbles);

/**
 * @brief Validate bit timing
 * 
 * @param track_data Track data
 * @param bit_count Number of bits
 * @param optimal_timing Expected timing (e.g., 32 for 4us)
 * @return Timing quality score (0.0-1.0)
 */
float woz_validate_timing(const uint8_t *track_data, uint32_t bit_count,
                         uint8_t optimal_timing);

/**
 * @brief Free analysis results
 * 
 * @param analysis Analysis to free
 */
void woz_analysis_free(woz_analysis_t *analysis);

/**
 * @brief Free nibble data
 * 
 * @param nibbles Nibble data to free
 */
void woz_nibbles_free(woz_nibble_data_t *nibbles);

/**
 * @brief Get protection type name
 * 
 * @param type Protection type
 * @return Protection name string
 */
const char *woz_protection_name(woz_protection_type_t type);

/**
 * @brief Print analysis report
 * 
 * @param analysis Analysis results
 */
void woz_print_analysis(const woz_analysis_t *analysis);

#ifdef __cplusplus
}
#endif

#endif /* WOZ_ANALYZER_H */
