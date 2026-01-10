/**
 * @file uft_profile_us.c
 * @brief Platform profiles for US computer formats
 * 
 * Profiles for:
 * - Texas Instruments TI-99/4A
 * - TRS-80 Model I/III/4
 * - Victor 9000/Sirius 1 (GCR, variable sectors)
 * - Kaypro (CP/M)
 * - Osborne (CP/M)
 */

#include "uft_track_analysis.h"
#include <string.h>

/*===========================================================================
 * Texas Instruments TI-99/4A
 *===========================================================================*/

static const uint32_t TI99_FM_SYNCS[] = {
    0xFE,       /* FM ID address mark */
    0xFB,       /* FM Data address mark */
};

static const uint32_t TI99_MFM_SYNCS[] = {
    0x4489,     /* MFM A1 sync */
};

/**
 * @brief TI-99/4A Single Density (FM, 90KB)
 * 
 * 40 tracks × 1 side × 9 sectors × 256 bytes = 92,160 bytes
 */
const uft_platform_profile_t UFT_PROFILE_TI99_SSSD = {
    .platform = PLATFORM_CUSTOM,  /* TI-99 */
    .encoding = ENCODING_FM,
    .name = "TI-99/4A SSSD",
    .sync_patterns = TI99_FM_SYNCS,
    .sync_count = 2,
    .sync_bits = 8,
    .track_length_min = 3000,
    .track_length_max = 3500,
    .track_length_nominal = 3125,
    .long_track_threshold = 3300,
    .sectors_per_track = 9,
    .sector_size = 256,
    .sector_mfm_size = 340,     /* FM uses more overhead */
    .sector_tolerance = 24,
    .data_rate_kbps = 125.0,    /* FM is half MFM rate */
    .rpm = 300.0
};

/**
 * @brief TI-99/4A Double Density (MFM, 180KB)
 * 
 * 40 tracks × 1 side × 18 sectors × 256 bytes = 184,320 bytes
 */
const uft_platform_profile_t UFT_PROFILE_TI99_SSDD = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "TI-99/4A SSDD",
    .sync_patterns = TI99_MFM_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 18,
    .sector_size = 256,
    .sector_mfm_size = 340,
    .sector_tolerance = 24,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/**
 * @brief TI-99/4A Double-Sided Double Density (360KB)
 */
const uft_platform_profile_t UFT_PROFILE_TI99_DSDD = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "TI-99/4A DSDD",
    .sync_patterns = TI99_MFM_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 18,
    .sector_size = 256,
    .sector_mfm_size = 340,
    .sector_tolerance = 24,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * TRS-80 Model I/III/4
 *===========================================================================*/

static const uint32_t TRS80_FM_SYNCS[] = {
    0xFE,       /* ID address mark */
    0xFB,       /* Data address mark */
    0xF8,       /* Deleted data mark */
};

static const uint32_t TRS80_MFM_SYNCS[] = {
    0x4489,     /* MFM A1 sync */
};

/**
 * @brief TRS-80 Model I SSSD (80KB)
 * 
 * 35 tracks × 1 side × 10 sectors × 256 bytes = 89,600 bytes
 * Note: Track 17 is directory, 0-indexed sectors
 */
const uft_platform_profile_t UFT_PROFILE_TRS80_SSSD = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_FM,
    .name = "TRS-80 SSSD",
    .sync_patterns = TRS80_FM_SYNCS,
    .sync_count = 3,
    .sync_bits = 8,
    .track_length_min = 2800,
    .track_length_max = 3400,
    .track_length_nominal = 3125,
    .long_track_threshold = 3200,
    .sectors_per_track = 10,
    .sector_size = 256,
    .sector_mfm_size = 340,
    .sector_tolerance = 24,
    .data_rate_kbps = 125.0,
    .rpm = 300.0
};

/**
 * @brief TRS-80 Model III/4 DSDD (360KB)
 * 
 * 40 tracks × 2 sides × 18 sectors × 256 bytes = 368,640 bytes
 */
