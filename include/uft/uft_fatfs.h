/**
 * @file uft_fatfs.h
 * @brief FatFs Integration for UFT
 * 
 * Provides FAT12/16/32 filesystem support for disk images.
 * Uses ChaN's FatFs library with UFT-specific disk I/O.
 * 
 * @copyright UFT Project 2026
 * @see http://elm-chan.org/fsw/ff/
 */

#ifndef UFT_FATFS_H
#define UFT_FATFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Floppy Disk Geometry
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Standard floppy disk formats
 */
typedef enum {
    UFT_FLOPPY_160K,    /**< 5.25" SS/SD 40T 8S  (160KB) */
    UFT_FLOPPY_180K,    /**< 5.25" SS/SD 40T 9S  (180KB) */
    UFT_FLOPPY_320K,    /**< 5.25" DS/SD 40T 8S  (320KB) */
    UFT_FLOPPY_360K,    /**< 5.25" DS/DD 40T 9S  (360KB) */
    UFT_FLOPPY_720K,    /**< 3.5"  DS/DD 80T 9S  (720KB) */
    UFT_FLOPPY_1200K,   /**< 5.25" HD    80T 15S (1.2MB) */
    UFT_FLOPPY_1440K,   /**< 3.5"  HD    80T 18S (1.44MB) */
    UFT_FLOPPY_2880K,   /**< 3.5"  ED    80T 36S (2.88MB) */
    UFT_FLOPPY_CUSTOM   /**< Custom geometry */
} uft_floppy_type_t;

/**
 * @brief Floppy disk geometry
 */
typedef struct {
    uint16_t cylinders;     /**< Tracks (40 or 80) */
    uint8_t  heads;         /**< Sides (1 or 2) */
    uint8_t  sectors;       /**< Sectors per track */
    uint16_t sector_size;   /**< Bytes per sector (512) */
    uint32_t total_sectors; /**< Total sectors */
    uint32_t total_bytes;   /**< Total bytes */
    const char *name;       /**< Format name */
} uft_floppy_geometry_t;

/**
 * @brief Get geometry for standard floppy type
 */
const uft_floppy_geometry_t* uft_floppy_get_geometry(uft_floppy_type_t type);

/**
 * @brief Detect floppy type from image size
 */
uft_floppy_type_t uft_floppy_detect_type(size_t image_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * FAT Image Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief FAT filesystem handle
 */
typedef struct uft_fat_image uft_fat_image_t;

/**
 * @brief File entry information
 */
typedef struct {
    char     name[260];     /**< File name */
    uint32_t size;          /**< File size in bytes */
    uint16_t date;          /**< Modification date (FAT format) */
    uint16_t time;          /**< Modification time (FAT format) */
    uint8_t  attr;          /**< File attributes */
    bool     is_dir;        /**< Is directory */
    bool     is_readonly;   /**< Is read-only */
    bool     is_hidden;     /**< Is hidden */
    bool     is_system;     /**< Is system file */
} uft_fat_entry_t;

/**
 * @brief Open a FAT disk image
 * 
 * @param path Path to image file
 * @param readonly Open in read-only mode
 * @return Handle or NULL on error
 */
uft_fat_image_t* uft_fat_open(const char *path, bool readonly);

/**
 * @brief Close FAT disk image
 */
void uft_fat_close(uft_fat_image_t *img);

/**
 * @brief Get image information
 */
int uft_fat_get_info(const uft_fat_image_t *img,
                     uft_floppy_geometry_t *geometry,
                     uint32_t *free_clusters,
                     uint32_t *total_clusters);

/**
 * @brief List directory contents
 * 
 * @param img Image handle
 * @param path Directory path (use "/" for root)
 * @param entries Output array
 * @param max_entries Maximum entries to return
 * @return Number of entries or -1 on error
 */
int uft_fat_list_dir(uft_fat_image_t *img, const char *path,
                     uft_fat_entry_t *entries, int max_entries);

/**
 * @brief Extract file from image
 * 
 * @param img Image handle
 * @param src_path Path in image
 * @param dst_path Destination path on host
 * @return 0 on success, -1 on error
 */
int uft_fat_extract(uft_fat_image_t *img, const char *src_path,
                    const char *dst_path);

/**
 * @brief Add file to image
 * 
 * @param img Image handle
 * @param src_path Source path on host
 * @param dst_path Destination path in image
 * @return 0 on success, -1 on error
 */
int uft_fat_add(uft_fat_image_t *img, const char *src_path,
                const char *dst_path);

/**
 * @brief Delete file from image
 */
int uft_fat_delete(uft_fat_image_t *img, const char *path);

/**
 * @brief Create directory in image
 */
int uft_fat_mkdir(uft_fat_image_t *img, const char *path);

/**
 * @brief Rename/move file in image
 */
int uft_fat_rename(uft_fat_image_t *img, const char *old_path,
                   const char *new_path);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Image Creation
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create new FAT12 floppy image
 * 
 * @param path Output path
 * @param type Floppy type
 * @param label Volume label (max 11 chars)
 * @return 0 on success, -1 on error
 */
int uft_fat_create_image(const char *path, uft_floppy_type_t type,
                         const char *label);

/**
 * @brief Format existing image
 * 
 * @param img Image handle
 * @param label Volume label
 * @return 0 on success, -1 on error
 */
int uft_fat_format(uft_fat_image_t *img, const char *label);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Boot Sector Analysis
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief FAT boot sector information
 */
typedef struct {
    char     oem_name[9];       /**< OEM name */
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    char     volume_label[12];
    char     fs_type[9];        /**< "FAT12", "FAT16", etc. */
} uft_fat_boot_sector_t;

/**
 * @brief Parse boot sector
 */
int uft_fat_parse_boot_sector(const uint8_t *data,
                              uft_fat_boot_sector_t *info);

/**
 * @brief Get FAT type from boot sector
 */
const char* uft_fat_detect_type(const uft_fat_boot_sector_t *info);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FATFS_H */
