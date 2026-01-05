/**
 * @file uft_amigados.h
 * @brief AmigaDOS Filesystem - Complete OFS/FFS Implementation
 * 
 * PHASE 2 Priority 2: AmigaDOS Filesystem Layer
 * 
 * Supported formats:
 * - OFS (Original File System) - AmigaOS 1.x
 * - FFS (Fast File System) - AmigaOS 2.x+
 * - OFS/FFS + International Mode (INTL)
 * - OFS/FFS + Directory Cache (DCACHE)
 * - OFS/FFS + Long Filenames (FFS2)
 * 
 * Disk types:
 * - DD (Double Density): 880KB, 1760 blocks
 * - HD (High Density): 1760KB, 3520 blocks
 * - Custom geometries
 * 
 * Features:
 * - Block-level access with checksum verification
 * - Directory parsing with hash table traversal
 * - File extraction with data block chain following
 * - File injection with bitmap allocation
 * - Bitmap management and validation
 * - Hardlink/Softlink support
 * - Comment and protection bits
 * 
 * @version 3.6.0
 * @date 2025
 */

#ifndef UFT_AMIGADOS_H
#define UFT_AMIGADOS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Block size (always 512 bytes on Amiga) */
#define UFT_AMIGA_BLOCK_SIZE        512

/** Standard disk geometries */
#define UFT_AMIGA_DD_BLOCKS         1760    /**< 80×2×11 = 880KB */
#define UFT_AMIGA_HD_BLOCKS         3520    /**< 80×2×22 = 1760KB */
#define UFT_AMIGA_DD_SIZE           (UFT_AMIGA_DD_BLOCKS * UFT_AMIGA_BLOCK_SIZE)
#define UFT_AMIGA_HD_SIZE           (UFT_AMIGA_HD_BLOCKS * UFT_AMIGA_BLOCK_SIZE)

/** Max values */
#define UFT_AMIGA_MAX_FILENAME      30      /**< Standard max filename */
#define UFT_AMIGA_MAX_FILENAME_LFS  107     /**< Long filename support */
#define UFT_AMIGA_MAX_COMMENT       79      /**< Max comment length */
#define UFT_AMIGA_MAX_PATH          1024    /**< Max path length */
#define UFT_AMIGA_HASH_SIZE         72      /**< Hash table entries */
#define UFT_AMIGA_MAX_BITMAP_BLOCKS 25      /**< Max bitmap blocks */
#define UFT_AMIGA_MAX_DATA_BLOCKS   72      /**< Data blocks per header (OFS) */
#define UFT_AMIGA_MAX_EXT_BLOCKS    72      /**< Extension blocks per list */

/** Block types (primary type at offset 0) */
#define UFT_AMIGA_T_SHORT           2       /**< Short block (header) */
#define UFT_AMIGA_T_DATA            8       /**< Data block (OFS) */
#define UFT_AMIGA_T_LIST            16      /**< Extension list block */
#define UFT_AMIGA_T_DIRCACHE        33      /**< Directory cache block */

/** Secondary types (at offset 508) */
#define UFT_AMIGA_ST_ROOT           1       /**< Root block */
#define UFT_AMIGA_ST_USERDIR        2       /**< User directory */
#define UFT_AMIGA_ST_SOFTLINK       3       /**< Soft link */
#define UFT_AMIGA_ST_LINKDIR        4       /**< Hard link to directory */
#define UFT_AMIGA_ST_FILE           (-3)    /**< Regular file */
#define UFT_AMIGA_ST_LINKFILE       (-4)    /**< Hard link to file */

