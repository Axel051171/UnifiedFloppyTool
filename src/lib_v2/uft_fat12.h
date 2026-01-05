/**
 * @file uft_fat12.h
 * @brief FAT12 filesystem implementation for floppy disks
 * 
 * Provides complete FAT12 filesystem access including:
 * - Directory listing and navigation
 * - File reading and writing
 * - File creation and deletion
 * - Formatting
 * 
 * Based on:
 * - BootLoader-Test Fat12.inc
 * - discdiag FAT handling
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UFT_FAT12_H
#define UFT_FAT12_H

#ifdef __cplusplus
extern "C" {
#endif

#include "uft_floppy_types.h"
#include "uft_floppy_io.h"

/*===========================================================================
 * Opaque Handles
 *===========================================================================*/

/** FAT12 volume handle */
typedef struct uft_fat12_s uft_fat12_t;

/** Directory handle */
typedef struct uft_fat12_dir_s uft_fat12_dir_t;

/** File handle */
typedef struct uft_fat12_file_s uft_fat12_file_t;

/*===========================================================================
 * Volume Information
 *===========================================================================*/

typedef struct uft_fat12_info {
    char     oem_name[9];           /**< OEM name (null-terminated) */
    char     volume_label[12];      /**< Volume label (null-terminated) */
    uint32_t volume_serial;         /**< Volume serial number */
    
    uint32_t total_sectors;         /**< Total sectors */
    uint32_t free_sectors;          /**< Free sectors */
    uint32_t used_sectors;          /**< Used sectors */
    
    uint32_t total_clusters;        /**< Total data clusters */
    uint32_t free_clusters;         /**< Free clusters */
    
    uint16_t bytes_per_sector;      /**< Bytes per sector */
    uint8_t  sectors_per_cluster;   /**< Sectors per cluster */
    uint16_t root_entries;          /**< Root directory entries */
    uint8_t  fat_count;             /**< Number of FAT copies */
    uint16_t fat_sectors;           /**< Sectors per FAT */
    
    uint8_t  media_type;            /**< Media descriptor byte */
    
    bool     is_dirty;              /**< Volume needs sync */
} uft_fat12_info_t;

/*===========================================================================
 * File/Directory Entry
 *===========================================================================*/

typedef struct uft_fat12_entry {
    char     name[13];              /**< Filename (8.3 format, null-term) */
    char     short_name[12];        /**< Raw 8.3 name (space-padded) */
    
    uint8_t  attributes;            /**< File attributes */
    uint32_t size;                  /**< File size in bytes */
    uint16_t cluster;               /**< Starting cluster */
    
    struct {
        uint16_t year;
        uint8_t  month;
        uint8_t  day;
        uint8_t  hour;
        uint8_t  minute;
        uint8_t  second;
    } created, modified, accessed;
    
    bool     is_directory;          /**< Is a directory */
    bool     is_hidden;             /**< Hidden attribute set */
    bool     is_system;             /**< System attribute set */
    bool     is_readonly;           /**< Read-only attribute set */
    bool     is_deleted;            /**< Entry is deleted (E5) */
    
    /* Internal use */
    uint32_t dir_sector;            /**< Sector containing entry */
    uint8_t  dir_offset;            /**< Offset within sector */
} uft_fat12_entry_t;

/*===========================================================================
 * Volume Operations
 *===========================================================================*/

/**
 * @brief Mount a FAT12 volume
 * 
 * @param disk Opened disk handle
 * @param[out] vol Pointer to receive volume handle
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_mount(uft_disk_t *disk, uft_fat12_t **vol);

/**
 * @brief Unmount a FAT12 volume
 * 
 * Flushes all pending writes before unmounting.
 * 
 * @param vol Volume handle
 */
void uft_fat12_unmount(uft_fat12_t *vol);

/**
 * @brief Get volume information
 * 
 * @param vol Volume handle
 * @param[out] info Information structure to fill
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_get_info(uft_fat12_t *vol, uft_fat12_info_t *info);

/**
 * @brief Flush all pending writes
 * 
 * @param vol Volume handle
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_sync(uft_fat12_t *vol);

/**
 * @brief Set volume label
 * 
 * @param vol Volume handle
 * @param label New volume label (max 11 chars)
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_set_label(uft_fat12_t *vol, const char *label);

/*===========================================================================
 * Directory Operations
 *===========================================================================*/

