/**
 * @file uft_hfs_extended.h
 * @brief Apple HFS/HFS+ Extended Support
 * 
 * EXT-012: Extended HFS/HFS+ filesystem support
 * 
 * Features:
 * - HFS (Hierarchical File System) - Classic Mac
 * - HFS+ (HFS Extended/Mac OS Extended)
 * - Case-sensitive variant
 * - Journaling support (read-only)
 * - Resource fork extraction
 */

#ifndef UFT_HFS_EXTENDED_H
#define UFT_HFS_EXTENDED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/* Volume signatures */
#define UFT_HFS_SIGNATURE       0x4244      /**< 'BD' - HFS */
#define UFT_HFSPLUS_SIGNATURE   0x482B      /**< 'H+' - HFS+ */
#define UFT_HFSX_SIGNATURE      0x4858      /**< 'HX' - HFSX (case-sensitive) */

/* Special file IDs */
#define UFT_HFS_CNID_ROOT       2           /**< Root folder */
#define UFT_HFS_CNID_EXTENTS    3           /**< Extents B-tree */
#define UFT_HFS_CNID_CATALOG    4           /**< Catalog B-tree */
#define UFT_HFS_CNID_BADBLOCKS  5           /**< Bad blocks file */
#define UFT_HFS_CNID_ALLOC      6           /**< Allocation bitmap */
#define UFT_HFS_CNID_STARTUP    7           /**< Startup file */
#define UFT_HFS_CNID_ATTRIBUTES 8           /**< Attributes B-tree */

/* File types */
#define UFT_HFS_FOLDER          0x0001
#define UFT_HFS_FILE            0x0002
#define UFT_HFS_FOLDER_THREAD   0x0003
#define UFT_HFS_FILE_THREAD     0x0004

/* Attribute flags */
#define UFT_HFS_FLAG_LOCKED         0x0001
#define UFT_HFS_FLAG_HAS_THREAD     0x0002
#define UFT_HFS_FLAG_HAS_INLINE     0x0004
#define UFT_HFS_FLAG_HAS_RESOURCE   0x0008

/* Max values */
#define UFT_HFS_MAX_FILENAME    255
#define UFT_HFS_MAX_PATH        1024

/*===========================================================================
 * Enumerations
 *===========================================================================*/

typedef enum {
    UFT_HFS_TYPE_HFS = 0,           /**< Original HFS */
    UFT_HFS_TYPE_HFSPLUS,           /**< HFS+ */
    UFT_HFS_TYPE_HFSX,              /**< HFS+ case-sensitive */
    UFT_HFS_TYPE_WRAPPED            /**< HFS+ wrapped in HFS */
} uft_hfs_type_t;

typedef enum {
    UFT_HFS_JOURNAL_NONE = 0,
    UFT_HFS_JOURNAL_ENABLED,
    UFT_HFS_JOURNAL_NEEDS_REPLAY
} uft_hfs_journal_state_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief HFS extent descriptor
 */
typedef struct __attribute__((packed)) {
    uint16_t start_block;           /**< First allocation block */
    uint16_t block_count;           /**< Number of blocks */
} uft_hfs_extent_t;

/**
 * @brief HFS+ extent descriptor
 */
typedef struct __attribute__((packed)) {
    uint32_t start_block;           /**< First allocation block */
    uint32_t block_count;           /**< Number of blocks */
} uft_hfsplus_extent_t;

/**
 * @brief HFS+ fork data
 */
typedef struct __attribute__((packed)) {
    uint64_t logical_size;          /**< Logical size in bytes */
    uint32_t clump_size;            /**< Clump size for allocation */
    uint32_t total_blocks;          /**< Total blocks in fork */
    uft_hfsplus_extent_t extents[8];/**< First 8 extents */
} uft_hfsplus_fork_t;

/**
 * @brief HFS volume header (Master Directory Block)
 */
typedef struct __attribute__((packed)) {
    uint16_t signature;             /**< 0x4244 'BD' */
    uint32_t create_date;           /**< Creation date */
    uint32_t modify_date;           /**< Modification date */
    uint16_t attributes;            /**< Volume attributes */
    uint16_t root_files;            /**< Files in root directory */
    uint16_t volume_bitmap;         /**< First bitmap block */
    uint16_t next_allocation;       /**< Start of next allocation search */
    uint16_t allocation_blocks;     /**< Number of allocation blocks */
    uint32_t block_size;            /**< Size of allocation blocks */
    uint32_t clump_size;            /**< Default clump size */
    uint16_t alloc_block_start;     /**< First allocation block in volume */
    uint32_t next_cnid;             /**< Next unused CNID */
    uint16_t free_blocks;           /**< Number of free allocation blocks */
    uint8_t  volume_name[28];       /**< Volume name (Pascal string) */
    uint32_t backup_date;           /**< Last backup date */
    uint16_t backup_seq;            /**< Backup sequence number */
    uint32_t write_count;           /**< Volume write count */
    uint32_t extents_clump;         /**< Extents file clump size */
    uint32_t catalog_clump;         /**< Catalog file clump size */
    uint16_t root_dirs;             /**< Directories in root */
    uint32_t file_count;            /**< Total files */
    uint32_t dir_count;             /**< Total directories */
    uint32_t finder_info[8];        /**< Finder info */
    /* Extents and catalog info follow */
} uft_hfs_mdb_t;

/**
 * @brief HFS+ volume header
 */