const uft_platform_profile_t UFT_PROFILE_TRS80_DSDD = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "TRS-80 DSDD",
    .sync_patterns = TRS80_MFM_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 18,
    .sector_size = 256,
    .sector_mfm_size = 340,
    .sector_tolerance = 24,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/**
 * @brief TRS-80 Model 4 80-track (720KB)
 */
const uft_platform_profile_t UFT_PROFILE_TRS80_80TRACK = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "TRS-80 80-Track",
    .sync_patterns = TRS80_MFM_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 18,
    .sector_size = 256,
    .sector_mfm_size = 340,
    .sector_tolerance = 24,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Victor 9000 / Sirius 1 (GCR, Variable Sectors)
 *===========================================================================*/

static const uint32_t VICTOR_SYNCS[] = {
    0x4E,       /* Victor sync byte */
    0x00,       /* Zero pattern */
};

/**
 * @brief Victor 9000 / Sirius 1 GCR Format
 * 
 * UNIQUE: Variable sectors per track (10-19) based on track position
 * Uses GCR encoding similar to Apple but different scheme
 * 80 tracks × 2 sides × variable sectors × 512 bytes ≈ 1.2MB
 * 
 * Track zones:
 *   Tracks 0-3:   19 sectors
 *   Tracks 4-15:  18 sectors
 *   Tracks 16-26: 17 sectors
 *   Tracks 27-37: 16 sectors
 *   Tracks 38-47: 15 sectors
 *   Tracks 48-59: 14 sectors
 *   Tracks 60-67: 13 sectors
 *   Tracks 68-74: 12 sectors
 *   Tracks 75-79: 11 sectors
 */
const uft_platform_profile_t UFT_PROFILE_VICTOR_9000 = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_GCR_VICTOR,
    .name = "Victor 9000/Sirius 1",
    .sync_patterns = VICTOR_SYNCS,
    .sync_count = 2,
    .sync_bits = 8,
    .track_length_min = 5500,
    .track_length_max = 10000,  /* Variable due to zone */
    .track_length_nominal = 7500,
    .long_track_threshold = 9500,
    .sectors_per_track = 15,    /* Average - varies by zone */
    .sector_size = 512,
    .sector_mfm_size = 600,
    .sector_tolerance = 48,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Kaypro (CP/M)
 *===========================================================================*/

static const uint32_t KAYPRO_SYNCS[] = {
    0x4489,     /* MFM sync */
};

/**
 * @brief Kaypro II/4 SSDD (191KB)
 * 
 * 40 tracks × 1 side × 10 sectors × 512 bytes = 204,800 bytes
 */
const uft_platform_profile_t UFT_PROFILE_KAYPRO_SSDD = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "Kaypro SSDD",
    .sync_patterns = KAYPRO_SYNCS,
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
 * @brief Kaypro 2X/4/10 DSDD (390KB)
 * 
 * 40 tracks × 2 sides × 10 sectors × 512 bytes = 409,600 bytes
 */
const uft_platform_profile_t UFT_PROFILE_KAYPRO_DSDD = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "Kaypro DSDD",
    .sync_patterns = KAYPRO_SYNCS,
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
 * Osborne (CP/M)
 *===========================================================================*/

/**
 * @brief Osborne 1 SSSD (92KB)
 * 
 * 40 tracks × 1 side × 10 sectors × 256 bytes = 102,400 bytes
 */
const uft_platform_profile_t UFT_PROFILE_OSBORNE_SSSD = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_FM,
    .name = "Osborne SSSD",
    .sync_patterns = TRS80_FM_SYNCS,  /* Same as TRS-80 */
    .sync_count = 3,
    .sync_bits = 8,
    .track_length_min = 2800,
    .track_length_max = 3400,
    .track_length_nominal = 3125,
    .long_track_threshold = 3200,
    .sectors_per_track = 10,
    .sector_size = 256,
    .sector_mfm_size = 340,
    .sector_tolerance = 24,
    .data_rate_kbps = 125.0,
    .rpm = 300.0
};

