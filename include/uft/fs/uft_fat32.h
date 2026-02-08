/**
 * @file uft_fat32.h
 * @brief FAT32 Filesystem Support for Hard Disk Images
 * @version 1.0.0
 * 
 * FAT32 implementation for larger disk images:
 * - ZIP/JAZ drives, CF cards, HDD images
 * - Up to 2TB volume size
 * - Long filename support
 * - FSInfo sector management
 * - Backup boot sector
 * 
 * @note Part of UnifiedFloppyTool preservation suite
 */

#ifndef UFT_FAT32_H
#define UFT_FAT32_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_compiler.h"
#include "uft/fs/uft_fat12.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * FAT32 Constants
 *===========================================================================*/

/** FAT32 special values */
#define UFT_FAT32_FREE              0x00000000
#define UFT_FAT32_RESERVED_MIN      0x0FFFFFF0
#define UFT_FAT32_RESERVED_MAX      0x0FFFFFF6
#define UFT_FAT32_BAD               0x0FFFFFF7
#define UFT_FAT32_EOF_MIN           0x0FFFFFF8
#define UFT_FAT32_EOF_MAX           0x0FFFFFFF
#define UFT_FAT32_EOF               0x0FFFFFFF
#define UFT_FAT32_CLUSTER_MASK      0x0FFFFFFF

/** FSInfo signatures */
#define UFT_FAT32_FSINFO_SIG1       0x41615252  /* "RRaA" */
#define UFT_FAT32_FSINFO_SIG2       0x61417272  /* "rrAa" */
#define UFT_FAT32_FSINFO_SIG3       0xAA550000

/** Minimum FAT32 cluster count */
#define UFT_FAT32_MIN_CLUSTERS      65525

/** Maximum FAT32 cluster count (practical limit) */
#define UFT_FAT32_MAX_CLUSTERS      0x0FFFFFEF

/** Default clusters per FAT32 volume size */
#define UFT_FAT32_CLUSTER_SIZE_DEFAULT  4096

/*===========================================================================
 * FAT32 Boot Sector Extension
 *===========================================================================*/

/**
 * @brief FAT32 Extended Boot Sector
 * 
 * Contains FAT32-specific fields after the standard BPB
 */
UFT_PACK_BEGIN
typedef struct {
    /* Standard BPB (0x00-0x23) */
    uint8_t  jmp_boot[3];           /**< 0x00: Jump instruction */
    char     oem_name[8];           /**< 0x03: OEM name */
    uint16_t bytes_per_sector;      /**< 0x0B: Bytes per sector */
    uint8_t  sectors_per_cluster;   /**< 0x0D: Sectors per cluster */
    uint16_t reserved_sectors;      /**< 0x0E: Reserved sectors (32 typical) */
    uint8_t  num_fats;              /**< 0x10: Number of FATs */
    uint16_t root_entry_count;      /**< 0x11: Root entries (0 for FAT32) */
    uint16_t total_sectors_16;      /**< 0x13: Total sectors (0 for FAT32) */
    uint8_t  media_type;            /**< 0x15: Media descriptor */
    uint16_t fat_size_16;           /**< 0x16: Sectors per FAT (0 for FAT32) */
    uint16_t sectors_per_track;     /**< 0x18: Sectors per track */
    uint16_t num_heads;             /**< 0x1A: Number of heads */
    uint32_t hidden_sectors;        /**< 0x1C: Hidden sectors */
    uint32_t total_sectors_32;      /**< 0x20: Total sectors (32-bit) */
    
    /* FAT32 Extended BPB */
    uint32_t fat_size_32;           /**< 0x24: Sectors per FAT (FAT32) */
    uint16_t ext_flags;             /**< 0x28: Extended flags */
    uint16_t fs_version;            /**< 0x2A: Filesystem version (0.0) */
    uint32_t root_cluster;          /**< 0x2C: First cluster of root dir */
    uint16_t fsinfo_sector;         /**< 0x30: FSInfo sector (usually 1) */
    uint16_t backup_boot_sector;    /**< 0x32: Backup boot sector (usually 6) */
    uint8_t  reserved[12];          /**< 0x34: Reserved */
    
    /* Extended boot record */
    uint8_t  drive_number;          /**< 0x40: Drive number */
    uint8_t  reserved1;             /**< 0x41: Reserved */
    uint8_t  boot_signature;        /**< 0x42: Extended boot signature (0x29) */
    uint32_t volume_serial;         /**< 0x43: Volume serial number */
    char     volume_label[11];      /**< 0x47: Volume label */
    char     fs_type[8];            /**< 0x52: "FAT32   " */
    uint8_t  boot_code[420];        /**< 0x5A: Boot code */
    uint16_t signature;             /**< 0x1FE: Boot signature (0xAA55) */
} uft_fat32_bootsect_t;
UFT_PACK_END