/**
 * @brief Open root directory
 * 
 * @param vol Volume handle
 * @param[out] dir Pointer to receive directory handle
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_opendir_root(uft_fat12_t *vol, uft_fat12_dir_t **dir);

/**
 * @brief Open a subdirectory
 * 
 * @param vol Volume handle
 * @param path Path to directory (use / or \ as separator)
 * @param[out] dir Pointer to receive directory handle
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_opendir(uft_fat12_t *vol, const char *path,
                               uft_fat12_dir_t **dir);

/**
 * @brief Read next directory entry
 * 
 * @param dir Directory handle
 * @param[out] entry Entry structure to fill
 * @return UFT_OK on success, UFT_ERR_END_OF_FILE when done
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_readdir(uft_fat12_dir_t *dir, uft_fat12_entry_t *entry);

/**
 * @brief Rewind directory to beginning
 * 
 * @param dir Directory handle
 */
void uft_fat12_rewinddir(uft_fat12_dir_t *dir);

/**
 * @brief Close directory handle
 * 
 * @param dir Directory handle
 */
void uft_fat12_closedir(uft_fat12_dir_t *dir);

/**
 * @brief Find entry by name
 * 
 * @param vol Volume handle
 * @param path Full path to file/directory
 * @param[out] entry Entry structure to fill
 * @return UFT_OK if found, UFT_ERR_NOT_FOUND otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_find(uft_fat12_t *vol, const char *path,
                            uft_fat12_entry_t *entry);

/**
 * @brief Create a new directory
 * 
 * @param vol Volume handle
 * @param path Path for new directory
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_mkdir(uft_fat12_t *vol, const char *path);

/**
 * @brief Remove an empty directory
 * 
 * @param vol Volume handle
 * @param path Path to directory
 * @return UFT_OK on success, UFT_ERR_DIR_NOT_EMPTY if not empty
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_rmdir(uft_fat12_t *vol, const char *path);

/*===========================================================================
 * File Operations
 *===========================================================================*/

/** File open modes */
typedef enum uft_fat12_mode {
    UFT_FAT12_READ        = 0x01,   /**< Open for reading */
    UFT_FAT12_WRITE       = 0x02,   /**< Open for writing */
    UFT_FAT12_CREATE      = 0x04,   /**< Create if not exists */
    UFT_FAT12_TRUNCATE    = 0x08,   /**< Truncate if exists */
    UFT_FAT12_APPEND      = 0x10,   /**< Append to file */
} uft_fat12_mode_t;

/**
 * @brief Open a file
 * 
 * @param vol Volume handle
 * @param path Path to file
 * @param mode Open mode flags
 * @param[out] file Pointer to receive file handle
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_open(uft_fat12_t *vol, const char *path,
                            uft_fat12_mode_t mode, uft_fat12_file_t **file);

/**
 * @brief Close a file
 * 
 * @param file File handle
 */
void uft_fat12_close(uft_fat12_file_t *file);

/**
 * @brief Read from file
 * 
 * @param file File handle
 * @param buffer Buffer to read into
 * @param size Maximum bytes to read
 * @param[out] bytes_read Actual bytes read
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_read(uft_fat12_file_t *file, void *buffer,
                            size_t size, size_t *bytes_read);

/**
 * @brief Write to file
 * 
 * @param file File handle
 * @param buffer Data to write
 * @param size Number of bytes to write
 * @param[out] bytes_written Actual bytes written
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_write(uft_fat12_file_t *file, const void *buffer,
                             size_t size, size_t *bytes_written);

/**
 * @brief Seek within file
 * 
 * @param file File handle
 * @param offset Byte offset
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_seek(uft_fat12_file_t *file, int32_t offset, int whence);

/**
 * @brief Get current file position
 * 
 * @param file File handle
 * @return Current position in bytes
 */
uint32_t uft_fat12_tell(uft_fat12_file_t *file);

/**
 * @brief Get file size
 * 
 * @param file File handle
 * @return File size in bytes
 */
uint32_t uft_fat12_size(uft_fat12_file_t *file);

/**
 * @brief Check for end of file
 * 
 * @param file File handle
 * @return true if at EOF
 */
bool uft_fat12_eof(uft_fat12_file_t *file);

