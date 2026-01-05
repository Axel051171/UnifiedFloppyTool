/**
 * @file uft_cpm_fs.h
 * @brief CP/M Filesystem Layer - Complete Implementation
 * 
 * Comprehensive CP/M filesystem support for preservation and analysis:
 * - CP/M 2.2 and CP/M 3.0 (CP/M Plus)
 * - Multiple disk formats (8", 5.25", 3.5", 3")
 * - Various sector sizes (128, 256, 512, 1024 bytes)
 * - Block sizes from 1KB to 16KB
 * - Directory label and date stamps (CP/M 3.0)
 * - System tracks and reserved areas
 * - Extent handling for files >16KB
 * - User number support (0-15 for CP/M 2.2, 0-31 for 3.0)
 * 
 * Supported systems:
 * - Standard 8" SSSD/DSDD (IBM 3740 format)
 * - Kaypro II/4/10
 * - Osborne 1
 * - Amstrad CPC/PCW
 * - Epson QX-10
 * - Commodore 128 CP/M
 * - Apple II with Z80 card
 * - TRS-80 Model 4 CP/M
 * - BBC Master 512 CP/M
 * - MSX-DOS (CP/M compatible)
 * - NEC PC-8801/PC-9801 CP/M
 * - Zorba
 * - Morrow
 * - Xerox 820
 * 
 * @author UFT Team
 * @version 1.0.0
 */

#ifndef UFT_CPM_FS_H
#define UFT_CPM_FS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include "uft/uft_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum filename length (8 chars) */
#define UFT_CPM_MAX_NAME        8

/** Maximum extension length (3 chars) */
#define UFT_CPM_MAX_EXT         3

/** Directory entry size (always 32 bytes) */
#define UFT_CPM_DIR_ENTRY_SIZE  32

/** Maximum extent number in one entry */
#define UFT_CPM_MAX_EXTENT      31

/** Maximum user number (CP/M 2.2) */
#define UFT_CPM_MAX_USER_22     15

/** Maximum user number (CP/M 3.0) */
#define UFT_CPM_MAX_USER_30     31

/** Deleted file marker */
#define UFT_CPM_DELETED         0xE5

/** Directory label marker */
#define UFT_CPM_DIR_LABEL       0x20

/** Date stamps marker */
#define UFT_CPM_DATE_STAMPS     0x21

/** Maximum files in directory listing */
#define UFT_CPM_MAX_FILES       1024

/** Maximum disk parameter blocks */
#define UFT_CPM_MAX_DPB         64

/*===========================================================================
 * CP/M Version
 *===========================================================================*/

/**
 * @brief CP/M version enumeration
 */
typedef enum {
    UFT_CPM_VERSION_UNKNOWN = 0,
    UFT_CPM_VERSION_22,         /**< CP/M 2.2 */
    UFT_CPM_VERSION_30,         /**< CP/M 3.0 (Plus) */
    UFT_CPM_VERSION_MSDOS,      /**< MSX-DOS (CP/M compatible) */
    UFT_CPM_VERSION_CDOS,       /**< Cromemco CDOS */
    UFT_CPM_VERSION_ZDOS,       /**< Z80DOS */
    UFT_CPM_VERSION_ZCPR,       /**< ZCPR3 */
} uft_cpm_version_t;

/*===========================================================================
 * Disk Formats - Disk Parameter Block (DPB)
 *===========================================================================*/

/**
 * @brief Known CP/M disk format types
 */
