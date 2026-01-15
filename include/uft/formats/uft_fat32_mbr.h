/**
 * @file uft_fat32_mbr.h
 * @brief FAT32 filesystem and MBR partition table support
 * 
 * Ported and enhanced from MEGA65 FDISK project (GPL-3.0)
 * Original: Copyright (C) MEGA65 Project
 * 
 * This module provides:
 * - FAT32 filesystem formatting
 * - MBR partition table reading/writing
 * - Boot sector creation
 * - FSInfo sector handling
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#ifndef UFT_FAT32_MBR_H
#define UFT_FAT32_MBR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * CONSTANTS
 *============================================================================*/

#define UFT_SECTOR_SIZE         512
#define UFT_MAX_PARTITIONS      4

/* FAT32 constants */
#define UFT_FAT32_RESERVED_SECTORS  32
#define UFT_FAT32_NUM_FATS          2
#define UFT_FAT32_ROOT_CLUSTER      2

/* MBR signature */
#define UFT_MBR_SIGNATURE       0xAA55

/* Partition type codes */
#define UFT_PART_TYPE_EMPTY     0x00
#define UFT_PART_TYPE_FAT12     0x01
#define UFT_PART_TYPE_FAT16_SM  0x04    /* FAT16 < 32MB */
#define UFT_PART_TYPE_EXTENDED  0x05
#define UFT_PART_TYPE_FAT16     0x06    /* FAT16 >= 32MB */
#define UFT_PART_TYPE_NTFS      0x07
#define UFT_PART_TYPE_FAT32_CHS 0x0B
#define UFT_PART_TYPE_FAT32_LBA 0x0C
#define UFT_PART_TYPE_FAT16_LBA 0x0E
#define UFT_PART_TYPE_EXTENDED_LBA 0x0F
#define UFT_PART_TYPE_MEGA65_SYS   0x41 /* MEGA65 system partition */
#define UFT_PART_TYPE_LINUX     0x83
#define UFT_PART_TYPE_LINUX_LVM 0x8E

/* Directory entry attributes */
#define UFT_ATTR_READ_ONLY  0x01
#define UFT_ATTR_HIDDEN     0x02
#define UFT_ATTR_SYSTEM     0x04
#define UFT_ATTR_VOLUME_ID  0x08
#define UFT_ATTR_DIRECTORY  0x10
#define UFT_ATTR_ARCHIVE    0x20
#define UFT_ATTR_LONG_NAME  (UFT_ATTR_READ_ONLY | UFT_ATTR_HIDDEN | \
                             UFT_ATTR_SYSTEM | UFT_ATTR_VOLUME_ID)

/* Error codes */
#define UFT_FAT32_OK            0
#define UFT_FAT32_ERROR_READ    (-1)
#define UFT_FAT32_ERROR_WRITE   (-2)
#define UFT_FAT32_ERROR_PARAM   (-3)
#define UFT_FAT32_ERROR_NO_MBR  (-4)
#define UFT_FAT32_ERROR_FULL    (-5)
#define UFT_FAT32_ERROR_SIZE    (-6)

/*============================================================================*
 * STRUCTURES
 *============================================================================*/

/**
 * @brief Partition table entry (MBR format, 16 bytes)
 */
typedef struct {
    uint8_t  boot_flag;         /**< 0x80 = bootable, 0x00 = not bootable */
    uint8_t  start_head;        /**< Starting head (CHS) */
    uint8_t  start_sector;      /**< Starting sector (bits 0-5), cyl bits 8-9 in 6-7 */
    uint8_t  start_cylinder;    /**< Starting cylinder (lower 8 bits) */
    uint8_t  type;              /**< Partition type code */
    uint8_t  end_head;          /**< Ending head (CHS) */
    uint8_t  end_sector;        /**< Ending sector */
    uint8_t  end_cylinder;      /**< Ending cylinder */
    uint32_t lba_start;         /**< Starting LBA address */
    uint32_t lba_count;         /**< Number of sectors */
} __attribute__((packed)) uft_partition_entry_t;

/**
 * @brief Master Boot Record structure (512 bytes)
 */
typedef struct {
    uint8_t  bootstrap[446];            /**< Bootstrap code area */
    uft_partition_entry_t partitions[4]; /**< Partition table entries */
    uint16_t signature;                  /**< MBR signature (0xAA55) */
} __attribute__((packed)) uft_mbr_t;

/**
 * @brief FAT32 Boot Sector / BPB structure
 */
