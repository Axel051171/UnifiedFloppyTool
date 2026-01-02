// SPDX-License-Identifier: MIT
/*
 * dmk.h - DMK (Disk Master Kopyright) Format Support
 * 
 * DMK Format Specification:
 * - Created by David Keil for TRS-80 emulators
 * - Used for TRS-80, CP/M, and other platforms
 * - Supports variable sector sizes (128, 256, 512, 1024 bytes)
 * - Single/double density and single/double sided
 * - Track-based format with IDAM pointers
 * 
 * @version 2.8.5
 * @date 2024-12-26
 */

#ifndef DMK_H
#define DMK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * DMK CONSTANTS
 *============================================================================*/

#define DMK_HEADER_SIZE         16
#define DMK_TRACK_HEADER_SIZE   128     /* IDAM pointer table */
#define DMK_MAX_TRACKS          96      /* Typical maximum */
#define DMK_MAX_SIDES           2

/* Write protect */
#define DMK_WRITE_PROTECTED     0xFF
#define DMK_NOT_WRITE_PROTECTED 0x00

/* Flags (byte 4) */
#define DMK_FLAG_SINGLE_SIDED   0x00
#define DMK_FLAG_DOUBLE_SIDED   0x10
#define DMK_FLAG_SINGLE_DENSITY 0x00
#define DMK_FLAG_DOUBLE_DENSITY 0x40
#define DMK_FLAG_IGNORE_DENSITY 0x80

/* IDAM pointer flags */
#define DMK_IDAM_OFFSET_MASK    0x3FFF  /* Bits 0-13 */
#define DMK_IDAM_UNDEFINED      0x4000  /* Bit 14 */
#define DMK_IDAM_DOUBLE_DENSITY 0x8000  /* Bit 15 */

/* Standard track lengths */
#define DMK_TRACK_LENGTH_SD_SS  3072    /* Single density, single sided */
#define DMK_TRACK_LENGTH_SD_DS  3200    /* Single density, double sided */
#define DMK_TRACK_LENGTH_DD_SS  6272    /* Double density, single sided */
#define DMK_TRACK_LENGTH_DD_DS  6400    /* Double density, double sided */

/* Sector sizes */
#define DMK_SECTOR_SIZE_128     128
#define DMK_SECTOR_SIZE_256     256
#define DMK_SECTOR_SIZE_512     512
#define DMK_SECTOR_SIZE_1024    1024

/* Maximum sectors per track */
#define DMK_MAX_SECTORS         64

/*============================================================================*
 * DMK STRUCTURES
 *============================================================================*/

/**
 * @brief DMK file header (16 bytes)
 */
typedef struct {
    uint8_t  write_protect;     /* 0xFF = protected, 0x00 = not */
    uint8_t  tracks;            /* Number of tracks (0-based, add 1) */
    uint16_t track_length;      /* Length of each track in bytes */
    uint8_t  flags;             /* Density and side flags */
    uint8_t  reserved[11];      /* Reserved (0x00) */
} __attribute__((packed)) dmk_header_t;

/**
 * @brief IDAM (ID Address Mark) pointer entry
 * 
 * Each track has a 128-byte header with up to 64 IDAM pointers.
 * Each pointer is 2 bytes (little endian).
 */
typedef struct {
    uint16_t offset;            /* Offset to IDAM + flags */
} __attribute__((packed)) dmk_idam_pointer_t;

/**
 * @brief Sector information
 */
typedef struct {
    uint8_t  track;             /* Track number */
    uint8_t  side;              /* Side number (0 or 1) */
    uint8_t  sector;            /* Sector number */
    uint8_t  size_code;         /* Size code (0=128, 1=256, 2=512, 3=1024) */
    uint16_t crc;               /* CRC of ID field */
    uint8_t  *data;             /* Pointer to sector data */
    size_t   data_size;         /* Actual data size */
    bool     double_density;    /* DD (true) or SD (false) */
    bool     deleted;           /* Deleted data mark */
    bool     crc_error;         /* CRC error flag */
} dmk_sector_t;

/**
 * @brief DMK track information
 */
