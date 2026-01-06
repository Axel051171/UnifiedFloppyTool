/**
 * @file uft_atari_image.h
 * @brief Atari 8-bit Disk Image Formats (ATR/XFD/ATX/DCM)
 * 
 * Based on:
 * - mkatr by Daniel Serpell (GPL-2.0)
 * - atari-tools by Joseph H. Allen (GPL)
 * - VAPI/ATX format specification
 * 
 * Supports:
 * - ATR (Standard Atari disk image)
 * - XFD (Raw sector dump)
 * - ATX (Advanced copy protection format)
 * - DCM (DiskComm compressed format)
 */

#ifndef UFT_ATARI_IMAGE_H
#define UFT_ATARI_IMAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Disk Geometry Constants
 *===========================================================================*/

/** Standard Atari disk sizes */
#define UFT_ATARI_SD_SECTORS     720     /**< Single density: 90KB */
#define UFT_ATARI_ED_SECTORS     1040    /**< Enhanced density: 130KB */
#define UFT_ATARI_DD_SECTORS     720     /**< Double density: 180KB */
#define UFT_ATARI_QD_SECTORS     1440    /**< Quad density: 360KB */

#define UFT_ATARI_SD_SECSIZE     128     /**< SD sector size */
#define UFT_ATARI_DD_SECSIZE     256     /**< DD sector size */

/** Calculated image sizes (raw data, no header) */
#define UFT_ATARI_SD_SIZE        (720 * 128)           /**< 92,160 bytes */
#define UFT_ATARI_ED_SIZE        (1040 * 128)          /**< 133,120 bytes */
#define UFT_ATARI_DD_SIZE        (3 * 128 + 717 * 256) /**< 183,936 bytes */

/** Track/sector geometry */
#define UFT_ATARI_SD_TRACKS      40
#define UFT_ATARI_SD_SPT         18      /**< Sectors per track (SD) */
#define UFT_ATARI_ED_SPT         26      /**< Sectors per track (ED) */
#define UFT_ATARI_DD_SPT         18      /**< Sectors per track (DD) */

/*===========================================================================
 * ATR File Format
 *===========================================================================*/

#define UFT_ATR_MAGIC            0x0296  /**< "NICKATARI" signature */
#define UFT_ATR_HEADER_SIZE      16

/**
 * @brief ATR file header (16 bytes)
 * 
 * The ATR format stores disk paragraphs (16-byte units) and sector size.
 * First 3 sectors in DD images may be 128 bytes (boot sectors).
 */
typedef struct __attribute__((packed)) {
    uint8_t  magic[2];          /**< 0x96, 0x02 */
    uint8_t  para_lo;           /**< Paragraphs low byte */
    uint8_t  para_hi;           /**< Paragraphs high byte */
    uint8_t  secsize_lo;        /**< Sector size low byte */
    uint8_t  secsize_hi;        /**< Sector size high byte */
    uint8_t  para_ext;          /**< Extended paragraphs (high byte) */
    uint8_t  crc[4];            /**< Optional CRC-32 */
    uint8_t  unused[4];         /**< Reserved */
    uint8_t  flags;             /**< Flags (write protect, etc.) */
} uft_atr_header_t;

/** ATR flags */
#define UFT_ATR_FLAG_WRITE_PROT  0x01

/*===========================================================================
 * ATX File Format (Advanced Copy Protection)
 *===========================================================================*/

#define UFT_ATX_MAGIC            "AT8X"
#define UFT_ATX_VERSION          0x01

/** ATX creator IDs */
typedef enum {
    UFT_ATX_CR_FX7      = 0x01, /**< Conversion from FX7 */
    UFT_ATX_CR_FX8      = 0x02, /**< Conversion from FX8 */
    UFT_ATX_CR_ATR      = 0x03, /**< Conversion from ATR */
    UFT_ATX_CR_WH2PC    = 0x10  /**< WhatsHere2PC hardware */
} uft_atx_creator_t;

/** ATX record types */
typedef enum {
    UFT_ATX_RT_TRACK    = 0x00,
    UFT_ATX_RT_HOSTDATA = 0x100 /**< Private data */
} uft_atx_rectype_t;

/** ATX track flags */
#define UFT_ATX_TF_NOSKEW        0x100   /**< Couldn't measure skew */

/** ATX chunk types within track */
typedef enum {
    UFT_ATX_CU_DATA     = 0x00, /**< Sector data */
    UFT_ATX_CU_HDRLST   = 0x01, /**< Header list */
    UFT_ATX_CU_WK7      = 0x10, /**< FX7-style weak bits */
    UFT_ATX_CU_EXTHDR   = 0x11  /**< Extended sector header */
} uft_atx_chunk_t;