typedef enum {
    UFT_CPM_FMT_UNKNOWN = 0,
    
    /* 8" formats */
    UFT_CPM_FMT_8_SSSD,         /**< 8" SSSD IBM 3740, 77×26×128 */
    UFT_CPM_FMT_8_SSDD,         /**< 8" SSDD, 77×26×256 */
    UFT_CPM_FMT_8_DSSD,         /**< 8" DSSD, 77×26×128×2 */
    UFT_CPM_FMT_8_DSDD,         /**< 8" DSDD, 77×26×256×2 */
    
    /* 5.25" formats */
    UFT_CPM_FMT_525_SSSD,       /**< 5.25" SSSD 40×10×128 */
    UFT_CPM_FMT_525_SSDD,       /**< 5.25" SSDD 40×9×512 */
    UFT_CPM_FMT_525_DSDD,       /**< 5.25" DSDD 40×9×512×2 */
    UFT_CPM_FMT_525_DSQD,       /**< 5.25" DSQD 80×9×512×2 */
    UFT_CPM_FMT_525_DSHD,       /**< 5.25" DSHD 80×15×512×2 */
    
    /* 3.5" formats */
    UFT_CPM_FMT_35_SSDD,        /**< 3.5" SSDD 80×9×512 */
    UFT_CPM_FMT_35_DSDD,        /**< 3.5" DSDD 80×9×512×2 */
    UFT_CPM_FMT_35_DSHD,        /**< 3.5" DSHD 80×18×512×2 */
    
    /* 3" formats */
    UFT_CPM_FMT_3_SSDD,         /**< 3" SSDD 40×9×512 (Amstrad) */
    UFT_CPM_FMT_3_DSDD,         /**< 3" DSDD 40×9×512×2 (Amstrad) */
    
    /* Specific machine formats */
    UFT_CPM_FMT_KAYPRO_II,      /**< Kaypro II: 40×10×512 */
    UFT_CPM_FMT_KAYPRO_4,       /**< Kaypro 4: 40×10×512×2 */
    UFT_CPM_FMT_KAYPRO_10,      /**< Kaypro 10: 80×10×512×2 */
    UFT_CPM_FMT_OSBORNE_1,      /**< Osborne 1: 40×10×256 */
    UFT_CPM_FMT_OSBORNE_DD,     /**< Osborne DD: 40×5×1024×2 */
    UFT_CPM_FMT_AMSTRAD_PCW,    /**< Amstrad PCW: 80×9×512 */
    UFT_CPM_FMT_AMSTRAD_CPC_SYS,/**< Amstrad CPC System: 40×9×512 */
    UFT_CPM_FMT_AMSTRAD_CPC_DATA,/**< Amstrad CPC Data: 40×9×512 */
    UFT_CPM_FMT_EPSON_QX10,     /**< Epson QX-10: 40×16×256×2 */
    UFT_CPM_FMT_C128,           /**< Commodore 128: 40×17×256×2 */
    UFT_CPM_FMT_APPLE_CPM,      /**< Apple II CP/M: 35×16×256 */
    UFT_CPM_FMT_TRS80_M4,       /**< TRS-80 Model 4 CP/M */
    UFT_CPM_FMT_BBC_CPM,        /**< BBC Master 512 CP/M */
    UFT_CPM_FMT_MORROW,         /**< Morrow Micro Decision */
    UFT_CPM_FMT_XEROX_820,      /**< Xerox 820 */
    UFT_CPM_FMT_ZORBA,          /**< Zorba */
    UFT_CPM_FMT_NEC_PC88,       /**< NEC PC-8801 */
    UFT_CPM_FMT_NEC_PC98,       /**< NEC PC-9801 */
    UFT_CPM_FMT_MSX_DOS,        /**< MSX-DOS */
    UFT_CPM_FMT_GENERIC,        /**< Generic/auto-detect */
    
    UFT_CPM_FMT_COUNT
} uft_cpm_format_t;

/**
 * @brief Sector skew/interleave type
 */
typedef enum {
    UFT_CPM_SKEW_NONE = 0,      /**< No skew */
    UFT_CPM_SKEW_PHYSICAL,      /**< Physical sector skew */
    UFT_CPM_SKEW_LOGICAL,       /**< Logical sector skew */
    UFT_CPM_SKEW_CUSTOM,        /**< Custom skew table */
} uft_cpm_skew_type_t;

/**
 * @brief Side ordering for double-sided disks
 */
typedef enum {
    UFT_CPM_SIDES_ALT = 0,      /**< Alternating sides (track 0 side 0, track 0 side 1, ...) */
    UFT_CPM_SIDES_SEQ,          /**< Sequential sides (all side 0, then all side 1) */
    UFT_CPM_SIDES_OUTOUT,       /**< Out-Out (both heads same direction) */
    UFT_CPM_SIDES_OUTIN,        /**< Out-In (head 1 reversed) */
} uft_cpm_side_order_t;

/**
 * @brief Disk Parameter Block (DPB)
 * 
 * Central structure defining CP/M disk geometry and allocation
 */
