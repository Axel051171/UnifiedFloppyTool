/**
 * @file uft_profiles_all.h
 * @brief Master header for all platform profiles
 * 
 * Includes profiles for 50+ disk formats across:
 * - Japanese computers (PC-98, X68000, FM-Towns)
 * - UK computers (Archimedes, SAM Coupé, Spectrum +3, Oric, Dragon)
 * - US computers (TI-99, TRS-80, Victor 9000, Kaypro, Osborne)
 * - Misc computers (Enterprise, Einstein, Thomson, Microbee, etc.)
 * 
 * Usage:
 *   #include "uft_profiles_all.h"
 *   const uft_platform_profile_t *p = uft_find_profile_by_name("Amiga DD");
 *   // or
 *   const uft_platform_profile_t *p = uft_detect_profile_by_size(901120);
 */

#ifndef UFT_PROFILES_ALL_H
#define UFT_PROFILES_ALL_H

#include "uft_track_analysis.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Built-in Profiles (from uft_track_analysis.c)
 *===========================================================================*/

/* Amiga */
extern const uft_platform_profile_t UFT_PROFILE_AMIGA_DD;
extern const uft_platform_profile_t UFT_PROFILE_AMIGA_HD;

/* Atari ST */
extern const uft_platform_profile_t UFT_PROFILE_ATARI_ST_DD;
extern const uft_platform_profile_t UFT_PROFILE_ATARI_ST_HD;

/* IBM PC */
extern const uft_platform_profile_t UFT_PROFILE_IBM_DD;
extern const uft_platform_profile_t UFT_PROFILE_IBM_HD;

/* Apple II */
extern const uft_platform_profile_t UFT_PROFILE_APPLE_DOS33;
extern const uft_platform_profile_t UFT_PROFILE_APPLE_PRODOS;

/* Commodore 64 */
extern const uft_platform_profile_t UFT_PROFILE_C64;

/* BBC Micro */
extern const uft_platform_profile_t UFT_PROFILE_BBC_DFS;
extern const uft_platform_profile_t UFT_PROFILE_BBC_ADFS;

/* MSX */
extern const uft_platform_profile_t UFT_PROFILE_MSX;

/* Amstrad CPC */
extern const uft_platform_profile_t UFT_PROFILE_AMSTRAD;

/*===========================================================================
 * Japanese Profiles (from uft_profile_japanese.c)
 *===========================================================================*/

/* NEC PC-98 */
extern const uft_platform_profile_t UFT_PROFILE_PC98_2DD;
extern const uft_platform_profile_t UFT_PROFILE_PC98_2HD;

/* Sharp X68000 */
extern const uft_platform_profile_t UFT_PROFILE_X68000_2DD;
extern const uft_platform_profile_t UFT_PROFILE_X68000_2HD;

/* Fujitsu FM-Towns */
extern const uft_platform_profile_t UFT_PROFILE_FMTOWNS_2HD;

/*===========================================================================
 * UK Profiles (from uft_profile_uk.c)
 *===========================================================================*/

/* Acorn Archimedes */
extern const uft_platform_profile_t UFT_PROFILE_ARCHIMEDES_D;
extern const uft_platform_profile_t UFT_PROFILE_ARCHIMEDES_F;
extern const uft_platform_profile_t UFT_PROFILE_ARCHIMEDES_G;

/* SAM Coupé */
extern const uft_platform_profile_t UFT_PROFILE_SAM_COUPE;
extern const uft_platform_profile_t UFT_PROFILE_SAM_BOOT;

/* Spectrum +3 */
extern const uft_platform_profile_t UFT_PROFILE_SPECTRUM_PLUS3;
extern const uft_platform_profile_t UFT_PROFILE_SPECTRUM_PLUS3_EXT;

/* Oric */
extern const uft_platform_profile_t UFT_PROFILE_ORIC_SEDORIC;
extern const uft_platform_profile_t UFT_PROFILE_ORIC_JASMIN;

/* Dragon */
extern const uft_platform_profile_t UFT_PROFILE_DRAGON_DOS;
extern const uft_platform_profile_t UFT_PROFILE_DRAGON_OS9;

/*===========================================================================
 * US Profiles (from uft_profile_us.c)
 *===========================================================================*/

/* TI-99/4A */
extern const uft_platform_profile_t UFT_PROFILE_TI99_SSSD;
extern const uft_platform_profile_t UFT_PROFILE_TI99_SSDD;
extern const uft_platform_profile_t UFT_PROFILE_TI99_DSDD;

/* TRS-80 */
extern const uft_platform_profile_t UFT_PROFILE_TRS80_SSSD;
extern const uft_platform_profile_t UFT_PROFILE_TRS80_DSDD;
extern const uft_platform_profile_t UFT_PROFILE_TRS80_80TRACK;

/* Victor 9000 / Sirius 1 */
extern const uft_platform_profile_t UFT_PROFILE_VICTOR_9000;

