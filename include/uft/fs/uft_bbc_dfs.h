/**
 * @file uft_bbc_dfs.h
 * @brief BBC Micro DFS/ADFS Filesystem Layer
 * @version 2.0.0
 *
 * Complete filesystem support for BBC Micro disk formats:
 * - Acorn DFS (standard 31 files)
 * - Watford DFS (62 files)
 * - Opus DDOS
 * - ADFS (S/M/L/D/E/F/G/+)
 *
 * Image formats: SSD, DSD, ADF, ADL, ADM, ADS
 *
 * Based on:
 * - BBC Micro Advanced User Guide (AUG)
 * - Acorn DFS User Guide
 * - ADFS Reference Manual
 * - bbctapedisc by W.H.Scholten, R.Schmidt, Jon Welch
 */

#ifndef UFT_BBC_DFS_H
#define UFT_BBC_DFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_compiler.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Version and Limits
 *===========================================================================*/

#define UFT_BBC_DFS_VERSION_MAJOR   2
#define UFT_BBC_DFS_VERSION_MINOR   0
#define UFT_BBC_DFS_VERSION_PATCH   0

#define UFT_BBC_MAX_FILENAME        10      /**< Max filename length */
#define UFT_BBC_MAX_PATH            256     /**< Max path length (ADFS) */
#define UFT_BBC_MAX_TITLE           12      /**< Max disk title */

/*===========================================================================
 * DFS Disk Geometry
 *===========================================================================*/

#define UFT_DFS_SECTOR_SIZE         256     /**< Bytes per sector */
#define UFT_DFS_SECTORS_PER_TRACK   10      /**< Sectors per track (FM) */
#define UFT_DFS_SECTORS_PER_TRACK_MFM 16    /**< Sectors per track (MFM) */
#define UFT_DFS_TRACKS_40           40      /**< 40-track disk */
#define UFT_DFS_TRACKS_80           80      /**< 80-track disk */

/** Standard disk sizes */
typedef enum {
    UFT_DFS_SS40    = 0,    /**< Single-sided 40-track (100KB) */
    UFT_DFS_SS80    = 1,    /**< Single-sided 80-track (200KB) */
    UFT_DFS_DS40    = 2,    /**< Double-sided 40-track (200KB) */
    UFT_DFS_DS80    = 3,    /**< Double-sided 80-track (400KB) */
    UFT_DFS_DS80_MFM = 4    /**< Double-sided 80-track MFM (640KB) */
} uft_dfs_geometry_t;

/** Disk size table */
#define UFT_DFS_SS40_SECTORS    400
#define UFT_DFS_SS80_SECTORS    800
#define UFT_DFS_DS40_SECTORS    800
#define UFT_DFS_DS80_SECTORS    1600
#define UFT_DFS_DS80_MFM_SECTORS 2560

#define UFT_DFS_SS40_SIZE       (400 * 256)     /**< 102,400 bytes */
#define UFT_DFS_SS80_SIZE       (800 * 256)     /**< 204,800 bytes */
#define UFT_DFS_DS40_SIZE       (800 * 256)     /**< 204,800 bytes */
#define UFT_DFS_DS80_SIZE       (1600 * 256)    /**< 409,600 bytes */
#define UFT_DFS_DS80_MFM_SIZE   (2560 * 256)    /**< 655,360 bytes */

/*===========================================================================
 * DFS Types
 *===========================================================================*/

/** DFS variant */
typedef enum {
    UFT_DFS_ACORN       = 0,    /**< Standard Acorn DFS (31 files) */
    UFT_DFS_WATFORD     = 1,    /**< Watford DFS (62 files) */
    UFT_DFS_OPUS        = 2,    /**< Opus DDOS */
    UFT_DFS_SOLIDISK    = 3,    /**< Solidisk DFS */
    UFT_DFS_UNKNOWN     = 255
} uft_dfs_variant_t;

/** Boot options */
typedef enum {
    UFT_DFS_BOOT_NONE   = 0,    /**< No boot action */
    UFT_DFS_BOOT_LOAD   = 1,    /**< *LOAD $.!BOOT */
    UFT_DFS_BOOT_RUN    = 2,    /**< *RUN $.!BOOT */
    UFT_DFS_BOOT_EXEC   = 3     /**< *EXEC $.!BOOT */
} uft_dfs_boot_t;

/*===========================================================================
 * DFS Catalog Limits
 *===========================================================================*/

