/**
 * @file uft_profiles_all.c
 * @brief Central profile lookup implementation
 * 
 * Provides unified access to all 50+ platform profiles.
 */

#include "uft_profiles_all.h"
#include <string.h>
#include <ctype.h>

/*===========================================================================
 * Profile Registry
 *===========================================================================*/

/* Forward declarations for external lookup functions */
extern const uft_platform_profile_t* uft_get_japanese_profile(uft_platform_t platform, bool high_density);
extern const uft_platform_profile_t* uft_detect_japanese_profile(size_t image_size);
extern const uft_platform_profile_t* uft_get_uk_profile(const char *name);
extern const uft_platform_profile_t* uft_detect_uk_profile(size_t image_size);
extern const uft_platform_profile_t* uft_get_us_profile(const char *name);
extern const uft_platform_profile_t* uft_detect_us_profile(size_t image_size);
extern const uft_platform_profile_t* uft_get_misc_profile(const char *name);

/* All profiles in a flat array for iteration */
static const uft_platform_profile_t* const ALL_PROFILES[] = {
    /* Built-in (13) */
    &UFT_PROFILE_AMIGA_DD,
    &UFT_PROFILE_AMIGA_HD,
    &UFT_PROFILE_ATARI_ST_DD,
    &UFT_PROFILE_ATARI_ST_HD,
    &UFT_PROFILE_IBM_DD,
    &UFT_PROFILE_IBM_HD,
    &UFT_PROFILE_APPLE_DOS33,
    &UFT_PROFILE_APPLE_PRODOS,
    &UFT_PROFILE_C64,
    &UFT_PROFILE_BBC_DFS,
    &UFT_PROFILE_BBC_ADFS,
    &UFT_PROFILE_MSX,
    &UFT_PROFILE_AMSTRAD,
    
    /* Japanese (5) */
    &UFT_PROFILE_PC98_2DD,
    &UFT_PROFILE_PC98_2HD,
    &UFT_PROFILE_X68000_2DD,
    &UFT_PROFILE_X68000_2HD,
    &UFT_PROFILE_FMTOWNS_2HD,
    
    /* UK (12) */
    &UFT_PROFILE_ARCHIMEDES_D,
    &UFT_PROFILE_ARCHIMEDES_F,
    &UFT_PROFILE_ARCHIMEDES_G,
    &UFT_PROFILE_SAM_COUPE,
    &UFT_PROFILE_SAM_BOOT,
    &UFT_PROFILE_SPECTRUM_PLUS3,
    &UFT_PROFILE_SPECTRUM_PLUS3_EXT,
    &UFT_PROFILE_ORIC_SEDORIC,
    &UFT_PROFILE_ORIC_JASMIN,
    &UFT_PROFILE_DRAGON_DOS,
    &UFT_PROFILE_DRAGON_OS9,
    
    /* US (12) */
    &UFT_PROFILE_TI99_SSSD,
    &UFT_PROFILE_TI99_SSDD,
    &UFT_PROFILE_TI99_DSDD,
    &UFT_PROFILE_TRS80_SSSD,
    &UFT_PROFILE_TRS80_DSDD,
    &UFT_PROFILE_TRS80_80TRACK,
    &UFT_PROFILE_VICTOR_9000,
    &UFT_PROFILE_KAYPRO_SSDD,
    &UFT_PROFILE_KAYPRO_DSDD,
    &UFT_PROFILE_OSBORNE_SSSD,
    &UFT_PROFILE_OSBORNE_SSDD,
    
    /* Misc (11) */
    &UFT_PROFILE_ELECTRON_DFS,
    &UFT_PROFILE_ELECTRON_ADFS,
    &UFT_PROFILE_ENTERPRISE,
    &UFT_PROFILE_EINSTEIN,
    &UFT_PROFILE_MEMOTECH,
    &UFT_PROFILE_THOMSON_MO5,
    &UFT_PROFILE_THOMSON_TO8,
    &UFT_PROFILE_MICROBEE_DS40,
    &UFT_PROFILE_MICROBEE_DS80,
    &UFT_PROFILE_SORD_M5,
    
    /* IBM Extended Formats (3) */
    &UFT_PROFILE_IBM_XDF,
    &UFT_PROFILE_IBM_XXDF,
    &UFT_PROFILE_IBM_DMF,
};

#define PROFILE_COUNT (sizeof(ALL_PROFILES) / sizeof(ALL_PROFILES[0]))

/*===========================================================================
 * String Helpers
 *===========================================================================*/

static int str_contains_ci(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return 0;
    
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    
    if (nlen > hlen) return 0;
    
    for (size_t i = 0; i <= hlen - nlen; i++) {
        size_t j;
        for (j = 0; j < nlen; j++) {
            if (tolower((unsigned char)haystack[i+j]) != tolower((unsigned char)needle[j]))
                break;
        }
        if (j == nlen) return 1;
    }
    return 0;
}

