/**
 * @file uft_atari_dos.h
 * @brief Atari DOS 2.x/MyDOS/SpartaDOS Filesystem Layer
 * 
 * Complete filesystem support for Atari 8-bit disk formats:
 * - Atari DOS 1.0, 2.0S, 2.5
 * - MyDOS 4.5x
 * - SpartaDOS (basic support)
 * - DOS XE
 * 
 * Disk formats:
 * - Single density (SD): 40 tracks, 18 sectors, 128 bytes = 90KB
 * - Enhanced density (ED): 40 tracks, 26 sectors, 128 bytes = 130KB
 * - Double density (DD): 40 tracks, 18 sectors, 256 bytes = 180KB
 * - Quad density (QD): 80 tracks, 18 sectors, 256 bytes = 360KB
 * - High density (HD): Various MyDOS formats up to 16MB
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PARTIAL
 * PARTIALLY IMPLEMENTED — Filesystem layer
 *
 * This header declares 42 public functions; 38 are NOT implemented
 * in the source tree (only 4 have a definition). Callers exist
 * for some of the unimplemented prototypes, so this file is a live hazard:
 * compile passes but link may fail depending on call pattern.
 *
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 IMPLEMENT-Welle).
 * Decision per function: IMPLEMENT (finish it), or DELETE prototype + all
 * call sites. Do NOT add new call sites until each prototype is resolved.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_FS_UFT_ATARI_DOS_H
#define UFT_FS_UFT_ATARI_DOS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "uft/uft_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Sector sizes */
#define UFT_ATARI_SECTOR_SD         128     /**< Single density sector */
#define UFT_ATARI_SECTOR_DD         256     /**< Double density sector */

/** Standard disk parameters */
#define UFT_ATARI_TRACKS_40         40      /**< Standard tracks */
#define UFT_ATARI_TRACKS_80         80      /**< Enhanced tracks */

/** Sectors per track */
#define UFT_ATARI_SPT_SD            18      /**< Single density */
#define UFT_ATARI_SPT_ED            26      /**< Enhanced density */
#define UFT_ATARI_SPT_DD            18      /**< Double density */

/** Standard disk sizes */
#define UFT_ATARI_SIZE_SD           92160   /**< 720 sectors × 128 bytes */
#define UFT_ATARI_SIZE_ED           133120  /**< 1040 sectors × 128 bytes */
#define UFT_ATARI_SIZE_DD           184320  /**< 720 sectors × 256 bytes */
#define UFT_ATARI_SIZE_QD           368640  /**< 1440 sectors × 256 bytes */

/** VTOC/Directory locations */
#define UFT_ATARI_VTOC_SECTOR       360     /**< DOS 2 VTOC sector */
#define UFT_ATARI_DIR_START         361     /**< First directory sector */
#define UFT_ATARI_DIR_SECTORS       8       /**< Directory sectors (361-368) */
#define UFT_ATARI_MAX_FILES         64      /**< Maximum files in directory */

/** File entry constants */
#define UFT_ATARI_ENTRY_SIZE        16      /**< Bytes per directory entry */
#define UFT_ATARI_FILENAME_LEN      8       /**< Filename length */
#define UFT_ATARI_EXTENSION_LEN     3       /**< Extension length */

/** Boot sector location */
#define UFT_ATARI_BOOT_SECTORS      3       /**< Sectors 1-3 are boot */

/*===========================================================================
 * DOS Types
 *===========================================================================*/

/**
 * @brief Atari DOS variants
 */
typedef enum {
    UFT_ATARI_DOS_UNKNOWN = 0,
    UFT_ATARI_DOS_1,           /**< Atari DOS 1.0 */
    UFT_ATARI_DOS_2S,          /**< Atari DOS 2.0S (single density) */
    UFT_ATARI_DOS_2D,          /**< Atari DOS 2.0D (double density) */
    UFT_ATARI_DOS_25,          /**< Atari DOS 2.5 (enhanced density) */
    UFT_ATARI_MYDOS,           /**< MyDOS 4.5x */
    UFT_ATARI_SPARTADOS,       /**< SpartaDOS */
    UFT_ATARI_DOSXE,           /**< DOS XE */
    UFT_ATARI_DOS_COUNT
} uft_atari_dos_type_t;

/**
 * @brief Disk density types
 */
typedef enum {
    UFT_ATARI_DENSITY_SD = 0,  /**< Single density (128 bytes/sector) */
    UFT_ATARI_DENSITY_ED,      /**< Enhanced density (26 spt, 128 bytes) */
    UFT_ATARI_DENSITY_DD,      /**< Double density (256 bytes/sector) */
    UFT_ATARI_DENSITY_QD,      /**< Quad density (80 tracks) */
    UFT_ATARI_DENSITY_HD,      /**< High density (MyDOS extended) */
    UFT_ATARI_DENSITY_COUNT
} uft_atari_density_t;

