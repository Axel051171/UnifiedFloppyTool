/**
 * @file uft_dos_handlers.c
 * @brief DOS Recognition and Handling for RIDE module
 * 
 * Implements detection and basic parsing of:
 * - MS-DOS FAT12/16/32
 * - TR-DOS (ZX Spectrum)
 * - Plus3DOS (Spectrum +3)
 * - AMSDOS (Amstrad CPC)
 * - CP/M 2.2/3.0
 * - Commodore 1541 DOS
 * - Apple DOS 3.3 / ProDOS
 * - Atari ST TOS
 * - BBC Micro DFS
 * - MDOS/GDOS (Didaktik)
 * 
 * @copyright Based on RIDE (c) Tomas Nestorovic
 * @license MIT
 */

#include "uft/ride/uft_dos_recognition.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/* FAT signatures */
#define FAT12_MAGIC         0xFFF8
#define FAT16_MAGIC         0xFFF8
#define FAT32_MAGIC         0x0FFFFFF8
#define FAT_BOOT_SIG        0xAA55

/* TR-DOS constants */
#define TRDOS_SECTOR_SIZE   256
#define TRDOS_DIR_TRACK     0
#define TRDOS_DIR_SECTOR    0
#define TRDOS_INFO_SECTOR   8
#define TRDOS_MAGIC_BYTE    0x10    /* At offset 0xE7 */

/* CP/M constants */
#define CPM_SECTOR_SIZE     128
#define CPM_DIR_ENTRY_SIZE  32
#define CPM_EMPTY_ENTRY     0xE5

/* Commodore 1541 */
#define D64_DIR_TRACK       18
#define D64_DIR_SECTOR      1
#define D64_BAM_TRACK       18
#define D64_BAM_SECTOR      0

/* Apple DOS */
#define APPLE_VTOC_TRACK    17
#define APPLE_VTOC_SECTOR   0
#define PRODOS_BLOCK_SIZE   512

/*============================================================================
 * PRIVATE HELPERS
 *============================================================================*/

/**
 * @brief Read 16-bit little-endian value
 */
static inline uint16_t read_le16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

/**
 * @brief Read 32-bit little-endian value
 */
static inline uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

/**
 * @brief Check if data looks like valid ASCII/printable
 */
static bool is_printable_string(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] != 0 && (data[i] < 0x20 || data[i] > 0x7E)) {
            return false;
        }
    }
    return true;
}

/*============================================================================
 * FAT12/16/32 DETECTION
 *============================================================================*/

/**
 * @brief Probe for FAT filesystem
 */
int uft_dos_probe_fat(const uint8_t *boot_sector, size_t size,
                       uft_dos_info_t *info) {
    if (size < 512 || !boot_sector || !info) return 0;
    
    /* Check boot signature */
    if (read_le16(boot_sector + 510) != FAT_BOOT_SIG) {
        return 0;
    }
    
    /* BPB (BIOS Parameter Block) */
    uint16_t bytes_per_sector = read_le16(boot_sector + 11);
    uint8_t  sectors_per_cluster = boot_sector[13];
    uint16_t reserved_sectors = read_le16(boot_sector + 14);
    uint8_t  num_fats = boot_sector[16];
    uint16_t root_entries = read_le16(boot_sector + 17);
    uint16_t total_sectors_16 = read_le16(boot_sector + 19);
    uint8_t  media_type = boot_sector[21];
    uint16_t fat_size_16 = read_le16(boot_sector + 22);
    uint32_t total_sectors_32 = read_le32(boot_sector + 32);
    
    /* Validate BPB */
    if (bytes_per_sector < 512 || bytes_per_sector > 4096) return 0;
    if (sectors_per_cluster == 0 || (sectors_per_cluster & (sectors_per_cluster - 1))) return 0;
    if (num_fats == 0 || num_fats > 2) return 0;
    if (media_type < 0xF0) return 0;
    
    /* Calculate total sectors */
    uint32_t total_sectors = total_sectors_16 ? total_sectors_16 : total_sectors_32;
    if (total_sectors == 0) return 0;
    
    /* Calculate cluster count to determine FAT type */
    uint32_t root_dir_sectors = ((root_entries * 32) + (bytes_per_sector - 1)) / bytes_per_sector;
    uint32_t fat_size = fat_size_16 ? fat_size_16 : read_le32(boot_sector + 36);
    uint32_t data_sectors = total_sectors - (reserved_sectors + (num_fats * fat_size) + root_dir_sectors);
    uint32_t cluster_count = data_sectors / sectors_per_cluster;
    
    /* Determine FAT type */
    memset(info, 0, sizeof(*info));
    
    if (cluster_count < 4085) {
        info->dos_type = UFT_DOS_FAT12_S;
        strncpy(info->dos_name, "FAT12", sizeof(info->dos_name) - 1);
    } else if (cluster_count < 65525) {
        info->dos_type = UFT_DOS_FAT16_S;
        strncpy(info->dos_name, "FAT16", sizeof(info->dos_name) - 1);
    } else {
        info->dos_type = UFT_DOS_FAT32_S;
        strncpy(info->dos_name, "FAT32", sizeof(info->dos_name) - 1);
    }
    
    info->confidence = 95;
    info->sector_size = bytes_per_sector;
    info->cluster_size = sectors_per_cluster * bytes_per_sector;
    info->total_sectors = total_sectors;
    info->root_entries = root_entries;
    
    /* Copy volume label if present */
    if (info->dos_type == UFT_DOS_FAT32_S) {
        memcpy(info->volume_label, boot_sector + 71, 11);
    } else {
        memcpy(info->volume_label, boot_sector + 43, 11);
    }
    
    /* Trim volume label */
    for (int i = 10; i >= 0 && info->volume_label[i] == ' '; i--) {
        info->volume_label[i] = '\0';
    }
    
    return 1;
}