typedef struct {
    /** Format name */
    char name[32];
    
    /** Physical geometry */
    uint8_t  tracks;            /**< Total tracks per side */
    uint8_t  sides;             /**< Number of sides (1 or 2) */
    uint8_t  sectors_per_track; /**< Sectors per track */
    uint16_t sector_size;       /**< Bytes per sector (128, 256, 512, 1024) */
    
    /** CP/M parameters */
    uint16_t spt;               /**< Sectors per track (128-byte units) */
    uint8_t  bsh;               /**< Block shift (log2(block_size/128)) */
    uint8_t  blm;               /**< Block mask (2^bsh - 1) */
    uint8_t  exm;               /**< Extent mask */
    uint16_t dsm;               /**< Disk size in blocks - 1 */
    uint16_t drm;               /**< Directory entries - 1 */
    uint8_t  al0;               /**< Directory allocation bitmap (high) */
    uint8_t  al1;               /**< Directory allocation bitmap (low) */
    uint16_t cks;               /**< Checksum vector size */
    uint16_t off;               /**< Track offset (reserved tracks) */
    
    /** Derived values */
    uint16_t block_size;        /**< Block size in bytes */
    uint16_t dir_entries;       /**< Total directory entries */
    uint16_t dir_blocks;        /**< Blocks used by directory */
    uint32_t total_bytes;       /**< Total disk capacity */
    
    /** Sector handling */
    uint8_t  first_sector;      /**< First sector number (0 or 1) */
    uint8_t  skew;              /**< Sector skew factor */
    uft_cpm_skew_type_t skew_type;
    uint8_t  skew_table[64];    /**< Custom skew table if needed */
    
    /** Side ordering */
    uft_cpm_side_order_t side_order;
    
    /** Format identification */
    uft_cpm_format_t format;
    uft_cpm_version_t version;
    
} uft_cpm_dpb_t;

/*===========================================================================
 * Directory Structures
 *===========================================================================*/

/**
 * @brief CP/M directory entry (32 bytes)
 * 
 * Raw structure as stored on disk
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  user;              /**< User number (0-15/31) or 0xE5 deleted */
    char     name[8];           /**< Filename (padded with spaces) */
    char     ext[3];            /**< Extension (padded with spaces) */
    uint8_t  ex;                /**< Extent low byte */
    uint8_t  s1;                /**< Reserved (usually 0) */
    uint8_t  s2;                /**< Extent high byte */
    uint8_t  rc;                /**< Record count (sectors in this extent) */
    uint8_t  al[16];            /**< Allocation map (block numbers) */
} uft_cpm_dir_entry_t;
UFT_PACK_END

/**
 * @brief File attribute flags
 */
typedef struct {
    bool read_only;             /**< F1' - Read-only attribute */
    bool system;                /**< F2' - System file */
    bool archived;              /**< F3' - Archived (CP/M 3.0) */
    bool f4;                    /**< F4' - User defined */
    bool f5;                    /**< F5' - User defined */
    bool f6;                    /**< F6' - User defined */
    bool f7;                    /**< F7' - User defined */
    bool f8;                    /**< F8' - User defined */
} uft_cpm_attrib_t;

/**
 * @brief CP/M 3.0 date stamp entry
 */
typedef struct {
    uint16_t create_date;       /**< Days since 1/1/1978 */
    uint8_t  create_time_h;     /**< Create hour (BCD) */
    uint8_t  create_time_m;     /**< Create minute (BCD) */
    uint16_t modify_date;       /**< Days since 1/1/1978 */
    uint8_t  modify_time_h;     /**< Modify hour (BCD) */
    uint8_t  modify_time_m;     /**< Modify minute (BCD) */
    uint16_t access_date;       /**< Days since 1/1/1978 */
    uint8_t  access_time_h;     /**< Access hour (BCD) */
    uint8_t  access_time_m;     /**< Access minute (BCD) */
} uft_cpm_stamps_t;

/**
 * @brief Decoded file entry
 */
