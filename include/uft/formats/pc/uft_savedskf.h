/**
 * @file uft_savedskf.h
 * @brief IBM OS/2 SaveDskF (Disk Save Format) support
 *
 * SaveDskF is the disk imaging format used by IBM's OS/2 SAVEDSKF/LOADDSKF
 * utilities. It stores a FAT12 floppy image, either uncompressed or compressed
 * with IBM's LZW (the OS/2 PACK scheme) — NOT RLE and NOT LZSS.
 *
 * Authoritative layout + magic verified against Deark (jsummers/deark
 * modules/fat.c::de_run_loaddskf + loaddskf_read_header) and the public-domain
 * dskdcmps.c LZW decompressor (deark foreign/dskdcmps.h, modname "ibmlzw"); the
 * archiveteam/justsolve LoadDskF/SaveDskF page corroborates the magic bytes.
 *
 * MF-356 rewrite: the prior 1.0.0 reader was fabricated against a wrong spec
 * (magic 0x5A4B — matches NO real file; invented geometry offsets; compression
 * mislabelled "LZSS"). See docs/KNOWN_ISSUES.md (SaveDskF fabrication finding).
 *
 * @version 2.0.0
 * @date 2026-07-06
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

/* Signature is a BIG-ENDIAN u16 at offset 0 (bytes AA 58 / AA 59 / AA 5A). */
#define UFT_SAVEDSKF_MAGIC_OLD      0xAA58  /* old fmt, uncompressed, data @0x200 */
#define UFT_SAVEDSKF_MAGIC_NEW      0xAA59  /* new fmt, uncompressed, data @hdr_size */
#define UFT_SAVEDSKF_MAGIC_LZW      0xAA5A  /* new fmt, IBM-LZW compressed */
#define UFT_SAVEDSKF_HEADER_SIZE    40      /* min bytes to read all header fields */
#define UFT_SAVEDSKF_OLD_DATA_OFF   0x200   /* old-fmt fixed sector-data offset */

/* On-disk header field offsets (magic is big-endian; the rest little-endian). */
#define UFT_SAVEDSKF_OFF_SECSIZE    4       /* u16 bytes per sector */
#define UFT_SAVEDSKF_OFF_CHECKSUM   20      /* u32 reported checksum */
#define UFT_SAVEDSKF_OFF_CYLINDERS  24      /* u16 */
#define UFT_SAVEDSKF_OFF_HEADS      26      /* u16 */
#define UFT_SAVEDSKF_OFF_SPT        28      /* u16 sectors per track */
#define UFT_SAVEDSKF_OFF_NUMSECS    34      /* u16 sectors present in image */
#define UFT_SAVEDSKF_OFF_COMMENT    36      /* u16 comment offset */
#define UFT_SAVEDSKF_OFF_HDRSIZE    38      /* u16 header size (0 => 512) */

/* Compression methods (runtime classification). Real SaveDskF has no RLE. */
#define UFT_SAVEDSKF_COMP_NONE      0       /* Uncompressed (old or new fmt) */
#define UFT_SAVEDSKF_COMP_LZW       1       /* IBM LZW (dskdcmps "ibmlzw") */

/* Maximum geometry values */
#define UFT_SAVEDSKF_MAX_CYLINDERS  200
#define UFT_SAVEDSKF_MAX_HEADS      2
#define UFT_SAVEDSKF_MAX_SECTORS    200
#define UFT_SAVEDSKF_MAX_SEC_SIZE   8192

/*===========================================================================
 * STRUCTURES
 *===========================================================================*/

#pragma pack(push, 1)

/**
 * @brief SaveDskF file header (23 bytes)
 */
typedef struct {
    uint16_t magic;                 /* BE sig: 0xAA58 old / 0xAA59 new / 0xAA5A LZW */
    uint16_t sector_size;           /* Bytes per sector (usually 512) */
    uint16_t sectors_per_track;     /* Sectors per track */
    uint16_t heads;                 /* Number of heads/sides */
    uint16_t cylinders;             /* Number of cylinders */
    uint8_t  compression;           /* UFT_SAVEDSKF_COMP_NONE / _LZW */
    uint16_t num_sectors_in_image;  /* Sectors physically stored (used sectors) */
    uint16_t header_size;           /* Offset where sector data begins */
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
