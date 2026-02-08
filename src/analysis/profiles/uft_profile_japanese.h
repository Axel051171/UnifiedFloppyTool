/**
 * @file uft_profile_japanese.h
 * @brief Platform profiles for Japanese computer formats
 * 
 * NEC PC-98, Sharp X68000, Fujitsu FM-Towns
 */

#ifndef UFT_PROFILE_JAPANESE_H
#define UFT_PROFILE_JAPANESE_H

#include "uft_track_analysis.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Pre-defined Profiles
 *===========================================================================*/

/* NEC PC-98 */
extern const uft_platform_profile_t UFT_PROFILE_PC98_2DD;   /* 640KB, 8×1024 */
extern const uft_platform_profile_t UFT_PROFILE_PC98_2HD;   /* 1.2MB, 15×512 */

/* Sharp X68000 */
extern const uft_platform_profile_t UFT_PROFILE_X68000_2DD; /* 640KB */
extern const uft_platform_profile_t UFT_PROFILE_X68000_2HD; /* 1.2MB */

/* Fujitsu FM-Towns */
extern const uft_platform_profile_t UFT_PROFILE_FMTOWNS_2HD;

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Get Japanese platform profile
 * 
 * @param platform PLATFORM_PC98, PLATFORM_X68000, or PLATFORM_FM_TOWNS
 * @param high_density true for HD, false for DD
 * @return Profile pointer or NULL
 */
const uft_platform_profile_t* uft_get_japanese_profile(uft_platform_t platform, bool high_density);

/**
 * @brief Auto-detect Japanese format by image size
 * 
 * @param image_size Size of disk image in bytes
 * @return Best matching profile or NULL
 */
const uft_platform_profile_t* uft_detect_japanese_profile(size_t image_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROFILE_JAPANESE_H */
