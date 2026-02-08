/**
 * @file uft_trsdos.h
 * @brief TRSDOS/LDOS/NewDOS Filesystem Layer - Complete Implementation
 * 
 * Comprehensive TRS-80 DOS filesystem support for preservation and analysis:
 * - TRSDOS 2.3 (Model I)
 * - TRSDOS 1.3 (Model III)
 * - TRSDOS 6.x / LS-DOS (Model 4)
 * - LDOS 5.x
 * - NewDOS/80 2.x
 * - DoubleDOS
 * - MultiDOS
 * - RS-DOS / Disk BASIC (CoCo)
 * 
 * Features:
 * - Granule Allocation Table (GAT)
 * - Hash Index Table (HIT)
 * - Directory entry parsing
 * - File chain following
 * - Password protection support
 * - System file detection
 * - Date/time stamps (TRSDOS 6/LDOS)
 * - Logical record length (LRL)
 * - File operations: list, extract, inject, delete, rename
 * 
 * @author UFT Team
 * @version 1.0.0
 */

#ifndef UFT_TRSDOS_H
#define UFT_TRSDOS_H

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
#define UFT_TRSDOS_MAX_NAME         8

/** Maximum extension length (3 chars) */
#define UFT_TRSDOS_MAX_EXT          3

/** Directory entry size */
#define UFT_TRSDOS_DIR_ENTRY_SIZE   32

/** TRSDOS 2.3 directory entry size */
#define UFT_TRSDOS23_DIR_ENTRY_SIZE 48

/** Maximum files in directory listing */
#define UFT_TRSDOS_MAX_FILES        256

/** Deleted file marker */
#define UFT_TRSDOS_DELETED          0x00

/** End of directory marker */
#define UFT_TRSDOS_END_DIR          0xFF

/** Granule size (in sectors) */
#define UFT_TRSDOS_GRANULE_SECTORS  5

/** Sectors per track (standard) */
#define UFT_TRSDOS_SECTORS_TRACK    10

/** Standard sector size */
#define UFT_TRSDOS_SECTOR_SIZE      256

/** GAT track location */
#define UFT_TRSDOS_GAT_TRACK        17

/** Directory track location (Model I) */
#define UFT_TRSDOS_DIR_TRACK        17

/** Maximum granules */
#define UFT_TRSDOS_MAX_GRANULES     192

/** Maximum extents per file (TRSDOS 2.3) */
#define UFT_TRSDOS_MAX_EXTENTS      4

/** Password hash table size */
#define UFT_TRSDOS_HASH_SIZE        256

/*===========================================================================
 * DOS Version Types
 *===========================================================================*/

/**
 * @brief TRS-80 DOS version enumeration
 */
typedef enum {
    UFT_TRSDOS_VERSION_UNKNOWN = 0,
    UFT_TRSDOS_VERSION_23,          /**< TRSDOS 2.3 (Model I) */
    UFT_TRSDOS_VERSION_13,          /**< TRSDOS 1.3 (Model III) */
    UFT_TRSDOS_VERSION_6,           /**< TRSDOS 6.x / LS-DOS */
    UFT_TRSDOS_VERSION_LDOS5,       /**< LDOS 5.x */
    UFT_TRSDOS_VERSION_NEWDOS80,    /**< NewDOS/80 */
    UFT_TRSDOS_VERSION_DOSPLUS,     /**< DOS+ */
    UFT_TRSDOS_VERSION_MULTIDOS,    /**< MultiDOS */
    UFT_TRSDOS_VERSION_DOUBLEDOS,   /**< DoubleDOS */
    UFT_TRSDOS_VERSION_RSDOS,       /**< RS-DOS / Disk BASIC (CoCo) */
} uft_trsdos_version_t;

/**
 * @brief Disk density type
 */