/**
 * @brief Truncate file at current position
 * 
 * @param file File handle
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_truncate(uft_fat12_file_t *file);

/**
 * @brief Delete a file
 * 
 * @param vol Volume handle
 * @param path Path to file
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_delete(uft_fat12_t *vol, const char *path);

/**
 * @brief Rename a file or directory
 * 
 * @param vol Volume handle
 * @param old_path Current path
 * @param new_path New path
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_rename(uft_fat12_t *vol, const char *old_path,
                              const char *new_path);

/**
 * @brief Set file attributes
 * 
 * @param vol Volume handle
 * @param path Path to file
 * @param attr New attributes
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_set_attr(uft_fat12_t *vol, const char *path, uint8_t attr);

/*===========================================================================
 * FAT Table Operations (Low-Level)
 *===========================================================================*/

/**
 * @brief Read FAT entry for a cluster
 * 
 * @param vol Volume handle
 * @param cluster Cluster number
 * @param[out] value FAT entry value
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_get_fat_entry(uft_fat12_t *vol, uint16_t cluster,
                                     uint16_t *value);

/**
 * @brief Write FAT entry for a cluster
 * 
 * Updates all FAT copies.
 * 
 * @param vol Volume handle
 * @param cluster Cluster number
 * @param value New FAT entry value
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_set_fat_entry(uft_fat12_t *vol, uint16_t cluster,
                                     uint16_t value);

/**
 * @brief Find a free cluster
 * 
 * @param vol Volume handle
 * @param[out] cluster Free cluster number
 * @return UFT_OK on success, UFT_ERR_DISK_FULL if none available
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_find_free_cluster(uft_fat12_t *vol, uint16_t *cluster);

/**
 * @brief Get cluster chain length
 * 
 * @param vol Volume handle
 * @param start_cluster Starting cluster
 * @param[out] count Number of clusters in chain
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_chain_length(uft_fat12_t *vol, uint16_t start_cluster,
                                    uint32_t *count);

/*===========================================================================
 * Formatting
 *===========================================================================*/

/** Format options */
typedef struct uft_fat12_format_opts {
    const char *volume_label;       /**< Volume label (NULL for default) */
    uint32_t volume_serial;         /**< Volume serial (0 for random) */
    uft_floppy_type_t type;         /**< Target format type */
    bool quick_format;              /**< Quick format (don't zero data) */
} uft_fat12_format_opts_t;

/**
 * @brief Format a disk as FAT12
 * 
 * @param disk Disk handle
 * @param opts Format options (NULL for defaults)
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_format(uft_disk_t *disk, const uft_fat12_format_opts_t *opts);

/**
 * @brief Verify FAT12 format integrity
 * 
 * @param vol Volume handle
 * @param[out] errors Number of errors found
 * @return UFT_OK if valid, UFT_ERR_INVALID_FORMAT if corrupt
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_verify(uft_fat12_t *vol, uint32_t *errors);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Convert 8.3 filename to readable format
 * 
 * Converts "FILENAME EXT" to "FILENAME.EXT"
 * 
 * @param raw Raw 11-byte 8.3 name
 * @param[out] buffer Output buffer (at least 13 bytes)
 */
void uft_fat12_format_name(const char *raw, char *buffer);

/**
 * @brief Convert filename to 8.3 format
 * 
 * @param name Input filename
 * @param[out] buffer Output buffer (exactly 11 bytes)
 * @return UFT_OK on success, UFT_ERR_INVALID_PARAM if name too long
 */
UFT_WARN_UNUSED
uft_error_t uft_fat12_parse_name(const char *name, char *buffer);

/**
 * @brief Validate filename for FAT12
 * 
 * @param name Filename to validate
 * @return true if valid 8.3 filename
 */
bool uft_fat12_valid_name(const char *name);

/**
 * @brief Decode FAT date/time
 * 
 * @param date FAT date word
 * @param time FAT time word
 * @param[out] year Year (1980-2107)
 * @param[out] month Month (1-12)
 * @param[out] day Day (1-31)
 * @param[out] hour Hour (0-23)
 * @param[out] minute Minute (0-59)
 * @param[out] second Second (0-59, even only)
 */
void uft_fat12_decode_datetime(uint16_t date, uint16_t time,
                                uint16_t *year, uint8_t *month, uint8_t *day,
                                uint8_t *hour, uint8_t *minute, uint8_t *second);

/**
 * @brief Encode FAT date/time
 * 
 * @param year Year (1980-2107)
 * @param month Month (1-12)
 * @param day Day (1-31)
 * @param hour Hour (0-23)
 * @param minute Minute (0-59)
 * @param second Second (0-59)
 * @param[out] date FAT date word
 * @param[out] time FAT time word
 */
void uft_fat12_encode_datetime(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second,
                                uint16_t *date, uint16_t *time);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT12_H */
