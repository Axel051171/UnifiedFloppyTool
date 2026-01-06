/**
 * @file uft_floppy_io.h
 * @brief Cross-platform disk I/O abstraction layer
 * 
 * Provides unified access to physical floppy drives and image files
 * across Linux, Windows, macOS, and DOS platforms.
 * 
 * Based on:
 * - discdiag linuxio.c / winio.c / dosio.c
 * - Fosfat libw32disk
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UFT_FLOPPY_IO_H
#define UFT_FLOPPY_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "uft_floppy_types.h"

/*===========================================================================
 * Opaque Handle
 *===========================================================================*/

/** Opaque disk handle */
typedef struct uft_disk_s uft_disk_t;

/*===========================================================================
 * Disk Access Mode
 *===========================================================================*/

typedef enum uft_access_mode {
    UFT_ACCESS_READ       = 0x01,   /**< Read-only access */
    UFT_ACCESS_WRITE      = 0x02,   /**< Write access */
    UFT_ACCESS_READWRITE  = 0x03,   /**< Read and write access */
} uft_access_mode_t;

/*===========================================================================
 * Disk Source Type
 *===========================================================================*/

typedef enum uft_disk_source {
    UFT_SOURCE_UNKNOWN    = 0,      /**< Unknown source */
    UFT_SOURCE_PHYSICAL   = 1,      /**< Physical drive */
    UFT_SOURCE_IMAGE      = 2,      /**< Disk image file */
} uft_disk_source_t;

/*===========================================================================
 * Disk Information Structure
 *===========================================================================*/

typedef struct uft_disk_info {
    uft_disk_source_t source;       /**< Source type */
    uft_access_mode_t mode;         /**< Current access mode */
    
    char path[256];                 /**< Device path or filename */
    
    uint64_t total_size;            /**< Total size in bytes */
    uint32_t total_sectors;         /**< Total sectors */
    uint16_t sector_size;           /**< Sector size (usually 512) */
    
    uft_geometry_t geometry;        /**< Disk geometry */
    
    bool write_protected;           /**< Write protection status */
    bool is_open;                   /**< Currently open */
} uft_disk_info_t;

/*===========================================================================
 * Initialization / Cleanup
 *===========================================================================*/

/**
 * @brief Initialize the disk I/O subsystem
 * 
 * Must be called before any other disk I/O functions.
 * 
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_disk_init(void);

/**
 * @brief Cleanup the disk I/O subsystem
 * 
 * Closes all open disks and frees resources.
 */
void uft_disk_cleanup(void);

/*===========================================================================
 * Drive Enumeration
 *===========================================================================*/

/**
 * @brief Get the number of available physical drives
 * 
 * @return Number of drives (0 if none or error)
 */
int uft_disk_get_drive_count(void);

/**
 * @brief Test if a physical drive exists
 * 
 * @param drive_index Drive index (0-based)
 * @return true if drive exists and is accessible
 */
bool uft_disk_drive_exists(int drive_index);

/**
 * @brief Get platform-specific drive path
 * 
 * @param drive_index Drive index (0-based)
 * @return Drive path string or NULL if invalid
 * 
 * @note The returned string is statically allocated
 */
const char* uft_disk_get_drive_path(int drive_index);

/**
 * @brief Query drive size without opening it
 * 
 * @param drive_index Drive index (0-based)
 * @param[out] size_bytes Total size in bytes
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_disk_query_size(int drive_index, uint64_t *size_bytes);

/*===========================================================================
 * Open / Close Operations
 *===========================================================================*/

/**
 * @brief Open a physical drive
 * 
 * @param drive_index Drive index (0-based)
 * @param mode Access mode (read, write, or both)
 * @param[out] disk Pointer to receive disk handle
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_disk_open_drive(int drive_index, uft_access_mode_t mode, 
                                 uft_disk_t **disk);

/**
 * @brief Open a disk image file
 * 
 * @param path Path to the image file
 * @param mode Access mode (read, write, or both)
 * @param[out] disk Pointer to receive disk handle
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_disk_open_image(const char *path, uft_access_mode_t mode,
                                 uft_disk_t **disk);

/**
 * @brief Close a disk handle
 * 
 * @param disk Disk handle to close
 */
void uft_disk_close(uft_disk_t *disk);

/*===========================================================================
 * Information Queries
 *===========================================================================*/

/**
 * @brief Get disk information
 * 
 * @param disk Disk handle
 * @param[out] info Information structure to fill
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_disk_get_info(uft_disk_t *disk, uft_disk_info_t *info);

/**
 * @brief Get disk size in sectors
 * 
 * @param disk Disk handle
 * @param[out] sectors Total sector count
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_disk_get_size(uft_disk_t *disk, uint64_t *sectors);

/**
 * @brief Get sector size
 * 
 * @param disk Disk handle
 * @return Sector size in bytes (0 on error)
 */
uint16_t uft_disk_get_sector_size(uft_disk_t *disk);

/**
 * @brief Check if disk is write-protected
 * 
 * @param disk Disk handle
 * @return true if write-protected
 */
bool uft_disk_is_write_protected(uft_disk_t *disk);

/*===========================================================================
 * Read / Write Operations
 *===========================================================================*/

/**
 * @brief Read sectors from disk
 * 
 * @param disk Disk handle
 * @param buffer Buffer to read into
 * @param lba Starting logical block address
 * @param count Number of sectors to read
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_disk_read_sectors(uft_disk_t *disk, void *buffer,
                                   uint64_t lba, uint32_t count);

/**
 * @brief Write sectors to disk
 * 
 * @param disk Disk handle
 * @param buffer Buffer containing data to write
 * @param lba Starting logical block address
 * @param count Number of sectors to write
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_disk_write_sectors(uft_disk_t *disk, const void *buffer,
                                    uint64_t lba, uint32_t count);

/**
 * @brief Read raw bytes from disk
 * 
 * @param disk Disk handle
 * @param buffer Buffer to read into
 * @param offset Byte offset from start
 * @param size Number of bytes to read
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_disk_read_bytes(uft_disk_t *disk, void *buffer,
                                 uint64_t offset, size_t size);

/**
 * @brief Write raw bytes to disk
 * 
 * @param disk Disk handle
 * @param buffer Buffer containing data to write
 * @param offset Byte offset from start
 * @param size Number of bytes to write
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_disk_write_bytes(uft_disk_t *disk, const void *buffer,
                                  uint64_t offset, size_t size);

/*===========================================================================
 * Synchronization
 *===========================================================================*/

/**
 * @brief Flush any buffered writes to disk
 * 
 * @param disk Disk handle
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_disk_sync(uft_disk_t *disk);

/*===========================================================================
 * Write Protection
 *===========================================================================*/

/**
 * @brief Set software write protection
 * 
 * Prevents write operations even if the disk is opened with write access.
 * 
 * @param disk Disk handle
 * @param protect true to enable protection, false to disable
 * @return UFT_OK on success, error code otherwise
 */
uft_error_t uft_disk_set_protection(uft_disk_t *disk, bool protect);

/*===========================================================================
 * Error Handling
 *===========================================================================*/

/**
 * @brief Get the last error code for a disk handle
 * 
 * @param disk Disk handle
 * @return Last error code
 */
uft_error_t uft_disk_get_last_error(uft_disk_t *disk);

/**
 * @brief Get error message for an error code
 * 
 * @param error Error code
 * @return Human-readable error message
 */
const char* uft_disk_error_string(uft_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLOPPY_IO_H */
