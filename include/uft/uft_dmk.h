/**
 * @file uft_dmk.h
 * @brief DMK Disk Image Format Support for UFT
 * 
 * DMK is a disk image format created by David Keil for TRS-80 emulators.
 * It records raw track data including address marks and CRC bytes,
 * making it suitable for preserving copy-protected disks.
 * 
 * The format stores an IDAM (ID Address Mark) pointer table at the
 * beginning of each track, followed by the raw MFM/FM encoded track data.
 * 
 * @copyright UFT Project
 */

#ifndef UFT_DMK_H
#define UFT_DMK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * DMK Format Constants
 *============================================================================*/

/** Maximum tracks in a DMK image */
#define UFT_DMK_MAX_TRACKS      160

/** Maximum IDAM pointers per track */
#define UFT_DMK_MAX_IDAMS       64

/** Size of IDAM pointer table in bytes */
#define UFT_DMK_IDAM_TABLE_SIZE 128

/** DMK header size */
#define UFT_DMK_HEADER_SIZE     16

/** Native mode signature bytes (at offset 0x0C) */
#define UFT_DMK_NATIVE_SIG      0x12345678

/** Single-density IDAM marker bit in pointer */
#define UFT_DMK_IDAM_SD_FLAG    0x8000

/** IDAM pointer mask (actual offset) */
#define UFT_DMK_IDAM_MASK       0x3FFF

/** Double-byte flag (bit 15 of IDAM) */
#define UFT_DMK_IDAM_DOUBLE     0x8000

/*============================================================================
 * DMK Header Flags (byte at offset 4)
 *============================================================================*/

/** Single-sided disk */
#define UFT_DMK_FLAG_SS         0x10

/** Single-density disk (FM) */
#define UFT_DMK_FLAG_SD         0x40

/** Ignore density (treat all as MFM) */
#define UFT_DMK_FLAG_IGNDEN     0x80

/*============================================================================
 * DMK Address Marks
 *============================================================================*/

/** MFM ID Address Mark */
#define UFT_DMK_MFM_IDAM        0xFE

/** MFM Data Address Mark (normal) */
#define UFT_DMK_MFM_DAM         0xFB

/** MFM Deleted Data Address Mark */
#define UFT_DMK_MFM_DDAM        0xF8

/** FM ID Address Mark */
#define UFT_DMK_FM_IDAM         0xFE

/** FM Data Address Mark */
#define UFT_DMK_FM_DAM          0xFB

/** FM Deleted Data Address Mark */
#define UFT_DMK_FM_DDAM         0xF8

/** FM Index Address Mark */
#define UFT_DMK_FM_IAM          0xFC

/** MFM sync byte (0xA1 with missing clock) */
#define UFT_DMK_MFM_SYNC        0xA1

/*============================================================================
 * DMK File Structures
 *============================================================================*/

/**
 * @brief DMK file header (16 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  write_protect;     /**< 0x00=R/W, 0xFF=Read-only */
    uint8_t  tracks;            /**< Number of tracks */
    uint16_t track_length;      /**< Track length in bytes (little-endian) */
    uint8_t  flags;             /**< Disk flags */
    uint8_t  reserved[7];       /**< Reserved (should be 0) */
    uint32_t native_flag;       /**< 0x12345678 if native mode */
} uft_dmk_header_t;

/**
 * @brief DMK IDAM entry (parsed)
 */
typedef struct {
    uint16_t offset;            /**< Offset to IDAM in track data */
    bool     single_density;    /**< True if FM, false if MFM */
    bool     valid;             /**< True if this IDAM entry is valid */
} uft_dmk_idam_t;

/**
 * @brief DMK sector ID field (from track data)
 */
typedef struct {
    uint8_t  cylinder;          /**< Cylinder number */
    uint8_t  head;              /**< Head/side number */
    uint8_t  sector;            /**< Sector number */
    uint8_t  size_code;         /**< Size code (128 << code) */
    uint16_t crc;               /**< CRC-16 */
} uft_dmk_sector_id_t;

/**
 * @brief DMK sector (expanded)
 */
typedef struct {
    uft_dmk_sector_id_t id;     /**< Sector ID */
    bool     deleted;           /**< Has deleted address mark */
    bool     crc_error;         /**< CRC error detected */
    bool     fm_encoding;       /**< FM (true) or MFM (false) */
    uint16_t data_offset;       /**< Offset to data in track */
    uint16_t data_size;         /**< Actual data size */
    uint8_t* data;              /**< Sector data (NULL if not read) */
} uft_dmk_sector_t;

/**
 * @brief DMK track (expanded)
 */
