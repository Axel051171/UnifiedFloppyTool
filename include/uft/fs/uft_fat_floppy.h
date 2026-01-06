/**
 * @file uft_fat_floppy.h
 * @brief FAT12/16 Filesystem for Floppy Disks
 * 
 * Based on libllfat by sgerwk@aol.com
 * License: GPL-3.0+
 * 
 * Low-level FAT access for floppy disk images (DD/HD)
 */

#ifndef UFT_FAT_FLOPPY_H
#define UFT_FAT_FLOPPY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * FAT Boot Sector Structure
 *===========================================================================*/

/**
 * @brief FAT12/16 Boot Sector (BPB - BIOS Parameter Block)
 * 
 * Offsets are from start of boot sector (sector 0)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  jmp_boot[3];           /**< 0x00: Jump instruction */
    char     oem_name[8];           /**< 0x03: OEM name */
    uint16_t bytes_per_sector;      /**< 0x0B: Bytes per sector (512) */
    uint8_t  sectors_per_cluster;   /**< 0x0D: Sectors per cluster */
    uint16_t reserved_sectors;      /**< 0x0E: Reserved sectors (1 for FAT12) */
    uint8_t  num_fats;              /**< 0x10: Number of FATs (usually 2) */
    uint16_t root_entry_count;      /**< 0x11: Root directory entries */
    uint16_t total_sectors_16;      /**< 0x13: Total sectors (16-bit) */
    uint8_t  media_type;            /**< 0x15: Media descriptor */
    uint16_t fat_size_16;           /**< 0x16: Sectors per FAT */
    uint16_t sectors_per_track;     /**< 0x18: Sectors per track */
    uint16_t num_heads;             /**< 0x1A: Number of heads */
    uint32_t hidden_sectors;        /**< 0x1C: Hidden sectors */
    uint32_t total_sectors_32;      /**< 0x20: Total sectors (32-bit) */
    
    /* Extended boot record (FAT12/16) */
    uint8_t  drive_number;          /**< 0x24: Drive number */
    uint8_t  reserved1;             /**< 0x25: Reserved */
    uint8_t  boot_signature;        /**< 0x26: Extended boot signature (0x29) */
    uint32_t volume_serial;         /**< 0x27: Volume serial number */
    char     volume_label[11];      /**< 0x2B: Volume label */
    char     fs_type[8];            /**< 0x36: Filesystem type string */
    uint8_t  boot_code[448];        /**< 0x3E: Boot code */
    uint16_t signature;             /**< 0x1FE: Boot signature (0xAA55) */
} uft_fat_bootsect_t;
UFT_PACK_END

/*===========================================================================
 * Media Descriptor Types
 *===========================================================================*/

/** Media descriptor byte values */
#define UFT_FAT_MEDIA_FIXED         0xF8    /**< Fixed disk */
#define UFT_FAT_MEDIA_1440K         0xF0    /**< 3.5" HD 1.44MB */
#define UFT_FAT_MEDIA_2880K         0xF0    /**< 3.5" ED 2.88MB */
#define UFT_FAT_MEDIA_720K          0xF9    /**< 3.5" DD 720KB */
#define UFT_FAT_MEDIA_1200K         0xF9    /**< 5.25" HD 1.2MB */
#define UFT_FAT_MEDIA_360K          0xFD    /**< 5.25" DD 360KB */
#define UFT_FAT_MEDIA_320K          0xFF    /**< 5.25" DD 320KB */
#define UFT_FAT_MEDIA_180K          0xFC    /**< 5.25" SS 180KB */
#define UFT_FAT_MEDIA_160K          0xFE    /**< 5.25" SS 160KB */

/*===========================================================================
 * Standard Floppy Geometries
 *===========================================================================*/

typedef struct {
    const char *name;
    uint32_t total_sectors;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint16_t tracks;
    uint8_t  sectors_per_cluster;
    uint16_t root_entries;
    uint16_t fat_sectors;
    uint8_t  media_type;
} uft_fat_geometry_t;

