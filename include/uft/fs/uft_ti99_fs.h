/**
 * @file uft_ti99_fs.h
 * @brief TI-99/4A Disk Manager Filesystem Layer
 * 
 * Complete filesystem support for TI-99/4A disk formats:
 * - Single-Sided Single-Density (SSSD): 90KB
 * - Single-Sided Double-Density (SSDD): 180KB
 * - Double-Sided Double-Density (DSDD): 360KB
 * - Double-Sided Quad-Density (DSQD): 720KB (80 track)
 * 
 * Features:
 * - Volume Information Block (VIB) parsing
 * - File Descriptor Index Records (FDIR)
 * - File Descriptor Records (FDR) with data chains
 * - DIS/VAR, DIS/FIX, INT/VAR, INT/FIX, PROGRAM file types
 * - Protection and file attributes
 * - File extraction and injection
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_TI99_FS_H
#define UFT_TI99_FS_H

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

/** Sector size (always 256 bytes) */
#define UFT_TI99_SECTOR_SIZE        256

/** Maximum sectors per track */
#define UFT_TI99_SPT_SD             9       /**< Single density */
#define UFT_TI99_SPT_DD             18      /**< Double density */
#define UFT_TI99_SPT_HD             36      /**< High density */

/** Track counts */
#define UFT_TI99_TRACKS_40          40
#define UFT_TI99_TRACKS_80          80

/** Standard disk sizes */
#define UFT_TI99_SIZE_SSSD          92160   /**< 40×9×256 = 90KB */
#define UFT_TI99_SIZE_SSDD          184320  /**< 40×18×256 = 180KB */
#define UFT_TI99_SIZE_DSDD          368640  /**< 40×18×256×2 = 360KB */
#define UFT_TI99_SIZE_DSQD          737280  /**< 80×18×256×2 = 720KB */

/** Directory/System sectors */
#define UFT_TI99_VIB_SECTOR         0       /**< Volume Information Block */
#define UFT_TI99_FDIR_START         1       /**< First FDIR sector */
#define UFT_TI99_FDIR_COUNT         2       /**< Number of FDIR sectors (SSSD) */
#define UFT_TI99_FDIR_COUNT_DD      2       /**< Number of FDIR sectors (DD) */

/** Directory limits */
#define UFT_TI99_MAX_FILES          127     /**< Maximum files in directory */
#define UFT_TI99_FILENAME_LEN       10      /**< Filename length */

/** File types */
#define UFT_TI99_FILETYPE_DIS       0x00    /**< DISPLAY format */
#define UFT_TI99_FILETYPE_INT       0x01    /**< INTERNAL format */
#define UFT_TI99_FILETYPE_PRG       0x02    /**< PROGRAM (binary) */

/** File flags (in status byte) */
#define UFT_TI99_FLAG_PROGRAM       0x01    /**< PROGRAM file */
#define UFT_TI99_FLAG_INTERNAL      0x02    /**< INTERNAL format */
#define UFT_TI99_FLAG_PROTECTED     0x08    /**< Write protected */
#define UFT_TI99_FLAG_BACKUP        0x10    /**< Backed up */
#define UFT_TI99_FLAG_MODIFIED      0x20    /**< Modified since backup */
#define UFT_TI99_FLAG_VARIABLE      0x80    /**< Variable length records */

/*===========================================================================
 * Disk Format Types
 *===========================================================================*/

/**
 * @brief TI-99/4A disk formats
 */
typedef enum {
    UFT_TI99_FORMAT_UNKNOWN = 0,
    UFT_TI99_FORMAT_SSSD,           /**< Single-Sided Single-Density 90KB */
    UFT_TI99_FORMAT_SSDD,           /**< Single-Sided Double-Density 180KB */
    UFT_TI99_FORMAT_DSDD,           /**< Double-Sided Double-Density 360KB */
    UFT_TI99_FORMAT_DSQD,           /**< Double-Sided Quad-Density 720KB */
    UFT_TI99_FORMAT_DSHD,           /**< Double-Sided High-Density 1.44MB */
    UFT_TI99_FORMAT_COUNT
} uft_ti99_format_t;