/*============================================================================
 * TR-DOS DETECTION (ZX Spectrum)
 *============================================================================*/

/**
 * @brief Probe for TR-DOS filesystem
 */
int uft_dos_probe_trdos(const uint8_t *sector0, const uint8_t *sector8,
                         size_t size, uft_dos_info_t *info) {
    if (!sector8 || size < 256 || !info) return 0;
    
    memset(info, 0, sizeof(*info));
    
    /* Check info sector (track 0, sector 8) */
    /* Byte 0xE1: Disk type (0x16=DS80, 0x17=DS40, 0x18=SS80, 0x19=SS40) */
    uint8_t disk_type = sector8[0xE1];
    if (disk_type < 0x16 || disk_type > 0x19) {
        /* Could be empty/unformatted */
        if (sector8[0xE7] != TRDOS_MAGIC_BYTE) {
            return 0;
        }
    }
    
    /* Check magic byte at 0xE7 */
    if (sector8[0xE7] != TRDOS_MAGIC_BYTE) {
        return 0;
    }
    
    /* Valid TR-DOS */
    info->dos_type = UFT_DOS_TRDOS;
    info->confidence = 90;
    info->sector_size = TRDOS_SECTOR_SIZE;
    
    /* Decode disk type */
    switch (disk_type) {
        case 0x16: 
            strncpy(info->dos_name, "TR-DOS DS/80", sizeof(info->dos_name) - 1);
            info->total_sectors = 2 * 80 * 16;
            break;
        case 0x17:
            strncpy(info->dos_name, "TR-DOS DS/40", sizeof(info->dos_name) - 1);
            info->total_sectors = 2 * 40 * 16;
            break;
        case 0x18:
            strncpy(info->dos_name, "TR-DOS SS/80", sizeof(info->dos_name) - 1);
            info->total_sectors = 80 * 16;
            break;
        case 0x19:
            strncpy(info->dos_name, "TR-DOS SS/40", sizeof(info->dos_name) - 1);
            info->total_sectors = 40 * 16;
            break;
        default:
            strncpy(info->dos_name, "TR-DOS", sizeof(info->dos_name) - 1);
            break;
    }
    
    /* File count and free sectors */
    info->file_count = sector8[0xE4];
    info->free_sectors = read_le16(sector8 + 0xE5);
    
    /* Disk label (bytes 0xF5-0xFC) */
    memcpy(info->volume_label, sector8 + 0xF5, 8);
    
    return 1;
}

/*============================================================================
 * CP/M DETECTION
 *============================================================================*/

/**
 * @brief Probe for CP/M filesystem
 */
