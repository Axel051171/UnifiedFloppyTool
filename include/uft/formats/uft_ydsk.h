/**
 * @file uft_ydsk.h
 * @brief YAZE (Yet Another Z80 Emulator) ydsk format support
 * @version 3.9.0
 * 
 * YAZE ydsk format used by the YAZE CP/M emulator.
 * Simple format with 128-byte header followed by raw sector data.
 * 
 * Reference: libdsk drvydsk.c by John Elliott
 */

#ifndef UFT_YDSK_H
#define UFT_YDSK_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* YDSK signature */
#define YDSK_SIGNATURE          "<CPM_Disk>"
#define YDSK_SIGNATURE_LEN      10
#define YDSK_HEADER_SIZE        128

/**
 * @brief YDSK file header
 */
#pragma pack(push, 1)
typedef struct {
    char     signature[10];      /* "<CPM_Disk>" */
    uint8_t  cylinders;          /* Number of cylinders */
    uint8_t  heads;              /* Number of heads (sides) */
    uint8_t  sectors;            /* Sectors per track */
    uint8_t  sector_size_code;   /* 0=128, 1=256, 2=512, 3=1024 */
    uint8_t  first_sector;       /* First sector number (usually 1) */
    uint8_t  reserved[113];      /* Padding to 128 bytes */
} ydsk_header_t;
#pragma pack(pop)

/**
 * @brief YDSK read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    /* Image info */
    uint8_t cylinders;
    uint8_t heads;
    uint8_t sectors;
    uint16_t sector_size;
    uint8_t first_sector;
    
    /* Statistics */
    size_t image_size;
    size_t data_size;
    
} ydsk_read_result_t;

/* ============================================================================
 * YDSK File I/O
 * ============================================================================ */

/**
 * @brief Read YDSK file
 */
uft_error_t uft_ydsk_read(const char *path,
                          uft_disk_image_t **out_disk,
                          ydsk_read_result_t *result);

/**
 * @brief Read YDSK from memory
 */
uft_error_t uft_ydsk_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              ydsk_read_result_t *result);

/**
 * @brief Write YDSK file
 */
uft_error_t uft_ydsk_write(const uft_disk_image_t *disk,
                           const char *path);

/**
 * @brief Validate YDSK header
 */
bool uft_ydsk_validate_header(const ydsk_header_t *header);

/**
 * @brief Probe if data is YDSK format
 */
bool uft_ydsk_probe(const uint8_t *data, size_t size, int *confidence);

#ifdef __cplusplus
}
#endif

#endif /* UFT_YDSK_H */
