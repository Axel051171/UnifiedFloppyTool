/**
 * @file uft_cbm_fs.h
 * @brief Commodore CBM DOS Filesystem - D64/D71/D81 Support
 * 
 * Complete implementation of CBM DOS filesystem operations:
 * - D64: 1541/1570 (35/40 tracks, 170/192KB)
 * - D71: 1571 (70/80 tracks, 340/384KB, double-sided)
 * - D81: 1581 (80 tracks, 800KB, 3.5" DD)
 * 
 * Features:
 * - Directory parsing with all entry types
 * - BAM (Block Allocation Map) management
 * - File chain following and validation
 * - PRG/SEQ/USR/REL/DEL file types
 * - Operations: list, extract, inject, delete, rename, validate
 * - Scratch (delete), Copy, Format operations
 * - GEOS file detection and metadata extraction
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @copyright UFT Project
 */

#ifndef UFT_CBM_FS_H
#define UFT_CBM_FS_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Sector size in bytes */
#define UFT_CBM_SECTOR_SIZE         256

/** Maximum filename length (PETSCII) */
#define UFT_CBM_FILENAME_MAX        16

/** Maximum directory entries per image type */
#define UFT_CBM_D64_MAX_ENTRIES     144
#define UFT_CBM_D71_MAX_ENTRIES     288
#define UFT_CBM_D81_MAX_ENTRIES     296

/** BAM locations */
#define UFT_CBM_D64_BAM_TRACK       18
#define UFT_CBM_D64_BAM_SECTOR      0
#define UFT_CBM_D71_BAM2_TRACK      53
#define UFT_CBM_D71_BAM2_SECTOR     0
#define UFT_CBM_D81_BAM_TRACK       40
#define UFT_CBM_D81_BAM_SECTOR      1

/** Directory locations */
#define UFT_CBM_D64_DIR_TRACK       18
#define UFT_CBM_D64_DIR_SECTOR      1
#define UFT_CBM_D81_DIR_TRACK       40
#define UFT_CBM_D81_DIR_SECTOR      3

/** Track counts */
#define UFT_CBM_D64_TRACKS          35
#define UFT_CBM_D64_EXT_TRACKS      40
#define UFT_CBM_D71_TRACKS          70
#define UFT_CBM_D71_EXT_TRACKS      80
#define UFT_CBM_D81_TRACKS          80

/** Image sizes */
#define UFT_CBM_D64_SIZE            174848   /* 35 tracks, no errors */
#define UFT_CBM_D64_SIZE_ERR        175531   /* 35 tracks + errors */
#define UFT_CBM_D64_EXT_SIZE        196608   /* 40 tracks, no errors */
#define UFT_CBM_D64_EXT_SIZE_ERR    197376   /* 40 tracks + errors */
#define UFT_CBM_D71_SIZE            349696   /* 70 tracks */
#define UFT_CBM_D71_SIZE_ERR        351062   /* 70 tracks + errors */
#define UFT_CBM_D81_SIZE            819200   /* 80 tracks */
#define UFT_CBM_D81_SIZE_ERR        822400   /* 80 tracks + errors */

/*============================================================================
 * Enumerations
 *============================================================================*/

/**
 * @brief CBM image type
 */
typedef enum {
    UFT_CBM_TYPE_UNKNOWN = 0,
    UFT_CBM_TYPE_D64,           /**< 1541/1570 single-sided */
    UFT_CBM_TYPE_D64_40,        /**< 1541/1570 40-track (extended) */
    UFT_CBM_TYPE_D71,           /**< 1571 double-sided */
    UFT_CBM_TYPE_D71_80,        /**< 1571 80-track (extended) */
    UFT_CBM_TYPE_D81,           /**< 1581 3.5" DD */
    UFT_CBM_TYPE_G64,           /**< GCR-encoded flux image */
    UFT_CBM_TYPE_G71            /**< GCR-encoded 1571 flux image */
} uft_cbm_type_t;

/**
 * @brief CBM file types
 */
