/**
 * @file uft_imd.h
 * @brief ImageDisk (IMD) Format Support
 * @version 3.1.4.002
 *
 * IMD is Dave Dunfield's ImageDisk format for preserving floppy disk images
 * with full track metadata including:
 * - Recording mode (FM/MFM)
 * - Data rate (250/300/500 kbps)
 * - Sector interleave maps
 * - Deleted data marks
 * - CRC error flags
 * - Sector compression
 *
 * Reference: http://dunfield.classiccmp.org/img/index.htm
 *
 * Based on SIMH sim_imd by Howard M. Harte (MIT License)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_IMD_H
#define UFT_IMD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * IMD Constants
 *============================================================================*/

/** Maximum cylinders supported */
#define UFT_IMD_MAX_CYL     84

/** Maximum heads */
#define UFT_IMD_MAX_HEAD    2

/** Maximum sectors per track */
#define UFT_IMD_MAX_SPT     26

/** IMD file header terminator */
#define UFT_IMD_EOF_MARKER  0x1A

/*============================================================================
 * IMD Recording Modes
 *============================================================================*/

typedef enum {
    UFT_IMD_MODE_500K_FM  = 0,  /**< 500 kbps FM */
    UFT_IMD_MODE_300K_FM  = 1,  /**< 300 kbps FM */
    UFT_IMD_MODE_250K_FM  = 2,  /**< 250 kbps FM */
    UFT_IMD_MODE_500K_MFM = 3,  /**< 500 kbps MFM (HD) */
    UFT_IMD_MODE_300K_MFM = 4,  /**< 300 kbps MFM */
    UFT_IMD_MODE_250K_MFM = 5,  /**< 250 kbps MFM (DD) */
} uft_imd_mode_t;

/** Check if mode is FM (single density) */
#define UFT_IMD_IS_FM(mode)   ((mode) <= UFT_IMD_MODE_250K_FM)

/** Check if mode is MFM (double density) */
#define UFT_IMD_IS_MFM(mode)  ((mode) >= UFT_IMD_MODE_500K_MFM)

/*============================================================================
 * IMD Sector Record Types
 *============================================================================*/

typedef enum {
    UFT_IMD_UNAVAILABLE       = 0,  /**< Data could not be read */
    UFT_IMD_NORMAL            = 1,  /**< Normal data */
    UFT_IMD_NORMAL_COMP       = 2,  /**< Compressed (all same byte) */
    UFT_IMD_DELETED           = 3,  /**< Deleted data mark */
    UFT_IMD_DELETED_COMP      = 4,  /**< Compressed deleted data */
    UFT_IMD_NORMAL_ERR        = 5,  /**< Normal with CRC error */
    UFT_IMD_NORMAL_COMP_ERR   = 6,  /**< Compressed with CRC error */
    UFT_IMD_DELETED_ERR       = 7,  /**< Deleted with CRC error */
    UFT_IMD_DELETED_COMP_ERR  = 8,  /**< Compressed deleted with error */
} uft_imd_sector_type_t;

/** Check if sector type indicates compression */
#define UFT_IMD_IS_COMPRESSED(t)  ((t) == 2 || (t) == 4 || (t) == 6 || (t) == 8)

/** Check if sector type indicates CRC error */
#define UFT_IMD_HAS_ERROR(t)      ((t) >= 5)

/** Check if sector type indicates deleted data */
#define UFT_IMD_IS_DELETED(t)     ((t) == 3 || (t) == 4 || (t) == 7 || (t) == 8)

/*============================================================================
 * IMD Header Flags
 *============================================================================*/

/** Sector head map present in track header */
#define UFT_IMD_FLAG_SECT_HEAD_MAP  (1 << 6)

/** Sector cylinder map present in track header */
#define UFT_IMD_FLAG_SECT_CYL_MAP   (1 << 7)

/*============================================================================
 * I/O Status Flags
 *============================================================================*/

#define UFT_IMD_IO_ERROR        (1 << 0)  /**< General error */
#define UFT_IMD_IO_CRC_ERROR    (1 << 1)  /**< CRC error on read */
#define UFT_IMD_IO_DELETED      (1 << 2)  /**< Deleted address mark */
#define UFT_IMD_IO_COMPRESSED   (1 << 3)  /**< Sector was compressed */
#define UFT_IMD_IO_WPROT        (1 << 4)  /**< Write protected */

/*============================================================================
 * Data Structures
 *============================================================================*/

/** IMD track header (as stored in file) */
typedef struct __attribute__((packed)) {
    uint8_t mode;       /**< Recording mode */
    uint8_t cylinder;   /**< Physical cylinder */
    uint8_t head;       /**< Head (with optional flags in bits 6-7) */
    uint8_t nsects;     /**< Number of sectors */
    uint8_t sectsize;   /**< Sector size code (128 << N) */
} uft_imd_track_header_t;

/** Track information (parsed) */
typedef struct {
    uint8_t     mode;           /**< Recording mode */
    uint8_t     nsects;         /**< Number of sectors */
    uint32_t    sectsize;       /**< Sector size in bytes */
    uint8_t     start_sector;   /**< First sector number */
    
    /** File offset for each sector */
    uint32_t    sector_offset[UFT_IMD_MAX_SPT];
    
    /** Sector type for each sector */
    uint8_t     sector_type[UFT_IMD_MAX_SPT];
    
    /** Logical head for each sector (if head map present) */
    uint8_t     logical_head[UFT_IMD_MAX_SPT];
    
    /** Logical cylinder for each sector (if cyl map present) */
    uint8_t     logical_cyl[UFT_IMD_MAX_SPT];
    
    /** Sector number map */
    uint8_t     sector_map[UFT_IMD_MAX_SPT];
} uft_imd_track_info_t;