/**
 * @brief Disk geometry structure
 */
typedef struct {
    uint8_t  tracks;           /**< Number of tracks */
    uint8_t  sides;            /**< Number of sides (1 or 2) */
    uint8_t  sectors_per_track;/**< Sectors per track */
    uint16_t sector_size;      /**< Bytes per sector */
    uint16_t total_sectors;    /**< Total sectors */
    uint32_t total_bytes;      /**< Total capacity */
    uint16_t vtoc_sector;      /**< VTOC location */
    uint16_t dir_start;        /**< First directory sector */
    uint8_t  dir_sectors;      /**< Number of directory sectors */
    uft_atari_density_t density;/**< Density type */
} uft_atari_geometry_t;

/*===========================================================================
 * VTOC Structure (Volume Table of Contents)
 *===========================================================================*/

/**
 * @brief DOS 2.0 VTOC header (first bytes of sector 360)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  dos_code;         /**< DOS code (0 = DOS 2) */
    uint16_t total_sectors;    /**< Total sectors (little-endian) */
    uint16_t free_sectors;     /**< Free sectors (little-endian) */
    uint8_t  reserved[5];      /**< Reserved */
    uint8_t  bitmap[90];       /**< Sector allocation bitmap */
} uft_atari_vtoc_t;
UFT_PACK_END

/**
 * @brief MyDOS extended VTOC
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  dos_code;         /**< DOS code (2 = MyDOS) */
    uint16_t total_sectors;    /**< Total sectors */
    uint16_t free_sectors;     /**< Free sectors */
    uint8_t  reserved[5];      /**< Reserved */
    uint8_t  bitmap[118];      /**< Extended bitmap for MyDOS */
    uint16_t vtoc2_sector;     /**< Second VTOC sector (for large disks) */
} uft_atari_mydos_vtoc_t;
UFT_PACK_END

/*===========================================================================
 * Directory Entry Structure
 *===========================================================================*/

/**
 * @brief File status flags
 */
typedef enum {
    UFT_ATARI_FLAG_OPEN     = 0x01,  /**< File is open for write */
    UFT_ATARI_FLAG_DOS2     = 0x02,  /**< Created by DOS 2 */
    UFT_ATARI_FLAG_MYDOS    = 0x04,  /**< MyDOS extended */
    UFT_ATARI_FLAG_LOCKED   = 0x20,  /**< File is locked */
    UFT_ATARI_FLAG_INUSE    = 0x40,  /**< Entry in use */
    UFT_ATARI_FLAG_DELETED  = 0x80   /**< Entry deleted */
} uft_atari_file_flags_t;

/**
 * @brief Directory entry (16 bytes, on-disk format)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  flags;            /**< File flags */
    uint16_t sector_count;     /**< Number of sectors (little-endian) */
    uint16_t start_sector;     /**< First sector (little-endian) */
    char     filename[8];      /**< Filename (space-padded) */
    char     extension[3];     /**< Extension (space-padded) */
} uft_atari_dir_entry_raw_t;
UFT_PACK_END

/**
 * @brief Unified file entry (internal representation)
 */
typedef struct {
    char     filename[9];      /**< Filename (null-terminated) */
    char     extension[4];     /**< Extension (null-terminated) */
    char     full_name[13];    /**< Full name: NAME.EXT */
    uint8_t  flags;            /**< Original flags byte */
    bool     in_use;           /**< Entry is valid file */
    bool     deleted;          /**< Entry was deleted */
    bool     locked;           /**< File is locked */
    bool     open_for_write;   /**< File open for write */
    uint16_t start_sector;     /**< First sector number */
    uint16_t sector_count;     /**< Number of sectors used */
    uint32_t file_size;        /**< Actual file size (from sector chain) */
    uint8_t  dir_index;        /**< Index in directory */
} uft_atari_entry_t;

/**
 * @brief Directory listing
 */
typedef struct {
    size_t            file_count;      /**< Number of valid files */
    size_t            deleted_count;   /**< Number of deleted entries */
    uft_atari_entry_t files[UFT_ATARI_MAX_FILES]; /**< File entries */
    uint16_t          total_sectors;   /**< Total disk sectors */
    uint16_t          free_sectors;    /**< Free sectors */
    uint32_t          free_bytes;      /**< Free space in bytes */
} uft_atari_dir_t;

/*===========================================================================
 * Sector Link Structure
 *===========================================================================*/

