/**
 * @file uft_profile_misc.c
 * @brief Platform profiles for miscellaneous computer formats
 * 
 * Profiles for:
 * - Acorn Electron (DFS)
 * - Enterprise 64/128
 * - Sord M5
 * - Tatung Einstein
 * - Memotech MTX
 * - Thomson MO/TO
 * - Microbee
 */

#include "uft_track_analysis.h"
#include <string.h>

/*===========================================================================
 * Acorn Electron (uses DFS like BBC)
 *===========================================================================*/

static const uint32_t ELECTRON_SYNCS[] = {
    0xFE,       /* FM ID mark */
    0xFB,       /* FM Data mark */
};

/**
 * @brief Acorn Electron DFS (100KB)
 * 
 * 40 tracks × 1 side × 10 sectors × 256 bytes = 102,400 bytes
 */
const uft_platform_profile_t UFT_PROFILE_ELECTRON_DFS = {
    .platform = PLATFORM_BBC_MICRO,  /* Compatible with BBC */
    .encoding = ENCODING_FM,
    .name = "Acorn Electron DFS",
    .sync_patterns = ELECTRON_SYNCS,
    .sync_count = 2,
    .sync_bits = 8,
    .track_length_min = 3000,
    .track_length_max = 3500,
    .track_length_nominal = 3125,
    .long_track_threshold = 3300,
    .sectors_per_track = 10,
    .sector_size = 256,
    .sector_mfm_size = 340,
    .sector_tolerance = 24,
    .data_rate_kbps = 125.0,
    .rpm = 300.0
};

/**
 * @brief Acorn Electron ADFS (640KB)
 */
