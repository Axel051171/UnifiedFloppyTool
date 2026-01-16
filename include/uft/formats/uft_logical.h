/**
 * @file uft_logical.h
 * @brief Logical Disk format support
 * @version 3.9.0
 * 
 * Logical disk format for storing disk images with explicit geometry header.
 * Reference: libdsk drvlogi.c
 */

#ifndef UFT_LOGICAL_H
#define UFT_LOGICAL_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Logical disk signature */
#define LOGICAL_SIGNATURE      "LGD\0"
#define LOGICAL_SIGNATURE_LEN  4
#define LOGICAL_HEADER_SIZE    32

/**
 * @brief Logical disk header
 */
#pragma pack(push, 1)
typedef struct {
    char     signature[4];       /* "LGD\0" */
    uint16_t cylinders;          /* Number of cylinders */
    uint16_t heads;              /* Number of heads */
    uint16_t sectors;            /* Sectors per track */
    uint16_t sector_size;        /* Bytes per sector */
    uint8_t  first_sector;       /* First sector number */
    uint8_t  encoding;           /* 0=FM, 1=MFM */
    uint16_t data_rate;          /* Data rate (kbps) */
    uint8_t  reserved[14];       /* Reserved */
} logical_header_t;
#pragma pack(pop)

/**
 * @brief Logical disk read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors;
    uint16_t sector_size;
    size_t   image_size;
    
} logical_read_result_t;

/* ============================================================================
 * Logical Disk File I/O
 * ============================================================================ */

/**
 * @brief Read Logical disk file
 */
uft_error_t uft_logical_read(const char *path,
                             uft_disk_image_t **out_disk,
                             logical_read_result_t *result);

/**
 * @brief Read Logical disk from memory
 */
uft_error_t uft_logical_read_mem(const uint8_t *data, size_t size,
                                 uft_disk_image_t **out_disk,
                                 logical_read_result_t *result);

/**
 * @brief Write Logical disk file
 */
uft_error_t uft_logical_write(const uft_disk_image_t *disk,
                              const char *path);

/**
 * @brief Validate Logical disk header
 */
bool uft_logical_validate_header(const logical_header_t *header);

/**
 * @brief Probe if data is Logical disk format
 */
bool uft_logical_probe(const uint8_t *data, size_t size, int *confidence);

#ifdef __cplusplus
}
#endif

#endif /* UFT_LOGICAL_H */
