/**
 * @file uft_pc_protection.c
 * @brief TICKET-008: PC Copy Protection Detection Implementation
 * 
 * Detects SafeDisc, SecuROM, StarForce and other PC protections.
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#include "uft/uft_pc_protection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * Signature Database
 *===========================================================================*/

/* SafeDisc signatures */
static const uint8_t sig_safedisc_00015000[] = {
    0x00, 0x00, 0x01, 0x50, 0x00  /* SafeDisc marker in .data section */
};

static const uint8_t sig_safedisc_clcd32[] = {
    'C', 'L', 'C', 'D', '3', '2', '.', 'D', 'L', 'L'  /* CLCD32.DLL */
};

static const uint8_t sig_safedisc_clokspl[] = {
    'C', 'L', 'O', 'K', 'S', 'P', 'L', '.', 'E', 'X', 'E'
};

static const uint8_t sig_safedisc_secdrv[] = {
    'S', 'E', 'C', 'D', 'R', 'V', '.', 'S', 'Y', 'S'
};

static const uint8_t sig_safedisc_dplayerx[] = {
    'd', 'p', 'l', 'a', 'y', 'e', 'r', 'x', '.', 'd', 'l', 'l'
};

/* SecuROM signatures */
static const uint8_t sig_securom_cms16[] = {
    'C', 'M', 'S', '1', '6', '.', 'D', 'L', 'L'
};

static const uint8_t sig_securom_cms32[] = {
    'C', 'M', 'S', '3', '2', '_', 'N', 'T', '.', 'D', 'L', 'L'
};

static const uint8_t sig_securom_secdrvnt[] = {
    'S', 'E', 'C', 'D', 'R', 'V', 'N', 'T', '.', 'S', 'Y', 'S'
};

static const uint8_t sig_securom_pa[] = {
    0x57, 0x57, 0x57, 0x2E, 0x73, 0x65, 0x63, 0x75,  /* "WWW.secu" */
    0x72, 0x6F, 0x6D, 0x2E, 0x63, 0x6F, 0x6D         /* "rom.com" */
};

static const uint8_t sig_securom_drm[] = {
    0x53, 0x65, 0x63, 0x75, 0x52, 0x4F, 0x4D
};

/* StarForce signatures */
static const uint8_t sig_starforce_protect[] = {
    'p', 'r', 'o', 't', 'e', 'c', 't', '.', 'd', 'l', 'l'
};

static const uint8_t sig_starforce_sfdrv01[] = {
    's', 'f', 'd', 'r', 'v', '0', '1', '.', 's', 'y', 's'
};

static const uint8_t sig_starforce_sfhlp01[] = {
    's', 'f', 'h', 'l', 'p', '0', '1', '.', 's', 'y', 's'
};

static const uint8_t sig_starforce_sfvfs[] = {
    's', 'f', 'v', 'f', 's', '0', '2', '.', 's', 'y', 's'
};

/* LaserLock signatures */
static const uint8_t sig_laserlock_laserlok[] = {
    'L', 'A', 'S', 'E', 'R', 'L', 'O', 'K', '.', 'I', 'N'
};

static const uint8_t sig_laserlock_llock[] = {
    'L', 'L', 'O', 'C', 'K', '0', '1', '0'
};

/* CD-Cops signatures */
static const uint8_t sig_cdcops_icd[] = {
    'I', 'C', 'D', '1', '0', '.', 'I', 'C', 'D'
};

static const uint8_t sig_cdcops_cd32[] = {
    'C', 'D', '3', '2', '.', 'D', 'L', 'L'
};

/* TAGES signatures */
static const uint8_t sig_tages_wave[] = {
    'W', 'A', 'V', 'E', '.', 'A', 'L', 'L'
};

/* Solidshield signatures */
static const uint8_t sig_solidshield[] = {
    'S', 'o', 'l', 'i', 'd', 'S', 'h', 'i', 'e', 'l', 'd'
};

/*===========================================================================
 * Built-in Signature Table
 *===========================================================================*/