/**
 * @brief ATX file header (48 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t  signature[4];      /**< "AT8X" */
    uint16_t version;           /**< Format version */
    uint16_t min_version;       /**< Minimum reader version */
    uint16_t creator;           /**< Creator ID */
    uint16_t creator_version;   /**< Creator version */
    uint32_t flags;             /**< Image flags */
    uint16_t image_type;        /**< Image type */
    uint16_t reserved0;
    uint32_t image_id;          /**< Unique image ID */
    uint16_t image_version;     /**< Image version */
    uint16_t reserved1;
    uint32_t start_data;        /**< Offset to first record */
    uint32_t end_data;          /**< Offset past last record */
    uint8_t  reserved2[12];
} uft_atx_header_t;

/**
 * @brief ATX record header
 */
typedef struct __attribute__((packed)) {
    uint32_t next;              /**< Offset to next record */
    uint16_t type;              /**< Record type */
    uint16_t pad0;
} uft_atx_rec_header_t;

/**
 * @brief ATX track header
 */
typedef struct __attribute__((packed)) {
    uint32_t next;              /**< Offset to next record */
    uint16_t type;              /**< Record type (0 = track) */
    uint16_t pad0;
    uint8_t  track;             /**< Track number */
    uint8_t  pad1;
    uint16_t num_headers;       /**< Number of sector headers */
    uint16_t rate;              /**< Rotation rate */
    uint16_t pad3;
    uint32_t flags;             /**< Track flags */
    uint32_t start_data;        /**< Offset to chunk data */
    uint8_t  reserved[8];
} uft_atx_track_header_t;

/**
 * @brief ATX sector header
 * 
 * Time is start of header in 8-microsecond units (at nominal speed).
 */
typedef struct __attribute__((packed)) {
    uint8_t  sector;            /**< Sector number */
    uint8_t  status;            /**< FDC status */
    uint16_t time;              /**< Angular position */
    uint32_t data;              /**< Offset to sector data */
} uft_atx_sector_header_t;

/** FDC status bits in ATX sector header */
#define UFT_ATX_STATUS_CRC_ERROR     0x08   /**< CRC error in data */
#define UFT_ATX_STATUS_LOST_DATA     0x04   /**< Lost data */
#define UFT_ATX_STATUS_RNF           0x10   /**< Record not found */
#define UFT_ATX_STATUS_DELETED       0x20   /**< Deleted data mark */
#define UFT_ATX_STATUS_WEAK          0x40   /**< Weak/fuzzy bits */

/*===========================================================================
 * DCM Format (DiskComm Compressed)
 *===========================================================================*/

/** DCM file types */
#define UFT_DCM_SINGLE           0xF9    /**< Single file */
#define UFT_DCM_MULTI_START      0xFA    /**< Multi-file start */
#define UFT_DCM_MULTI_CONT       0xFB    /**< Multi-file continuation */
#define UFT_DCM_MULTI_END        0xFC    /**< Multi-file end */

/** DCM compression types */
typedef enum {
    UFT_DCM_CHANGE      = 0x41, /**< Change */
    UFT_DCM_SAME        = 0x42, /**< Same as previous */
    UFT_DCM_COMP        = 0x43, /**< Compressed */
    UFT_DCM_MODIFY      = 0x44, /**< Modify previous */
    UFT_DCM_UNCOMP      = 0x46, /**< Uncompressed */
    UFT_DCM_END         = 0x45  /**< End of file */
} uft_dcm_comptype_t;

/*===========================================================================
 * Image Detection and Info
 *===========================================================================*/

/** Detected disk format */
typedef enum {
    UFT_ATARI_FMT_UNKNOWN = 0,
    UFT_ATARI_FMT_ATR_SD,       /**< ATR single density */
    UFT_ATARI_FMT_ATR_ED,       /**< ATR enhanced density */
    UFT_ATARI_FMT_ATR_DD,       /**< ATR double density */
    UFT_ATARI_FMT_ATR_QD,       /**< ATR quad density */
    UFT_ATARI_FMT_XFD_SD,       /**< XFD single density */
    UFT_ATARI_FMT_XFD_ED,       /**< XFD enhanced density */
    UFT_ATARI_FMT_XFD_DD,       /**< XFD double density */
    UFT_ATARI_FMT_ATX,          /**< ATX copy protected */
    UFT_ATARI_FMT_DCM           /**< DCM compressed */
} uft_atari_format_t;

/**
 * @brief Disk image info
 */
typedef struct {
    uft_atari_format_t format;
    uint16_t sectors;           /**< Total sectors */
    uint16_t sector_size;       /**< Bytes per sector */
    uint32_t data_size;         /**< Raw data size */
    bool     write_protected;
    bool     has_errors;        /**< Contains bad sectors */
    bool     has_weak_bits;     /**< Contains weak/fuzzy bits */
} uft_atari_image_info_t;