/** Filesystem types (from DOS type in bootblock) */
typedef enum {
    UFT_AMIGA_FS_OFS         = 0x00,    /**< DOS0: OFS */
    UFT_AMIGA_FS_FFS         = 0x01,    /**< DOS1: FFS */
    UFT_AMIGA_FS_OFS_INTL    = 0x02,    /**< DOS2: OFS + International */
    UFT_AMIGA_FS_FFS_INTL    = 0x03,    /**< DOS3: FFS + International */
    UFT_AMIGA_FS_OFS_DC      = 0x04,    /**< DOS4: OFS + DirCache */
    UFT_AMIGA_FS_FFS_DC      = 0x05,    /**< DOS5: FFS + DirCache */
    UFT_AMIGA_FS_OFS_LNFS    = 0x06,    /**< DOS6: OFS + Long Names */
    UFT_AMIGA_FS_FFS_LNFS    = 0x07,    /**< DOS7: FFS + Long Names */
    UFT_AMIGA_FS_UNKNOWN     = 0xFF
} uft_amiga_fs_type_t;

/** Protection bits (HSPARWED) */
typedef enum {
    UFT_AMIGA_PROT_DELETE    = 0x0001,  /**< D - Deletable */
    UFT_AMIGA_PROT_EXECUTE   = 0x0002,  /**< E - Executable */
    UFT_AMIGA_PROT_WRITE     = 0x0004,  /**< W - Writable */
    UFT_AMIGA_PROT_READ      = 0x0008,  /**< R - Readable */
    UFT_AMIGA_PROT_ARCHIVE   = 0x0010,  /**< A - Archived */
    UFT_AMIGA_PROT_PURE      = 0x0020,  /**< P - Pure (reentrant) */
    UFT_AMIGA_PROT_SCRIPT    = 0x0040,  /**< S - Script */
    UFT_AMIGA_PROT_HOLD      = 0x0080   /**< H - Hold in memory */
} uft_amiga_protection_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Filesystem detection result
 */
typedef struct {
    bool                is_valid;           /**< Valid AmigaDOS image */
    uft_amiga_fs_type_t fs_type;           /**< Filesystem type */
    bool                is_ffs;            /**< FFS mode (vs OFS) */
    bool                is_intl;           /**< International mode */
    bool                is_dircache;       /**< Directory cache enabled */
    bool                is_longnames;      /**< Long filename support */
    uint32_t            total_blocks;      /**< Total blocks in image */
    uint32_t            root_block;        /**< Root block number */
    uint32_t            bootblock_checksum; /**< Bootblock checksum */
    bool                bootblock_valid;   /**< Bootblock checksum valid */
    char                dos_type[5];       /**< DOS type string (DOSx) */
} uft_amiga_detect_t;

/**
 * @brief Directory entry
 */
typedef struct {
    char                name[UFT_AMIGA_MAX_FILENAME_LFS + 1];
    char                comment[UFT_AMIGA_MAX_COMMENT + 1];
    bool                is_dir;            /**< Is directory */
    bool                is_file;           /**< Is file */
    bool                is_softlink;       /**< Is soft link */
    bool                is_hardlink;       /**< Is hard link */
    uint32_t            header_block;      /**< Header block number */
    uint32_t            parent_block;      /**< Parent directory block */
    uint32_t            hash_chain;        /**< Next in hash chain */
    uint32_t            size;              /**< File size in bytes */
    uint32_t            blocks;            /**< Blocks used */
    uint32_t            first_data;        /**< First data block (OFS) */
    uint32_t            extension;         /**< Extension block (files) */
    uint32_t            protection;        /**< Protection bits */
    time_t              mtime;             /**< Modification time */
    int32_t             secondary_type;    /**< Secondary type */
    uint32_t            real_entry;        /**< For links: real header */
    char                link_target[UFT_AMIGA_MAX_PATH]; /**< Softlink target */
} uft_amiga_entry_t;

/**
 * @brief Directory listing
 */
typedef struct {
    uft_amiga_entry_t  *entries;           /**< Entry array */
    size_t              count;             /**< Number of entries */
    size_t              capacity;          /**< Allocated capacity */
    char                dir_name[UFT_AMIGA_MAX_FILENAME_LFS + 1];
    uint32_t            dir_block;         /**< Directory block */
} uft_amiga_dir_t;

/**
 * @brief Block chain for file data
 */