typedef struct __attribute__((packed)) {
    uint16_t signature;             /**< 0x482B 'H+' or 0x4858 'HX' */
    uint16_t version;               /**< Volume format version */
    uint32_t attributes;            /**< Volume attributes */
    uint32_t last_mounted_version;  /**< Last mount version */
    uint32_t journal_info_block;    /**< Journal info block (0 if none) */
    
    uint32_t create_date;           /**< Creation date */
    uint32_t modify_date;           /**< Modification date */
    uint32_t backup_date;           /**< Backup date */
    uint32_t checked_date;          /**< Last fsck date */
    
    uint32_t file_count;            /**< Total files */
    uint32_t folder_count;          /**< Total folders */
    
    uint32_t block_size;            /**< Allocation block size */
    uint32_t total_blocks;          /**< Total allocation blocks */
    uint32_t free_blocks;           /**< Free allocation blocks */
    
    uint32_t next_allocation;       /**< Start of next allocation search */
    uint32_t rsrc_clump_size;       /**< Default resource fork clump */
    uint32_t data_clump_size;       /**< Default data fork clump */
    uint32_t next_cnid;             /**< Next catalog node ID */
    
    uint32_t write_count;           /**< Write count */
    uint64_t encodings_bitmap;      /**< Encodings used */
    
    uint32_t finder_info[8];        /**< Finder info */
    
    uft_hfsplus_fork_t allocation_file;  /**< Allocation bitmap */
    uft_hfsplus_fork_t extents_file;     /**< Extents overflow */
    uft_hfsplus_fork_t catalog_file;     /**< Catalog B-tree */
    uft_hfsplus_fork_t attributes_file;  /**< Attributes B-tree */
    uft_hfsplus_fork_t startup_file;     /**< Boot loader */
} uft_hfsplus_header_t;

/**
 * @brief File info
 */
typedef struct {
    uint32_t cnid;                  /**< Catalog Node ID */
    uint32_t parent_cnid;           /**< Parent folder CNID */
    
    char name[UFT_HFS_MAX_FILENAME + 1];
    char path[UFT_HFS_MAX_PATH];
    
    bool is_folder;
    bool is_locked;
    bool is_invisible;
    
    /* Size info */
    uint64_t data_size;             /**< Data fork size */
    uint64_t resource_size;         /**< Resource fork size */
    uint32_t data_blocks;
    uint32_t resource_blocks;
    
    /* Dates (Mac epoch: Jan 1, 1904) */
    uint32_t create_date;
    uint32_t modify_date;
    uint32_t access_date;
    uint32_t backup_date;
    
    /* Type/Creator */
    uint32_t file_type;             /**< Mac file type */
    uint32_t file_creator;          /**< Mac creator code */
    
    /* Permissions (HFS+) */
    uint32_t owner_id;
    uint32_t group_id;
    uint16_t permissions;
} uft_hfs_file_t;

/**
 * @brief Volume context
 */
typedef struct {
    uft_hfs_type_t type;
    
    const uint8_t *data;
    size_t size;
    
    /* Volume info */
    char volume_name[256];
    uint32_t block_size;
    uint64_t total_size;
    uint64_t free_size;
    
    /* Counts */
    uint32_t file_count;
    uint32_t folder_count;
    
    /* Journal */
    uft_hfs_journal_state_t journal_state;
    
    /* Internal */
    union {
        uft_hfs_mdb_t hfs;
        uft_hfsplus_header_t hfsplus;
    } header;
} uft_hfs_ctx_t;

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

/**
 * @brief Detect HFS/HFS+ filesystem
 */
uft_hfs_type_t uft_hfs_detect(const uint8_t *data, size_t size);

/**
 * @brief Open HFS volume
 */
int uft_hfs_open(uft_hfs_ctx_t *ctx, const uint8_t *data, size_t size);

/**
 * @brief Close context
 */
void uft_hfs_close(uft_hfs_ctx_t *ctx);

/**
 * @brief Get volume info
 */
int uft_hfs_get_info(const uft_hfs_ctx_t *ctx, char *buffer, size_t size);

/**
 * @brief List directory contents
 */
int uft_hfs_list_dir(uft_hfs_ctx_t *ctx, uint32_t parent_cnid,
                     uft_hfs_file_t *files, size_t max_files, size_t *count);

/**
 * @brief Find file by path
 */
int uft_hfs_find(uft_hfs_ctx_t *ctx, const char *path, uft_hfs_file_t *file);

/**
 * @brief Read data fork
 */
int uft_hfs_read_data(uft_hfs_ctx_t *ctx, uint32_t cnid,
                      uint8_t *buffer, size_t max_size, size_t *bytes_read);

/**
 * @brief Read resource fork
 */
int uft_hfs_read_resource(uft_hfs_ctx_t *ctx, uint32_t cnid,
                          uint8_t *buffer, size_t max_size, size_t *bytes_read);

/**
 * @brief Extract file with both forks
 */
int uft_hfs_extract_file(uft_hfs_ctx_t *ctx, uint32_t cnid,
                         const char *data_path, const char *rsrc_path);

/**
 * @brief Extract all files
 */
int uft_hfs_extract_all(uft_hfs_ctx_t *ctx, const char *output_dir,
                        bool include_resources);

/**
 * @brief Convert Mac date to Unix timestamp
 */
time_t uft_hfs_mac_to_unix_time(uint32_t mac_date);

/**
 * @brief Get type string (e.g., "TEXT")
 */
void uft_hfs_type_to_string(uint32_t type, char *str);

/**
 * @brief Export volume catalog
 */
int uft_hfs_catalog_json(const uft_hfs_ctx_t *ctx, char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HFS_EXTENDED_H */
