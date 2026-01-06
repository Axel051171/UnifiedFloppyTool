/**
 * @file uft_atari8_disk.h
 * @brief Atari 8-bit (400/800/XL/XE) Floppy Disk Access
 * 
 * Based on "Direct Atari Disk Access" by Andrew Lieberman
 * COMPUTE! Magazine, Issue 34, March 1983
 * 
 * Implements direct sector access for Atari 810/1050 disk drives
 * using the SIO (Serial I/O) protocol and DCB (Device Control Block).
 */

#ifndef UFT_ATARI8_DISK_H
#define UFT_ATARI8_DISK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Atari Disk Geometry Constants
 *===========================================================================*/

/** Standard Atari 810/1050 disk parameters */
#define UFT_ATARI_SECTOR_SIZE       128     /**< Bytes per sector (SD) */
#define UFT_ATARI_SECTOR_SIZE_DD    256     /**< Bytes per sector (DD/ED) */
#define UFT_ATARI_SECTORS_PER_TRACK 18      /**< Sectors per track (SD) */
#define UFT_ATARI_TRACKS            40      /**< Tracks per disk */
#define UFT_ATARI_TOTAL_SECTORS     720     /**< Total sectors (1-720) */
#define UFT_ATARI_FIRST_SECTOR      1       /**< Sectors are 1-based! */
#define UFT_ATARI_LAST_SECTOR       720
#define UFT_ATARI_SECTORS_PER_KB    8       /**< 8 sectors = 1KB */

/** Enhanced density (1050) */
#define UFT_ATARI_ED_SECTORS        1040    /**< Enhanced density sectors */
#define UFT_ATARI_ED_SECTOR_SIZE    128     /**< Still 128 bytes */

/** Double density (XF551, etc.) */
#define UFT_ATARI_DD_SECTORS        720     /**< Same count, larger sectors */

/** Boot sectors */
#define UFT_ATARI_BOOT_SECTORS      3       /**< Sectors 1-3 are boot */
#define UFT_ATARI_VTOC_SECTOR       360     /**< Volume Table of Contents */
#define UFT_ATARI_DIR_START         361     /**< Directory starts here */
#define UFT_ATARI_DIR_END           368     /**< Directory ends here */

/*===========================================================================
 * Atari SIO Device Control Block (DCB)
 * Memory locations $300-$30B on the Atari
 *===========================================================================*/

/** DCB memory addresses (Atari memory map) */
#define UFT_ATARI_DCB_BASE      0x0300  /**< DCB base address */
#define UFT_ATARI_DDEVIC        0x0300  /**< Device ID ($31 for disk) */
#define UFT_ATARI_DUNIT         0x0301  /**< Drive unit number (1-4) */
#define UFT_ATARI_DCOMD         0x0302  /**< Command byte */
#define UFT_ATARI_DSTATS        0x0303  /**< Status after operation */
#define UFT_ATARI_DBUFLO        0x0304  /**< Buffer pointer low byte */
#define UFT_ATARI_DBUFHI        0x0305  /**< Buffer pointer high byte */
#define UFT_ATARI_DTIMLO        0x0306  /**< Timeout value (low) */
#define UFT_ATARI_DUNUSE        0x0307  /**< Unused */
#define UFT_ATARI_DBYTLO        0x0308  /**< Byte count low */
#define UFT_ATARI_DBYTHI        0x0309  /**< Byte count high */
#define UFT_ATARI_DAUXLO        0x030A  /**< Auxiliary (sector) low byte */
#define UFT_ATARI_DAUXHI        0x030B  /**< Auxiliary (sector) high byte */

/** SIO ROM routine address */
#define UFT_ATARI_SIOV          0xE459  /**< SIO vector */
#define UFT_ATARI_DSKINV        0xE453  /**< Disk handler entry point */

/*===========================================================================
 * SIO Commands
 *===========================================================================*/

typedef enum {
    /* Standard disk commands */
    UFT_ATARI_CMD_READ          = 0x52, /**< 'R' - Read sector */
    UFT_ATARI_CMD_WRITE         = 0x57, /**< 'W' - Write sector (with verify) */
    UFT_ATARI_CMD_WRITE_NO_VFY  = 0x50, /**< 'P' - Write without verify */
    UFT_ATARI_CMD_STATUS        = 0x53, /**< 'S' - Get drive status */
    UFT_ATARI_CMD_FORMAT        = 0x21, /**< '!' - Format disk */
    UFT_ATARI_CMD_FORMAT_DD     = 0x22, /**< '"' - Format double density */
    
    /* 1050 specific commands */
    UFT_ATARI_CMD_READ_ADDR     = 0x54, /**< Read address (track info) */
    UFT_ATARI_CMD_SPIN          = 0x51, /**< Spin up motor */
    UFT_ATARI_CMD_MOTOR_OFF     = 0x55, /**< Motor off */
    
    /* Happy/US Doubler commands */
    UFT_ATARI_CMD_HIGH_SPEED    = 0x48, /**< 'H' - High speed mode */
    
    /* XF551 commands */
    UFT_ATARI_CMD_GET_CONFIG    = 0x4E, /**< 'N' - Get configuration */
    UFT_ATARI_CMD_SET_CONFIG    = 0x4F  /**< 'O' - Set configuration */
} uft_atari_cmd_t;

