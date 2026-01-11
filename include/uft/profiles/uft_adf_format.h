/**
 * @file uft_adf_format.h
 * @brief ADF (Amiga Disk File) format profile - Amiga standard disk image format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * ADF is the standard sector-level disk image format for Amiga computers.
 * It stores raw sector data in a simple sequential format, supporting both
 * OFS (Original File System) and FFS (Fast File System) formatted disks.
 *
 * Key features:
 * - Simple sequential sector storage
 * - Supports DD (880KB) and HD (1760KB) disks
 * - Compatible with UAE and other emulators
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_ADF_FORMAT_H
#define UFT_ADF_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * ADF Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief ADF sector size (always 512 bytes) */
#define UFT_ADF_SECTOR_SIZE         512

/** @brief ADF sectors per track (DD) */
#define UFT_ADF_SECTORS_DD          11

/** @brief ADF sectors per track (HD) */
#define UFT_ADF_SECTORS_HD          22

/** @brief ADF tracks per side */
#define UFT_ADF_TRACKS_PER_SIDE     80

/** @brief ADF number of sides */
#define UFT_ADF_SIDES               2

/** @brief ADF total tracks */
#define UFT_ADF_TOTAL_TRACKS        160

/** @brief ADF DD disk size (880 KB) */
#define UFT_ADF_SIZE_DD             (UFT_ADF_SECTORS_DD * UFT_ADF_TOTAL_TRACKS * UFT_ADF_SECTOR_SIZE)

/** @brief ADF HD disk size (1760 KB) */
#define UFT_ADF_SIZE_HD             (UFT_ADF_SECTORS_HD * UFT_ADF_TOTAL_TRACKS * UFT_ADF_SECTOR_SIZE)

/** @brief ADF DD disk size in bytes */
#define UFT_ADF_DD_BYTES            901120

/** @brief ADF HD disk size in bytes */
#define UFT_ADF_HD_BYTES            1802240

/** @brief Track size for DD disks */
#define UFT_ADF_TRACK_SIZE_DD       (UFT_ADF_SECTORS_DD * UFT_ADF_SECTOR_SIZE)

/** @brief Track size for HD disks */
#define UFT_ADF_TRACK_SIZE_HD       (UFT_ADF_SECTORS_HD * UFT_ADF_SECTOR_SIZE)

/* ═══════════════════════════════════════════════════════════════════════════
 * AmigaDOS Filesystem Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief OFS (Original File System) block size */
#define UFT_ADF_OFS_BLOCK_SIZE      488

/** @brief FFS (Fast File System) block size */
#define UFT_ADF_FFS_BLOCK_SIZE      512

/** @brief Boot block size (2 sectors) */
#define UFT_ADF_BOOTBLOCK_SIZE      1024

/** @brief Root block location (DD) - sector 880 */
#define UFT_ADF_ROOT_BLOCK_DD       880

/** @brief Root block location (HD) - sector 1760 */
#define UFT_ADF_ROOT_BLOCK_HD       1760

/* ═══════════════════════════════════════════════════════════════════════════
 * AmigaDOS Boot Block Signatures
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief DOS\0 signature (OFS) */
#define UFT_ADF_DOS0_SIGNATURE      "DOS\x00"

/** @brief DOS\1 signature (FFS) */
#define UFT_ADF_DOS1_SIGNATURE      "DOS\x01"

/** @brief DOS\2 signature (OFS + International) */
#define UFT_ADF_DOS2_SIGNATURE      "DOS\x02"

/** @brief DOS\3 signature (FFS + International) */
#define UFT_ADF_DOS3_SIGNATURE      "DOS\x03"

/** @brief DOS\4 signature (OFS + Directory Cache) */
#define UFT_ADF_DOS4_SIGNATURE      "DOS\x04"

/** @brief DOS\5 signature (FFS + Directory Cache) */
#define UFT_ADF_DOS5_SIGNATURE      "DOS\x05"

/** @brief DOS\6 signature (OFS + Long Filenames) */
#define UFT_ADF_DOS6_SIGNATURE      "DOS\x06"

