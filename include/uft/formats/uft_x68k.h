/**
 * @file uft_x68k.h
 * @brief Sharp X68000 disk image format support
 * @version 3.9.0
 * 
 * Supports X68000 disk formats:
 * - XDF: Raw sector dump with optional header
 * - DIM: D88-compatible format with 256-byte header
 * - Human68k FAT filesystem support
 * 
 * X68000 Standard Floppy Specs:
 * - 3.5" 2HD: 77 tracks, 2 sides, 8 sectors/track, 1024 bytes/sector = 1.2MB
 * - 3.5" 2DD: 80 tracks, 2 sides, 9 sectors/track, 512 bytes/sector = 720KB
 * 
 * Reference: x68000-floppy-tools by leaded-solder
 */

#ifndef UFT_X68K_H
#define UFT_X68K_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* X68000 standard geometry constants */
#define X68K_2HD_CYLS           77
#define X68K_2HD_HEADS          2
#define X68K_2HD_SECTORS        8
#define X68K_2HD_SECSIZE        1024
#define X68K_2HD_SIZE           (77 * 2 * 8 * 1024)  /* 1,261,568 bytes */

#define X68K_2DD_CYLS           80
#define X68K_2DD_HEADS          2
#define X68K_2DD_SECTORS        9
#define X68K_2DD_SECSIZE        512
#define X68K_2DD_SIZE           (80 * 2 * 9 * 512)   /* 737,280 bytes */

/* DIM header constants */
#define DIM_HEADER_SIZE         256
#define DIM_SIGNATURE           0x00  /* No specific signature, detect by size/structure */

/* Media types */
typedef enum {
    X68K_MEDIA_2HD = 0,          /* 1.2MB 2HD */
    X68K_MEDIA_2DD = 1,          /* 720KB 2DD */
    X68K_MEDIA_2HQ = 2,          /* 1.44MB (rare on X68000) */
} x68k_media_type_t;

/**
 * @brief DIM file header (256 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  media_type;         /* Media type byte */
    uint8_t  reserved1[9];       /* Reserved */
    uint8_t  track_map[154];     /* Track usage bitmap */
    uint8_t  reserved2[92];      /* Padding to 256 bytes */
} dim_header_t;
#pragma pack(pop)

/**
 * @brief X68000 disk read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    /* Image info */
    x68k_media_type_t media_type;
    uint8_t cylinders;
    uint8_t heads;
    uint8_t sectors;
    uint16_t sector_size;
    
    /* Format detection */
    bool is_dim;                 /* DIM format (with header) */
    bool is_xdf;                 /* Raw XDF format */
    bool has_human68k;           /* Human68k filesystem detected */
    
    /* Statistics */
    size_t image_size;
    uint32_t used_tracks;
    
} x68k_read_result_t;

/**
 * @brief X68000 write options
 */
typedef struct {
    bool write_dim_header;       /* Write DIM header */
    x68k_media_type_t media_type;
} x68k_write_options_t;

/* ============================================================================
 * X68000 Format Detection
 * ============================================================================ */

/**
 * @brief Detect X68000 media type from image size
 */
x68k_media_type_t x68k_detect_media_type(size_t image_size);

/**
 * @brief Probe if data is X68000 XDF format
 */
bool uft_x68k_xdf_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Probe if data is X68000 DIM format
 */
bool uft_x68k_dim_probe(const uint8_t *data, size_t size, int *confidence);

/* ============================================================================
 * X68000 File I/O
 * ============================================================================ */

/**
 * @brief Read X68000 XDF file
 */
uft_error_t uft_x68k_read(const char *path,
                          uft_disk_image_t **out_disk,
                          x68k_read_result_t *result);

/**
 * @brief Read X68000 from memory
 */
uft_error_t uft_x68k_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              x68k_read_result_t *result);

/**
 * @brief Write X68000 XDF file
 */
uft_error_t uft_x68k_write(const uft_disk_image_t *disk,
                           const char *path,
                           const x68k_write_options_t *opts);

/**
 * @brief Convert DIM to raw XDF (strip header)
 */
uft_error_t uft_x68k_dim_to_xdf(const uint8_t *dim_data, size_t dim_size,
                                uint8_t **out_xdf, size_t *out_size);

/**
 * @brief Convert XDF to DIM (add header)
 */
uft_error_t uft_x68k_xdf_to_dim(const uint8_t *xdf_data, size_t xdf_size,
                                x68k_media_type_t media_type,
                                uint8_t **out_dim, size_t *out_size);

/**
 * @brief Initialize write options with defaults
 */
void uft_x68k_write_options_init(x68k_write_options_t *opts);

/* ============================================================================
 * Human68k Filesystem Support
 * ============================================================================ */

/**
 * @brief Check if disk has Human68k filesystem
 */
bool uft_x68k_has_human68k(const uft_disk_image_t *disk);

/**
 * @brief Get Human68k volume label
 */
const char* uft_x68k_get_volume_label(const uft_disk_image_t *disk,
                                       char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_X68K_H */