typedef struct {
    uint32_t           *blocks;            /**< Block numbers */
    size_t              count;             /**< Number of blocks */
    size_t              capacity;          /**< Allocated capacity */
    uint32_t            header_block;      /**< File header block */
    uint32_t            total_size;        /**< Total data size */
    bool                has_extension;     /**< Has extension blocks */
} uft_amiga_chain_t;

/**
 * @brief Bitmap allocation status
 */
typedef struct {
    uint32_t            total_blocks;      /**< Total blocks */
    uint32_t            free_blocks;       /**< Free blocks */
    uint32_t            used_blocks;       /**< Used blocks */
    uint32_t            reserved_blocks;   /**< Reserved (boot, root, bitmap) */
    double              percent_used;      /**< Usage percentage */
    uint32_t            bitmap_blocks[UFT_AMIGA_MAX_BITMAP_BLOCKS];
    size_t              bitmap_count;      /**< Number of bitmap blocks */
} uft_amiga_bitmap_info_t;

/**
 * @brief Validation report
 */
typedef struct {
    bool                is_valid;          /**< Overall validity */
    uint32_t            errors;            /**< Error count */
    uint32_t            warnings;          /**< Warning count */
    uint32_t            files_found;       /**< Files found */
    uint32_t            dirs_found;        /**< Directories found */
    uint32_t            links_found;       /**< Links found */
    
    /* Specific issues */
    bool                bootblock_bad;     /**< Bad bootblock checksum */
    bool                root_bad;          /**< Bad root block */
    bool                bitmap_corrupt;    /**< Bitmap inconsistency */
    uint32_t            orphan_blocks;     /**< Orphaned blocks */
    uint32_t            cross_linked;      /**< Cross-linked blocks */
    uint32_t            broken_chains;     /**< Broken block chains */
    uint32_t            bad_checksums;     /**< Bad block checksums */
    uint32_t            invalid_dates;     /**< Invalid dates */
    
    /* Error messages */
    char              **messages;          /**< Error/warning messages */
    size_t              message_count;     /**< Number of messages */
    size_t              message_capacity;  /**< Message capacity */
} uft_amiga_validation_t;

/**
 * @brief Filesystem context
 */
typedef struct {
    /* Image data */
    uint8_t            *data;              /**< Image data */
    size_t              size;              /**< Image size */
    bool                owns_data;         /**< Free data on close */
    bool                modified;          /**< Image was modified */
    
    /* Filesystem info */
    bool                is_valid;          /**< Valid filesystem */
    uft_amiga_fs_type_t fs_type;           /**< Filesystem type */
    bool                is_ffs;            /**< FFS mode */
    bool                is_intl;           /**< International mode */
    bool                is_dircache;       /**< Directory cache */
    bool                is_longnames;      /**< Long filenames */
    
    /* Geometry */
    uint32_t            total_blocks;      /**< Total blocks */
    uint32_t            root_block;        /**< Root block number */
    uint32_t            bitmap_blocks[UFT_AMIGA_MAX_BITMAP_BLOCKS];
    size_t              bitmap_count;      /**< Bitmap block count */
    
    /* Volume info */
    char                volume_name[UFT_AMIGA_MAX_FILENAME_LFS + 1];
    time_t              creation_date;     /**< Volume creation date */
    time_t              last_modified;     /**< Last modification */
    uint32_t            disk_days;         /**< Days since 1978-01-01 */
    uint32_t            disk_mins;         /**< Minutes of day */
    uint32_t            disk_ticks;        /**< Ticks (1/50 second) */
    
    /* Options */
    bool                verify_checksums;  /**< Verify block checksums */
    bool                auto_fix;          /**< Auto-fix minor issues */
    bool                preserve_dates;    /**< Preserve timestamps */
} uft_amiga_ctx_t;

/**
 * @brief Filesystem options
 */
typedef struct {
    bool                verify_checksums;  /**< Verify checksums on read */
    bool                auto_fix;          /**< Auto-fix minor issues */
    bool                preserve_dates;    /**< Preserve original dates */
    bool                follow_links;      /**< Follow hard/soft links */
    uint32_t            interleave;        /**< Block allocation interleave */
} uft_amiga_options_t;

