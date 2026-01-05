/**
 * @file uft_atari_dos.h
 * @brief Atari DOS Filesystem Support
 * 
 * Based on:
 * - mkatr by Daniel Serpell (GPL-2.0)
 * - atari-tools by Joseph H. Allen (GPL)
 * 
 * Supports:
 * - DOS 1.0 (SD only, 720 sectors)
 * - DOS 2.0S (Single Density)
 * - DOS 2.0D (Double Density)
 * - DOS 2.5 (Enhanced Density)
 * - MyDOS (Large disk support)
 * - SpartaDOS (Hierarchical directories)
 * - LiteDOS (Lightweight DOS)
 * - Bibo-DOS (DD variant)
 */

#ifndef UFT_ATARI_DOS_H
#define UFT_ATARI_DOS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * DOS 2.x Filesystem Layout
 *===========================================================================*/

/** Critical sector numbers */
#define UFT_DOS_BOOT_START       1       /**< Boot sectors start */
#define UFT_DOS_BOOT_END         3       /**< Boot sectors end */
#define UFT_DOS_VTOC_SECTOR      360     /**< Volume Table of Contents */
#define UFT_DOS_VTOC2_SECTOR     1024    /**< VTOC2 for ED (DOS 2.5) */
#define UFT_DOS_DIR_START        361     /**< Directory start sector */
#define UFT_DOS_DIR_END          368     /**< Directory end sector */
#define UFT_DOS_DIR_SECTORS      8       /**< Directory sector count */

/** Directory limits */
#define UFT_DOS_MAX_FILES        64      /**< Max files per directory */
#define UFT_DOS_ENTRY_SIZE       16      /**< Bytes per directory entry */
#define UFT_DOS_ENTRIES_PER_SEC  8       /**< Entries per sector (SD) */
#define UFT_DOS_FILENAME_LEN     8       /**< Filename length */
#define UFT_DOS_EXT_LEN          3       /**< Extension length */

/** Data sector structure */
#define UFT_DOS_SD_DATA_BYTES    125     /**< Data bytes per SD sector */
#define UFT_DOS_DD_DATA_BYTES    253     /**< Data bytes per DD sector */

/*===========================================================================
 * DOS Versions and Signatures
 *===========================================================================*/

typedef enum {
    UFT_DOS_VER_UNKNOWN = 0,
    UFT_DOS_VER_1,              /**< DOS 1.0 */
    UFT_DOS_VER_2S,             /**< DOS 2.0S (Single Density) */
    UFT_DOS_VER_2D,             /**< DOS 2.0D (Double Density) */
    UFT_DOS_VER_25,             /**< DOS 2.5 (Enhanced Density) */
    UFT_DOS_VER_MYDOS,          /**< MyDOS (Large disks) */
    UFT_DOS_VER_SPARTA,         /**< SpartaDOS */
    UFT_DOS_VER_LITEDOS,        /**< LiteDOS */
    UFT_DOS_VER_LITEDOS_SE,     /**< LiteDOS-SE */
    UFT_DOS_VER_BIBO            /**< Bibo-DOS */
} uft_dos_version_t;

/** VTOC signature values */
#define UFT_DOS_SIG_DOS1         1       /**< DOS 1.0 */
#define UFT_DOS_SIG_DOS2         2       /**< DOS 2.x */
#define UFT_DOS_SIG_LITEDOS      0x80    /**< LiteDOS marker */
#define UFT_DOS_SIG_LITEDOS_SE   0x40    /**< LiteDOS-SE marker */

/*===========================================================================
 * Directory Entry Flags
 *===========================================================================*/

/** Directory entry flag bits */
#define UFT_DOS_FLAG_OPENED      0x01    /**< File opened for output */
#define UFT_DOS_FLAG_DOS2        0x02    /**< Created by DOS 2.x */
#define UFT_DOS_FLAG_MYDOS       0x04    /**< MyDOS extended */
#define UFT_DOS_FLAG_SUBDIR      0x10    /**< Subdirectory (MyDOS/Sparta) */
#define UFT_DOS_FLAG_LOCKED      0x20    /**< File locked */
#define UFT_DOS_FLAG_IN_USE      0x40    /**< Entry in use */
#define UFT_DOS_FLAG_DELETED     0x80    /**< Entry deleted */

/** Combined flags */
#define UFT_DOS_FLAG_NEVER_USED  0x00    /**< Empty slot */
#define UFT_DOS_FLAG_VALID       0x42    /**< Normal DOS 2 file */
#define UFT_DOS_FLAG_VALID_ED    0x43    /**< Enhanced density file */

/*===========================================================================
 * VTOC Structure (Volume Table of Contents)
 *===========================================================================*/

/**
 * @brief DOS 2.x VTOC structure (sector 360)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  dos_code;          /**< DOS version signature */
    uint8_t  alloc_lo;          /**< Total allocatable sectors (lo) */
    uint8_t  alloc_hi;          /**< Total allocatable sectors (hi) */
    uint8_t  free_lo;           /**< Free sectors (lo) */
    uint8_t  free_hi;           /**< Free sectors (hi) */
    uint8_t  unused[5];         /**< Unused bytes */
    uint8_t  bitmap[90];        /**< Sector allocation bitmap */
    /**< Bit = 0 means sector in use */
    /**< Bit = 1 means sector free */
    /**< Covers sectors 0-719 */
    uint8_t  reserved[28];      /**< Reserved (to 128 bytes) */
} uft_dos_vtoc_t;
UFT_PACK_END

