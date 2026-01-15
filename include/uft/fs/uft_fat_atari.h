/**
 * @file uft_fat_atari.h
 * @brief Atari ST FAT Filesystem Variants
 * @version 1.0.0
 * 
 * Atari ST/TT/Falcon-specific FAT handling:
 * - GEMDOS boot sector format
 * - Atari serial number generation
 * - TOS-compatible formatting
 * - Logical sector size handling (up to 8192)
 * - AHDI partition table support
 * - BigDOS/BIGFAT extensions
 * 
 * Based on mkfs.fat Atari mode (-A flag)
 * 
 * @note Part of UnifiedFloppyTool preservation suite
 */

#ifndef UFT_FAT_ATARI_H
#define UFT_FAT_ATARI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_compiler.h"
#include "uft/fs/uft_fat12.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Atari Constants
 *===========================================================================*/

/** Atari GEMDOS maximum sectors (16-bit limitation) */
#define UFT_ATARI_MAX_SECTORS       65535

/** Atari logical sector sizes */
#define UFT_ATARI_SECTOR_512        512
#define UFT_ATARI_SECTOR_1024       1024
#define UFT_ATARI_SECTOR_2048       2048
#define UFT_ATARI_SECTOR_4096       4096
#define UFT_ATARI_SECTOR_8192       8192

/** Atari preferred sectors per cluster */
#define UFT_ATARI_DEFAULT_SPC       2

/** Serial number flag for Atari format */
#define UFT_ATARI_SERIAL_FLAG       0x01000000

/** Atari boot sector checksum target */
#define UFT_ATARI_BOOT_CHECKSUM     0x1234

/*===========================================================================
 * Atari Disk Formats
 *===========================================================================*/

/**
 * @brief Standard Atari ST floppy formats
 */
typedef enum {
    UFT_ATARI_FMT_UNKNOWN = 0,
    UFT_ATARI_FMT_SS_DD_9,          /**< SS/DD 9 sectors = 360KB */
    UFT_ATARI_FMT_DS_DD_9,          /**< DS/DD 9 sectors = 720KB */
    UFT_ATARI_FMT_DS_DD_10,         /**< DS/DD 10 sectors = 800KB */
    UFT_ATARI_FMT_DS_DD_11,         /**< DS/DD 11 sectors = 880KB (Twister) */
    UFT_ATARI_FMT_DS_HD_18,         /**< DS/HD 18 sectors = 1.44MB */
    UFT_ATARI_FMT_DS_ED_36,         /**< DS/ED 36 sectors = 2.88MB */
    UFT_ATARI_FMT_CUSTOM            /**< Custom format */
} uft_atari_format_t;

/**
 * @brief Atari format geometry
 */
typedef struct {
    const char *name;               /**< Format name */
    uft_atari_format_t type;        /**< Format type */
    uint16_t sectors;               /**< Total sectors */
    uint8_t  spt;                   /**< Sectors per track */
    uint8_t  sides;                 /**< Number of sides */
    uint8_t  tracks;                /**< Number of tracks */
    uint16_t dir_entries;           /**< Root directory entries */
    uint8_t  fat_sectors;           /**< Sectors per FAT */
    uint8_t  spc;                   /**< Sectors per cluster */
    uint8_t  media;                 /**< Media descriptor */
    bool     is_standard;           /**< TOS-standard format */
} uft_atari_geometry_t;

/*===========================================================================
 * Atari Boot Sector
 *===========================================================================*/

/**
 * @brief Atari ST Boot Sector Structure
 * 
 * Atari uses a slightly different boot sector layout.
 * Some PC fields are not used, checksum is required for bootable disks.
 */
