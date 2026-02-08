/**
 * @file uft_opus.h
 * @brief OPUS Discovery disk format support
 * @version 3.9.0
 * 
 * OPUS Discovery was a disk-based storage system for the ZX Spectrum.
 * Format: 40 tracks, single-sided, 18 sectors of 256 bytes.
 * Total capacity: 180KB formatted.
 * 
 * Reference: libdsk drvopus.c, World of Spectrum
 */

#ifndef UFT_OPUS_H
#define UFT_OPUS_H

#include "uft/uft_format_common.h"
#include "uft/core/uft_disk_image_compat.h"
#include "uft/core/uft_error_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OPUS Discovery disk parameters */
#define OPUS_CYLINDERS          40
#define OPUS_HEADS              1
#define OPUS_SECTORS            18
#define OPUS_SECTOR_SIZE        256
#define OPUS_FIRST_SECTOR       0
#define OPUS_TRACK_SIZE         (OPUS_SECTORS * OPUS_SECTOR_SIZE)  /* 4608 bytes */
#define OPUS_DISK_SIZE          (OPUS_CYLINDERS * OPUS_TRACK_SIZE) /* 184320 bytes */

/* OPUS directory structure */
#define OPUS_DIR_TRACK          0       /* Directory is on track 0 */
#define OPUS_DIR_ENTRIES        80      /* Maximum directory entries */
#define OPUS_DIR_ENTRY_SIZE     32      /* Bytes per directory entry */

/**
 * @brief OPUS directory entry (32 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  status;            /* 0=free, 1=used, other=deleted */
    char     filename[10];      /* Filename (space-padded) */
    uint8_t  type;              /* File type */
    uint16_t length;            /* File length */
    uint16_t start_address;     /* Load address */
    uint8_t  start_track;       /* First track */
    uint8_t  start_sector;      /* First sector */
    uint8_t  reserved[11];      /* Reserved */
} opus_dir_entry_t;
#pragma pack(pop)

/* File types */
#define OPUS_TYPE_BASIC         0       /* BASIC program */
#define OPUS_TYPE_CODE          3       /* Machine code */
#define OPUS_TYPE_DATA          1       /* Data array */
#define OPUS_TYPE_STRING        2       /* String array */

/**
 * @brief OPUS read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    uint8_t  cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
    size_t   image_size;
    
    /* Directory info */
    uint32_t dir_entries;
    uint32_t used_entries;
    
} opus_read_result_t;

/* ============================================================================
 * OPUS Disk Functions
 * ============================================================================ */

/**
 * @brief Read OPUS disk image
 */
uft_error_t uft_opus_read(const char *path,
                          uft_disk_image_t **out_disk,
                          opus_read_result_t *result);

/**
 * @brief Read OPUS disk from memory
 */
uft_error_t uft_opus_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              opus_read_result_t *result);

/**
 * @brief Write OPUS disk image
 */
uft_error_t uft_opus_write(const uft_disk_image_t *disk,
                           const char *path);

/**
 * @brief Probe if data is OPUS format
 */
bool uft_opus_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Read OPUS directory
 */
uft_error_t uft_opus_read_directory(const uft_disk_image_t *disk,
                                    opus_dir_entry_t *entries,
                                    size_t max_entries,
                                    size_t *entry_count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_OPUS_H */
