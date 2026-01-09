/**
 * @file uft_atarist_protection.c
 * @brief Atari ST Protection Detection Implementation
 * 
 * @version 2.0.0
 * @date 2025-01-08
 */

#include "uft/protection/uft_atarist_protection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * Memory Management
 *===========================================================================*/

uft_atarist_prot_result_t *uft_atarist_result_alloc(void) {
    uft_atarist_prot_result_t *result = calloc(1, sizeof(uft_atarist_prot_result_t));
    return result;
}

void uft_atarist_result_free(uft_atarist_prot_result_t *result) {
    if (!result) return;
    
    free(result->fuzzy_sectors);
    free(result->flaschels);
    free(result->long_tracks);
    free(result);
}

/*===========================================================================
 * Initialization
 *===========================================================================*/

void uft_atarist_prot_init(uft_atarist_prot_result_t *result) {
    if (!result) return;
    
    memset(result, 0, sizeof(*result));
    result->primary_type = UFT_ATARIST_PROT_NONE;
    result->type_flags = 0;
    result->overall_confidence = 0.0;
}

/*===========================================================================
 * Detection Helpers
 *===========================================================================*/

/**
 * @brief Check for Copylock signature in track data
 */
static bool detect_copylock(const uint8_t *data, size_t len, 
                            uft_copylock_st_t *info) {
    if (!data || len < 512 || !info) return false;
    
    /* Copylock signatures: Look for LFSR patterns */
    /* Rob Northen's Copylock uses specific sector layouts and timing */
    
    /* Simplified detection: Look for typical Copylock boot sector markers */
    /* Real detection would analyze sector timing and fuzzy bits */
    
    /* Look for "RNC" or Copylock magic */
    for (size_t i = 0; i + 3 < len; i++) {
        if (data[i] == 'R' && data[i+1] == 'N' && data[i+2] == 'C') {
            info->detected = true;
            info->track = 79;  /* Typically on last track */
            info->side = 0;
            info->lfsr_seed = 0x12345678;  /* Placeholder */
            info->confidence = 0.75;
            return true;
        }
    }
    
    /* Look for fuzzy sector pattern (alternating bytes) */
    int fuzzy_count = 0;
    for (size_t i = 0; i + 16 < len; i += 16) {
        bool pattern = true;
        for (int j = 0; j < 8; j++) {
            if (data[i + j*2] != data[i] || data[i + j*2 + 1] != data[i+1]) {
                pattern = false;
                break;
            }
        }
        if (pattern) fuzzy_count++;
    }
    
    if (fuzzy_count > 10) {
        info->detected = true;
        info->confidence = 0.6;
        return true;
    }
    
    return false;
}

/**
 * @brief Check for long track (extended track length)
 */
static bool detect_long_track(const uint8_t *data, size_t len,
                              uft_long_track_st_t *info) {
    if (!data || !info) return false;
    
    /* Long tracks are > 6500 bytes for DD, > 13000 for HD */
    if (len > UFT_ATARIST_LONG_TRACK_MIN) {
        info->detected = true;
        info->actual_length = (uint32_t)len;
        info->standard_length = 6250;
        info->extra_bytes = (uint32_t)(len - 6250);
        info->confidence = 0.9;
        return true;
    }
    
    return false;
}

/**
 * @brief Check for Flaschel protection (FDC bug exploit)
 */
static bool detect_flaschel(const uint8_t *data, size_t len,
                            uft_flaschel_t *info) {
    if (!data || len < 512 || !info) return false;
    
    /* Flaschel uses specific gap patterns that exploit WD1772 FDC bug */
    /* Look for unusual gap bytes (not 0x4E) in specific positions */
    
    int gap_anomalies = 0;
    for (size_t i = 0; i + 512 < len; i += 512) {
        /* Check gap area after sector data */
        for (size_t j = i + 512; j < i + 530 && j < len; j++) {
            if (data[j] != 0x4E && data[j] != 0x00) {
                gap_anomalies++;
            }
        }
    }
    
    if (gap_anomalies > 5) {
        info->detected = true;
        info->track = 0;
        info->confidence = 0.7;
        return true;
    }
    
    return false;
}

/*===========================================================================
 * Main Detection
 *===========================================================================*/