typedef enum {
    UFT_CBM_FT_DEL = 0,         /**< Deleted file */
    UFT_CBM_FT_SEQ = 1,         /**< Sequential file */
    UFT_CBM_FT_PRG = 2,         /**< Program file */
    UFT_CBM_FT_USR = 3,         /**< User-defined file */
    UFT_CBM_FT_REL = 4,         /**< Relative/random-access file */
    UFT_CBM_FT_CBM = 5,         /**< CBM partition (D81 only) */
    UFT_CBM_FT_DIR = 6          /**< Directory (D81 sub-directory) */
} uft_cbm_filetype_t;

/**
 * @brief Directory entry flags
 */
typedef enum {
    UFT_CBM_FLAG_CLOSED   = 0x80,  /**< File properly closed */
    UFT_CBM_FLAG_LOCKED   = 0x40,  /**< Write-protected/locked */
    UFT_CBM_FLAG_SAVEAT   = 0x20,  /**< GEOS save@replace */
    UFT_CBM_FLAG_GEOS     = 0x10,  /**< GEOS file type present */
    UFT_CBM_FLAG_PROTECT  = 0x0F   /**< Protection bits mask */
} uft_cbm_flags_t;

/**
 * @brief GEOS file types
 */
typedef enum {
    UFT_GEOS_TYPE_NON     = 0x00,  /**< Non-GEOS file */
    UFT_GEOS_TYPE_BASIC   = 0x01,  /**< BASIC program */
    UFT_GEOS_TYPE_ASM     = 0x02,  /**< Assembly program */
    UFT_GEOS_TYPE_DATA    = 0x03,  /**< Data file */
    UFT_GEOS_TYPE_SYS     = 0x04,  /**< System file */
    UFT_GEOS_TYPE_DESK    = 0x05,  /**< Desk accessory */
    UFT_GEOS_TYPE_APPL    = 0x06,  /**< Application */
    UFT_GEOS_TYPE_PRINT   = 0x07,  /**< Print driver */
    UFT_GEOS_TYPE_INPUT   = 0x08,  /**< Input driver */
    UFT_GEOS_TYPE_FONT    = 0x09,  /**< Font file */
    UFT_GEOS_TYPE_BOOT    = 0x0A,  /**< Boot file */
    UFT_GEOS_TYPE_TEMP    = 0x0B,  /**< Temporary file */
    UFT_GEOS_TYPE_AUTO    = 0x0C   /**< Auto-exec file */
} uft_geos_type_t;

/**
 * @brief GEOS structure types
 */
typedef enum {
    UFT_GEOS_STRUCT_SEQ   = 0x00,  /**< Sequential (like SEQ) */
    UFT_GEOS_STRUCT_VLIR  = 0x01   /**< VLIR (Variable Length Index Record) */
} uft_geos_struct_t;

/*============================================================================
 * Structures
 *============================================================================*/

/**
 * @brief Track/sector address
 */
typedef struct {
    uint8_t track;
    uint8_t sector;
} uft_cbm_ts_t;

/**
 * @brief Directory entry (parsed)
 */
typedef struct {
    /** Entry index in directory (0-based) */
    uint16_t index;
    
    /** Raw file type byte */
    uint8_t type_byte;
    
    /** Parsed file type */
    uft_cbm_filetype_t file_type;
    
    /** Entry flags (closed, locked, etc.) */
    uint8_t flags;
    
    /** First data track/sector */
    uft_cbm_ts_t first_ts;
    
    /** Filename (PETSCII, null-terminated) */
    uint8_t filename[UFT_CBM_FILENAME_MAX + 1];
    
    /** Filename length (without padding) */
    uint8_t filename_len;
    
    /** Side-sector track/sector (REL files) */
    uft_cbm_ts_t side_ts;
    
    /** Record length (REL files, 1-254) */
    uint8_t rel_record_len;
    
    /** GEOS info block track/sector */
    uft_cbm_ts_t geos_info_ts;
    
    /** GEOS file type */
    uft_geos_type_t geos_type;
    
    /** GEOS structure type */
    uft_geos_struct_t geos_struct;
    
    /** File size in blocks (from directory) */
    uint16_t blocks;
    
    /** Calculated file size in bytes */
    uint32_t size_bytes;
    
    /** Chain validated flag */
    bool chain_valid;
    
    /** Actual block count from chain */
    uint16_t actual_blocks;
    
    /** Track/sector where this entry is stored */
    uft_cbm_ts_t entry_ts;
    
    /** Offset within sector (0-7, *32 for byte offset) */
    uint8_t entry_offset;
} uft_cbm_dir_entry_t;

