/**
 * @file uft_ldbs.h
 * @brief LDBS (LibDsk Block Store) format support
 * @version 3.9.0
 * 
 * LDBS is a block-based disk image format designed by John Elliott
 * for LibDsk. It stores disk data in variable-sized blocks with
 * a directory structure, supporting compression and metadata.
 * 
 * Features:
 * - Block-based storage with deduplication potential
 * - Track directory for efficient access
 * - Support for sector-level metadata (deleted marks, CRC errors)
 * - Optional compression per block
 * 
 * Reference: http://www.seasip.info/Unix/LibDsk/ldbs.html
 */

#ifndef UFT_LDBS_H
#define UFT_LDBS_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LDBS Magic Numbers */
#define LDBS_FILE_MAGIC         0x4C425301  /* "LBS\01" */
#define LDBS_FILE_TYPE_DSK      0x44534B02  /* "DSK\02" */
#define LDBS_BLOCK_MAGIC        0x4C444201  /* "LDB\01" */
#define LDBS_TRACK_BLOCK        0x44495201  /* "DIR\01" (track directory) */
#define LDBS_SECTOR_BLOCK       0x53454301  /* "SEC\01" (sector data) */
#define LDBS_GEOM_BLOCK         0x47454F01  /* "GEO\01" (geometry) */
#define LDBS_INFO_BLOCK         0x494E4601  /* "INF\01" (info/comment) */

/* LDBS Header Size */
#define LDBS_HEADER_SIZE        20
#define LDBS_BLOCK_HEADER_SIZE  12

/* Maximum values */
#define LDBS_MAX_BLOCKS         65536
#define LDBS_MAX_COMMENT        256

/* Sector flags */
#define LDBS_SECT_DELETED       0x01    /* Deleted data mark */
#define LDBS_SECT_CRC_ERROR     0x02    /* CRC error */
#define LDBS_SECT_MISSING       0x04    /* Sector not found */
#define LDBS_SECT_WEAK          0x08    /* Weak bits present */

/**
 * @brief LDBS file header (20 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;             /* LDBS_FILE_MAGIC */
    uint32_t type;              /* LDBS_FILE_TYPE_DSK */
    uint32_t dir_offset;        /* Offset to track directory */
    uint32_t total_blocks;      /* Total number of blocks */
    uint32_t reserved;          /* Reserved */
} ldbs_file_header_t;

/**
 * @brief LDBS block header (12 bytes)
 */
typedef struct {
    uint32_t magic;             /* LDBS_BLOCK_MAGIC */
    uint32_t type;              /* Block type */
    uint32_t size;              /* Data size (excluding header) */
} ldbs_block_header_t;

/**
 * @brief LDBS geometry block
 */
typedef struct {
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
    uint8_t  first_sector;
    uint8_t  encoding;          /* 0=FM, 1=MFM */
    uint16_t data_rate;         /* kbps */
    uint8_t  reserved[6];
} ldbs_geometry_t;

/**
 * @brief LDBS track directory entry
 */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint32_t block_offset;      /* Offset to track block */
} ldbs_track_entry_t;

/**
 * @brief LDBS sector entry (in track block)
 */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;
    uint8_t  flags;             /* LDBS_SECT_* flags */
    uint8_t  filler;            /* Fill byte if missing */
    uint16_t data_size;         /* Actual data size */
    uint32_t data_offset;       /* Offset to sector data block */
} ldbs_sector_entry_t;
#pragma pack(pop)

/**
 * @brief LDBS write options
 */
typedef struct {
    bool     deduplicate;       /* Enable block deduplication */
    bool     compress;          /* Enable compression (future) */
    char     comment[LDBS_MAX_COMMENT];  /* Optional comment */
} ldbs_write_options_t;

/**
 * @brief LDBS read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    uint32_t total_blocks;
    uint32_t unique_blocks;     /* After deduplication */
    
    ldbs_geometry_t geometry;
    char comment[LDBS_MAX_COMMENT];
    
    size_t image_size;
    
} ldbs_read_result_t;

/* ============================================================================
 * LDBS Functions
 * ============================================================================ */

/**
 * @brief Initialize write options
 */
void uft_ldbs_write_options_init(ldbs_write_options_t *opts);

/**
 * @brief Read LDBS file
 */
uft_error_t uft_ldbs_read(const char *path,
                          uft_disk_image_t **out_disk,
                          ldbs_read_result_t *result);

/**
 * @brief Read LDBS from memory
 */
uft_error_t uft_ldbs_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              ldbs_read_result_t *result);

/**
 * @brief Write LDBS file
 */
uft_error_t uft_ldbs_write(const uft_disk_image_t *disk,
                           const char *path,
                           const ldbs_write_options_t *opts);

/**
 * @brief Probe if data is LDBS format
 */
bool uft_ldbs_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Validate LDBS file header
 */
bool uft_ldbs_validate_header(const ldbs_file_header_t *header);

#ifdef __cplusplus
}
#endif

#endif /* UFT_LDBS_H */