/*===========================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Create filesystem context
 * @return New context or NULL on failure
 */
uft_amiga_ctx_t *uft_amiga_create(void);

/**
 * @brief Destroy filesystem context
 * @param ctx Context to destroy
 */
void uft_amiga_destroy(uft_amiga_ctx_t *ctx);

/**
 * @brief Open ADF image file
 * @param ctx Context
 * @param filename Path to ADF file
 * @param options Optional settings (NULL for defaults)
 * @return 0 on success, negative on error
 */
int uft_amiga_open_file(uft_amiga_ctx_t *ctx, const char *filename,
                        const uft_amiga_options_t *options);

/**
 * @brief Open ADF from memory buffer
 * @param ctx Context
 * @param data Image data
 * @param size Data size
 * @param copy If true, copy data; if false, use directly
 * @param options Optional settings
 * @return 0 on success, negative on error
 */
int uft_amiga_open_buffer(uft_amiga_ctx_t *ctx, uint8_t *data, size_t size,
                          bool copy, const uft_amiga_options_t *options);

/**
 * @brief Save image to file
 * @param ctx Context
 * @param filename Output path
 * @return 0 on success, negative on error
 */
int uft_amiga_save(const uft_amiga_ctx_t *ctx, const char *filename);

/**
 * @brief Close image (free internal data)
 * @param ctx Context
 */
void uft_amiga_close(uft_amiga_ctx_t *ctx);

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

/**
 * @brief Detect AmigaDOS filesystem in buffer
 * @param data Image data
 * @param size Data size
 * @param result Detection result
 * @return 0 on success (detected), negative if not AmigaDOS
 */
int uft_amiga_detect(const uint8_t *data, size_t size, uft_amiga_detect_t *result);

/**
 * @brief Check if file is ADF image
 * @param filename Path to file
 * @return true if AmigaDOS image
 */
bool uft_amiga_is_adf(const char *filename);

/**
 * @brief Get filesystem type string
 * @param fs_type Filesystem type
 * @return Human-readable string
 */
const char *uft_amiga_fs_type_str(uft_amiga_fs_type_t fs_type);

/*===========================================================================
 * Block Access Functions
 *===========================================================================*/

/**
 * @brief Read block by number
 * @param ctx Context
 * @param block_num Block number
 * @param buffer Output buffer (512 bytes)
 * @return 0 on success, negative on error
 */
int uft_amiga_read_block(const uft_amiga_ctx_t *ctx, uint32_t block_num,
                         uint8_t *buffer);

/**
 * @brief Write block by number
 * @param ctx Context
 * @param block_num Block number
 * @param buffer Input buffer (512 bytes)
 * @return 0 on success, negative on error
 */
int uft_amiga_write_block(uft_amiga_ctx_t *ctx, uint32_t block_num,
                          const uint8_t *buffer);

/**
 * @brief Calculate block checksum
 * @param block Block data (512 bytes)
 * @return Checksum value (should be 0 for valid block)
 */
uint32_t uft_amiga_block_checksum(const uint8_t *block);

/**
 * @brief Update block checksum
 * @param block Block data (512 bytes) - modified in place
 */
void uft_amiga_update_checksum(uint8_t *block);

/**
 * @brief Verify block checksum
 * @param block Block data
 * @return true if valid
 */
bool uft_amiga_verify_checksum(const uint8_t *block);

/*===========================================================================
 * Directory Functions
 *===========================================================================*/

/**
 * @brief Load root directory
 * @param ctx Context
 * @param dir Output directory structure
 * @return 0 on success
 */
int uft_amiga_load_root(const uft_amiga_ctx_t *ctx, uft_amiga_dir_t *dir);

/**
 * @brief Load directory by block number
 * @param ctx Context
 * @param block_num Directory header block
 * @param dir Output directory structure
 * @return 0 on success
 */
