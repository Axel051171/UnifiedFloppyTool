/**
 * @file uft_fat_editor.h
 * @brief FAT Filesystem Editor API
 * 
 * Features:
 * - FAT12/FAT16/FAT32 support
 * - Cluster chain visualization
 * - Bad cluster marking
 * - Boot sector editing
 * - Directory entry manipulation
 * - Cross-linked file detection
 * - Lost cluster recovery
 * 
 * Inspired by DiskImageTool's FAT Editor.
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_FAT_EDITOR_H
#define UFT_FAT_EDITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * FAT TYPES
 *===========================================================================*/

/**
 * @brief FAT type
 */
typedef enum {
    UFT_FAT_UNKNOWN = 0,
    UFT_FAT_12,
    UFT_FAT_16,
    UFT_FAT_32
} uft_fat_type_t;

/**
 * @brief Cluster status
 */
typedef enum {
    UFT_CLUSTER_FREE = 0,       /**< Available */
    UFT_CLUSTER_USED,           /**< In use by file */
    UFT_CLUSTER_BAD,            /**< Marked bad */
    UFT_CLUSTER_RESERVED,       /**< Reserved */
    UFT_CLUSTER_END,            /**< End of chain */
    UFT_CLUSTER_ORPHAN          /**< Lost cluster */
} uft_cluster_status_t;

/**
 * @brief Directory entry attributes
 */
typedef enum {
    UFT_ATTR_READ_ONLY  = 0x01,
    UFT_ATTR_HIDDEN     = 0x02,
    UFT_ATTR_SYSTEM     = 0x04,
    UFT_ATTR_VOLUME_ID  = 0x08,
    UFT_ATTR_DIRECTORY  = 0x10,
    UFT_ATTR_ARCHIVE    = 0x20,
    UFT_ATTR_LFN        = 0x0F  /**< Long filename entry */
} uft_fat_attr_t;

/*===========================================================================
 * STRUCTURES
 *===========================================================================*/

#pragma pack(push, 1)

/**
 * @brief FAT12/16 Boot Sector (BPB)
 */
typedef struct {
    uint8_t  jump[3];           /**< Jump instruction */
    char     oem_name[8];       /**< OEM name */
    uint16_t bytes_per_sector;  /**< Bytes per sector */
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    /* Extended BPB (FAT12/16) */
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
    uint8_t  boot_code[448];
    uint16_t signature;         /**< 0xAA55 */
} uft_fat16_boot_t;

/**
 * @brief FAT32 Boot Sector
 */