static const uft_pcprot_sig_t builtin_signatures[] = {
    /* SafeDisc */
    { UFT_PCPROT_SAFEDISC_1, "SafeDisc CLCD32", UFT_SIG_EXACT,
      sig_safedisc_clcd32, sizeof(sig_safedisc_clcd32), NULL, -1,
      "*.dll", "1.x" },
    { UFT_PCPROT_SAFEDISC_1, "SafeDisc CLOKSPL", UFT_SIG_EXACT,
      sig_safedisc_clokspl, sizeof(sig_safedisc_clokspl), NULL, -1,
      "*.exe", "1.x" },
    { UFT_PCPROT_SAFEDISC_2, "SafeDisc SECDRV", UFT_SIG_EXACT,
      sig_safedisc_secdrv, sizeof(sig_safedisc_secdrv), NULL, -1,
      "*.sys", "2.x+" },
    { UFT_PCPROT_SAFEDISC_3, "SafeDisc dplayerx", UFT_SIG_EXACT,
      sig_safedisc_dplayerx, sizeof(sig_safedisc_dplayerx), NULL, -1,
      "*.dll", "3.x+" },
    { UFT_PCPROT_SAFEDISC_1, "SafeDisc marker", UFT_SIG_EXACT,
      sig_safedisc_00015000, sizeof(sig_safedisc_00015000), NULL, -1,
      "*.exe", "1.x" },
      
    /* SecuROM */
    { UFT_PCPROT_SECUROM_1, "SecuROM CMS16", UFT_SIG_EXACT,
      sig_securom_cms16, sizeof(sig_securom_cms16), NULL, -1,
      "*.dll", "1.x-3.x" },
    { UFT_PCPROT_SECUROM_3, "SecuROM CMS32_NT", UFT_SIG_EXACT,
      sig_securom_cms32, sizeof(sig_securom_cms32), NULL, -1,
      "*.dll", "3.x+" },
    { UFT_PCPROT_SECUROM_4, "SecuROM SECDRVNT", UFT_SIG_EXACT,
      sig_securom_secdrvnt, sizeof(sig_securom_secdrvnt), NULL, -1,
      "*.sys", "4.x+" },
    { UFT_PCPROT_SECUROM_PA, "SecuROM PA URL", UFT_SIG_EXACT,
      sig_securom_pa, sizeof(sig_securom_pa), NULL, -1,
      NULL, "PA" },
    { UFT_PCPROT_SECUROM_7, "SecuROM DRM", UFT_SIG_EXACT,
      sig_securom_drm, sizeof(sig_securom_drm), NULL, -1,
      "*.exe", "7.x" },
      
    /* StarForce */
    { UFT_PCPROT_STARFORCE_1, "StarForce protect.dll", UFT_SIG_EXACT,
      sig_starforce_protect, sizeof(sig_starforce_protect), NULL, -1,
      "*.dll", "1.x+" },
    { UFT_PCPROT_STARFORCE_2, "StarForce sfdrv01", UFT_SIG_EXACT,
      sig_starforce_sfdrv01, sizeof(sig_starforce_sfdrv01), NULL, -1,
      "*.sys", "2.x+" },
    { UFT_PCPROT_STARFORCE_3, "StarForce sfhlp01", UFT_SIG_EXACT,
      sig_starforce_sfhlp01, sizeof(sig_starforce_sfhlp01), NULL, -1,
      "*.sys", "3.x" },
    { UFT_PCPROT_STARFORCE_PRO, "StarForce sfvfs", UFT_SIG_EXACT,
      sig_starforce_sfvfs, sizeof(sig_starforce_sfvfs), NULL, -1,
      "*.sys", "Pro" },
      
    /* LaserLock */
    { UFT_PCPROT_LASERLOCK, "LaserLock LASERLOK.IN", UFT_SIG_EXACT,
      sig_laserlock_laserlok, sizeof(sig_laserlock_laserlok), NULL, -1,
      NULL, NULL },
    { UFT_PCPROT_LASERLOCK_XTREME, "LaserLock LLOCK010", UFT_SIG_EXACT,
      sig_laserlock_llock, sizeof(sig_laserlock_llock), NULL, -1,
      NULL, "Xtreme" },
      
    /* CD-Cops */
    { UFT_PCPROT_CDCOPS, "CD-Cops ICD10", UFT_SIG_EXACT,
      sig_cdcops_icd, sizeof(sig_cdcops_icd), NULL, -1,
      "*.icd", NULL },
    { UFT_PCPROT_CDCOPS, "CD-Cops CD32", UFT_SIG_EXACT,
      sig_cdcops_cd32, sizeof(sig_cdcops_cd32), NULL, -1,
      "*.dll", NULL },
      
    /* TAGES */
    { UFT_PCPROT_TAGES, "TAGES WAVE.ALL", UFT_SIG_EXACT,
      sig_tages_wave, sizeof(sig_tages_wave), NULL, -1,
      NULL, NULL },
      
    /* SolidShield */
    { UFT_PCPROT_SOLIDSHIELD, "SolidShield marker", UFT_SIG_EXACT,
      sig_solidshield, sizeof(sig_solidshield), NULL, -1,
      NULL, NULL },
};

#define BUILTIN_SIG_COUNT (sizeof(builtin_signatures) / sizeof(builtin_signatures[0]))

/*===========================================================================
 * String Tables
 *===========================================================================*/

