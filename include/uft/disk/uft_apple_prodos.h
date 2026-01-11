/**
 * @file uft_apple_prodos.h
 * @brief Apple II ProDOS Disk Format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * ProDOS is the professional disk operating system for Apple II (1983+).
 * More advanced than DOS 3.3 with hierarchical directories.
 *
 * Block-based:
 * - Block size: 512 bytes (2 sectors)
 * - Blocks addressed directly, not track/sector
 *
 * Storage Types:
 * - Seedling (0x1): 1 block, data in key block
 * - Sapling (0x2): 2-256 blocks, index block points to data
 * - Tree (0x3): 257-32768 blocks, master index
 * - Pascal Area (0x4): Pascal area
 * - Subdirectory (0xD): Directory
 * - Subdirectory Header (0xE): Directory header
 * - Volume Header (0xF): Root directory header
 *
 * File Types:
 * - $00 TYPELESS, $04 TEXT, $06 BINARY, $0F DIRECTORY
 * - $FA INTEGER BASIC, $FC APPLESOFT BASIC
 * - $FE RELOCATABLE, $FF SYSTEM
 *
 * Directory Entry: 39 bytes
 *
 * References:
 * - ProDOS Technical Reference Manual
 * - Beneath Apple ProDOS
 * - RetroGhidra Apple2ProDosDskFileSystem.java
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_APPLE_PRODOS_H
#define UFT_APPLE_PRODOS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * ProDOS Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Block size */
#define UFT_PRODOS_BLOCK_SIZE       512

/** @brief Standard 140K disk */
#define UFT_PRODOS_140K_BLOCKS      280
#define UFT_PRODOS_140K_SIZE        (280 * 512)  /* 143360 bytes */

/** @brief 800K disk */
#define UFT_PRODOS_800K_BLOCKS      1600
#define UFT_PRODOS_800K_SIZE        (1600 * 512) /* 819200 bytes */

/** @brief Key locations */
#define UFT_PRODOS_BOOT_BLOCK       0
#define UFT_PRODOS_ROOT_DIR_BLOCK   2   /* Volume directory starts here */
#define UFT_PRODOS_BITMAP_START     6   /* Volume bitmap starts here */

/** @brief Directory entry */
#define UFT_PRODOS_ENTRY_SIZE       39
#define UFT_PRODOS_ENTRIES_PER_BLOCK 13
#define UFT_PRODOS_FILENAME_LEN     15

/* ═══════════════════════════════════════════════════════════════════════════
 * ProDOS Storage Types
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_PRODOS_STORAGE_DELETED  0x00    /**< Deleted entry */
#define UFT_PRODOS_STORAGE_SEEDLING 0x01    /**< 1 block file */
#define UFT_PRODOS_STORAGE_SAPLING  0x02    /**< 2-256 blocks */
#define UFT_PRODOS_STORAGE_TREE     0x03    /**< 257-32768 blocks */
#define UFT_PRODOS_STORAGE_PASCAL   0x04    /**< Pascal area */
#define UFT_PRODOS_STORAGE_SUBDIR   0x0D    /**< Subdirectory */
#define UFT_PRODOS_STORAGE_SUBDIR_HDR 0x0E  /**< Subdirectory header */
#define UFT_PRODOS_STORAGE_VOL_HDR  0x0F    /**< Volume directory header */

/* ═══════════════════════════════════════════════════════════════════════════
 * ProDOS File Types
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_PRODOS_TYPE_TYPELESS    0x00    /**< Typeless */
#define UFT_PRODOS_TYPE_BAD         0x01    /**< Bad blocks */
#define UFT_PRODOS_TYPE_TEXT        0x04    /**< Text file */
#define UFT_PRODOS_TYPE_BINARY      0x06    /**< Binary file */
#define UFT_PRODOS_TYPE_FONT        0x07    /**< Font */
#define UFT_PRODOS_TYPE_GRAPHICS    0x08    /**< Graphics screen */
#define UFT_PRODOS_TYPE_DIRECTORY   0x0F    /**< Directory */
#define UFT_PRODOS_TYPE_ADB         0x19    /**< AppleWorks Database */
#define UFT_PRODOS_TYPE_AWP         0x1A    /**< AppleWorks Word Processor */
#define UFT_PRODOS_TYPE_ASP         0x1B    /**< AppleWorks Spreadsheet */
#define UFT_PRODOS_TYPE_INT_BASIC   0xFA    /**< Integer BASIC */
#define UFT_PRODOS_TYPE_INT_VAR     0xFB    /**< Integer BASIC variables */
#define UFT_PRODOS_TYPE_APP_BASIC   0xFC    /**< Applesoft BASIC */
#define UFT_PRODOS_TYPE_APP_VAR     0xFD    /**< Applesoft variables */
#define UFT_PRODOS_TYPE_RELOCATABLE 0xFE    /**< Relocatable code */
#define UFT_PRODOS_TYPE_SYSTEM      0xFF    /**< ProDOS system file */