#define UFT_DFS_MAX_FILES           31      /**< Standard Acorn DFS */
#define UFT_DFS_MAX_FILES_WATFORD   62      /**< Watford DFS */
#define UFT_DFS_FILENAME_LEN        7       /**< Filename length (excl. dir) */
#define UFT_DFS_ENTRY_SIZE          8       /**< Bytes per catalog entry */

/** Catalog sector locations */
#define UFT_DFS_CAT_SECTOR0         0       /**< Catalog sector 0 */
#define UFT_DFS_CAT_SECTOR1         1       /**< Catalog sector 1 */

/*===========================================================================
 * DFS On-Disk Structures
 *===========================================================================*/

/**
 * @brief DFS Catalog Sector 0 (256 bytes)
 *
 * Layout:
 * - Bytes 0-7: Disk title (first 8 chars)
 * - Bytes 8-255: File entries (31 max), 8 bytes each:
 *   - Bytes 0-6: Filename (space-padded, 7F masked)
 *   - Byte 7: Directory letter (bit 7 = locked flag)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t title1[8];          /**< Disk title (first 8 chars) */
    uint8_t entries[248];       /**< File entries (31 × 8 bytes) */
} uft_dfs_cat0_t;
UFT_PACK_END

/**
 * @brief DFS Catalog Sector 1 (256 bytes)
 *
 * Layout:
 * - Bytes 0-3: Disk title (last 4 chars)
 * - Byte 4: Sequence number (BCD cycle number)
 * - Byte 5: Number of catalog entries × 8
 * - Byte 6: Boot option (bits 4-5) + sectors high (bits 0-1)
 * - Byte 7: Total sectors (low byte)
 * - Bytes 8-255: File info entries, 8 bytes each:
 *   - Bytes 0-1: Load address (low 16 bits)
 *   - Bytes 2-3: Exec address (low 16 bits)
 *   - Bytes 4-5: File length (low 16 bits)
 *   - Byte 6: Mixed bits (address/length high bits)
 *   - Byte 7: Start sector (low byte)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t title2[4];          /**< Disk title (last 4 chars) */
    uint8_t sequence;           /**< Sequence number (BCD) */
    uint8_t num_entries;        /**< Number of entries × 8 */
    uint8_t opt_sectors_hi;     /**< Boot option + sectors high */
    uint8_t sectors_lo;         /**< Total sectors low */
    uint8_t info[248];          /**< File info entries */
} uft_dfs_cat1_t;
UFT_PACK_END

/**
 * @brief Mixed bits byte layout (catalog sector 1, entry byte 6)
 *
 * - Bits 0-1: Start sector (bits 8-9)
 * - Bits 2-3: Load address (bits 16-17)
 * - Bits 4-5: File length (bits 16-17)
 * - Bits 6-7: Exec address (bits 16-17)
 */
#define UFT_DFS_MIXED_START_HI(m)   ((m) & 0x03)
#define UFT_DFS_MIXED_LOAD_HI(m)    (((m) >> 2) & 0x03)
#define UFT_DFS_MIXED_LEN_HI(m)     (((m) >> 4) & 0x03)
#define UFT_DFS_MIXED_EXEC_HI(m)    (((m) >> 6) & 0x03)

/** Create mixed bits byte */
#define UFT_DFS_MAKE_MIXED(start, load, len, exec) \
    ((((start) >> 8) & 0x03) | \
     ((((load) >> 16) & 0x03) << 2) | \
     ((((len) >> 16) & 0x03) << 4) | \
     ((((exec) >> 16) & 0x03) << 6))

/*===========================================================================
 * ADFS Definitions
 *===========================================================================*/

/** ADFS format types */
typedef enum {
    UFT_ADFS_S      = 0,    /**< 160KB (single density) */
    UFT_ADFS_M      = 1,    /**< 320KB (double density) */
    UFT_ADFS_L      = 2,    /**< 640KB (interleaved) */
    UFT_ADFS_D      = 3,    /**< 800KB (hard disc) */
    UFT_ADFS_E      = 4,    /**< 800KB (new map) */
    UFT_ADFS_F      = 5,    /**< 1.6MB (new map) */
    UFT_ADFS_G      = 6,    /**< Large hard disc */
    UFT_ADFS_PLUS   = 7,    /**< ADFS+ extended */
    UFT_ADFS_UNKNOWN = 255
} uft_adfs_format_t;