typedef struct {
    uint8_t  jump_boot[3];          /**< Jump instruction to boot code */
    uint8_t  oem_name[8];           /**< OEM name (e.g., "MSWIN4.1") */
    uint16_t bytes_per_sector;      /**< Bytes per sector (usually 512) */
    uint8_t  sectors_per_cluster;   /**< Sectors per cluster (power of 2) */
    uint16_t reserved_sectors;      /**< Reserved sector count */
    uint8_t  num_fats;              /**< Number of FATs (usually 2) */
    uint16_t root_entry_count;      /**< Root entries (0 for FAT32) */
    uint16_t total_sectors_16;      /**< Total sectors (0 for FAT32) */
    uint8_t  media_type;            /**< Media type (0xF8 for fixed) */
    uint16_t fat_size_16;           /**< FAT size (0 for FAT32) */
    uint16_t sectors_per_track;     /**< Sectors per track */
    uint16_t num_heads;             /**< Number of heads */
    uint32_t hidden_sectors;        /**< Hidden sectors (partition start) */
    uint32_t total_sectors_32;      /**< Total sectors (32-bit) */
    /* FAT32 specific fields (offset 36) */
    uint32_t fat_size_32;           /**< FAT size in sectors */
    uint16_t ext_flags;             /**< Extended flags */
    uint16_t fs_version;            /**< Filesystem version */
    uint32_t root_cluster;          /**< Root directory cluster */
    uint16_t fs_info;               /**< FSInfo sector number */
    uint16_t backup_boot_sector;    /**< Backup boot sector location */
    uint8_t  reserved[12];          /**< Reserved */
    uint8_t  drive_number;          /**< Drive number (0x80) */
    uint8_t  reserved1;             /**< Reserved */
    uint8_t  boot_signature;        /**< Boot signature (0x29) */
    uint32_t volume_id;             /**< Volume serial number */
    uint8_t  volume_label[11];      /**< Volume label */
    uint8_t  fs_type[8];            /**< Filesystem type ("FAT32   ") */
    uint8_t  boot_code[420];        /**< Boot code */
    uint16_t signature;             /**< Sector signature (0xAA55) */
} __attribute__((packed)) uft_fat32_boot_sector_t;

/**
 * @brief FAT32 FSInfo structure
 */
typedef struct {
    uint32_t lead_signature;    /**< Lead signature (0x41615252) */
    uint8_t  reserved1[480];    /**< Reserved */
    uint32_t struct_signature;  /**< Structure signature (0x61417272) */
    uint32_t free_count;        /**< Free cluster count (0xFFFFFFFF if unknown) */
    uint32_t next_free;         /**< Next free cluster hint */
    uint8_t  reserved2[12];     /**< Reserved */
    uint32_t trail_signature;   /**< Trail signature (0xAA550000) */
} __attribute__((packed)) uft_fat32_fsinfo_t;

/**
 * @brief FAT32 directory entry (32 bytes)
 */
typedef struct {
    uint8_t  name[11];          /**< Short name (8.3 format, space padded) */
    uint8_t  attributes;        /**< File attributes */
    uint8_t  nt_reserved;       /**< Reserved for NT */
    uint8_t  create_time_tenth; /**< Creation time (tenths of second) */
    uint16_t create_time;       /**< Creation time */
    uint16_t create_date;       /**< Creation date */
    uint16_t access_date;       /**< Last access date */
    uint16_t cluster_high;      /**< High 16 bits of first cluster */
    uint16_t modify_time;       /**< Modification time */
    uint16_t modify_date;       /**< Modification date */
    uint16_t cluster_low;       /**< Low 16 bits of first cluster */
    uint32_t file_size;         /**< File size in bytes */
} __attribute__((packed)) uft_fat32_dir_entry_t;

/**
 * @brief FAT32 format parameters
 */
typedef struct {
    uint32_t partition_start;       /**< Partition start sector (LBA) */
    uint32_t partition_size;        /**< Partition size in sectors */
    uint8_t  sectors_per_cluster;   /**< Sectors per cluster (0 = auto) */
    char     volume_label[12];      /**< Volume label (null-terminated) */
    char     oem_name[9];           /**< OEM name (null-terminated) */
    uint32_t volume_id;             /**< Volume ID (0 = generate) */
} uft_fat32_format_params_t;

/**
 * @brief Partition information (high-level view)
 */
typedef struct {
    uint8_t  index;             /**< Partition index (0-3) */
    uint8_t  type;              /**< Partition type code */
    uint8_t  bootable;          /**< Bootable flag */
    uint32_t start_lba;         /**< Start sector (LBA) */
    uint32_t size_sectors;      /**< Size in sectors */
    uint64_t size_bytes;        /**< Size in bytes */
    char     type_name[32];     /**< Human-readable type name */
} uft_partition_info_t;

/**
 * @brief I/O callback function type for sector read
 * @param sector LBA sector number
 * @param buffer Buffer to read into (512 bytes)
 * @param user_data User-provided context
 * @return 0 on success, negative on error
 */