/*===========================================================================
 * SIO Status Codes
 *===========================================================================*/

typedef enum {
    UFT_ATARI_STS_OK            = 0x01, /**< Operation successful */
    UFT_ATARI_STS_TIMEOUT       = 0x8A, /**< Device timeout */
    UFT_ATARI_STS_NAK           = 0x8B, /**< Device NAK */
    UFT_ATARI_STS_FRAME_ERR     = 0x8C, /**< Serial frame error */
    UFT_ATARI_STS_CHECKSUM      = 0x8E, /**< Checksum error */
    UFT_ATARI_STS_DEVICE_ERR    = 0x90, /**< Device error */
    UFT_ATARI_STS_WRITE_PROT    = 0xB5, /**< Write protected */
    UFT_ATARI_STS_DRIVE_ERR     = 0x80  /**< General drive error */
} uft_atari_status_t;

/*===========================================================================
 * Drive Status Byte (from STATUS command)
 *===========================================================================*/

/** Status byte bit flags */
#define UFT_ATARI_STAT_CMD_FRAME    0x01    /**< Invalid command frame */
#define UFT_ATARI_STAT_CHECKSUM     0x02    /**< Data frame checksum error */
#define UFT_ATARI_STAT_WRITE_PROT   0x08    /**< Write protected */
#define UFT_ATARI_STAT_MOTOR_ON     0x10    /**< Motor running */
#define UFT_ATARI_STAT_DOUBLE_DEN   0x20    /**< Double density sector */
#define UFT_ATARI_STAT_ENHANCED     0x80    /**< Enhanced density mode */

/*===========================================================================
 * Device Control Block Structure
 *===========================================================================*/

/**
 * @brief Atari SIO Device Control Block
 * 
 * This structure mirrors the DCB in Atari memory at $0300-$030B.
 * Used for all SIO device communication.
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  ddevic;    /**< Device ID ($31='1' for disk) */
    uint8_t  dunit;     /**< Unit number (1-4) */
    uint8_t  dcomd;     /**< Command byte */
    uint8_t  dstats;    /**< Status/direction: $40=read, $80=write */
    uint8_t  dbuflo;    /**< Buffer address low byte */
    uint8_t  dbufhi;    /**< Buffer address high byte */
    uint8_t  dtimlo;    /**< Timeout in seconds */
    uint8_t  dunuse;    /**< Unused */
    uint8_t  dbytlo;    /**< Byte count low */
    uint8_t  dbythi;    /**< Byte count high */
    uint8_t  dauxlo;    /**< Aux1 (sector low byte) */
    uint8_t  dauxhi;    /**< Aux2 (sector high byte) */
} uft_atari_dcb_t;
UFT_PACK_END

/** DCB direction flags for dstats */
#define UFT_ATARI_DCB_READ      0x40    /**< Data transfer: device to computer */
#define UFT_ATARI_DCB_WRITE     0x80    /**< Data transfer: computer to device */
#define UFT_ATARI_DCB_NONE      0x00    /**< No data transfer */

/*===========================================================================
 * Atari DOS 2.0S Directory Structure
 *===========================================================================*/

/** Directory entry flags */
#define UFT_ATARI_DIR_DELETED   0x80    /**< File deleted */
#define UFT_ATARI_DIR_IN_USE    0x40    /**< File in use (being written) */
#define UFT_ATARI_DIR_LOCKED    0x20    /**< File locked */
#define UFT_ATARI_DIR_DOS2      0x02    /**< DOS 2 file */
#define UFT_ATARI_DIR_OPENED    0x01    /**< File opened for output */

#define UFT_ATARI_DIR_ENTRIES   64      /**< Max files per disk */
#define UFT_ATARI_DIR_ENTRY_SIZE 16     /**< Bytes per directory entry */
#define UFT_ATARI_FILENAME_LEN  8       /**< Filename length */
#define UFT_ATARI_EXT_LEN       3       /**< Extension length */