/* Kaypro */
extern const uft_platform_profile_t UFT_PROFILE_KAYPRO_SSDD;
extern const uft_platform_profile_t UFT_PROFILE_KAYPRO_DSDD;

/* Osborne */
extern const uft_platform_profile_t UFT_PROFILE_OSBORNE_SSSD;
extern const uft_platform_profile_t UFT_PROFILE_OSBORNE_SSDD;

/*===========================================================================
 * Misc Profiles (from uft_profile_misc.c)
 *===========================================================================*/

/* Acorn Electron */
extern const uft_platform_profile_t UFT_PROFILE_ELECTRON_DFS;
extern const uft_platform_profile_t UFT_PROFILE_ELECTRON_ADFS;

/* Enterprise */
extern const uft_platform_profile_t UFT_PROFILE_ENTERPRISE;

/* Tatung Einstein */
extern const uft_platform_profile_t UFT_PROFILE_EINSTEIN;

/* Memotech */
extern const uft_platform_profile_t UFT_PROFILE_MEMOTECH;

/* Thomson */
extern const uft_platform_profile_t UFT_PROFILE_THOMSON_MO5;
extern const uft_platform_profile_t UFT_PROFILE_THOMSON_TO8;

/* Microbee */
extern const uft_platform_profile_t UFT_PROFILE_MICROBEE_DS40;
extern const uft_platform_profile_t UFT_PROFILE_MICROBEE_DS80;

/* Sord M5 */
extern const uft_platform_profile_t UFT_PROFILE_SORD_M5;

/* IBM Extended Formats (XDF, DMF) */
extern const uft_platform_profile_t UFT_PROFILE_IBM_XDF;
extern const uft_platform_profile_t UFT_PROFILE_IBM_XXDF;
extern const uft_platform_profile_t UFT_PROFILE_IBM_DMF;

/*===========================================================================
 * Profile Lookup Functions
 *===========================================================================*/

/**
 * @brief Find profile by name (case-insensitive partial match)
 * 
 * @param name Profile name to search (e.g., "Amiga DD", "PC-98", "TRS-80")
 * @return Profile pointer or NULL if not found
 */
const uft_platform_profile_t* uft_find_profile_by_name(const char *name);

/**
 * @brief Detect profile by disk image size
 * 
 * @param image_size Size of disk image in bytes
 * @return Best matching profile or NULL
 */
const uft_platform_profile_t* uft_detect_profile_by_size(size_t image_size);

/**
 * @brief Get profile by platform enum
 * 
 * @param platform Platform enum value
 * @param high_density true for HD, false for DD
 * @return Profile pointer or NULL
 */
const uft_platform_profile_t* uft_get_profile_by_platform(uft_platform_t platform, bool high_density);

/**
 * @brief Get all available profiles as array
 * 
 * @param count Output: number of profiles
 * @return Array of profile pointers (do not free)
 */
const uft_platform_profile_t* const* uft_get_all_profiles(int *count);

/**
 * @brief Get total number of available profiles
 */
int uft_get_profile_count(void);

/*===========================================================================
 * Victor 9000 Special Helper
 *===========================================================================*/

/**
 * @brief Get sectors per track for Victor 9000 (zone-based)
 * 
 * Victor 9000 uses variable sectors per track (19 to 11)
 * based on track position.
 * 
 * @param track Track number (0-79)
 * @return Sectors per track for this zone
 */
int uft_victor_sectors_for_track(int track);

/*===========================================================================
 * XDF Special Helpers
 *===========================================================================*/

/**
 * @brief Get sectors per track for IBM XDF
 * 
 * XDF uses variable sector counts (4 on track 0, 5 on others)
 * and variable sector sizes (512B to 8KB).
 * 
 * @param track Track number (0-79)
 * @return Sectors per track (4 or 5)
 */
int uft_xdf_sectors_for_track(int track);

/**
 * @brief Check if format requires Track Copy mode
 * 
 * XDF and similar formats cannot use normal sector copy.
 * 
 * @param format_name Format identifier
 * @return true if Track/Nibble/Flux copy required
 */
bool uft_format_requires_track_copy(const char *format_name);

/**
 * @brief Get recommended copy mode for XDF
 * 
 * @param has_protection true if copy protection detected
 * @return 2=Track Copy, 3=Flux Copy
 */
int uft_xdf_recommended_copy_mode(bool has_protection);

/*===========================================================================
 * Profile Categories
 *===========================================================================*/

typedef enum {
    PROFILE_CAT_ALL = 0,
    PROFILE_CAT_JAPANESE,
    PROFILE_CAT_UK,
    PROFILE_CAT_US,
    PROFILE_CAT_MISC,
    PROFILE_CAT_MFM,
    PROFILE_CAT_FM,
    PROFILE_CAT_GCR
} uft_profile_category_t;

/**
 * @brief Get profiles by category
 * 
 * @param category Category filter
 * @param count Output: number of profiles
 * @return Array of profile pointers
 */
const uft_platform_profile_t* const* uft_get_profiles_by_category(
    uft_profile_category_t category, int *count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROFILES_ALL_H */