/**
 * @brief Disk geometry structure
 */
typedef struct {
    uint8_t  tracks;                /**< Number of tracks */
    uint8_t  sides;                 /**< Number of sides (1 or 2) */
    uint8_t  sectors_per_track;     /**< Sectors per track */
    uint16_t total_sectors;         /**< Total sectors */
    uint32_t total_bytes;           /**< Total capacity */
    uint8_t  density;               /**< Density code (1=SD, 2=DD, 3=HD) */
    uft_ti99_format_t format;       /**< Format type */
} uft_ti99_geometry_t;

/*===========================================================================
 * Volume Information Block (VIB) - Sector 0
 *===========================================================================*/

/**
 * @brief VIB on-disk structure (256 bytes)
 * 
 * Note: All multi-byte values are big-endian (Motorola format)
 */
typedef struct __attribute__((packed)) {
    char     disk_name[10];         /**< 0x00: Volume name (space padded) */
    uint16_t total_sectors;         /**< 0x0A: Total sectors (big-endian) */
    uint8_t  sectors_per_track;     /**< 0x0C: Sectors per track */
    char     dsk_id[3];             /**< 0x0D: "DSK" identifier */
    uint8_t  protection;            /**< 0x10: Protection byte (0x20 = protected) */
    uint8_t  tracks_per_side;       /**< 0x11: Tracks per side */
    uint8_t  sides;                 /**< 0x12: Number of sides */
    uint8_t  density;               /**< 0x13: Density (1=SD, 2=DD, 3=HD) */
    uint8_t  reserved1[36];         /**< 0x14: Reserved */
    uint8_t  creation_date[8];      /**< 0x38: Creation date (optional) */
    uint8_t  reserved2[16];         /**< 0x40: Reserved */
    uint8_t  bitmap[176];           /**< 0x50: Allocation bitmap (0x50-0xFF = 176 bytes) */
} uft_ti99_vib_t;

#define UFT_TI99_VIB_DSK_ID         "DSK"
#define UFT_TI99_PROTECTED          0x20

/*===========================================================================
 * File Descriptor Index Record (FDIR) - Sectors 1-2
 *===========================================================================*/

/**
 * @brief FDIR entry (2 bytes)
 * 
 * Each entry is a sector pointer to a File Descriptor Record (FDR)
 * Big-endian format: high byte first
 * Value 0x0000 = empty slot
 */
typedef struct __attribute__((packed)) {
    uint8_t  fdr_sector_hi;         /**< FDR sector high byte */
    uint8_t  fdr_sector_lo;         /**< FDR sector low byte */
} uft_ti99_fdir_entry_t;

/** Extract FDR sector from FDIR entry */
#define UFT_TI99_FDIR_SECTOR(e) \
    (((uint16_t)(e)->fdr_sector_hi << 8) | (e)->fdr_sector_lo)

/** FDIR entries per sector (256/2 = 128) */
#define UFT_TI99_FDIR_ENTRIES_PER_SECTOR    128

/*===========================================================================
 * File Descriptor Record (FDR) - 256 bytes per file
 *===========================================================================*/

/**
 * @brief FDR on-disk structure
 */
typedef struct __attribute__((packed)) {
    char     filename[10];          /**< 0x00: Filename (space padded) */
    uint16_t reserved1;             /**< 0x0A: Reserved */
    uint8_t  status_flags;          /**< 0x0C: File type and flags */
    uint8_t  records_per_sector;    /**< 0x0D: Records per sector */
    uint16_t total_sectors;         /**< 0x0E: Sectors allocated (big-endian) */
    uint8_t  eof_offset;            /**< 0x10: Bytes in last sector */
    uint8_t  record_length;         /**< 0x11: Logical record length */
    uint16_t level3_records;        /**< 0x12: L3 records (fixed) or sectors (big-endian) */
    uint8_t  creation_time[4];      /**< 0x14: Creation date/time */
    uint8_t  update_time[4];        /**< 0x18: Update date/time */
    uint8_t  data_chain[256-28];    /**< 0x1C: Cluster allocation map */
} uft_ti99_fdr_t;