/*===========================================================================
 * Profile Lookup Functions
 *===========================================================================*/

const uft_platform_profile_t* uft_find_profile_by_name(const char *name)
{
    if (!name) return NULL;
    
    /* Search all profiles by name */
    for (size_t i = 0; i < PROFILE_COUNT; i++) {
        if (str_contains_ci(ALL_PROFILES[i]->name, name)) {
            return ALL_PROFILES[i];
        }
    }
    
    /* Try category-specific lookups */
    const uft_platform_profile_t *p;
    
    p = uft_get_uk_profile(name);
    if (p) return p;
    
    p = uft_get_us_profile(name);
    if (p) return p;
    
    p = uft_get_misc_profile(name);
    if (p) return p;
    
    return NULL;
}

const uft_platform_profile_t* uft_detect_profile_by_size(size_t image_size)
{
    const uft_platform_profile_t *p;
    
    /* Common sizes - check most likely first */
    switch (image_size) {
        /* Amiga */
        case 901120:    return &UFT_PROFILE_AMIGA_DD;   /* ADF */
        case 1802240:   return &UFT_PROFILE_AMIGA_HD;   /* ADF HD */
        
        /* IBM PC */
        case 163840:    return &UFT_PROFILE_IBM_DD;     /* 160KB */
        case 184320:    return &UFT_PROFILE_IBM_DD;     /* 180KB */
        case 327680:    return &UFT_PROFILE_IBM_DD;     /* 320KB */
        case 368640:    return &UFT_PROFILE_IBM_DD;     /* 360KB */
        case 737280:    return &UFT_PROFILE_IBM_DD;     /* 720KB */
        case 1228800:   return &UFT_PROFILE_IBM_HD;     /* 1.2MB */
        case 1474560:   return &UFT_PROFILE_IBM_HD;     /* 1.44MB */
        case 2949120:   return &UFT_PROFILE_IBM_HD;     /* 2.88MB ED */
        
        /* IBM XDF / DMF Extended Formats */
        case 1720320:   return &UFT_PROFILE_IBM_DMF;    /* DMF 1.68MB */
        case 1763328:   return &UFT_PROFILE_IBM_DMF;    /* DMF variant */
        /* Note: 1802240 conflicts with Amiga HD - use name detection for XXDF */
        case 1884160:   return &UFT_PROFILE_IBM_XDF;    /* XDF variant */
        case 1900544:   return &UFT_PROFILE_IBM_XDF;    /* XDF variant 2 */
        case 1915904:   return &UFT_PROFILE_IBM_XDF;    /* Standard XDF */
        
        /* Atari ST */
        case 357376:    return &UFT_PROFILE_ATARI_ST_DD; /* SSDD */
        case 714752:    return &UFT_PROFILE_ATARI_ST_DD; /* DSDD */
        
        /* C64 */
        case 174848:    return &UFT_PROFILE_C64;        /* D64 */
        case 175531:    return &UFT_PROFILE_C64;        /* D64 + errors */
        case 196608:    return &UFT_PROFILE_C64;        /* D64 40-track */
        case 349696:    return &UFT_PROFILE_C64;        /* D71 */
        /* Note: 819200 (D81) conflicts with Apple ProDOS - handled below */
        
        /* Apple II */
        case 143360:    return &UFT_PROFILE_APPLE_DOS33; /* 5.25" */
        /* Note: 819200 conflicts with D81, prefer Apple ProDOS for this size */
        case 819200:    return &UFT_PROFILE_APPLE_PRODOS; /* 3.5" 800K or D81 */
    }
    
    /* Try Japanese */
    p = uft_detect_japanese_profile(image_size);
    if (p) return p;
    
    /* Try UK */
    p = uft_detect_uk_profile(image_size);
    if (p) return p;
    
    /* Try US */
    p = uft_detect_us_profile(image_size);
    if (p) return p;
    
    return NULL;
}

