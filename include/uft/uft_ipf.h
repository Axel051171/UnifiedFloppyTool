/*
 * uft_ipf.h
 * 
 * UnifiedFloppyTool - IPF (Interchangeable Preservation Format) Parser
 * 
 * SPS/CAPS format for copy-protected disk preservation
 * Supports weak bits, timing data, multiple revolutions
 * 
 * References:
 * - IPF documentation by Jean Louis-Guerin v1.6
 * - MAME IPF parser (BSD licensed)
 * - FluxFox IPF implementation (MIT licensed)
 * 
 * Copyright (c) 2025 UFT Project
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_IPF_H
#define UFT_IPF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * IPF Constants
 *============================================================================*/

#define IPF_MAGIC               "CAPS"
#define IPF_MAGIC_LEN           4

#define IPF_MAX_TRACKS          168     /* 84 tracks * 2 sides */
#define IPF_MAX_SECTORS         32
#define IPF_MAX_REVOLUTIONS     5
#define IPF_MAX_BLOCK_COUNT     256

/* Chunk types */
#define IPF_CHUNK_CAPS          0x53504143  /* 'CAPS' */
#define IPF_CHUNK_INFO          0x4F464E49  /* 'INFO' */
#define IPF_CHUNK_IMGE          0x45474D49  /* 'IMGE' */
#define IPF_CHUNK_DATA          0x41544144  /* 'DATA' */

/*============================================================================
 * IPF Enumerations
 *============================================================================*/

typedef enum ipf_media_type {
    IPF_MEDIA_UNKNOWN = 0,
    IPF_MEDIA_FLOPPY = 1
} ipf_media_type_t;

typedef enum ipf_encoder_type {
    IPF_ENCODER_UNKNOWN = 0,
    IPF_ENCODER_V1 = 1,         /* CAPS encoder */
    IPF_ENCODER_V2 = 2          /* SPS encoder */
} ipf_encoder_type_t;

typedef enum ipf_platform {
    IPF_PLAT_NONE = 0,
    IPF_PLAT_AMIGA = 1,
    IPF_PLAT_ATARI_ST = 2,
    IPF_PLAT_PC = 3,
    IPF_PLAT_AMSTRAD_CPC = 4,
    IPF_PLAT_SPECTRUM = 5,
    IPF_PLAT_SAM_COUPE = 6,
    IPF_PLAT_ARCHIMEDES = 7,
    IPF_PLAT_C64 = 8,
    IPF_PLAT_ATARI_8BIT = 9
} ipf_platform_t;

typedef enum ipf_density {
    IPF_DENSITY_UNKNOWN = 0,
    IPF_DENSITY_NOISE = 1,
    IPF_DENSITY_AUTO = 2,
    IPF_DENSITY_HD = 3          /* High density */
} ipf_density_t;

typedef enum ipf_signal_type {
    IPF_SIGNAL_NONE = 0,
    IPF_SIGNAL_2US = 1          /* 2µs cell time */
} ipf_signal_type_t;

/* Data stream element types */
typedef enum ipf_data_type {
    IPF_DATA_END = 0,
    IPF_DATA_SYNC = 1,
    IPF_DATA_DATA = 2,
    IPF_DATA_GAP = 3,
    IPF_DATA_RAW = 4,
    IPF_DATA_FUZZY = 5          /* Weak bits */
} ipf_data_type_t;

/* Gap stream element types */
typedef enum ipf_gap_type {
    IPF_GAP_END = 0,
    IPF_GAP_LENGTH = 1,         /* Repeat count */
    IPF_GAP_SAMPLE = 2          /* Bit pattern */
} ipf_gap_type_t;

/* Block flags */
typedef enum ipf_block_flags {
    IPF_BLOCK_FWD_GAP = (1 << 0),   /* Forward gap */
    IPF_BLOCK_BWD_GAP = (1 << 1),   /* Backward gap */
    IPF_BLOCK_DATA_IN_BIT = (1 << 2) /* Data is in bits, not bytes */
} ipf_block_flags_t;

/*============================================================================
 * IPF Structures - File Format
 *============================================================================*/

/* Chunk header (8 bytes) */
typedef struct __attribute__((packed)) ipf_chunk_header {
    uint32_t type;              /* Chunk type ID */
    uint32_t length;            /* Chunk length (excluding header) */
} ipf_chunk_header_t;

