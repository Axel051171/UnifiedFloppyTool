/**
 * @file uft_geos_protection.c
 * @brief GEOS Copy Protection Detection and Analysis
 * @version 4.1.1
 * 
 * GEOS (Graphic Environment Operating System) used several
 * copy protection methods:
 * 
 * 1. Track 1 Sector 0 Signature
 *    - Special boot sector with GEOS signature
 *    
 * 2. Track 18 Directory Modifications
 *    - Modified BAM entries
 *    - Special directory structure
 *    
 * 3. Sector Interleave Verification
 *    - Non-standard sector interleave
 *    
 * 4. Half-Track Protection
 *    - Data written between tracks
 *    
 * 5. V1 Disk Protection (Original GEOS)
 *    - Key disk verification
 *    - Serial number check
 *    
 * 6. V2 Disk Protection (GEOS 2.0+)
 *    - Enhanced verification
 *    - Hardware fingerprinting
 *
 * Reference: GEOS Inside and Out, GEOS Programmer's Reference
 */

#include "uft/protection/uft_geos_protection.h"
#include "uft/core/uft_unified_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * GEOS Format Constants
 * ============================================================================ */

/* GEOS file structure */
#define GEOS_HEADER_SIZE        256
#define GEOS_ICON_WIDTH         24
#define GEOS_ICON_HEIGHT        21

/* GEOS file types */
#define GEOS_TYPE_NON_GEOS      0
#define GEOS_TYPE_BASIC         1
#define GEOS_TYPE_ASSEMBLER     2
#define GEOS_TYPE_DATA          3
#define GEOS_TYPE_SYSTEM        4
#define GEOS_TYPE_DESK_ACC      5
#define GEOS_TYPE_APPLICATION   6
#define GEOS_TYPE_PRINTER       7
#define GEOS_TYPE_INPUT         8
#define GEOS_TYPE_DISK          9
#define GEOS_TYPE_BOOT          10
#define GEOS_TYPE_TEMP          11
#define GEOS_TYPE_AUTO_EXEC     12
#define GEOS_TYPE_DIRECTORY     13
#define GEOS_TYPE_FONT          14
#define GEOS_TYPE_DOCUMENT      15

/* GEOS structure types */
#define GEOS_STRUCT_SEQ         0
#define GEOS_STRUCT_VLIR        1

/* GEOS signature locations */
#define GEOS_BOOT_TRACK         1
#define GEOS_BOOT_SECTOR        0
#define GEOS_DIR_TRACK          18
#define GEOS_DIR_SECTOR         1

/* GEOS boot signatures */
static const uint8_t GEOS_BOOT_SIG[] = {
    0x47, 0x45, 0x4F, 0x53    /* "GEOS" */
};

static const uint8_t GEOS_BOOT_EXTENDED[] = {
    0x47, 0x45, 0x4F, 0x53, 0x20, 0x66, 0x6F, 0x72,
    0x6D, 0x61, 0x74                                  /* "GEOS format" */
};

/* ============================================================================
 * Protection Types
 * ============================================================================ */

static const geos_protection_info_t g_geos_protections[] = {
    {
        .type = GEOS_PROT_V1_KEY_DISK,
        .name = "GEOS V1 Key Disk",
        .description = "Original GEOS key disk protection",
        .severity = GEOS_SEV_STANDARD,
        .copyable_with_nibbler = true,
        .requires_original = true
    },
    {
        .type = GEOS_PROT_V2_ENHANCED,
        .name = "GEOS V2 Enhanced",
        .description = "GEOS 2.0+ enhanced protection",
        .severity = GEOS_SEV_STANDARD,
        .copyable_with_nibbler = true,
        .requires_original = true
    },
    {
        .type = GEOS_PROT_SERIAL_CHECK,
        .name = "Serial Number Check",
        .description = "Disk-specific serial number verification",
        .severity = GEOS_SEV_STANDARD,
        .copyable_with_nibbler = true,
        .requires_original = false
    },
    {
        .type = GEOS_PROT_HALF_TRACK,
        .name = "Half-Track Protection",
        .description = "Data written between standard tracks",
        .severity = GEOS_SEV_DIFFICULT,
        .copyable_with_nibbler = true,
        .requires_original = true
    },
    {
        .type = GEOS_PROT_BAM_SIGNATURE,
        .name = "BAM Signature",
        .description = "Modified BAM entries for verification",
        .severity = GEOS_SEV_TRIVIAL,
        .copyable_with_nibbler = false,
        .requires_original = false
    },
    {
        .type = GEOS_PROT_INTERLEAVE,
        .name = "Non-Standard Interleave",
        .description = "Custom sector interleave pattern",
        .severity = GEOS_SEV_TRIVIAL,
        .copyable_with_nibbler = false,
        .requires_original = false
    },
    {
        .type = GEOS_PROT_SYNC_MARK,
        .name = "Custom Sync Marks",
        .description = "Modified GCR sync patterns",
        .severity = GEOS_SEV_DIFFICULT,
        .copyable_with_nibbler = true,
        .requires_original = true
    },
    {
        .type = GEOS_PROT_NONE,
        .name = "No Protection",
        .description = "Standard GEOS disk without protection",
        .severity = GEOS_SEV_NONE,
        .copyable_with_nibbler = false,
        .requires_original = false
    }
};