/**
 * @brief Directory listing result
 */
typedef struct {
    /** Array of directory entries */
    uft_cbm_dir_entry_t* entries;
    
    /** Number of entries */
    uint16_t count;
    
    /** Allocated capacity */
    uint16_t capacity;
    
    /** Disk name (16 chars, PETSCII) */
    uint8_t disk_name[UFT_CBM_FILENAME_MAX + 1];
    
    /** Disk ID (2 chars) */
    uint8_t disk_id[3];
    
    /** DOS type (2 chars) */
    uint8_t dos_type[3];
    
    /** Blocks free (from BAM) */
    uint16_t blocks_free;
    
    /** Total blocks */
    uint16_t blocks_total;
} uft_cbm_directory_t;

/**
 * @brief BAM entry for a single track
 */
typedef struct {
    uint8_t track;
    uint8_t free_sectors;
    uint8_t bitmap[3];     /**< D64/D71: 3 bytes, D81: uses sector-based BAM */
} uft_cbm_bam_track_t;

/**
 * @brief BAM context
 */
typedef struct {
    /** Image type */
    uft_cbm_type_t type;
    
    /** Total tracks */
    uint8_t total_tracks;
    
    /** Per-track BAM entries */
    uft_cbm_bam_track_t* tracks;
    
    /** Total free blocks */
    uint16_t total_free;
    
    /** Total blocks */
    uint16_t total_blocks;
    
    /** BAM modified flag */
    bool modified;
} uft_cbm_bam_t;

/**
 * @brief File chain for following sector links
 */
typedef struct {
    /** Array of track/sector pairs */
    uft_cbm_ts_t* chain;
    
    /** Number of blocks in chain */
    uint16_t count;
    
    /** Allocated capacity */
    uint16_t capacity;
    
    /** Last sector used bytes (1-254) */
    uint8_t last_bytes;
    
    /** Total file size in bytes */
    uint32_t total_bytes;
    
    /** Chain is circular (error) */
    bool circular;
    
    /** Chain has invalid links (error) */
    bool broken;
    
    /** Cross-linked with another file */
    bool cross_linked;
} uft_cbm_chain_t;

/**
 * @brief Validation report
 */
typedef struct {
    /** Image type detected */
    uft_cbm_type_t type;
    
    /** Image has error bytes */
    bool has_errors;
    
    /** BAM valid */
    bool bam_valid;
    
    /** BAM matches actual usage */
    bool bam_consistent;
    
    /** Directory valid */
    bool dir_valid;
    
    /** All file chains valid */
    bool chains_valid;
    
    /** Total files */
    uint16_t total_files;
    
    /** Files with broken chains */
    uint16_t broken_chains;
    
    /** Cross-linked sectors */
    uint16_t cross_links;
    
    /** Orphan sectors (allocated but unused) */
    uint16_t orphan_sectors;
    
    /** Unallocated but used sectors */
    uint16_t unallocated_used;
    
    /** Error messages (allocated, null-terminated) */
    char** errors;
    uint16_t error_count;
    
    /** Warning messages (allocated, null-terminated) */
    char** warnings;
    uint16_t warning_count;
} uft_cbm_validation_t;

/**
 * @brief CBM filesystem context
 */