/**
 * @brief Data chain entry (3 bytes each)
 * 
 * Each entry describes a contiguous run of sectors:
 * - Byte 0: Start sector bits 4-11
 * - Byte 1: Start sector bits 0-3 (high nibble), offset (low nibble)
 * - Byte 2: End sector offset from start
 */
typedef struct __attribute__((packed)) {
    uint8_t  start_hi;              /**< Start sector high 8 bits */
    uint8_t  start_lo_offset;       /**< Start low nibble + offset */
    uint8_t  end_offset;            /**< End offset from start */
} uft_ti99_chain_entry_t;

/** Maximum chain entries in FDR: (256-28)/3 = 76 */
#define UFT_TI99_MAX_CHAIN_ENTRIES  76

/*===========================================================================
 * File Entry (Parsed)
 *===========================================================================*/

/**
 * @brief File type enumeration
 */
typedef enum {
    UFT_TI99_TYPE_DIS_FIX = 0,      /**< DISPLAY/FIXED */
    UFT_TI99_TYPE_DIS_VAR,          /**< DISPLAY/VARIABLE */
    UFT_TI99_TYPE_INT_FIX,          /**< INTERNAL/FIXED */
    UFT_TI99_TYPE_INT_VAR,          /**< INTERNAL/VARIABLE */
    UFT_TI99_TYPE_PROGRAM           /**< PROGRAM (binary) */
} uft_ti99_file_type_t;

/**
 * @brief Parsed file entry
 */
typedef struct {
    char     filename[11];          /**< Filename (null-terminated) */
    uft_ti99_file_type_t type;      /**< File type */
    uint8_t  status_flags;          /**< Raw status flags */
    uint8_t  record_length;         /**< Logical record length */
    uint16_t total_sectors;         /**< Sectors allocated */
    uint16_t total_records;         /**< Total records (or sectors used) */
    uint32_t file_size;             /**< Approximate file size */
    uint16_t fdr_sector;            /**< FDR sector location */
    uint8_t  fdir_index;            /**< Index in FDIR */
    bool     protected;             /**< Write protected */
    bool     variable_length;       /**< Variable length records */
    bool     internal_format;       /**< Internal (binary) format */
    bool     is_program;            /**< PROGRAM file */
} uft_ti99_entry_t;

/**
 * @brief Directory listing
 */
typedef struct {
    char              disk_name[11];/**< Volume name (null-terminated) */
    uft_ti99_format_t format;       /**< Disk format */
    uint16_t          total_sectors;/**< Total sectors */
    uint16_t          free_sectors; /**< Free sectors */
    uint32_t          free_bytes;   /**< Free space in bytes */
    size_t            file_count;   /**< Number of files */
    uft_ti99_entry_t  files[UFT_TI99_MAX_FILES]; /**< File entries */
} uft_ti99_dir_t;

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    UFT_TI99_OK = 0,
    UFT_TI99_ERR_PARAM,             /**< Invalid parameter */
    UFT_TI99_ERR_MEMORY,            /**< Memory allocation failed */
    UFT_TI99_ERR_FORMAT,            /**< Invalid format / not TI-99 image */
    UFT_TI99_ERR_READ,              /**< Read error */
    UFT_TI99_ERR_WRITE,             /**< Write error */
    UFT_TI99_ERR_SECTOR,            /**< Sector out of range */
    UFT_TI99_ERR_VIB,               /**< VIB corrupt or unreadable */
    UFT_TI99_ERR_NOTFOUND,          /**< File not found */
    UFT_TI99_ERR_EXISTS,            /**< File already exists */
    UFT_TI99_ERR_FULL,              /**< Disk full */
    UFT_TI99_ERR_DIRFULL,           /**< Directory full */
    UFT_TI99_ERR_PROTECTED,         /**< File or disk is protected */
    UFT_TI99_ERR_CORRUPT,           /**< Data corruption detected */
    UFT_TI99_ERR_CHAIN,             /**< Bad data chain */
    UFT_TI99_ERR_NOT_OPEN,          /**< Context not open */
    UFT_TI99_ERR_COUNT
} uft_ti99_error_t;

/*===========================================================================
 * Context
 *===========================================================================*/

