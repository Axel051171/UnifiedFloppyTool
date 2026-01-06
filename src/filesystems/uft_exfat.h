/**
 * @file uft_exfat.h
 * @brief exFAT Filesystem Support
 * 
 * Based on exfatprogs (GPL-2.0-or-later)
 * 
 * exFAT is used on modern flash media and larger removable disks.
 * This header provides structures for reading exFAT volumes.
 */

#ifndef UFT_EXFAT_H
#define UFT_EXFAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * exFAT Constants
 *===========================================================================*/

#define UFT_EXFAT_SIGNATURE         0xAA55
#define UFT_EXFAT_BOOT_SIGNATURE    "EXFAT   "

/** Sector and cluster limits */
#define UFT_EXFAT_MIN_SECTOR_SIZE   512
#define UFT_EXFAT_MAX_SECTOR_SIZE   4096
#define UFT_EXFAT_FIRST_CLUSTER     2
#define UFT_EXFAT_BAD_CLUSTER       0xFFFFFFF7
#define UFT_EXFAT_EOF_CLUSTER       0xFFFFFFFF

/** Directory entry size */
#define UFT_EXFAT_DENTRY_SIZE       32

/*===========================================================================
 * Volume Flags
 *===========================================================================*/

#define UFT_EXFAT_VOL_CLEAN         0x0000
#define UFT_EXFAT_VOL_DIRTY         0x0002

/*===========================================================================
 * File Attributes
 *===========================================================================*/

#define UFT_EXFAT_ATTR_READONLY     0x0001
#define UFT_EXFAT_ATTR_HIDDEN       0x0002
#define UFT_EXFAT_ATTR_SYSTEM       0x0004
#define UFT_EXFAT_ATTR_VOLUME       0x0008
#define UFT_EXFAT_ATTR_DIRECTORY    0x0010
#define UFT_EXFAT_ATTR_ARCHIVE      0x0020

/*===========================================================================
 * Directory Entry Types
 *===========================================================================*/

typedef enum {
    UFT_EXFAT_ENTRY_EOD         = 0x00, /**< End of directory */
    UFT_EXFAT_ENTRY_BITMAP      = 0x81, /**< Allocation bitmap */
    UFT_EXFAT_ENTRY_UPCASE      = 0x82, /**< Upcase table */
    UFT_EXFAT_ENTRY_VOLUME      = 0x83, /**< Volume label */
    UFT_EXFAT_ENTRY_FILE        = 0x85, /**< File or directory */
    UFT_EXFAT_ENTRY_GUID        = 0xA0, /**< Volume GUID */
    UFT_EXFAT_ENTRY_STREAM      = 0xC0, /**< Stream extension */
    UFT_EXFAT_ENTRY_NAME        = 0xC1, /**< File name */
    UFT_EXFAT_ENTRY_VENDOR      = 0xE0  /**< Vendor extension */
} uft_exfat_entry_type_t;

/** Check if entry is deleted */
#define UFT_EXFAT_IS_DELETED(type)  ((type) < 0x80)

/*===========================================================================
 * Boot Sector Structures
 *===========================================================================*/

/**
 * @brief exFAT BIOS Parameter Block (64 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t  jmp_boot[3];       /**< Jump instruction */
    uint8_t  oem_name[8];       /**< "EXFAT   " */
    uint8_t  reserved[53];      /**< Must be zero */
} uft_exfat_bpb_t;

/**
 * @brief exFAT Extended Boot Sector (56 bytes)
 */
typedef struct __attribute__((packed)) {
    uint64_t vol_offset;        /**< Partition offset (sectors) */
    uint64_t vol_length;        /**< Volume length (sectors) */
    uint32_t fat_offset;        /**< FAT offset (sectors) */
    uint32_t fat_length;        /**< FAT length (sectors) */
    uint32_t cluster_offset;    /**< Cluster heap offset (sectors) */
    uint32_t cluster_count;     /**< Total clusters */
    uint32_t root_cluster;      /**< Root directory first cluster */
    uint32_t vol_serial;        /**< Volume serial number */
    uint8_t  fs_version[2];     /**< Filesystem version (major.minor) */
    uint16_t vol_flags;         /**< Volume flags */
    uint8_t  sect_size_bits;    /**< Sector size as power of 2 */
    uint8_t  sect_per_clus_bits;/**< Sectors per cluster as power of 2 */
    uint8_t  num_fats;          /**< Number of FATs (1 or 2) */
    uint8_t  drive_select;      /**< INT 13h drive select */
    uint8_t  percent_used;      /**< Percent of heap in use */
    uint8_t  reserved[7];
} uft_exfat_bsx_t;

/**
 * @brief exFAT Partition Boot Record (512 bytes)
 */
typedef struct __attribute__((packed)) {
    uft_exfat_bpb_t bpb;        /**< BIOS Parameter Block */
    uft_exfat_bsx_t bsx;        /**< Extended Boot Sector */
    uint8_t  boot_code[390];    /**< Boot code */
    uint16_t signature;         /**< 0xAA55 */
} uft_exfat_pbr_t;

