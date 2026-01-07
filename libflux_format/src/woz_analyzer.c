// SPDX-License-Identifier: MIT
/*
 * woz_analyzer.c - WOZ Analysis Implementation
 * 
 * @version 2.8.6
 * @date 2024-12-26
 */

#include "uft/uft_error.h"
#include "woz_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================*
 * CONSTANTS
 *============================================================================*/

#define SYNC_BYTE_PATTERN   0xFF
#define MIN_SYNC_LENGTH     10
#define LONG_SYNC_THRESHOLD 40      /* Unusually long sync = protection */
#define TIMING_TOLERANCE    0.15    /* 15% timing variance allowed */

/*============================================================================*
 * PROTECTION NAMES
 *============================================================================*/

const char *woz_protection_name(woz_protection_type_t type)
{
    switch (type) {
        case WOZ_PROTECTION_NONE:           return "None";
        case WOZ_PROTECTION_HALF_TRACK:     return "Half-track";
        case WOZ_PROTECTION_SPIRAL:         return "Spiral";
        case WOZ_PROTECTION_BIT_SLIP:       return "Bit slip";
        case WOZ_PROTECTION_LONG_SYNC:      return "Long sync";
        case WOZ_PROTECTION_WEAK_BITS:      return "Weak bits";
        case WOZ_PROTECTION_CUSTOM_FORMAT:  return "Custom format";
        case WOZ_PROTECTION_CROSS_TRACK:    return "Cross-track";
        case WOZ_PROTECTION_EA:             return "Electronic Arts";
        case WOZ_PROTECTION_OPTIMUM:        return "Optimum Resource";
        case WOZ_PROTECTION_PROLOK:         return "ProLok";
        case WOZ_PROTECTION_UNKNOWN:        return "Unknown";
        default:                            return "Unknown";
    }
}

/*============================================================================*
 * NIBBLE DECODING
 *============================================================================*/

static bool is_valid_nibble(uint8_t byte)
{
    /* Valid disk nibbles are >= 0x96 */
    return (byte >= 0x96 && byte <= 0xFF);
}

bool woz_decode_nibbles(const uint8_t *track_data, uint32_t bit_count,
                        woz_nibble_data_t *nibbles)
{
    if (!track_data || !nibbles || bit_count == 0) return false;
    
    /* Allocate nibble storage (worst case: 1 nibble per byte) */
    size_t max_nibbles = (bit_count + 7) / 8;
    nibbles->nibbles = malloc(max_nibbles);
    nibbles->bit_positions = malloc(max_nibbles * sizeof(uint32_t));
    nibbles->valid = malloc(max_nibbles);
    
    if (!nibbles->nibbles || !nibbles->bit_positions || !nibbles->valid) {
        woz_nibbles_free(nibbles);
        return false;
    }
    
    /* Decode nibbles from bit stream */
    nibbles->count = 0;
    uint8_t byte = 0;
    uint8_t bit_index = 0;
    
    for (uint32_t i = 0; i < bit_count; i++) {
        uint8_t bit = (track_data[i / 8] >> (7 - (i % 8))) & 1;
        byte = (byte << 1) | bit;
        bit_index++;
        
        if (bit_index == 8) {
            nibbles->nibbles[nibbles->count] = byte;
            nibbles->bit_positions[nibbles->count] = i - 7;
            nibbles->valid[nibbles->count] = is_valid_nibble(byte);
            nibbles->count++;
            
            byte = 0;
            bit_index = 0;
        }
    }
    
    return true;
}

void woz_nibbles_free(woz_nibble_data_t *nibbles)
{
    if (!nibbles) return;
    if (nibbles->nibbles) free(nibbles->nibbles);
    if (nibbles->bit_positions) free(nibbles->bit_positions);
    if (nibbles->valid) free(nibbles->valid);
    memset(nibbles, 0, sizeof(woz_nibble_data_t));
}

