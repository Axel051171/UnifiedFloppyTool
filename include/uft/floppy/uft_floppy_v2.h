/**
 * @file uft_floppy_v2.h
 * @brief UFT Floppy Library v2 - Unified Disk Interface
 * 
 * EXT4-016: Modern unified floppy disk library
 */

#ifndef UFT_FLOPPY_V2_H
#define UFT_FLOPPY_V2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Types
 *===========================================================================*/

/* Opaque disk handle */
typedef struct uft_disk_s uft_disk_t;

/* Disk format types */
typedef enum {
    UFT_FORMAT_UNKNOWN = 0,
    UFT_FORMAT_RAW,         /* Raw sector image (IMG, IMA, DSK) */
    UFT_FORMAT_D64,         /* Commodore 64 */
    UFT_FORMAT_D81,         /* Commodore 128/1581 */
    UFT_FORMAT_ADF,         /* Amiga */
    UFT_FORMAT_ST,          /* Atari ST */
    UFT_FORMAT_MSA,         /* Atari MSA */
    UFT_FORMAT_HFE,         /* HxC */
    UFT_FORMAT_IMD,         /* ImageDisk */
    UFT_FORMAT_TD0,         /* Teledisk */
    UFT_FORMAT_FDI,         /* FDI */
    UFT_FORMAT_DSK,         /* CPC DSK */
    UFT_FORMAT_EDSK,        /* Extended DSK */
    UFT_FORMAT_SAD,         /* SAM Coupé SAD */
    UFT_FORMAT_NIB,         /* Apple NIB */
    UFT_FORMAT_WOZ,         /* Apple WOZ */
    UFT_FORMAT_FLUX         /* Raw flux */
} uft_disk_format_t;

/* Disk geometry */
typedef struct {
    int tracks;
    int sides;
    int sectors;
    int sector_size;
    size_t total_size;
} uft_disk_geometry_t;

/*===========================================================================
 * Disk Operations
 *===========================================================================*/

/**
 * Create a new disk handle
 * @return New disk handle or NULL on error
 */
uft_disk_t *uft_disk_create(void);

/**
 * Destroy a disk handle and free resources
 * @param disk Disk handle
 */
void uft_disk_destroy(uft_disk_t *disk);

/**
 * Open a disk image from memory
 * @param disk Disk handle
 * @param data Image data
 * @param size Image size
 * @return 0 on success, -1 on error
 */
int uft_disk_open(uft_disk_t *disk, const uint8_t *data, size_t size);

/**
 * Open a disk image from file
 * @param disk Disk handle
 * @param filename Path to image file
 * @return 0 on success, -1 on error
 */
int uft_disk_open_file(uft_disk_t *disk, const char *filename);

/**
 * Save disk image to file
 * @param disk Disk handle
 * @param filename Path to output file
 * @return 0 on success, -1 on error
 */
int uft_disk_save(uft_disk_t *disk, const char *filename);

/**
 * Create a blank disk image
 * @param disk Disk handle
 * @param geometry Disk geometry
 * @return 0 on success, -1 on error
 */
int uft_disk_create_blank(uft_disk_t *disk, const uft_disk_geometry_t *geometry);

/**
 * Format disk (fill with pattern)
 * @param disk Disk handle
 * @param fill_byte Fill pattern
 * @return 0 on success, -1 on error
 */
int uft_disk_format(uft_disk_t *disk, uint8_t fill_byte);

/*===========================================================================
 * Geometry
 *===========================================================================*/

/**
 * Get disk geometry
 */
int uft_disk_get_geometry(uft_disk_t *disk, uft_disk_geometry_t *geometry);

/**
 * Set disk geometry
 */
int uft_disk_set_geometry(uft_disk_t *disk, const uft_disk_geometry_t *geometry);

/*===========================================================================
 * Sector Operations
 *===========================================================================*/

/**
 * Read a sector
 * @param disk Disk handle
 * @param track Track number (0-based)
 * @param side Side number (0-based)
 * @param sector Sector number (1-based)
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes read or -1 on error
 */
int uft_disk_read_sector(uft_disk_t *disk, int track, int side, int sector,
                         uint8_t *buffer, size_t size);

/**
 * Write a sector
 * @param disk Disk handle
 * @param track Track number (0-based)
 * @param side Side number (0-based)
 * @param sector Sector number (1-based)
 * @param buffer Input buffer
 * @param size Data size
 * @return Bytes written or -1 on error
 */
int uft_disk_write_sector(uft_disk_t *disk, int track, int side, int sector,
                          const uint8_t *buffer, size_t size);

/**
 * Read an entire track
 * @param disk Disk handle
 * @param track Track number (0-based)
 * @param side Side number (0-based)
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes read or -1 on error
 */
int uft_disk_read_track(uft_disk_t *disk, int track, int side,
                        uint8_t *buffer, size_t size);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * Get disk format
 */
uft_disk_format_t uft_disk_get_format(uft_disk_t *disk);

/**
 * Get format name string
 */
const char *uft_disk_format_name(uft_disk_format_t format);

/**
 * Check if disk has been modified
 */
bool uft_disk_is_modified(uft_disk_t *disk);

/**
 * Get disk image size
 */
size_t uft_disk_get_size(uft_disk_t *disk);

/**
 * Get raw disk data pointer
 */
const uint8_t *uft_disk_get_data(uft_disk_t *disk);

/**
 * Get last error message
 */
const char *uft_disk_get_error(uft_disk_t *disk);

/**
 * Generate JSON info report
 */
int uft_disk_info_json(uft_disk_t *disk, char *buffer, size_t size);

/*===========================================================================
 * Standard Geometries
 *===========================================================================*/

/* PC formats */
#define UFT_GEOM_PC_160K    {40, 1, 8, 512, 163840}
#define UFT_GEOM_PC_180K    {40, 1, 9, 512, 184320}
#define UFT_GEOM_PC_320K    {40, 2, 8, 512, 327680}
#define UFT_GEOM_PC_360K    {40, 2, 9, 512, 368640}
#define UFT_GEOM_PC_720K    {80, 2, 9, 512, 737280}
#define UFT_GEOM_PC_1200K   {80, 2, 15, 512, 1228800}
#define UFT_GEOM_PC_1440K   {80, 2, 18, 512, 1474560}
#define UFT_GEOM_PC_2880K   {80, 2, 36, 512, 2949120}

/* Amiga */
#define UFT_GEOM_AMIGA_DD   {80, 2, 11, 512, 901120}
#define UFT_GEOM_AMIGA_HD   {80, 2, 22, 512, 1802240}

/* Atari ST */
#define UFT_GEOM_ATARI_SS   {80, 1, 9, 512, 368640}
#define UFT_GEOM_ATARI_DS   {80, 2, 9, 512, 737280}

/* Commodore */
#define UFT_GEOM_C64_35     {35, 1, 0, 256, 174848}  /* Variable sectors */
#define UFT_GEOM_C64_40     {40, 1, 0, 256, 196608}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLOPPY_V2_H */
