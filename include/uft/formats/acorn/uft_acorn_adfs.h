/**
 * @file uft_acorn_adfs.h
 * @brief Acorn ADFS / Archimedes Disk Format
 * 
 * Based on arcimage by Jasper Renow-Clarke
 * 
 * Supports Acorn Archimedes D/E/F format disks:
 *   D format: 800KB (80 tracks, 2 heads, 5 sectors/track, 1024 bytes/sector)
 *   E format: 800KB (same geometry as D, different directory structure)
 *   F format: 1.6MB (80 tracks, 2 heads, 10 sectors/track, 1024 bytes/sector)
 */

#ifndef UFT_ACORN_ADFS_H
#define UFT_ACORN_ADFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Archimedes Disk Geometry
 *===========================================================================*/

/** D/E Format: 800KB */
#define UFT_ACORN_DE_TRACKS         80
#define UFT_ACORN_DE_HEADS          2
#define UFT_ACORN_DE_SECTORS        5
#define UFT_ACORN_DE_SECTOR_SIZE    1024
#define UFT_ACORN_DE_TRACK_SIZE     (UFT_ACORN_DE_SECTORS * UFT_ACORN_DE_SECTOR_SIZE)
#define UFT_ACORN_DE_TOTAL_SIZE     (UFT_ACORN_DE_TRACKS * UFT_ACORN_DE_HEADS * UFT_ACORN_DE_TRACK_SIZE)

/** F Format: 1.6MB */
#define UFT_ACORN_F_TRACKS          80
#define UFT_ACORN_F_HEADS           2
#define UFT_ACORN_F_SECTORS         10
#define UFT_ACORN_F_SECTOR_SIZE     1024
#define UFT_ACORN_F_TRACK_SIZE      (UFT_ACORN_F_SECTORS * UFT_ACORN_F_SECTOR_SIZE)
#define UFT_ACORN_F_TOTAL_SIZE      (UFT_ACORN_F_TRACKS * UFT_ACORN_F_HEADS * UFT_ACORN_F_TRACK_SIZE)

/** L Format: 640KB (Archimedes DOS-compatible) */
#define UFT_ACORN_L_TRACKS          80
#define UFT_ACORN_L_HEADS           2
#define UFT_ACORN_L_SECTORS         16
#define UFT_ACORN_L_SECTOR_SIZE     256
#define UFT_ACORN_L_TRACK_SIZE      (UFT_ACORN_L_SECTORS * UFT_ACORN_L_SECTOR_SIZE)

/*===========================================================================
 * ADFS Directory Structure
 *===========================================================================*/

/** Directory magic bytes */
#define UFT_ADFS_DIR_MAGIC_HUGO     0x6F677548  /**< "Hugo" - old format */
#define UFT_ADFS_DIR_MAGIC_NICK     0x6B63694E  /**< "Nick" - new format */

/** Maximum filename length */
#define UFT_ADFS_MAX_FILENAME       10

/** Maximum directory entries */
#define UFT_ADFS_MAX_DIR_ENTRIES    47

/** File attributes */
#define UFT_ADFS_ATTR_READ_OWNER    0x01
#define UFT_ADFS_ATTR_WRITE_OWNER   0x02
#define UFT_ADFS_ATTR_LOCKED        0x04
#define UFT_ADFS_ATTR_DIRECTORY     0x08
#define UFT_ADFS_ATTR_READ_PUBLIC   0x10
#define UFT_ADFS_ATTR_WRITE_PUBLIC  0x20

/**
 * @brief ADFS directory entry (26 bytes in old format)
 */
typedef struct __attribute__((packed)) {
    char     name[10];              /**< Filename (space-padded, top bit = dir flag) */
    uint32_t load_addr;             /**< Load address */
    uint32_t exec_addr;             /**< Execution address */
    uint32_t length;                /**< File length */
    uint8_t  sector_low;            /**< Start sector (low byte) */
    uint8_t  sector_mid;            /**< Start sector (middle byte) */
    uint8_t  sector_high_attr;      /**< Start sector high (bits 0-1) + attributes (bits 2-7) */
} uft_adfs_old_dirent_t;

/**
 * @brief ADFS New Directory entry (Big Directory format)
 */
typedef struct __attribute__((packed)) {
    uint32_t load_addr;             /**< Load address */
    uint32_t exec_addr;             /**< Execution address */
    uint32_t length;                /**< File length */
    uint32_t indirect_addr;         /**< Indirect disc address */
    uint32_t attributes;            /**< Attributes (new format) */
    uint32_t name_len;              /**< Name length */
    /* Variable length name follows */
} uft_adfs_new_dirent_t;

/**
 * @brief ADFS Old Directory header
 */
typedef struct __attribute__((packed)) {
    uint8_t  master_seq;            /**< Master sequence number */
    char     dir_name[10];          /**< Directory name */
    uint32_t parent_sector;         /**< Parent directory sector (3 bytes) + seq */
} uft_adfs_old_dir_header_t;

/**
 * @brief ADFS Old Directory tail
 */
typedef struct __attribute__((packed)) {
    uint8_t  last_entry;            /**< Last entry marker (0) */
    char     dir_name[10];          /**< Directory name (copy) */
    uint8_t  parent_high;           /**< Parent sector high byte */
    char     dir_title[19];         /**< Directory title */
    uint8_t  reserved[14];          /**< Reserved */
    uint8_t  end_marker;            /**< End marker */
    uint8_t  checksum;              /**< Directory checksum */
} uft_adfs_old_dir_tail_t;

/*===========================================================================
 * ADFS Boot Block / Free Space Map
 *===========================================================================*/

/**
 * @brief ADFS Free Space Map (old format, sectors 0-1)
 */