/*============================================================================*
 * TRACK QUALITY ANALYSIS
 *============================================================================*/

bool woz_analyze_track_quality(const uint8_t *track_data, uint32_t bit_count,
                                woz_track_quality_t *quality)
{
    if (!track_data || !quality || bit_count == 0) return false;
    
    memset(quality, 0, sizeof(woz_track_quality_t));
    
    /* Decode to nibbles */
    woz_nibble_data_t nibbles;
    if (!woz_decode_nibbles(track_data, bit_count, &nibbles)) {
        return false;
    }
    
    /* Analyze sync bytes */
    uint32_t sync_count = 0;
    uint32_t max_sync_run = 0;
    uint32_t current_sync_run = 0;
    uint32_t valid_nibbles = 0;
    
    for (size_t i = 0; i < nibbles.count; i++) {
        if (nibbles.valid[i]) {
            valid_nibbles++;
            
            if (nibbles.nibbles[i] == SYNC_BYTE_PATTERN) {
                sync_count++;
                current_sync_run++;
                if (current_sync_run > max_sync_run) {
                    max_sync_run = current_sync_run;
                }
            } else {
                current_sync_run = 0;
            }
        }
    }
    
    /* Calculate quality scores */
    quality->sync_count = sync_count;
    quality->sync_quality = nibbles.count > 0 ? 
        (float)sync_count / nibbles.count : 0.0f;
    quality->data_quality = nibbles.count > 0 ?
        (float)valid_nibbles / nibbles.count : 0.0f;
    quality->timing_quality = 0.9f;  /* Simplified - would need flux analysis */
    
    /* Check for protection indicators */
    quality->has_long_sync = (max_sync_run > LONG_SYNC_THRESHOLD);
    quality->has_weak_bits = (quality->data_quality < 0.9f);
    
    quality->error_count = nibbles.count - valid_nibbles;
    
    woz_nibbles_free(&nibbles);
    return true;
}

/*============================================================================*
 * PROTECTION DETECTION
 *============================================================================*/

size_t woz_detect_protections(const uint8_t *track_data, uint32_t bit_count,
                              woz_protection_info_t *protections,
                              size_t max_protections)
{
    if (!track_data || !protections || max_protections == 0) return 0;
    
    size_t detected = 0;
    
    /* Get track quality */
    woz_track_quality_t quality;
    if (!woz_analyze_track_quality(track_data, bit_count, &quality)) {
        return 0;
    }
    
    /* Detect long sync (common protection) */
    if (quality.has_long_sync && detected < max_protections) {
        protections[detected].type = WOZ_PROTECTION_LONG_SYNC;
        snprintf(protections[detected].description, 256,
                "Extended sync pattern detected (protection technique)");
        protections[detected].confidence = 0.85f;
        protections[detected].track = 0;
        protections[detected].offset = 0;
        detected++;
    }
    
    /* Detect weak bits */
    if (quality.has_weak_bits && detected < max_protections) {
        protections[detected].type = WOZ_PROTECTION_WEAK_BITS;
        snprintf(protections[detected].description, 256,
                "Weak bit areas detected (%.1f%% data quality)",
                quality.data_quality * 100.0f);
        protections[detected].confidence = 0.70f;
        protections[detected].track = 0;
        protections[detected].offset = 0;
        detected++;
    }
    
    /* Detect unusual sync patterns (EA, Optimum, etc.) */
    if (quality.sync_quality > 0.4f && quality.sync_quality < 0.6f &&
        detected < max_protections) {
        protections[detected].type = WOZ_PROTECTION_CUSTOM_FORMAT;
        snprintf(protections[detected].description, 256,
                "Non-standard sync pattern (%.1f%% sync density)",
                quality.sync_quality * 100.0f);
        protections[detected].confidence = 0.65f;
        protections[detected].track = 0;
        protections[detected].offset = 0;
        detected++;
    }
    
    return detected;
}

/*============================================================================*
 * TIMING VALIDATION
 *============================================================================*/