/** @brief DOS\7 signature (FFS + Long Filenames) */
#define UFT_ADF_DOS7_SIGNATURE      "DOS\x07"

/** @brief Kickstart 1.x bootblock signature */
#define UFT_ADF_KICK_SIGNATURE      "KICK"

/* ═══════════════════════════════════════════════════════════════════════════
 * AmigaDOS Block Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief AmigaDOS block types (T_xxx)
 */
typedef enum {
    UFT_ADF_T_HEADER        = 2,    /**< Header block (root, file, dir) */
    UFT_ADF_T_DATA          = 8,    /**< Data block (OFS) */
    UFT_ADF_T_LIST          = 16,   /**< File extension block */
    UFT_ADF_T_DIRCACHE      = 33    /**< Directory cache block */
} uft_adf_block_type_t;

/**
 * @brief AmigaDOS secondary types (ST_xxx)
 */
typedef enum {
    UFT_ADF_ST_ROOT         = 1,    /**< Root directory */
    UFT_ADF_ST_DIR          = 2,    /**< User directory */
    UFT_ADF_ST_FILE         = -3,   /**< File header */
    UFT_ADF_ST_SOFTLINK     = 3,    /**< Soft link */
    UFT_ADF_ST_HARDLINK     = -4    /**< Hard link (file) */
} uft_adf_sec_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Disk Type Enumeration
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief ADF disk types
 */
typedef enum {
    UFT_ADF_TYPE_UNKNOWN    = 0,    /**< Unknown/invalid */
    UFT_ADF_TYPE_DD         = 1,    /**< Double Density (880 KB) */
    UFT_ADF_TYPE_HD         = 2     /**< High Density (1760 KB) */
} uft_adf_disk_type_t;

/**
 * @brief AmigaDOS filesystem types
 */
typedef enum {
    UFT_ADF_FS_UNKNOWN      = 0,    /**< Unknown filesystem */
    UFT_ADF_FS_OFS          = 1,    /**< Original File System */
    UFT_ADF_FS_FFS          = 2,    /**< Fast File System */
    UFT_ADF_FS_OFS_INTL     = 3,    /**< OFS + International mode */
    UFT_ADF_FS_FFS_INTL     = 4,    /**< FFS + International mode */
    UFT_ADF_FS_OFS_DC       = 5,    /**< OFS + Directory Cache */
    UFT_ADF_FS_FFS_DC       = 6,    /**< FFS + Directory Cache */
    UFT_ADF_FS_OFS_LNFS     = 7,    /**< OFS + Long Filenames */
    UFT_ADF_FS_FFS_LNFS     = 8     /**< FFS + Long Filenames */
} uft_adf_fs_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * ADF Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief AmigaDOS boot block structure (first 2 sectors)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t disk_type[4];           /**< "DOS" + filesystem type byte */
    uint32_t checksum;              /**< Boot block checksum */
    uint32_t root_block;            /**< Root block location */
    uint8_t boot_code[1012];        /**< Boot code (optional) */
} uft_adf_bootblock_t;
#pragma pack(pop)

/**
 * @brief AmigaDOS root block structure
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t type;                  /**< Block type (T_HEADER = 2) */
    uint32_t header_key;            /**< Self pointer */
    uint32_t high_seq;              /**< Unused (0) */
    uint32_t ht_size;               /**< Hash table size (72 for DD) */
    uint32_t first_data;            /**< Unused (0) */
    uint32_t checksum;              /**< Block checksum */
    uint32_t hash_table[72];        /**< Hash table */
    uint32_t bm_flag;               /**< Bitmap valid flag (-1 = valid) */
    uint32_t bm_pages[25];          /**< Bitmap block pointers */
    uint32_t bm_ext;                /**< First bitmap extension block */
    uint32_t r_days;                /**< Root creation days (since 1/1/78) */
    uint32_t r_mins;                /**< Root creation minutes */
    uint32_t r_ticks;               /**< Root creation ticks (1/50 sec) */
    uint8_t name_len;               /**< Disk name length */
    char name[30];                  /**< Disk name (BCPL string) */
    uint8_t unused1;                /**< Padding */
    uint32_t unused2[2];            /**< Reserved */
    uint32_t v_days;                /**< Last modification days */
    uint32_t v_mins;                /**< Last modification minutes */
    uint32_t v_ticks;               /**< Last modification ticks */
    uint32_t c_days;                /**< Creation days */
    uint32_t c_mins;                /**< Creation minutes */
    uint32_t c_ticks;               /**< Creation ticks */
    uint32_t next_hash;             /**< Unused (0) */
    uint32_t parent;                /**< Unused (0) */
    uint32_t extension;             /**< FFS: first dir cache block */
    uint32_t sec_type;              /**< Secondary type (ST_ROOT = 1) */
} uft_adf_rootblock_t;
#pragma pack(pop)