typedef struct {
    /** User and filename */
    uint8_t  user;              /**< User number (0-15/31) */
    char     name[UFT_CPM_MAX_NAME + 1];  /**< Filename (null-terminated) */
    char     ext[UFT_CPM_MAX_EXT + 1];    /**< Extension (null-terminated) */
    char     fullname[16];      /**< Full filename as "NAME.EXT" */
    
    /** File info */
    uint32_t size;              /**< File size in bytes */
    uint32_t records;           /**< Number of 128-byte records */
    uint16_t blocks;            /**< Number of blocks allocated */
    uint16_t extents;           /**< Number of extents */
    
    /** Attributes */
    uft_cpm_attrib_t attrib;
    
    /** Date stamps (CP/M 3.0) */
    bool     has_stamps;
    time_t   create_time;
    time_t   modify_time;
    time_t   access_time;
    
    /** Internal tracking */
    uint16_t first_extent_idx;  /**< Index of first directory entry */
    
} uft_cpm_file_t;

/**
 * @brief Directory listing result
 */
typedef struct {
    uft_cpm_file_t *files;      /**< Array of files */
    size_t         count;       /**< Number of files */
    size_t         capacity;    /**< Array capacity */
    
    /** Statistics */
    uint32_t total_files;       /**< Total file count (including deleted) */
    uint32_t deleted_files;     /**< Deleted file count */
    uint32_t used_entries;      /**< Used directory entries */
    uint32_t free_entries;      /**< Free directory entries */
    uint32_t used_blocks;       /**< Used blocks */
    uint32_t free_blocks;       /**< Free blocks */
    uint32_t used_bytes;        /**< Used space in bytes */
    uint32_t free_bytes;        /**< Free space in bytes */
    
} uft_cpm_dir_t;

/*===========================================================================
 * Detection Result
 *===========================================================================*/

/**
 * @brief CP/M filesystem detection result
 */
typedef struct {
    bool detected;              /**< CP/M filesystem detected */
    float confidence;           /**< Detection confidence (0.0-1.0) */
    
    uft_cpm_format_t format;    /**< Detected format */
    uft_cpm_version_t version;  /**< Detected version */
    
    char format_name[64];       /**< Human-readable format name */
    char version_name[32];      /**< Human-readable version name */
    
    /** Detection details */
    uint16_t dir_entries_found; /**< Valid directory entries found */
    uint16_t deleted_entries;   /**< Deleted entries found */
    bool     has_dir_label;     /**< Directory label present */
    bool     has_date_stamps;   /**< Date stamps present */
    
    /** Geometry estimate */
    uft_cpm_dpb_t dpb;          /**< Best-match DPB */
    
} uft_cpm_detect_t;

/*===========================================================================
 * Filesystem Context
 *===========================================================================*/

/**
 * @brief CP/M filesystem context
 */
typedef struct {
    /** Image data */
    uint8_t *data;              /**< Disk image data */
    size_t   size;              /**< Image size in bytes */
    bool     owns_data;         /**< Whether context owns the data */
    bool     modified;          /**< Whether data has been modified */
    
    /** Disk parameters */
    uft_cpm_dpb_t dpb;          /**< Disk parameter block */
    
    /** Block allocation map */
    uint8_t *block_map;         /**< Block allocation bitmap */
    size_t   block_map_size;
    
    /** Directory cache */
    uft_cpm_dir_entry_t *dir_cache;  /**< Cached directory entries */
    size_t   dir_cache_size;
    bool     dir_dirty;         /**< Directory cache needs write */
    
    /** Statistics */
    uint16_t used_blocks;
    uint16_t free_blocks;
    uint16_t used_entries;
    uint16_t free_entries;
    
} uft_cpm_ctx_t;

/*===========================================================================
 * Error Codes
 *===========================================================================*/

/**
 * @brief CP/M filesystem error codes
 */
typedef enum {
    UFT_CPM_OK = 0,
    UFT_CPM_ERR_NULL,           /**< Null pointer */
    UFT_CPM_ERR_MEMORY,         /**< Memory allocation failed */
    UFT_CPM_ERR_IO,             /**< I/O error */
    UFT_CPM_ERR_FORMAT,         /**< Invalid format */
    UFT_CPM_ERR_NOT_CPM,        /**< Not a CP/M filesystem */
    UFT_CPM_ERR_NOT_FOUND,      /**< File not found */
    UFT_CPM_ERR_EXISTS,         /**< File already exists */
    UFT_CPM_ERR_DIR_FULL,       /**< Directory full */
    UFT_CPM_ERR_DISK_FULL,      /**< Disk full */
    UFT_CPM_ERR_READ_ONLY,      /**< File is read-only */
    UFT_CPM_ERR_INVALID_USER,   /**< Invalid user number */
    UFT_CPM_ERR_INVALID_NAME,   /**< Invalid filename */
    UFT_CPM_ERR_BAD_EXTENT,     /**< Corrupt extent chain */
    UFT_CPM_ERR_VERSION,        /**< Unsupported CP/M version */
} uft_cpm_err_t;