/**
 * @brief Atari DOS 2.x directory entry (16 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  flags;             /**< File flags */
    uint16_t sector_count;      /**< Number of sectors (LE) */
    uint16_t start_sector;      /**< Starting sector (LE) */
    uint8_t  filename[8];       /**< Filename (space-padded) */
    uint8_t  extension[3];      /**< Extension (space-padded) */
} uft_atari_dir_entry_t;
UFT_PACK_END

/*===========================================================================
 * Volume Table of Contents (VTOC) - Sector 360
 *===========================================================================*/

/**
 * @brief Atari DOS 2.x VTOC structure
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  dos_code;          /**< DOS code (usually 2) */
    uint16_t total_sectors;     /**< Total sectors (LE) */
    uint16_t free_sectors;      /**< Free sectors (LE) */
    uint8_t  unused[5];         /**< Unused bytes */
    uint8_t  bitmap[90];        /**< Sector allocation bitmap */
    /**< Bit=1 means sector is free */
    /**< Covers sectors 0-719 */
} uft_atari_vtoc_t;
UFT_PACK_END

/*===========================================================================
 * Sector Data Link (for file chains)
 *===========================================================================*/

/**
 * @brief Atari DOS 2.x sector structure
 * 
 * Each data sector has 125 bytes of data + 3 bytes of link info
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  file_num_hi;       /**< File number bits 7-2 + next sector hi bits */
    uint8_t  data[125];         /**< Actual file data */
    uint8_t  next_sector_lo;    /**< Next sector low byte (0 = last) */
    uint8_t  data_bytes;        /**< Bytes used in this sector (usually 125) */
} uft_atari_data_sector_t;
UFT_PACK_END

/** Extract file number from sector */
#define UFT_ATARI_SECTOR_FILENUM(s) (((s)->file_num_hi >> 2) & 0x3F)

/** Extract next sector number */
#define UFT_ATARI_SECTOR_NEXT(s) \
    ((uint16_t)((s)->next_sector_lo) | \
     (((uint16_t)((s)->file_num_hi & 0x03)) << 8))

/*===========================================================================
 * Disk Image Formats
 *===========================================================================*/

/** ATR file header (16 bytes) */
UFT_PACK_BEGIN
typedef struct {
    uint16_t magic;             /**< 0x0296 = NICKATARI */
    uint16_t paragraphs;        /**< Size in 16-byte paragraphs (low) */
    uint16_t sector_size;       /**< Sector size (128 or 256) */
    uint8_t  paragraphs_hi;     /**< Size high byte */
    uint32_t crc;               /**< Optional CRC */
    uint32_t unused;
    uint8_t  flags;             /**< Flags (write protect, etc.) */
} uft_atari_atr_header_t;
UFT_PACK_END

#define UFT_ATARI_ATR_MAGIC     0x0296  /**< "NICKATARI" signature */

/** ATR flags */
#define UFT_ATARI_ATR_WRITE_PROT 0x01   /**< Write protected */

/** Calculate ATR file offset for sector (1-based!) */
static inline size_t uft_atari_atr_sector_offset(uint16_t sector, 
                                                  uint16_t sector_size)
{
    /* ATR header is 16 bytes */
    /* First 3 sectors (boot) are always 128 bytes in DD images */
    if (sector <= 3 || sector_size == 128) {
        return 16 + ((size_t)(sector - 1) * 128);
    } else {
        /* DD: 3 boot sectors at 128 + rest at 256 */
        return 16 + (3 * 128) + ((size_t)(sector - 4) * sector_size);
    }
}

/*===========================================================================
 * XFD Format (Raw Sector Dump)
 *===========================================================================*/

/** XFD has no header - just raw sectors */
static inline size_t uft_atari_xfd_sector_offset(uint16_t sector,
                                                  uint16_t sector_size)
{
    return (size_t)(sector - 1) * sector_size;
}

/*===========================================================================
 * Disk Format Definitions
 *===========================================================================*/

typedef enum {
    UFT_ATARI_FMT_UNKNOWN = 0,
    UFT_ATARI_FMT_SD,           /**< Single density (90KB) */
    UFT_ATARI_FMT_ED,           /**< Enhanced density (130KB) - 1050 */
    UFT_ATARI_FMT_DD,           /**< Double density (180KB) */
    UFT_ATARI_FMT_QD,           /**< Quad density (360KB) - XF551 DS */
    UFT_ATARI_FMT_HD            /**< High density (720KB) - 3.5" */
} uft_atari_density_t;

/**
 * @brief Atari disk format info
 */