const uft_platform_profile_t UFT_PROFILE_ELECTRON_ADFS = {
    .platform = PLATFORM_BBC_MICRO,
    .encoding = ENCODING_MFM,
    .name = "Acorn Electron ADFS",
    .sync_patterns = (const uint32_t[]){0x4489},
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 10000,
    .track_length_max = 13000,
    .track_length_nominal = 12500,
    .long_track_threshold = 12800,
    .sectors_per_track = 16,
    .sector_size = 256,
    .sector_mfm_size = 340,
    .sector_tolerance = 24,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Enterprise 64/128
 *===========================================================================*/

static const uint32_t ENTERPRISE_SYNCS[] = {
    0x4489,     /* MFM sync */
};

/**
 * @brief Enterprise 64/128 EXDOS
 * 
 * CP/M compatible format
 * 40 tracks × 2 sides × 9 sectors × 512 bytes = 368,640 bytes
 */
const uft_platform_profile_t UFT_PROFILE_ENTERPRISE = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "Enterprise EXDOS",
    .sync_patterns = ENTERPRISE_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 9,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Tatung Einstein
 *===========================================================================*/

static const uint32_t EINSTEIN_SYNCS[] = {
    0x4489,
};

/**
 * @brief Tatung Einstein (200KB)
 * 
 * 40 tracks × 1 side × 10 sectors × 512 bytes = 204,800 bytes
 */
const uft_platform_profile_t UFT_PROFILE_EINSTEIN = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "Tatung Einstein",
    .sync_patterns = EINSTEIN_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 10,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Memotech MTX
 *===========================================================================*/

static const uint32_t MEMOTECH_SYNCS[] = {
    0x4489,
};

/**
 * @brief Memotech MTX (CP/M format)
 * 
 * 40 tracks × 2 sides × 16 sectors × 256 bytes = 327,680 bytes
 */
const uft_platform_profile_t UFT_PROFILE_MEMOTECH = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "Memotech MTX",
    .sync_patterns = MEMOTECH_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 16,
    .sector_size = 256,
    .sector_mfm_size = 340,
    .sector_tolerance = 24,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Thomson MO/TO Series (French)
 *===========================================================================*/

static const uint32_t THOMSON_SYNCS[] = {
    0x4489,
    0xA1A1,
};

/**
 * @brief Thomson MO5/TO7 (160KB)
 * 
 * 40 tracks × 1 side × 16 sectors × 256 bytes = 163,840 bytes
 */
const uft_platform_profile_t UFT_PROFILE_THOMSON_MO5 = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "Thomson MO5/TO7",
    .sync_patterns = THOMSON_SYNCS,
    .sync_count = 2,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 16,
    .sector_size = 256,
    .sector_mfm_size = 340,
    .sector_tolerance = 24,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/**
 * @brief Thomson TO8/TO9/MO6 (640KB)
 * 
 * 80 tracks × 2 sides × 16 sectors × 256 bytes = 655,360 bytes
 */
const uft_platform_profile_t UFT_PROFILE_THOMSON_TO8 = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "Thomson TO8/MO6",
    .sync_patterns = THOMSON_SYNCS,
    .sync_count = 2,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 16,
    .sector_size = 256,
    .sector_mfm_size = 340,
    .sector_tolerance = 24,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Microbee (Australian)
 *===========================================================================*/

static const uint32_t MICROBEE_SYNCS[] = {
    0x4489,
};

/**
 * @brief Microbee DS40 (400KB)
 * 
 * 40 tracks × 2 sides × 10 sectors × 512 bytes = 409,600 bytes
 */
const uft_platform_profile_t UFT_PROFILE_MICROBEE_DS40 = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "Microbee DS40",
    .sync_patterns = MICROBEE_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 10,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/**
 * @brief Microbee DS80 (800KB)
 */
const uft_platform_profile_t UFT_PROFILE_MICROBEE_DS80 = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "Microbee DS80",
    .sync_patterns = MICROBEE_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 10,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Sord M5
 *===========================================================================*/

static const uint32_t SORD_SYNCS[] = {
    0x4489,
};

/**
 * @brief Sord M5 (160KB)
 * 
 * 40 tracks × 1 side × 16 sectors × 256 bytes = 163,840 bytes
 */
const uft_platform_profile_t UFT_PROFILE_SORD_M5 = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "Sord M5",
    .sync_patterns = SORD_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 16,
    .sector_size = 256,
    .sector_mfm_size = 340,
    .sector_tolerance = 24,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Profile Lookup
 *===========================================================================*/

/**
 * @brief Get miscellaneous platform profile by name
 */
const uft_platform_profile_t* uft_get_misc_profile(const char *name)
{
    if (!name) return NULL;
    
    /* Acorn Electron */
    if (strstr(name, "Electron")) {
        if (strstr(name, "ADFS")) return &UFT_PROFILE_ELECTRON_ADFS;
        return &UFT_PROFILE_ELECTRON_DFS;
    }
    
    /* Enterprise */
    if (strstr(name, "Enterprise") || strstr(name, "EXDOS"))
        return &UFT_PROFILE_ENTERPRISE;
    
    /* Tatung Einstein */
    if (strstr(name, "Einstein"))
        return &UFT_PROFILE_EINSTEIN;
    
    /* Memotech */
    if (strstr(name, "Memotech") || strstr(name, "MTX"))
        return &UFT_PROFILE_MEMOTECH;
    
    /* Thomson */
    if (strstr(name, "Thomson") || strstr(name, "MO5") || strstr(name, "TO7"))
        return &UFT_PROFILE_THOMSON_MO5;
    if (strstr(name, "TO8") || strstr(name, "TO9") || strstr(name, "MO6"))
        return &UFT_PROFILE_THOMSON_TO8;
    
    /* Microbee */
    if (strstr(name, "Microbee")) {
        if (strstr(name, "80")) return &UFT_PROFILE_MICROBEE_DS80;
        return &UFT_PROFILE_MICROBEE_DS40;
    }
    
    /* Sord */
    if (strstr(name, "Sord") || strstr(name, "M5"))
        return &UFT_PROFILE_SORD_M5;
    
    return NULL;
}