typedef enum {
    UFT_TRSDOS_DENSITY_SD = 0,      /**< Single density (FM) */
    UFT_TRSDOS_DENSITY_DD,          /**< Double density (MFM) */
    UFT_TRSDOS_DENSITY_HD,          /**< High density */
} uft_trsdos_density_t;

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    UFT_TRSDOS_OK = 0,
    UFT_TRSDOS_ERR_NULL,
    UFT_TRSDOS_ERR_NOMEM,
    UFT_TRSDOS_ERR_IO,
    UFT_TRSDOS_ERR_NOT_TRSDOS,
    UFT_TRSDOS_ERR_CORRUPT,
    UFT_TRSDOS_ERR_NOT_FOUND,
    UFT_TRSDOS_ERR_EXISTS,
    UFT_TRSDOS_ERR_FULL,
    UFT_TRSDOS_ERR_PROTECTED,
    UFT_TRSDOS_ERR_INVALID,
    UFT_TRSDOS_ERR_READONLY,
    UFT_TRSDOS_ERR_PASSWORD,
    UFT_TRSDOS_ERR_LOCKED,
    UFT_TRSDOS_ERR_RANGE,
} uft_trsdos_err_t;

/*===========================================================================
 * File Attributes
 *===========================================================================*/

/**
 * @brief File visibility/protection flags
 */
typedef enum {
    UFT_TRSDOS_ATTR_VISIBLE = 0,    /**< Normal visible file */
    UFT_TRSDOS_ATTR_INVISIBLE = 1,  /**< Hidden file */
    UFT_TRSDOS_ATTR_SYSTEM = 2,     /**< System file */
} uft_trsdos_visibility_t;

/**
 * @brief File access protection level
 */
typedef enum {
    UFT_TRSDOS_PROT_FULL = 0,       /**< Full access */
    UFT_TRSDOS_PROT_EXEC = 1,       /**< Execute only */
    UFT_TRSDOS_PROT_READ = 2,       /**< Read only */
    UFT_TRSDOS_PROT_RENAME = 3,     /**< Rename protected */
    UFT_TRSDOS_PROT_REMOVE = 4,     /**< Remove protected */
    UFT_TRSDOS_PROT_WRITE = 5,      /**< Write protected */
    UFT_TRSDOS_PROT_UPDATE = 6,     /**< Update protected */
    UFT_TRSDOS_PROT_LOCKED = 7,     /**< Fully locked */
} uft_trsdos_protection_t;

/**
 * @brief File attributes structure
 */
typedef struct {
    uft_trsdos_visibility_t visibility;
    uft_trsdos_protection_t protection;
    bool has_password;              /**< Password protected */
    bool is_system;                 /**< System file (SYS attribute) */
    bool is_backup;                 /**< Backup file */
    uint8_t user_number;            /**< User number (LDOS/TRSDOS 6) */
} uft_trsdos_attrib_t;

/*===========================================================================
 * Disk Geometry
 *===========================================================================*/

/**
 * @brief Disk geometry preset
 */
typedef enum {
    UFT_TRSDOS_GEOM_UNKNOWN = 0,
    UFT_TRSDOS_GEOM_M1_SSSD,        /**< Model I: 35T×1H×10S×256B = 89.6KB */
    UFT_TRSDOS_GEOM_M1_SSDD,        /**< Model I: 35T×1H×18S×256B = 161KB */
    UFT_TRSDOS_GEOM_M1_DSSD,        /**< Model I: 35T×2H×10S×256B = 179KB */
    UFT_TRSDOS_GEOM_M1_DSDD,        /**< Model I: 35T×2H×18S×256B = 322KB */
    UFT_TRSDOS_GEOM_M3_SSDD,        /**< Model III: 40T×1H×18S×256B = 184KB */
    UFT_TRSDOS_GEOM_M3_DSDD,        /**< Model III: 40T×2H×18S×256B = 368KB */
    UFT_TRSDOS_GEOM_M4_DSDD,        /**< Model 4: 40T×2H×18S×256B = 368KB */
    UFT_TRSDOS_GEOM_M4_80T,         /**< Model 4: 80T×2H×18S×256B = 737KB */
    UFT_TRSDOS_GEOM_COCO_SSSD,      /**< CoCo: 35T×1H×18S×256B = 161KB */
    UFT_TRSDOS_GEOM_COCO_DSDD,      /**< CoCo: 40T×2H×18S×256B = 368KB */
    UFT_TRSDOS_GEOM_COUNT,
} uft_trsdos_geom_type_t;

/**
 * @brief Disk geometry structure
 */
