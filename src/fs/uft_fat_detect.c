/**
 * @file uft_fat_detect.c
 * @brief P1-5: FAT Format Detection with Confidence Scoring
 * 
 * Detects FAT12/FAT16/FAT32 formatted disk images with confidence scores.
 * Integrates with UFT format detection system.
 * 
 * Key Features:
 * - BPB validation with detailed checks
 * - Confidence scoring (0-100)
 * - False positive prevention for D64/ADF/SCP
 * - Support for common floppy and HDD sizes
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/fs/fat_bpb.h"
#include "uft/fs/uft_fat_detect.h"
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint16_t rd_le16(const uint8_t *p) { 
    return (uint16_t)(p[0] | (p[1] << 8)); 
}

static uint32_t rd_le32(const uint8_t *p) { 
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | 
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); 
}

static int is_power_of_two(unsigned x) {
    return x && ((x & (x - 1)) == 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * False Positive Prevention
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if data looks like a D64 (Commodore) image
 * D64 has specific sizes and no 0x55AA signature at normal location
 */
static bool looks_like_d64(const uint8_t *data, size_t size)
{
    /* D64 known sizes */
    static const size_t d64_sizes[] = {
        174848, 175531, 196608, 197376, 205312, 206114
    };
    
    for (size_t i = 0; i < sizeof(d64_sizes)/sizeof(d64_sizes[0]); i++) {
        if (size == d64_sizes[i]) {
            /* D64 files don't have FAT BPB at sector 0 */
            /* Check if first bytes look like CBM directory */
            if (data[0] == 0x12 || data[0] == 0x00) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief Check if data looks like an ADF (Amiga) image
 */
static bool looks_like_adf(const uint8_t *data, size_t size)
{
    /* ADF sizes */
    if (size == 901120 || size == 1802240) {  /* DD / HD */
        /* Check for "DOS" magic at offset 0 */
        if (data[0] == 'D' && data[1] == 'O' && data[2] == 'S') {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if data looks like SCP (SuperCard Pro) flux
 */
static bool looks_like_scp(const uint8_t *data, size_t size)
{
    if (size >= 16) {
        if (data[0] == 'S' && data[1] == 'C' && data[2] == 'P') {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if data looks like HFE (HxC Floppy Emulator)
 */
static bool looks_like_hfe(const uint8_t *data, size_t size)
{
    if (size >= 8) {
        if (memcmp(data, "HXCPICFE", 8) == 0 || 
            memcmp(data, "HXCHFEV3", 8) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if data looks like G64 (GCR-1541)
 */
static bool looks_like_g64(const uint8_t *data, size_t size)
{
    if (size >= 8) {
        if (memcmp(data, "GCR-1541", 8) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if data looks like IPF (CAPS)
 */
static bool looks_like_ipf(const uint8_t *data, size_t size)
{
    if (size >= 4) {
        if (memcmp(data, "CAPS", 4) == 0) {
            return true;
        }
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * BPB Sanity Checks
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Validate BPB fields and compute confidence
 */
static int validate_bpb_fields(const uint8_t *data, size_t size, 
                               uft_fat_bpb_t *bpb, int *confidence)
{
    *confidence = 0;
    
    /* Need at least 512 bytes for boot sector */
    if (size < 512) return -1;
    
    /* Check boot signature (0x55AA at 510-511) */
    if (data[510] != 0x55 || data[511] != 0xAA) {
        return -1;  /* Not a FAT boot sector */
    }
    *confidence += 20;  /* Boot signature present */
    
    /* Parse BPB fields */
    uint16_t bytes_per_sector = rd_le16(data + 11);
    uint8_t sectors_per_cluster = data[13];
    uint16_t reserved_sectors = rd_le16(data + 14);
    uint8_t fat_count = data[16];
    uint16_t root_entries = rd_le16(data + 17);
    uint16_t total_sectors_16 = rd_le16(data + 19);
    uint8_t media_descriptor = data[21];
    uint16_t sectors_per_fat_16 = rd_le16(data + 22);
    uint16_t sectors_per_track = rd_le16(data + 24);
    uint16_t heads = rd_le16(data + 26);
    uint32_t hidden_sectors = rd_le32(data + 28);
    uint32_t total_sectors_32 = rd_le32(data + 32);
    
    /* Validate bytes per sector (must be 512, 1024, 2048, or 4096) */
    if (!is_power_of_two(bytes_per_sector) || 
        bytes_per_sector < 512 || bytes_per_sector > 4096) {
        return -1;
    }
    *confidence += 10;
    
    /* Validate sectors per cluster (power of 2, 1-128) */
    if (!is_power_of_two(sectors_per_cluster) || sectors_per_cluster == 0) {
        return -1;
    }
    *confidence += 10;
    
    /* Validate reserved sectors */
    if (reserved_sectors == 0) {
        return -1;
    }
    *confidence += 5;
    
    /* Validate FAT count (usually 2, sometimes 1) */
    if (fat_count == 0 || fat_count > 4) {
        return -1;
    }
    if (fat_count == 2) *confidence += 5;
    
    /* Validate media descriptor */
    /* Common values: 0xF0 (3.5" removable), 0xF8 (fixed), 0xF9-0xFF */
    if (media_descriptor < 0xF0) {
        *confidence -= 10;  /* Unusual, but not fatal */
    } else {
        *confidence += 5;
    }
    
    /* Determine total sectors */
    uint32_t total_sectors = total_sectors_16 ? total_sectors_16 : total_sectors_32;
    if (total_sectors < 16) {
        return -1;
    }
    
    /* Validate sectors per FAT */
    uint32_t sectors_per_fat = sectors_per_fat_16;
    if (sectors_per_fat == 0) {
        /* FAT32 */
        sectors_per_fat = rd_le32(data + 36);
    }
    if (sectors_per_fat == 0) {
        return -1;
    }
    *confidence += 5;
    
    /* Geometry checks (optional but increase confidence) */
    if (sectors_per_track > 0 && sectors_per_track <= 63) {
        *confidence += 5;
    }
    if (heads > 0 && heads <= 255) {
        *confidence += 5;
    }
    
    /* Check for common floppy sizes (high confidence) */
    size_t expected_size = (size_t)total_sectors * bytes_per_sector;
    static const size_t common_sizes[] = {
        163840,   /* 160K */
        184320,   /* 180K */
        327680,   /* 320K */
        368640,   /* 360K */
        737280,   /* 720K */
        1228800,  /* 1.2M */
        1474560,  /* 1.44M */
        2949120,  /* 2.88M */
        1720320,  /* 1.68M DMF */
        1763328,  /* 1.72M XDF */
    };
    
    for (size_t i = 0; i < sizeof(common_sizes)/sizeof(common_sizes[0]); i++) {
        if (size == common_sizes[i] || expected_size == common_sizes[i]) {
            *confidence += 15;
            break;
        }
    }
    
    /* Fill BPB structure */
    if (bpb) {
        bpb->bytes_per_sector = bytes_per_sector;
        bpb->sectors_per_cluster = sectors_per_cluster;
        bpb->reserved_sectors = reserved_sectors;
        bpb->fats = fat_count;
        bpb->root_entries = root_entries;
        bpb->total_sectors = total_sectors;
        bpb->sectors_per_fat = sectors_per_fat;
        bpb->sectors_per_track = sectors_per_track;
        bpb->heads = heads;
        bpb->hidden_sectors = hidden_sectors;
        
        /* Determine FAT type */
        uint32_t root_sectors = ((root_entries * 32) + (bytes_per_sector - 1)) / bytes_per_sector;
        uint32_t fat_sectors = fat_count * sectors_per_fat;
        uint32_t data_sectors = total_sectors - reserved_sectors - fat_sectors - root_sectors;
        uint32_t cluster_count = data_sectors / sectors_per_cluster;
        
        if (cluster_count < 4085) {
            bpb->fat_type = UFT_FAT12;
        } else if (cluster_count < 65525) {
            bpb->fat_type = UFT_FAT16;
        } else {
            bpb->fat_type = UFT_FAT32;
            bpb->root_cluster = rd_le32(data + 44);
        }
        
        bpb->confidence = *confidence;
    }
    
    /* Cap confidence at 100 */
    if (*confidence > 100) *confidence = 100;
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect FAT format from buffer with confidence scoring
 * 
 * @param data Image data (at least 512 bytes)
 * @param size Image size
 * @param result Detection result (output)
 * @return 0 on success (FAT detected), -1 on failure
 */
int uft_fat_detect(const uint8_t *data, size_t size, uft_fat_detect_result_t *result)
{
    if (!data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->is_fat = false;
    result->confidence = 0;
    
    /* Quick rejection of known non-FAT formats */
    if (looks_like_d64(data, size)) {
        snprintf(result->reason, sizeof(result->reason), "Looks like D64 (Commodore)");
        return -1;
    }
    if (looks_like_adf(data, size)) {
        snprintf(result->reason, sizeof(result->reason), "Looks like ADF (Amiga)");
        return -1;
    }
    if (looks_like_scp(data, size)) {
        snprintf(result->reason, sizeof(result->reason), "Looks like SCP (flux)");
        return -1;
    }
    if (looks_like_hfe(data, size)) {
        snprintf(result->reason, sizeof(result->reason), "Looks like HFE (flux)");
        return -1;
    }
    if (looks_like_g64(data, size)) {
        snprintf(result->reason, sizeof(result->reason), "Looks like G64 (GCR)");
        return -1;
    }
    if (looks_like_ipf(data, size)) {
        snprintf(result->reason, sizeof(result->reason), "Looks like IPF (CAPS)");
        return -1;
    }
    
    /* Validate BPB */
    int confidence = 0;
    if (validate_bpb_fields(data, size, &result->bpb, &confidence) != 0) {
        snprintf(result->reason, sizeof(result->reason), "Invalid BPB");
        return -1;
    }
    
    result->is_fat = true;
    result->confidence = confidence;
    result->fat_type = result->bpb.fat_type;
    
    /* Generate description */
    const char *type_name = "FAT";
    switch (result->fat_type) {
        case UFT_FAT12: type_name = "FAT12"; break;
        case UFT_FAT16: type_name = "FAT16"; break;
        case UFT_FAT32: type_name = "FAT32"; break;
        default: break;
    }
    
    snprintf(result->reason, sizeof(result->reason), 
             "%s, %u sectors, %u bytes/sector",
             type_name,
             result->bpb.total_sectors,
             result->bpb.bytes_per_sector);
    
    return 0;
}

/**
 * @brief Get FAT type name
 */
const char* uft_fat_type_name(uft_fat_type_t type)
{
    switch (type) {
        case UFT_FAT12: return "FAT12";
        case UFT_FAT16: return "FAT16";
        case UFT_FAT32: return "FAT32";
        default: return "Unknown";
    }
}

/**
 * @brief Check if size matches common FAT floppy format
 */
bool uft_fat_is_floppy_size(size_t size)
{
    static const size_t floppy_sizes[] = {
        163840,   /* 160K */
        184320,   /* 180K */
        327680,   /* 320K */
        368640,   /* 360K */
        737280,   /* 720K */
        1228800,  /* 1.2M */
        1474560,  /* 1.44M */
        2949120,  /* 2.88M */
        1720320,  /* 1.68M DMF */
        1763328,  /* 1.72M XDF */
    };
    
    for (size_t i = 0; i < sizeof(floppy_sizes)/sizeof(floppy_sizes[0]); i++) {
        if (size == floppy_sizes[i]) return true;
    }
    return false;
}