/*===========================================================================
 * FSInfo Sector
 *===========================================================================*/

/**
 * @brief FAT32 FSInfo Sector Structure
 * 
 * Contains hints about free space for faster allocation
 */
UFT_PACK_BEGIN
typedef struct {
    uint32_t lead_sig;              /**< 0x00: Lead signature (0x41615252) */
    uint8_t  reserved1[480];        /**< 0x04: Reserved */
    uint32_t struct_sig;            /**< 0x1E4: Structure signature (0x61417272) */
    uint32_t free_count;            /**< 0x1E8: Free cluster count (0xFFFFFFFF=unknown) */
    uint32_t next_free;             /**< 0x1EC: Next free cluster hint */
    uint8_t  reserved2[12];         /**< 0x1F0: Reserved */
    uint32_t trail_sig;             /**< 0x1FC: Trail signature (0xAA550000) */
} uft_fat32_fsinfo_t;
UFT_PACK_END

/*===========================================================================
 * FAT32 Context Extension
 *===========================================================================*/

/**
 * @brief FAT32-specific volume information
 */
typedef struct {
    uint32_t fat_size_32;           /**< Sectors per FAT */
    uint32_t root_cluster;          /**< Root directory cluster */
    uint16_t fsinfo_sector;         /**< FSInfo sector location */
    uint16_t backup_boot;           /**< Backup boot sector location */
    uint32_t free_count;            /**< Cached free cluster count */
    uint32_t next_free;             /**< Next free cluster hint */
    bool     fsinfo_valid;          /**< FSInfo is valid */
    bool     fsinfo_dirty;          /**< FSInfo needs update */
} uft_fat32_info_t;

/**
 * @brief FAT32 format options
 */
typedef struct {
    uint32_t volume_size;           /**< Target volume size in bytes */
    uint16_t sector_size;           /**< Sector size (512/1024/2048/4096) */
    uint8_t  sectors_per_cluster;   /**< Cluster size (0=auto) */
    uint16_t reserved_sectors;      /**< Reserved sectors (0=default 32) */
    uint8_t  num_fats;              /**< Number of FATs (1 or 2) */
    uint16_t backup_boot;           /**< Backup boot sector (0=6) */
    char     volume_label[12];      /**< Volume label */
    uint32_t volume_serial;         /**< Serial number (0=random) */
    char     oem_name[9];           /**< OEM name */
    bool     align_structures;      /**< Align to cluster boundary */
} uft_fat32_format_opts_t;

/*===========================================================================
 * FAT32 API - Detection
 *===========================================================================*/

/**
 * @brief Check if image is FAT32
 * @param data Image data
 * @param size Image size
 * @return true if FAT32 detected
 */
bool uft_fat32_detect(const uint8_t *data, size_t size);

/**
 * @brief Get FAT32 boot sector
 * @param data Image data
 * @return Pointer to boot sector or NULL
 */
const uft_fat32_bootsect_t *uft_fat32_get_boot(const uint8_t *data);

/**
 * @brief Validate FAT32 parameters
 * @param boot Boot sector
 * @return true if valid FAT32
 */
bool uft_fat32_validate(const uft_fat32_bootsect_t *boot);

/*===========================================================================
 * FAT32 API - Formatting
 *===========================================================================*/

/**
 * @brief Calculate optimal FAT32 parameters
 * @param size Volume size in bytes
 * @param opts Output options (pre-filled with defaults)
 * @return 0 on success, error code otherwise
 */
int uft_fat32_calc_params(uint64_t size, uft_fat32_format_opts_t *opts);

/**
 * @brief Format image as FAT32
 * @param data Image buffer (will be modified)
 * @param size Buffer size
 * @param opts Format options
 * @return 0 on success
 */
int uft_fat32_format(uint8_t *data, size_t size, const uft_fat32_format_opts_t *opts);

/**
 * @brief Initialize default format options
 * @param opts Options structure to initialize
 */
void uft_fat32_format_opts_init(uft_fat32_format_opts_t *opts);

/*===========================================================================
 * FAT32 API - FSInfo Management
 *===========================================================================*/

/**
 * @brief Read FSInfo sector
 * @param data Image data
 * @param boot Boot sector
 * @param info Output FSInfo
 * @return 0 on success
 */
int uft_fat32_read_fsinfo(const uint8_t *data, const uft_fat32_bootsect_t *boot,
                          uft_fat32_fsinfo_t *info);

/**
 * @brief Write FSInfo sector
 * @param data Image data (modified)
 * @param boot Boot sector
 * @param info FSInfo to write
 * @return 0 on success
 */
