#ifndef UFT_D80_FORMAT_H
#define UFT_D80_FORMAT_H

/**
 * @file uft_d80_format.h
 * @brief Commodore D80 (8050/8250) Disk Format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * D80 is the disk image format for Commodore 8050 and 8250 dual drives.
 * These were professional/business drives with 77 tracks (8050) or 154 tracks (8250).
 *
 * Geometry:
 * - Tracks 1-39:  29 sectors/track
 * - Tracks 40-53: 27 sectors/track  
 * - Tracks 54-64: 25 sectors/track
 * - Tracks 65-77: 23 sectors/track
 * - Sector size: 256 bytes
 * - Total: 2083 sectors (533248 bytes) for D80
 * - D82 (8250): Double-sided, 154 tracks
 *
 * Directory:
 * - Located at Track 39, Sectors 1-28
 * - Sector 0 is the header (BAM)
 * - 8 directory entries per sector (32 bytes each)
 *
 * File Types:
 * - DEL (0), SEQ (1), PRG (2), USR (3), REL (4)
 *
 * References:
 * - http://www.baltissen.org/newhtm/1001-3.htm
 * - RetroGhidra CommodoreD80FileSystem.java
 *
 * SPDX-License-Identifier: MIT
 */


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * D80 Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief D80 geometry */
#define UFT_D80_TRACKS              77
#define UFT_D80_TOTAL_SECTORS       2083
#define UFT_D80_SECTOR_SIZE         256
#define UFT_D80_FILE_SIZE           (2083 * 256)  /* 533248 bytes */

/** @brief D82 (8250 double-sided) */
#define UFT_D82_TRACKS              154
#define UFT_D82_TOTAL_SECTORS       4166
#define UFT_D82_FILE_SIZE           (4166 * 256)  /* 1066496 bytes */

/** @brief Directory location */
#define UFT_D80_DIR_TRACK           39
#define UFT_D80_HEADER_SECTOR       0
#define UFT_D80_DIR_START_SECTOR    1
#define UFT_D80_DIR_END_SECTOR      28
#define UFT_D80_ENTRIES_PER_SECTOR  8
#define UFT_D80_ENTRY_SIZE          32

/** @brief Disk name/ID location */
#define UFT_D80_DISK_NAME_OFFSET    0x06  /* In header sector */
#define UFT_D80_DISK_NAME_LEN       16
#define UFT_D80_DISK_ID_OFFSET      0x18
#define UFT_D80_DISK_ID_LEN         2
#define UFT_D80_DOS_TYPE_OFFSET     0x1A

/* ═══════════════════════════════════════════════════════════════════════════
 * D80 File Types
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_D80_TYPE_DEL            0x00    /**< Deleted */
#define UFT_D80_TYPE_SEQ            0x01    /**< Sequential */
#define UFT_D80_TYPE_PRG            0x02    /**< Program */
#define UFT_D80_TYPE_USR            0x03    /**< User */
#define UFT_D80_TYPE_REL            0x04    /**< Relative */
#define UFT_D80_TYPE_LOCKED         0x40    /**< Locked flag */
#define UFT_D80_TYPE_CLOSED         0x80    /**< Closed flag (splat if not set) */

/* ═══════════════════════════════════════════════════════════════════════════
 * D80 Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief D80 directory entry (32 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t next_dir_track;     /**< Next directory track (0 = last) */
    uint8_t next_dir_sector;    /**< Next directory sector */
    uint8_t file_type;          /**< File type + flags */
    uint8_t first_track;        /**< First track of file */
    uint8_t first_sector;       /**< First sector of file */
    char filename[16];          /**< Filename (PETSCII, 0xA0 padded) */
    uint8_t rel_side_track;     /**< REL: Side sector track */
    uint8_t rel_side_sector;    /**< REL: Side sector sector */
    uint8_t rel_record_len;     /**< REL: Record length */
    uint8_t reserved[6];        /**< Reserved/unused */
    uint16_t size_in_sectors;   /**< File size in sectors (LE) */
} uft_d80_dir_entry_t;
#pragma pack(pop)

