/**
 * @file uft_fs_detect.h
 * @brief Filesystem Detection via Magic Signatures
 * 
 * Based on util-linux libblkid superblocks (LGPL)
 * 
 * Detects filesystem types from disk images using magic bytes,
 * particularly useful for floppy disk format identification.
 */

#ifndef UFT_FS_DETECT_H
#define UFT_FS_DETECT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Filesystem Types
 *===========================================================================*/

typedef enum {
    UFT_FS_UNKNOWN = 0,
    
    /* FAT filesystems (common on floppies) */
    UFT_FS_FAT12,
    UFT_FS_FAT16,
    UFT_FS_FAT32,
    UFT_FS_EXFAT,
    
    /* Minix (classic floppy filesystem) */
    UFT_FS_MINIX1,          /**< Minix v1, 14-char names */
    UFT_FS_MINIX1_30,       /**< Minix v1, 30-char names */
    UFT_FS_MINIX2,          /**< Minix v2, 14-char names */
    UFT_FS_MINIX2_30,       /**< Minix v2, 30-char names */
    UFT_FS_MINIX3,          /**< Minix v3 */
    
    /* Unix/Linux */
    UFT_FS_EXT2,
    UFT_FS_EXT3,
    UFT_FS_EXT4,
    UFT_FS_XFS,
    UFT_FS_BTRFS,
    UFT_FS_REISERFS,
    
    /* BSD */
    UFT_FS_UFS,
    UFT_FS_UFS2,
    
    /* Windows */
    UFT_FS_NTFS,
    UFT_FS_HPFS,
    
    /* Optical/Archive */
    UFT_FS_ISO9660,
    UFT_FS_UDF,
    
    /* Retro/8-bit (floppy era) */
    UFT_FS_AMIGA_OFS,       /**< Amiga Original File System */
    UFT_FS_AMIGA_FFS,       /**< Amiga Fast File System */
    UFT_FS_AMIGA_PFS,       /**< Amiga Professional FS */
    UFT_FS_HFS,             /**< Apple HFS */
    UFT_FS_HFS_PLUS,        /**< Apple HFS+ */
    UFT_FS_ADFS,            /**< Acorn ADFS */
    UFT_FS_ROMFS,
    UFT_FS_CRAMFS,
    UFT_FS_SQUASHFS,
    
    /* Other */
    UFT_FS_SWAP,            /**< Linux swap */
    UFT_FS_LVM,
    UFT_FS_LUKS,
    
    UFT_FS_COUNT
} uft_fs_type_t;

/*===========================================================================
 * Magic Signature Definition
 *===========================================================================*/

/**
 * @brief Magic signature for filesystem detection
 */
typedef struct {
    const uint8_t *magic;   /**< Magic bytes to match */
    size_t len;             /**< Length of magic */
    size_t offset;          /**< Offset from start of sector */
    uint32_t kb_offset;     /**< Offset in KB (1024-byte blocks) */
} uft_fs_magic_t;

/*===========================================================================
 * FAT Filesystem Detection
 *===========================================================================*/

/* FAT Boot Sector Magic */
#define UFT_FAT_MAGIC_55AA      0xAA55  /**< At offset 510 */

/* FAT filesystem strings (at various offsets) */
#define UFT_FAT12_MAGIC         "FAT12   "
#define UFT_FAT16_MAGIC         "FAT16   "
#define UFT_FAT32_MAGIC         "FAT32   "
#define UFT_MSDOS_MAGIC         "MSDOS"
#define UFT_MSWIN_MAGIC         "MSWIN"

/* FAT magic offsets */
#define UFT_FAT16_FSTYPE_OFF    0x36    /**< "FAT12   " or "FAT16   " */
#define UFT_FAT32_FSTYPE_OFF    0x52    /**< "FAT32   " */

/* FAT media descriptor (valid values) */
#define UFT_FAT_MEDIA_FLOPPY    0xF0    /**< 3.5" floppy */
#define UFT_FAT_MEDIA_FIXED     0xF8    /**< Fixed disk */
#define UFT_FAT_MEDIA_F9        0xF9    /**< 720KB or 1.2MB floppy */
#define UFT_FAT_MEDIA_FA        0xFA    /**< 320KB floppy */
#define UFT_FAT_MEDIA_FB        0xFB    /**< 640KB floppy */
#define UFT_FAT_MEDIA_FC        0xFC    /**< 180KB floppy */
#define UFT_FAT_MEDIA_FD        0xFD    /**< 360KB floppy */
#define UFT_FAT_MEDIA_FE        0xFE    /**< 160KB floppy */
#define UFT_FAT_MEDIA_FF        0xFF    /**< 320KB floppy */