/* ============================================================================
 * Detection Functions
 * ============================================================================ */

bool uft_geos_detect_boot_signature(const uint8_t *sector_data, size_t size) {
    if (!sector_data || size < 256) return false;
    
    /* Check for GEOS signature at various offsets */
    for (size_t i = 0; i <= size - sizeof(GEOS_BOOT_SIG); i++) {
        if (memcmp(sector_data + i, GEOS_BOOT_SIG, sizeof(GEOS_BOOT_SIG)) == 0) {
            return true;
        }
    }
    
    return false;
}

bool uft_geos_detect_extended_signature(const uint8_t *sector_data, size_t size) {
    if (!sector_data || size < 256) return false;
    
    for (size_t i = 0; i <= size - sizeof(GEOS_BOOT_EXTENDED); i++) {
        if (memcmp(sector_data + i, GEOS_BOOT_EXTENDED, 
                   sizeof(GEOS_BOOT_EXTENDED)) == 0) {
            return true;
        }
    }
    
    return false;
}

int uft_geos_detect_file_type(const uint8_t *info_sector) {
    if (!info_sector) return GEOS_TYPE_NON_GEOS;
    
    /* GEOS info sector structure:
     * Offset 0x00: Info block ID ($00)
     * Offset 0x01: Icon bitmap (63 bytes)
     * Offset 0x40: File type
     * Offset 0x41: GEOS file type
     * Offset 0x42: Structure type (SEQ/VLIR)
     */
    
    if (info_sector[0] != 0x00) {
        return GEOS_TYPE_NON_GEOS;
    }
    
    return info_sector[0x41];
}

/* ============================================================================
 * Main Detection Function
 * ============================================================================ */

int uft_geos_analyze_disk(const uft_disk_image_t *disk, 
                          geos_analysis_result_t *result) {
    if (!disk || !result) return UFT_ERR_INVALID_PARAM;
    
    memset(result, 0, sizeof(*result));
    result->is_geos_disk = false;
    result->protection_count = 0;
    
    /* Get boot sector (Track 1, Sector 0) */
    uft_track_t *boot_track = NULL;
    if (disk->track_count > 0) {
        boot_track = disk->track_data[0];
    }
    
    if (!boot_track || boot_track->sector_count == 0) {
        return UFT_OK;  /* Not a valid disk */
    }
    
    /* Check boot sector for GEOS signature */
    uft_sector_t *boot_sector = &boot_track->sectors[0];
    if (boot_sector->data && boot_sector->data_len >= 256) {
        if (uft_geos_detect_boot_signature(boot_sector->data, 
                                           boot_sector->data_len)) {
            result->is_geos_disk = true;
            
            /* Detect extended signature */
            if (uft_geos_detect_extended_signature(boot_sector->data,
                                                   boot_sector->data_len)) {
                result->geos_version = 2;
            } else {
                result->geos_version = 1;
            }
        }
    }
    
    if (!result->is_geos_disk) {
        return UFT_OK;
    }
    
    /* Analyze for specific protections */
    
    /* Check for V1 key disk protection */
    if (result->geos_version == 1) {
        /* V1 protection typically on track 36 */
        if (disk->track_count > 35) {
            uft_track_t *prot_track = disk->track_data[35];
            if (prot_track && prot_track->sector_count > 0) {
                result->protections[result->protection_count++] = 
                    GEOS_PROT_V1_KEY_DISK;
            }
        }
    }
    
    /* Check for V2 enhanced protection */
    if (result->geos_version >= 2) {
        result->protections[result->protection_count++] = 
            GEOS_PROT_V2_ENHANCED;
    }
    
    /* Check BAM for modifications */
    if (disk->track_count >= 18) {
        uft_track_t *dir_track = disk->track_data[17];  /* Track 18 */
        if (dir_track && dir_track->sector_count > 0) {
            uft_sector_t *bam_sector = &dir_track->sectors[0];
            if (bam_sector->data) {
                /* Check for non-standard BAM entries */
                /* GEOS often marks certain sectors as used in BAM */
                bool bam_modified = false;
                
                /* Check track 18 BAM entry for directory markers */
                if (bam_sector->data_len >= 144) {
                    uint8_t track18_bam = bam_sector->data[4 + 18*4];
                    if (track18_bam != 0x11) {  /* Standard would be 17 free */
                        bam_modified = true;
                    }
                }
                
                if (bam_modified) {
                    result->protections[result->protection_count++] = 
                        GEOS_PROT_BAM_SIGNATURE;
                }
            }
        }
    }
    
    /* Check for non-standard interleave */
    /* Would need raw GCR data to detect this properly */
    
    /* Check for half-track data */
    /* Would need flux-level data to detect this properly */
    
    return UFT_OK;
}