typedef struct {
    uint8_t  track_num;         /* Physical track number */
    uint8_t  side;              /* Side number (0 or 1) */
    uint16_t num_sectors;       /* Number of sectors in track */
    dmk_sector_t sectors[DMK_MAX_SECTORS];
    uint8_t  *raw_data;         /* Raw track data (MFM/FM) */
    size_t   raw_size;          /* Size of raw data */
} dmk_track_t;

/**
 * @brief DMK image container (in-memory representation)
 */
typedef struct {
    /* Header */
    dmk_header_t header;
    
    /* Track data */
    uint8_t num_tracks;
    uint8_t num_sides;
    dmk_track_t *tracks;        /* Array of tracks */
    size_t num_track_entries;
    
    /* File info */
    char *filename;
    bool modified;
} dmk_image_t;

/*============================================================================*
 * DMK API
 *============================================================================*/

/**
 * @brief Read DMK image from file
 * 
 * @param filename Path to DMK file
 * @param image Output image structure
 * @return true on success
 */
bool dmk_read(const char *filename, dmk_image_t *image);

/**
 * @brief Write DMK image to file
 * 
 * @param filename Path to output file
 * @param image Image to write
 * @return true on success
 */
bool dmk_write(const char *filename, const dmk_image_t *image);

/**
 * @brief Initialize empty DMK image
 * 
 * @param image Image to initialize
 * @param num_tracks Number of tracks
 * @param num_sides Number of sides (1 or 2)
 * @param double_density true for DD, false for SD
 * @return true on success
 */
bool dmk_init(dmk_image_t *image, uint8_t num_tracks, uint8_t num_sides,
              bool double_density);

/**
 * @brief Free DMK image resources
 * 
 * @param image Image to free
 */
void dmk_free(dmk_image_t *image);

/**
 * @brief Get sector data from DMK image
 * 
 * @param image Image to read from
 * @param track Track number
 * @param side Side number (0 or 1)
 * @param sector Sector number
 * @param data Output buffer for sector data
 * @param size Output size of sector data
 * @return true on success
 */
bool dmk_get_sector(const dmk_image_t *image, uint8_t track, uint8_t side,
                    uint8_t sector, uint8_t **data, size_t *size);

/**
 * @brief Write sector data to DMK image
 * 
 * @param image Image to modify
 * @param track Track number
 * @param side Side number (0 or 1)
 * @param sector Sector number
 * @param data Sector data
 * @param size Size of sector data
 * @return true on success
 */
bool dmk_write_sector(dmk_image_t *image, uint8_t track, uint8_t side,
                      uint8_t sector, const uint8_t *data, size_t size);

/**
 * @brief Get track information
 * 
 * @param image Image to read from
 * @param track Track number
 * @param side Side number
 * @param track_info Output track information
 * @return true on success
 */
bool dmk_get_track(const dmk_image_t *image, uint8_t track, uint8_t side,
                   dmk_track_t **track_info);

/**
 * @brief Validate DMK image
 * 
 * @param image Image to validate
 * @param errors Output buffer for error messages (optional)
 * @param errors_size Size of error buffer
 * @return true if valid
 */
bool dmk_validate(const dmk_image_t *image, char *errors, size_t errors_size);

/**
 * @brief Get sector size from size code
 * 
 * @param size_code Size code (0-3)
 * @return Sector size in bytes
 */
size_t dmk_sector_size(uint8_t size_code);

/**
 * @brief Get size code from sector size
 * 
 * @param size Sector size in bytes
 * @return Size code (0-3) or 0xFF on error
 */
uint8_t dmk_size_code(size_t size);

/**
 * @brief Calculate CRC-16 (CCITT) for DMK data
 * 
 * @param data Data to checksum
 * @param size Size of data
 * @param initial Initial CRC value (typically 0xFFFF)
 * @return CRC-16 value
 */
uint16_t dmk_crc16(const uint8_t *data, size_t size, uint16_t initial);

/**
 * @brief Format track in DMK image
 * 
 * @param image Image to modify
 * @param track Track number
 * @param side Side number
 * @param num_sectors Number of sectors
 * @param sector_size Sector size in bytes
 * @param fill_byte Fill byte for sector data
 * @return true on success
 */
bool dmk_format_track(dmk_image_t *image, uint8_t track, uint8_t side,
                      uint8_t num_sectors, size_t sector_size,
                      uint8_t fill_byte);

#ifdef __cplusplus
}
#endif

#endif /* DMK_H */