/**
 * @brief Check if byte is valid FAT media descriptor
 */
static inline bool uft_fat_valid_media(uint8_t media)
{
    return media >= 0xF0 || media == 0xF8;
}

/**
 * @brief FAT Boot Sector (BPB) structure
 */
typedef struct __attribute__((packed)) {
    uint8_t  jmp[3];            /**< Jump instruction */
    uint8_t  oem_name[8];       /**< OEM name */
    uint8_t  bytes_per_sec[2];  /**< Bytes per sector (usually 512) */
    uint8_t  sec_per_clus;      /**< Sectors per cluster */
    uint16_t reserved_secs;     /**< Reserved sectors before FAT */
    uint8_t  num_fats;          /**< Number of FATs (usually 2) */
    uint8_t  root_entries[2];   /**< Root directory entries (FAT12/16) */
    uint8_t  total_secs_16[2];  /**< Total sectors (16-bit) */
    uint8_t  media;             /**< Media descriptor */
    uint16_t fat_size_16;       /**< Sectors per FAT (FAT12/16) */
    uint16_t sec_per_track;     /**< Sectors per track */
    uint16_t heads;             /**< Number of heads */
    uint32_t hidden_secs;       /**< Hidden sectors */
    uint32_t total_secs_32;     /**< Total sectors (32-bit) */
} uft_fat_bpb_t;

/**
 * @brief FAT32 Extended BPB
 */
typedef struct __attribute__((packed)) {
    uft_fat_bpb_t bpb;
    uint32_t fat_size_32;       /**< Sectors per FAT (FAT32) */
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;      /**< Root directory cluster */
    uint16_t fs_info;           /**< FSInfo sector */
    uint16_t backup_boot;       /**< Backup boot sector */
    uint8_t  reserved[12];
    uint8_t  drive_num;
    uint8_t  reserved1;
    uint8_t  boot_sig;          /**< 0x29 if following fields valid */
    uint8_t  vol_id[4];         /**< Volume serial number */
    uint8_t  vol_label[11];     /**< Volume label */
    uint8_t  fs_type[8];        /**< "FAT32   " */
} uft_fat32_bpb_t;

/*===========================================================================
 * Minix Filesystem Detection
 *===========================================================================*/

/* Minix superblock is at offset 1024 (1KB) */
#define UFT_MINIX_SB_OFFSET     1024

/* Minix magic numbers (at offset 0x10 in superblock) */
#define UFT_MINIX1_MAGIC        0x137F  /**< Minix v1, 14-char names */
#define UFT_MINIX1_MAGIC2       0x138F  /**< Minix v1, 30-char names */
#define UFT_MINIX2_MAGIC        0x2468  /**< Minix v2, 14-char names */
#define UFT_MINIX2_MAGIC2       0x2478  /**< Minix v2, 30-char names */
#define UFT_MINIX3_MAGIC        0x4D5A  /**< Minix v3 */

#define UFT_MINIX_BLOCK_SIZE    1024

/**
 * @brief Minix v1/v2 superblock
 */
typedef struct __attribute__((packed)) {
    uint16_t s_ninodes;         /**< Number of inodes */
    uint16_t s_nzones;          /**< Number of zones (v1) */
    uint16_t s_imap_blocks;     /**< Inode bitmap blocks */
    uint16_t s_zmap_blocks;     /**< Zone bitmap blocks */
    uint16_t s_firstdatazone;   /**< First data zone */
    uint16_t s_log_zone_size;   /**< Log2 of zone/block ratio */
    uint32_t s_max_size;        /**< Maximum file size */
    uint16_t s_magic;           /**< Magic number */
    uint16_t s_state;           /**< Mount state */
    uint32_t s_zones;           /**< Number of zones (v2) */
} uft_minix_sb_t;

/**
 * @brief Minix v3 superblock
 */
typedef struct __attribute__((packed)) {
    uint32_t s_ninodes;
    uint16_t s_pad0;
    uint16_t s_imap_blocks;
    uint16_t s_zmap_blocks;
    uint16_t s_firstdatazone;
    uint16_t s_log_zone_size;
    uint16_t s_pad1;
    uint32_t s_max_size;
    uint32_t s_zones;
    uint16_t s_magic;
    uint16_t s_pad2;
    uint16_t s_blocksize;
    uint8_t  s_disk_version;
} uft_minix3_sb_t;

/*===========================================================================
 * Other Filesystem Signatures
 *===========================================================================*/

/* Ext2/3/4 superblock at 1024 bytes */
#define UFT_EXT_SB_OFFSET       1024
#define UFT_EXT_MAGIC           0xEF53  /**< At offset 0x38 in superblock */

