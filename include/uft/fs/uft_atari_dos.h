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

#ifndef UFT_ATARI_DOS_H
#define UFT_ATARI_DOS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

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
typedef struct __attribute__((packed)) {
    uint8_t  dos_code;         /**< DOS code (0 = DOS 2) */
    uint16_t total_sectors;    /**< Total sectors (little-endian) */
    uint16_t free_sectors;     /**< Free sectors (little-endian) */
    uint8_t  reserved[5];      /**< Reserved */
    uint8_t  bitmap[90];       /**< Sector allocation bitmap */
} uft_atari_vtoc_t;

/**
 * @brief MyDOS extended VTOC
 */
typedef struct __attribute__((packed)) {
    uint8_t  dos_code;         /**< DOS code (2 = MyDOS) */
    uint16_t total_sectors;    /**< Total sectors */
    uint16_t free_sectors;     /**< Free sectors */
    uint8_t  reserved[5];      /**< Reserved */
    uint8_t  bitmap[118];      /**< Extended bitmap for MyDOS */
    uint16_t vtoc2_sector;     /**< Second VTOC sector (for large disks) */
} uft_atari_mydos_vtoc_t;

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
typedef struct __attribute__((packed)) {
    uint8_t  flags;            /**< File flags */
    uint16_t sector_count;     /**< Number of sectors (little-endian) */
    uint16_t start_sector;     /**< First sector (little-endian) */
    char     filename[8];      /**< Filename (space-padded) */
    char     extension[3];     /**< Extension (space-padded) */
} uft_atari_dir_entry_raw_t;

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
typedef struct __attribute__((packed)) {
    uint8_t  file_id_hi;       /**< File ID (0-62) + next sector high bits */
    uint8_t  next_lo;          /**< Next sector low byte */
    uint8_t  bytes_used;       /**< Data bytes in this sector */
} uft_atari_sector_link_t;

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

/**
 * @brief Destroy Atari DOS context
 * @param ctx Context to destroy
 */
void uft_atari_destroy(uft_atari_ctx_t *ctx);

/**
 * @brief Open Atari disk image
 * @param ctx Context
 * @param data Image data
 * @param size Data size
 * @param copy_data If true, copy data; if false, reference only
 * @return Error code
 */
uft_atari_error_t uft_atari_open(uft_atari_ctx_t *ctx,
                                 const uint8_t *data,
                                 size_t size,
                                 bool copy_data);

/**
 * @brief Open with explicit geometry
 * @param ctx Context
 * @param data Image data
 * @param size Data size
 * @param density Density type
 * @param dos_type DOS type
 * @return Error code
 */
uft_atari_error_t uft_atari_open_as(uft_atari_ctx_t *ctx,
                                    const uint8_t *data,
                                    size_t size,
                                    uft_atari_density_t density,
                                    uft_atari_dos_type_t dos_type);

/**
 * @brief Close image (keeps context for reuse)
 * @param ctx Context
 */
void uft_atari_close(uft_atari_ctx_t *ctx);

/**
 * @brief Save modified image to file
 * @param ctx Context
 * @param path Output file path
 * @return Error code
 */
uft_atari_error_t uft_atari_save(uft_atari_ctx_t *ctx, const char *path);

/**
 * @brief Get image data for external saving
 * @param ctx Context
 * @param[out] data Pointer to data
 * @param[out] size Data size
 * @return Error code
 */
uft_atari_error_t uft_atari_get_data(const uft_atari_ctx_t *ctx,
                                     const uint8_t **data,
                                     size_t *size);

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
 * @brief Check if data is an Atari disk image
 * @param data Image data
 * @param size Data size
 * @return true if Atari format detected
 */
bool uft_atari_is_atari_image(const uint8_t *data, size_t size);

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

/**
 * @brief Write VTOC back to disk
 * @param ctx Context
 * @return Error code
 */
uft_atari_error_t uft_atari_write_vtoc(uft_atari_ctx_t *ctx);

/**
 * @brief Check if sector is allocated
 * @param ctx Context
 * @param sector Sector number
 * @return true if allocated, false if free
 */
bool uft_atari_is_sector_allocated(const uft_atari_ctx_t *ctx, uint16_t sector);