typedef struct {
    uint8_t tracks;                 /**< Tracks per side */
    uint8_t sides;                  /**< Number of sides */
    uint8_t sectors_per_track;      /**< Sectors per track */
    uint16_t sector_size;           /**< Bytes per sector */
    uint16_t dir_track;             /**< Directory track */
    uint8_t granule_sectors;        /**< Sectors per granule */
    uint16_t total_granules;        /**< Total granules on disk */
    uint32_t total_bytes;           /**< Total capacity */
    uft_trsdos_density_t density;
    const char *name;
} uft_trsdos_geometry_t;

/*===========================================================================
 * GAT - Granule Allocation Table
 *===========================================================================*/

/**
 * @brief Granule entry in GAT
 */
typedef struct {
    uint8_t track;                  /**< Track number */
    uint8_t granule_in_track;       /**< Granule within track (0-1) */
    bool allocated;                 /**< True if allocated */
    bool is_directory;              /**< Part of directory */
    bool is_system;                 /**< System granule */
} uft_trsdos_granule_t;

/**
 * @brief GAT structure
 */
typedef struct {
    uint8_t raw[UFT_TRSDOS_MAX_GRANULES];   /**< Raw allocation bitmap */
    uint16_t total_granules;        /**< Total available */
    uint16_t free_granules;         /**< Free granules */
    uint16_t dir_granules;          /**< Directory granules */
    uint16_t system_granules;       /**< System granules */
    uint8_t lockout_table[16];      /**< Track lockout (NewDOS) */
} uft_trsdos_gat_t;

/*===========================================================================
 * HIT - Hash Index Table
 *===========================================================================*/

/**
 * @brief HIT structure (directory hashing)
 */
typedef struct {
    uint8_t hash[UFT_TRSDOS_HASH_SIZE];  /**< Hash table entries */
    uint16_t entries_used;          /**< Number of entries used */
} uft_trsdos_hit_t;

/*===========================================================================
 * Directory Structures
 *===========================================================================*/

/**
 * @brief TRSDOS 2.3 Directory Entry (48 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    /* Extent 0: bytes 0-7 */
    uint8_t attr;                   /**< Attribute byte */
    uint8_t month;                  /**< Month (ASCII) */
    uint8_t day;                    /**< Day (ASCII) */
    uint8_t year;                   /**< Year (ASCII) */
    uint8_t eof_offset;             /**< EOF offset in last sector */
    uint8_t lrl;                    /**< Logical record length */
    uint8_t password[2];            /**< Password hash */
    
    /* Extent 1: bytes 8-15 (filename) */
    char name[8];                   /**< Filename */
    
    /* Extent 2: bytes 16-23 (extension + extents) */
    char ext[3];                    /**< Extension */
    uint8_t ext_info[5];            /**< Extent allocation info */
    
    /* Extent 3: bytes 24-47 (granule allocation) */
    struct {
        uint8_t start_granule;      /**< Starting granule */
        uint8_t num_granules;       /**< Number of granules */
    } extents[4];                   /**< Up to 4 extents */
    uint8_t reserved[16];           /**< Reserved */
} uft_trsdos23_dir_entry_t;
UFT_PACK_END

/**
 * @brief TRSDOS 6/LDOS Directory Entry (32 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t attr;                   /**< Attribute byte */
    char name[8];                   /**< Filename (space-padded) */
    char ext[3];                    /**< Extension (space-padded) */
    uint8_t update_password[2];     /**< Update password hash */
    uint8_t access_password[2];     /**< Access password hash */
    uint16_t eof;                   /**< End-of-file offset */
    uint8_t lrl;                    /**< Logical record length */
    uint8_t flags;                  /**< Flags byte */
    union {
        struct {
            uint8_t month;          /**< Month (1-12) */
            uint8_t day;            /**< Day (1-31) */
            uint8_t year;           /**< Year (0-99) */
        } date;
        uint8_t raw_date[3];
    };
    uint8_t fde_cnt;                /**< Extent count */
    uint8_t fxde[7];                /**< File extent data elements */
} uft_trsdos6_dir_entry_t;
UFT_PACK_END

/**
 * @brief RS-DOS / CoCo Directory Entry (32 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    char name[8];                   /**< Filename (space-padded) */
    char ext[3];                    /**< Extension (space-padded) */
    uint8_t file_type;              /**< File type: 0=BASIC, 1=Data, 2=ML, 3=Text */
    uint8_t ascii_flag;             /**< 0=Binary, 0xFF=ASCII */
    uint8_t first_granule;          /**< First granule number */
    uint16_t last_sector_bytes;     /**< Bytes in last sector (LE) */
    uint8_t reserved[16];           /**< Reserved */
} uft_rsdos_dir_entry_t;
UFT_PACK_END