/*===========================================================================
 * ATR Helper Functions
 *===========================================================================*/

/**
 * @brief Check if data has valid ATR header
 */
static inline bool uft_atr_is_valid(const uint8_t *data)
{
    return data[0] == 0x96 && data[1] == 0x02;
}

/**
 * @brief Parse ATR header
 */
static inline int uft_atr_parse_header(const uint8_t *data, 
                                        uft_atr_header_t *hdr)
{
    if (!uft_atr_is_valid(data)) return -1;
    
    hdr->magic[0] = data[0];
    hdr->magic[1] = data[1];
    hdr->para_lo = data[2];
    hdr->para_hi = data[3];
    hdr->secsize_lo = data[4];
    hdr->secsize_hi = data[5];
    hdr->para_ext = data[6];
    hdr->flags = data[15];
    
    return 0;
}

/**
 * @brief Get sector size from ATR header
 */
static inline uint16_t uft_atr_sector_size(const uft_atr_header_t *hdr)
{
    return hdr->secsize_lo | (hdr->secsize_hi << 8);
}

/**
 * @brief Get image size from ATR header (in paragraphs * 16)
 */
static inline uint32_t uft_atr_image_size(const uft_atr_header_t *hdr)
{
    return ((uint32_t)hdr->para_lo | 
            ((uint32_t)hdr->para_hi << 8) |
            ((uint32_t)hdr->para_ext << 16)) * 16;
}

/**
 * @brief Calculate sector count from ATR header
 */
static inline uint16_t uft_atr_sector_count(const uft_atr_header_t *hdr)
{
    uint32_t isz = uft_atr_image_size(hdr);
    uint16_t ssz = uft_atr_sector_size(hdr);
    
    if (ssz == 128) {
        return (uint16_t)(isz / 128);
    } else {
        /* DD: first 3 sectors are 128 bytes, rest are 256 */
        uint32_t pad = (ssz - 128) * 3;
        return (uint16_t)((isz + pad) / ssz);
    }
}

/**
 * @brief Calculate offset to sector in ATR file
 * @param sector Sector number (1-based!)
 * @param sector_size Sector size (128 or 256)
 * @return File offset including header
 */
static inline size_t uft_atr_sector_offset(uint16_t sector, 
                                            uint16_t sector_size)
{
    if (sector < 1) return 0;
    
    if (sector_size == 128 || sector <= 3) {
        /* SD or boot sectors: all 128 bytes */
        return UFT_ATR_HEADER_SIZE + (size_t)(sector - 1) * 128;
    } else {
        /* DD: boot sectors (1-3) are 128, rest are 256 */
        return UFT_ATR_HEADER_SIZE + 
               3 * 128 + 
               (size_t)(sector - 4) * 256;
    }
}

/**
 * @brief Calculate offset in XFD file (no header)
 */
static inline size_t uft_xfd_sector_offset(uint16_t sector,
                                            uint16_t sector_size)
{
    if (sector < 1) return 0;
    return (size_t)(sector - 1) * sector_size;
}

/*===========================================================================
 * ATX Helper Functions
 *===========================================================================*/

/**
 * @brief Check if data has valid ATX header
 */
static inline bool uft_atx_is_valid(const uint8_t *data)
{
    return data[0] == 'A' && data[1] == 'T' && 
           data[2] == '8' && data[3] == 'X';
}

/*===========================================================================
 * Format Detection
 *===========================================================================*/

/**
 * @brief Detect Atari disk image format
 * @param data Image data
 * @param size Image size
 * @param info Output image info
 * @return Format type
 */
uft_atari_format_t uft_atari_detect_format(const uint8_t *data, 
                                            size_t size,
                                            uft_atari_image_info_t *info);

/*===========================================================================
 * Sector Access API
 *===========================================================================*/

/**
 * @brief Read sector from ATR image
 */
int uft_atr_read_sector(const uint8_t *image, size_t image_size,
                        uint16_t sector, uint8_t *buffer,
                        uint16_t sector_size);

/**
 * @brief Write sector to ATR image
 */
int uft_atr_write_sector(uint8_t *image, size_t image_size,
                         uint16_t sector, const uint8_t *buffer,
                         uint16_t sector_size);

/**
 * @brief Create new ATR image
 * @param buffer Output buffer
 * @param sectors Number of sectors
 * @param sector_size Sector size (128 or 256)
 * @return Total image size including header
 */
size_t uft_atr_create(uint8_t *buffer, uint16_t sectors, 
                      uint16_t sector_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARI_IMAGE_H */