/**
 * @brief Allocate a free sector
 * @param ctx Context
 * @return Sector number, or 0 if disk full
 */
uint16_t uft_atari_allocate_sector(uft_atari_ctx_t *ctx);

/**
 * @brief Free a sector
 * @param ctx Context
 * @param sector Sector number to free
 * @return Error code
 */
uft_atari_error_t uft_atari_free_sector(uft_atari_ctx_t *ctx, uint16_t sector);

/**
 * @brief Get free space info
 * @param ctx Context
 * @param[out] free_sectors Number of free sectors
 * @param[out] free_bytes Free space in bytes
 * @return Error code
 */
uft_atari_error_t uft_atari_free_space(const uft_atari_ctx_t *ctx,
                                       uint16_t *free_sectors,
                                       uint32_t *free_bytes);

/*===========================================================================
 * Directory Operations
 *===========================================================================*/

/**
 * @brief Read directory
 * @param ctx Context
 * @param[out] dir Directory structure
 * @return Error code
 */
uft_atari_error_t uft_atari_read_directory(uft_atari_ctx_t *ctx,
                                           uft_atari_dir_t *dir);

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
uft_atari_error_t uft_atari_foreach(uft_atari_ctx_t *ctx,
                                    uft_atari_foreach_cb callback,
                                    void *user_data);

/*===========================================================================
 * File Operations
 *===========================================================================*/

/**
 * @brief Extract file from Atari image
 * @param ctx Context
 * @param filename Filename
 * @param[out] buffer Output buffer (caller must free)
 * @param[out] size File size
 * @return Error code
 */
uft_atari_error_t uft_atari_extract(uft_atari_ctx_t *ctx,
                                    const char *filename,
                                    uint8_t **buffer,
                                    size_t *size);

/**
 * @brief Extract file to host filesystem
 * @param ctx Context
 * @param filename Atari filename
 * @param output_path Output file path
 * @return Error code
 */
uft_atari_error_t uft_atari_extract_to_file(uft_atari_ctx_t *ctx,
                                            const char *filename,
                                            const char *output_path);

/**
 * @brief Inject file into Atari image
 * @param ctx Context
 * @param filename Atari filename (8.3 format)
 * @param data File data
 * @param size File size
 * @return Error code
 */
uft_atari_error_t uft_atari_inject(uft_atari_ctx_t *ctx,
                                   const char *filename,
                                   const uint8_t *data,
                                   size_t size);

/**
 * @brief Inject file from host filesystem
 * @param ctx Context
 * @param input_path Host file path
 * @param filename Atari filename (NULL to derive from path)
 * @return Error code
 */
uft_atari_error_t uft_atari_inject_from_file(uft_atari_ctx_t *ctx,
                                             const char *input_path,
                                             const char *filename);

/**
 * @brief Delete file from Atari image
 * @param ctx Context
 * @param filename Filename to delete
 * @return Error code
 */
uft_atari_error_t uft_atari_delete(uft_atari_ctx_t *ctx, const char *filename);

/**
 * @brief Rename file in Atari image
 * @param ctx Context
 * @param old_name Current filename
 * @param new_name New filename
 * @return Error code
 */
uft_atari_error_t uft_atari_rename(uft_atari_ctx_t *ctx,
                                   const char *old_name,
                                   const char *new_name);

/**
 * @brief Lock/unlock file
 * @param ctx Context
 * @param filename Filename
 * @param locked True to lock, false to unlock
 * @return Error code
 */
uft_atari_error_t uft_atari_set_locked(uft_atari_ctx_t *ctx,
                                       const char *filename,
                                       bool locked);

/*===========================================================================
 * Image Creation
 *===========================================================================*/

/**
 * @brief Create new blank Atari disk image
 * @param ctx Context
 * @param density Disk density
 * @param dos_type DOS type
 * @return Error code
 */
uft_atari_error_t uft_atari_create_image(uft_atari_ctx_t *ctx,
                                         uft_atari_density_t density,
                                         uft_atari_dos_type_t dos_type);

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

/**
 * @brief Validate disk image
 * @param ctx Context
 * @param[out] result Validation result
 * @return Error code
 */