/*===========================================================================
 * File Entry (unified structure)
 *===========================================================================*/

/**
 * @brief Unified file entry structure
 */
typedef struct {
    char name[UFT_TRSDOS_MAX_NAME + 1];     /**< Filename */
    char ext[UFT_TRSDOS_MAX_EXT + 1];       /**< Extension */
    uint32_t size;                  /**< File size in bytes */
    uint16_t sectors;               /**< Sectors used */
    uint8_t granules;               /**< Granules used */
    
    /** Attributes */
    uft_trsdos_attrib_t attrib;
    
    /** Logical record length */
    uint8_t lrl;
    
    /** Date/time (if supported) */
    bool has_date;
    struct {
        uint8_t year;               /**< Year (0-99 or 78-127) */
        uint8_t month;              /**< Month (1-12) */
        uint8_t day;                /**< Day (1-31) */
    } date;
    
    /** Extent chain */
    struct {
        uint8_t start_granule;
        uint8_t num_granules;
    } extents[16];                  /**< Up to 16 extents */
    uint8_t extent_count;
    
    /** Directory info */
    uint16_t dir_entry_index;       /**< Directory entry index */
    
    /** Raw entry type */
    uft_trsdos_version_t version;
} uft_trsdos_entry_t;

/**
 * @brief Directory listing structure
 */
typedef struct {
    uft_trsdos_entry_t *entries;
    size_t count;
    size_t capacity;
    
    /** Summary */
    uint32_t total_files;
    uint32_t total_size;
    uint32_t free_size;
    uint16_t free_granules;
} uft_trsdos_dir_t;

/*===========================================================================
 * Detection Result
 *===========================================================================*/

typedef struct {
    bool valid;
    uft_trsdos_version_t version;
    uft_trsdos_geom_type_t geometry;
    uint8_t confidence;             /**< 0-100% */
    char disk_name[16];
    time_t disk_date;
    bool is_bootable;
    bool has_password;
    const char *description;
} uft_trsdos_detect_t;

/*===========================================================================
 * Filesystem Context
 *===========================================================================*/

typedef struct {
    /** Image data */
    uint8_t *data;
    size_t size;
    bool owns_data;
    bool writable;
    bool modified;
    
    /** Filesystem info */
    uft_trsdos_version_t version;
    uft_trsdos_geometry_t geometry;
    
    /** Allocation structures */
    uft_trsdos_gat_t gat;
    uft_trsdos_hit_t hit;
    
    /** Directory info */
    uint16_t dir_track;
    uint16_t dir_sectors;
    uint16_t dir_entries_max;
    
    /** Disk metadata */
    char disk_name[16];
    char disk_date[12];
    bool auto_date;
    uint8_t master_password[2];
    
    /** Cached directory */
    uft_trsdos_dir_t dir_cache;
    bool dir_cache_valid;
} uft_trsdos_ctx_t;

/*===========================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create filesystem context
 * @return New context or NULL
 */
uft_trsdos_ctx_t *uft_trsdos_create(void);

/**
 * @brief Destroy filesystem context
 * @param ctx Context to destroy
 */
void uft_trsdos_destroy(uft_trsdos_ctx_t *ctx);

/**
 * @brief Open image with automatic detection
 * @param ctx Context
 * @param data Image data
 * @param size Image size
 * @param copy Copy data (true) or reference (false)
 * @param writable Allow modifications
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_open(uft_trsdos_ctx_t *ctx,
                                  const uint8_t *data, size_t size,
                                  bool copy, bool writable);

/**
 * @brief Open with explicit version/geometry
 * @param ctx Context
 * @param data Image data
 * @param size Image size
 * @param version DOS version
 * @param geom Geometry type
 * @param copy Copy data
 * @param writable Allow modifications
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_open_as(uft_trsdos_ctx_t *ctx,
                                     const uint8_t *data, size_t size,
                                     uft_trsdos_version_t version,
                                     uft_trsdos_geom_type_t geom,
                                     bool copy, bool writable);

/**
 * @brief Close filesystem
 * @param ctx Context
 */