/** Opaque context handle */
typedef struct uft_ti99_ctx uft_ti99_ctx_t;

/**
 * @brief Detection result
 */
typedef struct {
    bool             valid;         /**< Valid TI-99 image */
    uft_ti99_format_t format;       /**< Detected format */
    uft_ti99_geometry_t geometry;   /**< Disk geometry */
    char             disk_name[11]; /**< Volume name */
    uint8_t          confidence;    /**< Detection confidence (0-100) */
} uft_ti99_detect_result_t;

/*===========================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Create new TI-99 context
 * @return New context or NULL
 */
uft_ti99_ctx_t *uft_ti99_create(void);

/**
 * @brief Destroy context
 * @param ctx Context to destroy
 */
void uft_ti99_destroy(uft_ti99_ctx_t *ctx);

/**
 * @brief Open disk image
 * @param ctx Context
 * @param data Image data
 * @param size Data size
 * @param copy_data True to copy data, false to reference
 * @return Error code
 */
uft_ti99_error_t uft_ti99_open(uft_ti99_ctx_t *ctx,
                               uint8_t *data,
                               size_t size,
                               bool copy_data);

/**
 * @brief Close disk image
 * @param ctx Context
 */
void uft_ti99_close(uft_ti99_ctx_t *ctx);

/**
 * @brief Save disk image to file
 * @param ctx Context
 * @param path Output path
 * @return Error code
 */
uft_ti99_error_t uft_ti99_save(uft_ti99_ctx_t *ctx, const char *path);

/*===========================================================================
 * Detection
 *===========================================================================*/

/**
 * @brief Detect TI-99 disk format
 * @param data Image data
 * @param size Data size
 * @param[out] result Detection result
 * @return Error code
 */
uft_ti99_error_t uft_ti99_detect(const uint8_t *data,
                                  size_t size,
                                  uft_ti99_detect_result_t *result);

/*===========================================================================
 * Sector I/O
 *===========================================================================*/

/**
 * @brief Read sector
 * @param ctx Context
 * @param sector Sector number (0-based)
 * @param buffer Output buffer (256 bytes)
 * @return Error code
 */
uft_ti99_error_t uft_ti99_read_sector(uft_ti99_ctx_t *ctx,
                                       uint16_t sector,
                                       uint8_t *buffer);

/**
 * @brief Write sector
 * @param ctx Context
 * @param sector Sector number
 * @param buffer Input buffer (256 bytes)
 * @return Error code
 */
uft_ti99_error_t uft_ti99_write_sector(uft_ti99_ctx_t *ctx,
                                        uint16_t sector,
                                        const uint8_t *buffer);

/*===========================================================================
 * VIB Operations
 *===========================================================================*/

/**
 * @brief Read VIB
 * @param ctx Context
 * @param[out] vib VIB structure
 * @return Error code
 */
uft_ti99_error_t uft_ti99_read_vib(uft_ti99_ctx_t *ctx, uft_ti99_vib_t *vib);

/**
 * @brief Write VIB
 * @param ctx Context
 * @param vib VIB structure
 * @return Error code
 */
uft_ti99_error_t uft_ti99_write_vib(uft_ti99_ctx_t *ctx, const uft_ti99_vib_t *vib);

/**
 * @brief Check if sector is free
 * @param ctx Context
 * @param sector Sector number
 * @return true if free
 */
bool uft_ti99_is_sector_free(uft_ti99_ctx_t *ctx, uint16_t sector);

/**
 * @brief Allocate sector
 * @param ctx Context
 * @param sector Sector to allocate
 * @return Error code
 */
uft_ti99_error_t uft_ti99_allocate_sector(uft_ti99_ctx_t *ctx, uint16_t sector);

/**
 * @brief Free sector
 * @param ctx Context
 * @param sector Sector to free
 * @return Error code
 */
uft_ti99_error_t uft_ti99_free_sector(uft_ti99_ctx_t *ctx, uint16_t sector);

/**
 * @brief Find free sector
 * @param ctx Context
 * @return Sector number or 0 if none
 */
uint16_t uft_ti99_find_free_sector(uft_ti99_ctx_t *ctx);

