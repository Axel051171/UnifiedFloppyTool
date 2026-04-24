/**
 * @file uft_profile_xdf.c
 * @brief IBM XDF, XXDF, and DMF Platform Profiles
 * 
 * Extended density formats for IBM PC compatible systems.
 * 
 * @author UFT Team
 * @date 2025
 */

#include "../uft_track_analysis.h"
#include <string.h>

/* ============================================================================
 * XDF Sync Patterns
 * ============================================================================ */

/* XDF uses standard IBM MFM sync */
static const uint32_t XDF_SYNCS[] = {
    0x4489,     /* MFM A1 sync */
    0x5224,     /* Alternative sync */
    0
};

/* ============================================================================
 * IBM XDF Profile (~1.86MB on HD disk)
 * 
 * Variable sector sizes: 512B, 1KB, 2KB, 8KB per track
 * ============================================================================ */

const uft_platform_profile_t UFT_PROFILE_IBM_XDF = {
    .platform = PLATFORM_IBM_PC,
    .encoding = ENCODING_MFM,
    .name = "IBM XDF (Extended Density)",
    
    .sync_patterns = XDF_SYNCS,
    .sync_count = 2,
    .sync_bits = 16,
    
    /* Track geometry - longer than standard HD */
    .track_length_min = 20000,
    .track_length_max = 30000,
    .track_length_nominal = 25000,
    .long_track_threshold = 28000,
    
    /* Variable sectors - use uft_xdf_sectors_for_track() */
    .sectors_per_track = 0,  /* Variable! */
    .sector_size = 0,        /* Variable: 512-8192 bytes */
    .sector_mfm_size = 0,
    .sector_tolerance = 10,
    
    /* HD timing at 500 kbps */
    .data_rate_kbps = 500,
    .rpm = 300.0,
};

/* ============================================================================
 * IBM XXDF Profile (2M.EXE format, even more aggressive)
 * ============================================================================ */

const uft_platform_profile_t UFT_PROFILE_IBM_XXDF = {
    .platform = PLATFORM_IBM_PC,
    .encoding = ENCODING_MFM,
    .name = "IBM XXDF (2M Extended)",
    
    .sync_patterns = XDF_SYNCS,
    .sync_count = 2,
    .sync_bits = 16,
    
    .track_length_min = 22000,
    .track_length_max = 32000,
    .track_length_nominal = 27000,
    .long_track_threshold = 30000,
    
    .sectors_per_track = 0,   /* Variable */
    .sector_size = 0,         /* Variable */
    .sector_mfm_size = 0,
    .sector_tolerance = 10,
    
    .data_rate_kbps = 500,
    .rpm = 300.0,
};

/* ============================================================================
 * Microsoft DMF Profile (1.68MB Distribution Media Format)
 * 
 * Fixed 21 sectors × 512 bytes = 10.5KB per track
 * Used for Windows installation media
 * ============================================================================ */

const uft_platform_profile_t UFT_PROFILE_IBM_DMF = {
    .platform = PLATFORM_IBM_PC,
    .encoding = ENCODING_MFM,
    .name = "Microsoft DMF (1.68MB)",
    
    .sync_patterns = XDF_SYNCS,
    .sync_count = 2,
    .sync_bits = 16,
    
    .track_length_min = 12000,
    .track_length_max = 15000,
    .track_length_nominal = 13500,
    .long_track_threshold = 14500,
    
    .sectors_per_track = 21,  /* Fixed 21 sectors */
    .sector_size = 512,       /* All 512 bytes */
    .sector_mfm_size = 574,   /* With header and gap */
    .sector_tolerance = 5,
    
    .data_rate_kbps = 500,
    .rpm = 300.0,
};

/* ============================================================================
 * XDF Helper Functions
 * ============================================================================ */

/**
 * @brief Get sectors per track for XDF format
 * 
 * @param track Track number (0-79)
 * @return 4 for track 0, 5 for tracks 1-79, 0 for invalid
 */
int uft_xdf_sectors_for_track(int track)
{
    if (track < 0 || track >= 80) {
        return 0;
    }
    return (track == 0) ? 4 : 5;
}

/**
 * @brief Get recommended copy mode for XDF
 * 
 * @param has_protection true if protection detected
 * @return 2=Track Copy, 3=Flux Copy
 */
int uft_xdf_recommended_copy_mode(bool has_protection)
{
    if (has_protection) {
        return 3;  /* Flux Copy for protected disks */
    }
    return 2;      /* Track Copy (required for variable sectors) */
}

/**
 * @brief Check if format requires Track Copy mode
 * 
 * @param format_name Format name
 * @return true if sector copy won't work
 */
bool uft_format_requires_track_copy(const char *format_name)
{
    if (!format_name) return false;
    
    /* Variable sector formats */
    static const char *TRACK_ONLY[] = {
        "XDF", "XXDF", "2M",
        "DMF",
        "Victor",
        "Apple", "GCR",
        "C64", "Commodore",
        NULL
    };
    
    for (int i = 0; TRACK_ONLY[i]; i++) {
        if (strstr(format_name, TRACK_ONLY[i])) {
            return true;
        }
    }
    
    return false;
}