/**
 * @brief Osborne SSDD (184KB)
 */
const uft_platform_profile_t UFT_PROFILE_OSBORNE_SSDD = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "Osborne SSDD",
    .sync_patterns = KAYPRO_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 5,
    .sector_size = 1024,
    .sector_mfm_size = 1200,
    .sector_tolerance = 48,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Profile Lookup
 *===========================================================================*/

/**
 * @brief Get US platform profile by name
 */
const uft_platform_profile_t* uft_get_us_profile(const char *name)
{
    if (!name) return NULL;
    
    /* TI-99 */
    if (strstr(name, "TI-99") || strstr(name, "TI99")) {
        if (strstr(name, "DSDD")) return &UFT_PROFILE_TI99_DSDD;
        if (strstr(name, "SSDD") || strstr(name, "DD")) return &UFT_PROFILE_TI99_SSDD;
        return &UFT_PROFILE_TI99_SSSD;
    }
    
    /* TRS-80 */
    if (strstr(name, "TRS-80") || strstr(name, "TRS80")) {
        if (strstr(name, "80") && strstr(name, "track")) return &UFT_PROFILE_TRS80_80TRACK;
        if (strstr(name, "DSDD") || strstr(name, "DD")) return &UFT_PROFILE_TRS80_DSDD;
        return &UFT_PROFILE_TRS80_SSSD;
    }
    
    /* Victor 9000 */
    if (strstr(name, "Victor") || strstr(name, "Sirius")) return &UFT_PROFILE_VICTOR_9000;
    
    /* Kaypro */
    if (strstr(name, "Kaypro")) {
        if (strstr(name, "DSDD") || strstr(name, "2X") || strstr(name, "4") || strstr(name, "10"))
            return &UFT_PROFILE_KAYPRO_DSDD;
        return &UFT_PROFILE_KAYPRO_SSDD;
    }
    
    /* Osborne */
    if (strstr(name, "Osborne")) {
        if (strstr(name, "DD")) return &UFT_PROFILE_OSBORNE_SSDD;
        return &UFT_PROFILE_OSBORNE_SSSD;
    }
    
    return NULL;
}

/**
 * @brief Auto-detect US format by image size
 */
const uft_platform_profile_t* uft_detect_us_profile(size_t image_size)
{
    switch (image_size) {
        /* TI-99 */
        case 92160:     /* SSSD */
            return &UFT_PROFILE_TI99_SSSD;
        case 184320:    /* SSDD */
            return &UFT_PROFILE_TI99_SSDD;
        case 368640:    /* DSDD */
            return &UFT_PROFILE_TI99_DSDD;
            
        /* TRS-80 */
        case 89600:     /* Model I SSSD */
            return &UFT_PROFILE_TRS80_SSSD;
        case 737280:    /* 80-track */
            return &UFT_PROFILE_TRS80_80TRACK;
            
        /* Kaypro */
        case 204800:    /* SSDD */
            return &UFT_PROFILE_KAYPRO_SSDD;
        case 409600:    /* DSDD */
            return &UFT_PROFILE_KAYPRO_DSDD;
            
        /* Osborne */
        case 102400:    /* SSSD */
            return &UFT_PROFILE_OSBORNE_SSSD;
            
        default:
            return NULL;
    }
}

/*===========================================================================
 * Victor 9000 Zone Helper
 *===========================================================================*/

/**
 * @brief Get sectors per track for Victor 9000 (zone-based)
 */
int uft_victor_sectors_for_track(int track)
{
    if (track < 0 || track > 79) return 0;
    
    if (track <= 3)  return 19;
    if (track <= 15) return 18;
    if (track <= 26) return 17;
    if (track <= 37) return 16;
    if (track <= 47) return 15;
    if (track <= 59) return 14;
    if (track <= 67) return 13;
    if (track <= 74) return 12;
    return 11;  /* tracks 75-79 */
}