/*===========================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Create a CP/M filesystem context
 * @return New context or NULL on error
 */
uft_cpm_ctx_t *uft_cpm_create(void);

/**
 * @brief Destroy a CP/M filesystem context
 * @param ctx Context to destroy
 */
void uft_cpm_destroy(uft_cpm_ctx_t *ctx);

/**
 * @brief Open a CP/M disk image
 * @param ctx Context
 * @param data Image data
 * @param size Data size
 * @param copy Whether to copy data (true) or reference it (false)
 * @return Error code
 */
uft_cpm_err_t uft_cpm_open(uft_cpm_ctx_t *ctx, 
                           const uint8_t *data, size_t size,
                           bool copy);

/**
 * @brief Open with specific format
 * @param ctx Context
 * @param data Image data
 * @param size Data size
 * @param format Format to use
 * @param copy Whether to copy data
 * @return Error code
 */
uft_cpm_err_t uft_cpm_open_format(uft_cpm_ctx_t *ctx,
                                   const uint8_t *data, size_t size,
                                   uft_cpm_format_t format, bool copy);

/**
 * @brief Open with custom DPB
 * @param ctx Context
 * @param data Image data
 * @param size Data size
 * @param dpb Custom disk parameter block
 * @param copy Whether to copy data
 * @return Error code
 */
uft_cpm_err_t uft_cpm_open_dpb(uft_cpm_ctx_t *ctx,
                                const uint8_t *data, size_t size,
                                const uft_cpm_dpb_t *dpb, bool copy);

/**
 * @brief Close the image (flushes changes)
 * @param ctx Context
 * @return Error code
 */
uft_cpm_err_t uft_cpm_close(uft_cpm_ctx_t *ctx);

/**
 * @brief Save image to file
 * @param ctx Context
 * @param filename Output filename
 * @return Error code
 */
uft_cpm_err_t uft_cpm_save(uft_cpm_ctx_t *ctx, const char *filename);

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

/**
 * @brief Detect CP/M filesystem
 * @param data Image data
 * @param size Data size
 * @param result Detection result (output)
 * @return true if CP/M detected
 */
bool uft_cpm_detect(const uint8_t *data, size_t size,
                    uft_cpm_detect_t *result);

/**
 * @brief Try to detect format from image size
 * @param size Image size
 * @return Best-match format
 */
uft_cpm_format_t uft_cpm_detect_format_by_size(size_t size);

/**
 * @brief Get DPB for known format
 * @param format Format type
 * @param dpb Output DPB
 * @return true if format known
 */
bool uft_cpm_get_dpb(uft_cpm_format_t format, uft_cpm_dpb_t *dpb);

/**
 * @brief Get format name string
 * @param format Format type
 * @return Format name
 */
const char *uft_cpm_format_name(uft_cpm_format_t format);

/**
 * @brief Get version name string
 * @param version Version type
 * @return Version name
 */
const char *uft_cpm_version_name(uft_cpm_version_t version);

/*===========================================================================
 * Sector/Block Access
 *===========================================================================*/

/**
 * @brief Convert track/sector/side to image offset
 * @param ctx Context
 * @param track Track number
 * @param sector Sector number
 * @param side Side number (0 or 1)
 * @return Byte offset or -1 on error
 */
int32_t uft_cpm_sector_offset(const uft_cpm_ctx_t *ctx,
                               uint8_t track, uint8_t sector, uint8_t side);

/**
 * @brief Read a sector
 * @param ctx Context
 * @param track Track number
 * @param sector Sector number
 * @param side Side number
 * @param buffer Output buffer (must be sector_size bytes)
 * @return Error code
 */
uft_cpm_err_t uft_cpm_read_sector(const uft_cpm_ctx_t *ctx,
                                   uint8_t track, uint8_t sector, uint8_t side,
                                   uint8_t *buffer);

