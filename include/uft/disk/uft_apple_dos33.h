/**
 * @file uft_apple_dos33.h
 * @brief Apple II DOS 3.3 Disk Format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * DOS 3.3 is the standard Apple II disk format (1980).
 *
 * Geometry:
 * - 35 tracks × 16 sectors × 256 bytes = 143360 bytes
 * - Catalog track: 17
 * - VTOC (Volume Table of Contents) at Track 17, Sector 0
 *
 * Sector Ordering:
 * - DOS 3.3 order (different from ProDOS/physical order)
 * - Interleaving for performance
 *
 * File Types:
 * - TEXT (0x00), INTEGER BASIC (0x01), APPLESOFT BASIC (0x02)
 * - BINARY (0x04), S-type (0x08), RELOCATABLE (0x10), A-type (0x20), B-type (0x40)
 *
 * Directory Entry: 35 bytes
 * - 7 entries per sector
 * - 30-character filename max
 *
 * References:
 * - Beneath Apple DOS
 * - RetroGhidra Apple2Dos3DskFileSystem.java
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_APPLE_DOS33_H
#define UFT_APPLE_DOS33_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * DOS 3.3 Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Geometry */
#define UFT_DOS33_TRACKS            35
#define UFT_DOS33_SECTORS           16
#define UFT_DOS33_SECTOR_SIZE       256
#define UFT_DOS33_FILE_SIZE         (35 * 16 * 256)  /* 143360 bytes */

/** @brief Catalog location */
#define UFT_DOS33_CATALOG_TRACK     17
#define UFT_DOS33_VTOC_SECTOR       0
#define UFT_DOS33_CATALOG_START     1   /* First catalog sector */

/** @brief Directory entry */
#define UFT_DOS33_ENTRY_SIZE        35
#define UFT_DOS33_ENTRIES_PER_SECTOR 7
#define UFT_DOS33_FILENAME_LEN      30

/* ═══════════════════════════════════════════════════════════════════════════
 * DOS 3.3 File Types
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_DOS33_TYPE_TEXT         0x00    /**< Text file */
#define UFT_DOS33_TYPE_INTEGER      0x01    /**< Integer BASIC */
#define UFT_DOS33_TYPE_APPLESOFT    0x02    /**< Applesoft BASIC */
#define UFT_DOS33_TYPE_BINARY       0x04    /**< Binary file */
#define UFT_DOS33_TYPE_S            0x08    /**< S-type file */
#define UFT_DOS33_TYPE_RELOCATABLE  0x10    /**< Relocatable object */
#define UFT_DOS33_TYPE_A            0x20    /**< A-type file */
#define UFT_DOS33_TYPE_B            0x40    /**< B-type file */
#define UFT_DOS33_TYPE_LOCKED       0x80    /**< Locked flag */

/* ═══════════════════════════════════════════════════════════════════════════
 * DOS 3.3 Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief VTOC (Volume Table of Contents) - Track 17, Sector 0
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t unused1;                /**< Not used */
    uint8_t catalog_track;          /**< First catalog track */
    uint8_t catalog_sector;         /**< First catalog sector */
    uint8_t dos_release;            /**< DOS release number */
    uint8_t unused2[2];             /**< Not used */
    uint8_t volume_number;          /**< Volume number (1-254) */
    uint8_t unused3[32];            /**< Not used */
    uint8_t max_ts_pairs;           /**< Max track/sector pairs (0x7A) */
    uint8_t unused4[8];             /**< Not used */
    uint8_t last_alloc_track;       /**< Last allocated track */
    uint8_t direction;              /**< Allocation direction (+1/-1) */
    uint8_t unused5[2];             /**< Not used */
    uint8_t num_tracks;             /**< Number of tracks (35) */
    uint8_t sectors_per_track;      /**< Sectors per track (16) */
    uint16_t bytes_per_sector;      /**< Bytes per sector (256 LE) */
    uint8_t free_sector_map[140];   /**< Free sector bitmap (4 bytes/track × 35) */
} uft_dos33_vtoc_t;
#pragma pack(pop)

/**
 * @brief Catalog sector header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t unused;                 /**< Not used */
    uint8_t next_track;             /**< Next catalog track (0=last) */
    uint8_t next_sector;            /**< Next catalog sector */
    uint8_t unused2[8];             /**< Not used */
} uft_dos33_catalog_header_t;
#pragma pack(pop)

/**
 * @brief Directory entry (35 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t first_ts_track;         /**< First T/S list track */
    uint8_t first_ts_sector;        /**< First T/S list sector */
    uint8_t file_type;              /**< File type + locked flag */
    char filename[30];              /**< Filename (high bit set, space padded) */
    uint16_t sector_count;          /**< Sector count (LE) */
} uft_dos33_dir_entry_t;
#pragma pack(pop)