/* INFO record (96 bytes payload) */
typedef struct __attribute__((packed)) ipf_info_record {
    uint32_t media_type;        /* ipf_media_type_t */
    uint32_t encoder_type;      /* ipf_encoder_type_t */
    uint32_t encoder_rev;       /* Encoder revision */
    uint32_t file_key;          /* File identifier */
    uint32_t file_rev;          /* File revision */
    uint32_t origin;            /* CRC of original */
    uint32_t min_track;         /* Minimum track number */
    uint32_t max_track;         /* Maximum track number */
    uint32_t min_side;          /* Minimum side (0 or 1) */
    uint32_t max_side;          /* Maximum side (0 or 1) */
    uint32_t creation_date;     /* Creation date (packed) */
    uint32_t creation_time;     /* Creation time (packed) */
    uint32_t platforms[4];      /* Target platforms */
    uint32_t disk_number;       /* Disk number in set */
    uint32_t creator_id;        /* Creator tool ID */
    uint32_t reserved[3];       /* Reserved */
} ipf_info_record_t;

/* IMAGE record (80 bytes payload) */
typedef struct __attribute__((packed)) ipf_image_record {
    uint32_t track;             /* Physical track number */
    uint32_t side;              /* Physical side (0 or 1) */
    uint32_t density;           /* ipf_density_t */
    uint32_t signal_type;       /* ipf_signal_type_t */
    uint32_t track_bytes;       /* Track size in bytes */
    uint32_t start_byte;        /* Start byte position */
    uint32_t start_bit;         /* Start bit position */
    uint32_t data_bits;         /* Data bits count */
    uint32_t gap_bits;          /* Gap bits count */
    uint32_t track_bits;        /* Total track bits */
    uint32_t block_count;       /* Number of blocks */
    uint32_t encoder_process;   /* Encoder process used */
    uint32_t track_flags;       /* Track flags */
    uint32_t data_key;          /* Key to DATA record */
    uint32_t reserved[6];       /* Reserved */
} ipf_image_record_t;

/* DATA record header (28 bytes payload before EDB) */
typedef struct __attribute__((packed)) ipf_data_record {
    uint32_t length;            /* Extra Data Block length */
    uint32_t bit_size;          /* Bits in data stream */
    uint32_t crc;               /* CRC of EDB */
    uint32_t data_key;          /* Key matching IMAGE record */
    uint32_t reserved[3];       /* Reserved */
} ipf_data_record_t;

/* Block descriptor (V1: 28 bytes, V2: 32 bytes) */
typedef struct ipf_block_desc {
    uint32_t data_bits;         /* Data bits in block */
    uint32_t gap_bits;          /* Gap bits in block */
    
    /* V1 fields */
    uint32_t data_bytes;        /* (V1) Data bytes */
    uint32_t gap_bytes;         /* (V1) Gap bytes */
    
    /* V2 fields */
    uint32_t gap_offset;        /* (V2) Gap stream offset */
    uint32_t cell_type;         /* (V2) Cell type */
    
    /* Common */
    uint32_t encoder_type;      /* Block encoder type */
    uint32_t block_flags;       /* ipf_block_flags_t */
    uint32_t gap_default;       /* Default gap value */
    uint32_t data_offset;       /* Data stream offset */
} ipf_block_desc_t;

/*============================================================================
 * IPF Structures - Decoded Data
 *============================================================================*/

/* Decoded sector */
typedef struct ipf_sector {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;         /* N value (size = 128 << N) */
    
    uint16_t actual_size;       /* Actual data size */
    uint8_t *data;              /* Sector data */
    
    bool     deleted;           /* Deleted data mark */
    bool     crc_error;         /* CRC error flag */
    bool     weak_bits;         /* Contains weak bits */
    
    uint32_t weak_offset;       /* Offset of weak bits */
    uint32_t weak_length;       /* Length of weak region */
} ipf_sector_t;

/* Decoded track */
typedef struct ipf_track {
    uint8_t  track;             /* Track number */
    uint8_t  side;              /* Side (0 or 1) */
    
    uint32_t bit_length;        /* Track length in bits */
    uint32_t byte_length;       /* Track length in bytes */
    
    /* Raw bitstream data */
    uint8_t *bitstream;         /* MFM/FM encoded data */
    size_t   bitstream_len;
    
    /* Weak bit mask (same length as bitstream) */
    uint8_t *weak_mask;
    
    /* Timing data (optional) */
    uint16_t *timing;           /* Per-bitcell timing */
    size_t    timing_len;
    
    /* Decoded sectors */
    ipf_sector_t sectors[IPF_MAX_SECTORS];
    size_t       sector_count;
    
    /* Metadata */
    ipf_density_t     density;
    ipf_signal_type_t signal_type;
    uint32_t          flags;
} ipf_track_t;