int uft_fat32_write_fsinfo(uint8_t *data, const uft_fat32_bootsect_t *boot,
                           const uft_fat32_fsinfo_t *info);

/**
 * @brief Update FSInfo from FAT scan
 * @param data Image data (modified)
 * @param boot Boot sector
 * @return 0 on success
 */
int uft_fat32_update_fsinfo(uint8_t *data, const uft_fat32_bootsect_t *boot);

/*===========================================================================
 * FAT32 API - FAT Operations
 *===========================================================================*/

/**
 * @brief Get FAT32 entry
 * @param data Image data
 * @param boot Boot sector
 * @param cluster Cluster number
 * @return Next cluster or special value
 */
uint32_t uft_fat32_get_entry(const uint8_t *data, const uft_fat32_bootsect_t *boot,
                             uint32_t cluster);

/**
 * @brief Set FAT32 entry
 * @param data Image data (modified)
 * @param boot Boot sector
 * @param cluster Cluster number
 * @param value New value
 * @return 0 on success
 */
int uft_fat32_set_entry(uint8_t *data, const uft_fat32_bootsect_t *boot,
                        uint32_t cluster, uint32_t value);

/**
 * @brief Check if cluster value indicates EOF
 */
static inline bool uft_fat32_is_eof(uint32_t value) {
    return (value & UFT_FAT32_CLUSTER_MASK) >= UFT_FAT32_EOF_MIN;
}

/**
 * @brief Check if cluster is free
 */
static inline bool uft_fat32_is_free(uint32_t value) {
    return (value & UFT_FAT32_CLUSTER_MASK) == UFT_FAT32_FREE;
}

/**
 * @brief Check if cluster is bad
 */
static inline bool uft_fat32_is_bad(uint32_t value) {
    return (value & UFT_FAT32_CLUSTER_MASK) == UFT_FAT32_BAD;
}

/*===========================================================================
 * FAT32 API - Cluster Operations
 *===========================================================================*/

/**
 * @brief Get data region offset
 * @param boot Boot sector
 * @return Byte offset of data region
 */
uint64_t uft_fat32_data_offset(const uft_fat32_bootsect_t *boot);

/**
 * @brief Get cluster offset
 * @param boot Boot sector
 * @param cluster Cluster number
 * @return Byte offset of cluster
 */
uint64_t uft_fat32_cluster_offset(const uft_fat32_bootsect_t *boot, uint32_t cluster);

/**
 * @brief Get cluster size in bytes
 */
static inline uint32_t uft_fat32_cluster_size(const uft_fat32_bootsect_t *boot) {
    return boot->bytes_per_sector * boot->sectors_per_cluster;
}

/**
 * @brief Count total data clusters
 */
uint32_t uft_fat32_count_clusters(const uft_fat32_bootsect_t *boot);

/**
 * @brief Allocate cluster chain
 * @param data Image data (modified)
 * @param boot Boot sector
 * @param count Number of clusters needed
 * @param first_cluster Output: first allocated cluster
 * @return 0 on success
 */
int uft_fat32_alloc_chain(uint8_t *data, const uft_fat32_bootsect_t *boot,
                          uint32_t count, uint32_t *first_cluster);

/**
 * @brief Free cluster chain
 * @param data Image data (modified)
 * @param boot Boot sector
 * @param start First cluster of chain
 * @return Number of clusters freed
 */
uint32_t uft_fat32_free_chain(uint8_t *data, const uft_fat32_bootsect_t *boot,
                              uint32_t start);

/*===========================================================================
 * FAT32 API - Backup Boot Sector
 *===========================================================================*/

/**
 * @brief Copy boot sector to backup location
 * @param data Image data (modified)
 * @param boot Boot sector
 * @return 0 on success
 */
int uft_fat32_write_backup_boot(uint8_t *data, const uft_fat32_bootsect_t *boot);

/**
 * @brief Restore boot sector from backup
 * @param data Image data (modified)
 * @return 0 on success
 */
int uft_fat32_restore_from_backup(uint8_t *data);

/**
 * @brief Compare boot sector with backup
 * @param data Image data
 * @return true if identical
 */
bool uft_fat32_compare_backup(const uint8_t *data);

/*===========================================================================
 * FAT32 API - Conversion
 *===========================================================================*/

/**
 * @brief Determine appropriate FAT type for size
 * @param size Volume size in bytes
 * @return FAT type (12, 16, or 32)
 */
uft_fat_type_t uft_fat_type_for_size(uint64_t size);

/**
 * @brief Get recommended cluster size for FAT32
 * @param size Volume size in bytes
 * @return Recommended sectors per cluster
 */
uint8_t uft_fat32_recommended_spc(uint64_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT32_H */