/**
 * @brief D80 sector link (first 2 bytes of each sector)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t next_track;         /**< Next track (0 = last sector) */
    uint8_t next_sector;        /**< Next sector (or bytes used if last) */
} uft_d80_sector_link_t;
#pragma pack(pop)

/**
 * @brief D80 file information
 */
typedef struct {
    char filename[17];          /**< Filename (null terminated) */
    uint8_t file_type;          /**< Type without flags */
    bool is_locked;
    bool is_closed;
    uint8_t first_track;
    uint8_t first_sector;
    uint16_t size_in_sectors;
    uint32_t size_in_bytes;     /**< Calculated */
} uft_d80_file_info_t;

/**
 * @brief D80 disk information
 */
typedef struct {
    char disk_name[17];         /**< Disk name (null terminated) */
    char disk_id[3];            /**< Disk ID (null terminated) */
    char dos_type[3];           /**< DOS type (null terminated) */
    uint32_t file_size;
    uint16_t total_sectors;
    uint8_t total_tracks;
    uint16_t file_count;
    uint16_t free_sectors;
    bool is_d82;                /**< D82 (8250 double-sided) */
    bool valid;
} uft_d80_disk_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Track Offset Table
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Sector offsets for each track (pre-calculated)
 * Track N starts at D80_TRACK_OFFSETS[N-1] * 256
 */
static const uint16_t D80_TRACK_OFFSETS[77] = {
    /* Tracks 1-39: 29 sectors each */
    0, 29, 58, 87, 116, 145, 174, 203, 232, 261,
    290, 319, 348, 377, 406, 435, 464, 493, 522, 551,
    580, 609, 638, 667, 696, 725, 754, 783, 812, 841,
    870, 899, 928, 957, 986, 1015, 1044, 1073, 1102,
    /* Tracks 40-53: 27 sectors each */
    1131, 1158, 1185, 1212, 1239, 1266, 1293, 1320, 1347, 1374,
    1401, 1428, 1455, 1482,
    /* Tracks 54-64: 25 sectors each */
    1509, 1534, 1559, 1584, 1609, 1634, 1659, 1684, 1709, 1734, 1759,
    /* Tracks 65-77: 23 sectors each */
    1784, 1807, 1830, 1853, 1876, 1899, 1922, 1945, 1968, 1991, 2014, 2037, 2060
};

/**
 * @brief Get sectors per track
 */
static inline uint8_t uft_d80_sectors_per_track(uint8_t track) {
    if (track >= 1 && track <= 39) return 29;
    if (track >= 40 && track <= 53) return 27;
    if (track >= 54 && track <= 64) return 25;
    if (track >= 65 && track <= 77) return 23;
    return 0;  /* Invalid track */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_d80_dir_entry_t) == 32, "D80 dir entry must be 32 bytes");
