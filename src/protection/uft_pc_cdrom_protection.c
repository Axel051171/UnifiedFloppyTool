/**
 * @file uft_pc_cdrom_protection.c
 * @brief PC CD-ROM Copy Protection Detection Implementation
 * 
 * @version 2.0.0
 * @date 2025-01-08
 */

#include "uft/protection/uft_pc_cdrom_protection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

static bool search_string(const uint8_t *data, size_t len,
                          const char *needle, size_t *offset) {
    size_t needle_len = strlen(needle);
    if (needle_len > len) return false;
    
    for (size_t i = 0; i <= len - needle_len; i++) {
        if (memcmp(data + i, needle, needle_len) == 0) {
            if (offset) *offset = i;
            return true;
        }
    }
    return false;
}

/*===========================================================================
 * Memory Management
 *===========================================================================*/

void uft_pc_config_init(uft_pc_detect_config_t *config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->detect_safedisc = true;
    config->detect_securom = true;
    config->detect_others = true;
    config->scan_executables = true;
    config->max_weak_sectors = UFT_WEAK_SECTOR_MAX;
}

uft_pc_prot_result_t *uft_pc_result_alloc(void) {
    uft_pc_prot_result_t *result = calloc(1, sizeof(uft_pc_prot_result_t));
    return result;
}

void uft_pc_result_free(uft_pc_prot_result_t *result) {
    if (!result) return;
    free(result->weak_sectors);
    free(result);
}

/*===========================================================================
 * SafeDisc Detection
 *===========================================================================*/

int uft_pc_detect_safedisc(const uint8_t *exe_data, size_t exe_len,
                           const char *filename, uft_safedisc_t *result) {
    if (!exe_data || !result) return -1;
    
    (void)filename;
    memset(result, 0, sizeof(*result));
    
    size_t offset;
    
    /* Check for SafeDisc v1 signature */
    if (search_string(exe_data, exe_len, UFT_SAFEDISC_SIG_V1, &offset)) {
        result->detected = true;
        result->major_version = 1;
        result->sig_offset = (uint32_t)offset;
        result->confidence = 0.9;
        return 0;
    }
    
    /* Check for SafeDisc v2+ signature */
    if (search_string(exe_data, exe_len, UFT_SAFEDISC_SIG_V2, &offset)) {
        result->detected = true;
        result->major_version = 2;
        result->sig_offset = (uint32_t)offset;
        result->confidence = 0.85;
        
        /* Check for CLCD.DLL marker */
        if (search_string(exe_data, exe_len, UFT_SAFEDISC_CLCD_SIG, NULL)) {
            result->has_clcd = true;
            result->major_version = 3;  /* CLCD indicates v3+ */
        }
        
        return 0;
    }
    
    return 1;  /* Not detected */
}

/*===========================================================================
 * SecuROM Detection
 *===========================================================================*/

int uft_pc_detect_securom(const uint8_t *exe_data, size_t exe_len,
                          const char *filename, uft_securom_t *result) {
    if (!exe_data || !result) return -1;
    
    (void)filename;
    memset(result, 0, sizeof(*result));
    
    size_t offset;
    
    /* Check for SecuROM v4+ signature */
    if (search_string(exe_data, exe_len, UFT_SECUROM_SIG_V4, &offset)) {
        result->detected = true;
        result->major_version = 4;
        result->sig_offset = (uint32_t)offset;
        result->confidence = 0.85;
        return 0;
    }
    
    /* Check for CMS signature */
    if (search_string(exe_data, exe_len, UFT_SECUROM_CMS_SIG, &offset)) {
        result->detected = true;
        result->major_version = 5;  /* CMS16 is typically v5+ */
        result->sig_offset = (uint32_t)offset;
        result->confidence = 0.8;
        return 0;
    }
    
    /* Check for .cms file reference */
    if (search_string(exe_data, exe_len, UFT_SECUROM_DAT_SIG, &offset)) {
        result->detected = true;
        result->major_version = 7;  /* .cms is typically v7+ */
        result->sig_offset = (uint32_t)offset;
        result->confidence = 0.75;
        return 0;
    }
    
    return 1;  /* Not detected */
}

/*===========================================================================
 * Weak Sector Analysis
 *===========================================================================*/