typedef struct {
    /** Image data (owned) */
    uint8_t* image;
    
    /** Image size */
    size_t image_size;
    
    /** Image type */
    uft_cbm_type_t type;
    
    /** Has error table */
    bool has_errors;
    
    /** Error table (if present) */
    uint8_t* error_table;
    
    /** Track count */
    uint8_t tracks;
    
    /** File path (owned, may be NULL) */
    char* path;
    
    /** Writable mode */
    bool writable;
    
    /** Modified flag */
    bool modified;
    
    /** BAM cache */
    uft_cbm_bam_t* bam;
    
    /** Directory cache */
    uft_cbm_directory_t* dir;
    
    /** Error context */
    uft_error_ctx_t error;
} uft_cbm_fs_t;

/**
 * @brief File extraction options
 */
typedef struct {
    /** Include load address (PRG files) */
    bool include_load_addr;
    
    /** Convert PETSCII to ASCII */
    bool convert_petscii;
    
    /** Handle GEOS VLIR records */
    bool handle_geos_vlir;
    
    /** Maximum size limit (0 = unlimited) */
    size_t max_size;
} uft_cbm_extract_opts_t;

/**
 * @brief File injection options
 */
typedef struct {
    /** File type to use */
    uft_cbm_filetype_t file_type;
    
    /** PRG load address (if auto-detect disabled) */
    uint16_t load_address;
    
    /** Auto-detect load address from first 2 bytes */
    bool auto_load_addr;
    
    /** REL record length (for REL files) */
    uint8_t rel_record_len;
    
    /** Replace existing file with same name */
    bool replace_existing;
    
    /** Lock file after writing */
    bool lock_file;
    
    /** Interleave for allocation (0 = default) */
    uint8_t interleave;
} uft_cbm_inject_opts_t;

/**
 * @brief Directory callback function
 * 
 * @param entry Directory entry
 * @param user_data User-provided context
 * @return true to continue, false to stop
 */
typedef bool (*uft_cbm_dir_callback_t)(const uft_cbm_dir_entry_t* entry, void* user_data);

/*============================================================================
 * Lifecycle Functions
 *============================================================================*/

/**
 * @brief Create CBM filesystem context
 * 
 * @param[out] fs Pointer to receive context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_fs_create(uft_cbm_fs_t** fs);

/**
 * @brief Destroy CBM filesystem context
 * 
 * @param[in,out] fs Context to destroy (set to NULL)
 */
void uft_cbm_fs_destroy(uft_cbm_fs_t** fs);

/**
 * @brief Open image file
 * 
 * @param fs Context
 * @param path File path
 * @param writable Open for writing
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_fs_open(uft_cbm_fs_t* fs, const char* path, bool writable);

/**
 * @brief Open from memory buffer
 * 
 * @param fs Context
 * @param data Image data (copied)
 * @param size Data size
 * @param writable Allow modifications
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_fs_open_mem(uft_cbm_fs_t* fs, const uint8_t* data, 
                              size_t size, bool writable);

/**
 * @brief Save changes to file
 * 
 * @param fs Context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_fs_save(uft_cbm_fs_t* fs);

/**
 * @brief Save to new file
 * 
 * @param fs Context
 * @param path New file path
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_fs_save_as(uft_cbm_fs_t* fs, const char* path);

/**
 * @brief Close image
 * 
 * @param fs Context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_fs_close(uft_cbm_fs_t* fs);

/**
 * @brief Create new blank image
 * 
 * @param fs Context
 * @param type Image type to create
 * @param disk_name Disk name (PETSCII, max 16 chars)
 * @param disk_id Disk ID (2 chars)
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_fs_format(uft_cbm_fs_t* fs, uft_cbm_type_t type,
                            const char* disk_name, const char* disk_id);

/*============================================================================
 * Detection Functions
 *============================================================================*/

/**
 * @brief Detect image type from data
 * 
 * @param data Image data
 * @param size Data size
 * @param[out] type Detected type
 * @param[out] has_errors Has error table
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_detect_type(const uint8_t* data, size_t size,
                              uft_cbm_type_t* type, bool* has_errors);

/**
 * @brief Get type name string
 * 
 * @param type Image type
 * @return Type name or "Unknown"
 */
const char* uft_cbm_type_name(uft_cbm_type_t type);

/**
 * @brief Get file type name string
 * 
 * @param ft File type
 * @return File type name (e.g., "PRG", "SEQ")
 */