int uft_dos_probe_cpm(const uint8_t *directory, size_t size,
                       uft_dos_info_t *info) {
    if (!directory || size < 128 || !info) return 0;
    
    memset(info, 0, sizeof(*info));
    
    /* Check directory entries */
    int valid_entries = 0;
    int empty_entries = 0;
    int total_entries = size / CPM_DIR_ENTRY_SIZE;
    
    for (int i = 0; i < total_entries && i < 64; i++) {
        const uint8_t *entry = directory + i * CPM_DIR_ENTRY_SIZE;
        uint8_t user = entry[0];
        
        if (user == CPM_EMPTY_ENTRY) {
            empty_entries++;
            continue;
        }
        
        if (user > 15) {
            /* Invalid user number (unless special) */
            continue;
        }
        
        /* Check filename (bytes 1-8) - should be printable or space */
        bool valid_name = true;
        for (int j = 1; j <= 8; j++) {
            uint8_t c = entry[j] & 0x7F;  /* Mask off attribute bits */
            if (c != ' ' && (c < 0x21 || c > 0x7E)) {
                valid_name = false;
                break;
            }
        }
        
        if (valid_name) {
            valid_entries++;
        }
    }
    
    /* Need at least some valid entries */
    if (valid_entries < 1 && empty_entries < total_entries / 2) {
        return 0;
    }
    
    /* Looks like CP/M */
    info->dos_type = UFT_DOS_CPM;
    strncpy(info->dos_name, "CP/M 2.2", sizeof(info->dos_name) - 1);
    info->confidence = 70 + (valid_entries * 2);
    if (info->confidence > 95) info->confidence = 95;
    info->sector_size = CPM_SECTOR_SIZE;
    info->file_count = valid_entries;
    
    return 1;
}

/*============================================================================
 * COMMODORE 1541 DOS DETECTION
 *============================================================================*/

/**
 * @brief Probe for Commodore 1541 DOS
 */
int uft_dos_probe_cbm(const uint8_t *bam, size_t size,
                       uft_dos_info_t *info) {
    if (!bam || size < 256 || !info) return 0;
    
    memset(info, 0, sizeof(*info));
    
    /* Check BAM at track 18, sector 0 */
    /* Byte 0: Track of first directory sector (should be 18) */
    /* Byte 1: Sector of first directory sector (should be 1) */
    /* Byte 2: DOS version ('A') */
    
    if (bam[0] != D64_DIR_TRACK) return 0;
    if (bam[1] != D64_DIR_SECTOR) return 0;
    
    uint8_t dos_version = bam[2];
    if (dos_version != 0x41 && dos_version != 0x32) {
        /* Not standard DOS version */
        return 0;
    }
    
    /* Valid CBM DOS */
    info->dos_type = UFT_DOS_CBM;
    snprintf(info->dos_name, sizeof(info->dos_name), "CBM DOS %c", dos_version);
    info->confidence = 90;
    info->sector_size = 256;
    
    /* Disk name at offset 144 (16 bytes, PETSCII) */
    for (int i = 0; i < 16 && bam[144 + i] != 0xA0; i++) {
        info->volume_label[i] = bam[144 + i];
    }
    
    /* Disk ID at offset 162 (2 bytes) */
    info->volume_label[17] = bam[162];
    info->volume_label[18] = bam[163];
    
    return 1;
}

/*============================================================================
 * APPLE DOS / PRODOS DETECTION
 *============================================================================*/

/**
 * @brief Probe for Apple DOS 3.3
 */
int uft_dos_probe_apple_dos(const uint8_t *vtoc, size_t size,
                             uft_dos_info_t *info) {
    if (!vtoc || size < 256 || !info) return 0;
    
    memset(info, 0, sizeof(*info));
    
    /* VTOC at Track 17, Sector 0 */
    /* Byte 0x01: Track of first catalog sector */
    /* Byte 0x02: Sector of first catalog sector */
    /* Byte 0x03: DOS version (usually 3) */
    /* Byte 0x27: Volume number */
    
    uint8_t catalog_track = vtoc[0x01];
    uint8_t catalog_sector = vtoc[0x02];
    uint8_t dos_version = vtoc[0x03];
    uint8_t volume = vtoc[0x06];
    
    /* Validate */
    if (catalog_track > 35) return 0;
    if (catalog_sector > 15) return 0;
    if (dos_version != 3 && dos_version != 2) return 0;
    
    info->dos_type = UFT_DOS_APPLE_DOS;
    snprintf(info->dos_name, sizeof(info->dos_name), "Apple DOS 3.%d", dos_version);
    info->confidence = 85;
    info->sector_size = 256;
    snprintf(info->volume_label, sizeof(info->volume_label), "VOLUME %d", volume);
    
    return 1;
}

/**
 * @brief Probe for ProDOS
 */