uft_atari_error_t uft_atari_validate(uft_atari_ctx_t *ctx,
                                     uft_atari_val_result_t *result);

/**
 * @brief Rebuild VTOC from directory and file chains
 * @param ctx Context
 * @return Error code
 */
uft_atari_error_t uft_atari_rebuild_vtoc(uft_atari_ctx_t *ctx);

/**
 * @brief List deleted files (potentially recoverable)
 * @param ctx Context
 * @param[out] entries Deleted file entries
 * @param[out] count Number of entries found
 * @return Error code
 */
uft_atari_error_t uft_atari_list_deleted(uft_atari_ctx_t *ctx,
                                         uft_atari_entry_t **entries,
                                         size_t *count);

/**
 * @brief Attempt to recover deleted file
 * @param ctx Context
 * @param dir_index Directory index of deleted file
 * @param[out] buffer Recovered data
 * @param[out] size Data size
 * @return Error code
 */
uft_atari_error_t uft_atari_recover_deleted(uft_atari_ctx_t *ctx,
                                            uint8_t dir_index,
                                            uint8_t **buffer,
                                            size_t *size);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Parse Atari filename
 * @param input Input string (e.g., "FILE.EXT" or "FILE")
 * @param[out] filename 8-char filename buffer
 * @param[out] extension 3-char extension buffer
 * @return Error code
 */
uft_atari_error_t uft_atari_parse_filename(const char *input,
                                           char *filename,
                                           char *extension);

/**
 * @brief Format Atari filename for display
 * @param filename 8-char filename
 * @param extension 3-char extension
 * @param[out] buffer Output buffer (13 bytes min)
 */
void uft_atari_format_filename(const char *filename,
                               const char *extension,
                               char *buffer);

/**
 * @brief Validate Atari filename
 * @param filename Filename to validate
 * @return true if valid
 */
bool uft_atari_valid_filename(const char *filename);

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

/**
 * @brief Print directory listing to stdout
 * @param ctx Context
 * @param output Output file (NULL for stdout)
 */
void uft_atari_print_directory(uft_atari_ctx_t *ctx, FILE *output);

/**
 * @brief Print disk info
 * @param ctx Context
 * @param output Output file (NULL for stdout)
 */
void uft_atari_print_info(uft_atari_ctx_t *ctx, FILE *output);

/**
 * @brief Export directory to JSON
 * @param ctx Context
 * @param[out] buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or negative error
 */
int uft_atari_directory_to_json(uft_atari_ctx_t *ctx,
                                char *buffer,
                                size_t buffer_size);

/*===========================================================================
 * ATR Header Support
 *===========================================================================*/

/**
 * @brief ATR file header (16 bytes)
 */
typedef struct __attribute__((packed)) {
    uint16_t magic;            /**< 0x0296 = NICKATARI */
    uint16_t paragraphs;       /**< Image size in 16-byte paragraphs (lo) */
    uint16_t sector_size;      /**< Sector size (128 or 256) */
    uint8_t  paragraphs_hi;    /**< High byte of paragraphs */
    uint32_t crc;              /**< Optional CRC */
    uint32_t reserved;         /**< Reserved */
    uint8_t  flags;            /**< Flags (bit 0 = write protect) */
} uft_atari_atr_header_t;

#define UFT_ATARI_ATR_MAGIC     0x0296

/**
 * @brief Check if data is ATR format (has header)
 * @param data Image data
 * @param size Data size
 * @return true if ATR format
 */
bool uft_atari_is_atr(const uint8_t *data, size_t size);

/**
 * @brief Parse ATR header
 * @param data Image data
 * @param size Data size
 * @param[out] header Header structure
 * @param[out] data_offset Offset to actual disk data
 * @return Error code
 */
uft_atari_error_t uft_atari_parse_atr(const uint8_t *data,
                                      size_t size,
                                      uft_atari_atr_header_t *header,
                                      size_t *data_offset);

/**
 * @brief Create ATR header for disk data
 * @param density Disk density
 * @param[out] header Header structure
 * @return Error code
 */
uft_atari_error_t uft_atari_make_atr_header(uft_atari_density_t density,
                                            uft_atari_atr_header_t *header);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARI_DOS_H */