/**
 * @brief DOS 2.5 VTOC2 structure (sector 1024)
 * 
 * Extended bitmap for enhanced density disks.
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  bitmap_720[84];    /**< Bitmap mirror for sectors 48-719 */
    uint8_t  bitmap_ext[38];    /**< Bitmap for sectors 720-1023 */
    uint8_t  free_lo;           /**< Free sectors above 719 (lo) */
    uint8_t  free_hi;           /**< Free sectors above 719 (hi) */
    uint8_t  reserved[4];
} uft_dos_vtoc2_t;
UFT_PACK_END

/*===========================================================================
 * Directory Entry Structure
 *===========================================================================*/

/**
 * @brief DOS 2.x directory entry (16 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  flags;             /**< File flags */
    uint8_t  count_lo;          /**< Sector count (lo) */
    uint8_t  count_hi;          /**< Sector count (hi) */
    uint8_t  start_lo;          /**< Start sector (lo) */
    uint8_t  start_hi;          /**< Start sector (hi) */
    uint8_t  filename[8];       /**< Filename (space-padded) */
    uint8_t  extension[3];      /**< Extension (space-padded) */
} uft_dos_dirent_t;
UFT_PACK_END

/*===========================================================================
 * Data Sector Structure
 *===========================================================================*/

/**
 * @brief DOS 2.x data sector (128-byte)
 * 
 * File data is stored with a 3-byte link structure at the end.
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  data[125];         /**< File data bytes */
    uint8_t  link_hi;           /**< File# (bits 7-2), next sector (bits 1-0) */
    uint8_t  link_lo;           /**< Next sector (bits 7-0) */
    uint8_t  bytes_used;        /**< Bytes used in this sector (usually 125) */
} uft_dos_sector_sd_t;
UFT_PACK_END

/**
 * @brief DOS 2.x data sector (256-byte DD)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  data[253];         /**< File data bytes */
    uint8_t  link_hi;           /**< File# (bits 7-2), next sector (bits 1-0) */
    uint8_t  link_lo;           /**< Next sector (bits 7-0) */
    uint8_t  bytes_used;        /**< Bytes used in this sector (usually 253) */
} uft_dos_sector_dd_t;
UFT_PACK_END

/*===========================================================================
 * Sparta DOS Structures
 *===========================================================================*/

/** Sparta DOS superblock location */
#define UFT_SPARTA_SUPERBLOCK    1

/**
 * @brief Sparta DOS boot sector / superblock
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  boot_jmp;          /**< 0x00 = short jmp */
    uint8_t  boot_sectors;      /**< Boot sector count */
    uint8_t  load_addr_lo;      /**< Load address (lo) */
    uint8_t  load_addr_hi;      /**< Load address (hi) */
    uint8_t  init_addr_lo;      /**< Init address (lo) */
    uint8_t  init_addr_hi;      /**< Init address (hi) */
    uint8_t  dos_jmp[3];        /**< JMP to DOS */
    uint8_t  root_lo;           /**< Root directory (lo) */
    uint8_t  root_hi;           /**< Root directory (hi) */
    uint8_t  sectors_lo;        /**< Total sectors (lo) */
    uint8_t  sectors_hi;        /**< Total sectors (hi) */
    uint8_t  free_lo;           /**< Free sectors (lo) */
    uint8_t  free_hi;           /**< Free sectors (hi) */
    uint8_t  vtoc_count;        /**< VTOC sector count */
    uint8_t  vtoc_lo;           /**< First VTOC sector (lo) */
    uint8_t  vtoc_hi;           /**< First VTOC sector (hi) */
    uint8_t  vtoc_seq_lo;       /**< Sec dir next (lo) */
    uint8_t  vtoc_seq_hi;       /**< Sec dir next (hi) */
    uint8_t  vol_name[8];       /**< Volume name */
    uint8_t  track_count;       /**< Tracks per side */
    uint8_t  sec_size;          /**< Sector size: 0=128, 1=256, 2=512 */
    uint8_t  revision;          /**< Sparta revision */
    /* Boot code follows */
} uft_sparta_boot_t;
UFT_PACK_END

/** Sparta directory entry flags */
#define UFT_SPARTA_FLAG_LOCKED   0x01    /**< Locked */
#define UFT_SPARTA_FLAG_HIDDEN   0x02    /**< Hidden */
#define UFT_SPARTA_FLAG_ARCHIVE  0x04    /**< Archive */
#define UFT_SPARTA_FLAG_INUSE    0x08    /**< In use */
#define UFT_SPARTA_FLAG_DELETED  0x10    /**< Deleted */
#define UFT_SPARTA_FLAG_SUBDIR   0x20    /**< Subdirectory */
#define UFT_SPARTA_FLAG_OPENED   0x80    /**< Opened for write */