/* ============================================================================
 * Information Functions
 * ============================================================================ */

const geos_protection_info_t* uft_geos_get_protection_info(
    geos_protection_type_t type) {
    
    for (size_t i = 0; i < sizeof(g_geos_protections) / sizeof(g_geos_protections[0]); i++) {
        if (g_geos_protections[i].type == type) {
            return &g_geos_protections[i];
        }
    }
    
    return NULL;
}

int uft_geos_get_report(const geos_analysis_result_t *result,
                        char *buffer, size_t buffer_size) {
    if (!result || !buffer) return UFT_ERR_INVALID_PARAM;
    
    size_t offset = 0;
    
    offset += snprintf(buffer + offset, buffer_size - offset,
        "════════════════════════════════════════════════════════════════\n"
        "                    GEOS DISK ANALYSIS\n"
        "════════════════════════════════════════════════════════════════\n\n");
    
    if (!result->is_geos_disk) {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "This disk does not appear to be a GEOS disk.\n"
            "No GEOS boot signature was detected.\n");
        return (int)offset;
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset,
        "GEOS Disk Detected: YES\n"
        "GEOS Version:       %d.x\n"
        "Protections Found:  %d\n\n",
        result->geos_version, result->protection_count);
    
    if (result->protection_count == 0) {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "No copy protection detected.\n"
            "This disk can be copied with standard tools.\n\n");
    } else {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "Detected Protections:\n"
            "────────────────────────────────────────────────────────────────\n");
        
        for (size_t i = 0; i < result->protection_count && offset < buffer_size; i++) {
            const geos_protection_info_t *info = 
                uft_geos_get_protection_info(result->protections[i]);
            
            if (info) {
                const char *severity_str;
                switch (info->severity) {
                    case GEOS_SEV_NONE:     severity_str = "None"; break;
                    case GEOS_SEV_TRIVIAL:  severity_str = "Trivial"; break;
                    case GEOS_SEV_STANDARD: severity_str = "Standard"; break;
                    case GEOS_SEV_DIFFICULT: severity_str = "Difficult"; break;
                    default: severity_str = "Unknown";
                }
                
                offset += snprintf(buffer + offset, buffer_size - offset,
                    "\n  [%d] %s\n"
                    "      Description: %s\n"
                    "      Severity:    %s\n"
                    "      Nibbler:     %s\n"
                    "      Original:    %s\n",
                    i + 1, info->name,
                    info->description,
                    severity_str,
                    info->copyable_with_nibbler ? "Can copy" : "Not needed",
                    info->requires_original ? "Required" : "Not required");
            }
        }
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset,
        "\n════════════════════════════════════════════════════════════════\n"
        "                    COPY RECOMMENDATIONS\n"
        "════════════════════════════════════════════════════════════════\n\n");
    
    if (result->protection_count == 0) {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "Standard D64 copy is sufficient.\n"
            "Use: uft read --device xum1541 --format d64\n");
    } else {
        bool needs_nibbler = false;
        bool needs_original = false;
        
        for (int i = 0; i < result->protection_count; i++) {
            const geos_protection_info_t *info = 
                uft_geos_get_protection_info(result->protections[i]);
            if (info) {
                if (info->copyable_with_nibbler) needs_nibbler = true;
                if (info->requires_original) needs_original = true;
            }
        }
        
        if (needs_nibbler) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                "Recommended: Use G64 format with nibbler for best results.\n"
                "Use: uft read --device xum1541 --format g64 --nibtools\n\n");
        }
        
        if (needs_original) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                "⚠️  Original disk may be required for full functionality.\n"
                "    Some protection checks may fail on copies.\n");
        }
    }
    
    return (int)offset;
}

