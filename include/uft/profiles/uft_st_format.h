/**
 * @file uft_st_format.h
 * @brief ST format profile - Atari ST raw sector disk images
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * ST files are raw sector dumps of Atari ST disks without any header.
 * Format is detected by file size and FAT boot sector contents.
 * Common formats are 360KB (SS), 720KB (DS), and variants.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_ST_FORMAT_H
#define UFT_ST_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * ST Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief ST sector size (always 512 bytes) */
#define UFT_ST_SECTOR_SIZE          512

/** @brief Standard ST disk sizes */
#define UFT_ST_SIZE_360K            368640      /**< SS/DD 80×9 */
#define UFT_ST_SIZE_400K            409600      /**< SS/DD 80×10 */
#define UFT_ST_SIZE_720K            737280      /**< DS/DD 80×9 */
#define UFT_ST_SIZE_800K            819200      /**< DS/DD 80×10 */
#define UFT_ST_SIZE_880K            901120      /**< DS/DD 80×11 */
#define UFT_ST_SIZE_1440K           1474560     /**< DS/HD 80×18 */

/** @brief Sectors per track variants */
#define UFT_ST_SPT_9                9
#define UFT_ST_SPT_10               10
#define UFT_ST_SPT_11               11
#define UFT_ST_SPT_18               18

/** @brief Boot sector locations */
#define UFT_ST_BPS_OFFSET           11          /**< Bytes per sector */
#define UFT_ST_SPC_OFFSET           13          /**< Sectors per cluster */
#define UFT_ST_SPT_OFFSET           24          /**< Sectors per track */
#define UFT_ST_HEADS_OFFSET         26          /**< Number of heads */
#define UFT_ST_SERIAL_OFFSET        8           /**< Serial number */

/* ═══════════════════════════════════════════════════════════════════════════
 * ST Disk Types
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_ST_TYPE_UNKNOWN     = 0,
    UFT_ST_TYPE_SSDD_9      = 1,    /**< SS/DD 9 sectors (360KB) */
    UFT_ST_TYPE_SSDD_10     = 2,    /**< SS/DD 10 sectors (400KB) */
    UFT_ST_TYPE_DSDD_9      = 3,    /**< DS/DD 9 sectors (720KB) */
    UFT_ST_TYPE_DSDD_10     = 4,    /**< DS/DD 10 sectors (800KB) */
    UFT_ST_TYPE_DSDD_11     = 5,    /**< DS/DD 11 sectors (880KB) */
    UFT_ST_TYPE_DSHD        = 6     /**< DS/HD 18 sectors (1.44MB) */
} uft_st_disk_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * ST Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief ST boot sector (first 512 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t branch;                /**< Branch instruction */
    uint8_t oem[6];                 /**< OEM name or loader */
    uint8_t serial[3];              /**< Serial number */
    uint16_t bytes_per_sector;      /**< Bytes per sector (512) */
    uint8_t sectors_per_cluster;    /**< Sectors per cluster */
    uint16_t reserved_sectors;      /**< Reserved sectors */
    uint8_t fat_count;              /**< Number of FATs (2) */
    uint16_t root_entries;          /**< Root directory entries */
    uint16_t total_sectors;         /**< Total sectors */
    uint8_t media_descriptor;       /**< Media descriptor */
    uint16_t sectors_per_fat;       /**< Sectors per FAT */
    uint16_t sectors_per_track;     /**< Sectors per track */
    uint16_t heads;                 /**< Number of heads */
    uint16_t hidden_sectors;        /**< Hidden sectors */
    /* Boot code follows */
} uft_st_boot_sector_t;
#pragma pack(pop)

/**
 * @brief ST disk geometry
 */
typedef struct {
    const char* name;
    uft_st_disk_type_t type;
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors;
    uint32_t total_size;
} uft_st_geometry_t;

/**
 * @brief Parsed ST information
 */
