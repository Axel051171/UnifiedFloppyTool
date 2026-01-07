/**
 * @file amigados_filesystem.h
 * @brief AmigaDOS Filesystem - Public API
 * 
 * 
 * @version 0.8.0
 * @date 2024-12-22
 */

#ifndef UFT_AMIGADOS_FILESYSTEM_H
#define UFT_AMIGADOS_FILESYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * FORWARD DECLARATIONS
 *============================================================================*/

typedef struct disk disk_t;
typedef struct amigados_fs amigados_fs_t;

/*============================================================================*
 * TYPES
 *============================================================================*/

/**
 * @brief AmigaDOS filesystem type
 */
typedef enum {
    AMIGADOS_UNKNOWN = 0,
    AMIGADOS_OFS,           /**< Old File System */
    AMIGADOS_FFS,           /**< Fast File System */
    AMIGADOS_OFS_INTL,      /**< OFS International */
    AMIGADOS_FFS_INTL       /**< FFS International */
} amigados_type_t;

/**
 * @brief File information
 */
typedef struct {
    char filename[32];
    uint32_t size;
    bool is_directory;
    uint32_t protection;        /**< Protection bits */
    uint32_t days;              /**< Date (days since 1978-01-01) */
    uint32_t mins;
    uint32_t ticks;
} amigados_file_info_t;

/*============================================================================*
 * FUNCTIONS
 *============================================================================*/

/**
 * @brief Mount AmigaDOS filesystem
 * 
 * Performance: ~3ms
 * 
 * @param disk Disk image
 * @return Filesystem or NULL
 */
amigados_fs_t* amigados_mount(disk_t *disk);

/**
 * @brief Unmount filesystem
 */
void amigados_unmount(amigados_fs_t *fs);

/**
 * @brief List directory
 * 
 * Performance: ~5ms
 * 
 * @param fs Filesystem
 * @param path Directory path
 * @param files_out Output: array of files
 * @return Number of files or negative on error
 */
int amigados_list_directory(amigados_fs_t *fs, const char *path,
                            amigados_file_info_t ***files_out);

/**
 * @brief Extract file
 * 
 * Performance: ~15ms for 100KB (8x faster)
 * 
 * @param fs Filesystem
 * @param filename Filename
 * @param data_out Output: file data (allocated)
 * @param size_out Output: file size
 * @return 0 on success
 */
int amigados_extract_file(amigados_fs_t *fs, const char *filename,
                          uint8_t **data_out, size_t *size_out);

/**
 * @brief Get disk name
 */
int amigados_get_disk_name(amigados_fs_t *fs, char *name, size_t max_len);

/**
 * @brief Get filesystem type string
 */
const char* amigados_get_type_string(amigados_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGADOS_FILESYSTEM_H */