/**
 * @brief Write a sector
 * @param ctx Context
 * @param track Track number
 * @param sector Sector number
 * @param side Side number
 * @param buffer Input buffer (must be sector_size bytes)
 * @return Error code
 */
uft_cpm_err_t uft_cpm_write_sector(uft_cpm_ctx_t *ctx,
                                    uint8_t track, uint8_t sector, uint8_t side,
                                    const uint8_t *buffer);

/**
 * @brief Convert block number to sector list
 * @param ctx Context
 * @param block Block number
 * @param track_out Output track array
 * @param sector_out Output sector array
 * @param side_out Output side array
 * @param count_out Number of sectors in block
 * @return Error code
 */
uft_cpm_err_t uft_cpm_block_to_sectors(const uft_cpm_ctx_t *ctx,
                                        uint16_t block,
                                        uint8_t *track_out, uint8_t *sector_out,
                                        uint8_t *side_out, uint8_t *count_out);

/**
 * @brief Read a block
 * @param ctx Context
 * @param block Block number
 * @param buffer Output buffer (must be block_size bytes)
 * @return Error code
 */
uft_cpm_err_t uft_cpm_read_block(const uft_cpm_ctx_t *ctx,
                                  uint16_t block, uint8_t *buffer);

/**
 * @brief Write a block
 * @param ctx Context
 * @param block Block number
 * @param buffer Input buffer (must be block_size bytes)
 * @return Error code
 */
uft_cpm_err_t uft_cpm_write_block(uft_cpm_ctx_t *ctx,
                                   uint16_t block, const uint8_t *buffer);

/*===========================================================================
 * Directory Operations
 *===========================================================================*/

/**
 * @brief Read directory
 * @param ctx Context
 * @param dir Directory listing (output)
 * @param user User number filter (-1 for all)
 * @return Error code
 */
uft_cpm_err_t uft_cpm_read_dir(const uft_cpm_ctx_t *ctx,
                                uft_cpm_dir_t *dir, int user);

/**
 * @brief Free directory listing
 * @param dir Directory to free
 */
void uft_cpm_free_dir(uft_cpm_dir_t *dir);

/**
 * @brief Find file in directory
 * @param ctx Context
 * @param user User number
 * @param name Filename (with or without extension)
 * @param file Output file info
 * @return Error code (UFT_CPM_ERR_NOT_FOUND if not found)
 */
uft_cpm_err_t uft_cpm_find_file(const uft_cpm_ctx_t *ctx,
                                 uint8_t user, const char *name,
                                 uft_cpm_file_t *file);

/**
 * @brief Iterate over directory entries
 * @param ctx Context
 * @param callback Callback function (return false to stop)
 * @param userdata User data for callback
 * @param user User number filter (-1 for all)
 * @return Number of entries processed
 */
size_t uft_cpm_foreach_file(const uft_cpm_ctx_t *ctx,
                             bool (*callback)(const uft_cpm_file_t *file, void *userdata),
                             void *userdata, int user);

/*===========================================================================
 * File Operations
 *===========================================================================*/

/**
 * @brief Extract file to buffer
 * @param ctx Context
 * @param user User number
 * @param name Filename
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param bytes_read Actual bytes read (output)
 * @return Error code
 */
uft_cpm_err_t uft_cpm_extract(const uft_cpm_ctx_t *ctx,
                               uint8_t user, const char *name,
                               uint8_t *buffer, size_t buffer_size,
                               size_t *bytes_read);

/**
 * @brief Extract file to disk
 * @param ctx Context
 * @param user User number
 * @param name Source filename
 * @param dest_path Destination path
 * @return Error code
 */
uft_cpm_err_t uft_cpm_extract_file(const uft_cpm_ctx_t *ctx,
                                    uint8_t user, const char *name,
                                    const char *dest_path);

/**
 * @brief Inject file from buffer
 * @param ctx Context
 * @param user User number
 * @param name Filename
 * @param data File data
 * @param size Data size
 * @return Error code
 */
uft_cpm_err_t uft_cpm_inject(uft_cpm_ctx_t *ctx,
                              uint8_t user, const char *name,
                              const uint8_t *data, size_t size);