/** ADFS sector sizes */
#define UFT_ADFS_SECTOR_256     256
#define UFT_ADFS_SECTOR_512     512
#define UFT_ADFS_SECTOR_1024    1024

/** ADFS limits */
#define UFT_ADFS_DIR_ENTRIES    47      /**< Max entries per directory */
#define UFT_ADFS_ENTRY_SIZE     26      /**< Bytes per directory entry */
#define UFT_ADFS_FILENAME_LEN   10      /**< Max filename length */

/** ADFS attributes */
#define UFT_ADFS_ATTR_READ          0x01    /**< Owner read */
#define UFT_ADFS_ATTR_WRITE         0x02    /**< Owner write */
#define UFT_ADFS_ATTR_LOCKED        0x04    /**< Locked */
#define UFT_ADFS_ATTR_DIRECTORY     0x08    /**< Is directory */
#define UFT_ADFS_ATTR_EXEC          0x10    /**< Owner execute */
#define UFT_ADFS_ATTR_PUBLIC_READ   0x20    /**< Public read */
#define UFT_ADFS_ATTR_PUBLIC_WRITE  0x40    /**< Public write */
#define UFT_ADFS_ATTR_PUBLIC_EXEC   0x80    /**< Public execute */

/**
 * @brief ADFS Free Space Entry (old map)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t start[3];           /**< Start sector (24-bit LE) */
    uint8_t length[3];          /**< Length in sectors (24-bit LE) */
} uft_adfs_free_entry_t;
UFT_PACK_END

/**
 * @brief ADFS Directory Entry (old map, 26 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  name[10];          /**< Filename (bit 7 of byte 0 = permissions) */
    uint32_t load_addr;         /**< Load address */
    uint32_t exec_addr;         /**< Exec address */
    uint32_t length;            /**< File length */
    uint8_t  start[3];          /**< Start sector (24-bit) */
} uft_adfs_dir_entry_t;
UFT_PACK_END

/**
 * @brief ADFS Directory Header (old map)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  seq;               /**< Sequence number */
    char     name[10];          /**< Directory name */
    uint8_t  parent[3];         /**< Parent directory sector */
    char     title[19];         /**< Directory title */
    uint8_t  reserved[14];      /**< Reserved */
    uint8_t  entries_count;     /**< Number of entries */
} uft_adfs_dir_header_t;
UFT_PACK_END

/*===========================================================================
 * High-Level Structures
 *===========================================================================*/

/**
 * @brief File entry (unified for DFS/ADFS)
 */
typedef struct {
    char        filename[UFT_BBC_MAX_FILENAME + 1];  /**< Filename */
    char        directory;      /**< Directory letter (DFS) or '\0' (ADFS) */
    char        path[UFT_BBC_MAX_PATH];              /**< Full path (ADFS) */
    
    uint32_t    load_addr;      /**< Load address (18/32-bit) */
    uint32_t    exec_addr;      /**< Exec address (18/32-bit) */
    uint32_t    length;         /**< File length */
    uint32_t    start_sector;   /**< Start sector */
    
    bool        locked;         /**< File is locked */
    bool        is_directory;   /**< Is directory (ADFS) */
    uint8_t     attributes;     /**< ADFS attributes */
    
    int         index;          /**< Catalog index */
    int         side;           /**< Side (0 or 1 for DSD) */
} uft_bbc_file_t;

/**
 * @brief Directory listing
 */
typedef struct {
    uft_bbc_file_t  *files;     /**< File array */
    int             count;      /**< Number of files */
    int             capacity;   /**< Array capacity */
    
    uint32_t        total_size; /**< Total bytes used */
    uint32_t        free_space; /**< Free bytes */
    int             free_sectors;/**< Free sectors */
} uft_bbc_dir_t;

/**
 * @brief Detection result
 */
typedef struct {
    bool            valid;          /**< Valid filesystem detected */
    int             confidence;     /**< Confidence 0-100% */
    
    bool            is_adfs;        /**< ADFS (vs DFS) */
    uft_dfs_variant_t dfs_variant;  /**< DFS variant */
    uft_adfs_format_t adfs_format;  /**< ADFS format */
    uft_dfs_geometry_t geometry;    /**< Disk geometry */
    
    uint16_t        total_sectors;  /**< Total sectors */
    int             tracks;         /**< Number of tracks */
    int             sides;          /**< Number of sides */
    int             sectors_per_track; /**< Sectors per track */
    
    char            title[UFT_BBC_MAX_TITLE + 1]; /**< Disk title */
    uft_dfs_boot_t  boot_option;    /**< Boot option */
    int             file_count;     /**< Number of files */
    
    const char      *description;   /**< Human-readable description */
} uft_bbc_detect_t;

