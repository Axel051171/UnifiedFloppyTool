/**
 * @file uft_protection_stubs.cpp
 * @brief Stub implementations for UFT protection functions
 * 
 * These are minimal stubs for Qt Creator builds.
 * Full implementation is in lib/floppy/src/formats/uft_protection.c
 */

#include "uft_protection.h"
#include <cstring>
#include <cstdio>

extern "C" {

void uft_prot_config_init(uft_prot_config_t* config)
{
    if (!config) return;
    
    std::memset(config, 0, sizeof(*config));
    config->flags = UFT_PROT_ANAL_QUICK;
    config->platform_hint = UFT_PLATFORM_UNKNOWN;
    config->start_cylinder = 0;
    config->end_cylinder = 0;
    config->progress_cb = nullptr;
    config->user_data = nullptr;
    config->confidence_threshold = 70;
    config->timing_tolerance_ns = 500;
    config->weak_bit_threshold = 50;
}

void uft_prot_result_init(uft_prot_result_t* result)
{
    if (!result) return;
    
    std::memset(result, 0, sizeof(*result));
    result->platform = UFT_PLATFORM_UNKNOWN;
    result->platform_confidence = 0;
    result->scheme_count = 0;
    result->cylinder_count = 0;
    result->head_count = 0;
}

void uft_prot_result_free(uft_prot_result_t* result)
{
    (void)result;
    // Nothing to free in stub
}

const char* uft_prot_scheme_name(uft_protection_scheme_t scheme)
{
    switch (scheme) {
        case UFT_PROT_NONE: return "None";
        case UFT_PROT_C64_VMAX_V1: return "V-MAX! v1";
        case UFT_PROT_C64_VMAX_V2: return "V-MAX! v2";
        case UFT_PROT_C64_VMAX_V3: return "V-MAX! v3";
        case UFT_PROT_C64_VMAX_GENERIC: return "V-MAX!";
        case UFT_PROT_C64_RAPIDLOK_V1: return "RapidLok v1";
        case UFT_PROT_C64_RAPIDLOK_V2: return "RapidLok v2";
        case UFT_PROT_C64_RAPIDLOK_V3: return "RapidLok v3";
        case UFT_PROT_C64_RAPIDLOK_V4: return "RapidLok v4";
        case UFT_PROT_C64_RAPIDLOK_GENERIC: return "RapidLok";
        case UFT_PROT_C64_VORPAL_V1: return "Vorpal v1";
        case UFT_PROT_C64_VORPAL_V2: return "Vorpal v2";
        case UFT_PROT_C64_VORPAL_GENERIC: return "Vorpal";
        case UFT_PROT_C64_PIRATESLAYER: return "PirateSlayer";
        case UFT_PROT_C64_FAT_TRACK: return "Fat Track";
        case UFT_PROT_C64_HALF_TRACK: return "Half Track";
        case UFT_PROT_C64_GCR_TIMING: return "GCR Timing";
        case UFT_PROT_C64_CUSTOM_SYNC: return "Custom Sync";
        case UFT_PROT_C64_SECTOR_GAP: return "Sector Gap";
        case UFT_PROT_C64_DENSITY_MISMATCH: return "Density Mismatch";
        default: 
            if ((scheme & 0xFF00) == UFT_PROT_C64_BASE) return "C64 Protection";
            if ((scheme & 0xFF00) == UFT_PROT_APPLE_BASE) return "Apple Protection";
            return "Unknown";
    }
}

const char* uft_prot_platform_name(uft_platform_t platform)
{
    switch (platform) {
        case UFT_PLATFORM_UNKNOWN: return "Unknown";
        case UFT_PLATFORM_C64: return "Commodore 64";
        case UFT_PLATFORM_C128: return "Commodore 128";
        case UFT_PLATFORM_VIC20: return "VIC-20";
        case UFT_PLATFORM_PLUS4: return "Plus/4";
        case UFT_PLATFORM_AMIGA: return "Amiga";
        case UFT_PLATFORM_APPLE_II: return "Apple II";
        case UFT_PLATFORM_APPLE_III: return "Apple III";
        case UFT_PLATFORM_MAC: return "Macintosh";
        case UFT_PLATFORM_ATARI_ST: return "Atari ST";
        case UFT_PLATFORM_ATARI_8BIT: return "Atari 8-bit";
        case UFT_PLATFORM_PC_DOS: return "IBM PC/DOS";
        case UFT_PLATFORM_PC_98: return "NEC PC-98";
        case UFT_PLATFORM_MSX: return "MSX";
        case UFT_PLATFORM_BBC: return "BBC Micro";
        case UFT_PLATFORM_SPECTRUM: return "ZX Spectrum";
        case UFT_PLATFORM_CPC: return "Amstrad CPC";
        case UFT_PLATFORM_TRS80: return "TRS-80";
        case UFT_PLATFORM_TI99: return "TI-99/4A";
        default: return "Unknown";
    }
}

const char* uft_prot_indicator_name(uft_indicator_type_t type)
{
    switch (type) {
        case UFT_IND_NONE: return "None";
        case UFT_IND_TRACK_LENGTH: return "Track Length";
        case UFT_IND_SECTOR_COUNT: return "Sector Count";
        case UFT_IND_SECTOR_SIZE: return "Sector Size";
        case UFT_IND_SECTOR_GAP: return "Sector Gap";
        case UFT_IND_HALF_TRACK: return "Half Track";
        case UFT_IND_QUARTER_TRACK: return "Quarter Track";
        case UFT_IND_CUSTOM_SYNC: return "Custom Sync";
        case UFT_IND_SYNC_LENGTH: return "Sync Length";
        case UFT_IND_SYNC_POSITION: return "Sync Position";
        case UFT_IND_ADDRESS_MARK: return "Address Mark";
        case UFT_IND_DATA_MARK: return "Data Mark";
        case UFT_IND_ENCODING_MIX: return "Encoding Mix";
        case UFT_IND_TIMING_VARIATION: return "Timing Variation";
        case UFT_IND_BITCELL_DEVIATION: return "Bitcell Deviation";
        case UFT_IND_DENSITY_ZONE: return "Density Zone";
        case UFT_IND_RPM_VARIATION: return "RPM Variation";
        case UFT_IND_WEAK_BITS: return "Weak Bits";
        case UFT_IND_CRC_ERROR: return "CRC Error";
        case UFT_IND_CHECKSUM_ERROR: return "Checksum Error";
        case UFT_IND_DATA_PATTERN: return "Data Pattern";
        case UFT_IND_TRACK_POSITION: return "Track Position";
        case UFT_IND_SECTOR_POSITION: return "Sector Position";
        case UFT_IND_GAP_DATA: return "Gap Data";
        case UFT_IND_INDEX_POSITION: return "Index Position";
        case UFT_IND_CODE_SIGNATURE: return "Code Signature";
        case UFT_IND_STRING_SIGNATURE: return "String Signature";
        case UFT_IND_PATTERN_SIGNATURE: return "Pattern Signature";
        case UFT_IND_TYPE_COUNT: return "Type Count";
        default: return "Unknown";
    }
}

void uft_prot_print_summary(const uft_prot_result_t* result)
{
    if (!result) return;
    
    std::printf("Protection Analysis Summary\n");
    std::printf("===========================\n");
    std::printf("Platform: %s (confidence: %d%%)\n", 
                uft_prot_platform_name(result->platform),
                result->platform_confidence);
    std::printf("Schemes detected: %d\n", result->scheme_count);
    std::printf("Protected tracks: %d\n", result->protected_track_count);
    std::printf("Weak tracks: %d\n", result->weak_track_count);
    std::printf("Timing anomalies: %d\n", result->timing_anomaly_count);
    
    if (result->notes[0]) {
        std::printf("Notes: %s\n", result->notes);
    }
}

const char* uft_prot_preservation_notes(uft_protection_scheme_t scheme)
{
    switch (scheme) {
        case UFT_PROT_NONE:
            return "Standard disk, no special preservation needed.";
        case UFT_PROT_C64_VMAX_V1:
        case UFT_PROT_C64_VMAX_V2:
        case UFT_PROT_C64_VMAX_V3:
        case UFT_PROT_C64_VMAX_GENERIC:
            return "V-MAX! requires flux-level preservation. G64/SCP/KF recommended.";
        case UFT_PROT_C64_RAPIDLOK_V1:
        case UFT_PROT_C64_RAPIDLOK_V2:
        case UFT_PROT_C64_RAPIDLOK_V3:
        case UFT_PROT_C64_RAPIDLOK_V4:
        case UFT_PROT_C64_RAPIDLOK_GENERIC:
            return "RapidLok uses timing-sensitive half-tracks. G64/SCP required.";
        case UFT_PROT_C64_VORPAL_V1:
        case UFT_PROT_C64_VORPAL_V2:
        case UFT_PROT_C64_VORPAL_GENERIC:
            return "Vorpal uses custom sync patterns. G64/SCP recommended.";
        case UFT_PROT_C64_FAT_TRACK:
            return "Fat track protection. G64 can preserve, flux capture preferred.";
        case UFT_PROT_C64_HALF_TRACK:
            return "Half-track protection. Requires sub-track resolution (SCP/KF).";
        case UFT_PROT_C64_GCR_TIMING:
            return "GCR timing variations. Flux-level capture required.";
        default:
            return "Flux-level preservation recommended for best results.";
    }
}

} // extern "C"