UFT_PACK_BEGIN
typedef struct {
    uint16_t bra;                   /**< 0x00: Branch to boot code (68000: BRA.S) */
    char     oem[6];                /**< 0x02: OEM/Loader name */
    uint8_t  serial[3];             /**< 0x08: 24-bit serial number */
    uint16_t bytes_per_sector;      /**< 0x0B: Bytes per sector */
    uint8_t  sectors_per_cluster;   /**< 0x0D: Sectors per cluster */
    uint16_t reserved_sectors;      /**< 0x0E: Reserved sectors */
    uint8_t  num_fats;              /**< 0x10: Number of FATs */
    uint16_t root_entries;          /**< 0x11: Root directory entries */
    uint16_t total_sectors;         /**< 0x13: Total sectors */
    uint8_t  media_type;            /**< 0x15: Media descriptor */
    uint16_t fat_sectors;           /**< 0x16: Sectors per FAT */
    uint16_t sectors_per_track;     /**< 0x18: Sectors per track */
    uint16_t num_heads;             /**< 0x1A: Number of heads */
    uint16_t hidden_sectors;        /**< 0x1C: Hidden sectors (16-bit!) */
    uint8_t  boot_code[480];        /**< 0x1E: Boot code */
    uint16_t checksum;              /**< 0x1FE: Boot checksum (for bootable) */
} uft_atari_bootsect_t;
UFT_PACK_END

/*===========================================================================
 * AHDI Partition Table
 *===========================================================================*/

/** AHDI partition entry */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  flag;                  /**< Partition flag (0x01 = exists, 0x81 = bootable) */
    char     id[3];                 /**< Partition type ID */
    uint32_t start;                 /**< Start sector */
    uint32_t size;                  /**< Size in sectors */
} uft_ahdi_part_t;
UFT_PACK_END

/** AHDI root sector */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  boot_code[0x1B6];      /**< Boot code area */
    uft_ahdi_part_t parts[4];       /**< Four partition entries */
    uint32_t bad_sector_list;       /**< Bad sector list start */
    uint32_t bad_sector_count;      /**< Number of bad sectors */
    uint16_t checksum;              /**< Checksum */
} uft_ahdi_root_t;
UFT_PACK_END

/** Partition type IDs */
#define UFT_AHDI_TYPE_GEM           "GEM"   /**< TOS partition < 16MB */
#define UFT_AHDI_TYPE_BGM           "BGM"   /**< TOS partition >= 16MB */
#define UFT_AHDI_TYPE_XGM           "XGM"   /**< Extended partition */
#define UFT_AHDI_TYPE_RAW           "RAW"   /**< Raw/unformatted */

/*===========================================================================
 * API - Serial Number
 *===========================================================================*/

/**
 * @brief Generate Atari-compatible serial number
 * @return 24-bit serial number
 * 
 * Atari serial numbers are based on:
 * - Current date/time
 * - Random component
 * - High byte indicates Atari format (0x01)
 */
uint32_t uft_atari_generate_serial(void);

/**
 * @brief Generate Atari serial from timestamp
 * @param timestamp Unix timestamp
 * @return 24-bit serial number
 */
uint32_t uft_atari_serial_from_time(uint32_t timestamp);

/**
 * @brief Check if serial number is Atari-style
 * @param serial Serial number
 * @return true if Atari format
 */
bool uft_atari_is_atari_serial(uint32_t serial);

/**
 * @brief Set serial number in Atari boot sector
 * @param boot Boot sector (modified)
 * @param serial 24-bit serial number
 */
void uft_atari_set_serial(uft_atari_bootsect_t *boot, uint32_t serial);

/**
 * @brief Get serial number from Atari boot sector
 * @param boot Boot sector
 * @return 24-bit serial number
 */
uint32_t uft_atari_get_serial(const uft_atari_bootsect_t *boot);

/*===========================================================================
 * API - Boot Sector Checksum
 *===========================================================================*/

/**
 * @brief Calculate Atari boot sector checksum
 * @param boot Boot sector (256 words)
 * @return Calculated checksum
 */
uint16_t uft_atari_calc_checksum(const uft_atari_bootsect_t *boot);

/**
 * @brief Make boot sector bootable (set correct checksum)
 * @param boot Boot sector (modified)
 * 
 * Sets checksum field so total of all words equals 0x1234
 */
void uft_atari_make_bootable(uft_atari_bootsect_t *boot);

/**
 * @brief Make boot sector non-bootable
 * @param boot Boot sector (modified)
 * 
 * Sets checksum to invalid value
 */
