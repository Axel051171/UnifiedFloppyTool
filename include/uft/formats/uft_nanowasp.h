/**
 * @file uft_nanowasp.h
 * @brief NanoWasp floppy image format support
 * @version 3.9.0
 * 
 * NanoWasp format used by the NanoWasp Microbee emulator.
 * Simple format with 80-byte header followed by raw track data.
 * 
 * Reference: libdsk drvnwasp.c
 */

#ifndef UFT_NANOWASP_H
#define UFT_NANOWASP_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NanoWasp signature */
#define NANOWASP_SIGNATURE      "nanowasp floppy image\r\n\032"
#define NANOWASP_SIGNATURE_LEN  24
#define NANOWASP_HEADER_SIZE    80

/* Default geometry (Microbee 3.5" DS DD) */
#define NANOWASP_DEF_CYLS       80
#define NANOWASP_DEF_HEADS      2
#define NANOWASP_DEF_SECTORS    10
#define NANOWASP_DEF_SECSIZE    512

/**
 * @brief NanoWasp file header
 */
#pragma pack(push, 1)
typedef struct {
    char     signature[24];      /* "nanowasp floppy image\r\n\032" */
    uint8_t  version;            /* Version (usually 0) */
    uint8_t  cylinders;          /* Number of cylinders */
    uint8_t  heads;              /* Number of heads */
    uint8_t  sectors;            /* Sectors per track */
    uint16_t sector_size;        /* Bytes per sector (little-endian) */
    uint8_t  reserved[50];       /* Reserved/padding */
} nanowasp_header_t;
#pragma pack(pop)

/**
 * @brief NanoWasp read result
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
    
    /* Statistics */
    size_t image_size;
    size_t data_size;
    
} nanowasp_read_result_t;

/* ============================================================================
 * NanoWasp File I/O
 * ============================================================================ */

/**
 * @brief Read NanoWasp file
 */
uft_error_t uft_nanowasp_read(const char *path,
                              uft_disk_image_t **out_disk,
                              nanowasp_read_result_t *result);

/**
 * @brief Read NanoWasp from memory
 */
uft_error_t uft_nanowasp_read_mem(const uint8_t *data, size_t size,
                                  uft_disk_image_t **out_disk,
                                  nanowasp_read_result_t *result);

/**
 * @brief Write NanoWasp file
 */
uft_error_t uft_nanowasp_write(const uft_disk_image_t *disk,
                               const char *path);

/**
 * @brief Validate NanoWasp header
 */
bool uft_nanowasp_validate_header(const nanowasp_header_t *header);

/**
 * @brief Probe if data is NanoWasp format
 */
bool uft_nanowasp_probe(const uint8_t *data, size_t size, int *confidence);

#ifdef __cplusplus
}
#endif

#endif /* UFT_NANOWASP_H */