/**
 * @brief Track/Sector list sector
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t unused;                 /**< Not used */
    uint8_t next_ts_track;          /**< Next T/S list track */
    uint8_t next_ts_sector;         /**< Next T/S list sector */
    uint8_t unused2[2];             /**< Not used */
    uint16_t sector_offset;         /**< Sector offset in file (LE) */
    uint8_t unused3[5];             /**< Not used */
    struct {
        uint8_t track;
        uint8_t sector;
    } pairs[122];                   /**< Track/sector pairs */
} uft_dos33_ts_list_t;
#pragma pack(pop)

/**
 * @brief File information
 */
typedef struct {
    char filename[31];              /**< Filename (null terminated) */
    uint8_t file_type;              /**< Type without locked flag */
    bool is_locked;
    uint8_t first_ts_track;
    uint8_t first_ts_sector;
    uint16_t sector_count;
    uint32_t size_in_bytes;         /**< Calculated */
} uft_dos33_file_info_t;

/**
 * @brief Disk information
 */
typedef struct {
    uint8_t volume_number;
    uint8_t dos_release;
    uint8_t num_tracks;
    uint8_t sectors_per_track;
    uint16_t free_sectors;
    uint16_t file_count;
    uint32_t file_size;
    bool valid;
} uft_dos33_disk_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Sector Interleave Table (DOS 3.3 order → physical)
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uint8_t DOS33_INTERLEAVE[16] = {
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};

static const uint8_t DOS33_DEINTERLEAVE[16] = {
    0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_dos33_dir_entry_t) == 35, "DOS 3.3 dir entry must be 35 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get file type name
 */
static inline const char* uft_dos33_type_name(uint8_t type) {
    type &= 0x7F;  /* Remove locked flag */
    switch (type) {
        case UFT_DOS33_TYPE_TEXT:        return "T";  /* Text */
        case UFT_DOS33_TYPE_INTEGER:     return "I";  /* Integer BASIC */
        case UFT_DOS33_TYPE_APPLESOFT:   return "A";  /* Applesoft BASIC */
        case UFT_DOS33_TYPE_BINARY:      return "B";  /* Binary */
        case UFT_DOS33_TYPE_S:           return "S";  /* S-type */
        case UFT_DOS33_TYPE_RELOCATABLE: return "R";  /* Relocatable */
        case UFT_DOS33_TYPE_A:           return "a";  /* A-type */
        case UFT_DOS33_TYPE_B:           return "b";  /* B-type */
        default:                         return "?";
    }
}

/**
 * @brief Get file type full name
 */
static inline const char* uft_dos33_type_full_name(uint8_t type) {
    type &= 0x7F;
    switch (type) {
        case UFT_DOS33_TYPE_TEXT:        return "Text";
        case UFT_DOS33_TYPE_INTEGER:     return "Integer BASIC";
        case UFT_DOS33_TYPE_APPLESOFT:   return "Applesoft BASIC";
        case UFT_DOS33_TYPE_BINARY:      return "Binary";
        case UFT_DOS33_TYPE_S:           return "S-type";
        case UFT_DOS33_TYPE_RELOCATABLE: return "Relocatable";
        case UFT_DOS33_TYPE_A:           return "A-type";
        case UFT_DOS33_TYPE_B:           return "B-type";
        default:                         return "Unknown";
    }
}

/**
 * @brief Calculate sector offset in file
 */
static inline uint32_t uft_dos33_sector_offset(uint8_t track, uint8_t sector) {
    if (track >= UFT_DOS33_TRACKS || sector >= UFT_DOS33_SECTORS) return 0;
    return (track * UFT_DOS33_SECTORS + sector) * UFT_DOS33_SECTOR_SIZE;
}

/**
 * @brief Convert Apple II filename (high bit set) to ASCII
 */
static inline void uft_dos33_filename_to_ascii(const char* apple, char* ascii, size_t len) {
    size_t end = len;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)apple[i] & 0x7F;  /* Clear high bit */
        if (c == ' ' && end == len) {
            end = i;  /* Track trailing spaces */
        } else if (c != ' ') {
            end = len;  /* Reset if non-space */
        }
        ascii[i] = (c >= 0x20 && c <= 0x7E) ? c : '?';
    }
    /* Trim trailing spaces */
    for (size_t i = len; i > 0; i--) {
        if (ascii[i-1] != ' ') {
            ascii[i] = '\0';
            return;
        }
    }
    ascii[0] = '\0';
}

/**
 * @brief Probe for DOS 3.3 format
 * @return Confidence score (0-100)
 */