static const char *protection_names[] = {
    [UFT_PCPROT_UNKNOWN] = "Unknown",
    [UFT_PCPROT_SAFEDISC_1] = "SafeDisc 1.x",
    [UFT_PCPROT_SAFEDISC_2] = "SafeDisc 2.x",
    [UFT_PCPROT_SAFEDISC_3] = "SafeDisc 3.x",
    [UFT_PCPROT_SAFEDISC_4] = "SafeDisc 4.x",
    [UFT_PCPROT_SECUROM_1] = "SecuROM 1.x",
    [UFT_PCPROT_SECUROM_2] = "SecuROM 2.x",
    [UFT_PCPROT_SECUROM_3] = "SecuROM 3.x",
    [UFT_PCPROT_SECUROM_4] = "SecuROM 4.x",
    [UFT_PCPROT_SECUROM_5] = "SecuROM 5.x",
    [UFT_PCPROT_SECUROM_7] = "SecuROM 7.x",
    [UFT_PCPROT_SECUROM_PA] = "SecuROM PA",
    [UFT_PCPROT_STARFORCE_1] = "StarForce 1.x",
    [UFT_PCPROT_STARFORCE_2] = "StarForce 2.x",
    [UFT_PCPROT_STARFORCE_3] = "StarForce 3.x",
    [UFT_PCPROT_STARFORCE_PRO] = "StarForce Pro",
    [UFT_PCPROT_CDCOPS] = "CD-Cops",
    [UFT_PCPROT_LINKDATA] = "Link Data",
    [UFT_PCPROT_LASERLOCK] = "LaserLock",
    [UFT_PCPROT_LASERLOCK_XTREME] = "LaserLock Xtreme",
    [UFT_PCPROT_TAGES] = "TAGES",
    [UFT_PCPROT_SOLIDSHIELD] = "SolidShield",
    [UFT_PCPROT_ARMADILLO] = "Armadillo",
    [UFT_PCPROT_ASPROTECT] = "ASProtect",
    [UFT_PCPROT_EXECRYPTOR] = "EXECryptor",
    [UFT_PCPROT_THEMIDA] = "Themida",
    [UFT_PCPROT_VMProtect] = "VMProtect",
    [UFT_PCPROT_CDCHECK] = "CD-Check",
    [UFT_PCPROT_ATIP_CHECK] = "ATIP Check",
    [UFT_PCPROT_OVERBURN] = "Overburn",
    [UFT_PCPROT_DUMMY_FILES] = "Dummy Files",
    [UFT_PCPROT_BAD_SECTORS] = "Bad Sectors",
    [UFT_PCPROT_TWIN_SECTORS] = "Twin Sectors",
    [UFT_PCPROT_WEAK_SECTORS] = "Weak Sectors",
    [UFT_PCPROT_SUBCODE] = "Subcode Protection",
};

static const char *vendor_names[] = {
    [UFT_PCPROT_UNKNOWN] = "Unknown",
    [UFT_PCPROT_SAFEDISC_1] = "Macrovision",
    [UFT_PCPROT_SAFEDISC_2] = "Macrovision",
    [UFT_PCPROT_SAFEDISC_3] = "Macrovision",
    [UFT_PCPROT_SAFEDISC_4] = "Macrovision",
    [UFT_PCPROT_SECUROM_1] = "Sony DADC",
    [UFT_PCPROT_SECUROM_2] = "Sony DADC",
    [UFT_PCPROT_SECUROM_3] = "Sony DADC",
    [UFT_PCPROT_SECUROM_4] = "Sony DADC",
    [UFT_PCPROT_SECUROM_5] = "Sony DADC",
    [UFT_PCPROT_SECUROM_7] = "Sony DADC",
    [UFT_PCPROT_SECUROM_PA] = "Sony DADC",
    [UFT_PCPROT_STARFORCE_1] = "Protection Technology",
    [UFT_PCPROT_STARFORCE_2] = "Protection Technology",
    [UFT_PCPROT_STARFORCE_3] = "Protection Technology",
    [UFT_PCPROT_STARFORCE_PRO] = "Protection Technology",
    [UFT_PCPROT_CDCOPS] = "Link Data Security",
    [UFT_PCPROT_LINKDATA] = "Link Data Security",
    [UFT_PCPROT_LASERLOCK] = "MLS LaserLock",
    [UFT_PCPROT_LASERLOCK_XTREME] = "MLS LaserLock",
    [UFT_PCPROT_TAGES] = "Thomson/MPO",
    [UFT_PCPROT_SOLIDSHIELD] = "Solidshield",
};

/*===========================================================================
 * Memory Pattern Matching
 *===========================================================================*/

/**
 * @brief Find pattern in buffer (Boyer-Moore-Horspool)
 */