void uft_atari_make_non_bootable(uft_atari_bootsect_t *boot);

/**
 * @brief Check if boot sector is bootable
 * @param boot Boot sector
 * @return true if checksum is valid (0x1234)
 */
bool uft_atari_is_bootable(const uft_atari_bootsect_t *boot);

/*===========================================================================
 * API - Format Detection
 *===========================================================================*/

/**
 * @brief Detect if image is Atari format
 * @param data Image data
 * @param size Image size
 * @return true if Atari ST format detected
 */
bool uft_atari_detect(const uint8_t *data, size_t size);

/**
 * @brief Identify Atari disk format
 * @param data Image data
 * @param size Image size
 * @return Format type
 */
uft_atari_format_t uft_atari_identify_format(const uint8_t *data, size_t size);

/**
 * @brief Get format geometry
 * @param format Format type
 * @return Geometry info or NULL
 */
const uft_atari_geometry_t *uft_atari_get_geometry(uft_atari_format_t format);

/**
 * @brief Get geometry from image size
 * @param size Image size in bytes
 * @return Best matching geometry or NULL
 */
const uft_atari_geometry_t *uft_atari_geometry_from_size(size_t size);

/*===========================================================================
 * API - Formatting
 *===========================================================================*/

/**
 * @brief Format image as Atari ST disk
 * @param data Image buffer (modified)
 * @param size Buffer size
 * @param format Desired format
 * @param label Volume label (max 11 chars)
 * @return 0 on success
 */
int uft_atari_format(uint8_t *data, size_t size, uft_atari_format_t format,
                     const char *label);

/**
 * @brief Format with custom geometry
 * @param data Image buffer (modified)
 * @param size Buffer size
 * @param geom Custom geometry
 * @param label Volume label
 * @return 0 on success
 */
int uft_atari_format_custom(uint8_t *data, size_t size,
                            const uft_atari_geometry_t *geom,
                            const char *label);

/**
 * @brief Calculate logical sector size for large volumes
 * @param total_size Volume size in bytes
 * @return Required logical sector size (512-8192)
 * 
 * GEMDOS is limited to 16-bit sector numbers, so large volumes
 * require larger logical sector sizes.
 */
uint16_t uft_atari_calc_sector_size(uint64_t total_size);

/**
 * @brief Convert PC FAT to Atari format
 * @param data Image data (modified)
 * @param size Image size
 * @return 0 on success
 * 
 * Updates boot sector to Atari conventions:
 * - Sets Atari serial number
 * - Ensures 2 sectors per cluster
 * - Removes PC-specific boot code
 */
int uft_atari_convert_from_pc(uint8_t *data, size_t size);

/*===========================================================================
 * API - AHDI Partitions
 *===========================================================================*/

/**
 * @brief Check for AHDI partition table
 * @param data Disk image data
 * @param size Image size
 * @return true if AHDI partitioned
 */
bool uft_ahdi_detect(const uint8_t *data, size_t size);

/**
 * @brief Get AHDI partition table
 * @param data Disk image data
 * @return Pointer to root sector or NULL
 */
const uft_ahdi_root_t *uft_ahdi_get_root(const uint8_t *data);

/**
 * @brief Count AHDI partitions
 * @param root AHDI root sector
 * @return Number of valid partitions
 */
int uft_ahdi_count_partitions(const uft_ahdi_root_t *root);

/**
 * @brief Get partition info
 * @param root AHDI root sector
 * @param index Partition index (0-3)
 * @param start Output: start sector
 * @param size Output: size in sectors
 * @param type Output: type string (4 bytes)
 * @return 0 on success
 */
int uft_ahdi_get_partition(const uft_ahdi_root_t *root, int index,
                           uint32_t *start, uint32_t *size, char *type);

/*===========================================================================
 * Standard Atari Geometries
 *===========================================================================*/

/** Table of standard Atari formats */
extern const uft_atari_geometry_t uft_atari_std_formats[];

/** Number of standard formats */
extern const size_t uft_atari_std_format_count;

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT_ATARI_H */