/**
 * @brief Inject file from disk
 * @param ctx Context
 * @param user User number
 * @param name Destination filename (NULL = use source name)
 * @param src_path Source path
 * @return Error code
 */
uft_cpm_err_t uft_cpm_inject_file(uft_cpm_ctx_t *ctx,
                                   uint8_t user, const char *name,
                                   const char *src_path);

/**
 * @brief Delete file
 * @param ctx Context
 * @param user User number
 * @param name Filename
 * @return Error code
 */
uft_cpm_err_t uft_cpm_delete(uft_cpm_ctx_t *ctx,
                              uint8_t user, const char *name);

/**
 * @brief Rename file
 * @param ctx Context
 * @param user User number
 * @param old_name Current filename
 * @param new_name New filename
 * @return Error code
 */
uft_cpm_err_t uft_cpm_rename(uft_cpm_ctx_t *ctx,
                              uint8_t user, const char *old_name,
                              const char *new_name);

/**
 * @brief Change file user number
 * @param ctx Context
 * @param old_user Current user
 * @param name Filename
 * @param new_user New user number
 * @return Error code
 */
uft_cpm_err_t uft_cpm_change_user(uft_cpm_ctx_t *ctx,
                                   uint8_t old_user, const char *name,
                                   uint8_t new_user);

/**
 * @brief Set file attributes
 * @param ctx Context
 * @param user User number
 * @param name Filename
 * @param attrib Attributes to set
 * @return Error code
 */
uft_cpm_err_t uft_cpm_set_attrib(uft_cpm_ctx_t *ctx,
                                  uint8_t user, const char *name,
                                  const uft_cpm_attrib_t *attrib);

/**
 * @brief Get file attributes
 * @param ctx Context
 * @param user User number
 * @param name Filename
 * @param attrib Attributes (output)
 * @return Error code
 */
uft_cpm_err_t uft_cpm_get_attrib(const uft_cpm_ctx_t *ctx,
                                  uint8_t user, const char *name,
                                  uft_cpm_attrib_t *attrib);

/*===========================================================================
 * Block Allocation
 *===========================================================================*/

/**
 * @brief Check if block is allocated
 * @param ctx Context
 * @param block Block number
 * @return true if allocated
 */
bool uft_cpm_block_used(const uft_cpm_ctx_t *ctx, uint16_t block);

/**
 * @brief Allocate a free block
 * @param ctx Context
 * @return Block number or 0xFFFF if full
 */
uint16_t uft_cpm_alloc_block(uft_cpm_ctx_t *ctx);

/**
 * @brief Free a block
 * @param ctx Context
 * @param block Block number
 */
void uft_cpm_free_block(uft_cpm_ctx_t *ctx, uint16_t block);

/**
 * @brief Get free block count
 * @param ctx Context
 * @return Number of free blocks
 */
uint16_t uft_cpm_free_blocks(const uft_cpm_ctx_t *ctx);

/**
 * @brief Get free bytes
 * @param ctx Context
 * @return Free space in bytes
 */
uint32_t uft_cpm_free_bytes(const uft_cpm_ctx_t *ctx);

/*===========================================================================
 * Directory Entry Operations
 *===========================================================================*/

/**
 * @brief Find free directory entry
 * @param ctx Context
 * @return Entry index or -1 if full
 */
int16_t uft_cpm_find_free_entry(uft_cpm_ctx_t *ctx);

/**
 * @brief Allocate directory entry
 * @param ctx Context
 * @param user User number
 * @param name Filename
 * @param ext Extension
 * @return Entry index or -1 on error
 */
int16_t uft_cpm_alloc_entry(uft_cpm_ctx_t *ctx,
                             uint8_t user, const char *name, const char *ext);

/**
 * @brief Get raw directory entry
 * @param ctx Context
 * @param index Entry index
 * @return Entry pointer or NULL
 */
uft_cpm_dir_entry_t *uft_cpm_get_entry(uft_cpm_ctx_t *ctx, uint16_t index);

/*===========================================================================
 * Image Creation
 *===========================================================================*/

/**
 * @brief Create blank CP/M disk image
 * @param format Format to create
 * @param data Output data pointer
 * @param size Output size
 * @return Error code
 */
uft_cpm_err_t uft_cpm_create_image(uft_cpm_format_t format,
                                    uint8_t **data, size_t *size);