void uft_trsdos_close(uft_trsdos_ctx_t *ctx);

/**
 * @brief Save changes to buffer
 * @param ctx Context
 * @param data Output buffer (NULL to query size)
 * @param size Buffer size / output size
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_save(uft_trsdos_ctx_t *ctx,
                                  uint8_t *data, size_t *size);

/*===========================================================================
 * Detection API
 *===========================================================================*/

/**
 * @brief Detect TRSDOS filesystem
 * @param data Image data
 * @param size Image size
 * @param result Detection result (output)
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_detect(const uint8_t *data, size_t size,
                                    uft_trsdos_detect_t *result);

/**
 * @brief Get geometry by type
 * @param type Geometry type
 * @return Geometry structure or NULL
 */
const uft_trsdos_geometry_t *uft_trsdos_get_geometry(uft_trsdos_geom_type_t type);

/**
 * @brief Detect geometry by file size
 * @param size File size
 * @param confidence Output confidence (0-100)
 * @return Geometry type
 */
uft_trsdos_geom_type_t uft_trsdos_detect_geometry(size_t size, uint8_t *confidence);

/*===========================================================================
 * Sector Access API
 *===========================================================================*/

/**
 * @brief Read sector
 * @param ctx Context
 * @param track Track number
 * @param side Side (0 or 1)
 * @param sector Sector number
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_read_sector(const uft_trsdos_ctx_t *ctx,
                                         uint8_t track, uint8_t side,
                                         uint8_t sector,
                                         uint8_t *buffer, size_t size);

/**
 * @brief Write sector
 * @param ctx Context
 * @param track Track number
 * @param side Side
 * @param sector Sector number
 * @param data Data to write
 * @param size Data size
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_write_sector(uft_trsdos_ctx_t *ctx,
                                          uint8_t track, uint8_t side,
                                          uint8_t sector,
                                          const uint8_t *data, size_t size);

/*===========================================================================
 * Granule Allocation API
 *===========================================================================*/

/**
 * @brief Read GAT from disk
 * @param ctx Context
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_read_gat(uft_trsdos_ctx_t *ctx);

/**
 * @brief Write GAT to disk
 * @param ctx Context
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_write_gat(uft_trsdos_ctx_t *ctx);

/**
 * @brief Check if granule is allocated
 * @param ctx Context
 * @param granule Granule number
 * @return true if allocated
 */
bool uft_trsdos_granule_allocated(const uft_trsdos_ctx_t *ctx, uint8_t granule);

/**
 * @brief Allocate a free granule
 * @param ctx Context
 * @return Granule number or 0xFF if full
 */
uint8_t uft_trsdos_alloc_granule(uft_trsdos_ctx_t *ctx);

/**
 * @brief Free a granule
 * @param ctx Context
 * @param granule Granule number
 */
void uft_trsdos_free_granule(uft_trsdos_ctx_t *ctx, uint8_t granule);

/**
 * @brief Get free granule count
 * @param ctx Context
 * @return Free granules
 */
uint16_t uft_trsdos_free_granules(const uft_trsdos_ctx_t *ctx);

/**
 * @brief Get free space in bytes
 * @param ctx Context
 * @return Free bytes
 */
uint32_t uft_trsdos_free_space(const uft_trsdos_ctx_t *ctx);

/**
 * @brief Convert granule to track/sector
 * @param ctx Context
 * @param granule Granule number
 * @param track Output track
 * @param first_sector Output first sector
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_granule_to_ts(const uft_trsdos_ctx_t *ctx,
                                           uint8_t granule,
                                           uint8_t *track, uint8_t *first_sector);

/*===========================================================================
 * Directory API
 *===========================================================================*/

/**
 * @brief Read directory listing
 * @param ctx Context
 * @param dir Directory structure (output)
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_read_dir(uft_trsdos_ctx_t *ctx,
                                      uft_trsdos_dir_t *dir);

/**
 * @brief Free directory listing
 * @param dir Directory structure
 */
void uft_trsdos_free_dir(uft_trsdos_dir_t *dir);

