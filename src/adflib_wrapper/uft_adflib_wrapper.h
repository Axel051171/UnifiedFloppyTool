/**
 * @file uft_adflib_wrapper.h
 * @brief ADFlib Wrapper for UnifiedFloppyTool
 * 
 * Provides high-level access to Amiga filesystems via ADFlib.
 * Enables: HDF partitions, file extraction, deleted recovery, bitmap repair.
 * 
 * Compile with -DUFT_USE_ADFLIB to enable.
 */

#ifndef UFT_ADFLIB_WRAPPER_H
#define UFT_ADFLIB_WRAPPER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Filesystem Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_ADF_FS_UNKNOWN = -1,
    UFT_ADF_FS_OFS = 0,         /* Original File System */
    UFT_ADF_FS_FFS = 1,         /* Fast File System */
    UFT_ADF_FS_OFS_INTL = 2,    /* OFS International */
    UFT_ADF_FS_FFS_INTL = 3,    /* FFS International */
    UFT_ADF_FS_OFS_DC = 4,      /* OFS with DirCache */
    UFT_ADF_FS_FFS_DC = 5,      /* FFS with DirCache */
} uft_adf_fs_type_t;

typedef enum {
    UFT_ADF_ENTRY_FILE = 0,
    UFT_ADF_ENTRY_DIR = 1,
    UFT_ADF_ENTRY_SOFTLINK = 2,
    UFT_ADF_ENTRY_HARDLINK = 3,
} uft_adf_entry_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_adf_context uft_adf_context_t;

typedef struct {
    char name[256];
    uft_adf_entry_type_t type;
    uint32_t size;
    uint32_t sector;
    int year, month, day;
    int hour, minute, second;
    char comment[80];
    uint32_t protection;        /* Amiga protection bits */
    bool is_deleted;
} uft_adf_entry_t;

typedef struct {
    char name[32];
    uft_adf_fs_type_t fs_type;
    uint32_t num_blocks;
    uint32_t free_blocks;
    uint32_t root_block;
    int year, month, day;
    bool is_bootable;
} uft_adf_volume_info_t;

typedef struct {
    int cylinders;
    int heads;
    int sectors;
    int num_volumes;
    uint32_t total_blocks;
    bool has_rdb;               /* Rigid Disk Block present */
} uft_adf_device_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Device Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open an ADF/HDF device
 * @param path Path to ADF/HDF file
 * @param readonly Open in read-only mode
 * @return Context or NULL on error
 */
uft_adf_context_t* uft_adf_open(const char *path, bool readonly);

/**
 * @brief Close an ADF/HDF device
 * @param ctx Context to close
 */
void uft_adf_close(uft_adf_context_t *ctx);

/**
 * @brief Get device information
 * @param ctx Open context
 * @param info Output device info
 * @return 0 on success
 */
int uft_adf_get_device_info(uft_adf_context_t *ctx, uft_adf_device_info_t *info);

/**
 * @brief Get volume (partition) information
 * @param ctx Open context
 * @param vol_index Volume index (0-based)
 * @param info Output volume info
 * @return 0 on success
 */
int uft_adf_get_volume_info(uft_adf_context_t *ctx, int vol_index, 
                            uft_adf_volume_info_t *info);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Volume Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Mount a volume for file operations
 * @param ctx Open context
 * @param vol_index Volume index
 * @return 0 on success
 */
int uft_adf_mount_volume(uft_adf_context_t *ctx, int vol_index);

/**
 * @brief Unmount current volume
 * @param ctx Open context
 */
void uft_adf_unmount_volume(uft_adf_context_t *ctx);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Directory Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Change current directory
 * @param ctx Open context
 * @param path Directory path
 * @return 0 on success
 */
int uft_adf_change_dir(uft_adf_context_t *ctx, const char *path);

/**
 * @brief Go to root directory
 * @param ctx Open context
 */
void uft_adf_to_root(uft_adf_context_t *ctx);

/**
 * @brief Get current directory path
 * @param ctx Open context
 * @return Current path (do not free)
 */
const char* uft_adf_get_current_dir(uft_adf_context_t *ctx);

/**
 * @brief List directory contents
 * @param ctx Open context
 * @param entries Output array (caller allocates)
 * @param max_entries Maximum entries to return
 * @return Number of entries, or negative on error
 */
int uft_adf_list_dir(uft_adf_context_t *ctx, uft_adf_entry_t *entries, 
                     int max_entries);

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Extract a file from the ADF
 * @param ctx Open context
 * @param adf_path Path within ADF
 * @param local_path Destination path on local filesystem
 * @return 0 on success
 */
int uft_adf_extract_file(uft_adf_context_t *ctx, const char *adf_path, 
                         const char *local_path);

/**
 * @brief Extract all files from current directory (recursively)
 * @param ctx Open context
 * @param local_dir Destination directory
 * @param recursive Include subdirectories
 * @return Number of files extracted, or negative on error
 */
int uft_adf_extract_all(uft_adf_context_t *ctx, const char *local_dir, 
                        bool recursive);

/**
 * @brief Add a file to the ADF
 * @param ctx Open context (must be writable)
 * @param local_path Source file on local filesystem
 * @param adf_path Destination path within ADF
 * @return 0 on success
 */
int uft_adf_add_file(uft_adf_context_t *ctx, const char *local_path, 
                     const char *adf_path);

/**
 * @brief Delete a file from the ADF
 * @param ctx Open context (must be writable)
 * @param adf_path Path within ADF
 * @return 0 on success
 */
int uft_adf_delete_file(uft_adf_context_t *ctx, const char *adf_path);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Recovery Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief List deleted files/directories
 * @param ctx Open context
 * @param entries Output array
 * @param max_entries Maximum entries
 * @return Number of deleted entries found
 */
int uft_adf_list_deleted(uft_adf_context_t *ctx, uft_adf_entry_t *entries, 
                         int max_entries);

/**
 * @brief Attempt to recover a deleted file
 * @param ctx Open context (must be writable)
 * @param name Name of deleted file
 * @return 0 on success, negative on error
 */
int uft_adf_recover_file(uft_adf_context_t *ctx, const char *name);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Repair Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Rebuild block allocation bitmap
 * @param ctx Open context (must be writable)
 * @return 0 on success
 */
int uft_adf_rebuild_bitmap(uft_adf_context_t *ctx);

/**
 * @brief Check filesystem consistency
 * @param ctx Open context
 * @param errors Output error count
 * @return 0 if consistent, >0 if errors found
 */
int uft_adf_check_consistency(uft_adf_context_t *ctx, int *errors);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get filesystem type name
 */
const char* uft_adf_fs_type_name(uft_adf_fs_type_t type);

/**
 * @brief Check if ADFlib support is compiled in
 */
bool uft_adf_is_available(void);

/**
 * @brief Get last error message
 */
const char* uft_adf_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ADFLIB_WRAPPER_H */