/**
 * @brief Create image with custom DPB
 * @param dpb Disk parameter block
 * @param data Output data pointer
 * @param size Output size
 * @return Error code
 */
uft_cpm_err_t uft_cpm_create_image_dpb(const uft_cpm_dpb_t *dpb,
                                        uint8_t **data, size_t *size);

/**
 * @brief Format existing image
 * @param ctx Context
 * @return Error code
 */
uft_cpm_err_t uft_cpm_format(uft_cpm_ctx_t *ctx);

/*===========================================================================
 * Utilities
 *===========================================================================*/

/**
 * @brief Parse CP/M filename
 * @param input Full filename (NAME.EXT)
 * @param name Output name (8 chars, space-padded)
 * @param ext Output extension (3 chars, space-padded)
 * @return true if valid
 */
bool uft_cpm_parse_filename(const char *input, char *name, char *ext);

/**
 * @brief Format CP/M filename for display
 * @param entry Directory entry
 * @param buffer Output buffer (at least 16 bytes)
 */
void uft_cpm_format_filename(const uft_cpm_dir_entry_t *entry, char *buffer);

/**
 * @brief Validate filename
 * @param name Filename to validate
 * @return true if valid CP/M filename
 */
bool uft_cpm_valid_filename(const char *name);

/**
 * @brief Convert CP/M date to Unix time
 * @param cpm_date Days since 1/1/1978
 * @param hour Hour (BCD)
 * @param minute Minute (BCD)
 * @return Unix timestamp
 */
time_t uft_cpm_to_unix_time(uint16_t cpm_date, uint8_t hour, uint8_t minute);

/**
 * @brief Convert Unix time to CP/M date
 * @param unix_time Unix timestamp
 * @param cpm_date Output days since 1/1/1978
 * @param hour Output hour (BCD)
 * @param minute Output minute (BCD)
 */
void uft_cpm_from_unix_time(time_t unix_time, uint16_t *cpm_date,
                             uint8_t *hour, uint8_t *minute);

/**
 * @brief Print directory listing
 * @param ctx Context
 * @param user User number filter (-1 for all)
 */
void uft_cpm_print_dir(const uft_cpm_ctx_t *ctx, int user);

/**
 * @brief Print disk info
 * @param ctx Context
 */
void uft_cpm_print_info(const uft_cpm_ctx_t *ctx);

/**
 * @brief Export directory as JSON
 * @param ctx Context
 * @param user User number filter (-1 for all)
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes written or -1 on error
 */
int uft_cpm_to_json(const uft_cpm_ctx_t *ctx, int user,
                    char *buffer, size_t size);

/**
 * @brief Get error message
 * @param err Error code
 * @return Error message
 */
const char *uft_cpm_strerror(uft_cpm_err_t err);

/*===========================================================================
 * Advanced: Deleted File Recovery
 *===========================================================================*/

/**
 * @brief List deleted files
 * @param ctx Context
 * @param dir Directory listing (output)
 * @return Error code
 */
uft_cpm_err_t uft_cpm_list_deleted(const uft_cpm_ctx_t *ctx,
                                    uft_cpm_dir_t *dir);

/**
 * @brief Attempt to recover deleted file
 * @param ctx Context
 * @param index Deleted entry index
 * @param user New user number
 * @return Error code
 */
uft_cpm_err_t uft_cpm_recover_deleted(uft_cpm_ctx_t *ctx,
                                       uint16_t index, uint8_t user);

/*===========================================================================
 * Advanced: Disk Analysis
 *===========================================================================*/

/**
 * @brief Validate disk structure
 * @param ctx Context
 * @param fix Attempt to fix errors
 * @param report Output report buffer
 * @param report_size Report buffer size
 * @return Number of errors found
 */
int uft_cpm_validate(uft_cpm_ctx_t *ctx, bool fix,
                     char *report, size_t report_size);

/**
 * @brief Check for cross-linked files
 * @param ctx Context
 * @return Number of cross-links found
 */
int uft_cpm_check_crosslinks(const uft_cpm_ctx_t *ctx);

/**
 * @brief Rebuild block allocation map
 * @param ctx Context
 */
void uft_cpm_rebuild_allocation(uft_cpm_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CPM_FS_H */