typedef struct {
    uint8_t  jump[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;  /**< 0 for FAT32 */
    uint16_t total_sectors_16;  /**< 0 for FAT32 */
    uint8_t  media_type;
    uint16_t fat_size_16;       /**< 0 for FAT32 */
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    /* FAT32 specific */
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
    uint8_t  boot_code[420];
    uint16_t signature;
} uft_fat32_boot_t;

/**
 * @brief Directory entry (8.3 format)
 */
typedef struct {
    char     name[8];           /**< Filename (padded) */
    char     ext[3];            /**< Extension (padded) */
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_hi;  /**< High 16 bits (FAT32) */
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t first_cluster_lo;  /**< Low 16 bits */
    uint32_t file_size;
} uft_fat_dirent_t;

#pragma pack(pop)

/**
 * @brief Cluster chain info
 */
typedef struct {
    uint32_t start_cluster;
    uint32_t *clusters;         /**< Array of cluster numbers */
    int cluster_count;
    bool has_cross_link;        /**< Chain crosses another file */
    uint32_t cross_link_cluster;/**< Where cross-link occurs */
} uft_cluster_chain_t;

/**
 * @brief FAT statistics
 */
typedef struct {
    uft_fat_type_t type;
    uint32_t total_clusters;
    uint32_t free_clusters;
    uint32_t used_clusters;
    uint32_t bad_clusters;
    uint32_t reserved_clusters;
    uint32_t orphan_clusters;   /**< Lost clusters */
    uint32_t bytes_per_cluster;
    uint64_t total_size;
    uint64_t free_size;
} uft_fat_stats_t;

/**
 * @brief File info
 */
typedef struct {
    char short_name[13];        /**< 8.3 name */
    char long_name[256];        /**< LFN if available */
    uint8_t attributes;
    uint32_t first_cluster;
    uint32_t file_size;
    uint16_t create_date;
    uint16_t create_time;
    uint16_t modify_date;
    uint16_t modify_time;
    bool is_deleted;
} uft_fat_file_info_t;

/*===========================================================================
 * CONTEXT
 *===========================================================================*/

typedef struct uft_fat_context uft_fat_t;

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Open FAT filesystem from image
 */
uft_fat_t* uft_fat_open(const uint8_t *image, size_t size);

/**
 * @brief Open FAT filesystem from file
 */
uft_fat_t* uft_fat_open_file(const char *path);

/**
 * @brief Create new FAT filesystem
 */
uft_fat_t* uft_fat_create(uint8_t *image, size_t size, 
                           uft_fat_type_t type,
                           const char *volume_label);

/**
 * @brief Close FAT context
 */
void uft_fat_close(uft_fat_t *fat);

/**
 * @brief Check if image has valid FAT
 */
bool uft_fat_probe(const uint8_t *image, size_t size);

/*===========================================================================
 * INFORMATION
 *===========================================================================*/

/**
 * @brief Get FAT type
 */
uft_fat_type_t uft_fat_get_type(uft_fat_t *fat);

/**
 * @brief Get FAT type name
 */
const char* uft_fat_type_name(uft_fat_type_t type);

/**
 * @brief Get filesystem statistics
 */
int uft_fat_get_stats(uft_fat_t *fat, uft_fat_stats_t *stats);

/**
 * @brief Get boot sector (FAT12/16)
 */
const uft_fat16_boot_t* uft_fat_get_boot16(uft_fat_t *fat);

/**
 * @brief Get boot sector (FAT32)
 */
const uft_fat32_boot_t* uft_fat_get_boot32(uft_fat_t *fat);

/**
 * @brief Get volume label
 */
const char* uft_fat_get_volume_label(uft_fat_t *fat);

/*===========================================================================
 * CLUSTER OPERATIONS
 *===========================================================================*/

/**
 * @brief Get cluster value from FAT
 */
uint32_t uft_fat_get_cluster(uft_fat_t *fat, uint32_t cluster);

/**
 * @brief Set cluster value in FAT
 */
int uft_fat_set_cluster(uft_fat_t *fat, uint32_t cluster, uint32_t value);

/**
 * @brief Get cluster status
 */
uft_cluster_status_t uft_fat_get_cluster_status(uft_fat_t *fat, uint32_t cluster);

/**
 * @brief Mark cluster as bad
 */
int uft_fat_mark_bad(uft_fat_t *fat, uint32_t cluster);

/**
 * @brief Mark cluster as free
 */
int uft_fat_mark_free(uft_fat_t *fat, uint32_t cluster);

/**
 * @brief Get cluster chain for file
 */
int uft_fat_get_chain(uft_fat_t *fat, uint32_t start_cluster,
                       uft_cluster_chain_t *chain);

/**
 * @brief Free cluster chain
 */
void uft_fat_free_chain(uft_cluster_chain_t *chain);

/**
 * @brief Read cluster data
 */
int uft_fat_read_cluster(uft_fat_t *fat, uint32_t cluster,
                          uint8_t *buffer, size_t size);

/**
 * @brief Write cluster data
 */
int uft_fat_write_cluster(uft_fat_t *fat, uint32_t cluster,
                           const uint8_t *buffer, size_t size);

/*===========================================================================
 * DIRECTORY OPERATIONS
 *===========================================================================*/

/**
 * @brief Read root directory
 */
int uft_fat_read_root(uft_fat_t *fat, uft_fat_file_info_t *entries,
                       int max_entries);

/**
 * @brief Read directory at cluster
 */
int uft_fat_read_dir(uft_fat_t *fat, uint32_t cluster,
                      uft_fat_file_info_t *entries, int max_entries);

/**
 * @brief Find file by name
 */
int uft_fat_find_file(uft_fat_t *fat, const char *path,
                       uft_fat_file_info_t *info);

/**
 * @brief Get raw directory entry
 */
int uft_fat_get_dirent(uft_fat_t *fat, uint32_t cluster, int index,
                        uft_fat_dirent_t *entry);

/**
 * @brief Set raw directory entry
 */
int uft_fat_set_dirent(uft_fat_t *fat, uint32_t cluster, int index,
                        const uft_fat_dirent_t *entry);

/*===========================================================================
 * FILE OPERATIONS
 *===========================================================================*/

/**
 * @brief Read file data
 */
int uft_fat_read_file(uft_fat_t *fat, const char *path,
                       uint8_t *buffer, size_t max_size);

/**
 * @brief Write file data
 */
int uft_fat_write_file(uft_fat_t *fat, const char *path,
                        const uint8_t *data, size_t size);

/**
 * @brief Delete file
 */
int uft_fat_delete_file(uft_fat_t *fat, const char *path);

/**
 * @brief Recover deleted file
 */
int uft_fat_recover_file(uft_fat_t *fat, uint32_t cluster,
                          const char *new_name);

/*===========================================================================
 * BOOT SECTOR EDITING
 *===========================================================================*/

/**
 * @brief Set OEM name
 */
int uft_fat_set_oem_name(uft_fat_t *fat, const char *oem_name);

/**
 * @brief Set volume label
 */
int uft_fat_set_volume_label(uft_fat_t *fat, const char *label);

/**
 * @brief Set volume ID
 */
int uft_fat_set_volume_id(uft_fat_t *fat, uint32_t volume_id);

/**
 * @brief Fix boot sector for compatibility
 */
int uft_fat_fix_boot_sector(uft_fat_t *fat);

/*===========================================================================
 * REPAIR/ANALYSIS
 *===========================================================================*/

/**
 * @brief Find lost clusters (orphans)
 */
int uft_fat_find_lost_clusters(uft_fat_t *fat, 
                                uint32_t *clusters, int max_clusters);

/**
 * @brief Find cross-linked files
 */
int uft_fat_find_cross_links(uft_fat_t *fat,
                              uint32_t *clusters, int max_clusters);

/**
 * @brief Validate FAT consistency
 */
int uft_fat_validate(uft_fat_t *fat, char *report, size_t report_size);

/**
 * @brief Repair FAT issues
 */
int uft_fat_repair(uft_fat_t *fat, bool fix_cross_links, 
                    bool recover_lost);

/**
 * @brief Compare FAT copies
 */
int uft_fat_compare_copies(uft_fat_t *fat, int *differences);

/**
 * @brief Sync FAT copies
 */
int uft_fat_sync_copies(uft_fat_t *fat);

/*===========================================================================
 * VISUALIZATION
 *===========================================================================*/

/**
 * @brief Get cluster map (for visualization)
 * 
 * @param fat FAT context
 * @param map Output: array of cluster statuses
 * @param max_clusters Maximum clusters to return
 * @return Number of clusters
 */
int uft_fat_get_cluster_map(uft_fat_t *fat, 
                             uft_cluster_status_t *map,
                             int max_clusters);

/**
 * @brief Print FAT summary
 */
void uft_fat_print_summary(uft_fat_t *fat);

/**
 * @brief Print cluster chain
 */
void uft_fat_print_chain(uft_fat_t *fat, uint32_t start_cluster);

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

/**
 * @brief Convert 8.3 name to string
 */
void uft_fat_name_to_string(const uft_fat_dirent_t *entry, 
                             char *name, size_t size);

/**
 * @brief Convert string to 8.3 name
 */
int uft_fat_string_to_name(const char *name, 
                            char *name83, char *ext83);

/**
 * @brief Decode FAT date
 */
void uft_fat_decode_date(uint16_t date, int *year, int *month, int *day);

/**
 * @brief Decode FAT time
 */
void uft_fat_decode_time(uint16_t time, int *hour, int *minute, int *second);

/**
 * @brief Encode FAT date
 */
uint16_t uft_fat_encode_date(int year, int month, int day);

/**
 * @brief Encode FAT time
 */
uint16_t uft_fat_encode_time(int hour, int minute, int second);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT_EDITOR_H */
