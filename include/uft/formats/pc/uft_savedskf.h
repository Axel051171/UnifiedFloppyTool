/**
 * @file uft_savedskf.h
 * @brief IBM OS/2 SaveDskF (Disk Save Format) support
 *
 * SaveDskF is the disk imaging format used by IBM's OS/2 SAVEDSKF/LOADDSKF
 * utilities. It stores floppy disk images with optional RLE or LZSS compression.
 *
 * Reference: OS/2 Museum, Wikipedia "SAVEDSKF"
 *
 * @version 1.0.0
 * @date 2026-04-10
 */

#ifndef UFT_SAVEDSKF_H
#define UFT_SAVEDSKF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * SAVEDSKF FORMAT DEFINITIONS
 *===========================================================================*/

#define UFT_SAVEDSKF_MAGIC          0x5A4B  /* "KZ" in little-endian */
#define UFT_SAVEDSKF_HEADER_SIZE    23

/* Compression methods */
#define UFT_SAVEDSKF_COMP_NONE      0       /* Uncompressed */
#define UFT_SAVEDSKF_COMP_RLE       1       /* Run-Length Encoding */
#define UFT_SAVEDSKF_COMP_LZSS      2       /* LZSS (Lempel-Ziv-Storer-Szymanski) */

/* Maximum geometry values */
#define UFT_SAVEDSKF_MAX_CYLINDERS  85
#define UFT_SAVEDSKF_MAX_HEADS      2
#define UFT_SAVEDSKF_MAX_SECTORS    36
#define UFT_SAVEDSKF_MAX_SEC_SIZE   8192

/* RLE escape byte */
#define UFT_SAVEDSKF_RLE_ESCAPE     0x00

/*===========================================================================
 * STRUCTURES
 *===========================================================================*/

#pragma pack(push, 1)

/**
 * @brief SaveDskF file header (23 bytes)
 */
typedef struct {
    uint16_t magic;                 /* 0x5A4B */
    uint16_t sector_size;           /* Bytes per sector (usually 512) */
    uint16_t sectors_per_track;     /* Sectors per track */
    uint16_t heads;                 /* Number of heads/sides */
    uint16_t cylinders;             /* Number of cylinders */
    uint8_t  compression;           /* 0=none, 1=RLE, 2=LZSS */
    uint8_t  reserved[12];          /* Reserved bytes */
} uft_savedskf_header_t;

#pragma pack(pop)

/*===========================================================================
 * RUNTIME STRUCTURES
 *===========================================================================*/

/**
 * @brief SaveDskF image context
 */
typedef struct {
    uft_savedskf_header_t header;

    /* Decompressed sector data */
    uint8_t *sector_data;
    size_t   sector_data_size;

    /* Calculated geometry */
    uint32_t total_sectors;
    uint32_t disk_size;             /* Total uncompressed size */

    bool     is_open;
} uft_savedskf_image_t;

/**
 * @brief SaveDskF read result
 */
typedef struct {
    bool        success;
    int         error_code;
    const char *error_detail;

    uint16_t    cylinders;
    uint16_t    heads;
    uint16_t    sectors_per_track;
    uint16_t    sector_size;
    uint8_t     compression;

    size_t      compressed_size;
    size_t      uncompressed_size;
} uft_savedskf_read_result_t;

/*===========================================================================
 * API FUNCTIONS
 *===========================================================================*/

/**
 * @brief Initialize SaveDskF image structure
 */
void uft_savedskf_image_init(uft_savedskf_image_t *image);

/**
 * @brief Free SaveDskF image resources
 */
void uft_savedskf_image_free(uft_savedskf_image_t *image);

/**
 * @brief Probe if data is SaveDskF format
 */
bool uft_savedskf_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Read SaveDskF file
 * @param path    Path to .dsk file
 * @param image   Output image structure
 * @param result  Optional read result details
 * @return 0 on success, negative on error
 */
int uft_savedskf_read(const char *path, uft_savedskf_image_t *image,
                      uft_savedskf_read_result_t *result);

/**
 * @brief Read sector from parsed SaveDskF image
 * @param image   Parsed image
 * @param cyl     Cylinder number
 * @param head    Head number
 * @param sector  Sector number (1-based)
 * @param buffer  Output buffer
 * @param size    Buffer size
 * @return Bytes read, or negative on error
 */
int uft_savedskf_read_sector(const uft_savedskf_image_t *image,
                             uint16_t cyl, uint8_t head, uint16_t sector,
                             uint8_t *buffer, size_t size);

/**
 * @brief Decompress RLE-encoded data
 * @param src     Compressed data
 * @param src_len Compressed data length
 * @param dst     Output buffer
 * @param dst_len Output buffer size
 * @param out_len Actual bytes written
 * @return 0 on success, negative on error
 */
int uft_savedskf_decompress_rle(const uint8_t *src, size_t src_len,
                                uint8_t *dst, size_t dst_len,
                                size_t *out_len);

/**
 * @brief Get compression method description
 */
const char *uft_savedskf_compression_name(uint8_t method);

/**
 * @brief Convert SaveDskF to raw IMG
 */
int uft_savedskf_to_img(const char *dskf_path, const char *img_path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SAVEDSKF_H */