/* NTFS */
#define UFT_NTFS_MAGIC          "NTFS    "
#define UFT_NTFS_MAGIC_OFF      3

/* ISO9660 */
#define UFT_ISO9660_MAGIC       "CD001"
#define UFT_ISO9660_MAGIC_OFF   0x8001  /**< Primary volume descriptor */

/* Amiga */
#define UFT_AMIGA_DOS_MAGIC     "DOS"   /**< At offset 0 */
#define UFT_AMIGA_OFS_TYPE      0       /**< DOS\0 */
#define UFT_AMIGA_FFS_TYPE      1       /**< DOS\1 */

/* HFS */
#define UFT_HFS_MAGIC           0x4244  /**< "BD" at offset 1024 */
#define UFT_HFS_PLUS_MAGIC      0x482B  /**< "H+" at offset 1024 */

/* exFAT */
#define UFT_EXFAT_MAGIC         "EXFAT   "
#define UFT_EXFAT_MAGIC_OFF     3

/* ADFS (Acorn) */
#define UFT_ADFS_MAGIC_HUGO     0x6F677548  /**< "Hugo" */
#define UFT_ADFS_MAGIC_NICK     0x6B63694E  /**< "Nick" */

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

/**
 * @brief Detect filesystem type from first sectors
 * @param data Disk data (at least 4KB recommended)
 * @param len Length of data available
 * @return Detected filesystem type
 */
uft_fs_type_t uft_fs_detect(const uint8_t *data, size_t len);

/**
 * @brief Detect FAT filesystem variant
 * @param data Boot sector (512 bytes minimum)
 * @return FAT type or UFT_FS_UNKNOWN
 */
uft_fs_type_t uft_fat_detect(const uint8_t *data);

/**
 * @brief Detect Minix filesystem version
 * @param data Disk data including superblock at 1KB
 * @param len Data length (>= 2KB)
 * @return Minix version or UFT_FS_UNKNOWN
 */
uft_fs_type_t uft_minix_detect(const uint8_t *data, size_t len);

/**
 * @brief Validate FAT boot sector
 * @param data Boot sector (512 bytes)
 * @return true if valid FAT boot sector
 */
bool uft_fat_validate(const uint8_t *data);

/**
 * @brief Get filesystem type name
 */
const char *uft_fs_type_name(uft_fs_type_t type);

/**
 * @brief Get FAT cluster count (determines FAT12/16/32)
 * @param data Boot sector
 * @param cluster_count Output: number of clusters
 * @return 12, 16, or 32 for FAT bits, 0 on error
 */
int uft_fat_get_bits(const uint8_t *data, uint32_t *cluster_count);

/*===========================================================================
 * Inline Detection Helpers
 *===========================================================================*/

/**
 * @brief Quick check for FAT boot sector signature
 */
static inline bool uft_has_fat_signature(const uint8_t *data)
{
    return data[510] == 0x55 && data[511] == 0xAA;
}

/**
 * @brief Read 16-bit little-endian
 */
static inline uint16_t uft_fs_read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Read 32-bit little-endian
 */
static inline uint32_t uft_fs_read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Check for Minix magic at superblock
 */
static inline uft_fs_type_t uft_check_minix_magic(uint16_t magic)
{
    switch (magic) {
        case UFT_MINIX1_MAGIC:  return UFT_FS_MINIX1;
        case UFT_MINIX1_MAGIC2: return UFT_FS_MINIX1_30;
        case UFT_MINIX2_MAGIC:  return UFT_FS_MINIX2;
        case UFT_MINIX2_MAGIC2: return UFT_FS_MINIX2_30;
        case UFT_MINIX3_MAGIC:  return UFT_FS_MINIX3;
        default: return UFT_FS_UNKNOWN;
    }
}

/**
 * @brief Quick Minix detection
 */
static inline bool uft_is_minix(const uint8_t *data, size_t len)
{
    if (len < UFT_MINIX_SB_OFFSET + 32) return false;
    uint16_t magic = uft_fs_read_le16(&data[UFT_MINIX_SB_OFFSET + 0x10]);
    return uft_check_minix_magic(magic) != UFT_FS_UNKNOWN;
}

/**
 * @brief Quick ext2/3/4 detection
 */
static inline bool uft_is_ext(const uint8_t *data, size_t len)
{
    if (len < UFT_EXT_SB_OFFSET + 0x3A) return false;
    return uft_fs_read_le16(&data[UFT_EXT_SB_OFFSET + 0x38]) == UFT_EXT_MAGIC;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FS_DETECT_H */