static inline int uft_dos33_probe(const uint8_t* data, size_t size) {
    if (!data) return 0;
    
    int score = 0;
    
    /* Check file size */
    if (size == UFT_DOS33_FILE_SIZE) {
        score += 30;
    } else if (size == UFT_DOS33_FILE_SIZE * 2) {
        score += 20;  /* Might be 2-sided or different format */
    } else {
        return 0;
    }
    
    /* Read VTOC at track 17, sector 0 */
    uint32_t vtoc_offset = uft_dos33_sector_offset(UFT_DOS33_CATALOG_TRACK, 
                                                    UFT_DOS33_VTOC_SECTOR);
    if (vtoc_offset + UFT_DOS33_SECTOR_SIZE > size) return 0;
    
    const uft_dos33_vtoc_t* vtoc = (const uft_dos33_vtoc_t*)(data + vtoc_offset);
    
    /* Check catalog track/sector */
    if (vtoc->catalog_track == UFT_DOS33_CATALOG_TRACK) {
        score += 25;
    }
    
    /* Check geometry */
    if (vtoc->num_tracks == UFT_DOS33_TRACKS) {
        score += 15;
    }
    if (vtoc->sectors_per_track == UFT_DOS33_SECTORS) {
        score += 15;
    }
    
    /* Check bytes per sector */
    uint16_t bps = vtoc->bytes_per_sector;
    if (bps == UFT_DOS33_SECTOR_SIZE) {
        score += 10;
    }
    
    /* Check volume number */
    if (vtoc->volume_number >= 1 && vtoc->volume_number <= 254) {
        score += 5;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse DOS 3.3 disk
 */
static inline bool uft_dos33_parse_disk(const uint8_t* data, size_t size,
                                         uft_dos33_disk_info_t* info) {
    if (!data || !info) return false;
    if (size < UFT_DOS33_FILE_SIZE) return false;
    
    memset(info, 0, sizeof(*info));
    info->file_size = size;
    
    /* Read VTOC */
    uint32_t vtoc_offset = uft_dos33_sector_offset(UFT_DOS33_CATALOG_TRACK,
                                                    UFT_DOS33_VTOC_SECTOR);
    const uft_dos33_vtoc_t* vtoc = (const uft_dos33_vtoc_t*)(data + vtoc_offset);
    
    info->volume_number = vtoc->volume_number;
    info->dos_release = vtoc->dos_release;
    info->num_tracks = vtoc->num_tracks;
    info->sectors_per_track = vtoc->sectors_per_track;
    
    /* Count free sectors from bitmap */
    for (int track = 0; track < UFT_DOS33_TRACKS; track++) {
        uint32_t bitmap = (vtoc->free_sector_map[track * 4] << 24) |
                          (vtoc->free_sector_map[track * 4 + 1] << 16) |
                          (vtoc->free_sector_map[track * 4 + 2] << 8) |
                          vtoc->free_sector_map[track * 4 + 3];
        for (int i = 0; i < 16; i++) {
            if (bitmap & (1 << (31 - i))) {
                info->free_sectors++;
            }
        }
    }
    
    /* Count files in catalog */
    uint8_t cat_track = vtoc->catalog_track;
    uint8_t cat_sector = vtoc->catalog_sector;
    
    while (cat_track != 0) {
        uint32_t cat_offset = uft_dos33_sector_offset(cat_track, cat_sector);
        if (cat_offset + UFT_DOS33_SECTOR_SIZE > size) break;
        
        const uft_dos33_catalog_header_t* cat_hdr = 
            (const uft_dos33_catalog_header_t*)(data + cat_offset);
        
        /* Check entries */
        for (int i = 0; i < UFT_DOS33_ENTRIES_PER_SECTOR; i++) {
            const uft_dos33_dir_entry_t* entry = 
                (const uft_dos33_dir_entry_t*)(data + cat_offset + 11 + i * UFT_DOS33_ENTRY_SIZE);
            
            /* Skip deleted (0xFF) or empty (0x00) entries */
            if (entry->first_ts_track == 0xFF || entry->first_ts_track == 0x00) {
                continue;
            }
            
            info->file_count++;
        }
        
        cat_track = cat_hdr->next_track;
        cat_sector = cat_hdr->next_sector;
    }
    
    info->valid = true;
    return true;
}

/**
 * @brief Print disk info
 */
static inline void uft_dos33_print_info(const uft_dos33_disk_info_t* info) {
    if (!info) return;
    
    printf("Apple II DOS 3.3 Disk:\n");
    printf("  Volume:         #%d\n", info->volume_number);
    printf("  DOS Release:    %d\n", info->dos_release);
    printf("  Tracks:         %d\n", info->num_tracks);
    printf("  Sectors/Track:  %d\n", info->sectors_per_track);
    printf("  Free Sectors:   %d\n", info->free_sectors);
    printf("  Files:          %d\n", info->file_count);
    printf("  File Size:      %u bytes\n", info->file_size);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_APPLE_DOS33_H */
