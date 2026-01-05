/**
 * @file uft_protection_api.c
 * @brief UFT Copy Protection Detection API Implementation
 * @version 3.3.0
 * 
 * "Bei uns geht kein Bit verloren" - UFT Preservation Philosophy
 */

#include "uft/uft_protection.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*============================================================================
 * INITIALIZATION FUNCTIONS
 *============================================================================*/

/**
 * @brief Initialize protection analysis configuration with defaults
 */
void uft_prot_config_init(uft_prot_config_t* config)
{
    if (!config) return;
    
    memset(config, 0, sizeof(uft_prot_config_t));
    
    /* Default flags: standard analysis */
    config->flags = UFT_PROT_ANAL_QUICK | UFT_PROT_ANAL_SIGNATURES;
    
    /* No platform hint (auto-detect) */
    config->platform_hint = UFT_PLATFORM_UNKNOWN;
    
    /* Full disk range */
    config->start_cylinder = 0;
    config->end_cylinder = 0;  /* 0 = all cylinders */
    
    /* No callback by default */
    config->progress_cb = NULL;
    config->user_data = NULL;
    
    /* Default thresholds */
    config->confidence_threshold = 70;      /* 70% minimum confidence */
    config->timing_tolerance_ns = 500;      /* 500ns timing tolerance */
    config->weak_bit_threshold = 50;        /* 50% stability for weak bits */
}

/**
 * @brief Initialize protection result structure
 */
void uft_prot_result_init(uft_prot_result_t* result)
{
    if (!result) return;
    
    memset(result, 0, sizeof(uft_prot_result_t));
    
    result->platform = UFT_PLATFORM_UNKNOWN;
    result->platform_confidence = 0;
    result->scheme_count = 0;
    result->cylinder_count = 0;
    result->head_count = 0;
    result->total_indicators = 0;
    result->protected_track_count = 0;
    result->weak_track_count = 0;
    result->timing_anomaly_count = 0;
    result->analysis_time_us = 0;
    result->flags = 0;
    result->notes[0] = '\0';
}

/**
 * @brief Free resources in protection result
 */
void uft_prot_result_free(uft_prot_result_t* result)
{
    if (!result) return;
    
    /* Clear sensitive data */
    memset(result, 0, sizeof(uft_prot_result_t));
}

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get protection scheme name
 */
const char* uft_prot_scheme_name(uft_protection_scheme_t scheme)
{
    /* C64 Protections */
    if (scheme >= UFT_PROT_C64_BASE && scheme < UFT_PROT_APPLE_BASE) {
        switch (scheme) {
            case UFT_PROT_C64_VMAX_V1:
            case UFT_PROT_C64_VMAX_V2:
            case UFT_PROT_C64_VMAX_V3:
            case UFT_PROT_C64_VMAX_GENERIC:     return "V-Max!";
            case UFT_PROT_C64_RAPIDLOK_V1:
            case UFT_PROT_C64_RAPIDLOK_V2:
            case UFT_PROT_C64_RAPIDLOK_V3:
            case UFT_PROT_C64_RAPIDLOK_V4:
            case UFT_PROT_C64_RAPIDLOK_GENERIC: return "RapidLok";
            case UFT_PROT_C64_VORPAL_V1:
            case UFT_PROT_C64_VORPAL_V2:
            case UFT_PROT_C64_VORPAL_GENERIC:   return "Vorpal";
            case UFT_PROT_C64_PIRATESLAYER:     return "PirateSlayer";
            case UFT_PROT_C64_FAT_TRACK:        return "Fat Track";
            case UFT_PROT_C64_HALF_TRACK:       return "Half Track";
            case UFT_PROT_C64_GCR_TIMING:       return "GCR Timing";
            case UFT_PROT_C64_CUSTOM_SYNC:      return "Custom Sync";
            case UFT_PROT_C64_SECTOR_GAP:       return "Sector Gap";
            case UFT_PROT_C64_DENSITY_MISMATCH: return "Density Mismatch";
            default:                            return "C64 Protection";
        }
    }
    
    /* Apple II Protections */
    if (scheme >= UFT_PROT_APPLE_BASE && scheme < UFT_PROT_ATARI_BASE) {
        switch (scheme) {
            case UFT_PROT_APPLE_NIBBLE_COUNT:   return "Nibble Count";
            case UFT_PROT_APPLE_TIMING_BITS:    return "Timing Bits";
            case UFT_PROT_APPLE_SPIRAL_TRACK:   return "Spiral Track";
            case UFT_PROT_APPLE_CROSS_TRACK:    return "Cross-Track Sync";
            case UFT_PROT_APPLE_HALF_TRACK:     return "Half Track";
            case UFT_PROT_APPLE_QUARTER_TRACK:  return "Quarter Track";
            default:                            return "Apple II Protection";
        }
    }
    
    /* Atari ST Protections */
    if (scheme >= UFT_PROT_ATARI_BASE && scheme < UFT_PROT_AMIGA_BASE) {
        switch (scheme) {
            case UFT_PROT_ATARI_COPYLOCK_V1:
            case UFT_PROT_ATARI_COPYLOCK_V2:
            case UFT_PROT_ATARI_COPYLOCK_V3:
            case UFT_PROT_ATARI_COPYLOCK_GENERIC: return "Copylock";
            case UFT_PROT_ATARI_MACRODOS:
            case UFT_PROT_ATARI_MACRODOS_PLUS:   return "Macrodos";
            case UFT_PROT_ATARI_FLASCHEL:        return "Flaschel";
            case UFT_PROT_ATARI_FUZZY_SECTOR:    return "Fuzzy Sector";
            case UFT_PROT_ATARI_LONG_TRACK:      return "Long Track";
            case UFT_PROT_ATARI_SHORT_TRACK:     return "Short Track";
            case UFT_PROT_ATARI_WEAK_BITS:       return "Weak Bits";
            default:                             return "Atari ST Protection";
        }
    }
    
    /* Amiga Protections */
    if (scheme >= UFT_PROT_AMIGA_BASE && scheme < UFT_PROT_PC_BASE) {
        switch (scheme) {
            case UFT_PROT_AMIGA_COPYLOCK:        return "Copylock";
            case UFT_PROT_AMIGA_SPEEDLOCK:       return "Speedlock";
            case UFT_PROT_AMIGA_LONG_TRACK:      return "Long Track";
            case UFT_PROT_AMIGA_SHORT_TRACK:     return "Short Track";
            case UFT_PROT_AMIGA_CUSTOM_SYNC:     return "Custom Sync";
            case UFT_PROT_AMIGA_WEAK_BITS:       return "Weak Bits";
            case UFT_PROT_AMIGA_CAPS_SPS:        return "CAPS/SPS";
            default:                             return "Amiga Protection";
        }
    }
    
    /* PC Protections */
    if (scheme >= UFT_PROT_PC_BASE && scheme < UFT_PROT_GENERIC_BASE) {
        switch (scheme) {
            case UFT_PROT_PC_WEAK_SECTOR:        return "Weak Sector";
            case UFT_PROT_PC_FAT_TRICKS:         return "FAT Tricks";
            case UFT_PROT_PC_EXTRA_SECTOR:       return "Extra Sector";
            case UFT_PROT_PC_LONG_SECTOR:        return "Long Sector";
            default:                             return "PC Protection";
        }
    }
    
    /* Generic */
    if (scheme >= UFT_PROT_GENERIC_BASE) {
        switch (scheme) {
            case UFT_PROT_GENERIC_WEAK_BITS:     return "Weak Bits";
            case UFT_PROT_GENERIC_LONG_TRACK:    return "Long Track";
            case UFT_PROT_GENERIC_TIMING:        return "Timing Variation";
            case UFT_PROT_GENERIC_CUSTOM_FORMAT: return "Custom Format";
            default:                             return "Unknown Protection";
        }
    }
    
    return (scheme == UFT_PROT_NONE) ? "None" : "Unknown";
}