/**
 * @brief Sparta DOS directory entry (23 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  flags;             /**< Entry flags */
    uint8_t  sector_lo;         /**< Start sector map (lo) */
    uint8_t  sector_hi;         /**< Start sector map (hi) */
    uint8_t  size[3];           /**< File size (24-bit) */
    uint8_t  filename[8];       /**< Filename */
    uint8_t  extension[3];      /**< Extension */
    uint8_t  date[3];           /**< Date (day, month, year) */
    uint8_t  time[3];           /**< Time (hour, min, sec) */
} uft_sparta_dirent_t;
UFT_PACK_END

#define UFT_SPARTA_ENTRY_SIZE    23

/*===========================================================================
 * Helper Macros
 *===========================================================================*/

/** Extract file number from DOS sector link */
#define UFT_DOS_SECTOR_FILENUM(link_hi)  (((link_hi) >> 2) & 0x3F)

/** Extract next sector from DOS sector link */
#define UFT_DOS_SECTOR_NEXT(link_hi, link_lo) \
    ((uint16_t)(link_lo) | (((uint16_t)(link_hi) & 0x03) << 8))

/** Check if directory entry is in use */
#define UFT_DOS_ENTRY_IN_USE(flags) \
    (((flags) & UFT_DOS_FLAG_IN_USE) && !((flags) & UFT_DOS_FLAG_DELETED))

/** Get sector count from directory entry */
#define UFT_DOS_ENTRY_SECTORS(e) \
    ((uint16_t)(e)->count_lo | ((uint16_t)(e)->count_hi << 8))

/** Get start sector from directory entry */
#define UFT_DOS_ENTRY_START(e) \
    ((uint16_t)(e)->start_lo | ((uint16_t)(e)->start_hi << 8))

/** Read 16-bit little-endian value */
static inline uint16_t uft_dos_read16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

/** Write 16-bit little-endian value */
static inline void uft_dos_write16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/*===========================================================================
 * Filesystem API
 *===========================================================================*/

/**
 * @brief Detect DOS version from VTOC
 * @param vtoc VTOC sector data (128 bytes)
 * @param sector_count Total sectors in image
 * @param sector_size Sector size
 * @return Detected DOS version
 */
uft_dos_version_t uft_dos_detect_version(const uint8_t *vtoc,
                                          uint16_t sector_count,
                                          uint16_t sector_size);

/**
 * @brief Parse VTOC
 * @param data VTOC sector data
 * @param vtoc Output structure
 * @return 0 on success
 */
int uft_dos_parse_vtoc(const uint8_t *data, uft_dos_vtoc_t *vtoc);

/**
 * @brief Read directory entry
 * @param image Disk image data
 * @param sector_size Sector size
 * @param index Entry index (0-63)
 * @param entry Output entry
 * @return 0 on success
 */
int uft_dos_read_dirent(const uint8_t *image, uint16_t sector_size,
                        int index, uft_dos_dirent_t *entry);

/**
 * @brief Check if sector is free in VTOC bitmap
 * @param vtoc VTOC data
 * @param sector Sector number
 * @return true if sector is free
 */
static inline bool uft_dos_sector_free(const uft_dos_vtoc_t *vtoc, 
                                        uint16_t sector)
{
    if (sector >= 720) return false;
    int byte_idx = sector / 8;
    int bit_idx = 7 - (sector % 8);
    return (vtoc->bitmap[byte_idx] & (1 << bit_idx)) != 0;
}

/**
 * @brief Get free sector count from VTOC
 */
static inline uint16_t uft_dos_vtoc_free(const uft_dos_vtoc_t *vtoc)
{
    return uft_dos_read16(&vtoc->free_lo);
}

/**
 * @brief Get total allocatable sectors from VTOC
 */
static inline uint16_t uft_dos_vtoc_total(const uft_dos_vtoc_t *vtoc)
{
    return uft_dos_read16(&vtoc->alloc_lo);
}

/**
 * @brief Convert Atari filename to C string
 * @param filename 8-byte filename
 * @param extension 3-byte extension
 * @param buffer Output buffer (at least 13 bytes)
 * @return Pointer to buffer
 */
char *uft_dos_filename_to_str(const uint8_t *filename, 
                               const uint8_t *extension,
                               char *buffer);

/**
 * @brief Convert C string to Atari filename
 * @param str Input string (e.g., "FILE.TXT")
 * @param filename Output 8-byte filename
 * @param extension Output 3-byte extension
 * @return 0 on success
 */
int uft_dos_str_to_filename(const char *str, uint8_t *filename,
                             uint8_t *extension);

/**
 * @brief Read file data following sector chain
 * @param image Disk image
 * @param sector_size Sector size
 * @param start_sector First sector
 * @param max_sectors Maximum sectors to read
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param bytes_read Output: bytes actually read
 * @return 0 on success
 */
int uft_dos_read_file(const uint8_t *image, uint16_t sector_size,
                      uint16_t start_sector, uint16_t max_sectors,
                      uint8_t *buffer, size_t buffer_size,
                      size_t *bytes_read);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARI_DOS_H */