int uft_amiga_load_dir(const uft_amiga_ctx_t *ctx, uint32_t block_num,
                       uft_amiga_dir_t *dir);

/**
 * @brief Load directory by path
 * @param ctx Context
 * @param path Directory path (e.g., "Devs/Printers")
 * @param dir Output directory structure
 * @return 0 on success
 */
int uft_amiga_load_dir_path(const uft_amiga_ctx_t *ctx, const char *path,
                            uft_amiga_dir_t *dir);

/**
 * @brief Free directory structure
 * @param dir Directory to free
 */
void uft_amiga_free_dir(uft_amiga_dir_t *dir);

/**
 * @brief Find entry in directory
 * @param ctx Context
 * @param dir_block Directory block (0 for root)
 * @param name Entry name
 * @param entry Output entry
 * @return 0 on success, -1 if not found
 */
int uft_amiga_find_entry(const uft_amiga_ctx_t *ctx, uint32_t dir_block,
                         const char *name, uft_amiga_entry_t *entry);

/**
 * @brief Find entry by path
 * @param ctx Context
 * @param path Full path (e.g., "C/Dir")
 * @param entry Output entry
 * @return 0 on success
 */
int uft_amiga_find_path(const uft_amiga_ctx_t *ctx, const char *path,
                        uft_amiga_entry_t *entry);

/**
 * @brief Calculate hash for filename
 * @param name Filename
 * @param intl International mode
 * @return Hash value (0-71)
 */
uint32_t uft_amiga_hash_name(const char *name, bool intl);

/**
 * @brief Iterate directory entries
 * @param ctx Context
 * @param dir_block Directory block (0 for root)
 * @param callback Callback function
 * @param user_data User data for callback
 * @return 0 on success
 */
typedef int (*uft_amiga_dir_callback_t)(const uft_amiga_entry_t *entry,
                                         void *user_data);
int uft_amiga_foreach_entry(const uft_amiga_ctx_t *ctx, uint32_t dir_block,
                            uft_amiga_dir_callback_t callback, void *user_data);

/**
 * @brief Iterate all files recursively
 * @param ctx Context
 * @param callback Callback function
 * @param user_data User data for callback
 * @return 0 on success
 */
int uft_amiga_foreach_file(const uft_amiga_ctx_t *ctx,
                           uft_amiga_dir_callback_t callback, void *user_data);

/*===========================================================================
 * File Operations
 *===========================================================================*/

/**
 * @brief Get file data block chain
 * @param ctx Context
 * @param file_block File header block
 * @param chain Output chain structure
 * @return 0 on success
 */
int uft_amiga_get_chain(const uft_amiga_ctx_t *ctx, uint32_t file_block,
                        uft_amiga_chain_t *chain);

/**
 * @brief Free chain structure
 * @param chain Chain to free
 */
void uft_amiga_free_chain(uft_amiga_chain_t *chain);

/**
 * @brief Extract file to buffer
 * @param ctx Context
 * @param path File path
 * @param data Output buffer (caller allocates)
 * @param size Buffer size / actual size on return
 * @return 0 on success
 */
int uft_amiga_extract_file(const uft_amiga_ctx_t *ctx, const char *path,
                           uint8_t *data, size_t *size);

/**
 * @brief Extract file to new buffer
 * @param ctx Context
 * @param path File path
 * @param data Output buffer (caller frees)
 * @param size Actual size on return
 * @return 0 on success
 */
int uft_amiga_extract_file_alloc(const uft_amiga_ctx_t *ctx, const char *path,
                                 uint8_t **data, size_t *size);

/**
 * @brief Extract file to disk
 * @param ctx Context
 * @param path File path in image
 * @param dest_path Destination path on disk
 * @return 0 on success
 */
int uft_amiga_extract_to_file(const uft_amiga_ctx_t *ctx, const char *path,
                              const char *dest_path);

/**
 * @brief Inject file into image
 * @param ctx Context
 * @param dest_dir Destination directory (empty for root)
 * @param name Filename
 * @param data File data
 * @param size Data size
 * @return 0 on success
 */