typedef struct __attribute__((packed)) {
    uint8_t  free_start[82 * 3];    /**< Free space start pointers (3 bytes each) */
    uint8_t  reserved1[4];
    char     disc_name[10];         /**< Disc name */
    uint8_t  disc_size[3];          /**< Disc size in sectors */
    uint8_t  check0;                /**< Checksum byte 0 */
    uint8_t  free_end[82 * 3];      /**< Free space end pointers (3 bytes each) */
    uint8_t  reserved2[4];
    char     disc_id[2];            /**< Disc identifier */
    uint8_t  boot_option;           /**< Boot option */
    uint8_t  free_end_ptr;          /**< Free space end pointer */
    uint8_t  check1;                /**< Checksum byte 1 */
} uft_adfs_free_space_map_t;

/*===========================================================================
 * Disc Record (E+ format)
 *===========================================================================*/

/**
 * @brief ADFS Disc Record (for E+ and F formats)
 */
typedef struct __attribute__((packed)) {
    uint8_t  log2_sector_size;      /**< Log2 of sector size */
    uint8_t  sectors_per_track;     /**< Sectors per track */
    uint8_t  heads;                 /**< Number of heads (surfaces) */
    uint8_t  density;               /**< Recording density */
    uint8_t  id_len;                /**< Length of id field (defect list) */
    uint8_t  log2_bytes_per_map;    /**< Log2 bytes per map bit */
    uint8_t  skew;                  /**< Track-to-track skew */
    uint8_t  boot_option;           /**< Boot option */
    uint8_t  low_sector;            /**< Lowest sector number */
    uint8_t  zones;                 /**< Number of zones */
    uint16_t zone_spare;            /**< Zone spare bits */
    uint32_t root_dir;              /**< Root directory address */
    uint32_t disc_size;             /**< Disc size in bytes */
    uint16_t disc_id;               /**< Disc cycle id */
    char     disc_name[10];         /**< Disc name */
    uint32_t disc_type;             /**< Disc type */
    uint32_t disc_size_high;        /**< Disc size high word (for large discs) */
    uint8_t  share_size;            /**< Share size */
    uint8_t  big_flag;              /**< Big flag */
    uint8_t  reserved[18];          /**< Reserved */
} uft_adfs_disc_record_t;

/*===========================================================================
 * Density Values
 *===========================================================================*/

#define UFT_ADFS_DENSITY_SINGLE     0   /**< Single density (FM) */
#define UFT_ADFS_DENSITY_DOUBLE     1   /**< Double density (MFM) */
#define UFT_ADFS_DENSITY_DOUBLE_P   2   /**< Double+ density */
#define UFT_ADFS_DENSITY_QUAD       3   /**< Quad density */
#define UFT_ADFS_DENSITY_OCTAL      4   /**< Octal density */

/*===========================================================================
 * Sector Offset Calculation
 *===========================================================================*/

/**
 * @brief Calculate sector offset in D/E format image
 * 
 * D/E format interleaving:
 *   Logical sector = (track * 10) + (head * 5) + sector
 *   Physical layout: Track 0 Head 0, Track 0 Head 1, Track 1 Head 0, ...
 */
static inline uint32_t uft_acorn_de_sector_offset(uint8_t track, uint8_t head, 
                                                   uint8_t sector)
{
    uint32_t logical_sector = (track * 10) + (head * 5) + sector;
    return logical_sector * UFT_ACORN_DE_SECTOR_SIZE;
}

/**
 * @brief Calculate sector offset in F format image
 */
static inline uint32_t uft_acorn_f_sector_offset(uint8_t track, uint8_t head,
                                                  uint8_t sector)
{
    uint32_t logical_sector = (track * 20) + (head * 10) + sector;
    return logical_sector * UFT_ACORN_F_SECTOR_SIZE;
}

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Detect Acorn disc format from image data
 * @param data Image data (at least first 2KB)
 * @param size Image size
 * @return 'D', 'E', 'F', 'L' or 0 if unknown
 */
char uft_adfs_detect_format(const uint8_t *data, size_t size);

/**
 * @brief Validate ADFS disc structure
 */
bool uft_adfs_validate(const uint8_t *data, size_t size);

/**
 * @brief Calculate old directory checksum
 * @param dir Directory data (2048 bytes)
 * @return Checksum byte
 */
uint8_t uft_adfs_dir_checksum(const uint8_t *dir);

/**
 * @brief Calculate free space map checksum
 * @param map Map sector data (256 bytes)
 * @return Checksum byte
 */
uint8_t uft_adfs_map_checksum(const uint8_t *map);

/**
 * @brief Extract filename from old directory entry
 * @param dirent Directory entry
 * @param buf Output buffer (at least 11 bytes)
 * @return Filename length
 */
int uft_adfs_get_filename(const uft_adfs_old_dirent_t *dirent, char *buf);

/**
 * @brief Get start sector from old directory entry
 */
static inline uint32_t uft_adfs_get_sector(const uft_adfs_old_dirent_t *dirent)
{
    return dirent->sector_low | 
           (dirent->sector_mid << 8) |
           ((dirent->sector_high_attr & 0x03) << 16);
}

/**
 * @brief Get attributes from old directory entry
 */
static inline uint8_t uft_adfs_get_attr(const uft_adfs_old_dirent_t *dirent)
{
    return (dirent->sector_high_attr >> 2) & 0x3F;
}

/**
 * @brief Check if entry is a directory (old format)
 */
static inline bool uft_adfs_is_directory(const uft_adfs_old_dirent_t *dirent)
{
    /* Top bit of first filename char indicates directory */
    return (dirent->name[0] & 0x80) != 0;
}

/**
 * @brief Read 24-bit little-endian value
 */
static inline uint32_t uft_read_le24(const uint8_t *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_ACORN_ADFS_H */