/**
 * @brief Get free sector count
 * @param ctx Context
 * @return Number of free sectors
 */
uint16_t uft_ti99_free_sectors(uft_ti99_ctx_t *ctx);

/*===========================================================================
 * Directory Operations
 *===========================================================================*/

/**
 * @brief Read directory
 * @param ctx Context
 * @param[out] dir Directory structure
 * @return Error code
 */
uft_ti99_error_t uft_ti99_read_directory(uft_ti99_ctx_t *ctx,
                                          uft_ti99_dir_t *dir);

/**
 * @brief Find file
 * @param ctx Context
 * @param filename Filename
 * @param[out] entry File entry
 * @return Error code
 */
uft_ti99_error_t uft_ti99_find_file(uft_ti99_ctx_t *ctx,
                                     const char *filename,
                                     uft_ti99_entry_t *entry);

/**
 * @brief File enumeration callback
 */
typedef bool (*uft_ti99_file_cb)(const uft_ti99_entry_t *entry, void *user_data);

/**
 * @brief Iterate over files
 * @param ctx Context
 * @param callback Callback function
 * @param user_data User data for callback
 * @return Error code
 */
uft_ti99_error_t uft_ti99_foreach_file(uft_ti99_ctx_t *ctx,
                                        uft_ti99_file_cb callback,
                                        void *user_data);

/*===========================================================================
 * File Operations
 *===========================================================================*/

/**
 * @brief Extract file to buffer
 * @param ctx Context
 * @param filename Filename
 * @param[out] buffer Allocated buffer
 * @param[out] size File size
 * @return Error code
 */
uft_ti99_error_t uft_ti99_extract_file(uft_ti99_ctx_t *ctx,
                                        const char *filename,
                                        uint8_t **buffer,
                                        size_t *size);

/**
 * @brief Extract file to host file
 * @param ctx Context
 * @param ti_name TI filename
 * @param host_path Host file path
 * @return Error code
 */
uft_ti99_error_t uft_ti99_extract_to_file(uft_ti99_ctx_t *ctx,
                                           const char *ti_name,
                                           const char *host_path);

/**
 * @brief Inject file
 * @param ctx Context
 * @param filename TI filename
 * @param data File data
 * @param size Data size
 * @param type File type
 * @param record_length Record length (0 for PROGRAM)
 * @return Error code
 */
uft_ti99_error_t uft_ti99_inject_file(uft_ti99_ctx_t *ctx,
                                       const char *filename,
                                       const uint8_t *data,
                                       size_t size,
                                       uft_ti99_file_type_t type,
                                       uint8_t record_length);

/**
 * @brief Inject from host file
 * @param ctx Context
 * @param host_path Host file path
 * @param ti_name TI filename (NULL to derive from path)
 * @param type File type
 * @param record_length Record length
 * @return Error code
 */
uft_ti99_error_t uft_ti99_inject_from_file(uft_ti99_ctx_t *ctx,
                                            const char *host_path,
                                            const char *ti_name,
                                            uft_ti99_file_type_t type,
                                            uint8_t record_length);

/**
 * @brief Delete file
 * @param ctx Context
 * @param filename Filename
 * @return Error code
 */
uft_ti99_error_t uft_ti99_delete_file(uft_ti99_ctx_t *ctx,
                                       const char *filename);

/**
 * @brief Rename file
 * @param ctx Context
 * @param old_name Current name
 * @param new_name New name
 * @return Error code
 */
uft_ti99_error_t uft_ti99_rename_file(uft_ti99_ctx_t *ctx,
                                       const char *old_name,
                                       const char *new_name);

/**
 * @brief Set file protection
 * @param ctx Context
 * @param filename Filename
 * @param protected True to protect
 * @return Error code
 */
uft_ti99_error_t uft_ti99_set_protected(uft_ti99_ctx_t *ctx,
                                         const char *filename,
                                         bool protected);

/*===========================================================================
 * Image Creation
 *===========================================================================*/

/**
 * @brief Create new disk image
 * @param ctx Context
 * @param format Disk format
 * @param disk_name Volume name
 * @return Error code
 */