/**
 * @brief Get platform name
 */
const char* uft_prot_platform_name(uft_platform_t platform)
{
    switch (platform) {
        case UFT_PLATFORM_UNKNOWN:      return "Unknown";
        case UFT_PLATFORM_C64:          return "Commodore 64";
        case UFT_PLATFORM_C128:         return "Commodore 128";
        case UFT_PLATFORM_VIC20:        return "VIC-20";
        case UFT_PLATFORM_PLUS4:        return "Plus/4";
        case UFT_PLATFORM_AMIGA:        return "Amiga";
        case UFT_PLATFORM_APPLE_II:     return "Apple II";
        case UFT_PLATFORM_APPLE_III:    return "Apple III";
        case UFT_PLATFORM_MAC:          return "Macintosh";
        case UFT_PLATFORM_ATARI_ST:     return "Atari ST";
        case UFT_PLATFORM_ATARI_8BIT:   return "Atari 8-bit";
        case UFT_PLATFORM_PC_DOS:       return "PC/DOS";
        case UFT_PLATFORM_PC_98:        return "NEC PC-98";
        case UFT_PLATFORM_MSX:          return "MSX";
        case UFT_PLATFORM_BBC:          return "BBC Micro";
        case UFT_PLATFORM_SPECTRUM:     return "ZX Spectrum";
        case UFT_PLATFORM_CPC:          return "Amstrad CPC";
        case UFT_PLATFORM_TRS80:        return "TRS-80";
        case UFT_PLATFORM_TI99:         return "TI-99/4A";
        default:                        return "Other";
    }
}

/**
 * @brief Print protection analysis summary
 */
void uft_prot_print_summary(const uft_prot_result_t* result)
{
    if (!result) return;
    
    printf("=== UFT Protection Analysis Summary ===\n");
    printf("Platform: %s (confidence: %u%%)\n", 
           uft_prot_platform_name(result->platform),
           result->platform_confidence);
    
    if (result->scheme_count == 0) {
        printf("No copy protection detected.\n");
    } else {
        printf("Detected schemes (%u):\n", result->scheme_count);
        for (uint8_t i = 0; i < result->scheme_count && i < UFT_PROT_MAX_SCHEMES; i++) {
            printf("  [%u] %s (confidence: %u%%)\n",
                   i + 1,
                   result->schemes[i].name ? result->schemes[i].name : 
                       uft_prot_scheme_name(result->schemes[i].scheme),
                   result->schemes[i].confidence);
        }
    }
    
    printf("Statistics:\n");
    printf("  Protected tracks: %u\n", result->protected_track_count);
    printf("  Weak tracks: %u\n", result->weak_track_count);
    printf("  Timing anomalies: %u\n", result->timing_anomaly_count);
    printf("  Total indicators: %u\n", result->total_indicators);
    printf("  Analysis time: %.2f ms\n", result->analysis_time_us / 1000.0);
    
    if (result->notes[0]) {
        printf("Notes: %s\n", result->notes);
    }
}