typedef struct {
    uft_atari_density_t density;
    uint16_t sectors;           /**< Total sectors */
    uint16_t sector_size;       /**< Bytes per sector */
    uint8_t  tracks;            /**< Tracks per side */
    uint8_t  sides;             /**< Number of sides */
    uint8_t  sectors_per_track; /**< Sectors per track */
    size_t   image_size;        /**< Total image size in bytes */
} uft_atari_format_t;

/** Standard format definitions */
static const uft_atari_format_t UFT_ATARI_FORMAT_SD = {
    UFT_ATARI_FMT_SD, 720, 128, 40, 1, 18, 92160
};

static const uft_atari_format_t UFT_ATARI_FORMAT_ED = {
    UFT_ATARI_FMT_ED, 1040, 128, 40, 1, 26, 133120
};

static const uft_atari_format_t UFT_ATARI_FORMAT_DD = {
    UFT_ATARI_FMT_DD, 720, 256, 40, 1, 18, 184320
};

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize DCB for sector read
 * @param dcb Device control block to initialize
 * @param unit Drive unit (1-4)
 * @param sector Sector number (1-720)
 * @param buffer Buffer address (as 16-bit Atari address)
 */
static inline void uft_atari_dcb_read(uft_atari_dcb_t *dcb,
                                       uint8_t unit, uint16_t sector,
                                       uint16_t buffer)
{
    dcb->ddevic = 0x31;  /* Disk device */
    dcb->dunit = unit;
    dcb->dcomd = UFT_ATARI_CMD_READ;
    dcb->dstats = UFT_ATARI_DCB_READ;
    dcb->dbuflo = buffer & 0xFF;
    dcb->dbufhi = (buffer >> 8) & 0xFF;
    dcb->dtimlo = 7;     /* 7 second timeout */
    dcb->dunuse = 0;
    dcb->dbytlo = 128;   /* SD sector */
    dcb->dbythi = 0;
    dcb->dauxlo = sector & 0xFF;
    dcb->dauxhi = (sector >> 8) & 0xFF;
}

/**
 * @brief Initialize DCB for sector write
 */
static inline void uft_atari_dcb_write(uft_atari_dcb_t *dcb,
                                        uint8_t unit, uint16_t sector,
                                        uint16_t buffer)
{
    uft_atari_dcb_read(dcb, unit, sector, buffer);
    dcb->dcomd = UFT_ATARI_CMD_WRITE;
    dcb->dstats = UFT_ATARI_DCB_WRITE;
}

/**
 * @brief Read sector from ATR image
 * @param image ATR image data (with header)
 * @param image_size Size of image
 * @param sector Sector number (1-based)
 * @param buffer Output buffer (128 or 256 bytes)
 * @param sector_size Sector size (from ATR header)
 * @return 0 on success, -1 on error
 */
int uft_atari_atr_read_sector(const uint8_t *image, size_t image_size,
                               uint16_t sector, uint8_t *buffer,
                               uint16_t sector_size);

/**
 * @brief Write sector to ATR image
 */
int uft_atari_atr_write_sector(uint8_t *image, size_t image_size,
                                uint16_t sector, const uint8_t *buffer,
                                uint16_t sector_size);

/**
 * @brief Parse ATR header
 */
int uft_atari_atr_parse_header(const uint8_t *data, uft_atari_atr_header_t *hdr);

/**
 * @brief Detect disk format from image
 */
uft_atari_density_t uft_atari_detect_density(const uint8_t *image, 
                                              size_t image_size);

/**
 * @brief Read VTOC from image
 */
int uft_atari_read_vtoc(const uint8_t *image, size_t image_size,
                         uft_atari_vtoc_t *vtoc);

/**
 * @brief Read directory entry
 * @param image Disk image
 * @param image_size Image size
 * @param index Entry index (0-63)
 * @param entry Output entry
 * @return 0 on success
 */
int uft_atari_read_dir_entry(const uint8_t *image, size_t image_size,
                              int index, uft_atari_dir_entry_t *entry);

/**
 * @brief Find file in directory
 * @param image Disk image
 * @param image_size Image size
 * @param filename 8.3 filename (e.g., "AUTORUN SYS")
 * @param entry Output entry if found
 * @return Entry index (0-63) or -1 if not found
 */
int uft_atari_find_file(const uint8_t *image, size_t image_size,
                         const char *filename, uft_atari_dir_entry_t *entry);

/**
 * @brief Calculate SIO checksum
 * @param data Data bytes
 * @param len Number of bytes
 * @return Checksum byte
 */
static inline uint8_t uft_atari_checksum(const uint8_t *data, size_t len)
{
    uint16_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
        if (sum > 255) {
            sum = (sum & 0xFF) + 1;  /* Add carry */
        }
    }
    return (uint8_t)sum;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARI8_DISK_H */
