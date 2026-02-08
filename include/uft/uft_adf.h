/**
 * @file uft_adf.h
 * @brief Amiga ADF (Amiga Disk File) Support for UFT
 * 
 * Supports reading, writing, and analyzing Amiga floppy disk images
 * in ADF format with OFS/FFS filesystem parsing.
 * 
 * @copyright UFT Project 2025
 * @see http://lclevy.free.fr/adflib/adf_info.html
 */

#ifndef UFT_ADF_H
#define UFT_ADF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>  /* ssize_t */

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** ADF Sector size (always 512 bytes) */
#define UFT_ADF_SECTOR_SIZE         512

/** DD (Double Density) disk parameters */
#define UFT_ADF_DD_TRACKS           80
#define UFT_ADF_DD_HEADS            2
#define UFT_ADF_DD_SECTORS          11
#define UFT_ADF_DD_TOTAL_SECTORS    1760
#define UFT_ADF_DD_SIZE             901120      /* 880 KB */

/** HD (High Density) disk parameters */
#define UFT_ADF_HD_TRACKS           80
#define UFT_ADF_HD_HEADS            2
#define UFT_ADF_HD_SECTORS          22
#define UFT_ADF_HD_TOTAL_SECTORS    3520
#define UFT_ADF_HD_SIZE             1802240     /* 1760 KB */

/** Root block location */
#define UFT_ADF_DD_ROOT_BLOCK       880         /* Middle of DD disk */
#define UFT_ADF_HD_ROOT_BLOCK       1760        /* Middle of HD disk */

/** Boot block */
#define UFT_ADF_BOOTBLOCK_SIZE      1024        /* 2 sectors */

/** Block types */
#define UFT_ADF_T_HEADER            2
#define UFT_ADF_T_DATA              8
#define UFT_ADF_T_LIST              16
#define UFT_ADF_T_DIRCACHE          33

/** Secondary types */
#define UFT_ADF_ST_ROOT             1
#define UFT_ADF_ST_DIR              2
#define UFT_ADF_ST_FILE             (-3)
#define UFT_ADF_ST_SOFTLINK         3
#define UFT_ADF_ST_HARDLINK         (-4)

/** Filesystem signatures */
#define UFT_ADF_DOS0                0x444F5300  /* "DOS\0" - OFS */
#define UFT_ADF_DOS1                0x444F5301  /* "DOS\1" - FFS */
#define UFT_ADF_DOS2                0x444F5302  /* "DOS\2" - OFS Intl */
#define UFT_ADF_DOS3                0x444F5303  /* "DOS\3" - FFS Intl */
#define UFT_ADF_DOS4                0x444F5304  /* "DOS\4" - OFS DC */
#define UFT_ADF_DOS5                0x444F5305  /* "DOS\5" - FFS DC */
#define UFT_ADF_DOS6                0x444F5306  /* "DOS\6" - OFS LNFS */
#define UFT_ADF_DOS7                0x444F5307  /* "DOS\7" - FFS LNFS */

/** Maximum filename length */
#define UFT_ADF_MAX_NAME            30

/** Maximum comment length */
#define UFT_ADF_MAX_COMMENT         79

/** Hash table size */
#define UFT_ADF_HT_SIZE             72

/** Data block pointers per file header/extension */
#define UFT_ADF_MAX_DATABLK         72

/*============================================================================
 * Types
 *============================================================================*/

/** Disk density type */
typedef enum {
    UFT_ADF_DENSITY_DD = 0,         /**< Double Density (880KB) */
    UFT_ADF_DENSITY_HD = 1          /**< High Density (1.76MB) */
} uft_adf_density_t;

/** Filesystem type */
typedef enum {
    UFT_ADF_FS_UNKNOWN = 0,
    UFT_ADF_FS_OFS,                 /**< Old File System */
    UFT_ADF_FS_FFS,                 /**< Fast File System */
    UFT_ADF_FS_OFS_INTL,            /**< OFS International */
    UFT_ADF_FS_FFS_INTL,            /**< FFS International */
    UFT_ADF_FS_OFS_DC,              /**< OFS Dir Cache */
    UFT_ADF_FS_FFS_DC,              /**< FFS Dir Cache */
    UFT_ADF_FS_OFS_LNFS,            /**< OFS Long Names */
    UFT_ADF_FS_FFS_LNFS             /**< FFS Long Names */
} uft_adf_fs_type_t;