/**
 * @brief Find file in directory
 * @param ctx Context
 * @param name Filename
 * @param ext Extension (or NULL)
 * @param entry Output entry
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_find_file(const uft_trsdos_ctx_t *ctx,
                                       const char *name, const char *ext,
                                       uft_trsdos_entry_t *entry);

/**
 * @brief Iterate over directory
 * @param ctx Context
 * @param callback Callback function
 * @param user_data User data for callback
 * @return Number of files processed
 */
int uft_trsdos_foreach(const uft_trsdos_ctx_t *ctx,
                       int (*callback)(const uft_trsdos_entry_t *entry, void *user),
                       void *user_data);

/*===========================================================================
 * File Operations API
 *===========================================================================*/

/**
 * @brief Extract file to buffer
 * @param ctx Context
 * @param name Filename
 * @param ext Extension
 * @param buffer Output buffer
 * @param size Buffer size / output size
 * @param password Password (or NULL)
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_extract(const uft_trsdos_ctx_t *ctx,
                                     const char *name, const char *ext,
                                     uint8_t *buffer, size_t *size,
                                     const char *password);

/**
 * @brief Extract file to file
 * @param ctx Context
 * @param name Filename
 * @param ext Extension
 * @param output_path Output file path
 * @param password Password (or NULL)
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_extract_to_file(const uft_trsdos_ctx_t *ctx,
                                             const char *name, const char *ext,
                                             const char *output_path,
                                             const char *password);

/**
 * @brief Inject file from buffer
 * @param ctx Context
 * @param name Filename
 * @param ext Extension
 * @param data File data
 * @param size Data size
 * @param attrib File attributes (or NULL for defaults)
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_inject(uft_trsdos_ctx_t *ctx,
                                    const char *name, const char *ext,
                                    const uint8_t *data, size_t size,
                                    const uft_trsdos_attrib_t *attrib);

/**
 * @brief Inject file from file
 * @param ctx Context
 * @param name Filename (or NULL to use source name)
 * @param ext Extension (or NULL to use source ext)
 * @param input_path Input file path
 * @param attrib File attributes (or NULL)
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_inject_from_file(uft_trsdos_ctx_t *ctx,
                                              const char *name, const char *ext,
                                              const char *input_path,
                                              const uft_trsdos_attrib_t *attrib);

/**
 * @brief Delete file
 * @param ctx Context
 * @param name Filename
 * @param ext Extension
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_delete(uft_trsdos_ctx_t *ctx,
                                    const char *name, const char *ext);

/**
 * @brief Rename file
 * @param ctx Context
 * @param old_name Current filename
 * @param old_ext Current extension
 * @param new_name New filename
 * @param new_ext New extension
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_rename(uft_trsdos_ctx_t *ctx,
                                    const char *old_name, const char *old_ext,
                                    const char *new_name, const char *new_ext);

/**
 * @brief Set file attributes
 * @param ctx Context
 * @param name Filename
 * @param ext Extension
 * @param attrib New attributes
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_set_attrib(uft_trsdos_ctx_t *ctx,
                                        const char *name, const char *ext,
                                        const uft_trsdos_attrib_t *attrib);

/**
 * @brief Set file password
 * @param ctx Context
 * @param name Filename
 * @param ext Extension
 * @param password New password (or NULL to remove)
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_set_password(uft_trsdos_ctx_t *ctx,
                                          const char *name, const char *ext,
                                          const char *password);

/*===========================================================================
 * Image Creation API
 *===========================================================================*/

/**
 * @brief Create blank TRSDOS disk image
 * @param version DOS version
 * @param geom Geometry type
 * @param disk_name Disk name (or NULL)
 * @param data Output data pointer
 * @param size Output size
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_create_image(uft_trsdos_version_t version,
                                          uft_trsdos_geom_type_t geom,
                                          const char *disk_name,
                                          uint8_t **data, size_t *size);

/**
 * @brief Format existing image
 * @param ctx Context
 * @param disk_name New disk name
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_format(uft_trsdos_ctx_t *ctx, const char *disk_name);

/*===========================================================================
 * Utilities
 *===========================================================================*/

/**
 * @brief Parse TRSDOS filename
 * @param input Full filename (NAME.EXT or NAME/EXT)
 * @param name Output name (8 chars, space-padded)
 * @param ext Output extension (3 chars, space-padded)
 * @return true if valid
 */
