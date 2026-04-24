/**
 * @file uft_profile_japanese.c
 * @brief Platform profiles for Japanese computer formats
 * 
 * Profiles for:
 * - NEC PC-98 series (1024-byte sectors, 360 RPM)
 * - Sharp X68000 (1024-byte sectors)
 * - Fujitsu FM-Towns (various formats)
 */

#include "uft_track_analysis.h"

/*===========================================================================
 * NEC PC-98 Series
 *===========================================================================*/

/* PC-98 uses standard IBM MFM sync but with different geometry */
static const uint32_t PC98_SYNCS[] = {
    0x4489,     /* Standard MFM A1 sync */
};

/**
 * @brief PC-98 2DD (640KB)
 * 
 * 80 tracks × 2 sides × 8 sectors × 1024 bytes = 1,310,720 bytes
 * But DOS format uses 77 tracks = 1,261,568 bytes
 */
const uft_platform_profile_t UFT_PROFILE_PC98_2DD = {
    .platform = PLATFORM_PC98,
    .encoding = ENCODING_MFM,
    .name = "NEC PC-98 2DD",
    .sync_patterns = PC98_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 10000,
    .track_length_max = 13000,
    .track_length_nominal = 12500,
    .long_track_threshold = 12800,
    .sectors_per_track = 8,
    .sector_size = 1024,
    .sector_mfm_size = 1200,    /* 1024 data + header + gaps */
    .sector_tolerance = 48,
    .data_rate_kbps = 250.0,
    .rpm = 360.0                /* PC-98 spezifisch! */
};

/**
 * @brief PC-98 2HD (1.2MB)
 * 
 * 77 tracks × 2 sides × 8 sectors × 1024 bytes = 1,261,568 bytes
 * HD version: 15 sectors × 512 bytes
 */
const uft_platform_profile_t UFT_PROFILE_PC98_2HD = {
    .platform = PLATFORM_PC98,
    .encoding = ENCODING_MFM,
    .name = "NEC PC-98 2HD",
    .sync_patterns = PC98_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 20000,
    .track_length_max = 26000,
    .track_length_nominal = 25000,
    .long_track_threshold = 25500,
    .sectors_per_track = 15,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 500.0,
    .rpm = 360.0
};

/*===========================================================================
 * Sharp X68000
 *===========================================================================*/

static const uint32_t X68000_SYNCS[] = {
    0x4489,     /* Standard MFM sync */
};

/**
 * @brief X68000 2HD (1.2MB Human68k format)
 * 
 * 80 tracks × 2 sides × 8 sectors × 1024 bytes = 1,310,720 bytes
 * Human68k uses this format
 */
const uft_platform_profile_t UFT_PROFILE_X68000_2HD = {
    .platform = PLATFORM_X68000,
    .encoding = ENCODING_MFM,
    .name = "Sharp X68000 2HD",
    .sync_patterns = X68000_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 20000,
    .track_length_max = 26000,
    .track_length_nominal = 25000,
    .long_track_threshold = 25600,
    .sectors_per_track = 8,
    .sector_size = 1024,
    .sector_mfm_size = 1200,
    .sector_tolerance = 48,
    .data_rate_kbps = 500.0,
    .rpm = 300.0
};

/**
 * @brief X68000 2DD (640KB)
 */
const uft_platform_profile_t UFT_PROFILE_X68000_2DD = {
    .platform = PLATFORM_X68000,
    .encoding = ENCODING_MFM,
    .name = "Sharp X68000 2DD",
    .sync_patterns = X68000_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 10000,
    .track_length_max = 13000,
    .track_length_nominal = 12500,
    .long_track_threshold = 12800,
    .sectors_per_track = 8,
    .sector_size = 1024,
    .sector_mfm_size = 1200,
    .sector_tolerance = 48,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Fujitsu FM-Towns
 *===========================================================================*/

static const uint32_t FMTOWNS_SYNCS[] = {
    0x4489,
};

/**
 * @brief FM-Towns 2HD (1.2MB)
 * 
 * Can use both IBM-compatible and Towns-specific formats
 */
const uft_platform_profile_t UFT_PROFILE_FMTOWNS_2HD = {
    .platform = PLATFORM_FM_TOWNS,
    .encoding = ENCODING_MFM,
    .name = "Fujitsu FM-Towns 2HD",
    .sync_patterns = FMTOWNS_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 20000,
    .track_length_max = 26000,
    .track_length_nominal = 25000,
    .long_track_threshold = 25500,
    .sectors_per_track = 8,
    .sector_size = 1024,
    .sector_mfm_size = 1200,
    .sector_tolerance = 48,
    .data_rate_kbps = 500.0,
    .rpm = 300.0
};

/*===========================================================================
 * Profile Lookup
 *===========================================================================*/

/**
 * @brief Get Japanese platform profile by platform and density
 */
const uft_platform_profile_t* uft_get_japanese_profile(uft_platform_t platform, bool high_density)
{
    switch (platform) {
        case PLATFORM_PC98:
            return high_density ? &UFT_PROFILE_PC98_2HD : &UFT_PROFILE_PC98_2DD;
        case PLATFORM_X68000:
            return high_density ? &UFT_PROFILE_X68000_2HD : &UFT_PROFILE_X68000_2DD;
        case PLATFORM_FM_TOWNS:
            return &UFT_PROFILE_FMTOWNS_2HD;
        default:
            return NULL;
    }
}

/**
 * @brief Auto-detect Japanese format by image size
 */
const uft_platform_profile_t* uft_detect_japanese_profile(size_t image_size)
{
    /* Common Japanese format sizes */
    switch (image_size) {
        /* PC-98 */
        case 1261568:   /* 77 tracks × 2 sides × 8 sectors × 1024 */
            return &UFT_PROFILE_PC98_2DD;
        case 1228800:   /* 77 tracks × 2 sides × 8 sectors × 1024 (alternate) */
            return &UFT_PROFILE_PC98_2DD;
        case 1474560:   /* Standard 1.44M - could be PC-98 HD */
            return &UFT_PROFILE_PC98_2HD;
            
        /* X68000 */
        case 1310720:   /* 80 tracks × 2 sides × 8 sectors × 1024 */
            return &UFT_PROFILE_X68000_2HD;
        case 655360:    /* 80 tracks × 2 sides × 8 sectors × 512 */
            return &UFT_PROFILE_X68000_2DD;
            
        default:
            return NULL;
    }
}
