/**
 * @file uft_cqm.h
 * @brief CopyQM (CQM) format support with LZSS decompression
 * 
 * P1-003: CQM format was read-only due to missing LZSS
 * 
 * CQM Format:
 * - Header: 133 bytes
 * - Comment block (optional)
 * - Compressed track data (LZSS variant)
 */

#ifndef UFT_CQM_H
#define UFT_CQM_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CQM Header constants */
#define CQM_SIGNATURE       "CQ\x14"
#define CQM_HEADER_SIZE     133
#define CQM_MAX_COMMENT     0x8000

/* CQM Density types */
#define CQM_DENSITY_DD      0    /* Double density */
#define CQM_DENSITY_HD      1    /* High density */
#define CQM_DENSITY_ED      2    /* Extra density */

/* CQM Drive types */
#define CQM_DRIVE_525       0    /* 5.25" */
#define CQM_DRIVE_35        1    /* 3.5" */
#define CQM_DRIVE_8         2    /* 8" */

/**
 * @brief CQM file header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  signature[3];       /* "CQ\x14" */
    uint8_t  version;            /* Version (1 or 2) */
    uint16_t sector_size;        /* Bytes per sector */
    uint16_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  comment_length_low;
    uint8_t  comment_length_high;
    uint8_t  sectors_per_track2;
    uint8_t  density;            /* 0=DD, 1=HD, 2=ED */
    uint8_t  used_tracks;        /* May be 0 */
    uint8_t  total_tracks;
    uint8_t  interleave;
    uint8_t  skew;
    uint8_t  drive_type;         /* 0=5.25", 1=3.5" */
    uint8_t  unused_0;
    uint8_t  heads2;
    uint8_t  dos_checksum;       /* Boot sector checksum */
    uint8_t  unused_1[107];      /* Padding to 133 bytes */
} cqm_header_t;
#pragma pack(pop)

/**
 * @brief CQM decompression context
 */
typedef struct {
    const uint8_t *input;
    size_t input_size;
    size_t input_pos;
    
    uint8_t *output;
    size_t output_size;
    size_t output_pos;
    
    /* LZSS ring buffer */
    uint8_t ring[4096];
    size_t ring_pos;
    
    /* Statistics */
    size_t bytes_read;
    size_t bytes_written;
    
} cqm_decompress_ctx_t;

/**
 * @brief CQM read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    /* Image info */
    uint16_t tracks;
    uint8_t heads;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    
    /* Comment */
    char *comment;
    size_t comment_len;
    
    /* Statistics */
    size_t compressed_size;
    size_t uncompressed_size;
    double compression_ratio;
    
} cqm_read_result_t;

/**
 * @brief CQM write options
 */
typedef struct {
    bool compress;              /* Use LZSS compression */
    uint8_t compression_level;  /* 1-9 */
    const char *comment;        /* Optional comment */
    bool include_bpb;           /* Include DOS BPB */
} cqm_write_options_t;

/* ============================================================================
 * LZSS Decompression
 * ============================================================================ */

/**
 * @brief Initialize LZSS decompression context
 */
void cqm_decompress_init(cqm_decompress_ctx_t *ctx,
                         const uint8_t *input, size_t input_size,
                         uint8_t *output, size_t output_size);

/**
 * @brief Decompress CQM data
 * @return Number of bytes decompressed, or -1 on error
 */
int cqm_decompress(cqm_decompress_ctx_t *ctx);

/**
 * @brief Decompress entire CQM image data
 */
int cqm_decompress_full(const uint8_t *compressed, size_t compressed_size,
                        uint8_t *output, size_t output_size,
                        size_t *out_decompressed_size);

/* ============================================================================
 * LZSS Compression
 * ============================================================================ */

/**
 * @brief Compress data for CQM format
 * @return Compressed size, or -1 on error
 */
int cqm_compress(const uint8_t *input, size_t input_size,
                 uint8_t *output, size_t output_capacity,
                 int level);

/* ============================================================================
 * CQM File I/O
 * ============================================================================ */

/**
 * @brief Read CQM file
 */
uft_error_t uft_cqm_read(const char *path,
                         uft_disk_image_t **out_disk,
                         cqm_read_result_t *result);

/**
 * @brief Read CQM from memory
 */
uft_error_t uft_cqm_read_mem(const uint8_t *data, size_t size,
                             uft_disk_image_t **out_disk,
                             cqm_read_result_t *result);

/**
 * @brief Write CQM file
 */
uft_error_t uft_cqm_write(const uft_disk_image_t *disk,
                          const char *path,
                          const cqm_write_options_t *opts);

/**
 * @brief Write CQM to memory
 */
uft_error_t uft_cqm_write_mem(const uft_disk_image_t *disk,
                              uint8_t *buffer, size_t buffer_size,
                              size_t *out_size,
                              const cqm_write_options_t *opts);

/**
 * @brief Validate CQM header
 */
bool uft_cqm_validate_header(const cqm_header_t *header);

/**
 * @brief Initialize write options with defaults
 */
void uft_cqm_write_options_init(cqm_write_options_t *opts);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CQM_H */
