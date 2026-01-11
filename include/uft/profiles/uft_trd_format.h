/**
 * @file uft_trd_format.h
 * @brief TRD format profile - ZX Spectrum TR-DOS disk image
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * TRD is the disk image format for TR-DOS (Technology Research DOS),
 * used with Beta Disk Interface for ZX Spectrum computers.
 * Standard format is 80 tracks, 2 sides, 16 sectors of 256 bytes.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_TRD_FORMAT_H
#define UFT_TRD_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * TRD Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief TRD signature byte (at offset 0x8E3) */
#define UFT_TRD_SIGNATURE           0x10

/** @brief TRD sector size (always 256 bytes) */
#define UFT_TRD_SECTOR_SIZE         256

/** @brief TRD sectors per track */
#define UFT_TRD_SECTORS_PER_TRACK   16

/** @brief Standard TRD file sizes */
#define UFT_TRD_SIZE_SSDD           (40 * 1 * 16 * 256)     /**< 160KB */
#define UFT_TRD_SIZE_DSDD           (40 * 2 * 16 * 256)     /**< 320KB */
#define UFT_TRD_SIZE_SSHD           (80 * 1 * 16 * 256)     /**< 320KB */
#define UFT_TRD_SIZE_DSHD           (80 * 2 * 16 * 256)     /**< 640KB */

/** @brief Disk info sector location (track 0, sector 9) */
#define UFT_TRD_INFO_SECTOR         8   /* 0-indexed */
#define UFT_TRD_INFO_OFFSET         (UFT_TRD_INFO_SECTOR * UFT_TRD_SECTOR_SIZE)

/** @brief Catalog entry size */
#define UFT_TRD_CATALOG_ENTRY_SIZE  16

/** @brief Maximum catalog entries (128 files) */
#define UFT_TRD_MAX_FILES           128

/* ═══════════════════════════════════════════════════════════════════════════
 * TRD Disk Type Codes
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_TRD_TYPE_80_2   = 0x16,     /**< 80 tracks, double-sided */
    UFT_TRD_TYPE_40_2   = 0x17,     /**< 40 tracks, double-sided */
    UFT_TRD_TYPE_80_1   = 0x18,     /**< 80 tracks, single-sided */
    UFT_TRD_TYPE_40_1   = 0x19      /**< 40 tracks, single-sided */
} uft_trd_disk_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * TRD File Types
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_TRD_FILE_BASIC      = 'B',  /**< BASIC program */
    UFT_TRD_FILE_NUMERIC    = 'N',  /**< Numeric array */
    UFT_TRD_FILE_STRING     = 'S',  /**< String array */
    UFT_TRD_FILE_CODE       = 'C',  /**< Code/bytes */
    UFT_TRD_FILE_PRINT      = '#',  /**< Print file */
    UFT_TRD_FILE_DELETED    = 0x01  /**< Deleted file marker */
} uft_trd_file_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * TRD Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TRD catalog entry (16 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char name[8];                   /**< Filename (space-padded) */
    char extension;                 /**< File type (B/C/N/S/#) */
    uint16_t start_address;         /**< Start address for CODE */
    uint16_t length;                /**< File length in bytes */
    uint8_t sector_count;           /**< Sectors occupied */
    uint8_t first_sector;           /**< First sector number */
    uint8_t first_track;            /**< First track number */
} uft_trd_catalog_entry_t;
#pragma pack(pop)

/**
 * @brief TRD disk info (at sector 9, offset 0xE1-0xFF)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t first_free_sector;      /**< First free sector */
    uint8_t first_free_track;       /**< First free track */
    uint8_t disk_type;              /**< Disk type code */
    uint8_t file_count;             /**< Number of files */
    uint16_t free_sectors;          /**< Free sector count */
    uint8_t signature;              /**< TR-DOS signature (0x10) */
    uint8_t reserved[2];            /**< Reserved */
    uint8_t reserved2[9];           /**< Reserved */
    uint8_t deleted_files;          /**< Deleted file count */
    char label[8];                  /**< Disk label */
    uint8_t reserved3[3];           /**< Reserved */
} uft_trd_disk_info_t;
#pragma pack(pop)

/**
 * @brief Parsed TRD information
 */
