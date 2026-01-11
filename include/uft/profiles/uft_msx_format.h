/**
 * @file uft_msx_format.h
 * @brief MSX DSK format profile - MSX computer disk images
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * MSX DSK files are raw sector dumps of MSX-DOS/MSX-BASIC disks.
 * They use FAT12 filesystem with MSX-specific boot sector values.
 * Common formats are 360KB (1DD) and 720KB (2DD).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_MSX_FORMAT_H
#define UFT_MSX_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * MSX Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief MSX sector size (always 512 bytes) */
#define UFT_MSX_SECTOR_SIZE         512

/** @brief Standard MSX disk sizes */
#define UFT_MSX_SIZE_1DD            368640      /**< 360KB 1DD */
#define UFT_MSX_SIZE_2DD            737280      /**< 720KB 2DD */

/** @brief MSX media descriptor bytes */
#define UFT_MSX_MEDIA_1DD_SS        0xF8        /**< 1DD single-sided */
#define UFT_MSX_MEDIA_1DD_DS        0xF9        /**< 1DD double-sided */
#define UFT_MSX_MEDIA_2DD_SS        0xFA        /**< 2DD single-sided */
#define UFT_MSX_MEDIA_2DD_DS        0xFB        /**< 2DD double-sided (rare) */
#define UFT_MSX_MEDIA_2DD           0xF9        /**< 720KB 2DD */

/** @brief Boot sector signature */
#define UFT_MSX_BOOT_SIG_55         0x55
#define UFT_MSX_BOOT_SIG_AA         0xAA

/* ═══════════════════════════════════════════════════════════════════════════
 * MSX Disk Types
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_MSX_TYPE_UNKNOWN    = 0,
    UFT_MSX_TYPE_1DD_SS     = 1,    /**< 180KB single-sided */
    UFT_MSX_TYPE_1DD_DS     = 2,    /**< 360KB double-sided */
    UFT_MSX_TYPE_2DD_SS     = 3,    /**< 360KB single-sided */
    UFT_MSX_TYPE_2DD_DS     = 4     /**< 720KB double-sided */
} uft_msx_disk_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * MSX Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief MSX boot sector (first 512 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t jump[3];                /**< Jump instruction */
    char oem_name[8];               /**< OEM name */
    uint16_t bytes_per_sector;      /**< Bytes per sector (512) */
    uint8_t sectors_per_cluster;    /**< Sectors per cluster */
    uint16_t reserved_sectors;      /**< Reserved sector count */
    uint8_t fat_count;              /**< Number of FATs */
    uint16_t root_entries;          /**< Root directory entries */
    uint16_t total_sectors;         /**< Total sectors (if < 65536) */
    uint8_t media_descriptor;       /**< Media descriptor byte */
    uint16_t sectors_per_fat;       /**< Sectors per FAT */
    uint16_t sectors_per_track;     /**< Sectors per track */
    uint16_t head_count;            /**< Number of heads */
    uint32_t hidden_sectors;        /**< Hidden sectors */
    uint32_t total_sectors_32;      /**< Total sectors (if >= 65536) */
    /* ... boot code follows ... */
} uft_msx_boot_sector_t;
#pragma pack(pop)

/**
 * @brief MSX disk geometry
 */
typedef struct {
    const char* name;
    uint8_t media_byte;
    uint8_t tracks;
    uint8_t heads;
    uint8_t sectors;
    uint32_t total_size;
} uft_msx_geometry_t;

/**
 * @brief Parsed MSX disk information
 */