int uft_atarist_prot_detect(const uint8_t *data, size_t data_len,
                            uft_atarist_prot_result_t *result) {
    if (!data || !result) return -1;
    
    uft_atarist_prot_init(result);
    
    /* Check Copylock */
    if (detect_copylock(data, data_len, &result->copylock)) {
        result->primary_type = UFT_ATARIST_PROT_COPYLOCK;
        result->type_flags |= (1 << UFT_ATARIST_PROT_COPYLOCK);
        result->overall_confidence = result->copylock.confidence;
        snprintf(result->description, sizeof(result->description),
                 "Rob Northen Copylock detected (confidence: %.0f%%)",
                 result->copylock.confidence * 100);
    }
    
    /* Check for long tracks */
    uft_long_track_st_t long_track = {0};
    if (detect_long_track(data, data_len, &long_track)) {
        if (result->primary_type == UFT_ATARIST_PROT_NONE) {
            result->primary_type = UFT_ATARIST_PROT_LONG_TRACK;
        } else {
            result->primary_type = UFT_ATARIST_PROT_MULTIPLE;
        }
        result->type_flags |= (1 << UFT_ATARIST_PROT_LONG_TRACK);
        
        /* Store long track info */
        result->long_tracks = malloc(sizeof(uft_long_track_st_t));
        if (result->long_tracks) {
            result->long_tracks[0] = long_track;
            result->long_track_count = 1;
        }
    }
    
    /* Check for Flaschel */
    uft_flaschel_t flaschel = {0};
    if (detect_flaschel(data, data_len, &flaschel)) {
        if (result->primary_type == UFT_ATARIST_PROT_NONE) {
            result->primary_type = UFT_ATARIST_PROT_FLASCHEL;
        } else {
            result->primary_type = UFT_ATARIST_PROT_MULTIPLE;
        }
        result->type_flags |= (1 << UFT_ATARIST_PROT_FLASCHEL);
        
        result->flaschels = malloc(sizeof(uft_flaschel_t));
        if (result->flaschels) {
            result->flaschels[0] = flaschel;
            result->flaschel_count = 1;
        }
    }
    
    return (result->primary_type != UFT_ATARIST_PROT_NONE) ? 0 : 1;
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

const char* uft_atarist_prot_type_name(uft_atarist_prot_type_t type) {
    switch (type) {
        case UFT_ATARIST_PROT_NONE:         return "None";
        case UFT_ATARIST_PROT_COPYLOCK:     return "Rob Northen CopyLock";
        case UFT_ATARIST_PROT_MACRODOS:     return "Macrodos";
        case UFT_ATARIST_PROT_FUZZY_SECTOR: return "Fuzzy Sector";
        case UFT_ATARIST_PROT_LONG_TRACK:   return "Long Track";
        case UFT_ATARIST_PROT_FLASCHEL:     return "Flaschel (FDC Bug)";
        case UFT_ATARIST_PROT_NO_FLUX:      return "No-Flux Area";
        case UFT_ATARIST_PROT_SECTOR_GAP:   return "Modified Sector Gap";
        case UFT_ATARIST_PROT_HIDDEN_DATA:  return "Hidden Data";
        case UFT_ATARIST_PROT_MULTIPLE:     return "Multiple Protections";
        default:                            return "Unknown";
    }
}

void uft_atarist_prot_print(FILE *out, const uft_atarist_prot_result_t *result) {
    if (!out || !result) return;
    
    fprintf(out, "=== Atari ST Protection Detection ===\n");
    fprintf(out, "Primary:    %s\n", uft_atarist_prot_type_name(result->primary_type));
    fprintf(out, "Type Flags: 0x%08X\n", result->type_flags);
    fprintf(out, "Confidence: %.1f%%\n", result->overall_confidence * 100);
    
    if (result->description[0]) {
        fprintf(out, "Details:    %s\n", result->description);
    }
    
    if (result->copylock.detected) {
        fprintf(out, "\nCopylock:\n");
        fprintf(out, "  Track/Side: %d/%d\n", result->copylock.track, result->copylock.side);
        fprintf(out, "  LFSR Seed:  0x%08X\n", result->copylock.lfsr_seed);
    }
    
    if (result->long_track_count > 0) {
        fprintf(out, "\nLong Tracks: %d\n", result->long_track_count);
        for (int i = 0; i < result->long_track_count; i++) {
            fprintf(out, "  Track %d: %d bytes (+%d extra)\n",
                    result->long_tracks[i].track,
                    result->long_tracks[i].actual_length,
                    result->long_tracks[i].extra_bytes);
        }
    }
    
    if (result->flaschel_count > 0) {
        fprintf(out, "\nFlaschel Protections: %d\n", result->flaschel_count);
    }
    
    if (result->fuzzy_sector_count > 0) {
        fprintf(out, "\nFuzzy Sectors: %d\n", result->fuzzy_sector_count);
    }
}
