/**
 * @file uft_chd.h
 * @brief MAME Compressed Hunks of Data (CHD) Read-Only Support
 *
 * CHD is MAME's container format for disk images. This implementation
 * provides read-only header/metadata parsing and uncompressed sector
 * access for CHD v3, v4, and v5.
 *
 * Compressed CHD files require libchdr for decompression and are
 * reported but not directly readable by this module.
 *
 * @author UFT Project
 * @date 2026-04-10
 */

#ifndef UFT_CHD_H
#define UFT_CHD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CHD_MAGIC           "MComprHD"
#define CHD_MAGIC_SIZE      8
#define CHD_V5_HEADER       124
#define CHD_V4_HEADER       108
#define CHD_V3_HEADER       120

/** Metadata tags (fourcc as uint32) */
#define CHD_META_GDDD       0x47444444  /* "GDDD" - hard disk geometry */
#define CHD_META_GDFL       0x4744464C  /* "GDFL" - floppy disk geometry (not standard) */

/** Compression identifiers */
#define CHD_COMP_NONE       0
#define CHD_COMP_ZLIB       1
#define CHD_COMP_ZLIB_PLUS  2
#define CHD_COMP_AV_CODEC   3

/** Maximum reasonable floppy size (10 MB) */
#define CHD_FLOPPY_MAX_SIZE (10 * 1024 * 1024)

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief CHD v5 header (124 bytes, all fields big-endian on disk)
 */
typedef struct {
    char     magic[8];        /* "MComprHD" */
    uint32_t header_size;     /* 124 for v5 */
    uint32_t version;         /* 5 */
    uint32_t compressors[4];  /* Compression codecs */
    uint64_t logical_bytes;   /* Total uncompressed size */
    uint64_t map_offset;      /* Offset to hunk map */
    uint64_t meta_offset;     /* Offset to metadata */
    uint32_t hunk_bytes;      /* Bytes per hunk */
    uint32_t unit_bytes;      /* Bytes per unit (sector) */
    uint8_t  raw_sha1[20];
    uint8_t  sha1[20];
    uint8_t  parent_sha1[20];
} chd_header_v5_t;

/**
 * @brief Parsed CHD image info
 */
typedef struct {
    chd_header_v5_t header;
    uint32_t version;          /* Actual CHD version (3, 4, or 5) */
    uint32_t hunk_count;
    uint32_t cylinders;
    uint32_t heads;
    uint32_t sectors;
    uint32_t sector_size;
    bool     is_floppy;        /* true if logical_bytes < 10MB */
    bool     is_compressed;    /* true if any compressor != NONE */
    char     description[256];
} uft_chd_info_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Open and parse a CHD file (header + metadata only)
 * @param path File path
 * @param info Output CHD info
 * @return 0 on success, negative on error
 */
int uft_chd_open(const char *path, uft_chd_info_t *info);

/**
 * @brief Read a sector from an uncompressed CHD file
 * @param path File path
 * @param info Parsed CHD info (from uft_chd_open)
 * @param cyl Cylinder number
 * @param head Head number
 * @param sector Sector number (1-based)
 * @param buf Output buffer
 * @param buflen Buffer size (must be >= info->sector_size)
 * @return 0 on success, negative on error
 */
int uft_chd_read_sector(const char *path, const uft_chd_info_t *info,
                         int cyl, int head, int sector,
                         uint8_t *buf, size_t buflen);

/**
 * @brief Read a metadata entry from CHD file
 * @param path File path
 * @param tag Metadata tag (e.g., "GDDD")
 * @param value Output buffer for metadata value string
 * @param valuelen Buffer size
 * @return 0 on success, negative on error
 */
int uft_chd_get_metadata(const char *path, const char *tag,
                          char *value, size_t valuelen);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CHD_H */