int uft_amiga_inject_file(uft_amiga_ctx_t *ctx, const char *dest_dir,
                          const char *name, const uint8_t *data, size_t size);

/**
 * @brief Inject file from disk
 * @param ctx Context
 * @param dest_dir Destination directory
 * @param src_path Source file path
 * @return 0 on success
 */
int uft_amiga_inject_from_file(uft_amiga_ctx_t *ctx, const char *dest_dir,
                               const char *src_path);

/**
 * @brief Delete file or directory
 * @param ctx Context
 * @param path Path to delete
 * @return 0 on success
 */
int uft_amiga_delete(uft_amiga_ctx_t *ctx, const char *path);

/**
 * @brief Rename file or directory
 * @param ctx Context
 * @param old_path Current path
 * @param new_name New name (not full path)
 * @return 0 on success
 */
int uft_amiga_rename(uft_amiga_ctx_t *ctx, const char *old_path,
                     const char *new_name);

/**
 * @brief Create directory
 * @param ctx Context
 * @param parent_dir Parent directory path
 * @param name Directory name
 * @return 0 on success
 */
int uft_amiga_mkdir(uft_amiga_ctx_t *ctx, const char *parent_dir,
                    const char *name);

/**
 * @brief Set file protection bits
 * @param ctx Context
 * @param path File path
 * @param protection Protection bits
 * @return 0 on success
 */
int uft_amiga_set_protection(uft_amiga_ctx_t *ctx, const char *path,
                             uint32_t protection);

/**
 * @brief Set file comment
 * @param ctx Context
 * @param path File path
 * @param comment Comment string
 * @return 0 on success
 */
int uft_amiga_set_comment(uft_amiga_ctx_t *ctx, const char *path,
                          const char *comment);

/*===========================================================================
 * Bitmap Functions
 *===========================================================================*/

/**
 * @brief Get bitmap information
 * @param ctx Context
 * @param info Output bitmap info
 * @return 0 on success
 */
int uft_amiga_get_bitmap_info(const uft_amiga_ctx_t *ctx,
                              uft_amiga_bitmap_info_t *info);

/**
 * @brief Check if block is free
 * @param ctx Context
 * @param block_num Block number
 * @return true if free
 */
bool uft_amiga_is_block_free(const uft_amiga_ctx_t *ctx, uint32_t block_num);

/**
 * @brief Allocate a free block
 * @param ctx Context
 * @param preferred Preferred block number (0 for any)
 * @return Block number or 0 on failure
 */
uint32_t uft_amiga_alloc_block(uft_amiga_ctx_t *ctx, uint32_t preferred);

/**
 * @brief Free a block
 * @param ctx Context
 * @param block_num Block to free
 * @return 0 on success
 */
int uft_amiga_free_block(uft_amiga_ctx_t *ctx, uint32_t block_num);

/**
 * @brief Allocate contiguous blocks
 * @param ctx Context
 * @param count Number of blocks needed
 * @param blocks Output array
 * @return Number of blocks allocated
 */
size_t uft_amiga_alloc_blocks(uft_amiga_ctx_t *ctx, size_t count,
                              uint32_t *blocks);

/*===========================================================================
 * Validation Functions
 *===========================================================================*/

/**
 * @brief Validate filesystem
 * @param ctx Context
 * @param report Output validation report
 * @return 0 on success (valid), negative on invalid
 */
int uft_amiga_validate(const uft_amiga_ctx_t *ctx, uft_amiga_validation_t *report);

/**
 * @brief Free validation report
 * @param report Report to free
 */
void uft_amiga_free_validation(uft_amiga_validation_t *report);

/**
 * @brief Fix bitmap inconsistencies
 * @param ctx Context
 * @return Number of fixes made
 */
int uft_amiga_fix_bitmap(uft_amiga_ctx_t *ctx);

/**
 * @brief Rebuild bitmap from scratch
 * @param ctx Context
 * @return 0 on success
 */