bool uft_trsdos_parse_filename(const char *input, char *name, char *ext);

/**
 * @brief Format filename for display
 * @param name Filename
 * @param ext Extension
 * @param buffer Output buffer (at least 16 bytes)
 */
void uft_trsdos_format_filename(const char *name, const char *ext, char *buffer);

/**
 * @brief Validate filename
 * @param name Filename to validate
 * @return true if valid
 */
bool uft_trsdos_valid_filename(const char *name);

/**
 * @brief Calculate password hash
 * @param password Password string
 * @param hash Output hash (2 bytes)
 */
void uft_trsdos_hash_password(const char *password, uint8_t *hash);

/**
 * @brief Verify password
 * @param password Password to check
 * @param hash Expected hash
 * @return true if match
 */
bool uft_trsdos_verify_password(const char *password, const uint8_t *hash);

/**
 * @brief Print directory listing
 * @param ctx Context
 */
void uft_trsdos_print_dir(const uft_trsdos_ctx_t *ctx);

/**
 * @brief Print disk info
 * @param ctx Context
 */
void uft_trsdos_print_info(const uft_trsdos_ctx_t *ctx);

/**
 * @brief Export directory as JSON
 * @param ctx Context
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes written or -1 on error
 */
int uft_trsdos_to_json(const uft_trsdos_ctx_t *ctx, char *buffer, size_t size);

/**
 * @brief Get version name
 * @param version Version type
 * @return Version name string
 */
const char *uft_trsdos_version_name(uft_trsdos_version_t version);

/**
 * @brief Get error message
 * @param err Error code
 * @return Error message
 */
const char *uft_trsdos_strerror(uft_trsdos_err_t err);

/*===========================================================================
 * Validation and Recovery
 *===========================================================================*/

/**
 * @brief Validate disk structure
 * @param ctx Context
 * @param fix Attempt to fix errors
 * @param report Output report buffer
 * @param report_size Report buffer size
 * @return Number of errors found
 */
int uft_trsdos_validate(uft_trsdos_ctx_t *ctx, bool fix,
                        char *report, size_t report_size);

/**
 * @brief Check for cross-linked files
 * @param ctx Context
 * @return Number of cross-links found
 */
int uft_trsdos_check_crosslinks(const uft_trsdos_ctx_t *ctx);

/**
 * @brief Rebuild GAT from directory
 * @param ctx Context
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_rebuild_gat(uft_trsdos_ctx_t *ctx);

/**
 * @brief List deleted files
 * @param ctx Context
 * @param dir Directory listing (output)
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_list_deleted(const uft_trsdos_ctx_t *ctx,
                                          uft_trsdos_dir_t *dir);

/**
 * @brief Attempt to recover deleted file
 * @param ctx Context
 * @param entry Deleted entry
 * @return Error code
 */
uft_trsdos_err_t uft_trsdos_recover_deleted(uft_trsdos_ctx_t *ctx,
                                             const uft_trsdos_entry_t *entry);

/*===========================================================================
 * RS-DOS / CoCo Specific
 *===========================================================================*/

/**
 * @brief RS-DOS file types
 */
typedef enum {
    UFT_RSDOS_TYPE_BASIC = 0,       /**< BASIC program */
    UFT_RSDOS_TYPE_DATA = 1,        /**< Data file */
    UFT_RSDOS_TYPE_ML = 2,          /**< Machine language */
    UFT_RSDOS_TYPE_TEXT = 3,        /**< Text file */
} uft_rsdos_type_t;

/**
 * @brief Check if RS-DOS disk
 * @param ctx Context
 * @return true if RS-DOS
 */
bool uft_trsdos_is_rsdos(const uft_trsdos_ctx_t *ctx);

/**
 * @brief Get RS-DOS file type
 * @param entry File entry
 * @return File type
 */
uft_rsdos_type_t uft_rsdos_get_type(const uft_trsdos_entry_t *entry);

/**
 * @brief Set RS-DOS file type
 * @param ctx Context
 * @param name Filename
 * @param ext Extension
 * @param type File type
 * @return Error code
 */
uft_trsdos_err_t uft_rsdos_set_type(uft_trsdos_ctx_t *ctx,
                                     const char *name, const char *ext,
                                     uft_rsdos_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRSDOS_H */
