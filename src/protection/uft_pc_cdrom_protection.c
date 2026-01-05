/**
 * @file uft_pc_cdrom_protection.c
 * @brief PC CD-ROM Copy Protection Detection Implementation
 * 
 * Detection for PC CD-ROM copy protection schemes:
 * - SafeDisc
 * - SecuROM
 * - StarForce
 * - CD-Cops
 * - LaserLock
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/protection/uft_pc_cdrom_protection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * SafeDisc Detection
 *===========================================================================*/

/** SafeDisc signature files */
static const char* safedisc_files[] = {
    "00000001.TMP",
    "CLCD16.DLL",
    "CLCD32.DLL",
    "CLOKSPL.EXE",
    "DPLAYERX.DLL",
    "DRVMGT.DLL",
    "SECDRV.SYS",
    NULL
};

/** SafeDisc EXE signature */
static const uint8_t safedisc_exe_sig[] = {
    0x42, 0x6F, 0x47, 0x20,  /* "BoG " */
    0x5F, 0x5E, 0x5F          /* "_^_" */
};

/**
 * @brief Check for SafeDisc protection
 */
bool uft_safedisc_detect(const uint8_t *exe_data, size_t exe_len,
                          uft_safedisc_info_t *info)
{
    if (!exe_data || !info) {
        return false;
    }
    
    memset(info, 0, sizeof(*info));
    
    /* Search for SafeDisc signature in executable */
    for (size_t i = 0; i + sizeof(safedisc_exe_sig) <= exe_len; i++) {
        if (memcmp(exe_data + i, safedisc_exe_sig, sizeof(safedisc_exe_sig)) == 0) {
            info->detected = true;
            info->signature_offset = i;
            
            /* Try to determine version from nearby data */
            /* SafeDisc versions: 1.x, 2.x, 3.x, 4.x */
            info->version_major = 1;  /* Default to v1 */
            info->version_minor = 0;
            
            return true;
        }
    }
    
    /* Alternative: Check for stxt section */
    /* PE files have SafeDisc encrypted code in .stxt section */
    const uint8_t stxt_sig[] = { '.', 's', 't', 'x', 't' };
    for (size_t i = 0; i + sizeof(stxt_sig) <= exe_len; i++) {
        if (memcmp(exe_data + i, stxt_sig, sizeof(stxt_sig)) == 0) {
            info->detected = true;
            info->has_stxt_section = true;
            return true;
        }
    }
    
    return false;
}

/*===========================================================================
 * SecuROM Detection
 *===========================================================================*/

/** SecuROM signature patterns */
static const uint8_t securom_sig_v4[] = {
    0xCA, 0xDD, 0xDD, 0xAC  /* SecuROM v4+ signature */
};

static const uint8_t securom_sig_v7[] = {
    0x53, 0x65, 0x63, 0x75, 0x52, 0x4F, 0x4D  /* "SecuROM" */
};

/**
 * @brief Check for SecuROM protection
 */
bool uft_securom_detect(const uint8_t *exe_data, size_t exe_len,
                         uft_securom_info_t *info)
{
    if (!exe_data || !info) {
        return false;
    }
    
    memset(info, 0, sizeof(*info));
    
    /* Check for SecuROM v7+ signature */
    for (size_t i = 0; i + sizeof(securom_sig_v7) <= exe_len; i++) {
        if (memcmp(exe_data + i, securom_sig_v7, sizeof(securom_sig_v7)) == 0) {
            info->detected = true;
            info->version_major = 7;
            info->signature_offset = i;
            return true;
        }
    }
    
    /* Check for SecuROM v4+ signature */
    for (size_t i = 0; i + sizeof(securom_sig_v4) <= exe_len; i++) {
        if (memcmp(exe_data + i, securom_sig_v4, sizeof(securom_sig_v4)) == 0) {
            info->detected = true;
            info->version_major = 4;
            info->signature_offset = i;
            return true;
        }
    }
    
    /* Check for CMS16/CMS32.DLL imports */
    const uint8_t cms_sig[] = { 'C', 'M', 'S' };
    for (size_t i = 0; i + 10 <= exe_len; i++) {
        if (memcmp(exe_data + i, cms_sig, 3) == 0) {
            if (exe_data[i+3] == '1' && exe_data[i+4] == '6') {
                info->detected = true;
                info->version_major = 1;
                return true;
            }
        }
    }
    
    return false;
}

/*===========================================================================
 * Generic CD-ROM Protection Detection
 *===========================================================================*/

/**
 * @brief Initialize result structure
 */
void uft_cdrom_prot_init(uft_cdrom_prot_result_t *result)
{
    if (!result) return;
    memset(result, 0, sizeof(*result));
    result->type = UFT_CDROM_PROT_NONE;
}

/**
 * @brief Detect any CD-ROM protection
 */
int uft_cdrom_prot_detect(const uint8_t *exe_data, size_t exe_len,
                           uft_cdrom_prot_result_t *result)
{
    if (!exe_data || !result) {
        return -1;
    }
    
    uft_cdrom_prot_init(result);
    
    /* Try SafeDisc */
    uft_safedisc_info_t sd_info;
    if (uft_safedisc_detect(exe_data, exe_len, &sd_info)) {
        result->detected = true;
        result->type = UFT_CDROM_PROT_SAFEDISC;
        result->version_major = sd_info.version_major;
        result->version_minor = sd_info.version_minor;
        snprintf(result->name, sizeof(result->name),
                 "SafeDisc v%d.%d", sd_info.version_major, sd_info.version_minor);
        return 0;
    }
    
    /* Try SecuROM */
    uft_securom_info_t sr_info;
    if (uft_securom_detect(exe_data, exe_len, &sr_info)) {
        result->detected = true;
        result->type = UFT_CDROM_PROT_SECUROM;
        result->version_major = sr_info.version_major;
        result->version_minor = sr_info.version_minor;
        snprintf(result->name, sizeof(result->name),
                 "SecuROM v%d.%d", sr_info.version_major, sr_info.version_minor);
        return 0;
    }
    
    /* TODO: Add StarForce, CD-Cops, LaserLock detection */
    
    return 1;  /* No protection detected */
}

/**
 * @brief Get protection type name
 */
const char* uft_cdrom_prot_type_name(uft_cdrom_prot_type_t type)
{
    switch (type) {
        case UFT_CDROM_PROT_NONE:      return "None";
        case UFT_CDROM_PROT_SAFEDISC:  return "SafeDisc";
        case UFT_CDROM_PROT_SECUROM:   return "SecuROM";
        case UFT_CDROM_PROT_STARFORCE: return "StarForce";
        case UFT_CDROM_PROT_CDCOPS:    return "CD-Cops";
        case UFT_CDROM_PROT_LASERLOCK: return "LaserLock";
        case UFT_CDROM_PROT_TAGES:     return "TAGES";
        case UFT_CDROM_PROT_SOLIDSHIELD: return "SolidShield";
        default:                        return "Unknown";
    }
}

/**
 * @brief Print detection result
 */
void uft_cdrom_prot_print(FILE *out, const uft_cdrom_prot_result_t *result)
{
    if (!out || !result) return;
    
    fprintf(out, "=== CD-ROM Protection Detection ===\n");
    fprintf(out, "Detected:   %s\n", result->detected ? "YES" : "NO");
    
    if (!result->detected) return;
    
    fprintf(out, "Type:       %s\n", uft_cdrom_prot_type_name(result->type));
    fprintf(out, "Name:       %s\n", result->name);
    fprintf(out, "Version:    %d.%d\n", result->version_major, result->version_minor);
}