int uft_amiga_rebuild_bitmap(uft_amiga_ctx_t *ctx);

/**
 * @brief Check block chain integrity
 * @param ctx Context
 * @param header_block File/dir header block
 * @return 0 if valid, negative on error
 */
int uft_amiga_check_chain(const uft_amiga_ctx_t *ctx, uint32_t header_block);

/*===========================================================================
 * Formatting Functions
 *===========================================================================*/

/**
 * @brief Format new ADF image
 * @param ctx Context (must have data buffer)
 * @param fs_type Filesystem type
 * @param volume_name Volume name
 * @return 0 on success
 */
int uft_amiga_format(uft_amiga_ctx_t *ctx, uft_amiga_fs_type_t fs_type,
                     const char *volume_name);

/**
 * @brief Create blank ADF image
 * @param filename Output filename
 * @param is_hd HD (true) or DD (false)
 * @param fs_type Filesystem type
 * @param volume_name Volume name
 * @return 0 on success
 */
int uft_amiga_create_adf(const char *filename, bool is_hd,
                         uft_amiga_fs_type_t fs_type, const char *volume_name);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Convert Amiga timestamp to Unix time
 * @param days Days since 1978-01-01
 * @param mins Minutes of day
 * @param ticks Ticks (1/50 second)
 * @return Unix timestamp
 */
time_t uft_amiga_to_unix_time(uint32_t days, uint32_t mins, uint32_t ticks);

/**
 * @brief Convert Unix time to Amiga timestamp
 * @param unix_time Unix timestamp
 * @param days Output days
 * @param mins Output minutes
 * @param ticks Output ticks
 */
void uft_amiga_from_unix_time(time_t unix_time, uint32_t *days,
                              uint32_t *mins, uint32_t *ticks);

/**
 * @brief Get protection string (e.g., "----rwed")
 * @param protection Protection bits
 * @param buffer Output buffer (9 bytes min)
 */
void uft_amiga_protection_str(uint32_t protection, char *buffer);

/**
 * @brief Parse protection string to bits
 * @param str Protection string
 * @return Protection bits
 */
uint32_t uft_amiga_parse_protection(const char *str);

/**
 * @brief Print directory listing
 * @param dir Directory structure
 */
void uft_amiga_print_dir(const uft_amiga_dir_t *dir);

/**
 * @brief Generate filesystem report as JSON
 * @param ctx Context
 * @param buffer Output buffer
 * @param size Buffer size
 * @return 0 on success
 */
int uft_amiga_report_json(const uft_amiga_ctx_t *ctx, char *buffer, size_t size);

/**
 * @brief Get default options
 * @return Default options structure
 */
uft_amiga_options_t uft_amiga_default_options(void);

/*===========================================================================
 * Bootblock Functions
 *===========================================================================*/

/**
 * @brief Read bootblock
 * @param ctx Context
 * @param block0 Output block 0 (512 bytes)
 * @param block1 Output block 1 (512 bytes)
 * @return 0 on success
 */
int uft_amiga_read_bootblock(const uft_amiga_ctx_t *ctx,
                             uint8_t *block0, uint8_t *block1);

/**
 * @brief Write bootblock
 * @param ctx Context
 * @param block0 Block 0 data
 * @param block1 Block 1 data
 * @return 0 on success
 */
int uft_amiga_write_bootblock(uft_amiga_ctx_t *ctx,
                              const uint8_t *block0, const uint8_t *block1);

/**
 * @brief Calculate bootblock checksum
 * @param boot Bootblock data (1024 bytes)
 * @return Checksum
 */
uint32_t uft_amiga_bootblock_checksum(const uint8_t *boot);

/**
 * @brief Make bootblock bootable
 * @param ctx Context
 * @return 0 on success
 */
int uft_amiga_make_bootable(uft_amiga_ctx_t *ctx);

/**
 * @brief Check if bootable
 * @param ctx Context
 * @return true if bootable
 */
bool uft_amiga_is_bootable(const uft_amiga_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGADOS_H */