_Static_assert(sizeof(uft_d80_sector_link_t) == 2, "D80 sector link must be 2 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get file type name
 */
static inline const char* uft_d80_type_name(uint8_t type) {
    switch (type & 0x0F) {
        case UFT_D80_TYPE_DEL: return "DEL";
        case UFT_D80_TYPE_SEQ: return "SEQ";
        case UFT_D80_TYPE_PRG: return "PRG";
        case UFT_D80_TYPE_USR: return "USR";
        case UFT_D80_TYPE_REL: return "REL";
        default:              return "???";
    }
}

/**
 * @brief Calculate sector offset in file
 */
static inline uint32_t uft_d80_sector_offset(uint8_t track, uint8_t sector) {
    if (track < 1 || track > 77) return 0;
    if (sector >= uft_d80_sectors_per_track(track)) return 0;
    
    return (D80_TRACK_OFFSETS[track - 1] + sector) * UFT_D80_SECTOR_SIZE;
}

/**
 * @brief Convert PETSCII filename to ASCII
 */
static inline void uft_d80_petscii_to_ascii(const char* petscii, char* ascii, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)petscii[i];
        if (c == 0xA0) {
            ascii[i] = '\0';  /* End of filename */
            return;
        } else if (c >= 0x41 && c <= 0x5A) {
            ascii[i] = c;  /* A-Z */
        } else if (c >= 0xC1 && c <= 0xDA) {
            ascii[i] = c - 0x80;  /* Shifted A-Z */
        } else if (c >= 0x30 && c <= 0x39) {
            ascii[i] = c;  /* 0-9 */
        } else if (c == ':') {
            ascii[i] = '_';  /* Replace : for filesystem safety */
        } else if (c >= 0x20 && c <= 0x7E) {
            ascii[i] = c;  /* Printable ASCII */
        } else {
            ascii[i] = '?';  /* Unknown */
        }
    }
    ascii[len] = '\0';
}

/**
 * @brief Probe for D80/D82 format
 * @return Confidence score (0-100)
 */