const char* uft_cbm_filetype_name(uft_cbm_filetype_t ft);

/*============================================================================
 * Sector Access Functions
 *============================================================================*/

/**
 * @brief Get sectors per track
 * 
 * @param type Image type
 * @param track Track number (1-based)
 * @return Number of sectors or 0 if invalid
 */
uint8_t uft_cbm_sectors_per_track(uft_cbm_type_t type, uint8_t track);

/**
 * @brief Calculate sector offset in image
 * 
 * @param type Image type
 * @param track Track number (1-based)
 * @param sector Sector number (0-based)
 * @return Byte offset or SIZE_MAX if invalid
 */
size_t uft_cbm_sector_offset(uft_cbm_type_t type, uint8_t track, uint8_t sector);

/**
 * @brief Read sector data
 * 
 * @param fs Context
 * @param track Track number
 * @param sector Sector number
 * @param[out] buffer Output buffer (256 bytes)
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_read_sector(uft_cbm_fs_t* fs, uint8_t track, uint8_t sector,
                              uint8_t buffer[UFT_CBM_SECTOR_SIZE]);

/**
 * @brief Write sector data
 * 
 * @param fs Context
 * @param track Track number
 * @param sector Sector number
 * @param buffer Input buffer (256 bytes)
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_write_sector(uft_cbm_fs_t* fs, uint8_t track, uint8_t sector,
                               const uint8_t buffer[UFT_CBM_SECTOR_SIZE]);

/**
 * @brief Get sector error code
 * 
 * @param fs Context
 * @param track Track number
 * @param sector Sector number
 * @return Error code (0 = OK) or 0xFF if no error table
 */
uint8_t uft_cbm_sector_error(uft_cbm_fs_t* fs, uint8_t track, uint8_t sector);

/*============================================================================
 * BAM Functions
 *============================================================================*/

/**
 * @brief Load/refresh BAM from image
 * 
 * @param fs Context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_bam_load(uft_cbm_fs_t* fs);

/**
 * @brief Write BAM back to image
 * 
 * @param fs Context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_bam_save(uft_cbm_fs_t* fs);

/**
 * @brief Check if sector is allocated
 * 
 * @param fs Context
 * @param track Track number
 * @param sector Sector number
 * @return true if allocated
 */
bool uft_cbm_bam_is_allocated(uft_cbm_fs_t* fs, uint8_t track, uint8_t sector);

/**
 * @brief Allocate sector in BAM
 * 
 * @param fs Context
 * @param track Track number
 * @param sector Sector number
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_bam_allocate(uft_cbm_fs_t* fs, uint8_t track, uint8_t sector);

/**
 * @brief Free sector in BAM
 * 
 * @param fs Context
 * @param track Track number
 * @param sector Sector number
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_bam_free(uft_cbm_fs_t* fs, uint8_t track, uint8_t sector);

/**
 * @brief Find and allocate next free sector
 * 
 * @param fs Context
 * @param near_track Preferred track (0 for any)
 * @param[out] track Allocated track
 * @param[out] sector Allocated sector
 * @param interleave Sector interleave (0 for default)
 * @return UFT_SUCCESS or UFT_ERR_DISK_FULL
 */
uft_rc_t uft_cbm_bam_alloc_next(uft_cbm_fs_t* fs, uint8_t near_track,
                                 uint8_t* track, uint8_t* sector,
                                 uint8_t interleave);

/**
 * @brief Get free blocks count
 * 
 * @param fs Context
 * @return Number of free blocks
 */
uint16_t uft_cbm_bam_free_blocks(uft_cbm_fs_t* fs);

/**
 * @brief Get total blocks count
 * 
 * @param fs Context
 * @return Total number of blocks
 */
uint16_t uft_cbm_bam_total_blocks(uft_cbm_fs_t* fs);

/*============================================================================
 * Directory Functions
 *============================================================================*/

/**
 * @brief Load/refresh directory
 * 
 * @param fs Context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_dir_load(uft_cbm_fs_t* fs);

/**
 * @brief Get directory
 * 
 * @param fs Context
 * @return Directory structure or NULL
 */
