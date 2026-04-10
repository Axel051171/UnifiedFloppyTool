/**
 * @file uft_aaru.h
 * @brief Aaru (DICF - Disc Image Chef Format) read-only support
 *
 * Aaru is a modern disk preservation format created by Natalia Portillo.
 * It uses Protocol Buffers internally for metadata, but we parse the
 * fixed binary fields for basic floppy geometry and sector data.
 *
 * Reference: https://github.com/aaru-dps/Aaru
 *
 * @version 1.0.0
 * @date 2026-04-10
 */

#ifndef UFT_AARU_H
#define UFT_AARU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * AARU FORMAT DEFINITIONS
 *===========================================================================*/

#define UFT_AARU_MAGIC          "AARUF"
#define UFT_AARU_MAGIC_LEN      5

/* Media types (subset relevant to floppy) */
#define UFT_AARU_MEDIA_UNKNOWN          0x0000
#define UFT_AARU_MEDIA_FLOPPY_525_DD    0x0010
#define UFT_AARU_MEDIA_FLOPPY_525_HD    0x0011
#define UFT_AARU_MEDIA_FLOPPY_35_DD     0x0020
#define UFT_AARU_MEDIA_FLOPPY_35_HD     0x0021
#define UFT_AARU_MEDIA_FLOPPY_35_ED     0x0022

/* Block types */
#define UFT_AARU_BLOCK_HEADER           0x00
#define UFT_AARU_BLOCK_MEDIA_INFO       0x01
#define UFT_AARU_BLOCK_TRACK_DESC       0x02
#define UFT_AARU_BLOCK_SECTOR_DATA      0x03
#define UFT_AARU_BLOCK_CHECKSUM         0x04
#define UFT_AARU_BLOCK_METADATA         0x05
#define UFT_AARU_BLOCK_INDEX            0x10

/* Compression types */
#define UFT_AARU_COMPRESS_NONE          0x00
#define UFT_AARU_COMPRESS_LZMA          0x01
#define UFT_AARU_COMPRESS_FLAC          0x02
#define UFT_AARU_COMPRESS_LZ4           0x03

/* Maximum values */
#define UFT_AARU_MAX_TRACKS             168
#define UFT_AARU_MAX_SECTORS            64
#define UFT_AARU_MAX_SECTOR_SIZE        8192
#define UFT_AARU_APP_NAME_LEN           64
#define UFT_AARU_APP_VER_LEN            32

/*===========================================================================
 * STRUCTURES
 *===========================================================================*/

#pragma pack(push, 1)

/**
 * @brief Aaru file header (fixed portion)
 */
typedef struct {
    char     magic[5];              /* "AARUF" */
    uint8_t  major_version;         /* Format major version */
    uint8_t  minor_version;         /* Format minor version */
    uint8_t  flags;                 /* File flags */
    uint16_t media_type;            /* Media type code */
    uint64_t creation_time;         /* Unix timestamp */
    uint64_t last_modified;         /* Unix timestamp */
} uft_aaru_file_header_t;

/**
 * @brief Aaru block header (precedes each data block)
 */
typedef struct {
    uint8_t  block_type;            /* Block type identifier */
    uint8_t  compression;           /* Compression method */
    uint32_t compressed_length;     /* Compressed data length */
    uint32_t uncompressed_length;   /* Original data length */
    uint32_t crc32;                 /* CRC-32 of uncompressed data */
} uft_aaru_block_header_t;

/**
 * @brief Aaru media info (fixed binary fields we parse)
 */
typedef struct {
    uint16_t media_type;            /* Media type code */
    uint16_t cylinders;             /* Number of cylinders */
    uint8_t  heads;                 /* Number of heads/sides */
    uint16_t sectors_per_track;     /* Sectors per track */
    uint16_t sector_size;           /* Bytes per sector */
    uint32_t total_sectors;         /* Total sector count */
    uint8_t  encoding;              /* Encoding type (MFM/FM/GCR) */
    uint32_t data_rate;             /* Data rate in kbps */
} uft_aaru_media_info_t;

#pragma pack(pop)

/*===========================================================================
 * CONTEXT
 *===========================================================================*/

/**
 * @brief Aaru image context
 */
typedef struct {
    uft_aaru_file_header_t file_header;
    uft_aaru_media_info_t  media_info;

    /* Application info (parsed from header block) */
    char     app_name[UFT_AARU_APP_NAME_LEN];
    char     app_version[UFT_AARU_APP_VER_LEN];

    /* Sector data */
    uint8_t *sector_data;           /* Flat sector data buffer */
    size_t   sector_data_size;

    /* Track descriptors */
    uint32_t track_count;

    /* Status */
    bool     is_open;
    bool     partial_parse;         /* True if protobuf sections skipped */
} uft_aaru_image_t;

/**
 * @brief Aaru read result
 */
typedef struct {
    bool        success;
    int         error_code;
    const char *error_detail;

    uint16_t    cylinders;
    uint8_t     heads;
    uint16_t    sectors_per_track;
    uint16_t    sector_size;
    uint32_t    total_sectors;

    uint8_t     format_major;
    uint8_t     format_minor;
    uint16_t    media_type;
    bool        has_full_data;      /* False if complex/compressed data skipped */
} uft_aaru_read_result_t;

/*===========================================================================
 * API FUNCTIONS
 *===========================================================================*/

/**
 * @brief Initialize Aaru image structure
 */
void uft_aaru_image_init(uft_aaru_image_t *image);

/**
 * @brief Free Aaru image resources
 */
void uft_aaru_image_free(uft_aaru_image_t *image);

/**
 * @brief Probe if data is Aaru/DICF format
 * @param data    File header bytes
 * @param size    Available bytes
 * @param confidence  Output confidence (0-100)
 * @return true if Aaru format detected
 */
bool uft_aaru_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Read Aaru file (read-only, minimal parse)
 * @param path    Path to .aaru/.dicf file
 * @param image   Output image structure
 * @param result  Optional read result details
 * @return 0 on success, negative on error
 */
int uft_aaru_read(const char *path, uft_aaru_image_t *image,
                  uft_aaru_read_result_t *result);

/**
 * @brief Read sector from parsed Aaru image
 * @param image   Parsed Aaru image
 * @param cyl     Cylinder number
 * @param head    Head number
 * @param sector  Sector number (1-based)
 * @param buffer  Output buffer
 * @param size    Buffer size
 * @return Bytes read, or negative on error
 */
int uft_aaru_read_sector(const uft_aaru_image_t *image,
                         uint16_t cyl, uint8_t head, uint16_t sector,
                         uint8_t *buffer, size_t size);

/**
 * @brief Get media type description string
 */
const char *uft_aaru_media_type_name(uint16_t media_type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AARU_H */