uft_ti99_error_t uft_ti99_create_image(uft_ti99_ctx_t *ctx,
                                        uft_ti99_format_t format,
                                        const char *disk_name);

/**
 * @brief Format existing image
 * @param ctx Context
 * @param disk_name New volume name (NULL to keep)
 * @return Error code
 */
uft_ti99_error_t uft_ti99_format(uft_ti99_ctx_t *ctx, const char *disk_name);

/*===========================================================================
 * Validation
 *===========================================================================*/

/**
 * @brief Validation result
 */
typedef struct {
    bool     valid;                 /**< Overall valid */
    bool     vib_ok;                /**< VIB valid */
    bool     fdir_ok;               /**< FDIR valid */
    bool     chains_ok;             /**< All file chains valid */
    uint32_t errors;                /**< Error count */
    uint32_t warnings;              /**< Warning count */
    uint16_t orphan_sectors;        /**< Orphan sectors */
    uint16_t cross_linked;          /**< Cross-linked sectors */
    char     report[1024];          /**< Detailed report */
} uft_ti99_val_result_t;

/**
 * @brief Validate disk image
 * @param ctx Context
 * @param[out] result Validation result
 * @return Error code
 */
uft_ti99_error_t uft_ti99_validate(uft_ti99_ctx_t *ctx,
                                    uft_ti99_val_result_t *result);

/**
 * @brief Rebuild allocation bitmap from files
 * @param ctx Context
 * @return Error code
 */
uft_ti99_error_t uft_ti99_rebuild_bitmap(uft_ti99_ctx_t *ctx);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Parse TI filename
 * @param input Input string
 * @param[out] filename 10-char buffer
 * @return Error code
 */
uft_ti99_error_t uft_ti99_parse_filename(const char *input, char *filename);

/**
 * @brief Format TI filename for display
 * @param filename 10-char filename
 * @param[out] buffer Output buffer (11 bytes min)
 */
void uft_ti99_format_filename(const char *filename, char *buffer);

/**
 * @brief Validate TI filename
 * @param filename Filename
 * @return true if valid
 */
bool uft_ti99_valid_filename(const char *filename);

/**
 * @brief Get format name
 * @param format Format type
 * @return Format name
 */
const char *uft_ti99_format_name(uft_ti99_format_t format);

/**
 * @brief Get file type name
 * @param type File type
 * @return Type name
 */
const char *uft_ti99_file_type_name(uft_ti99_file_type_t type);

/**
 * @brief Get error message
 * @param error Error code
 * @return Error message
 */
const char *uft_ti99_error_string(uft_ti99_error_t error);

/**
 * @brief Print directory listing
 * @param ctx Context
 * @param output Output file (NULL for stdout)
 */
void uft_ti99_print_directory(uft_ti99_ctx_t *ctx, FILE *output);

/**
 * @brief Print disk info
 * @param ctx Context
 * @param output Output file (NULL for stdout)
 */
void uft_ti99_print_info(uft_ti99_ctx_t *ctx, FILE *output);

/**
 * @brief Export directory to JSON
 * @param ctx Context
 * @param[out] buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or negative error
 */
int uft_ti99_directory_to_json(uft_ti99_ctx_t *ctx,
                               char *buffer,
                               size_t buffer_size);

/*===========================================================================
 * Accessors
 *===========================================================================*/

/**
 * @brief Get disk format
 * @param ctx Context
 * @return Format type
 */
uft_ti99_format_t uft_ti99_get_format(const uft_ti99_ctx_t *ctx);

/**
 * @brief Get geometry
 * @param ctx Context
 * @return Geometry pointer or NULL
 */
const uft_ti99_geometry_t *uft_ti99_get_geometry(const uft_ti99_ctx_t *ctx);

/**
 * @brief Check if modified
 * @param ctx Context
 * @return true if modified
 */
bool uft_ti99_is_modified(const uft_ti99_ctx_t *ctx);

/**
 * @brief Get raw data
 * @param ctx Context
 * @param[out] size Data size
 * @return Data pointer or NULL
 */
uint8_t *uft_ti99_get_data(uft_ti99_ctx_t *ctx, size_t *size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TI99_FS_H */