/* ═══════════════════════════════════════════════════════════════════════════
 * ProDOS Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Directory entry (39 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t storage_type_namelen;   /**< Upper nibble: storage type, lower: name length */
    char filename[15];              /**< Filename (no high bit, space filled) */
    uint8_t file_type;              /**< File type */
    uint16_t key_pointer;           /**< Key block pointer (LE) */
    uint16_t blocks_used;           /**< Blocks used (LE) */
    uint8_t eof[3];                 /**< EOF position (24-bit LE) */
    uint16_t creation_date;         /**< Creation date (LE) */
    uint8_t creation_time[2];       /**< Creation time */
    uint8_t version;                /**< ProDOS version */
    uint8_t min_version;            /**< Minimum version */
    uint8_t access;                 /**< Access flags */
    uint16_t aux_type;              /**< Auxiliary type (LE) */
    uint16_t mod_date;              /**< Modification date (LE) */
    uint8_t mod_time[2];            /**< Modification time */
    uint16_t header_pointer;        /**< Header block pointer (LE) */
} uft_prodos_dir_entry_t;
#pragma pack(pop)

/**
 * @brief Volume directory header (first entry in root directory)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t storage_type_namelen;   /**< 0xFn: volume header, n=name length */
    char volume_name[15];           /**< Volume name */
    uint8_t reserved[8];            /**< Reserved */
    uint16_t creation_date;         /**< Creation date (LE) */
    uint8_t creation_time[2];       /**< Creation time */
    uint8_t version;                /**< ProDOS version */
    uint8_t min_version;            /**< Minimum version */
    uint8_t access;                 /**< Access flags */
    uint8_t entry_length;           /**< Entry length (39) */
    uint8_t entries_per_block;      /**< Entries per block (13) */
    uint16_t file_count;            /**< Active file count (LE) */
    uint16_t bitmap_pointer;        /**< Bitmap start block (LE) */
    uint16_t total_blocks;          /**< Total blocks (LE) */
} uft_prodos_vol_header_t;
#pragma pack(pop)

/**
 * @brief File information
 */
typedef struct {
    char filename[16];              /**< Filename (null terminated) */
    uint8_t storage_type;           /**< Storage type */
    uint8_t file_type;              /**< File type */
    uint16_t key_block;
    uint16_t blocks_used;
    uint32_t eof;                   /**< File size */
    uint16_t aux_type;
    bool is_directory;
    bool is_locked;
} uft_prodos_file_info_t;

/**
 * @brief Disk information
 */