/*===========================================================================
 * Directory Entry Structures
 *===========================================================================*/

/**
 * @brief exFAT Volume Label Entry
 */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /**< 0x83 */
    uint8_t  char_count;        /**< Label length (0-11) */
    uint16_t label[11];         /**< Volume label (UTF-16LE) */
    uint8_t  reserved[8];
} uft_exfat_vol_entry_t;

/**
 * @brief exFAT File Directory Entry
 */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /**< 0x85 */
    uint8_t  secondary_count;   /**< Number of secondary entries */
    uint16_t checksum;          /**< Entry set checksum */
    uint16_t attributes;        /**< File attributes */
    uint16_t reserved1;
    uint16_t create_time;       /**< Creation time */
    uint16_t create_date;       /**< Creation date */
    uint16_t modify_time;       /**< Modification time */
    uint16_t modify_date;       /**< Modification date */
    uint16_t access_time;       /**< Access time */
    uint16_t access_date;       /**< Access date */
    uint8_t  create_time_cs;    /**< Creation time (centiseconds) */
    uint8_t  modify_time_cs;    /**< Modification time (centiseconds) */
    uint8_t  create_tz;         /**< Creation timezone */
    uint8_t  modify_tz;         /**< Modification timezone */
    uint8_t  access_tz;         /**< Access timezone */
    uint8_t  reserved2[7];
} uft_exfat_file_entry_t;

/**
 * @brief exFAT Stream Extension Entry
 */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /**< 0xC0 */
    uint8_t  flags;             /**< General flags */
    uint8_t  reserved1;
    uint8_t  name_length;       /**< Filename length */
    uint16_t name_hash;         /**< Filename hash */
    uint16_t reserved2;
    uint64_t valid_size;        /**< Valid data length */
    uint32_t reserved3;
    uint32_t start_cluster;     /**< First cluster */
    uint64_t data_length;       /**< Allocated size */
} uft_exfat_stream_entry_t;

/** Stream entry flags */
#define UFT_EXFAT_SF_CONTIGUOUS     0x02    /**< Data is contiguous */

/**
 * @brief exFAT File Name Entry
 */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /**< 0xC1 */
    uint8_t  flags;             /**< General flags */
    uint16_t name[15];          /**< Filename fragment (UTF-16LE) */
} uft_exfat_name_entry_t;

/**
 * @brief exFAT Allocation Bitmap Entry
 */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /**< 0x81 */
    uint8_t  flags;             /**< Bitmap flags */
    uint8_t  reserved[18];
    uint32_t start_cluster;     /**< First cluster of bitmap */
    uint64_t size;              /**< Bitmap size in bytes */
} uft_exfat_bitmap_entry_t;

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Check if data has valid exFAT signature
 */
static inline bool uft_exfat_is_valid(const uint8_t *data)
{
    /* Check OEM name "EXFAT   " at offset 3 */
    return (data[3] == 'E' && data[4] == 'X' && data[5] == 'F' &&
            data[6] == 'A' && data[7] == 'T' && data[8] == ' ' &&
            data[9] == ' ' && data[10] == ' ');
}

/**
 * @brief Get sector size from PBR
 */
static inline uint32_t uft_exfat_sector_size(const uft_exfat_pbr_t *pbr)
{
    return 1U << pbr->bsx.sect_size_bits;
}

/**
 * @brief Get cluster size from PBR
 */
static inline uint32_t uft_exfat_cluster_size(const uft_exfat_pbr_t *pbr)
{
    return 1U << (pbr->bsx.sect_size_bits + pbr->bsx.sect_per_clus_bits);
}

/**
 * @brief Calculate cluster offset in bytes
 */
static inline uint64_t uft_exfat_cluster_offset(const uft_exfat_pbr_t *pbr,
                                                 uint32_t cluster)
{
    uint32_t sector_size = uft_exfat_sector_size(pbr);
    uint32_t cluster_size = uft_exfat_cluster_size(pbr);
    uint64_t heap_offset = (uint64_t)pbr->bsx.cluster_offset * sector_size;
    return heap_offset + (uint64_t)(cluster - 2) * cluster_size;
}

/**
 * @brief Calculate directory entry checksum
 */
static inline uint16_t uft_exfat_checksum(const uint8_t *data, int count)
{
    uint16_t checksum = 0;
    for (int i = 0; i < count * 32; i++) {
        if (i == 2 || i == 3) continue;  /* Skip checksum field */
        checksum = ((checksum << 15) | (checksum >> 1)) + data[i];
    }
    return checksum;
}

/**
 * @brief Calculate filename hash (for name lookup)
 */
uint16_t uft_exfat_name_hash(const uint16_t *name, int length);

/**
 * @brief Parse exFAT boot sector
 */
int uft_exfat_parse_pbr(const uint8_t *data, uft_exfat_pbr_t *pbr);

#ifdef __cplusplus
}
#endif

#endif /* UFT_EXFAT_H */