int uft_dos_probe_prodos(const uint8_t *block2, size_t size,
                          uft_dos_info_t *info) {
    if (!block2 || size < 512 || !info) return 0;
    
    memset(info, 0, sizeof(*info));
    
    /* ProDOS Volume Directory Header at Block 2 */
    /* Byte 0x00: Storage type/name length (0xF for volume header) */
    /* Byte 0x04-0x0F: Volume name */
    
    uint8_t storage_type = (block2[0x04] >> 4) & 0x0F;
    uint8_t name_length = block2[0x04] & 0x0F;
    
    if (storage_type != 0x0F) return 0;  /* Not a volume header */
    if (name_length == 0 || name_length > 15) return 0;
    
    info->dos_type = UFT_DOS_PRODOS_S;
    strncpy(info->dos_name, "ProDOS", sizeof(info->dos_name) - 1);
    info->confidence = 90;
    info->sector_size = PRODOS_BLOCK_SIZE;
    
    /* Copy volume name */
    memcpy(info->volume_label, block2 + 0x05, name_length);
    info->volume_label[name_length] = '\0';
    
    return 1;
}

/*============================================================================
 * AMSTRAD AMSDOS DETECTION
 *============================================================================*/

/**
 * @brief Probe for AMSDOS
 */
int uft_dos_probe_amsdos(const uint8_t *directory, size_t size,
                          uft_dos_info_t *info) {
    if (!directory || size < 128 || !info) return 0;
    
    /* AMSDOS is essentially CP/M compatible */
    /* Check for AMSDOS-specific features */
    
    /* Try CP/M probe first */
    if (!uft_dos_probe_cpm(directory, size, info)) {
        return 0;
    }
    
    /* Override as AMSDOS */
    info->dos_type = UFT_DOS_AMSDOS_S;
    strncpy(info->dos_name, "AMSDOS", sizeof(info->dos_name) - 1);
    
    return 1;
}

/*============================================================================
 * ATARI ST TOS DETECTION
 *============================================================================*/

/**
 * @brief Probe for Atari ST TOS (FAT-based)
 */
int uft_dos_probe_atari_st(const uint8_t *boot_sector, size_t size,
                            uft_dos_info_t *info) {
    if (size < 512 || !boot_sector || !info) return 0;
    
    /* Atari ST uses FAT but with different boot sector */
    /* Check for OEM string or serial number */
    
    /* Try FAT probe first */
    if (!uft_dos_probe_fat(boot_sector, size, info)) {
        return 0;
    }
    
    /* Check for Atari-specific markers */
    /* Bytes 0-1: BRA.S instruction (0x60xx or 0xEB) */
    if (boot_sector[0] == 0x60 || boot_sector[0] == 0xEB) {
        /* Likely Atari or PC boot sector */
        
        /* Check serial number at offset 8 (Atari-specific) */
        uint32_t serial = read_le32(boot_sector + 8);
        if (serial != 0 && serial != 0xFFFFFFFF) {
            /* Has serial number, could be Atari */
            info->dos_type = UFT_DOS_ATARI_ST_S;
            strncpy(info->dos_name, "Atari ST TOS", sizeof(info->dos_name) - 1);
            return 1;
        }
    }
    
    /* Not Atari-specific, return generic FAT */
    return 1;
}

/*============================================================================
 * BBC MICRO DFS DETECTION
 *============================================================================*/

/**
 * @brief Probe for BBC Micro DFS
 */
int uft_dos_probe_dfs(const uint8_t *sector0, const uint8_t *sector1,
                       size_t size, uft_dos_info_t *info) {
    if (!sector0 || !sector1 || size < 256 || !info) return 0;
    
    memset(info, 0, sizeof(*info));
    
    /* DFS stores catalog in sectors 0 and 1 */
    /* Sector 0: File names (8 entries, 8 bytes each) */
    /* Sector 1: File attributes (8 entries, 8 bytes each) + disk info */
    
    /* Check disk title (sector 0, bytes 0-7 + sector 1, bytes 0-3) */
    /* Should be printable ASCII or spaces */
    
    for (int i = 0; i < 8; i++) {
        uint8_t c = sector0[i];
        if (c != 0 && c != ' ' && (c < 0x20 || c > 0x7E)) {
            return 0;  /* Invalid title character */
        }
    }
    
    /* Check sector count in sector 1 */
    uint8_t sector_count_lo = sector1[0x06];
    uint8_t sector_count_hi = sector1[0x07] & 0x03;
    uint16_t total_sectors = sector_count_lo | (sector_count_hi << 8);
    
    if (total_sectors == 0 || total_sectors > 800) {
        return 0;
    }
    
    /* Check boot option (should be 0-3) */
    uint8_t boot_opt = (sector1[0x06] >> 4) & 0x03;
    (void)boot_opt;  /* Valid range already guaranteed by mask */
    
    info->dos_type = UFT_DOS_DFS_S;
    strncpy(info->dos_name, "BBC DFS", sizeof(info->dos_name) - 1);
    info->confidence = 85;
    info->sector_size = 256;
    info->total_sectors = total_sectors;
    
    /* Copy disk title */
    memcpy(info->volume_label, sector0, 8);
    memcpy(info->volume_label + 8, sector1, 4);
    info->volume_label[12] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 11; i >= 0 && info->volume_label[i] == ' '; i--) {
        info->volume_label[i] = '\0';
    }
    
    return 1;
}