static const uft_fat_geometry_t uft_fat_geometries[] = {
    /* 3.5" formats */
    { "1.44MB HD",  2880, 18, 2, 80, 1, 224, 9, 0xF0 },
    { "2.88MB ED",  5760, 36, 2, 80, 2, 240, 9, 0xF0 },
    { "720KB DD",   1440,  9, 2, 80, 2, 112, 3, 0xF9 },
    
    /* 5.25" formats */
    { "1.2MB HD",   2400, 15, 2, 80, 1, 224, 7, 0xF9 },
    { "360KB DD",    720,  9, 2, 40, 2,  112, 2, 0xFD },
    { "320KB DD",    640,  8, 2, 40, 2,  112, 1, 0xFF },
    { "180KB SS",    360,  9, 1, 40, 1,   64, 2, 0xFC },
    { "160KB SS",    320,  8, 1, 40, 1,   64, 1, 0xFE },
    
    { NULL, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/*===========================================================================
 * FAT Entry Values
 *===========================================================================*/

/** FAT12 special values */
#define UFT_FAT12_FREE          0x000
#define UFT_FAT12_RESERVED_MIN  0xFF0
#define UFT_FAT12_RESERVED_MAX  0xFF6
#define UFT_FAT12_BAD           0xFF7
#define UFT_FAT12_EOF_MIN       0xFF8
#define UFT_FAT12_EOF_MAX       0xFFF

/** FAT16 special values */
#define UFT_FAT16_FREE          0x0000
#define UFT_FAT16_RESERVED_MIN  0xFFF0
#define UFT_FAT16_RESERVED_MAX  0xFFF6
#define UFT_FAT16_BAD           0xFFF7
#define UFT_FAT16_EOF_MIN       0xFFF8
#define UFT_FAT16_EOF_MAX       0xFFFF

/** Symbolic constants */
#define UFT_FAT_CLUSTER_FREE    0
#define UFT_FAT_CLUSTER_EOF    (-1)
#define UFT_FAT_CLUSTER_BAD    (-2)
#define UFT_FAT_CLUSTER_ERR    (-1000)

/** First valid data cluster */
#define UFT_FAT_FIRST_CLUSTER   2

/*===========================================================================
 * Directory Entry Structure
 *===========================================================================*/

/** File attribute flags */
#define UFT_FAT_ATTR_READONLY   0x01
#define UFT_FAT_ATTR_HIDDEN     0x02
#define UFT_FAT_ATTR_SYSTEM     0x04
#define UFT_FAT_ATTR_VOLUME_ID  0x08
#define UFT_FAT_ATTR_DIRECTORY  0x10
#define UFT_FAT_ATTR_ARCHIVE    0x20
#define UFT_FAT_ATTR_LFN        0x0F    /**< Long filename entry */

/** Directory entry special markers */
#define UFT_FAT_DIRENT_FREE     0xE5    /**< Deleted entry */
#define UFT_FAT_DIRENT_END      0x00    /**< End of directory */
#define UFT_FAT_DIRENT_KANJI    0x05    /**< First char is 0xE5 (Kanji) */

/**
 * @brief FAT directory entry (32 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    char     name[8];               /**< 0x00: Filename (space-padded) */
    char     ext[3];                /**< 0x08: Extension (space-padded) */
    uint8_t  attributes;            /**< 0x0B: File attributes */
    uint8_t  nt_reserved;           /**< 0x0C: Reserved for NT */
    uint8_t  create_time_tenth;     /**< 0x0D: Creation time (10ms units) */
    uint16_t create_time;           /**< 0x0E: Creation time */
    uint16_t create_date;           /**< 0x10: Creation date */
    uint16_t access_date;           /**< 0x12: Last access date */
    uint16_t cluster_high;          /**< 0x14: High word of cluster (FAT32) */
    uint16_t modify_time;           /**< 0x16: Last modification time */
    uint16_t modify_date;           /**< 0x18: Last modification date */
    uint16_t cluster_low;           /**< 0x1A: Low word of first cluster */
    uint32_t file_size;             /**< 0x1C: File size in bytes */
} uft_fat_dirent_t;
UFT_PACK_END

/*===========================================================================
 * Time/Date Conversion
 *===========================================================================*/

/**
 * FAT time format:
 *   Bits 0-4:   Seconds / 2 (0-29)
 *   Bits 5-10:  Minutes (0-59)
 *   Bits 11-15: Hours (0-23)
 * 
 * FAT date format:
 *   Bits 0-4:   Day (1-31)
 *   Bits 5-8:   Month (1-12)
 *   Bits 9-15:  Year - 1980
 */

static inline void uft_fat_decode_time(uint16_t time, uint16_t date,
                                       int *year, int *month, int *day,
                                       int *hour, int *minute, int *second)
{
    *second = (time & 0x1F) * 2;
    *minute = (time >> 5) & 0x3F;
    *hour   = (time >> 11) & 0x1F;
    *day    = date & 0x1F;
    *month  = (date >> 5) & 0x0F;
    *year   = ((date >> 9) & 0x7F) + 1980;
}

static inline uint16_t uft_fat_encode_time(int hour, int minute, int second)
{
    return ((hour & 0x1F) << 11) | ((minute & 0x3F) << 5) | ((second / 2) & 0x1F);
}

static inline uint16_t uft_fat_encode_date(int year, int month, int day)
{
    return (((year - 1980) & 0x7F) << 9) | ((month & 0x0F) << 5) | (day & 0x1F);
}

/*===========================================================================
 * FAT Volume Context
 *===========================================================================*/

typedef struct {
    /* Geometry from BPB */
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint32_t total_sectors;
    uint16_t fat_size;
    uint8_t  media_type;
    
    /* Calculated values */
    uint32_t fat_start_sector;      /**< First FAT sector */
    uint32_t root_dir_sector;       /**< First root dir sector */
    uint32_t root_dir_sectors;      /**< Root directory size */
    uint32_t data_start_sector;     /**< First data sector */
    uint32_t data_clusters;         /**< Total data clusters */
    uint32_t last_cluster;          /**< Last valid cluster number */
    
    /* FAT type */
    bool is_fat16;                  /**< true = FAT16, false = FAT12 */
    
    /* Volume info */
    uint32_t serial;
    char label[12];
} uft_fat_volume_t;

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Validate boot sector
 * @return 0 if valid, -1 if invalid
 */
int uft_fat_validate_bootsect(const uft_fat_bootsect_t *boot);

/**
 * @brief Initialize volume context from boot sector
 */
int uft_fat_init_volume(uft_fat_volume_t *vol, const uft_fat_bootsect_t *boot);

/**
 * @brief Determine FAT type from cluster count
 * @return 12 or 16
 */
static inline int uft_fat_determine_type(uint32_t cluster_count)
{
    return (cluster_count < 4085) ? 12 : 16;
}

/**
 * @brief Read FAT12 entry
 * @param fat FAT table data
 * @param cluster Cluster number
 * @return Next cluster or special value
 */
static inline uint16_t uft_fat12_get_entry(const uint8_t *fat, uint16_t cluster)
{
    size_t offset = cluster + (cluster / 2);  /* cluster * 1.5 */
    uint16_t value = fat[offset] | (fat[offset + 1] << 8);
    
    if (cluster & 1) {
        return value >> 4;      /* Odd cluster: high 12 bits */
    } else {
        return value & 0xFFF;   /* Even cluster: low 12 bits */
    }
}

/**
 * @brief Write FAT12 entry
 * @param fat FAT table data
 * @param cluster Cluster number
 * @param value Value to write
 */
static inline void uft_fat12_set_entry(uint8_t *fat, uint16_t cluster, uint16_t value)
{
    size_t offset = cluster + (cluster / 2);
    
    if (cluster & 1) {
        fat[offset] = (fat[offset] & 0x0F) | ((value & 0x0F) << 4);
        fat[offset + 1] = value >> 4;
    } else {
        fat[offset] = value & 0xFF;
        fat[offset + 1] = (fat[offset + 1] & 0xF0) | ((value >> 8) & 0x0F);
    }
}

/**
 * @brief Read FAT16 entry
 */
static inline uint16_t uft_fat16_get_entry(const uint8_t *fat, uint16_t cluster)
{
    return ((const uint16_t *)fat)[cluster];
}

/**
 * @brief Write FAT16 entry
 */
static inline void uft_fat16_set_entry(uint8_t *fat, uint16_t cluster, uint16_t value)
{
    ((uint16_t *)fat)[cluster] = value;
}

/**
 * @brief Check if cluster value indicates end of chain
 */
static inline bool uft_fat12_is_eof(uint16_t value)
{
    return value >= UFT_FAT12_EOF_MIN;
}

static inline bool uft_fat16_is_eof(uint16_t value)
{
    return value >= UFT_FAT16_EOF_MIN;
}

/**
 * @brief Check if cluster is bad
 */
static inline bool uft_fat12_is_bad(uint16_t value)
{
    return value == UFT_FAT12_BAD;
}

static inline bool uft_fat16_is_bad(uint16_t value)
{
    return value == UFT_FAT16_BAD;
}

/**
 * @brief Calculate sector offset for cluster
 */
static inline uint32_t uft_fat_cluster_to_sector(const uft_fat_volume_t *vol,
                                                  uint32_t cluster)
{
    return vol->data_start_sector + 
           (cluster - UFT_FAT_FIRST_CLUSTER) * vol->sectors_per_cluster;
}

/**
 * @brief Get directory entry filename as string
 * @param dirent Directory entry
 * @param buf Output buffer (at least 13 bytes: 8.3 + null)
 * @return Filename length
 */
int uft_fat_get_filename(const uft_fat_dirent_t *dirent, char *buf);

/**
 * @brief Check if directory entry is valid
 */
static inline bool uft_fat_dirent_is_valid(const uft_fat_dirent_t *dirent)
{
    uint8_t first = (uint8_t)dirent->name[0];
    return first != UFT_FAT_DIRENT_FREE && 
           first != UFT_FAT_DIRENT_END &&
           (dirent->attributes & UFT_FAT_ATTR_LFN) != UFT_FAT_ATTR_LFN;
}

/**
 * @brief Check if directory entry is deleted
 */
static inline bool uft_fat_dirent_is_deleted(const uft_fat_dirent_t *dirent)
{
    return (uint8_t)dirent->name[0] == UFT_FAT_DIRENT_FREE;
}

/**
 * @brief Check if directory entry marks end
 */
static inline bool uft_fat_dirent_is_end(const uft_fat_dirent_t *dirent)
{
    return (uint8_t)dirent->name[0] == UFT_FAT_DIRENT_END;
}

/**
 * @brief Check if entry is a directory
 */
static inline bool uft_fat_dirent_is_dir(const uft_fat_dirent_t *dirent)
{
    return (dirent->attributes & UFT_FAT_ATTR_DIRECTORY) != 0;
}

/**
 * @brief Get first cluster from directory entry
 */
static inline uint32_t uft_fat_dirent_cluster(const uft_fat_dirent_t *dirent)
{
    return dirent->cluster_low;  /* FAT12/16 only uses low word */
}

/**
 * @brief Detect floppy geometry from boot sector
 * @return Pointer to geometry entry or NULL
 */
const uft_fat_geometry_t *uft_fat_detect_geometry(const uft_fat_bootsect_t *boot);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT_FLOPPY_H */