/**
 * @brief Filesystem context
 */
typedef struct {
    uint8_t         *data;          /**< Image data */
    size_t          size;           /**< Image size */
    bool            owns_data;      /**< We own the data buffer */
    bool            modified;       /**< Data has been modified */
    
    bool            is_adfs;        /**< ADFS vs DFS */
    uft_dfs_variant_t dfs_variant;  /**< DFS variant */
    uft_adfs_format_t adfs_format;  /**< ADFS format */
    uft_dfs_geometry_t geometry;    /**< Disk geometry */
    
    uint16_t        total_sectors;  /**< Total sectors */
    int             tracks;         /**< Number of tracks */
    int             sides;          /**< Number of sides */
    int             spt;            /**< Sectors per track */
    int             sector_size;    /**< Bytes per sector */
    
    int             max_files;      /**< Max files (31 or 62) */
    
    /* Cached catalog (DFS) */
    uft_dfs_cat0_t  cat0[2];        /**< Catalog sector 0 (per side) */
    uft_dfs_cat1_t  cat1[2];        /**< Catalog sector 1 (per side) */
    bool            cat_valid[2];   /**< Catalog loaded for side */
    
    /* Current directory (ADFS) */
    uint32_t        current_dir;    /**< Current directory sector */
    char            cwd[UFT_BBC_MAX_PATH]; /**< Current working directory */
} uft_bbc_ctx_t;

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    UFT_BBC_OK              = 0,
    UFT_BBC_ERR_INVALID     = -1,   /**< Invalid parameter */
    UFT_BBC_ERR_NOMEM       = -2,   /**< Out of memory */
    UFT_BBC_ERR_IO          = -3,   /**< I/O error */
    UFT_BBC_ERR_FORMAT      = -4,   /**< Invalid format */
    UFT_BBC_ERR_NOT_FOUND   = -5,   /**< File not found */
    UFT_BBC_ERR_EXISTS      = -6,   /**< File already exists */
    UFT_BBC_ERR_FULL        = -7,   /**< Disk full */
    UFT_BBC_ERR_CAT_FULL    = -8,   /**< Catalog full */
    UFT_BBC_ERR_LOCKED      = -9,   /**< File is locked */
    UFT_BBC_ERR_READ_ONLY   = -10,  /**< Read-only image */
    UFT_BBC_ERR_NAME        = -11,  /**< Invalid filename */
    UFT_BBC_ERR_RANGE       = -12   /**< Out of range */
} uft_bbc_error_t;

/*===========================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create filesystem context
 * @return New context or NULL on failure
 */
uft_bbc_ctx_t *uft_bbc_create(void);

/**
 * @brief Destroy filesystem context
 * @param ctx Context to destroy
 */
void uft_bbc_destroy(uft_bbc_ctx_t *ctx);

/**
 * @brief Open disk image
 * @param ctx Context
 * @param data Image data
 * @param size Image size
 * @param copy If true, copy data; if false, reference it
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_open(uft_bbc_ctx_t *ctx, const uint8_t *data, size_t size, bool copy);

/**
 * @brief Open disk image with explicit format
 * @param ctx Context
 * @param data Image data
 * @param size Image size
 * @param is_adfs True for ADFS, false for DFS
 * @param variant DFS variant (if DFS)
 * @param geometry Disk geometry
 * @param copy Copy data flag
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_open_with_format(uft_bbc_ctx_t *ctx, const uint8_t *data, size_t size,
                             bool is_adfs, uft_dfs_variant_t variant,
                             uft_dfs_geometry_t geometry, bool copy);

/**
 * @brief Close disk image (keeps context)
 * @param ctx Context
 */
void uft_bbc_close(uft_bbc_ctx_t *ctx);

/**
 * @brief Save modified image
 * @param ctx Context
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or negative error
 */
int uft_bbc_save(uft_bbc_ctx_t *ctx, uint8_t *buffer, size_t buffer_size);

/*===========================================================================
 * Detection API
 *===========================================================================*/