size_t uft_pc_analyze_weak_sectors(const uint8_t **sector_data,
                                   uint8_t read_count, uint32_t sector_count,
                                   uint32_t lba_start,
                                   uft_weak_sector_t *results,
                                   size_t max_results) {
    if (!sector_data || !results || read_count < 2) return 0;
    
    size_t weak_count = 0;
    
    for (uint32_t s = 0; s < sector_count && weak_count < max_results; s++) {
        /* Compare multiple reads of the same sector */
        bool is_weak = false;
        double variance = 0.0;
        
        for (uint8_t r = 1; r < read_count; r++) {
            const uint8_t *read1 = sector_data[s * read_count];
            const uint8_t *read2 = sector_data[s * read_count + r];
            
            if (!read1 || !read2) continue;
            
            /* Check for differences */
            int diff_count = 0;
            for (int i = 0; i < 2048; i++) {
                if (read1[i] != read2[i]) diff_count++;
            }
            
            if (diff_count > 0) {
                is_weak = true;
                variance += (double)diff_count / 2048.0;
            }
        }
        
        if (is_weak) {
            results[weak_count].lba = lba_start + s;
            results[weak_count].signal_variance = variance / (read_count - 1);
            results[weak_count].edc_mismatch = true;
            weak_count++;
        }
    }
    
    return weak_count;
}

/*===========================================================================
 * Version Detection
 *===========================================================================*/

int uft_pc_safedisc_version(const uint8_t *signature, size_t sig_len,
                            uint8_t *major, uint8_t *minor) {
    if (!signature || sig_len < 4 || !major || !minor) return -1;
    
    /* Simple version detection based on signature bytes */
    /* Real detection would analyze more patterns */
    
    if (memcmp(signature, "BoG_", 4) == 0) {
        *major = 1;
        *minor = 0;
    } else if (memcmp(signature, "~SD~", 4) == 0) {
        *major = 2;
        *minor = 0;
    } else {
        return -1;
    }
    
    return 0;
}

int uft_pc_securom_version(const uint8_t *signature, size_t sig_len,
                           uint8_t *major, uint8_t *minor) {
    if (!signature || sig_len < 5 || !major || !minor) return -1;
    
    if (memcmp(signature, "~@&@~", 5) == 0) {
        *major = 4;
        *minor = 0;
    } else if (memcmp(signature, "CMS16", 5) == 0) {
        *major = 5;
        *minor = 0;
    } else {
        return -1;
    }
    
    return 0;
}

/*===========================================================================
 * File Scanning
 *===========================================================================*/

size_t uft_pc_scan_files(const char **files, size_t file_count,
                         uft_pc_prot_result_t *result) {
    if (!files || !result) return 0;
    
    size_t found = 0;
    
    static const char *safedisc_files[] = {
        "CLCD32.DLL", "CLCD16.DLL", "CLOKSPL.EXE",
        "drvmgt.dll", "secdrv.sys", NULL
    };
    
    static const char *securom_files[] = {
        "CMS16.DLL", "CMS32_95.DLL", "CMS32_NT.DLL",
        ".cms", NULL
    };
    
    for (size_t i = 0; i < file_count && found < 8; i++) {
        /* Check SafeDisc files */
        for (int j = 0; safedisc_files[j]; j++) {
            if (strstr(files[i], safedisc_files[j])) {
                strncpy(result->detected_files[found], files[i], 63);
                found++;
                result->safedisc.detected = true;
                break;
            }
        }
        
        /* Check SecuROM files */
        for (int j = 0; securom_files[j]; j++) {
            if (strstr(files[i], securom_files[j])) {
                strncpy(result->detected_files[found], files[i], 63);
                found++;
                result->securom.detected = true;
                break;
            }
        }
    }
    
    result->file_count = (uint8_t)found;
    return found;
}

/*===========================================================================
 * Full Detection
 *===========================================================================*/