/* ============================================================================
 * GEOS File Analysis
 * ============================================================================ */

int uft_geos_analyze_file(const uint8_t *info_sector, size_t size,
                          geos_file_info_t *info) {
    if (!info_sector || !info || size < 256) return UFT_ERR_INVALID_PARAM;
    
    memset(info, 0, sizeof(*info));
    
    /* Check if this is a GEOS file */
    if (info_sector[0] != 0x00) {
        info->is_geos_file = false;
        return UFT_OK;
    }
    
    info->is_geos_file = true;
    
    /* Parse info sector */
    /* Offset 0x01-0x3F: Icon bitmap (63 bytes) */
    memcpy(info->icon_data, &info_sector[0x01], 63);
    
    /* Offset 0x40: DOS file type */
    info->dos_file_type = info_sector[0x40];
    
    /* Offset 0x41: GEOS file type */
    info->geos_file_type = info_sector[0x41];
    
    /* Offset 0x42: Structure type */
    info->structure_type = info_sector[0x42];
    
    /* Offset 0x43-0x44: Load address */
    info->load_address = info_sector[0x43] | (info_sector[0x44] << 8);
    
    /* Offset 0x45-0x46: End address */
    info->end_address = info_sector[0x45] | (info_sector[0x46] << 8);
    
    /* Offset 0x47-0x48: Start address */
    info->start_address = info_sector[0x47] | (info_sector[0x48] << 8);
    
    /* Offset 0x49-0x5C: Class name (20 bytes) */
    memcpy(info->class_name, &info_sector[0x49], 20);
    info->class_name[20] = '\0';
    
    /* Offset 0x5D-0x74: Author (24 bytes) */
    memcpy(info->author, &info_sector[0x5D], 24);
    info->author[24] = '\0';
    
    /* Offset 0x75-0x88: Parent application (20 bytes) - for documents */
    memcpy(info->parent_app, &info_sector[0x75], 20);
    info->parent_app[20] = '\0';
    
    /* Offset 0x89-0xA8: Description (32 bytes) */
    memcpy(info->description, &info_sector[0x89], 32);
    info->description[32] = '\0';
    
    return UFT_OK;
}

const char* uft_geos_file_type_name(int type) {
    switch (type) {
        case GEOS_TYPE_NON_GEOS:    return "Non-GEOS";
        case GEOS_TYPE_BASIC:       return "BASIC";
        case GEOS_TYPE_ASSEMBLER:   return "Assembler";
        case GEOS_TYPE_DATA:        return "Data";
        case GEOS_TYPE_SYSTEM:      return "System";
        case GEOS_TYPE_DESK_ACC:    return "Desk Accessory";
        case GEOS_TYPE_APPLICATION: return "Application";
        case GEOS_TYPE_PRINTER:     return "Printer Driver";
        case GEOS_TYPE_INPUT:       return "Input Driver";
        case GEOS_TYPE_DISK:        return "Disk Driver";
        case GEOS_TYPE_BOOT:        return "Boot";
        case GEOS_TYPE_TEMP:        return "Temporary";
        case GEOS_TYPE_AUTO_EXEC:   return "Auto-Exec";
        case GEOS_TYPE_DIRECTORY:   return "Directory";
        case GEOS_TYPE_FONT:        return "Font";
        case GEOS_TYPE_DOCUMENT:    return "Document";
        default:                    return "Unknown";
    }
}