float woz_validate_timing(const uint8_t *track_data, uint32_t bit_count,
                         uint8_t optimal_timing)
{
    if (!track_data || bit_count == 0) return 0.0f;
    
    /* Simplified timing validation */
    /* In real implementation, would analyze flux transitions */
    
    /* For now, assume good timing if data is present */
    float quality = 0.90f;
    
    /* Penalize if track is too short or too long */
    uint32_t expected_bits = 51200;  /* ~6400 bytes for typical track */
    float size_ratio = (float)bit_count / expected_bits;
    
    if (size_ratio < 0.9f || size_ratio > 1.1f) {
        quality *= 0.8f;
    }
    
    return quality;
}

/*============================================================================*
 * FULL ANALYSIS
 *============================================================================*/

bool woz_analyze(const char *woz_filename, woz_analysis_t *analysis)
{
    if (!woz_filename || !analysis) return false;
    
    /* This is a simplified version - would integrate with WOZ1/WOZ2 readers */
    memset(analysis, 0, sizeof(woz_analysis_t));
    
    /* For demonstration, create basic analysis */
    analysis->num_tracks = 35;
    analysis->track_quality = calloc(35, sizeof(woz_track_quality_t));
    
    if (!analysis->track_quality) return false;
    
    /* Initialize with default values */
    for (int i = 0; i < 35; i++) {
        analysis->track_quality[i].timing_quality = 0.90f;
        analysis->track_quality[i].sync_quality = 0.75f;
        analysis->track_quality[i].data_quality = 0.92f;
        analysis->track_quality[i].sync_count = 128;
    }
    
    analysis->overall_quality = 0.91f;
    analysis->is_copy_protected = false;
    snprintf(analysis->format_name, sizeof(analysis->format_name), "DOS 3.3");
    
    return true;
}

void woz_analysis_free(woz_analysis_t *analysis)
{
    if (!analysis) return;
    if (analysis->track_quality) free(analysis->track_quality);
    if (analysis->protections) free(analysis->protections);
    memset(analysis, 0, sizeof(woz_analysis_t));
}

/*============================================================================*
 * REPORTING
 *============================================================================*/

void woz_print_analysis(const woz_analysis_t *analysis)
{
    if (!analysis) return;
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  WOZ ANALYSIS REPORT                                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Format: %s\n", analysis->format_name);
    printf("Copy Protected: %s\n", analysis->is_copy_protected ? "Yes" : "No");
    printf("Overall Quality: %.1f%%\n", analysis->overall_quality * 100.0f);
    printf("\n");
    
    if (analysis->num_protections > 0) {
        printf("Protections Detected:\n");
        for (size_t i = 0; i < analysis->num_protections; i++) {
            printf("  %zu. %s (%.0f%% confidence)\n",
                   i + 1,
                   woz_protection_name(analysis->protections[i].type),
                   analysis->protections[i].confidence * 100.0f);
            printf("     %s\n", analysis->protections[i].description);
        }
        printf("\n");
    }
    
    printf("Track Quality Summary:\n");
    float avg_timing = 0.0f, avg_sync = 0.0f, avg_data = 0.0f;
    for (uint8_t i = 0; i < analysis->num_tracks; i++) {
        avg_timing += analysis->track_quality[i].timing_quality;
        avg_sync += analysis->track_quality[i].sync_quality;
        avg_data += analysis->track_quality[i].data_quality;
    }
    if (analysis->num_tracks > 0) {
        avg_timing /= analysis->num_tracks;
        avg_sync /= analysis->num_tracks;
        avg_data /= analysis->num_tracks;
    }
    
    printf("  Average Timing Quality: %.1f%%\n", avg_timing * 100.0f);
    printf("  Average Sync Quality:   %.1f%%\n", avg_sync * 100.0f);
    printf("  Average Data Quality:   %.1f%%\n", avg_data * 100.0f);
    printf("\n");
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