const uft_cbm_directory_t* uft_cbm_dir_get(uft_cbm_fs_t* fs);

/**
 * @brief Iterate directory with callback
 * 
 * @param fs Context
 * @param callback Function to call for each entry
 * @param user_data User context passed to callback
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_dir_foreach(uft_cbm_fs_t* fs, uft_cbm_dir_callback_t callback,
                              void* user_data);

/**
 * @brief Find file by name
 * 
 * @param fs Context
 * @param filename Filename to find (PETSCII, supports wildcards * ?)
 * @param[out] entry Found entry (copied)
 * @return UFT_SUCCESS, UFT_ERR_NOT_FOUND, or error
 */
uft_rc_t uft_cbm_dir_find(uft_cbm_fs_t* fs, const char* filename,
                           uft_cbm_dir_entry_t* entry);

/**
 * @brief Get directory entry by index
 * 
 * @param fs Context
 * @param index Entry index
 * @param[out] entry Entry (copied)
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_dir_get_entry(uft_cbm_fs_t* fs, uint16_t index,
                                uft_cbm_dir_entry_t* entry);

/**
 * @brief Count directory entries
 * 
 * @param fs Context
 * @return Number of entries
 */
uint16_t uft_cbm_dir_count(uft_cbm_fs_t* fs);

/*============================================================================
 * File Chain Functions
 *============================================================================*/

/**
 * @brief Create chain context
 * 
 * @param[out] chain Chain to create
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_chain_create(uft_cbm_chain_t** chain);

/**
 * @brief Destroy chain context
 * 
 * @param[in,out] chain Chain to destroy
 */
void uft_cbm_chain_destroy(uft_cbm_chain_t** chain);

/**
 * @brief Follow file chain from starting T/S
 * 
 * @param fs Context
 * @param start_track Starting track
 * @param start_sector Starting sector
 * @param[out] chain Chain result
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_chain_follow(uft_cbm_fs_t* fs, uint8_t start_track,
                               uint8_t start_sector, uft_cbm_chain_t* chain);

/**
 * @brief Validate chain against BAM
 * 
 * @param fs Context
 * @param chain Chain to validate
 * @return true if chain is valid and matches BAM
 */
bool uft_cbm_chain_validate(uft_cbm_fs_t* fs, const uft_cbm_chain_t* chain);

/*============================================================================
 * File Operations
 *============================================================================*/

/**
 * @brief Extract file to buffer
 * 
 * @param fs Context
 * @param filename Filename (PETSCII)
 * @param opts Extraction options (NULL for defaults)
 * @param[out] data Output buffer (caller must free)
 * @param[out] size Data size
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_file_extract(uft_cbm_fs_t* fs, const char* filename,
                               const uft_cbm_extract_opts_t* opts,
                               uint8_t** data, size_t* size);

/**
 * @brief Extract file by directory entry
 * 
 * @param fs Context
 * @param entry Directory entry
 * @param opts Extraction options
 * @param[out] data Output buffer (caller must free)
 * @param[out] size Data size
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_file_extract_entry(uft_cbm_fs_t* fs,
                                     const uft_cbm_dir_entry_t* entry,
                                     const uft_cbm_extract_opts_t* opts,
                                     uint8_t** data, size_t* size);

/**
 * @brief Save file to disk
 * 
 * @param fs Context
 * @param filename CBM filename
 * @param path Output file path
 * @param opts Extraction options
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_file_save(uft_cbm_fs_t* fs, const char* filename,
                            const char* path, const uft_cbm_extract_opts_t* opts);

/**
 * @brief Inject file from buffer
 * 
 * @param fs Context
 * @param filename Filename (PETSCII, max 16 chars)
 * @param data File data
 * @param size Data size
 * @param opts Injection options
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_file_inject(uft_cbm_fs_t* fs, const char* filename,
                              const uint8_t* data, size_t size,
                              const uft_cbm_inject_opts_t* opts);

/**
 * @brief Inject file from disk
 * 
 * @param fs Context
 * @param filename CBM filename
 * @param path Input file path
 * @param opts Injection options
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_file_load(uft_cbm_fs_t* fs, const char* filename,
                            const char* path, const uft_cbm_inject_opts_t* opts);

/**
 * @brief Delete file (scratch)
 * 
 * @param fs Context
 * @param filename Filename (supports wildcards)
 * @param[out] deleted Number of files deleted (can be NULL)
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_file_delete(uft_cbm_fs_t* fs, const char* filename,
                              uint16_t* deleted);

/**
 * @brief Rename file
 * 
 * @param fs Context
 * @param old_name Current filename
 * @param new_name New filename
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_file_rename(uft_cbm_fs_t* fs, const char* old_name,
                              const char* new_name);

/**
 * @brief Copy file within image
 * 
 * @param fs Context
 * @param src_name Source filename
 * @param dst_name Destination filename
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_file_copy(uft_cbm_fs_t* fs, const char* src_name,
                            const char* dst_name);

/**
 * @brief Lock/unlock file
 * 
 * @param fs Context
 * @param filename Filename
 * @param locked true to lock, false to unlock
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_file_lock(uft_cbm_fs_t* fs, const char* filename, bool locked);

/*============================================================================
 * Validation Functions
 *============================================================================*/