/**
 * @brief Sector link bytes (last 3 bytes of each sector in DOS 2.x)
 * 
 * Format:
 * - Byte 0: File number (bits 0-5) + high bits of next sector (bits 6-7)
 * - Byte 1: Low byte of next sector (0 = last sector)
 * - Byte 2: Bytes used in sector (125 max for SD, 253 max for DD)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  file_id_hi;       /**< File ID (0-62) + next sector high bits */
    uint8_t  next_lo;          /**< Next sector low byte */
    uint8_t  bytes_used;       /**< Data bytes in this sector */
} uft_atari_sector_link_t;
UFT_PACK_END

/** Extract file ID from link */
#define UFT_ATARI_LINK_FILE_ID(link) ((link)->file_id_hi & 0x3F)

/** Extract next sector from link */
#define UFT_ATARI_LINK_NEXT(link) \
    (((uint16_t)((link)->file_id_hi & 0xC0) << 2) | (link)->next_lo)

/** Build link byte 0 */
#define UFT_ATARI_MAKE_LINK0(file_id, next_hi) \
    (((file_id) & 0x3F) | (((next_hi) >> 2) & 0xC0))

/*===========================================================================
 * Detection Result
 *===========================================================================*/

/**
 * @brief Filesystem detection result
 */
typedef struct {
    uft_atari_dos_type_t  dos_type;    /**< Detected DOS type */
    uft_atari_density_t   density;     /**< Detected density */
    uft_atari_geometry_t  geometry;    /**< Disk geometry */
    uint8_t               confidence;  /**< Detection confidence 0-100 */
    char                  description[64]; /**< Human-readable description */
    bool                  has_boot;    /**< Boot sectors present */
    bool                  has_vtoc;    /**< Valid VTOC found */
} uft_atari_detect_t;

/*===========================================================================
 * Filesystem Context
 *===========================================================================*/

/**
 * @brief Atari DOS filesystem context
 */
typedef struct {
    /* Image data */
    uint8_t             *data;         /**< Raw image data */
    size_t               data_size;    /**< Image size in bytes */
    bool                 owns_data;    /**< True if we allocated data */
    bool                 modified;     /**< Image has been modified */
    
    /* Filesystem info */
    uft_atari_dos_type_t dos_type;     /**< DOS type */
    uft_atari_geometry_t geometry;     /**< Disk geometry */
    
    /* VTOC cache */
    uint8_t              vtoc[256];    /**< VTOC sector cache */
    bool                 vtoc_valid;   /**< VTOC cache valid */
    uint16_t             total_sectors;/**< Total sectors */
    uint16_t             free_sectors; /**< Free sectors */
    
    /* Error tracking */
    char                 last_error[256];
} uft_atari_ctx_t;

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    UFT_ATARI_OK = 0,
    
    /* Parameter errors */
    UFT_ATARI_ERR_PARAM,            /**< Invalid parameter / NULL pointer */
    UFT_ATARI_ERR_MEMORY,           /**< Memory allocation failed */
    
    /* Format/Detection errors */
    UFT_ATARI_ERR_FORMAT,           /**< Invalid format / not Atari image */
    UFT_ATARI_ERR_NOT_ATR,          /**< Not an ATR file */
    
    /* I/O errors */
    UFT_ATARI_ERR_READ,             /**< Read error */
    UFT_ATARI_ERR_WRITE,            /**< Write error */
    UFT_ATARI_ERR_SECTOR,           /**< Sector out of range */
    
    /* Filesystem errors */
    UFT_ATARI_ERR_VTOC,             /**< VTOC corrupt or unreadable */
    UFT_ATARI_ERR_NOTFOUND,         /**< File not found */
    UFT_ATARI_ERR_EXISTS,           /**< File already exists */
    UFT_ATARI_ERR_FULL,             /**< Disk full */
    UFT_ATARI_ERR_DIRFULL,          /**< Directory full */
    UFT_ATARI_ERR_LOCKED,           /**< File is locked */
    UFT_ATARI_ERR_CORRUPT,          /**< Data corruption detected */
    UFT_ATARI_ERR_CHAIN,            /**< Bad sector chain */
    
    /* State errors */
    UFT_ATARI_ERR_NOT_OPEN,         /**< Context not open */
    UFT_ATARI_ERR_READONLY,         /**< Read-only image */
    
    UFT_ATARI_ERR_COUNT
} uft_atari_error_t;

/*===========================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Create new Atari DOS context
 * @return New context or NULL on failure
 */
uft_atari_ctx_t *uft_atari_create(void);







/*===========================================================================
 * Detection Functions
 *===========================================================================*/

