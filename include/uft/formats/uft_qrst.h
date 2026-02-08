/**
 * @file uft_qrst.h
 * @brief QRST (Compaq Quick Release Sector Transfer) format support
 * @version 3.9.0
 * 
 * QRST format was used by Compaq for quick disk imaging.
 * Similar to CopyQM but with different compression scheme.
 * 
 * Reference: libdsk drvqrst.c
 */

#ifndef UFT_QRST_H
#define UFT_QRST_H

#include "uft/uft_format_common.h"
#include "uft/core/uft_disk_image_compat.h"
#include "uft/core/uft_error_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* QRST signature */
#define QRST_SIGNATURE          "QRST"
#define QRST_SIGNATURE_LEN      4
#define QRST_HEADER_SIZE        22

/* QRST compression types */
#define QRST_COMP_NONE          0
#define QRST_COMP_RLE           1

/**
 * @brief QRST file header
 */
#pragma pack(push, 1)
typedef struct {
    char     signature[4];       /* "QRST" */
    uint16_t version;            /* Version number */
    uint16_t cylinders;          /* Number of cylinders */
    uint16_t heads;              /* Number of heads */
    uint16_t sectors;            /* Sectors per track */
    uint16_t sector_size;        /* Bytes per sector */
    uint8_t  compression;        /* Compression type */
    uint8_t  reserved[5];        /* Reserved */
} qrst_header_t;
#pragma pack(pop)

/**
 * @brief QRST track header (appears before each track)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t cylinder;           /* Track cylinder number */
    uint8_t  head;               /* Track head number */
    uint8_t  compressed;         /* 1 if track is compressed */
    uint32_t data_size;          /* Size of track data */
} qrst_track_header_t;
#pragma pack(pop)

/**
 * @brief QRST read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    /* Image info */
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors;
    uint16_t sector_size;
    
    /* Statistics */
    uint32_t total_tracks;
    uint32_t compressed_tracks;
    size_t original_size;
    size_t compressed_size;
    
} qrst_read_result_t;

/**
 * @brief QRST write options
 */
typedef struct {
    bool use_compression;        /* Use RLE compression */
} qrst_write_options_t;

/* ============================================================================
 * QRST RLE Compression
 * ============================================================================ */

/**
 * @brief Decompress QRST RLE data
 * @return Decompressed size, or -1 on error
 */
int qrst_rle_decompress(const uint8_t *input, size_t input_size,
                        uint8_t *output, size_t output_size);

/**
 * @brief Compress data using QRST RLE
 * @return Compressed size, or -1 on error
 */
int qrst_rle_compress(const uint8_t *input, size_t input_size,
                      uint8_t *output, size_t output_capacity);

/* ============================================================================
 * QRST File I/O
 * ============================================================================ */

/**
 * @brief Read QRST file
 */
uft_error_t uft_qrst_read(const char *path,
                          uft_disk_image_t **out_disk,
                          qrst_read_result_t *result);

/**
 * @brief Read QRST from memory
 */
uft_error_t uft_qrst_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              qrst_read_result_t *result);

/**
 * @brief Write QRST file
 */
uft_error_t uft_qrst_write(const uft_disk_image_t *disk,
                           const char *path,
                           const qrst_write_options_t *opts);

/**
 * @brief Validate QRST header
 */
bool uft_qrst_validate_header(const qrst_header_t *header);

/**
 * @brief Probe if data is QRST format
 */
bool uft_qrst_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Initialize write options with defaults
 */
void uft_qrst_write_options_init(qrst_write_options_t *opts);

#ifdef __cplusplus
}
#endif

#endif /* UFT_QRST_H */