/*============================================================================
 * UNIFIED DETECTION API
 *============================================================================*/

/**
 * @brief Detect DOS type from boot/system sectors
 */
int uft_dos_detect(const uint8_t *data, size_t size,
                    uft_dos_info_t *results, int max_results) {
    if (!data || size < 256 || !results || max_results < 1) {
        return 0;
    }
    
    int found = 0;
    uft_dos_info_t temp;
    
    /* Try each DOS type in order of specificity */
    
    /* FAT (most common) */
    if (found < max_results && uft_dos_probe_fat(data, size, &temp)) {
        memcpy(&results[found++], &temp, sizeof(temp));
    }
    
    /* TR-DOS (need sector 8) */
    if (found < max_results && size >= 9 * 256) {
        if (uft_dos_probe_trdos(data, data + 8 * 256, 256, &temp)) {
            memcpy(&results[found++], &temp, sizeof(temp));
        }
    }
    
    /* CBM DOS (BAM at track 18) */
    if (found < max_results && uft_dos_probe_cbm(data, size, &temp)) {
        memcpy(&results[found++], &temp, sizeof(temp));
    }
    
    /* Apple DOS (VTOC) */
    if (found < max_results && uft_dos_probe_apple_dos(data, size, &temp)) {
        memcpy(&results[found++], &temp, sizeof(temp));
    }
    
    /* ProDOS */
    if (found < max_results && size >= 1024) {
        if (uft_dos_probe_prodos(data + 512, 512, &temp)) {
            memcpy(&results[found++], &temp, sizeof(temp));
        }
    }
    
    /* CP/M (directory check) */
    if (found < max_results && uft_dos_probe_cpm(data, size, &temp)) {
        memcpy(&results[found++], &temp, sizeof(temp));
    }
    
    /* DFS (sectors 0+1) */
    if (found < max_results && size >= 512) {
        if (uft_dos_probe_dfs(data, data + 256, 256, &temp)) {
            memcpy(&results[found++], &temp, sizeof(temp));
        }
    }
    
    /* Sort by confidence (descending) */
    for (int i = 0; i < found - 1; i++) {
        for (int j = i + 1; j < found; j++) {
            if (results[j].confidence > results[i].confidence) {
                uft_dos_info_t swap = results[i];
                results[i] = results[j];
                results[j] = swap;
            }
        }
    }
    
    return found;
}

/**
 * @brief Get DOS type name string
 */
const char *uft_dos_type_name(uft_dos_type_simple_t type) {
    switch (type) {
        case UFT_DOS_FAT12_S:    return "FAT12";
        case UFT_DOS_FAT16_S:    return "FAT16";
        case UFT_DOS_FAT32_S:    return "FAT32";
        case UFT_DOS_TRDOS:      return "TR-DOS";
        case UFT_DOS_PLUS3DOS_S: return "Plus3DOS";
        case UFT_DOS_MDOS_S:     return "MDOS";
        case UFT_DOS_GDOS_S:     return "GDOS";
        case UFT_DOS_AMSDOS_S:   return "AMSDOS";
        case UFT_DOS_CPM:        return "CP/M";
        case UFT_DOS_CBM:        return "CBM DOS";
        case UFT_DOS_APPLE_DOS:  return "Apple DOS 3.3";
        case UFT_DOS_PRODOS_S:   return "ProDOS";
        case UFT_DOS_ATARI_ST_S: return "Atari ST TOS";
        case UFT_DOS_DFS_S:      return "BBC DFS";
        default:                 return "Unknown";
    }
}