typedef struct {
    char volume_name[16];           /**< Volume name (null terminated) */
    uint16_t total_blocks;
    uint16_t free_blocks;
    uint16_t file_count;
    uint32_t file_size;
    uint8_t version;
    bool valid;
} uft_prodos_disk_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_prodos_dir_entry_t) == 39, "ProDOS dir entry must be 39 bytes");
_Static_assert(sizeof(uft_prodos_vol_header_t) == 39, "ProDOS vol header must be 39 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get storage type name
 */
static inline const char* uft_prodos_storage_name(uint8_t type) {
    switch (type) {
        case UFT_PRODOS_STORAGE_DELETED:   return "Deleted";
        case UFT_PRODOS_STORAGE_SEEDLING:  return "Seedling";
        case UFT_PRODOS_STORAGE_SAPLING:   return "Sapling";
        case UFT_PRODOS_STORAGE_TREE:      return "Tree";
        case UFT_PRODOS_STORAGE_PASCAL:    return "Pascal";
        case UFT_PRODOS_STORAGE_SUBDIR:    return "Subdirectory";
        case UFT_PRODOS_STORAGE_SUBDIR_HDR:return "Subdir Header";
        case UFT_PRODOS_STORAGE_VOL_HDR:   return "Volume Header";
        default:                           return "Unknown";
    }
}

/**
 * @brief Get file type name
 */
static inline const char* uft_prodos_type_name(uint8_t type) {
    switch (type) {
        case UFT_PRODOS_TYPE_TYPELESS:    return "   ";
        case UFT_PRODOS_TYPE_BAD:         return "BAD";
        case UFT_PRODOS_TYPE_TEXT:        return "TXT";
        case UFT_PRODOS_TYPE_BINARY:      return "BIN";
        case UFT_PRODOS_TYPE_FONT:        return "FNT";
        case UFT_PRODOS_TYPE_GRAPHICS:    return "FOT";
        case UFT_PRODOS_TYPE_DIRECTORY:   return "DIR";
        case UFT_PRODOS_TYPE_ADB:         return "ADB";
        case UFT_PRODOS_TYPE_AWP:         return "AWP";
        case UFT_PRODOS_TYPE_ASP:         return "ASP";
        case UFT_PRODOS_TYPE_INT_BASIC:   return "INT";
        case UFT_PRODOS_TYPE_INT_VAR:     return "IVR";
        case UFT_PRODOS_TYPE_APP_BASIC:   return "BAS";
        case UFT_PRODOS_TYPE_APP_VAR:     return "VAR";
        case UFT_PRODOS_TYPE_RELOCATABLE: return "REL";
        case UFT_PRODOS_TYPE_SYSTEM:      return "SYS";
        default:                          return "$??";
    }
}

/**
 * @brief Calculate block offset in file
 */
static inline uint32_t uft_prodos_block_offset(uint16_t block) {
    return block * UFT_PRODOS_BLOCK_SIZE;
}

/**
 * @brief Extract 24-bit EOF value
 */
static inline uint32_t uft_prodos_get_eof(const uint8_t* eof) {
    return eof[0] | ((uint32_t)eof[1] << 8) | ((uint32_t)eof[2] << 16);
}

/**
 * @brief Probe for ProDOS format
 * @return Confidence score (0-100)
 */
static inline int uft_prodos_probe(const uint8_t* data, size_t size) {
    if (!data) return 0;
    
    int score = 0;
    
    /* Check file size */
    if (size == UFT_PRODOS_140K_SIZE) {
        score += 20;
    } else if (size == UFT_PRODOS_800K_SIZE) {
        score += 20;
    } else if (size % UFT_PRODOS_BLOCK_SIZE == 0 && size >= UFT_PRODOS_140K_SIZE) {
        score += 10;
    } else {
        return 0;
    }
    
    /* Read volume header at block 2 */
    uint32_t vol_offset = uft_prodos_block_offset(UFT_PRODOS_ROOT_DIR_BLOCK);
    if (vol_offset + UFT_PRODOS_BLOCK_SIZE > size) return 0;
    
    /* Skip block pointers (4 bytes) */
    const uft_prodos_vol_header_t* vol = 
        (const uft_prodos_vol_header_t*)(data + vol_offset + 4);
    
    /* Check storage type (should be 0xFx for volume header) */
    uint8_t storage_type = (vol->storage_type_namelen >> 4) & 0x0F;
    if (storage_type == UFT_PRODOS_STORAGE_VOL_HDR) {
        score += 40;
    } else {
        return 0;
    }
    
    /* Check name length */
    uint8_t name_len = vol->storage_type_namelen & 0x0F;
    if (name_len >= 1 && name_len <= 15) {
        score += 15;
    }
    
    /* Check entry length */
    if (vol->entry_length == UFT_PRODOS_ENTRY_SIZE) {
        score += 10;
    }
    
    /* Check entries per block */
    if (vol->entries_per_block == UFT_PRODOS_ENTRIES_PER_BLOCK) {
        score += 10;
    }
    
    /* Check total blocks */
    uint16_t total = vol->total_blocks;
    if (total > 0 && total <= 65535 && total * 512 <= size) {
        score += 5;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse ProDOS disk
 */
static inline bool uft_prodos_parse_disk(const uint8_t* data, size_t size,
                                          uft_prodos_disk_info_t* info) {
    if (!data || !info) return false;
    
    memset(info, 0, sizeof(*info));
    info->file_size = size;
    
    /* Read volume header */
    uint32_t vol_offset = uft_prodos_block_offset(UFT_PRODOS_ROOT_DIR_BLOCK);
    if (vol_offset + UFT_PRODOS_BLOCK_SIZE > size) return false;
    
    const uft_prodos_vol_header_t* vol = 
        (const uft_prodos_vol_header_t*)(data + vol_offset + 4);
    
    /* Extract volume name */
    uint8_t name_len = vol->storage_type_namelen & 0x0F;
    if (name_len > 15) name_len = 15;
    memcpy(info->volume_name, vol->volume_name, name_len);
    info->volume_name[name_len] = '\0';
    
    info->total_blocks = vol->total_blocks;
    info->file_count = vol->file_count;
    info->version = vol->version;
    
    /* Count free blocks from bitmap */
    uint16_t bitmap_block = vol->bitmap_pointer;
    uint16_t blocks_to_check = info->total_blocks;
    
    while (blocks_to_check > 0 && bitmap_block < size / UFT_PRODOS_BLOCK_SIZE) {
        uint32_t bm_offset = uft_prodos_block_offset(bitmap_block);
        if (bm_offset + UFT_PRODOS_BLOCK_SIZE > size) break;
        
        for (int i = 0; i < UFT_PRODOS_BLOCK_SIZE && blocks_to_check > 0; i++) {
            uint8_t byte = data[bm_offset + i];
            for (int bit = 7; bit >= 0 && blocks_to_check > 0; bit--) {
                if (byte & (1 << bit)) {
                    info->free_blocks++;
                }
                blocks_to_check--;
            }
        }
        bitmap_block++;
    }
    
    info->valid = true;
    return true;
}

/**
 * @brief Print disk info
 */
static inline void uft_prodos_print_info(const uft_prodos_disk_info_t* info) {
    if (!info) return;
    
    printf("Apple II ProDOS Disk:\n");
    printf("  Volume:       /%s\n", info->volume_name);
    printf("  Total Blocks: %d\n", info->total_blocks);
    printf("  Free Blocks:  %d\n", info->free_blocks);
    printf("  Files:        %d\n", info->file_count);
    printf("  Version:      %d\n", info->version);
    printf("  File Size:    %u bytes\n", info->file_size);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_APPLE_PRODOS_H */