/** Boot block structure (1024 bytes) */
typedef struct {
    uint32_t    dos_type;           /**< "DOS" + type byte */
    uint32_t    checksum;           /**< Boot block checksum */
    uint32_t    root_block;         /**< Root block pointer */
    uint8_t     bootcode[1012];     /**< Boot code (optional) */
} uft_adf_bootblock_t;

/** Root block structure */
typedef struct {
    uint32_t    type;               /**< T_HEADER (2) */
    uint32_t    header_key;         /**< Unused (0) */
    uint32_t    high_seq;           /**< Unused (0) */
    uint32_t    ht_size;            /**< Hash table size (72) */
    uint32_t    first_data;         /**< Unused (0) */
    uint32_t    checksum;           /**< Block checksum */
    uint32_t    ht[UFT_ADF_HT_SIZE]; /**< Hash table */
    uint32_t    bm_flag;            /**< Bitmap valid flag (-1) */
    uint32_t    bm_pages[25];       /**< Bitmap block pointers */
    uint32_t    bm_ext;             /**< Bitmap extension block */
    uint32_t    r_days;             /**< Root alteration days */
    uint32_t    r_mins;             /**< Root alteration mins */
    uint32_t    r_ticks;            /**< Root alteration ticks */
    uint8_t     name_len;           /**< Disk name length */
    char        name[30];           /**< Disk name (BCPL string) */
    uint8_t     unused1;
    uint32_t    unused2[2];
    uint32_t    v_days;             /**< Volume alteration days */
    uint32_t    v_mins;             /**< Volume alteration mins */
    uint32_t    v_ticks;            /**< Volume alteration ticks */
    uint32_t    c_days;             /**< Creation days */
    uint32_t    c_mins;             /**< Creation mins */
    uint32_t    c_ticks;            /**< Creation ticks */
    uint32_t    next_hash;          /**< Unused (0) */
    uint32_t    parent;             /**< Unused (0) */
    uint32_t    extension;          /**< FFS: extension block */
    uint32_t    sec_type;           /**< ST_ROOT (1) */
} uft_adf_rootblock_t;

/** File header block structure */
typedef struct {
    uint32_t    type;               /**< T_HEADER (2) */
    uint32_t    header_key;         /**< Self pointer */
    uint32_t    high_seq;           /**< Number of data blocks */
    uint32_t    data_size;          /**< Unused (0) for OFS */
    uint32_t    first_data;         /**< First data block */
    uint32_t    checksum;           /**< Block checksum */
    uint32_t    data_blocks[UFT_ADF_MAX_DATABLK]; /**< Data block ptrs */
    uint32_t    unused1;
    uint16_t    uid;                /**< User ID */
    uint16_t    gid;                /**< Group ID */
    uint32_t    protect;            /**< Protection bits */
    uint32_t    byte_size;          /**< File size in bytes */
    uint8_t     comm_len;           /**< Comment length */
    char        comment[79];        /**< Comment (BCPL string) */
    uint8_t     unused2[12];
    uint32_t    days;               /**< Last change days */
    uint32_t    mins;               /**< Last change mins */
    uint32_t    ticks;              /**< Last change ticks */
    uint8_t     name_len;           /**< Name length */
    char        name[30];           /**< File name (BCPL string) */
    uint8_t     unused3;
    uint32_t    unused4;
    uint32_t    real_entry;         /**< Hard link: real entry */
    uint32_t    next_link;          /**< Hard link: next link */
    uint32_t    unused5[5];
    uint32_t    hash_chain;         /**< Next entry in hash chain */
    uint32_t    parent;             /**< Parent directory */
    uint32_t    extension;          /**< Extension block */
    int32_t     sec_type;           /**< ST_FILE (-3) */
} uft_adf_fileheader_t;

/** Directory entry (simplified for API) */
typedef struct {
    char        name[UFT_ADF_MAX_NAME + 1];  /**< Entry name */
    uint32_t    block;              /**< Header block number */
    uint32_t    size;               /**< File size (0 for dirs) */
    uint32_t    protect;            /**< Protection bits */
    bool        is_dir;             /**< true if directory */
    bool        is_link;            /**< true if link */
    time_t      mtime;              /**< Modification time */
    char        comment[UFT_ADF_MAX_COMMENT + 1]; /**< Comment */
} uft_adf_entry_t;

/** Volume information */
typedef struct {
    char            name[UFT_ADF_MAX_NAME + 1]; /**< Volume name */
    uft_adf_density_t density;      /**< DD or HD */
    uft_adf_fs_type_t fs_type;      /**< Filesystem type */
    uint32_t        total_blocks;   /**< Total blocks */
    uint32_t        free_blocks;    /**< Free blocks */
    uint32_t        used_blocks;    /**< Used blocks */
    time_t          create_time;    /**< Creation time */
    time_t          modify_time;    /**< Last modification */
    bool            has_bootcode;   /**< Has boot code */
    bool            is_bootable;    /**< Valid boot checksum */
} uft_adf_info_t;