/**
 * @brief Create validation report
 * 
 * @param[out] report Report to create
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_validation_create(uft_cbm_validation_t** report);

/**
 * @brief Destroy validation report
 * 
 * @param[in,out] report Report to destroy
 */
void uft_cbm_validation_destroy(uft_cbm_validation_t** report);

/**
 * @brief Validate image
 * 
 * @param fs Context
 * @param[out] report Validation report
 * @return UFT_SUCCESS if valid, UFT_ERR_VALIDATION if issues found
 */
uft_rc_t uft_cbm_validate(uft_cbm_fs_t* fs, uft_cbm_validation_t* report);

/**
 * @brief Fix simple issues (BAM inconsistencies)
 * 
 * @param fs Context
 * @param[out] fixed Number of issues fixed (can be NULL)
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_fix_bam(uft_cbm_fs_t* fs, uint16_t* fixed);

/*============================================================================
 * PETSCII Conversion
 *============================================================================*/

/**
 * @brief Convert PETSCII to ASCII
 * 
 * @param petscii PETSCII string
 * @param petscii_len Length of PETSCII string
 * @param[out] ascii Output buffer
 * @param ascii_cap Output capacity
 * @return Number of characters converted
 */
size_t uft_cbm_petscii_to_ascii(const uint8_t* petscii, size_t petscii_len,
                                 char* ascii, size_t ascii_cap);

/**
 * @brief Convert ASCII to PETSCII
 * 
 * @param ascii ASCII string
 * @param[out] petscii Output buffer
 * @param petscii_cap Output capacity
 * @return Number of characters converted
 */
size_t uft_cbm_ascii_to_petscii(const char* ascii, uint8_t* petscii,
                                 size_t petscii_cap);

/**
 * @brief Pad filename with shifted spaces
 * 
 * @param[in,out] filename Filename buffer
 * @param current_len Current length
 * @param max_len Maximum length to pad to
 */
void uft_cbm_pad_filename(uint8_t* filename, size_t current_len, size_t max_len);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Get default extraction options
 * 
 * @return Default options
 */
uft_cbm_extract_opts_t uft_cbm_extract_opts_default(void);

/**
 * @brief Get default injection options
 * 
 * @return Default options
 */
uft_cbm_inject_opts_t uft_cbm_inject_opts_default(void);

/**
 * @brief Format blocks free message
 * 
 * @param fs Context
 * @param[out] buffer Output buffer
 * @param cap Buffer capacity
 * @return Number of characters written
 */
size_t uft_cbm_blocks_free_msg(uft_cbm_fs_t* fs, char* buffer, size_t cap);

/**
 * @brief Print directory listing
 * 
 * @param fs Context
 * @param stream Output stream (FILE*)
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_cbm_print_directory(uft_cbm_fs_t* fs, void* stream);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CBM_FS_H */