typedef int (*uft_sector_read_fn)(uint32_t sector, uint8_t *buffer, void *user_data);

/**
 * @brief I/O callback function type for sector write
 * @param sector LBA sector number
 * @param buffer Buffer to write from (512 bytes)
 * @param user_data User-provided context
 * @return 0 on success, negative on error
 */
typedef int (*uft_sector_write_fn)(uint32_t sector, const uint8_t *buffer, void *user_data);

/**
 * @brief I/O callback structure
 */
typedef struct {
    uft_sector_read_fn  read;       /**< Read callback */
    uft_sector_write_fn write;      /**< Write callback */
    void               *user_data;  /**< User context */
    uint32_t           total_sectors; /**< Total sectors on device */
} uft_disk_io_t;

/*============================================================================*
 * MBR FUNCTIONS
 *============================================================================*/

/**
 * @brief Read and parse MBR partition table
 * @param io I/O callbacks
 * @param partitions Array to store partition info (must hold 4 entries)
 * @param count Output: number of valid partitions found
 * @return UFT_FAT32_OK on success
 */
int uft_mbr_read_partitions(const uft_disk_io_t *io, 
                            uft_partition_info_t *partitions, 
                            int *count);

/**
 * @brief Write MBR with partition table
 * @param io I/O callbacks
 * @param partitions Array of partition entries
 * @param count Number of partitions (1-4)
 * @return UFT_FAT32_OK on success
 */
int uft_mbr_write_partitions(const uft_disk_io_t *io,
                             const uft_partition_entry_t *partitions,
                             int count);

/**
 * @brief Create new MBR with single FAT32 partition
 * @param io I/O callbacks
 * @param sys_partition_size System partition size in sectors (0 for none)
 * @return UFT_FAT32_OK on success
 */
int uft_mbr_create_default(const uft_disk_io_t *io, uint32_t sys_partition_size);

/**
 * @brief Check if MBR is valid
 * @param io I/O callbacks
 * @return 1 if valid MBR present, 0 otherwise
 */
int uft_mbr_is_valid(const uft_disk_io_t *io);

/**
 * @brief Get partition type name
 * @param type Partition type code
 * @return Human-readable name string
 */
const char *uft_partition_type_name(uint8_t type);

/*============================================================================*
 * FAT32 FUNCTIONS
 *============================================================================*/

/**
 * @brief Calculate optimal cluster size for partition
 * @param partition_size Partition size in sectors
 * @return Sectors per cluster (power of 2)
 */
uint8_t uft_fat32_calc_cluster_size(uint32_t partition_size);

/**
 * @brief Format partition as FAT32
 * @param io I/O callbacks
 * @param params Format parameters
 * @param progress_cb Optional progress callback (NULL to disable)
 * @param progress_ctx Context for progress callback
 * @return UFT_FAT32_OK on success
 */
int uft_fat32_format(const uft_disk_io_t *io,
                     const uft_fat32_format_params_t *params,
                     void (*progress_cb)(uint32_t current, uint32_t total, void *ctx),
                     void *progress_ctx);

/**
 * @brief Read FAT32 boot sector information
 * @param io I/O callbacks
 * @param partition_start Partition start sector
 * @param boot_sector Output: boot sector data
 * @return UFT_FAT32_OK on success
 */
int uft_fat32_read_boot_sector(const uft_disk_io_t *io,
                               uint32_t partition_start,
                               uft_fat32_boot_sector_t *boot_sector);

/**
 * @brief Validate FAT32 filesystem
 * @param io I/O callbacks
 * @param partition_start Partition start sector
 * @return UFT_FAT32_OK if valid FAT32
 */
int uft_fat32_validate(const uft_disk_io_t *io, uint32_t partition_start);

/**
 * @brief Generate random volume ID
 * @return 32-bit volume ID
 */
uint32_t uft_fat32_generate_volume_id(void);

/*============================================================================*
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Convert LBA to CHS address
 * @param lba LBA sector number
 * @param head Output: head (0-254)
 * @param sector Output: sector (1-63)
 * @param cylinder Output: cylinder (0-1023)
 */
void uft_lba_to_chs(uint32_t lba, uint8_t *head, uint8_t *sector, uint8_t *cylinder);

/**
 * @brief Convert CHS to LBA address
 * @param head Head (0-254)
 * @param sector Sector (1-63)
 * @param cylinder Cylinder (0-1023)
 * @return LBA sector number
 */
uint32_t uft_chs_to_lba(uint8_t head, uint8_t sector, uint8_t cylinder);

/**
 * @brief Format size as human-readable string
 * @param sectors Number of 512-byte sectors
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void uft_format_size_string(uint64_t sectors, char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT32_MBR_H */