/** Opaque volume handle */
typedef struct uft_adf_volume uft_adf_volume_t;

/** Directory iterator */
typedef struct uft_adf_dir_iter uft_adf_dir_iter_t;

/*============================================================================
 * Volume Operations
 *============================================================================*/

/**
 * @brief Open an ADF image file
 * @param path Path to ADF file
 * @param readonly Open read-only if true
 * @return Volume handle or NULL on error
 */
uft_adf_volume_t *uft_adf_open(const char *path, bool readonly);

/**
 * @brief Open an ADF from memory buffer
 * @param data Pointer to ADF data
 * @param size Size of data
 * @return Volume handle or NULL on error
 */
uft_adf_volume_t *uft_adf_open_memory(const void *data, size_t size);

/**
 * @brief Close an ADF volume
 * @param vol Volume handle
 */
void uft_adf_close(uft_adf_volume_t *vol);

/**
 * @brief Get volume information
 * @param vol Volume handle
 * @param info Pointer to info structure
 * @return 0 on success, -1 on error
 */
int uft_adf_get_info(uft_adf_volume_t *vol, uft_adf_info_t *info);

/**
 * @brief Get filesystem type as string
 * @param type Filesystem type
 * @return Human-readable string
 */
const char *uft_adf_fs_type_string(uft_adf_fs_type_t type);

/**
 * @brief Detect density from file size
 * @param size File size in bytes
 * @return Density type or -1 if invalid
 */
int uft_adf_detect_density(size_t size);

/*============================================================================
 * Directory Operations
 *============================================================================*/

/**
 * @brief Open root directory for iteration
 * @param vol Volume handle
 * @return Iterator or NULL on error
 */
uft_adf_dir_iter_t *uft_adf_opendir(uft_adf_volume_t *vol);

/**
 * @brief Open directory by block number
 * @param vol Volume handle
 * @param block Directory block number (0 for root)
 * @return Iterator or NULL on error
 */
uft_adf_dir_iter_t *uft_adf_opendir_block(uft_adf_volume_t *vol, 
                                           uint32_t block);

/**
 * @brief Open directory by path
 * @param vol Volume handle
 * @param path Directory path (Unix-style)
 * @return Iterator or NULL on error
 */
uft_adf_dir_iter_t *uft_adf_opendir_path(uft_adf_volume_t *vol,
                                          const char *path);

/**
 * @brief Read next directory entry
 * @param iter Directory iterator
 * @param entry Pointer to entry structure
 * @return 0 on success, 1 if no more entries, -1 on error
 */
int uft_adf_readdir(uft_adf_dir_iter_t *iter, uft_adf_entry_t *entry);

/**
 * @brief Close directory iterator
 * @param iter Directory iterator
 */
void uft_adf_closedir(uft_adf_dir_iter_t *iter);

/**
 * @brief Lookup entry by path
 * @param vol Volume handle
 * @param path File/directory path
 * @param entry Pointer to entry structure
 * @return 0 on success, -1 if not found
 */
int uft_adf_lookup(uft_adf_volume_t *vol, const char *path,
                   uft_adf_entry_t *entry);

/*============================================================================
 * File Operations
 *============================================================================*/

/**
 * @brief Read file data
 * @param vol Volume handle
 * @param block File header block
 * @param offset Offset into file
 * @param buffer Output buffer
 * @param size Bytes to read
 * @return Bytes read, or -1 on error
 */
ssize_t uft_adf_read_file(uft_adf_volume_t *vol, uint32_t block,
                          uint32_t offset, void *buffer, size_t size);

/**
 * @brief Extract file to disk
 * @param vol Volume handle
 * @param src_path Source path in ADF
 * @param dst_path Destination path on disk
 * @return 0 on success, -1 on error
 */
int uft_adf_extract(uft_adf_volume_t *vol, const char *src_path,
                    const char *dst_path);

/**
 * @brief Extract all files to directory
 * @param vol Volume handle
 * @param dst_dir Destination directory
 * @return Number of files extracted, or -1 on error
 */
int uft_adf_extract_all(uft_adf_volume_t *vol, const char *dst_dir);

/*============================================================================
 * Write Operations
 *============================================================================*/

