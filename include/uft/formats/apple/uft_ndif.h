/**
 * @file uft_ndif.h
 * @brief NDIF (New Disk Image Format / DiskCopy 6.x) parser
 *
 * Apple Macintosh disk image format, successor to DiskCopy 4.2.
 * Supports both data-fork-only (flat) images and resource-fork images
 * with BLKX block descriptors and optional ADC compression.
 *
 * Floppy sizes: 400KB (Mac GCR), 800KB (Mac GCR), 1440KB (Mac MFM)
 *
 * @author UFT Project
 * @date 2026-04-10
 */

#ifndef UFT_NDIF_H
#define UFT_NDIF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Flattened NDIF magic "bcem" at offset 4 (resource fork signature) */
#define NDIF_RSRC_MAGIC         0x6263656Du  /* "bcem" */

/** Maximum number of BLKX block entries */
#define NDIF_MAX_BLOCKS         256

/** Sector size */
#define NDIF_SECTOR_SIZE        512

/** Standard Mac floppy sizes in bytes */
#define NDIF_SIZE_400K          409600u
#define NDIF_SIZE_800K          819200u
#define NDIF_SIZE_720K          737280u
#define NDIF_SIZE_1440K         1474560u

/* ============================================================================
 * Error Codes
 * ============================================================================ */

typedef enum {
    UFT_NDIF_OK              =  0,
    UFT_NDIF_ERR_NULL        = -1,
    UFT_NDIF_ERR_OPEN        = -2,
    UFT_NDIF_ERR_READ        = -3,
    UFT_NDIF_ERR_TRUNCATED   = -4,
    UFT_NDIF_ERR_INVALID     = -5,
    UFT_NDIF_ERR_MEMORY      = -6,
    UFT_NDIF_ERR_UNSUPPORTED = -7,
    UFT_NDIF_ERR_DECOMPRESS  = -8,
    UFT_NDIF_ERR_SIZE        = -9,
} uft_ndif_error_t;

/* ============================================================================
 * Format Enumerations
 * ============================================================================ */

/** NDIF image sub-type */
typedef enum {
    NDIF_SUBTYPE_UNKNOWN    = 0,
    NDIF_SUBTYPE_FLAT       = 1,    /**< Data-fork-only (raw sectors) */
    NDIF_SUBTYPE_RSRC       = 2,    /**< Resource-fork based with BLKX */
} ndif_subtype_t;

/** NDIF disk type (by size) */
typedef enum {
    NDIF_DISK_UNKNOWN       = 0,
    NDIF_DISK_400K          = 1,    /**< Mac 400K GCR single-sided */
    NDIF_DISK_800K          = 2,    /**< Mac 800K GCR double-sided */
    NDIF_DISK_720K          = 3,    /**< PC 720K MFM */
    NDIF_DISK_1440K         = 4,    /**< Mac/PC 1.44MB MFM HD */
} ndif_disk_type_t;

/** BLKX block types */
typedef enum {
    NDIF_BLK_ZERO           = 0x00000000u,  /**< Zero-filled */
    NDIF_BLK_RAW            = 0x00000001u,  /**< Uncompressed raw */
    NDIF_BLK_ADC            = 0x80000004u,  /**< ADC compressed */
    NDIF_BLK_ZLIB           = 0x80000005u,  /**< zlib compressed */
    NDIF_BLK_COMMENT        = 0x7FFFFFFEu,  /**< Comment (ignore) */
    NDIF_BLK_TERM           = 0xFFFFFFFFu,  /**< Terminator */
} ndif_blk_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

#pragma pack(push, 1)

/** BLKX block descriptor (40 bytes) */
typedef struct {
    uint32_t    block_type;         /**< Block type (ndif_blk_type_t) */
    uint32_t    reserved;           /**< Reserved */
    uint64_t    sector_offset;      /**< Sector offset on logical disk */
    uint64_t    sector_count;       /**< Number of sectors in this block */
    uint64_t    compressed_offset;  /**< Offset in data fork */
    uint64_t    compressed_length;  /**< Length in data fork */
} ndif_blkx_entry_t;

#pragma pack(pop)

/** NDIF parsed context */
typedef struct {
    /* Image info */
    ndif_subtype_t      subtype;
    ndif_disk_type_t    disk_type;
    bool                is_valid;

    /* Disk geometry */
    uint32_t            data_size;      /**< Total uncompressed disk data */
    uint32_t            sector_count;
    uint32_t            sector_size;

    /* BLKX entries (for resource-fork images) */
    ndif_blkx_entry_t   blocks[NDIF_MAX_BLOCKS];
    int                  block_count;

    /* Format details */
    char                description[64];
} uft_ndif_context_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Detect if data is an NDIF image
 * @param data      File data
 * @param size      Size of file data
 * @param filename  Filename (for extension heuristic), can be NULL
 * @return true if this looks like an NDIF image
 */
bool uft_ndif_detect(const uint8_t *data, size_t size, const char *filename);

/**
 * @brief Parse NDIF image
 * @param data  File data (complete file in memory)
 * @param size  Size of data
 * @param ctx   Output: parsed context (caller-allocated)
 * @return UFT_NDIF_OK on success, negative error code on failure
 */
uft_ndif_error_t uft_ndif_parse(const uint8_t *data, size_t size,
                                 uft_ndif_context_t *ctx);

/**
 * @brief Extract raw disk data from NDIF image
 * @param data          Source file data
 * @param size          Size of source data
 * @param ctx           Previously parsed context
 * @param output        Output buffer for raw sector data
 * @param output_size   Size of output buffer
 * @return Number of bytes written, or negative error code
 */
int uft_ndif_extract(const uint8_t *data, size_t size,
                      const uft_ndif_context_t *ctx,
                      uint8_t *output, size_t output_size);

/**
 * @brief Get disk type description string
 * @param disk_type NDIF disk type
 * @return Static description string
 */
const char *uft_ndif_disk_type_string(ndif_disk_type_t disk_type);

/**
 * @brief Get error string for NDIF error code
 * @param err Error code
 * @return Static error description
 */
const char *uft_ndif_strerror(uft_ndif_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* UFT_NDIF_H */