int uft_pc_detect_all(const uint8_t **exe_data, const size_t *exe_lens,
                      const char **filenames, size_t file_count,
                      const uint8_t **sector_data, uint8_t read_count,
                      uint32_t sector_count, uint32_t lba_start,
                      const uft_pc_detect_config_t *config,
                      uft_pc_prot_result_t *result) {
    if (!result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->primary_type = UFT_PC_PROT_NONE;
    
    uft_pc_detect_config_t default_config;
    if (!config) {
        uft_pc_config_init(&default_config);
        config = &default_config;
    }
    
    /* Scan executables */
    if (config->scan_executables && exe_data && filenames) {
        for (size_t i = 0; i < file_count; i++) {
            if (!exe_data[i] || !exe_lens) continue;
            
            if (config->detect_safedisc) {
                uft_pc_detect_safedisc(exe_data[i], exe_lens[i],
                                       filenames[i], &result->safedisc);
            }
            
            if (config->detect_securom) {
                uft_pc_detect_securom(exe_data[i], exe_lens[i],
                                      filenames[i], &result->securom);
            }
        }
    }
    
    /* Analyze weak sectors */
    if (config->analyze_weak_sectors && sector_data && read_count >= 2) {
        result->weak_sectors = calloc(config->max_weak_sectors,
                                       sizeof(uft_weak_sector_t));
        if (result->weak_sectors) {
            result->weak_sector_count = (uint32_t)uft_pc_analyze_weak_sectors(
                sector_data, read_count, sector_count, lba_start,
                result->weak_sectors, config->max_weak_sectors);
        }
    }
    
    /* Determine primary type */
    if (result->safedisc.detected && result->securom.detected) {
        result->primary_type = UFT_PC_PROT_MULTIPLE;
    } else if (result->safedisc.detected) {
        switch (result->safedisc.major_version) {
            case 1: result->primary_type = UFT_PC_PROT_SAFEDISC; break;
            case 2: result->primary_type = UFT_PC_PROT_SAFEDISC2; break;
            case 3: result->primary_type = UFT_PC_PROT_SAFEDISC3; break;
            case 4: result->primary_type = UFT_PC_PROT_SAFEDISC4; break;
            default: result->primary_type = UFT_PC_PROT_SAFEDISC; break;
        }
        result->overall_confidence = result->safedisc.confidence;
    } else if (result->securom.detected) {
        result->primary_type = (result->securom.major_version >= 7) ?
            UFT_PC_PROT_SECUROM_NEW : UFT_PC_PROT_SECUROM;
        result->overall_confidence = result->securom.confidence;
    }
    
    /* Generate description */
    if (result->primary_type != UFT_PC_PROT_NONE) {
        snprintf(result->description, sizeof(result->description),
                 "%s detected", uft_pc_prot_name(result->primary_type));
    }
    
    return (result->primary_type != UFT_PC_PROT_NONE) ? 0 : 1;
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

const char* uft_pc_prot_name(uft_pc_prot_type_t type) {
    switch (type) {
        case UFT_PC_PROT_NONE:       return "None";
        case UFT_PC_PROT_SAFEDISC:   return "SafeDisc v1";
        case UFT_PC_PROT_SAFEDISC2:  return "SafeDisc v2.x";
        case UFT_PC_PROT_SAFEDISC3:  return "SafeDisc v3.x";
        case UFT_PC_PROT_SAFEDISC4:  return "SafeDisc v4.x";
        case UFT_PC_PROT_SECUROM:    return "SecuROM";
        case UFT_PC_PROT_SECUROM_NEW:return "SecuROM New (v7+)";
        case UFT_PC_PROT_LASERLOCK:  return "LaserLock";
        case UFT_PC_PROT_PROTECTCD:  return "ProtectCD-VOB";
        case UFT_PC_PROT_STARFORCE:  return "StarForce";
        case UFT_PC_PROT_MULTIPLE:   return "Multiple Protections";
        default:                     return "Unknown";
    }
}

int uft_pc_result_to_json(const uft_pc_prot_result_t *result,
                          char *buffer, size_t buffer_size) {
    if (!result || !buffer || buffer_size < 256) return -1;
    
    int len = snprintf(buffer, buffer_size,
        "{\n"
        "  \"type\": \"%s\",\n"
        "  \"confidence\": %.2f,\n"
        "  \"safedisc\": { \"detected\": %s, \"version\": \"%d.%d\" },\n"
        "  \"securom\": { \"detected\": %s, \"version\": \"%d.%d\" },\n"
        "  \"weak_sectors\": %u,\n"
        "  \"description\": \"%s\"\n"
        "}",
        uft_pc_prot_name(result->primary_type),
        result->overall_confidence,
        result->safedisc.detected ? "true" : "false",
        result->safedisc.major_version, result->safedisc.minor_version,
        result->securom.detected ? "true" : "false",
        result->securom.major_version, result->securom.minor_version,
        result->weak_sector_count,
        result->description);
    
    return (len > 0 && (size_t)len < buffer_size) ? 0 : -1;
}