const uft_platform_profile_t* uft_get_profile_by_platform(uft_platform_t platform, bool high_density)
{
    switch (platform) {
        case PLATFORM_AMIGA:
            return high_density ? &UFT_PROFILE_AMIGA_HD : &UFT_PROFILE_AMIGA_DD;
        case PLATFORM_ATARI_ST:
            return high_density ? &UFT_PROFILE_ATARI_ST_HD : &UFT_PROFILE_ATARI_ST_DD;
        case PLATFORM_IBM_PC:
            return high_density ? &UFT_PROFILE_IBM_HD : &UFT_PROFILE_IBM_DD;
        case PLATFORM_APPLE_II:
            return high_density ? &UFT_PROFILE_APPLE_PRODOS : &UFT_PROFILE_APPLE_DOS33;
        case PLATFORM_C64:
            return &UFT_PROFILE_C64;
        case PLATFORM_BBC_MICRO:
            return high_density ? &UFT_PROFILE_BBC_ADFS : &UFT_PROFILE_BBC_DFS;
        case PLATFORM_MSX:
            return &UFT_PROFILE_MSX;
        case PLATFORM_AMSTRAD_CPC:
            return &UFT_PROFILE_AMSTRAD;
        case PLATFORM_PC98:
            return uft_get_japanese_profile(platform, high_density);
        case PLATFORM_X68000:
            return uft_get_japanese_profile(platform, high_density);
        case PLATFORM_FM_TOWNS:
            return uft_get_japanese_profile(platform, high_density);
        case PLATFORM_ARCHIMEDES:
            return high_density ? &UFT_PROFILE_ARCHIMEDES_F : &UFT_PROFILE_ARCHIMEDES_D;
        case PLATFORM_SAM_COUPE:
            return &UFT_PROFILE_SAM_COUPE;
        case PLATFORM_SPECTRUM_PLUS3:
            return &UFT_PROFILE_SPECTRUM_PLUS3;
        default:
            return NULL;
    }
}

const uft_platform_profile_t* const* uft_get_all_profiles(int *count)
{
    if (count) *count = PROFILE_COUNT;
    return ALL_PROFILES;
}

int uft_get_profile_count(void)
{
    return PROFILE_COUNT;
}

/*===========================================================================
 * Category Filtering
 *===========================================================================*/

/* Japanese profiles */
static const uft_platform_profile_t* const JAPANESE_PROFILES[] = {
    &UFT_PROFILE_PC98_2DD,
    &UFT_PROFILE_PC98_2HD,
    &UFT_PROFILE_X68000_2DD,
    &UFT_PROFILE_X68000_2HD,
    &UFT_PROFILE_FMTOWNS_2HD,
};

/* UK profiles */
static const uft_platform_profile_t* const UK_PROFILES[] = {
    &UFT_PROFILE_BBC_DFS,
    &UFT_PROFILE_BBC_ADFS,
    &UFT_PROFILE_ARCHIMEDES_D,
    &UFT_PROFILE_ARCHIMEDES_F,
    &UFT_PROFILE_ARCHIMEDES_G,
    &UFT_PROFILE_SAM_COUPE,
    &UFT_PROFILE_SAM_BOOT,
    &UFT_PROFILE_SPECTRUM_PLUS3,
    &UFT_PROFILE_SPECTRUM_PLUS3_EXT,
    &UFT_PROFILE_ORIC_SEDORIC,
    &UFT_PROFILE_ORIC_JASMIN,
    &UFT_PROFILE_DRAGON_DOS,
    &UFT_PROFILE_DRAGON_OS9,
    &UFT_PROFILE_ELECTRON_DFS,
    &UFT_PROFILE_ELECTRON_ADFS,
};

/* US profiles */
static const uft_platform_profile_t* const US_PROFILES[] = {
    &UFT_PROFILE_APPLE_DOS33,
    &UFT_PROFILE_APPLE_PRODOS,
    &UFT_PROFILE_TI99_SSSD,
    &UFT_PROFILE_TI99_SSDD,
    &UFT_PROFILE_TI99_DSDD,
    &UFT_PROFILE_TRS80_SSSD,
    &UFT_PROFILE_TRS80_DSDD,
    &UFT_PROFILE_TRS80_80TRACK,
    &UFT_PROFILE_VICTOR_9000,
    &UFT_PROFILE_KAYPRO_SSDD,
    &UFT_PROFILE_KAYPRO_DSDD,
    &UFT_PROFILE_OSBORNE_SSSD,
    &UFT_PROFILE_OSBORNE_SSDD,
};

const uft_platform_profile_t* const* uft_get_profiles_by_category(
    uft_profile_category_t category, int *count)
{
    switch (category) {
        case PROFILE_CAT_ALL:
            if (count) *count = PROFILE_COUNT;
            return ALL_PROFILES;
            
        case PROFILE_CAT_JAPANESE:
            if (count) *count = sizeof(JAPANESE_PROFILES) / sizeof(JAPANESE_PROFILES[0]);
            return JAPANESE_PROFILES;
            
        case PROFILE_CAT_UK:
            if (count) *count = sizeof(UK_PROFILES) / sizeof(UK_PROFILES[0]);
            return UK_PROFILES;
            
        case PROFILE_CAT_US:
            if (count) *count = sizeof(US_PROFILES) / sizeof(US_PROFILES[0]);
            return US_PROFILES;
            
        default:
            if (count) *count = 0;
            return NULL;
    }
}