/**
 * @brief Detect disk format
 * @param data Image data
 * @param size Image size
 * @param result Output detection result
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_detect(const uint8_t *data, size_t size, uft_bbc_detect_t *result);

/**
 * @brief Get geometry from size
 * @param size Image size
 * @return Geometry enum or -1 if unknown
 */
int uft_bbc_geometry_from_size(size_t size);

/**
 * @brief Get size for geometry
 * @param geometry Geometry enum
 * @return Size in bytes
 */
size_t uft_bbc_size_for_geometry(uft_dfs_geometry_t geometry);

/*===========================================================================
 * Sector I/O API
 *===========================================================================*/

/**
 * @brief Read sector
 * @param ctx Context
 * @param track Track number
 * @param side Side (0 or 1)
 * @param sector Sector number
 * @param buffer Output buffer (256 or 512 bytes)
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_read_sector(uft_bbc_ctx_t *ctx, int track, int side, int sector,
                        uint8_t *buffer);

/**
 * @brief Write sector
 * @param ctx Context
 * @param track Track number
 * @param side Side (0 or 1)
 * @param sector Sector number
 * @param data Data to write
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_write_sector(uft_bbc_ctx_t *ctx, int track, int side, int sector,
                         const uint8_t *data);

/**
 * @brief Get sector offset in image
 * @param ctx Context
 * @param track Track number
 * @param side Side (0 or 1)
 * @param sector Sector number
 * @return Offset or -1 if invalid
 */
int uft_bbc_sector_offset(const uft_bbc_ctx_t *ctx, int track, int side, int sector);

/*===========================================================================
 * Catalog API (DFS)
 *===========================================================================*/

/**
 * @brief Read catalog from disk
 * @param ctx Context
 * @param side Side (0 or 1)
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_read_catalog(uft_bbc_ctx_t *ctx, int side);

/**
 * @brief Write catalog to disk
 * @param ctx Context
 * @param side Side (0 or 1)
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_write_catalog(uft_bbc_ctx_t *ctx, int side);

/**
 * @brief Get disk title
 * @param ctx Context
 * @param side Side (0 or 1)
 * @param title Output buffer (at least 13 bytes)
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_get_title(uft_bbc_ctx_t *ctx, int side, char *title);

/**
 * @brief Set disk title
 * @param ctx Context
 * @param side Side (0 or 1)
 * @param title New title (up to 12 chars)
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_set_title(uft_bbc_ctx_t *ctx, int side, const char *title);

/**
 * @brief Get boot option
 * @param ctx Context
 * @param side Side (0 or 1)
 * @return Boot option enum
 */
uft_dfs_boot_t uft_bbc_get_boot_option(uft_bbc_ctx_t *ctx, int side);

/**
 * @brief Set boot option
 * @param ctx Context
 * @param side Side (0 or 1)
 * @param boot Boot option
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_set_boot_option(uft_bbc_ctx_t *ctx, int side, uft_dfs_boot_t boot);

/**
 * @brief Get free space
 * @param ctx Context
 * @param side Side (0 or 1)
 * @param free_sectors Output: free sectors
 * @return Free bytes
 */
uint32_t uft_bbc_get_free_space(uft_bbc_ctx_t *ctx, int side, int *free_sectors);

/*===========================================================================
 * Directory API
 *===========================================================================*/

/**
 * @brief Read directory listing
 * @param ctx Context
 * @param side Side (0 or 1 for DFS, ignored for ADFS)
 * @param directory Directory letter (DFS) or path (ADFS), NULL for all/root
 * @param dir Output directory structure
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_read_directory(uft_bbc_ctx_t *ctx, int side, const char *directory,
                           uft_bbc_dir_t *dir);

/**
 * @brief Free directory listing
 * @param dir Directory to free
 */
void uft_bbc_free_directory(uft_bbc_dir_t *dir);

/**
 * @brief Find file by name
 * @param ctx Context
 * @param side Side (0 or 1)
 * @param filename Full filename (D.NAME or NAME)
 * @param entry Output file entry
 * @return UFT_BBC_OK or UFT_BBC_ERR_NOT_FOUND
 */
int uft_bbc_find_file(uft_bbc_ctx_t *ctx, int side, const char *filename,
                      uft_bbc_file_t *entry);

/**
 * @brief Iterate over files
 * @param ctx Context
 * @param side Side (0 or 1)
 * @param callback Called for each file
 * @param user_data User data passed to callback
 * @return Number of files processed
 */