/**
 * @brief Parsed ADF information
 */
typedef struct {
    uft_adf_disk_type_t disk_type;  /**< Disk type (DD/HD) */
    uft_adf_fs_type_t fs_type;      /**< Filesystem type */
    uint32_t size;                  /**< Image size in bytes */
    uint32_t sectors;               /**< Total sectors */
    uint32_t sectors_per_track;     /**< Sectors per track */
    uint32_t root_block;            /**< Root block location */
    char disk_name[31];             /**< Disk name (null-terminated) */
    bool is_bootable;               /**< Has boot code */
    bool has_valid_bootblock;       /**< Valid DOS signature */
    bool has_valid_rootblock;       /**< Valid root block */
    uint32_t creation_days;         /**< Days since 1/1/1978 */
} uft_adf_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Size Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_adf_bootblock_t) == 1024, "ADF boot block must be 1024 bytes");
_Static_assert(sizeof(uft_adf_rootblock_t) == 512, "ADF root block must be 512 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Inline Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read big-endian 32-bit value
 * @param data Pointer to data
 * @return 32-bit value in native byte order
 */
static inline uint32_t uft_adf_read_be32(const uint8_t* data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

/**
 * @brief Write big-endian 32-bit value
 * @param data Pointer to data
 * @param value Value to write
 */
static inline void uft_adf_write_be32(uint8_t* data, uint32_t value) {
    data[0] = (value >> 24) & 0xFF;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;
}

/**
 * @brief Get disk type from file size
 * @param size File size in bytes
 * @return Disk type (DD, HD, or UNKNOWN)
 */
static inline uft_adf_disk_type_t uft_adf_type_from_size(size_t size) {
    if (size == UFT_ADF_DD_BYTES) return UFT_ADF_TYPE_DD;
    if (size == UFT_ADF_HD_BYTES) return UFT_ADF_TYPE_HD;
    return UFT_ADF_TYPE_UNKNOWN;
}

/**
 * @brief Get disk type name
 * @param type Disk type
 * @return Disk type description
 */
static inline const char* uft_adf_disk_type_name(uft_adf_disk_type_t type) {
    switch (type) {
        case UFT_ADF_TYPE_DD: return "DD (880 KB)";
        case UFT_ADF_TYPE_HD: return "HD (1760 KB)";
        default: return "Unknown";
    }
}

/**
 * @brief Get filesystem type from DOS byte
 * @param dos_byte DOS type byte (4th byte of boot block)
 * @return Filesystem type
 */
static inline uft_adf_fs_type_t uft_adf_fs_from_dos_byte(uint8_t dos_byte) {
    switch (dos_byte) {
        case 0: return UFT_ADF_FS_OFS;
        case 1: return UFT_ADF_FS_FFS;
        case 2: return UFT_ADF_FS_OFS_INTL;
        case 3: return UFT_ADF_FS_FFS_INTL;
        case 4: return UFT_ADF_FS_OFS_DC;
        case 5: return UFT_ADF_FS_FFS_DC;
        case 6: return UFT_ADF_FS_OFS_LNFS;
        case 7: return UFT_ADF_FS_FFS_LNFS;
        default: return UFT_ADF_FS_UNKNOWN;
    }
}

/**
 * @brief Get filesystem type name
 * @param type Filesystem type
 * @return Filesystem description
 */
static inline const char* uft_adf_fs_type_name(uft_adf_fs_type_t type) {
    switch (type) {
        case UFT_ADF_FS_OFS:      return "OFS (Original File System)";
        case UFT_ADF_FS_FFS:      return "FFS (Fast File System)";
        case UFT_ADF_FS_OFS_INTL: return "OFS + International";
        case UFT_ADF_FS_FFS_INTL: return "FFS + International";
        case UFT_ADF_FS_OFS_DC:   return "OFS + DirCache";
        case UFT_ADF_FS_FFS_DC:   return "FFS + DirCache";
        case UFT_ADF_FS_OFS_LNFS: return "OFS + Long Names";
        case UFT_ADF_FS_FFS_LNFS: return "FFS + Long Names";
        default: return "Unknown";
    }
}

/**
 * @brief Check if filesystem is FFS
 * @param type Filesystem type
 * @return true if FFS variant
 */
static inline bool uft_adf_is_ffs(uft_adf_fs_type_t type) {
    return (type == UFT_ADF_FS_FFS || type == UFT_ADF_FS_FFS_INTL ||
            type == UFT_ADF_FS_FFS_DC || type == UFT_ADF_FS_FFS_LNFS);
}

/**
 * @brief Calculate sector offset in file
 * @param sector Sector number (0-based)
 * @return Byte offset in file
 */
static inline size_t uft_adf_sector_offset(uint32_t sector) {
    return sector * UFT_ADF_SECTOR_SIZE;
}

/**
 * @brief Calculate track/side from sector number
 * @param sector Sector number (0-based)
 * @param sectors_per_track Sectors per track (11 or 22)
 * @param track Output track number
 * @param side Output side number
 * @param sec Output sector on track
 */
static inline void uft_adf_sector_to_chs(uint32_t sector, uint8_t sectors_per_track,
                                         uint8_t* track, uint8_t* side, uint8_t* sec) {
    uint32_t track_num = sector / sectors_per_track;
    *track = track_num / 2;
    *side = track_num % 2;
    *sec = sector % sectors_per_track;
}

/**
 * @brief Calculate sector number from track/side/sector
 * @param track Track number (0-79)
 * @param side Side number (0-1)
 * @param sector Sector on track (0-10 or 0-21)
 * @param sectors_per_track Sectors per track
 * @return Absolute sector number
 */
static inline uint32_t uft_adf_chs_to_sector(uint8_t track, uint8_t side,
                                             uint8_t sector, uint8_t sectors_per_track) {
    return ((track * 2) + side) * sectors_per_track + sector;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Checksum Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate AmigaDOS boot block checksum
 * @param data Boot block data (1024 bytes)
 * @return Checksum value
 */
static inline uint32_t uft_adf_bootblock_checksum(const uint8_t* data) {
    if (!data) return 0;
    
    uint32_t checksum = 0;
    const uint32_t* ptr = (const uint32_t*)data;
    
    for (int i = 0; i < 256; i++) {
        uint32_t value = uft_adf_read_be32((const uint8_t*)&ptr[i]);
        
        /* Skip checksum field at offset 4 */
        if (i == 1) value = 0;
        
        uint32_t old = checksum;
        checksum += value;
        if (checksum < old) checksum++;  /* Handle overflow */
    }
    
    return ~checksum;
}

/**
 * @brief Calculate AmigaDOS block checksum
 * @param data Block data (512 bytes)
 * @return Checksum value
 */
static inline uint32_t uft_adf_block_checksum(const uint8_t* data) {
    if (!data) return 0;
    
    uint32_t checksum = 0;
    const uint32_t* ptr = (const uint32_t*)data;
    
    for (int i = 0; i < 128; i++) {
        uint32_t value = uft_adf_read_be32((const uint8_t*)&ptr[i]);
        
        /* Skip checksum field at offset 20 (index 5) */
        if (i == 5) value = 0;
        
        checksum += value;
    }
    
    return -checksum;
}

/**
 * @brief Verify boot block checksum
 * @param data Boot block data
 * @return true if checksum is valid
 */
static inline bool uft_adf_verify_bootblock(const uint8_t* data) {
    if (!data) return false;
    
    uint32_t stored = uft_adf_read_be32(data + 4);
    uint32_t calc = uft_adf_bootblock_checksum(data);
    
    return (stored == calc);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Header Validation and Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Validate ADF boot block signature
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @return true if valid DOS signature
 */
static inline bool uft_adf_validate_signature(const uint8_t* data, size_t size) {
    if (!data || size < 4) return false;
    return (data[0] == 'D' && data[1] == 'O' && data[2] == 'S');
}

/**
 * @brief Parse ADF file into info structure
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @param info Output info structure
 * @return true if successfully parsed
 */
static inline bool uft_adf_parse(const uint8_t* data, size_t size,
                                 uft_adf_info_t* info) {
    if (!data || !info) return false;
    
    memset(info, 0, sizeof(*info));
    
    /* Determine disk type from size */
    info->disk_type = uft_adf_type_from_size(size);
    if (info->disk_type == UFT_ADF_TYPE_UNKNOWN) {
        /* Accept close sizes for compatibility */
        if (size >= UFT_ADF_DD_BYTES - 512 && size <= UFT_ADF_DD_BYTES + 512) {
            info->disk_type = UFT_ADF_TYPE_DD;
        } else if (size >= UFT_ADF_HD_BYTES - 512 && size <= UFT_ADF_HD_BYTES + 512) {
            info->disk_type = UFT_ADF_TYPE_HD;
        } else {
            return false;
        }
    }
    
    info->size = size;
    info->sectors_per_track = (info->disk_type == UFT_ADF_TYPE_HD) ?
                               UFT_ADF_SECTORS_HD : UFT_ADF_SECTORS_DD;
    info->sectors = size / UFT_ADF_SECTOR_SIZE;
    info->root_block = (info->disk_type == UFT_ADF_TYPE_HD) ?
                        UFT_ADF_ROOT_BLOCK_HD : UFT_ADF_ROOT_BLOCK_DD;
    
    /* Check boot block */
    if (size >= UFT_ADF_BOOTBLOCK_SIZE) {
        info->has_valid_bootblock = uft_adf_validate_signature(data, size);
        
        if (info->has_valid_bootblock) {
            info->fs_type = uft_adf_fs_from_dos_byte(data[3]);
            info->is_bootable = uft_adf_verify_bootblock(data);
        }
    }
    
    /* Check root block */
    size_t root_offset = info->root_block * UFT_ADF_SECTOR_SIZE;
    if (size >= root_offset + UFT_ADF_SECTOR_SIZE) {
        const uint8_t* root_data = data + root_offset;
        
        uint32_t type = uft_adf_read_be32(root_data);
        uint32_t sec_type = uft_adf_read_be32(root_data + 508);
        
        if (type == UFT_ADF_T_HEADER && (int32_t)sec_type == UFT_ADF_ST_ROOT) {
            info->has_valid_rootblock = true;
            
            /* Extract disk name */
            uint8_t name_len = root_data[432];
            if (name_len > 0 && name_len <= 30) {
                memcpy(info->disk_name, root_data + 433, name_len);
                info->disk_name[name_len] = '\0';
            }
            
            /* Extract creation date */
            info->creation_days = uft_adf_read_be32(root_data + 420);
        }
    }
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Probe and Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Probe data to determine if it's an ADF file
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @return Confidence score 0-100 (0 = not ADF, 100 = definitely ADF)
 */
static inline int uft_adf_probe(const uint8_t* data, size_t size) {
    if (!data) return 0;
    
    int score = 0;
    
    /* Check size */
    uft_adf_disk_type_t type = uft_adf_type_from_size(size);
    if (type != UFT_ADF_TYPE_UNKNOWN) {
        score += 40;  /* Exact size match */
    } else if ((size >= UFT_ADF_DD_BYTES - 1024 && size <= UFT_ADF_DD_BYTES + 1024) ||
               (size >= UFT_ADF_HD_BYTES - 1024 && size <= UFT_ADF_HD_BYTES + 1024)) {
        score += 20;  /* Close size match */
        type = (size < UFT_ADF_HD_BYTES / 2) ? UFT_ADF_TYPE_DD : UFT_ADF_TYPE_HD;
    } else {
        return 0;  /* Size completely wrong */
    }
    
    /* Check DOS signature */
    if (size >= 4 && uft_adf_validate_signature(data, size)) {
        score += 30;
        
        /* Check DOS type byte */
        if (data[3] <= 7) {
            score += 10;
        }
        
        /* Verify boot block checksum */
        if (size >= UFT_ADF_BOOTBLOCK_SIZE && uft_adf_verify_bootblock(data)) {
            score += 15;
        }
    }
    
    /* Check root block */
    uint32_t root_block = (type == UFT_ADF_TYPE_HD) ?
                           UFT_ADF_ROOT_BLOCK_HD : UFT_ADF_ROOT_BLOCK_DD;
    size_t root_offset = root_block * UFT_ADF_SECTOR_SIZE;
    
    if (size >= root_offset + UFT_ADF_SECTOR_SIZE) {
        const uint8_t* root = data + root_offset;
        uint32_t block_type = uft_adf_read_be32(root);
        int32_t sec_type = (int32_t)uft_adf_read_be32(root + 508);
        
        if (block_type == UFT_ADF_T_HEADER && sec_type == UFT_ADF_ST_ROOT) {
            score += 5;
        }
    }
    
    return (score > 100) ? 100 : score;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Creation Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create empty ADF boot block
 * @param data Output buffer (must be at least 1024 bytes)
 * @param fs_type Filesystem type (0-7)
 */
static inline void uft_adf_create_bootblock(uint8_t* data, uint8_t fs_type) {
    if (!data) return;
    
    memset(data, 0, UFT_ADF_BOOTBLOCK_SIZE);
    
    data[0] = 'D';
    data[1] = 'O';
    data[2] = 'S';
    data[3] = fs_type & 0x07;
    
    /* Root block pointer (big-endian) */
    uft_adf_write_be32(data + 8, UFT_ADF_ROOT_BLOCK_DD);
    
    /* Calculate and set checksum */
    uint32_t checksum = uft_adf_bootblock_checksum(data);
    uft_adf_write_be32(data + 4, checksum);
}

/**
 * @brief Initialize root block
 * @param data Output buffer (must be at least 512 bytes)
 * @param block_num Block number of this root block
 * @param disk_name Disk name (max 30 chars)
 */
static inline void uft_adf_create_rootblock(uint8_t* data, uint32_t block_num,
                                            const char* disk_name) {
    if (!data) return;
    
    memset(data, 0, UFT_ADF_SECTOR_SIZE);
    
    /* Block type */
    uft_adf_write_be32(data + 0, UFT_ADF_T_HEADER);
    
    /* Self pointer */
    uft_adf_write_be32(data + 4, block_num);
    
    /* Hash table size (72 entries) */
    uft_adf_write_be32(data + 12, 72);
    
    /* Bitmap valid flag */
    uft_adf_write_be32(data + 312, 0xFFFFFFFF);
    
    /* Bitmap block pointer (block after root) */
    uft_adf_write_be32(data + 316, block_num + 1);
    
    /* Disk name */
    if (disk_name) {
        size_t len = strlen(disk_name);
        if (len > 30) len = 30;
        data[432] = (uint8_t)len;
        memcpy(data + 433, disk_name, len);
    }
    
    /* Secondary type */
    uft_adf_write_be32(data + 508, (uint32_t)UFT_ADF_ST_ROOT);
    
    /* Calculate and set checksum */
    uint32_t checksum = uft_adf_block_checksum(data);
    uft_adf_write_be32(data + 20, checksum);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_ADF_FORMAT_H */