/**
 * @brief Create a new ADF image
 * @param path Output file path
 * @param density DD or HD
 * @param fs_type Filesystem type
 * @param name Volume name
 * @return 0 on success, -1 on error
 */
int uft_adf_create(const char *path, uft_adf_density_t density,
                   uft_adf_fs_type_t fs_type, const char *name);

/**
 * @brief Format an existing ADF
 * @param vol Volume handle
 * @param fs_type Filesystem type
 * @param name Volume name
 * @return 0 on success, -1 on error
 */
int uft_adf_format(uft_adf_volume_t *vol, uft_adf_fs_type_t fs_type,
                   const char *name);

/**
 * @brief Add file to ADF
 * @param vol Volume handle
 * @param src_path Source file path
 * @param dst_path Destination path in ADF
 * @return 0 on success, -1 on error
 */
int uft_adf_add_file(uft_adf_volume_t *vol, const char *src_path,
                     const char *dst_path);

/**
 * @brief Create directory
 * @param vol Volume handle
 * @param path Directory path
 * @return 0 on success, -1 on error
 */
int uft_adf_mkdir(uft_adf_volume_t *vol, const char *path);

/**
 * @brief Delete file or empty directory
 * @param vol Volume handle
 * @param path Path to delete
 * @return 0 on success, -1 on error
 */
int uft_adf_delete(uft_adf_volume_t *vol, const char *path);

/**
 * @brief Rename file or directory
 * @param vol Volume handle
 * @param old_path Current path
 * @param new_name New name (not path)
 * @return 0 on success, -1 on error
 */
int uft_adf_rename(uft_adf_volume_t *vol, const char *old_path,
                   const char *new_name);

/*============================================================================
 * Boot Block Operations
 *============================================================================*/

/**
 * @brief Check if disk is bootable
 * @param vol Volume handle
 * @return true if bootable
 */
bool uft_adf_is_bootable(uft_adf_volume_t *vol);

/**
 * @brief Install boot block
 * @param vol Volume handle
 * @param bootcode Boot code data (max 1012 bytes)
 * @param size Boot code size
 * @return 0 on success, -1 on error
 */
int uft_adf_install_bootblock(uft_adf_volume_t *vol,
                              const void *bootcode, size_t size);

/**
 * @brief Extract boot block
 * @param vol Volume handle
 * @param bootcode Output buffer (1012 bytes)
 * @return Boot code size, or 0 if no boot code
 */
size_t uft_adf_get_bootblock(uft_adf_volume_t *vol, void *bootcode);

/*============================================================================
 * Low-Level Operations
 *============================================================================*/

/**
 * @brief Read raw block
 * @param vol Volume handle
 * @param block Block number
 * @param buffer Output buffer (512 bytes)
 * @return 0 on success, -1 on error
 */
int uft_adf_read_block(uft_adf_volume_t *vol, uint32_t block,
                       void *buffer);

/**
 * @brief Write raw block
 * @param vol Volume handle
 * @param block Block number
 * @param buffer Input buffer (512 bytes)
 * @return 0 on success, -1 on error
 */
int uft_adf_write_block(uft_adf_volume_t *vol, uint32_t block,
                        const void *buffer);

/**
 * @brief Calculate block checksum
 * @param block Block data (512 bytes)
 * @return Checksum value
 */
uint32_t uft_adf_checksum(const void *block);

/**
 * @brief Verify block checksum
 * @param block Block data
 * @return true if valid
 */
bool uft_adf_verify_checksum(const void *block);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Convert Amiga date to Unix time
 * @param days Days since 1978-01-01
 * @param mins Minutes since midnight
 * @param ticks Ticks (1/50 second)
 * @return Unix timestamp
 */
time_t uft_adf_to_unix_time(uint32_t days, uint32_t mins, uint32_t ticks);

/**
 * @brief Convert Unix time to Amiga date
 * @param t Unix timestamp
 * @param days Output: days since 1978-01-01
 * @param mins Output: minutes since midnight
 * @param ticks Output: ticks
 */
void uft_unix_to_adf_time(time_t t, uint32_t *days, uint32_t *mins,
                          uint32_t *ticks);

/**
 * @brief Calculate filename hash
 * @param name Filename
 * @param intl Use international mode
 * @return Hash value (0-71)
 */
uint32_t uft_adf_hash_name(const char *name, bool intl);

/**
 * @brief Decode protection bits to string
 * @param protect Protection bits
 * @param buffer Output buffer (min 9 bytes)
 * @return buffer pointer
 */
char *uft_adf_protect_string(uint32_t protect, char *buffer);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ADF_H */
