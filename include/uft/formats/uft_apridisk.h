/**
 * @file uft_apridisk.h
 * @brief ApriDisk format support
 * @version 3.9.0
 * 
 * ApriDisk format was created by the APRIDISK.EXE utility from Apricot computers.
 * Used for archiving Apricot MS-DOS and other disk formats.
 * 
 * File structure:
 * - 128-byte header (signature + info)
 * - Series of track/sector records
 * - Each record has: 16-byte descriptor + compressed/raw data
 * 
 * Reference: libdsk drvapdsk.c
 */

#ifndef UFT_APRIDISK_H
#define UFT_APRIDISK_H

#include "uft/uft_format_common.h"
#include "uft/core/uft_disk_image_compat.h"
#include "uft/core/uft_error_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ApriDisk signature */
#define APRIDISK_SIGNATURE      "ACT Apricot disk image\032\004"
#define APRIDISK_SIGNATURE_LEN  24
#define APRIDISK_HEADER_SIZE    128

/* Record types */
#define APRIDISK_DELETED        0x00000000  /* Deleted sector */
#define APRIDISK_SECTOR         0x00000002  /* Normal sector */
#define APRIDISK_COMMENT        0x00000001  /* Comment record */
#define APRIDISK_CREATOR        0x00000003  /* Creator record */

/* Compression types */
#define APRIDISK_COMP_NONE      0x00000000  /* Uncompressed */
#define APRIDISK_COMP_RLE       0x00000001  /* RLE compressed */

/**
 * @brief ApriDisk file header
 */
#pragma pack(push, 1)
typedef struct {
    char     signature[24];      /* "ACT Apricot disk image\032\004" */
    uint8_t  reserved[104];      /* Padding to 128 bytes */
} apridisk_header_t;

/**
 * @brief ApriDisk record descriptor
 */
typedef struct {
    uint32_t type;               /* Record type */
    uint32_t compression;        /* Compression method */
    uint32_t header_size;        /* Size of this header (16) */
    uint32_t data_size;          /* Size of following data */
} apridisk_record_desc_t;

/**
 * @brief ApriDisk sector descriptor (follows record_desc for sector records)
 */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;          /* 0=128, 1=256, 2=512, 3=1024 */
    uint8_t  reserved[4];
} apridisk_sector_desc_t;
#pragma pack(pop)

/**
 * @brief ApriDisk read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    /* Image info */
    uint16_t max_cylinder;
    uint8_t  max_head;
    uint8_t  max_sector;
    uint16_t sector_size;
    
    /* Comment */
    char *comment;
    size_t comment_len;
    
    /* Statistics */
    uint32_t total_sectors;
    uint32_t deleted_sectors;
    uint32_t rle_sectors;
    
} apridisk_read_result_t;

/**
 * @brief ApriDisk write options
 */
typedef struct {
    bool use_rle;                /* Use RLE compression */
    const char *comment;         /* Optional comment */
    const char *creator;         /* Creator string */
} apridisk_write_options_t;

/* ============================================================================
 * ApriDisk RLE Compression
 * ============================================================================ */

/**
 * @brief Decompress ApriDisk RLE data
 * @return Decompressed size, or -1 on error
 */
int apridisk_rle_decompress(const uint8_t *input, size_t input_size,
                            uint8_t *output, size_t output_size);

/**
 * @brief Compress data using ApriDisk RLE
 * @return Compressed size, or -1 on error (or if compression not beneficial)
 */
int apridisk_rle_compress(const uint8_t *input, size_t input_size,
                          uint8_t *output, size_t output_capacity);

/* ============================================================================
 * ApriDisk File I/O
 * ============================================================================ */

/**
 * @brief Read ApriDisk file
 */
uft_error_t uft_apridisk_read(const char *path,
                              uft_disk_image_t **out_disk,
                              apridisk_read_result_t *result);

/**
 * @brief Read ApriDisk from memory
 */
uft_error_t uft_apridisk_read_mem(const uint8_t *data, size_t size,
                                  uft_disk_image_t **out_disk,
                                  apridisk_read_result_t *result);

/**
 * @brief Write ApriDisk file
 */
uft_error_t uft_apridisk_write(const uft_disk_image_t *disk,
                               const char *path,
                               const apridisk_write_options_t *opts);

/**
 * @brief Validate ApriDisk header
 */
bool uft_apridisk_validate_header(const apridisk_header_t *header);

/**
 * @brief Initialize write options with defaults
 */
void uft_apridisk_write_options_init(apridisk_write_options_t *opts);

/**
 * @brief Probe if data is ApriDisk format
 */
bool uft_apridisk_probe(const uint8_t *data, size_t size, int *confidence);

#ifdef __cplusplus
}
#endif

#endif /* UFT_APRIDISK_H */