/** IMD disk image */
typedef struct {
    FILE       *file;           /**< File handle */
    char       *comment;        /**< Comment string (NULL terminated) */
    uint32_t    comment_len;    /**< Comment length */
    
    uint8_t     nsides;         /**< Number of sides */
    uint8_t     ntracks;        /**< Number of tracks (per side) */
    uint8_t     write_locked;   /**< Write lock flag */
    
    /** Track information array [cylinder][head] */
    uft_imd_track_info_t tracks[UFT_IMD_MAX_CYL][UFT_IMD_MAX_HEAD];
} uft_imd_disk_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Open an IMD disk image
 * @param path Path to IMD file
 * @param write Enable write mode
 * @return Disk handle or NULL on error
 */
uft_imd_disk_t *uft_imd_open(const char *path, bool write);

/**
 * @brief Open IMD from file handle
 * @param fp File handle (takes ownership)
 * @param verbose Print info during parse
 * @return Disk handle or NULL on error
 */
uft_imd_disk_t *uft_imd_open_fp(FILE *fp, bool verbose);

/**
 * @brief Close IMD disk image
 * @param disk Disk handle (will be freed)
 */
void uft_imd_close(uft_imd_disk_t *disk);

/**
 * @brief Create new IMD disk image
 * @param path Path for new file
 * @param comment Comment string (may be NULL)
 * @return Disk handle or NULL on error
 */
uft_imd_disk_t *uft_imd_create(const char *path, const char *comment);

/**
 * @brief Read sector from IMD image
 * @param disk Disk handle
 * @param cyl Cylinder number
 * @param head Head number (0 or 1)
 * @param sector Sector number
 * @param buffer Output buffer (must be >= sector size)
 * @param buflen Buffer size
 * @param flags Output flags (UFT_IMD_IO_*)
 * @param readlen Actual bytes read
 * @return 0 on success, -1 on error
 */
int uft_imd_read_sector(uft_imd_disk_t *disk, 
                        uint8_t cyl, uint8_t head, uint8_t sector,
                        uint8_t *buffer, size_t buflen,
                        uint32_t *flags, size_t *readlen);

/**
 * @brief Write sector to IMD image
 * @param disk Disk handle
 * @param cyl Cylinder number
 * @param head Head number
 * @param sector Sector number
 * @param buffer Data to write
 * @param buflen Data length
 * @param flags Input flags (deleted mark, etc)
 * @param writelen Actual bytes written
 * @return 0 on success, -1 on error
 */
int uft_imd_write_sector(uft_imd_disk_t *disk,
                         uint8_t cyl, uint8_t head, uint8_t sector,
                         const uint8_t *buffer, size_t buflen,
                         uint32_t flags, size_t *writelen);

/**
 * @brief Format (write) entire track
 * @param disk Disk handle
 * @param cyl Cylinder number
 * @param head Head number
 * @param nsects Number of sectors
 * @param sectsize Sector size
 * @param sector_map Array of sector numbers
 * @param mode Recording mode
 * @param fillbyte Fill byte for sectors
 * @param flags Output flags
 * @return 0 on success, -1 on error
 */
int uft_imd_format_track(uft_imd_disk_t *disk,
                         uint8_t cyl, uint8_t head,
                         uint8_t nsects, uint32_t sectsize,
                         const uint8_t *sector_map,
                         uft_imd_mode_t mode, uint8_t fillbyte,
                         uint32_t *flags);

/**
 * @brief Get track info
 * @param disk Disk handle
 * @param cyl Cylinder
 * @param head Head
 * @return Track info pointer or NULL
 */
const uft_imd_track_info_t *uft_imd_get_track(uft_imd_disk_t *disk,
                                               uint8_t cyl, uint8_t head);

/**
 * @brief Get disk geometry
 * @param disk Disk handle
 * @param cyls Output: number of cylinders
 * @param heads Output: number of heads
 * @param spt Output: max sectors per track
 * @param sectsize Output: sector size
 */
void uft_imd_get_geometry(uft_imd_disk_t *disk,
                          uint8_t *cyls, uint8_t *heads,
                          uint8_t *spt, uint32_t *sectsize);

/**
 * @brief Check if disk is write locked
 * @param disk Disk handle
 * @return true if locked
 */
bool uft_imd_is_write_locked(uft_imd_disk_t *disk);

/**
 * @brief Get mode name string
 * @param mode Recording mode
 * @return Mode name
 */
const char *uft_imd_mode_name(uft_imd_mode_t mode);

/**
 * @brief Convert IMD to raw sector image
 * @param disk IMD disk handle
 * @param output Output file path
 * @param interleave Apply interleave (false = sequential)
 * @return 0 on success, -1 on error
 */
int uft_imd_to_raw(uft_imd_disk_t *disk, const char *output, bool interleave);

/**
 * @brief Convert raw image to IMD
 * @param input Input file path
 * @param output IMD output path
 * @param cyls Number of cylinders
 * @param heads Number of heads
 * @param spt Sectors per track
 * @param sectsize Sector size
 * @param mode Recording mode
 * @param comment Comment string
 * @return 0 on success, -1 on error
 */
int uft_raw_to_imd(const char *input, const char *output,
                   uint8_t cyls, uint8_t heads, uint8_t spt,
                   uint32_t sectsize, uft_imd_mode_t mode,
                   const char *comment);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IMD_H */