static inline int uft_d80_probe(const uint8_t* data, size_t size) {
    if (!data) return 0;
    
    int score = 0;
    bool is_d82 = false;
    
    /* Check file size */
    if (size == UFT_D80_FILE_SIZE) {
        score += 40;
    } else if (size == UFT_D82_FILE_SIZE) {
        score += 40;
        is_d82 = true;
    } else {
        return 0;
    }
    
    /* Check header sector at track 39, sector 0 */
    uint32_t header_offset = uft_d80_sector_offset(UFT_D80_DIR_TRACK, 0);
    if (header_offset + UFT_D80_SECTOR_SIZE > size) return 0;
    
    /* Check DOS type (should be "2C" for 8050) */
    const char* dos_type = (const char*)(data + header_offset + UFT_D80_DOS_TYPE_OFFSET);
    if (dos_type[0] == '2' && dos_type[1] == 'C') {
        score += 30;
    } else if (dos_type[0] == '2' && dos_type[1] == 'D') {
        score += 30;  /* 8250 */
    }
    
    /* Check first directory sector link */
    uint32_t dir_offset = uft_d80_sector_offset(UFT_D80_DIR_TRACK, 1);
    if (dir_offset + 2 <= size) {
        uint8_t next_track = data[dir_offset];
        uint8_t next_sector = data[dir_offset + 1];
        
        /* Should point to track 39 */
        if (next_track == UFT_D80_DIR_TRACK || next_track == 0) {
            score += 15;
        }
        
        /* Next sector should be valid */
        if (next_sector <= 28 || (next_track == 0 && next_sector <= 254)) {
            score += 15;
        }
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse D80 disk header
 */
static inline bool uft_d80_parse_header(const uint8_t* data, size_t size,
                                         uft_d80_disk_info_t* info) {
    if (!data || !info) return false;
    
    memset(info, 0, sizeof(*info));
    info->file_size = size;
    
    /* Determine type */
    if (size == UFT_D80_FILE_SIZE) {
        info->total_tracks = UFT_D80_TRACKS;
        info->total_sectors = UFT_D80_TOTAL_SECTORS;
        info->is_d82 = false;
    } else if (size == UFT_D82_FILE_SIZE) {
        info->total_tracks = UFT_D82_TRACKS;
        info->total_sectors = UFT_D82_TOTAL_SECTORS;
        info->is_d82 = true;
    } else {
        return false;
    }
    
    /* Read header sector */
    uint32_t header_offset = uft_d80_sector_offset(UFT_D80_DIR_TRACK, 0);
    if (header_offset + UFT_D80_SECTOR_SIZE > size) return false;
    
    /* Disk name */
    uft_d80_petscii_to_ascii((const char*)(data + header_offset + UFT_D80_DISK_NAME_OFFSET),
                              info->disk_name, UFT_D80_DISK_NAME_LEN);
    
    /* Disk ID */
    info->disk_id[0] = data[header_offset + UFT_D80_DISK_ID_OFFSET];
    info->disk_id[1] = data[header_offset + UFT_D80_DISK_ID_OFFSET + 1];
    info->disk_id[2] = '\0';
    
    /* DOS type */
    info->dos_type[0] = data[header_offset + UFT_D80_DOS_TYPE_OFFSET];
    info->dos_type[1] = data[header_offset + UFT_D80_DOS_TYPE_OFFSET + 1];
    info->dos_type[2] = '\0';
    
    /* Count files in directory */
    for (uint8_t sector = UFT_D80_DIR_START_SECTOR; sector <= UFT_D80_DIR_END_SECTOR; sector++) {
        uint32_t dir_offset = uft_d80_sector_offset(UFT_D80_DIR_TRACK, sector);
        if (dir_offset + UFT_D80_SECTOR_SIZE > size) break;
        
        /* Skip sector link */
        uint8_t next_track = data[dir_offset];
        
        /* Check each entry */
        for (int i = 0; i < UFT_D80_ENTRIES_PER_SECTOR; i++) {
            const uft_d80_dir_entry_t* entry = 
                (const uft_d80_dir_entry_t*)(data + dir_offset + 2 + i * UFT_D80_ENTRY_SIZE);
            
            /* Skip empty/deleted entries */
            if (entry->file_type == 0x00) continue;
            
            /* Count valid files */
            if (entry->file_type & UFT_D80_TYPE_CLOSED) {
                info->file_count++;
            }
        }
        
        /* End of directory chain? */
        if (next_track == 0) break;
    }
    
    info->valid = true;
    return true;
}

/**
 * @brief Print D80 disk info
 */
static inline void uft_d80_print_info(const uft_d80_disk_info_t* info) {
    if (!info) return;
    
    printf("Commodore D80/D82 Disk Image:\n");
    printf("  Format:      %s\n", info->is_d82 ? "D82 (8250)" : "D80 (8050)");
    printf("  Disk Name:   \"%s\"\n", info->disk_name);
    printf("  Disk ID:     %s\n", info->disk_id);
    printf("  DOS Type:    %s\n", info->dos_type);
    printf("  File Size:   %u bytes\n", info->file_size);
    printf("  Tracks:      %d\n", info->total_tracks);
    printf("  Sectors:     %d\n", info->total_sectors);
    printf("  Files:       %d\n", info->file_count);
}

/**
 * @brief Read directory entry
 */
static inline bool uft_d80_read_entry(const uint8_t* data, size_t size,
                                       uint8_t track, uint8_t sector, uint8_t index,
                                       uft_d80_file_info_t* file) {
    if (!data || !file || index >= UFT_D80_ENTRIES_PER_SECTOR) return false;
    
    uint32_t offset = uft_d80_sector_offset(track, sector);
    if (offset + UFT_D80_SECTOR_SIZE > size) return false;
    
    const uft_d80_dir_entry_t* entry = 
        (const uft_d80_dir_entry_t*)(data + offset + 2 + index * UFT_D80_ENTRY_SIZE);
    
    /* Empty entry? */
    if (entry->file_type == 0x00) return false;
    
    memset(file, 0, sizeof(*file));
    
    uft_d80_petscii_to_ascii(entry->filename, file->filename, 16);
    file->file_type = entry->file_type & 0x0F;
    file->is_locked = (entry->file_type & UFT_D80_TYPE_LOCKED) != 0;
    file->is_closed = (entry->file_type & UFT_D80_TYPE_CLOSED) != 0;
    file->first_track = entry->first_track;
    file->first_sector = entry->first_sector;
    file->size_in_sectors = entry->size_in_sectors;
    file->size_in_bytes = file->size_in_sectors * (UFT_D80_SECTOR_SIZE - 2);  /* Approximate */
    
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_D80_FORMAT_H */