typedef struct {
    uft_st_disk_type_t disk_type;
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors_per_track;
    uint16_t bytes_per_sector;
    uint32_t total_size;
    uint8_t serial[3];
    bool has_valid_boot_sector;
    const char* type_name;
} uft_st_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Geometries
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_st_geometry_t UFT_ST_GEOMETRIES[] = {
    { "SS/DD 9 sectors (360KB)",  UFT_ST_TYPE_SSDD_9,  80, 1,  9, 368640 },
    { "SS/DD 10 sectors (400KB)", UFT_ST_TYPE_SSDD_10, 80, 1, 10, 409600 },
    { "DS/DD 9 sectors (720KB)",  UFT_ST_TYPE_DSDD_9,  80, 2,  9, 737280 },
    { "DS/DD 10 sectors (800KB)", UFT_ST_TYPE_DSDD_10, 80, 2, 10, 819200 },
    { "DS/DD 11 sectors (880KB)", UFT_ST_TYPE_DSDD_11, 80, 2, 11, 901120 },
    { "DS/HD 18 sectors (1.44MB)", UFT_ST_TYPE_DSHD,   80, 2, 18, 1474560 },
    { NULL, 0, 0, 0, 0, 0 }
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline const uft_st_geometry_t* uft_st_find_geometry(size_t size) {
    for (int i = 0; UFT_ST_GEOMETRIES[i].name != NULL; i++) {
        if (UFT_ST_GEOMETRIES[i].total_size == size) {
            return &UFT_ST_GEOMETRIES[i];
        }
    }
    return NULL;
}

static inline const char* uft_st_type_name(uft_st_disk_type_t type) {
    switch (type) {
        case UFT_ST_TYPE_SSDD_9:  return "SS/DD 9 sectors";
        case UFT_ST_TYPE_SSDD_10: return "SS/DD 10 sectors";
        case UFT_ST_TYPE_DSDD_9:  return "DS/DD 9 sectors";
        case UFT_ST_TYPE_DSDD_10: return "DS/DD 10 sectors";
        case UFT_ST_TYPE_DSDD_11: return "DS/DD 11 sectors";
        case UFT_ST_TYPE_DSHD:    return "DS/HD 18 sectors";
        default: return "Unknown";
    }
}

static inline bool uft_st_validate_boot_sector(const uint8_t* data, size_t size) {
    if (!data || size < UFT_ST_SECTOR_SIZE) return false;
    
    const uft_st_boot_sector_t* boot = (const uft_st_boot_sector_t*)data;
    
    /* Check bytes per sector */
    if (boot->bytes_per_sector != 512) return false;
    
    /* Check FAT count */
    if (boot->fat_count != 2) return false;
    
    /* Check sectors per track */
    if (boot->sectors_per_track < 8 || boot->sectors_per_track > 21) return false;
    
    /* Check heads */
    if (boot->heads < 1 || boot->heads > 2) return false;
    
    return true;
}

static inline int uft_st_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_ST_SECTOR_SIZE) return 0;
    
    int score = 0;
    
    /* Check known sizes */
    const uft_st_geometry_t* geom = uft_st_find_geometry(size);
    if (geom) {
        score += 40;
    }
    
    /* Validate boot sector */
    if (uft_st_validate_boot_sector(data, size)) {
        score += 40;
        
        /* Check boot sector geometry matches file size */
        const uft_st_boot_sector_t* boot = (const uft_st_boot_sector_t*)data;
        uint32_t calc_size = boot->total_sectors * 512;
        if (calc_size == size) {
            score += 15;
        }
    }
    
    /* Check for 68000 branch instruction (common in bootable disks) */
    if (data[0] == 0x60 || (data[0] == 0x00 && data[1] == 0x60)) {
        score += 5;
    }
    
    return (score > 100) ? 100 : score;
}

static inline bool uft_st_parse(const uint8_t* data, size_t size, uft_st_info_t* info) {
    if (!data || !info || size < UFT_ST_SECTOR_SIZE) return false;
    
    memset(info, 0, sizeof(*info));
    info->total_size = size;
    info->bytes_per_sector = UFT_ST_SECTOR_SIZE;
    
    /* Try geometry table first */
    const uft_st_geometry_t* geom = uft_st_find_geometry(size);
    if (geom) {
        info->disk_type = geom->type;
        info->tracks = geom->tracks;
        info->sides = geom->sides;
        info->sectors_per_track = geom->sectors;
        info->type_name = geom->name;
    }
    
    /* Parse boot sector for additional info */
    if (uft_st_validate_boot_sector(data, size)) {
        const uft_st_boot_sector_t* boot = (const uft_st_boot_sector_t*)data;
        
        info->has_valid_boot_sector = true;
        info->sectors_per_track = boot->sectors_per_track;
        info->sides = boot->heads;
        
        /* Calculate tracks from total sectors */
        if (boot->sectors_per_track > 0 && boot->heads > 0) {
            info->tracks = boot->total_sectors / (boot->sectors_per_track * boot->heads);
        }
        
        /* Copy serial number */
        memcpy(info->serial, boot->serial, 3);
        
        /* Determine disk type from geometry */
        if (!geom) {
            if (info->sides == 1) {
                if (info->sectors_per_track == 9) info->disk_type = UFT_ST_TYPE_SSDD_9;
                else if (info->sectors_per_track == 10) info->disk_type = UFT_ST_TYPE_SSDD_10;
            } else {
                if (info->sectors_per_track == 9) info->disk_type = UFT_ST_TYPE_DSDD_9;
                else if (info->sectors_per_track == 10) info->disk_type = UFT_ST_TYPE_DSDD_10;
                else if (info->sectors_per_track == 11) info->disk_type = UFT_ST_TYPE_DSDD_11;
                else if (info->sectors_per_track == 18) info->disk_type = UFT_ST_TYPE_DSHD;
            }
            info->type_name = uft_st_type_name(info->disk_type);
        }
    }
    
    return true;
}

static inline void uft_st_create_boot_sector(uint8_t* data, uft_st_disk_type_t type) {
    if (!data) return;
    
    memset(data, 0, UFT_ST_SECTOR_SIZE);
    
    const uft_st_geometry_t* geom = NULL;
    for (int i = 0; UFT_ST_GEOMETRIES[i].name != NULL; i++) {
        if (UFT_ST_GEOMETRIES[i].type == type) {
            geom = &UFT_ST_GEOMETRIES[i];
            break;
        }
    }
    if (!geom) return;
    
    uft_st_boot_sector_t* boot = (uft_st_boot_sector_t*)data;
    
    boot->branch = 0x6000;  /* BRA.S to boot code */
    boot->bytes_per_sector = 512;
    boot->sectors_per_cluster = 2;
    boot->reserved_sectors = 1;
    boot->fat_count = 2;
    boot->root_entries = 112;
    boot->total_sectors = geom->total_size / 512;
    boot->media_descriptor = 0xF8;
    boot->sectors_per_fat = (type == UFT_ST_TYPE_DSHD) ? 9 : 5;
    boot->sectors_per_track = geom->sectors;
    boot->heads = geom->sides;
    boot->hidden_sectors = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_ST_FORMAT_H */