int uft_bbc_foreach_file(uft_bbc_ctx_t *ctx, int side,
                         int (*callback)(const uft_bbc_file_t *entry, void *user),
                         void *user_data);

/*===========================================================================
 * File Operations API
 *===========================================================================*/

/**
 * @brief Extract file data
 * @param ctx Context
 * @param entry File entry
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes extracted or negative error
 */
int uft_bbc_extract_file(uft_bbc_ctx_t *ctx, const uft_bbc_file_t *entry,
                         uint8_t *buffer, size_t buffer_size);

/**
 * @brief Extract file to disk
 * @param ctx Context
 * @param entry File entry
 * @param path Output file path
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_extract_to_file(uft_bbc_ctx_t *ctx, const uft_bbc_file_t *entry,
                            const char *path);

/**
 * @brief Inject file into disk image
 * @param ctx Context
 * @param side Side (0 or 1)
 * @param filename BBC filename (e.g., "$.PROGRAM" or "PROGRAM")
 * @param load_addr Load address
 * @param exec_addr Exec address
 * @param data File data
 * @param length Data length
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_inject_file(uft_bbc_ctx_t *ctx, int side, const char *filename,
                        uint32_t load_addr, uint32_t exec_addr,
                        const uint8_t *data, size_t length);

/**
 * @brief Inject file from disk
 * @param ctx Context
 * @param side Side (0 or 1)
 * @param filename BBC filename
 * @param load_addr Load address
 * @param exec_addr Exec address
 * @param path Source file path
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_inject_from_file(uft_bbc_ctx_t *ctx, int side, const char *filename,
                             uint32_t load_addr, uint32_t exec_addr,
                             const char *path);

/**
 * @brief Delete file
 * @param ctx Context
 * @param side Side (0 or 1)
 * @param filename Filename to delete
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_delete_file(uft_bbc_ctx_t *ctx, int side, const char *filename);

/**
 * @brief Rename file
 * @param ctx Context
 * @param side Side (0 or 1)
 * @param old_name Current filename
 * @param new_name New filename
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_rename_file(uft_bbc_ctx_t *ctx, int side, const char *old_name,
                        const char *new_name);

/**
 * @brief Lock/unlock file
 * @param ctx Context
 * @param side Side (0 or 1)
 * @param filename Filename
 * @param locked True to lock, false to unlock
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_set_locked(uft_bbc_ctx_t *ctx, int side, const char *filename,
                       bool locked);

/**
 * @brief Set file attributes (ADFS)
 * @param ctx Context
 * @param filename Filename
 * @param attributes Attribute byte
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_set_attributes(uft_bbc_ctx_t *ctx, const char *filename,
                           uint8_t attributes);

/*===========================================================================
 * Image Creation API
 *===========================================================================*/

/**
 * @brief Create blank DFS disk image
 * @param buffer Output buffer
 * @param geometry Disk geometry
 * @param title Disk title (optional)
 * @param boot_option Boot option
 * @return Image size or negative error
 */
int uft_bbc_create_dfs_image(uint8_t *buffer, uft_dfs_geometry_t geometry,
                             const char *title, uft_dfs_boot_t boot_option);

/**
 * @brief Create blank ADFS disk image
 * @param buffer Output buffer
 * @param format ADFS format
 * @param title Disk title (optional)
 * @return Image size or negative error
 */
int uft_bbc_create_adfs_image(uint8_t *buffer, uft_adfs_format_t format,
                              const char *title);

/**
 * @brief Format existing context
 * @param ctx Context
 * @param title New title (optional)
 * @param boot_option Boot option (DFS)
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_format(uft_bbc_ctx_t *ctx, const char *title, uft_dfs_boot_t boot_option);

/*===========================================================================
 * Validation API
 *===========================================================================*/

/**
 * @brief Validate disk image
 * @param ctx Context
 * @param report Output buffer for report (optional)
 * @param report_size Report buffer size
 * @return UFT_BBC_OK if valid, error code if issues found
 */
int uft_bbc_validate(uft_bbc_ctx_t *ctx, char *report, size_t report_size);

/**
 * @brief Check for overlapping files
 * @param ctx Context
 * @param side Side (0 or 1)
 * @return Number of overlapping file pairs, 0 if none
 */
int uft_bbc_check_overlaps(uft_bbc_ctx_t *ctx, int side);

/**
 * @brief Compact disk (defragment)
 * @param ctx Context
 * @param side Side (0 or 1)
 * @return UFT_BBC_OK or error code
 */