/**
 * @brief Detect Atari DOS filesystem type
 * @param data Image data
 * @param size Data size
 * @param[out] result Detection result
 * @return Error code
 */
uft_atari_error_t uft_atari_detect(const uint8_t *data,
                                   size_t size,
                                   uft_atari_detect_t *result);


/**
 * @brief Get geometry for density type
 * @param density Density type
 * @param[out] geom Geometry structure
 * @return Error code
 */
uft_atari_error_t uft_atari_get_geometry(uft_atari_density_t density,
                                         uft_atari_geometry_t *geom);

/*===========================================================================
 * Sector I/O
 *===========================================================================*/

/**
 * @brief Read sector (1-based sector number, Atari convention)
 * @param ctx Context
 * @param sector Sector number (1 to total_sectors)
 * @param[out] buffer Output buffer
 * @return Error code
 */
uft_atari_error_t uft_atari_read_sector(const uft_atari_ctx_t *ctx,
                                        uint16_t sector,
                                        uint8_t *buffer);

/**
 * @brief Write sector
 * @param ctx Context
 * @param sector Sector number
 * @param buffer Data to write
 * @return Error code
 */
uft_atari_error_t uft_atari_write_sector(uft_atari_ctx_t *ctx,
                                         uint16_t sector,
                                         const uint8_t *buffer);

/*===========================================================================
 * VTOC Operations
 *===========================================================================*/

/**
 * @brief Read and cache VTOC
 * @param ctx Context
 * @return Error code
 */
uft_atari_error_t uft_atari_read_vtoc(uft_atari_ctx_t *ctx);






/*===========================================================================
 * Directory Operations
 *===========================================================================*/


/**
 * @brief Find file in directory
 * @param ctx Context
 * @param filename Filename (with or without extension)
 * @param[out] entry Entry structure
 * @return Error code
 */
uft_atari_error_t uft_atari_find_file(uft_atari_ctx_t *ctx,
                                      const char *filename,
                                      uft_atari_entry_t *entry);

/**
 * @brief Iterate over directory entries
 * @param ctx Context
 * @param callback Callback function (return false to stop)
 * @param user_data User data for callback
 * @return Error code
 */
typedef bool (*uft_atari_foreach_cb)(const uft_atari_entry_t *entry, void *user_data);

/*===========================================================================
 * File Operations
 *===========================================================================*/








/*===========================================================================
 * Image Creation
 *===========================================================================*/


/**
 * @brief Format existing image (clears all data)
 * @param ctx Context
 * @return Error code
 */
uft_atari_error_t uft_atari_format(uft_atari_ctx_t *ctx);

/*===========================================================================
 * Validation & Repair
 *===========================================================================*/

/**
 * @brief Validation result
 */
typedef struct {
    bool     valid;            /**< Overall valid */
    bool     vtoc_ok;          /**< VTOC valid */
    bool     directory_ok;     /**< Directory valid */
    bool     chains_ok;        /**< All file chains valid */
    uint32_t errors;           /**< Error count */
    uint32_t warnings;         /**< Warning count */
    uint16_t orphan_sectors;   /**< Sectors not in VTOC or files */
    uint16_t cross_linked;     /**< Cross-linked sectors */
    char     report[1024];     /**< Detailed report */
} uft_atari_val_result_t;





/*===========================================================================
 * Utility Functions
 *===========================================================================*/




/**
 * @brief Get DOS type name
 * @param type DOS type
 * @return Type name string
 */
const char *uft_atari_dos_name(uft_atari_dos_type_t type);

/**
 * @brief Get density name
 * @param density Density type
 * @return Density name string
 */
const char *uft_atari_density_name(uft_atari_density_t density);

/**
 * @brief Get error message
 * @param error Error code
 * @return Error message string
 */
const char *uft_atari_error_string(uft_atari_error_t error);




/*===========================================================================
 * ATR Header Support
 *===========================================================================*/

/**
 * @brief ATR file header (16 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint16_t magic;            /**< 0x0296 = NICKATARI */
    uint16_t paragraphs;       /**< Image size in 16-byte paragraphs (lo) */
    uint16_t sector_size;      /**< Sector size (128 or 256) */
    uint8_t  paragraphs_hi;    /**< High byte of paragraphs */
    uint32_t crc;              /**< Optional CRC */
    uint32_t reserved;         /**< Reserved */
    uint8_t  flags;            /**< Flags (bit 0 = write protect) */
} uft_atari_atr_header_t;
UFT_PACK_END

#define UFT_ATARI_ATR_MAGIC     0x0296




#ifdef __cplusplus
}
#endif

#endif /* UFT_FS_UFT_ATARI_DOS_H */
