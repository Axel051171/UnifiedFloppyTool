/**
 * @file uft_profile_uk.c
 * @brief Platform profiles for UK computer formats
 * 
 * Profiles for:
 * - Acorn Archimedes (ADFS E/F/G)
 * - SAM Coupé (MGT Format)
 * - Sinclair Spectrum +3 (+3DOS)
 * - Oric Atmos (Sedoric, OricDOS)
 * - Dragon 32/64 (DragonDOS, OS-9)
 */

#include "uft_track_analysis.h"
#include <string.h>

/*===========================================================================
 * Acorn Archimedes (ADFS)
 *===========================================================================*/

static const uint32_t ARCHIMEDES_SYNCS[] = {
    0x4489,     /* Standard MFM A1 sync */
};

/**
 * @brief Archimedes ADFS D/E (800KB)
 * 
 * 80 tracks × 2 sides × 5 sectors × 1024 bytes = 819,200 bytes
 */
const uft_platform_profile_t UFT_PROFILE_ARCHIMEDES_D = {
    .platform = PLATFORM_ARCHIMEDES,
    .encoding = ENCODING_MFM,
    .name = "Acorn ADFS D/E",
    .sync_patterns = ARCHIMEDES_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 10000,
    .track_length_max = 13000,
    .track_length_nominal = 12500,
    .long_track_threshold = 12800,
    .sectors_per_track = 5,
    .sector_size = 1024,
    .sector_mfm_size = 1200,
    .sector_tolerance = 48,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/**
 * @brief Archimedes ADFS F (1.6MB)
 * 
 * 80 tracks × 2 sides × 10 sectors × 1024 bytes = 1,638,400 bytes
 */
const uft_platform_profile_t UFT_PROFILE_ARCHIMEDES_F = {
    .platform = PLATFORM_ARCHIMEDES,
    .encoding = ENCODING_MFM,
    .name = "Acorn ADFS F",
    .sync_patterns = ARCHIMEDES_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 20000,
    .track_length_max = 26000,
    .track_length_nominal = 25000,
    .long_track_threshold = 25500,
    .sectors_per_track = 10,
    .sector_size = 1024,
    .sector_mfm_size = 1200,
    .sector_tolerance = 48,
    .data_rate_kbps = 500.0,
    .rpm = 300.0
};

/**
 * @brief Archimedes ADFS G (1.6MB interleaved)
 */
const uft_platform_profile_t UFT_PROFILE_ARCHIMEDES_G = {
    .platform = PLATFORM_ARCHIMEDES,
    .encoding = ENCODING_MFM,
    .name = "Acorn ADFS G",
    .sync_patterns = ARCHIMEDES_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 20000,
    .track_length_max = 26000,
    .track_length_nominal = 25000,
    .long_track_threshold = 25500,
    .sectors_per_track = 10,
    .sector_size = 1024,
    .sector_mfm_size = 1200,
    .sector_tolerance = 48,
    .data_rate_kbps = 500.0,
    .rpm = 300.0
};

/*===========================================================================
 * SAM Coupé (MGT Format)
 *===========================================================================*/

static const uint32_t SAM_SYNCS[] = {
    0x4489,     /* MFM sync */
};

/**
 * @brief SAM Coupé MGT Format (800KB)
 * 
 * 80 tracks × 2 sides × 10 sectors × 512 bytes = 819,200 bytes
 * Also used by: +D, DISCiPLE interfaces
 */
const uft_platform_profile_t UFT_PROFILE_SAM_COUPE = {
    .platform = PLATFORM_SAM_COUPE,
    .encoding = ENCODING_MFM,
    .name = "SAM Coupé MGT",
    .sync_patterns = SAM_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 10000,
    .track_length_max = 13500,
    .track_length_nominal = 12500,
    .long_track_threshold = 13000,
    .sectors_per_track = 10,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/**
 * @brief SAM Coupé Boot Format (for +D/DISCiPLE)
 * 
 * Special boot sector format with 512-byte sectors
 */
const uft_platform_profile_t UFT_PROFILE_SAM_BOOT = {
    .platform = PLATFORM_SAM_COUPE,
    .encoding = ENCODING_MFM,
    .name = "SAM/+D Boot",
    .sync_patterns = SAM_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 10000,
    .track_length_max = 13500,
    .track_length_nominal = 12500,
    .long_track_threshold = 13000,
    .sectors_per_track = 10,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Sinclair Spectrum +3 (+3DOS)
 *===========================================================================*/

static const uint32_t PLUS3_SYNCS[] = {
    0x4489,     /* MFM sync */
};

/**
 * @brief Spectrum +3 Standard (+3DOS)
 * 
 * 40 tracks × 1 side × 9 sectors × 512 bytes = 180KB (SS)
 * 80 tracks × 2 sides × 9 sectors × 512 bytes = 720KB (DS)
 */
const uft_platform_profile_t UFT_PROFILE_SPECTRUM_PLUS3 = {
    .platform = PLATFORM_SPECTRUM_PLUS3,
    .encoding = ENCODING_MFM,
    .name = "Spectrum +3DOS",
    .sync_patterns = PLUS3_SYNCS,
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

/**
 * @brief Spectrum +3 Extended (PCW-compatible)
 * 
 * CP/M compatible format, 180KB per side
 */
const uft_platform_profile_t UFT_PROFILE_SPECTRUM_PLUS3_EXT = {
    .platform = PLATFORM_SPECTRUM_PLUS3,
    .encoding = ENCODING_MFM,
    .name = "Spectrum +3 Extended",
    .sync_patterns = PLUS3_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 10000,
    .track_length_max = 13000,
    .track_length_nominal = 12500,
    .long_track_threshold = 12800,
    .sectors_per_track = 9,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Oric Atmos (Sedoric, OricDOS)
 *===========================================================================*/

static const uint32_t ORIC_SYNCS[] = {
    0x4489,     /* MFM sync - Sedoric uses standard MFM */
    0xA1A1,     /* Alternative sync pattern */
};

/**
 * @brief Oric Sedoric Format
 * 
 * 42 tracks × 1 side × 17 sectors × 256 bytes = 182,784 bytes
 * Double-sided: 365,568 bytes
 */
const uft_platform_profile_t UFT_PROFILE_ORIC_SEDORIC = {
    .platform = PLATFORM_CUSTOM,  /* Oric doesn't have dedicated enum */
    .encoding = ENCODING_MFM,
    .name = "Oric Sedoric",
    .sync_patterns = ORIC_SYNCS,
    .sync_count = 2,
    .sync_bits = 16,
    .track_length_min = 5000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 17,
    .sector_size = 256,
    .sector_mfm_size = 340,
    .sector_tolerance = 24,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/**
 * @brief Oric Jasmin Format
 * 
 * Jasmin interface used different format
 */
const uft_platform_profile_t UFT_PROFILE_ORIC_JASMIN = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "Oric Jasmin",
    .sync_patterns = ORIC_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 5500,
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
 * Dragon 32/64 (DragonDOS, OS-9)
 *===========================================================================*/

static const uint32_t DRAGON_SYNCS[] = {
    0x4489,     /* MFM sync */
};

/**
 * @brief Dragon 32/64 DragonDOS
 * 
 * 40 tracks × 1 side × 18 sectors × 256 bytes = 184,320 bytes
 * 80 tracks × 2 sides: 737,280 bytes
 */
const uft_platform_profile_t UFT_PROFILE_DRAGON_DOS = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "DragonDOS",
    .sync_patterns = DRAGON_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 5500,
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
 * @brief Dragon/CoCo OS-9 Format
 * 
 * 35/40/80 tracks, various sector configurations
 */
const uft_platform_profile_t UFT_PROFILE_DRAGON_OS9 = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "Dragon/CoCo OS-9",
    .sync_patterns = DRAGON_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 5500,
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
 * Profile Lookup
 *===========================================================================*/

/**
 * @brief Get UK platform profile by name
 */
const uft_platform_profile_t* uft_get_uk_profile(const char *name)
{
    if (!name) return NULL;
    
    /* Archimedes */
    if (strstr(name, "ADFS D") || strstr(name, "ADFS E")) return &UFT_PROFILE_ARCHIMEDES_D;
    if (strstr(name, "ADFS F")) return &UFT_PROFILE_ARCHIMEDES_F;
    if (strstr(name, "ADFS G")) return &UFT_PROFILE_ARCHIMEDES_G;
    
    /* SAM Coupé */
    if (strstr(name, "SAM") || strstr(name, "MGT")) return &UFT_PROFILE_SAM_COUPE;
    
    /* Spectrum +3 */
    if (strstr(name, "+3") || strstr(name, "Plus3")) return &UFT_PROFILE_SPECTRUM_PLUS3;
    
    /* Oric */
    if (strstr(name, "Sedoric")) return &UFT_PROFILE_ORIC_SEDORIC;
    if (strstr(name, "Jasmin")) return &UFT_PROFILE_ORIC_JASMIN;
    
    /* Dragon */
    if (strstr(name, "Dragon") || strstr(name, "CoCo")) {
        if (strstr(name, "OS-9") || strstr(name, "OS9")) return &UFT_PROFILE_DRAGON_OS9;
        return &UFT_PROFILE_DRAGON_DOS;
    }
    
    return NULL;
}

/**
 * @brief Auto-detect UK format by image size
 */
const uft_platform_profile_t* uft_detect_uk_profile(size_t image_size)
{
    switch (image_size) {
        /* Archimedes ADFS */
        case 819200:    /* 800KB - ADFS D/E or SAM MGT */
            return &UFT_PROFILE_ARCHIMEDES_D;
        case 1638400:   /* 1.6MB - ADFS F/G */
            return &UFT_PROFILE_ARCHIMEDES_F;
            
        /* Spectrum +3 - 184320 also used by DragonDOS, prefer +3 */
        case 184320:    /* 180KB SS - Spectrum +3 or DragonDOS */
            return &UFT_PROFILE_SPECTRUM_PLUS3;
        case 737280:    /* 720KB DS */
            return &UFT_PROFILE_SPECTRUM_PLUS3_EXT;
            
        /* Oric */
        case 182784:    /* Sedoric SS */
        case 365568:    /* Sedoric DS */
            return &UFT_PROFILE_ORIC_SEDORIC;
            
        /* Note: DragonDOS 184320 conflicts with +3, use profile lookup by name */
            
        default:
            return NULL;
    }
}