int uft_bbc_compact(uft_bbc_ctx_t *ctx, int side);

/*===========================================================================
 * Utility API
 *===========================================================================*/

/**
 * @brief Parse BBC filename
 * @param input Input string (e.g., "$.PROGRAM" or "A.FILE")
 * @param directory Output directory letter
 * @param filename Output filename (7 chars + null)
 * @return UFT_BBC_OK or UFT_BBC_ERR_NAME
 */
int uft_bbc_parse_filename(const char *input, char *directory, char *filename);

/**
 * @brief Format filename for display
 * @param directory Directory letter
 * @param filename Filename
 * @param buffer Output buffer (at least 10 bytes)
 */
void uft_bbc_format_filename(char directory, const char *filename, char *buffer);

/**
 * @brief Validate filename
 * @param filename Filename to validate
 * @return true if valid
 */
bool uft_bbc_validate_filename(const char *filename);

/**
 * @brief Get boot option name
 * @param boot Boot option
 * @return Human-readable name
 */
const char *uft_bbc_boot_option_name(uft_dfs_boot_t boot);

/**
 * @brief Get DFS variant name
 * @param variant DFS variant
 * @return Human-readable name
 */
const char *uft_bbc_dfs_variant_name(uft_dfs_variant_t variant);

/**
 * @brief Get ADFS format name
 * @param format ADFS format
 * @return Human-readable name
 */
const char *uft_bbc_adfs_format_name(uft_adfs_format_t format);

/**
 * @brief Get geometry name
 * @param geometry Geometry
 * @return Human-readable name
 */
const char *uft_bbc_geometry_name(uft_dfs_geometry_t geometry);

/**
 * @brief Get error message
 * @param error Error code
 * @return Human-readable error message
 */
const char *uft_bbc_error_string(int error);

/*===========================================================================
 * Print/Export API
 *===========================================================================*/

/**
 * @brief Print directory listing
 * @param ctx Context
 * @param side Side (0 or 1)
 * @param output Output file (stdout if NULL)
 */
void uft_bbc_print_directory(uft_bbc_ctx_t *ctx, int side, FILE *output);

/**
 * @brief Print disk info
 * @param ctx Context
 * @param output Output file (stdout if NULL)
 */
void uft_bbc_print_info(uft_bbc_ctx_t *ctx, FILE *output);

/**
 * @brief Export directory as JSON
 * @param ctx Context
 * @param side Side (0 or 1)
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or negative error
 */
int uft_bbc_directory_to_json(uft_bbc_ctx_t *ctx, int side, char *buffer,
                              size_t buffer_size);

/**
 * @brief Export disk info as JSON
 * @param ctx Context
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or negative error
 */
int uft_bbc_info_to_json(uft_bbc_ctx_t *ctx, char *buffer, size_t buffer_size);

/*===========================================================================
 * BBC CRC-16
 *===========================================================================*/

/**
 * @brief Calculate BBC CRC-16
 *
 * Uses the BBC-specific CRC polynomial from AUG p.348.
 *
 * @param data Input data
 * @param len Data length
 * @return CRC-16 value
 */
static inline uint16_t uft_bbc_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc ^= 0x0810;
                crc = (crc << 1) | 1;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/*===========================================================================
 * Inline Helpers
 *===========================================================================*/

/** Get total sectors from catalog */
static inline uint16_t uft_dfs_get_sectors(const uft_dfs_cat1_t *cat1)
{
    return cat1->sectors_lo | ((cat1->opt_sectors_hi & 0x03) << 8);
}

/** Get boot option from catalog */
static inline uft_dfs_boot_t uft_dfs_get_boot_opt(const uft_dfs_cat1_t *cat1)
{
    return (uft_dfs_boot_t)((cat1->opt_sectors_hi >> 4) & 0x03);
}

/** Get file count from catalog */
static inline int uft_dfs_get_file_count(const uft_dfs_cat1_t *cat1)
{
    return cat1->num_entries / 8;
}

/** Read 24-bit little-endian value */
static inline uint32_t uft_bbc_read24le(const uint8_t *p)
{
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

/** Write 24-bit little-endian value */
static inline void uft_bbc_write24le(uint8_t *p, uint32_t value)
{
    p[0] = value & 0xFF;
    p[1] = (value >> 8) & 0xFF;
    p[2] = (value >> 16) & 0xFF;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_BBC_DFS_H */