static int64_t find_pattern(const uint8_t *haystack, size_t haystack_len,
                            const uint8_t *needle, size_t needle_len,
                            const uint8_t *mask)
{
    if (!haystack || !needle || needle_len == 0 || haystack_len < needle_len) {
        return -1;
    }
    
    /* Simple search for short patterns or with mask */
    if (needle_len < 4 || mask) {
        for (size_t i = 0; i <= haystack_len - needle_len; i++) {
            bool match = true;
            for (size_t j = 0; j < needle_len; j++) {
                uint8_t m = mask ? mask[j] : 0xFF;
                if ((haystack[i + j] & m) != (needle[j] & m)) {
                    match = false;
                    break;
                }
            }
            if (match) return (int64_t)i;
        }
        return -1;
    }
    
    /* Build skip table */
    size_t skip[256];
    for (int i = 0; i < 256; i++) {
        skip[i] = needle_len;
    }
    for (size_t i = 0; i < needle_len - 1; i++) {
        skip[needle[i]] = needle_len - 1 - i;
    }
    
    /* Search */
    size_t i = 0;
    while (i <= haystack_len - needle_len) {
        size_t j = needle_len - 1;
        while (haystack[i + j] == needle[j]) {
            if (j == 0) return (int64_t)i;
            j--;
        }
        i += skip[haystack[i + needle_len - 1]];
    }
    
    return -1;
}

/**
 * @brief Case-insensitive pattern search
 */