typedef struct {
    uft_msx_disk_type_t disk_type;
    uint8_t tracks;
    uint8_t heads;
    uint8_t sectors_per_track;
    uint16_t bytes_per_sector;
    uint32_t total_size;
    uint8_t media_descriptor;
    char oem_name[9];               /**< Null-terminated */
    bool has_boot_signature;
} uft_msx_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Geometries
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_msx_geometry_t UFT_MSX_GEOMETRIES[] = {
    { "1DD SS (180KB)", UFT_MSX_MEDIA_1DD_SS, 40, 1, 9, 184320 },
    { "1DD DS (360KB)", UFT_MSX_MEDIA_1DD_DS, 40, 2, 9, 368640 },
    { "2DD SS (360KB)", UFT_MSX_MEDIA_2DD_SS, 80, 1, 9, 368640 },
    { "2DD DS (720KB)", UFT_MSX_MEDIA_2DD,    80, 2, 9, 737280 },
    { NULL, 0, 0, 0, 0, 0 }
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline const uft_msx_geometry_t* uft_msx_find_geometry(uint8_t media, size_t size) {
    for (int i = 0; UFT_MSX_GEOMETRIES[i].name != NULL; i++) {
        if (UFT_MSX_GEOMETRIES[i].media_byte == media ||
            UFT_MSX_GEOMETRIES[i].total_size == size) {
            return &UFT_MSX_GEOMETRIES[i];
        }
    }
    return NULL;
}

static inline bool uft_msx_validate(const uint8_t* data, size_t size) {
    if (!data || size < UFT_MSX_SECTOR_SIZE) return false;
    
    /* Check for valid media descriptor */
    uint8_t media = data[21];  /* Offset of media descriptor */
    return (media >= 0xF8 && media <= 0xFF);
}

static inline int uft_msx_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_MSX_SECTOR_SIZE) return 0;
    
    int score = 0;
    
    /* Check size */
    if (size == UFT_MSX_SIZE_1DD || size == UFT_MSX_SIZE_2DD ||
        size == 184320 || size == 368640) {
        score += 25;
    }
    
    /* Check media descriptor */
    uint8_t media = data[21];
    if (media >= 0xF8 && media <= 0xFB) {
        score += 25;
    }
    
    /* Check bytes per sector */
    uint16_t bps = data[11] | (data[12] << 8);
    if (bps == 512) {
        score += 15;
    }
    
    /* Check boot signature */
    if (size >= 512 && data[510] == UFT_MSX_BOOT_SIG_55 && data[511] == UFT_MSX_BOOT_SIG_AA) {
        score += 20;
    }
    
    /* Check sectors per track (9 is common for MSX) */
    uint16_t spt = data[24] | (data[25] << 8);
    if (spt == 9 || spt == 8) {
        score += 10;
    }
    
    return (score > 100) ? 100 : score;
}

static inline bool uft_msx_parse(const uint8_t* data, size_t size, uft_msx_info_t* info) {
    if (!data || size < UFT_MSX_SECTOR_SIZE || !info) return false;
    
    memset(info, 0, sizeof(*info));
    
    const uft_msx_boot_sector_t* boot = (const uft_msx_boot_sector_t*)data;
    
    info->bytes_per_sector = boot->bytes_per_sector;
    info->sectors_per_track = boot->sectors_per_track;
    info->heads = boot->head_count;
    info->media_descriptor = boot->media_descriptor;
    info->total_size = size;
    
    /* Copy OEM name */
    memcpy(info->oem_name, boot->oem_name, 8);
    info->oem_name[8] = '\0';
    
    /* Check boot signature */
    info->has_boot_signature = (data[510] == UFT_MSX_BOOT_SIG_55 && 
                                data[511] == UFT_MSX_BOOT_SIG_AA);
    
    /* Calculate tracks */
    uint32_t total_sectors = boot->total_sectors ? boot->total_sectors : boot->total_sectors_32;
    if (info->sectors_per_track > 0 && info->heads > 0) {
        info->tracks = total_sectors / (info->sectors_per_track * info->heads);
    }
    
    /* Determine disk type */
    if (info->tracks <= 40) {
        info->disk_type = (info->heads == 1) ? UFT_MSX_TYPE_1DD_SS : UFT_MSX_TYPE_1DD_DS;
    } else {
        info->disk_type = (info->heads == 1) ? UFT_MSX_TYPE_2DD_SS : UFT_MSX_TYPE_2DD_DS;
    }
    
    return true;
}

static inline const char* uft_msx_type_name(uft_msx_disk_type_t type) {
    switch (type) {
        case UFT_MSX_TYPE_1DD_SS: return "1DD Single-Sided (180KB)";
        case UFT_MSX_TYPE_1DD_DS: return "1DD Double-Sided (360KB)";
        case UFT_MSX_TYPE_2DD_SS: return "2DD Single-Sided (360KB)";
        case UFT_MSX_TYPE_2DD_DS: return "2DD Double-Sided (720KB)";
        default: return "Unknown";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_MSX_FORMAT_H */