/* Complete IPF image */
typedef struct ipf_image {
    /* File info */
    char          filename[256];
    ipf_media_type_t   media_type;
    ipf_encoder_type_t encoder_type;
    uint32_t      encoder_rev;
    
    /* Geometry */
    uint32_t min_track;
    uint32_t max_track;
    uint32_t min_side;
    uint32_t max_side;
    
    /* Platforms */
    ipf_platform_t platforms[4];
    size_t         platform_count;
    
    /* Metadata */
    uint32_t creation_date;     /* YYYYMMDD */
    uint32_t creation_time;     /* HHMMSS */
    uint32_t disk_number;
    uint32_t creator_id;
    
    /* Tracks */
    ipf_track_t *tracks[IPF_MAX_TRACKS];
    size_t       track_count;
    
    /* Statistics */
    size_t total_sectors;
    size_t weak_sectors;
    size_t error_sectors;
} ipf_image_t;

/*============================================================================
 * IPF Error Codes
 *============================================================================*/

typedef enum ipf_error {
    IPF_OK = 0,
    IPF_ERR_IO = -1,
    IPF_ERR_FORMAT = -2,
    IPF_ERR_CRC = -3,
    IPF_ERR_MEMORY = -4,
    IPF_ERR_UNSUPPORTED = -5,
    IPF_ERR_CORRUPT = -6,
    IPF_ERR_INVALID_TRACK = -7,
    IPF_ERR_INVALID_SECTOR = -8
} ipf_error_t;

/*============================================================================
 * IPF API Functions
 *============================================================================*/

/**
 * Check if file is IPF format
 * @param path File path
 * @return true if IPF format
 */
bool ipf_is_ipf_file(const char *path);

/**
 * Check if buffer contains IPF data
 * @param data Buffer
 * @param len Buffer length
 * @return true if IPF format
 */
bool ipf_is_ipf_buffer(const uint8_t *data, size_t len);

/**
 * Create new empty IPF image
 * @return New image or NULL on error
 */
ipf_image_t* ipf_image_create(void);

/**
 * Free IPF image and all associated memory
 * @param img Image to free
 */
void ipf_image_free(ipf_image_t *img);

/**
 * Load IPF file
 * @param path File path
 * @param img Output image (must be pre-allocated or NULL for auto-allocation)
 * @return IPF_OK on success, error code otherwise
 */
ipf_error_t ipf_load_file(const char *path, ipf_image_t **img);

/**
 * Load IPF from buffer
 * @param data Buffer
 * @param len Buffer length
 * @param img Output image
 * @return IPF_OK on success
 */
ipf_error_t ipf_load_buffer(const uint8_t *data, size_t len, ipf_image_t **img);

/**
 * Get track from image
 * @param img Image
 * @param track Track number
 * @param side Side (0 or 1)
 * @return Track or NULL if not found
 */
const ipf_track_t* ipf_get_track(const ipf_image_t *img, uint32_t track, uint32_t side);

/**
 * Read sector data
 * @param img Image
 * @param track Track number
 * @param side Side
 * @param sector Sector number
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes read or negative error code
 */
int ipf_read_sector(const ipf_image_t *img, uint32_t track, uint32_t side,
                    uint32_t sector, uint8_t *buffer, size_t buffer_size);

/**
 * Extract track bitstream
 * @param img Image
 * @param track Track number
 * @param side Side
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param include_weak Include weak bit data
 * @return Bits written or negative error code
 */
int ipf_extract_bitstream(const ipf_image_t *img, uint32_t track, uint32_t side,
                          uint8_t *buffer, size_t buffer_size, bool include_weak);

/**
 * Get weak bit mask for track
 * @param img Image
 * @param track Track number
 * @param side Side
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or negative error code
 */
int ipf_get_weak_mask(const ipf_image_t *img, uint32_t track, uint32_t side,
                      uint8_t *buffer, size_t buffer_size);

/**
 * Convert IPF to raw sector image (ADF/ST/IMG)
 * @param img IPF image
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param bytes_per_sector Sector size (usually 512)
 * @return Bytes written or negative error code
 */
int ipf_to_sector_image(const ipf_image_t *img, uint8_t *buffer, size_t buffer_size,
                        uint16_t bytes_per_sector);

/**
 * Get error string
 * @param err Error code
 * @return Error description
 */
const char* ipf_error_string(ipf_error_t err);

/**
 * Get platform name
 * @param platform Platform ID
 * @return Platform name
 */
const char* ipf_platform_name(ipf_platform_t platform);

/**
 * Print image info to stream
 * @param img Image
 * @param stream Output stream (stdout if NULL)
 */
void ipf_print_info(const ipf_image_t *img, FILE *stream);

/*============================================================================
 * IPF CRC Functions
 *============================================================================*/

/**
 * Calculate IPF CRC32
 * @param data Data buffer
 * @param len Data length
 * @return CRC32 value
 */
uint32_t ipf_crc32(const uint8_t *data, size_t len);

/**
 * Verify chunk CRC
 * @param data Chunk data (excluding header)
 * @param len Chunk length
 * @param expected Expected CRC
 * @return true if CRC matches
 */
bool ipf_verify_crc(const uint8_t *data, size_t len, uint32_t expected);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IPF_H */