typedef struct {
    uint8_t tracks;
    uint8_t sides;
    uint32_t total_size;
    uint8_t file_count;
    uint16_t free_sectors;
    char label[9];                  /**< Null-terminated */
    bool valid_signature;
    uft_trd_disk_type_t disk_type;
} uft_trd_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_trd_catalog_entry_t) == 16, "TRD catalog entry must be 16 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline bool uft_trd_validate_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_TRD_INFO_OFFSET + 0x100) return false;
    /* Check signature at offset 0x8E3 (sector 9, byte 0xE3) */
    return data[0x8E7] == UFT_TRD_SIGNATURE;
}

static inline const char* uft_trd_disk_type_name(uint8_t type) {
    switch (type) {
        case UFT_TRD_TYPE_80_2: return "80 tracks, double-sided";
        case UFT_TRD_TYPE_40_2: return "40 tracks, double-sided";
        case UFT_TRD_TYPE_80_1: return "80 tracks, single-sided";
        case UFT_TRD_TYPE_40_1: return "40 tracks, single-sided";
        default: return "Unknown";
    }
}

static inline const char* uft_trd_file_type_name(uint8_t type) {
    switch (type) {
        case UFT_TRD_FILE_BASIC:   return "BASIC";
        case UFT_TRD_FILE_NUMERIC: return "Numeric Array";
        case UFT_TRD_FILE_STRING:  return "String Array";
        case UFT_TRD_FILE_CODE:    return "Code";
        case UFT_TRD_FILE_PRINT:   return "Print";
        default: return "Unknown";
    }
}

static inline void uft_trd_decode_disk_type(uint8_t type, uint8_t* tracks, uint8_t* sides) {
    switch (type) {
        case UFT_TRD_TYPE_80_2: *tracks = 80; *sides = 2; break;
        case UFT_TRD_TYPE_40_2: *tracks = 40; *sides = 2; break;
        case UFT_TRD_TYPE_80_1: *tracks = 80; *sides = 1; break;
        case UFT_TRD_TYPE_40_1: *tracks = 40; *sides = 1; break;
        default: *tracks = 80; *sides = 2; break;
    }
}

static inline int uft_trd_probe(const uint8_t* data, size_t size) {
    if (!data) return 0;
    
    int score = 0;
    
    /* Check size is valid TRD size */
    if (size == UFT_TRD_SIZE_SSDD || size == UFT_TRD_SIZE_DSDD ||
        size == UFT_TRD_SIZE_SSHD || size == UFT_TRD_SIZE_DSHD) {
        score += 30;
    }
    
    /* Check signature */
    if (size >= 0x8E8 && data[0x8E7] == UFT_TRD_SIGNATURE) {
        score += 40;
    }
    
    /* Check disk type byte */
    if (size >= 0x8E3) {
        uint8_t dt = data[0x8E3];
        if (dt == UFT_TRD_TYPE_80_2 || dt == UFT_TRD_TYPE_40_2 ||
            dt == UFT_TRD_TYPE_80_1 || dt == UFT_TRD_TYPE_40_1) {
            score += 20;
        }
    }
    
    return (score > 100) ? 100 : score;
}

static inline bool uft_trd_parse(const uint8_t* data, size_t size, uft_trd_info_t* info) {
    if (!data || size < UFT_TRD_INFO_OFFSET + 0x100 || !info) return false;
    
    memset(info, 0, sizeof(*info));
    
    /* Read disk info from sector 9 */
    const uint8_t* disk_info = data + UFT_TRD_INFO_OFFSET + 0xE1;
    
    info->disk_type = disk_info[2];
    uft_trd_decode_disk_type(info->disk_type, &info->tracks, &info->sides);
    info->total_size = info->tracks * info->sides * UFT_TRD_SECTORS_PER_TRACK * UFT_TRD_SECTOR_SIZE;
    
    info->file_count = disk_info[3];
    info->free_sectors = disk_info[4] | (disk_info[5] << 8);
    info->valid_signature = (disk_info[6] == UFT_TRD_SIGNATURE);
    
    /* Copy label */
    memcpy(info->label, disk_info + 0xF5 - 0xE1, 8);
    info->label[8] = '\0';
    
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRD_FORMAT_H */