static int64_t find_pattern_nocase(const uint8_t *haystack, size_t haystack_len,
                                   const uint8_t *needle, size_t needle_len)
{
    if (!haystack || !needle || needle_len == 0 || haystack_len < needle_len) {
        return -1;
    }
    
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        bool match = true;
        for (size_t j = 0; j < needle_len; j++) {
            if (tolower(haystack[i + j]) != tolower(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return (int64_t)i;
    }
    
    return -1;
}

/*===========================================================================
 * Result Management
 *===========================================================================*/

static uft_pcprot_result_t *result_create(void)
{
    uft_pcprot_result_t *result = calloc(1, sizeof(uft_pcprot_result_t));
    if (!result) return NULL;
    
    result->hit_capacity = 32;
    result->hits = calloc(result->hit_capacity, sizeof(uft_pcprot_hit_t));
    if (!result->hits) {
        free(result);
        return NULL;
    }
    
    return result;
}

static int result_add_hit(uft_pcprot_result_t *result, const uft_pcprot_hit_t *hit)
{
    if (!result || !hit) return -1;
    
    if (result->hit_count >= result->hit_capacity) {
        int new_cap = result->hit_capacity * 2;
        uft_pcprot_hit_t *new_hits = realloc(result->hits, 
                                              new_cap * sizeof(uft_pcprot_hit_t));
        if (!new_hits) return -1;
        result->hits = new_hits;
        result->hit_capacity = new_cap;
    }
    
    result->hits[result->hit_count++] = *hit;
    
    /* Update primary protection if higher confidence */
    if (hit->confidence > result->overall_confidence) {
        result->primary = hit->protection;
        result->primary_version = hit->version;
        result->overall_confidence = hit->confidence;
    }
    
    return 0;
}

void uft_pcprot_result_free(uft_pcprot_result_t *result)
{
    if (!result) return;
    free(result->hits);
    free(result);
}

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

int uft_pcprot_detect_safedisc(const uint8_t *data, size_t size, char *version)
{
    if (!data || size == 0) return 0;
    
    int confidence = 0;
    int ver_major = 0;
    
    /* Look for SafeDisc signatures */
    if (find_pattern_nocase(data, size, sig_safedisc_clcd32, 
                            sizeof(sig_safedisc_clcd32)) >= 0) {
        confidence += 40;
        ver_major = 1;
    }
    
    if (find_pattern_nocase(data, size, sig_safedisc_secdrv,
                            sizeof(sig_safedisc_secdrv)) >= 0) {
        confidence += 30;
        if (ver_major < 2) ver_major = 2;
    }
    
    if (find_pattern_nocase(data, size, sig_safedisc_dplayerx,
                            sizeof(sig_safedisc_dplayerx)) >= 0) {
        confidence += 25;
        if (ver_major < 3) ver_major = 3;
    }
    
    if (find_pattern(data, size, sig_safedisc_00015000,
                     sizeof(sig_safedisc_00015000), NULL) >= 0) {
        confidence += 20;
    }
    
    /* Look for BoG_ (Bink of Games) marker - SafeDisc 4 */
    const uint8_t bog[] = {'B', 'o', 'G', '_'};
    if (find_pattern(data, size, bog, 4, NULL) >= 0) {
        confidence += 15;
        ver_major = 4;
    }
    
    if (confidence > 100) confidence = 100;
    
    if (version && confidence > 0) {
        snprintf(version, 64, "%d.x", ver_major);
    }
    
    return confidence;
}

int uft_pcprot_detect_securom(const uint8_t *data, size_t size, char *version)
{
    if (!data || size == 0) return 0;
    
    int confidence = 0;
    int ver_major = 0;
    
    /* CMS16.DLL - SecuROM 1-3 */
    if (find_pattern_nocase(data, size, sig_securom_cms16,
                            sizeof(sig_securom_cms16)) >= 0) {
        confidence += 40;
        ver_major = 3;
    }
    
    /* CMS32_NT.DLL - SecuROM 3+ */
    if (find_pattern_nocase(data, size, sig_securom_cms32,
                            sizeof(sig_securom_cms32)) >= 0) {
        confidence += 35;
        if (ver_major < 3) ver_major = 3;
    }
    
    /* SECDRVNT.SYS - SecuROM 4+ */
    if (find_pattern_nocase(data, size, sig_securom_secdrvnt,
                            sizeof(sig_securom_secdrvnt)) >= 0) {
        confidence += 30;
        if (ver_major < 4) ver_major = 4;
    }
    
    /* PA URL - SecuROM PA */
    if (find_pattern(data, size, sig_securom_pa,
                     sizeof(sig_securom_pa), NULL) >= 0) {
        confidence += 25;
        ver_major = 7;  /* PA is typically 7.x */
    }
    
    /* SecuROM string */
    if (find_pattern(data, size, sig_securom_drm,
                     sizeof(sig_securom_drm), NULL) >= 0) {
        confidence += 15;
    }
    
    /* Look for .securom section */
    const uint8_t sec[] = {'.', 's', 'e', 'c', 'u', 'r', 'o', 'm'};
    if (find_pattern(data, size, sec, 8, NULL) >= 0) {
        confidence += 20;
        if (ver_major < 7) ver_major = 7;
    }
    
    if (confidence > 100) confidence = 100;
    
    if (version && confidence > 0) {
        snprintf(version, 64, "%d.x", ver_major);
    }
    
    return confidence;
}

int uft_pcprot_detect_starforce(const uint8_t *data, size_t size, char *version)
{
    if (!data || size == 0) return 0;
    
    int confidence = 0;
    int ver_major = 1;
    
    /* protect.dll - all versions */
    if (find_pattern_nocase(data, size, sig_starforce_protect,
                            sizeof(sig_starforce_protect)) >= 0) {
        confidence += 40;
    }
    
    /* sfdrv01.sys - version 2+ */
    if (find_pattern_nocase(data, size, sig_starforce_sfdrv01,
                            sizeof(sig_starforce_sfdrv01)) >= 0) {
        confidence += 30;
        ver_major = 2;
    }
    
    /* sfhlp01.sys - version 3 */
    if (find_pattern_nocase(data, size, sig_starforce_sfhlp01,
                            sizeof(sig_starforce_sfhlp01)) >= 0) {
        confidence += 25;
        ver_major = 3;
    }
    
    /* sfvfs02.sys - Pro */
    if (find_pattern_nocase(data, size, sig_starforce_sfvfs,
                            sizeof(sig_starforce_sfvfs)) >= 0) {
        confidence += 20;
        ver_major = 3;
    }
    
    /* Look for StarForce string */
    const uint8_t sf[] = {'S', 't', 'a', 'r', 'F', 'o', 'r', 'c', 'e'};
    if (find_pattern(data, size, sf, 9, NULL) >= 0) {
        confidence += 15;
    }
    
    if (confidence > 100) confidence = 100;
    
    if (version && confidence > 0) {
        snprintf(version, 64, "%d.x", ver_major);
    }
    
    return confidence;
}

int uft_pcprot_detect_laserlock(const uint8_t *data, size_t size, char *version)
{
    if (!data || size == 0) return 0;
    
    int confidence = 0;
    bool xtreme = false;
    
    if (find_pattern_nocase(data, size, sig_laserlock_laserlok,
                            sizeof(sig_laserlock_laserlok)) >= 0) {
        confidence += 50;
    }
    
    if (find_pattern_nocase(data, size, sig_laserlock_llock,
                            sizeof(sig_laserlock_llock)) >= 0) {
        confidence += 40;
        xtreme = true;
    }
    
    const uint8_t ll[] = {'L', 'a', 's', 'e', 'r', 'L', 'o', 'c', 'k'};
    if (find_pattern(data, size, ll, 9, NULL) >= 0) {
        confidence += 20;
    }
    
    if (confidence > 100) confidence = 100;
    
    if (version && confidence > 0) {
        snprintf(version, 64, "%s", xtreme ? "Xtreme" : "Standard");
    }
    
    return confidence;
}

int uft_pcprot_detect_cdcops(const uint8_t *data, size_t size, char *version)
{
    if (!data || size == 0) return 0;
    
    int confidence = 0;
    
    if (find_pattern_nocase(data, size, sig_cdcops_icd,
                            sizeof(sig_cdcops_icd)) >= 0) {
        confidence += 50;
    }
    
    if (find_pattern_nocase(data, size, sig_cdcops_cd32,
                            sizeof(sig_cdcops_cd32)) >= 0) {
        confidence += 40;
    }
    
    if (confidence > 100) confidence = 100;
    
    if (version) version[0] = '\0';
    
    return confidence;
}

int uft_pcprot_detect_tages(const uint8_t *data, size_t size, char *version)
{
    if (!data || size == 0) return 0;
    
    int confidence = 0;
    
    if (find_pattern_nocase(data, size, sig_tages_wave,
                            sizeof(sig_tages_wave)) >= 0) {
        confidence += 50;
    }
    
    const uint8_t tages[] = {'T', 'A', 'G', 'E', 'S'};
    if (find_pattern(data, size, tages, 5, NULL) >= 0) {
        confidence += 30;
    }
    
    if (confidence > 100) confidence = 100;
    
    if (version) version[0] = '\0';
    
    return confidence;
}

/*===========================================================================
 * Scanner Implementation
 *===========================================================================*/

uft_pcprot_result_t *uft_pcprot_scan_buffer(const uint8_t *data, size_t size,
                                             const char *filename)
{
    if (!data || size == 0) return NULL;
    
    uft_pcprot_result_t *result = result_create();
    if (!result) return NULL;
    
    char version[64] = {0};
    int conf;
    
    /* Try each detector */
    conf = uft_pcprot_detect_safedisc(data, size, version);
    if (conf > 30) {
        uft_pcprot_hit_t hit = {
            .protection = conf > 70 ? UFT_PCPROT_SAFEDISC_3 : 
                         conf > 50 ? UFT_PCPROT_SAFEDISC_2 : UFT_PCPROT_SAFEDISC_1,
            .name = "SafeDisc",
            .version = strdup(version),
            .confidence = conf,
            .component = UFT_PCPROT_COMP_EXE,
            .file_path = filename ? strdup(filename) : NULL,
        };
        result_add_hit(result, &hit);
    }
    
    conf = uft_pcprot_detect_securom(data, size, version);
    if (conf > 30) {
        uft_pcprot_hit_t hit = {
            .protection = conf > 70 ? UFT_PCPROT_SECUROM_7 :
                         conf > 50 ? UFT_PCPROT_SECUROM_4 : UFT_PCPROT_SECUROM_3,
            .name = "SecuROM",
            .version = strdup(version),
            .confidence = conf,
            .component = UFT_PCPROT_COMP_EXE,
            .file_path = filename ? strdup(filename) : NULL,
        };
        result_add_hit(result, &hit);
    }
    
    conf = uft_pcprot_detect_starforce(data, size, version);
    if (conf > 30) {
        uft_pcprot_hit_t hit = {
            .protection = conf > 60 ? UFT_PCPROT_STARFORCE_3 : UFT_PCPROT_STARFORCE_2,
            .name = "StarForce",
            .version = strdup(version),
            .confidence = conf,
            .component = UFT_PCPROT_COMP_EXE,
            .file_path = filename ? strdup(filename) : NULL,
        };
        result_add_hit(result, &hit);
    }
    
    conf = uft_pcprot_detect_laserlock(data, size, version);
    if (conf > 30) {
        uft_pcprot_hit_t hit = {
            .protection = UFT_PCPROT_LASERLOCK,
            .name = "LaserLock",
            .version = strdup(version),
            .confidence = conf,
            .component = UFT_PCPROT_COMP_EXE,
            .file_path = filename ? strdup(filename) : NULL,
        };
        result_add_hit(result, &hit);
    }
    
    conf = uft_pcprot_detect_cdcops(data, size, version);
    if (conf > 30) {
        uft_pcprot_hit_t hit = {
            .protection = UFT_PCPROT_CDCOPS,
            .name = "CD-Cops",
            .confidence = conf,
            .component = UFT_PCPROT_COMP_EXE,
            .file_path = filename ? strdup(filename) : NULL,
        };
        result_add_hit(result, &hit);
    }
    
    conf = uft_pcprot_detect_tages(data, size, version);
    if (conf > 30) {
        uft_pcprot_hit_t hit = {
            .protection = UFT_PCPROT_TAGES,
            .name = "TAGES",
            .confidence = conf,
            .component = UFT_PCPROT_COMP_EXE,
            .file_path = filename ? strdup(filename) : NULL,
        };
        result_add_hit(result, &hit);
    }
    
    /* Scan for all built-in signatures */
    for (size_t i = 0; i < BUILTIN_SIG_COUNT; i++) {
        const uft_pcprot_sig_t *sig = &builtin_signatures[i];
        
        int64_t pos = find_pattern(data, size, sig->pattern, sig->pattern_len, sig->mask);
        if (pos >= 0) {
            /* Check if we already have this protection */
            bool found = false;
            for (int j = 0; j < result->hit_count; j++) {
                if (result->hits[j].protection == sig->protection) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                uft_pcprot_hit_t hit = {
                    .protection = sig->protection,
                    .name = sig->name,
                    .version = sig->version,
                    .confidence = 70,
                    .component = UFT_PCPROT_COMP_EXE,
                    .file_path = filename ? strdup(filename) : NULL,
                    .offset = (size_t)pos,
                    .sig_name = sig->name,
                };
                result_add_hit(result, &hit);
            }
        }
    }
    
    return result;
}

uft_pcprot_result_t *uft_pcprot_scan_file(const char *path)
{
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size <= 0 || size > 100 * 1024 * 1024) {  /* Max 100MB */
        fclose(f);
        return NULL;
    }
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    size_t read = fread(data, 1, size, f);
    fclose(f);
    
    if (read != (size_t)size) {
        free(data);
        return NULL;
    }
    
    uft_pcprot_result_t *result = uft_pcprot_scan_buffer(data, size, path);
    free(data);
    
    return result;
}

/*===========================================================================
 * String Functions
 *===========================================================================*/

const char *uft_pcprot_name(uft_pcprot_type_t type)
{
    if (type >= UFT_PCPROT_COUNT) return "Unknown";
    return protection_names[type] ? protection_names[type] : "Unknown";
}

const char *uft_pcprot_vendor(uft_pcprot_type_t type)
{
    if (type >= UFT_PCPROT_COUNT) return "Unknown";
    return vendor_names[type] ? vendor_names[type] : "Unknown";
}

const char *uft_pcprot_description(uft_pcprot_type_t type)
{
    switch (type) {
        case UFT_PCPROT_SAFEDISC_1:
        case UFT_PCPROT_SAFEDISC_2:
        case UFT_PCPROT_SAFEDISC_3:
        case UFT_PCPROT_SAFEDISC_4:
            return "Macrovision SafeDisc - CD/DVD copy protection using weak sectors";
        case UFT_PCPROT_SECUROM_1:
        case UFT_PCPROT_SECUROM_2:
        case UFT_PCPROT_SECUROM_3:
        case UFT_PCPROT_SECUROM_4:
        case UFT_PCPROT_SECUROM_5:
        case UFT_PCPROT_SECUROM_7:
        case UFT_PCPROT_SECUROM_PA:
            return "Sony DADC SecuROM - CD/DVD protection with online activation";
        case UFT_PCPROT_STARFORCE_1:
        case UFT_PCPROT_STARFORCE_2:
        case UFT_PCPROT_STARFORCE_3:
        case UFT_PCPROT_STARFORCE_PRO:
            return "StarForce - Driver-based protection with hardware fingerprinting";
        case UFT_PCPROT_LASERLOCK:
        case UFT_PCPROT_LASERLOCK_XTREME:
            return "LaserLock - Uses intentional read errors for authentication";
        case UFT_PCPROT_CDCOPS:
            return "CD-Cops - Disc fingerprinting protection";
        case UFT_PCPROT_TAGES:
            return "TAGES - French protection using encrypted executables";
        default:
            return "Unknown protection scheme";
    }
}

bool uft_pcprot_can_preserve(uft_pcprot_type_t type)
{
    switch (type) {
        case UFT_PCPROT_SAFEDISC_1:
        case UFT_PCPROT_SAFEDISC_2:
        case UFT_PCPROT_LASERLOCK:
        case UFT_PCPROT_BAD_SECTORS:
        case UFT_PCPROT_WEAK_SECTORS:
        case UFT_PCPROT_SUBCODE:
            return true;  /* These can be preserved with proper imaging */
        case UFT_PCPROT_STARFORCE_3:
        case UFT_PCPROT_STARFORCE_PRO:
        case UFT_PCPROT_SECUROM_PA:
            return false; /* Online activation, cannot preserve */
        default:
            return true;
    }
}

/*===========================================================================
 * Signature Database API
 *===========================================================================*/

int uft_pcprot_sig_count(void)
{
    return (int)BUILTIN_SIG_COUNT;
}

const uft_pcprot_sig_t *uft_pcprot_sig_get(int index)
{
    if (index < 0 || index >= (int)BUILTIN_SIG_COUNT) return NULL;
    return &builtin_signatures[index];
}

/*===========================================================================
 * Output Functions
 *===========================================================================*/

void uft_pcprot_print_result(const uft_pcprot_result_t *result)
{
    if (!result) {
        printf("No result\n");
        return;
    }
    
    printf("PC Protection Scan Result\n");
    printf("=========================\n");
    printf("Primary: %s", uft_pcprot_name(result->primary));
    if (result->primary_version) {
        printf(" %s", result->primary_version);
    }
    printf(" (confidence: %d%%)\n", result->overall_confidence);
    printf("Total hits: %d\n\n", result->hit_count);
    
    for (int i = 0; i < result->hit_count; i++) {
        const uft_pcprot_hit_t *hit = &result->hits[i];
        printf("  [%d] %s", i + 1, hit->name ? hit->name : "Unknown");
        if (hit->version) printf(" %s", hit->version);
        printf(" (%d%%)\n", hit->confidence);
        if (hit->file_path) {
            printf("      File: %s\n", hit->file_path);
        }
    }
}

char *uft_pcprot_result_to_json(const uft_pcprot_result_t *result)
{
    if (!result) return NULL;
    
    size_t size = 4096 + result->hit_count * 512;
    char *json = malloc(size);
    if (!json) return NULL;
    
    int pos = snprintf(json, size,
        "{\n"
        "  \"primary\": \"%s\",\n"
        "  \"primary_version\": \"%s\",\n"
        "  \"confidence\": %d,\n"
        "  \"hits\": [\n",
        uft_pcprot_name(result->primary),
        result->primary_version ? result->primary_version : "",
        result->overall_confidence);
    
    for (int i = 0; i < result->hit_count; i++) {
        const uft_pcprot_hit_t *hit = &result->hits[i];
        pos += snprintf(json + pos, size - pos,
            "    {\n"
            "      \"protection\": \"%s\",\n"
            "      \"version\": \"%s\",\n"
            "      \"confidence\": %d,\n"
            "      \"file\": \"%s\"\n"
            "    }%s\n",
            hit->name ? hit->name : "",
            hit->version ? hit->version : "",
            hit->confidence,
            hit->file_path ? hit->file_path : "",
            i < result->hit_count - 1 ? "," : "");
    }
    
    pos += snprintf(json + pos, size - pos, "  ]\n}\n");
    
    return json;
}

/*===========================================================================
 * Self-Test
 *===========================================================================*/

#ifdef UFT_PCPROT_TEST

int main(void)
{
    printf("UFT PC Protection Suite Self-Test\n");
    printf("==================================\n\n");
    
    /* Test signature count */
    printf("Built-in signatures: %d\n", uft_pcprot_sig_count());
    
    /* Test detection with dummy data */
    uint8_t test_data[1024];
    memset(test_data, 0, sizeof(test_data));
    
    /* Embed SafeDisc signature */
    memcpy(test_data + 100, sig_safedisc_secdrv, sizeof(sig_safedisc_secdrv));
    
    char version[64];
    int conf = uft_pcprot_detect_safedisc(test_data, sizeof(test_data), version);
    printf("\nSafeDisc detection test:\n");
    printf("  Confidence: %d%%\n", conf);
    printf("  Version: %s\n", version);
    printf("  Test: %s\n", conf >= 30 ? "PASS" : "FAIL");
    
    /* Test buffer scan */
    memset(test_data, 0, sizeof(test_data));
    memcpy(test_data + 200, sig_securom_cms32, sizeof(sig_securom_cms32));
    
    uft_pcprot_result_t *result = uft_pcprot_scan_buffer(test_data, sizeof(test_data), "test.exe");
    printf("\nBuffer scan test:\n");
    if (result) {
        printf("  Hits: %d\n", result->hit_count);
        printf("  Primary: %s\n", uft_pcprot_name(result->primary));
        printf("  Test: %s\n", result->hit_count > 0 ? "PASS" : "FAIL");
        
        /* Test JSON export */
        char *json = uft_pcprot_result_to_json(result);
        if (json) {
            printf("\nJSON export: PASS (%zu bytes)\n", strlen(json));
            free(json);
        }
        
        uft_pcprot_result_free(result);
    } else {
        printf("  Test: FAIL (no result)\n");
    }
    
    /* Test name functions */
    printf("\nName tests:\n");
    printf("  SafeDisc 3: %s\n", uft_pcprot_name(UFT_PCPROT_SAFEDISC_3));
    printf("  SecuROM vendor: %s\n", uft_pcprot_vendor(UFT_PCPROT_SECUROM_4));
    printf("  StarForce preservable: %s\n", 
           uft_pcprot_can_preserve(UFT_PCPROT_STARFORCE_3) ? "yes" : "no");
    
    printf("\nSelf-test complete.\n");
    return 0;
}

#endif /* UFT_PCPROT_TEST */