typedef struct {
    uint8_t  cylinder;          /**< Physical cylinder */
    uint8_t  head;              /**< Physical head */
    uint16_t track_length;      /**< Raw track data length */
    
    /* IDAM table */
    uint8_t  num_idams;         /**< Number of valid IDAMs */
    uft_dmk_idam_t idams[UFT_DMK_MAX_IDAMS];
    
    /* Sectors */
    uint8_t  num_sectors;       /**< Number of sectors found */
    uft_dmk_sector_t* sectors;
    
    /* Raw track data */
    uint8_t* raw_data;          /**< Raw track data including IDAM table */
} uft_dmk_track_t;

/**
 * @brief DMK image (expanded)
 */
typedef struct {
    uft_dmk_header_t header;
    
    /* Geometry */
    uint8_t  num_tracks;        /**< Number of tracks */
    uint8_t  num_heads;         /**< Number of heads (1 or 2) */
    uint8_t  num_cylinders;     /**< Number of cylinders */
    
    /* Flags */
    bool     single_sided;      /**< Single-sided disk */
    bool     single_density;    /**< Single-density (FM) */
    bool     write_protected;   /**< Write protected */
    bool     native_mode;       /**< Native mode flag set */
    
    /* Tracks */
    uft_dmk_track_t* tracks;
} uft_dmk_image_t;

/*============================================================================
 * DMK API Functions
 *============================================================================*/

/**
 * @brief Initialize DMK image structure
 */
int uft_dmk_init(uft_dmk_image_t* img);

/**
 * @brief Free DMK image resources
 */
void uft_dmk_free(uft_dmk_image_t* img);

/**
 * @brief Detect if data is DMK format
 * @param data Data buffer
 * @param size Data size
 * @return true if appears to be DMK
 */
bool uft_dmk_detect(const uint8_t* data, size_t size);

/**
 * @brief Read DMK image from file
 */
int uft_dmk_read(const char* filename, uft_dmk_image_t* img);

/**
 * @brief Read DMK image from memory
 */
int uft_dmk_read_mem(const uint8_t* data, size_t size, uft_dmk_image_t* img);

/**
 * @brief Write DMK image to file
 */
int uft_dmk_write(const char* filename, const uft_dmk_image_t* img);

/**
 * @brief Get track by cylinder and head
 */
uft_dmk_track_t* uft_dmk_get_track(uft_dmk_image_t* img,
                                    uint8_t cylinder, uint8_t head);

/**
 * @brief Parse IDAMs from raw track data
 * @param track Track to parse
 * @return Number of IDAMs found
 */
int uft_dmk_parse_idams(uft_dmk_track_t* track);

/**
 * @brief Extract sectors from track
 * @param track Track to process
 * @return Number of sectors found
 */
int uft_dmk_extract_sectors(uft_dmk_track_t* track);

/**
 * @brief Read sector data
 * @param track Track containing sector
 * @param sector_num Sector number
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes read or negative error
 */
int uft_dmk_read_sector(const uft_dmk_track_t* track, uint8_t sector_num,
                        uint8_t* buffer, size_t size);

/**
 * @brief Convert DMK to IMD format
 */
int uft_dmk_to_imd(const uft_dmk_image_t* dmk, struct uft_imd_image_t* imd);

/**
 * @brief Convert DMK to raw binary
 */
int uft_dmk_to_raw(const uft_dmk_image_t* img, uint8_t** data,
                   size_t* size, uint8_t fill);

/**
 * @brief Calculate CRC-16 for DMK data
 * 
 * Uses the same CRC-16-CCITT as floppy controllers but with
 * reversed byte and bit ordering as used in DMK format.
 * 
 * @param data Data to CRC
 * @param length Data length
 * @param crc Initial CRC value
 * @return Updated CRC
 */
uint16_t uft_dmk_crc16(const uint8_t* data, size_t length, uint16_t crc);

/**
 * @brief Calculate CRC for A1 A1 A1 sync pattern
 * This is a constant: 0xCDB4
 */
#define UFT_DMK_CRC_A1A1A1  0xCDB4

/**
 * @brief Print DMK image information
 */
void uft_dmk_print_info(const uft_dmk_image_t* img, bool verbose);

/*============================================================================
 * DMK Track Data Utilities
 *============================================================================*/

/**
 * @brief Find address mark in track data
 * @param track Track to search
 * @param start Starting offset
 * @param mark Address mark to find
 * @param fm True for FM, false for MFM
 * @return Offset to mark or -1 if not found
 */
int uft_dmk_find_mark(const uft_dmk_track_t* track, uint16_t start,
                      uint8_t mark, bool fm);

/**
 * @brief Check if offset contains valid MFM sync pattern
 * @param track Track data
 * @param offset Offset to check
 * @return true if 3x A1 sync found
 */
bool uft_dmk_is_mfm_sync(const uft_dmk_track_t* track, uint16_t offset);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DMK_H */
