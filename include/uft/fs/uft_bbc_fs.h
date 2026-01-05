/**
 * @file uft_bbc_fs.h
 * @brief BBC Micro DFS/ADFS Filesystem Support
 * 
 * Supports:
 * - Acorn DFS (Disc Filing System) - 40/80 track, single/double sided
 * - Acorn ADFS (Advanced Disc Filing System) - various formats
 * - Opus DDOS/EDOS variants
 * - Watford DDFS
 * 
 * DFS Format (Catalog in tracks 0/1):
 * - Sector 0: 8 filename entries (8 chars each)
 * - Sector 1: Directory info, disk title, file metadata
 * - File entries: load/exec addr, length, start sector
 * 
 * ADFS Format:
 * - Hierarchical directory structure
 * - Fragment map for allocation
 * - Multiple directory formats (Old/New/Big/+)
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#ifndef UFT_BBC_FS_H
#define UFT_BBC_FS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * DFS Constants
 *===========================================================================*/

/** DFS sector size */
#define UFT_DFS_SECTOR_SIZE     256

/** Maximum files in DFS catalog */
#define UFT_DFS_MAX_FILES       31

/** DFS filename length */
#define UFT_DFS_NAME_LEN        7

/** DFS directory character position */
#define UFT_DFS_DIR_CHAR        7

/*===========================================================================
 * DFS Boot Options
 *===========================================================================*/

typedef enum {
    UFT_DFS_BOOT_NONE = 0,      /**< No boot action */
    UFT_DFS_BOOT_LOAD = 1,      /**< *LOAD !BOOT */
    UFT_DFS_BOOT_RUN = 2,       /**< *RUN !BOOT */
    UFT_DFS_BOOT_EXEC = 3       /**< *EXEC !BOOT */
} uft_dfs_boot_t;

/*===========================================================================
 * DFS File Entry
 *===========================================================================*/

/**
 * @brief DFS file entry
 */
typedef struct {
    char        name[UFT_DFS_NAME_LEN + 1];  /**< Filename (null-terminated) */
    char        dir;                          /**< Directory character ($, !, etc) */
    uint32_t    load_addr;                    /**< Load address */
    uint32_t    exec_addr;                    /**< Execution address */
    uint32_t    length;                       /**< File length in bytes */
    uint16_t    start_sector;                 /**< Start sector on disk */
    bool        locked;                       /**< File locked flag */
} uft_dfs_file_t;

/*===========================================================================
 * DFS Disk Info
 *===========================================================================*/

/**
 * @brief DFS disk information
 */
typedef struct {
    char        title[13];          /**< Disk title (12 chars max) */
    uint8_t     sequence;           /**< Sequence number */
    uint8_t     boot_option;        /**< Boot option (0-3) */
    uint16_t    num_sectors;        /**< Total sectors on disk */
    uint8_t     num_files;          /**< Number of files in catalog */
    bool        double_sided;       /**< Double-sided disk flag */
    uint8_t     tracks;             /**< Number of tracks (40 or 80) */
    
    uft_dfs_file_t files[UFT_DFS_MAX_FILES];
} uft_dfs_info_t;

/*===========================================================================
 * ADFS Constants
 *===========================================================================*/

/** ADFS sector size (standard) */
#define UFT_ADFS_SECTOR_SIZE    256

/** ADFS big sector size */
#define UFT_ADFS_BIG_SECTOR     1024

/** ADFS directory entry size */
#define UFT_ADFS_DIRENTRY_SIZE  26

/** ADFS filename max length */
#define UFT_ADFS_NAME_LEN       10

/*===========================================================================
 * ADFS Directory Types
 *===========================================================================*/

typedef enum {
    UFT_ADFS_DIR_OLD = 0,       /**< Old directory format */
    UFT_ADFS_DIR_NEW,           /**< New directory format */
    UFT_ADFS_DIR_BIG,           /**< Big directory format */
    UFT_ADFS_DIR_PLUS           /**< ADFS+ directory format */
} uft_adfs_dir_type_t;

/*===========================================================================
 * ADFS File Attributes
 *===========================================================================*/

typedef enum {
    UFT_ADFS_ATTR_R = 0x01,     /**< Owner read */
    UFT_ADFS_ATTR_W = 0x02,     /**< Owner write */
    UFT_ADFS_ATTR_L = 0x04,     /**< Locked */
    UFT_ADFS_ATTR_D = 0x08,     /**< Directory */
    UFT_ADFS_ATTR_E = 0x10,     /**< Execute only */
    UFT_ADFS_ATTR_r = 0x20,     /**< Public read */
    UFT_ADFS_ATTR_w = 0x40,     /**< Public write */
    UFT_ADFS_ATTR_e = 0x80      /**< Public execute */
} uft_adfs_attr_t;

/*===========================================================================
 * ADFS File Entry
 *===========================================================================*/

/**
 * @brief ADFS file/directory entry
 */
typedef struct {
    char        name[UFT_ADFS_NAME_LEN + 1];  /**< Filename */
    uint32_t    load_addr;                     /**< Load address */
    uint32_t    exec_addr;                     /**< Execution address */
    uint32_t    length;                        /**< File length */
    uint32_t    sector;                        /**< Start sector/fragment */
    uint8_t     attributes;                    /**< File attributes */
    bool        is_directory;                  /**< True if directory */
} uft_adfs_entry_t;

/*===========================================================================
 * ADFS Disk Info
 *===========================================================================*/

/**
 * @brief ADFS disk information
 */
typedef struct {
    char        name[11];           /**< Disk name */
    uint8_t     dir_type;           /**< Directory format type */
    uint32_t    total_sectors;      /**< Total sectors */
    uint32_t    free_sectors;       /**< Free sectors */
    uint16_t    sector_size;        /**< Bytes per sector */
    uint8_t     log2_sector;        /**< Log2 of sector size */
    uint8_t     zones;              /**< Number of allocation zones */
    uint16_t    zone_bits;          /**< Bits per zone */
    uint32_t    root_dir;           /**< Root directory address */
    uint32_t    boot_option;        /**< Boot option */
} uft_adfs_info_t;

/*===========================================================================
 * Format Variants
 *===========================================================================*/

/**
 * @brief BBC disk format variant
 */
typedef enum {
    /* DFS variants */
    UFT_BBC_DFS_40T_SS = 0,     /**< 40 track, single sided (100KB) */
    UFT_BBC_DFS_80T_SS,         /**< 80 track, single sided (200KB) */
    UFT_BBC_DFS_40T_DS,         /**< 40 track, double sided (200KB) */
    UFT_BBC_DFS_80T_DS,         /**< 80 track, double sided (400KB) */
    
    /* ADFS variants */
    UFT_BBC_ADFS_S,             /**< ADFS S format (160KB) */
    UFT_BBC_ADFS_M,             /**< ADFS M format (320KB) */
    UFT_BBC_ADFS_L,             /**< ADFS L format (640KB) */
    UFT_BBC_ADFS_D,             /**< ADFS D format (800KB) */
    UFT_BBC_ADFS_E,             /**< ADFS E format (800KB) */
    UFT_BBC_ADFS_F,             /**< ADFS F format (1600KB) */
    UFT_BBC_ADFS_G,             /**< ADFS G format (3200KB HD) */
    
    /* Opus variants */
    UFT_BBC_DDOS_40T,           /**< Opus DDOS 40 track (180KB) */
    UFT_BBC_DDOS_80T,           /**< Opus DDOS 80 track (360KB) */
    UFT_BBC_EDOS,               /**< Opus EDOS (various) */
    
    /* Watford */
    UFT_BBC_WATFORD_DDFS,       /**< Watford DDFS (double density) */
    
    UFT_BBC_FORMAT_COUNT
} uft_bbc_format_t;

/*===========================================================================
 * API Functions - Detection
 *===========================================================================*/

/**
 * @brief Detect BBC disk format
 * @param data Disk image data
 * @param size Data size
 * @param format_out Detected format (or NULL)
 * @return Confidence (0-100), 0 if not detected
 */
int uft_bbc_detect(const uint8_t *data, size_t size,
                   uft_bbc_format_t *format_out);

/**
 * @brief Get format name
 * @param format Format ID
 * @return Format name string
 */
const char *uft_bbc_format_name(uft_bbc_format_t format);

/**
 * @brief Check if format is DFS variant
 */
bool uft_bbc_is_dfs(uft_bbc_format_t format);

/**
 * @brief Check if format is ADFS variant
 */
bool uft_bbc_is_adfs(uft_bbc_format_t format);

/*===========================================================================
 * API Functions - DFS
 *===========================================================================*/

/**
 * @brief Read DFS catalog
 * @param data Disk image data
 * @param size Data size
 * @param info Output disk information
 * @return 0 on success, -1 on error
 */
int uft_dfs_read_catalog(const uint8_t *data, size_t size,
                          uft_dfs_info_t *info);

/**
 * @brief Find file in DFS catalog
 * @param info Disk information
 * @param name Filename to find
 * @param dir Directory character ('$' for default)
 * @return Pointer to file entry or NULL
 */
const uft_dfs_file_t *uft_dfs_find_file(const uft_dfs_info_t *info,
                                         const char *name, char dir);

/**
 * @brief Extract DFS file
 * @param data Disk image data
 * @param size Data size  
 * @param file File entry to extract
 * @param buffer Output buffer
 * @param buf_size Buffer size
 * @return Bytes extracted, or -1 on error
 */
int uft_dfs_extract_file(const uint8_t *data, size_t size,
                          const uft_dfs_file_t *file,
                          uint8_t *buffer, size_t buf_size);

/**
 * @brief Create new DFS disk image
 * @param buffer Output buffer
 * @param buf_size Buffer size
 * @param tracks Number of tracks (40 or 80)
 * @param double_sided Double-sided flag
 * @param title Disk title (max 12 chars)
 * @return Image size, or -1 on error
 */
int uft_dfs_create(uint8_t *buffer, size_t buf_size,
                   int tracks, bool double_sided,
                   const char *title);

/**
 * @brief Add file to DFS disk image
 * @param data Disk image (modified in place)
 * @param size Image size
 * @param file File entry (start_sector filled in on success)
 * @param file_data File content
 * @return 0 on success, -1 on error
 */
int uft_dfs_add_file(uint8_t *data, size_t size,
                      uft_dfs_file_t *file,
                      const uint8_t *file_data);

/**
 * @brief Set DFS boot option
 */
int uft_dfs_set_boot(uint8_t *data, size_t size, uft_dfs_boot_t option);

/**
 * @brief Validate DFS image
 * @param data Disk image
 * @param size Image size
 * @param errors Optional error message buffer
 * @param err_size Error buffer size
 * @return Number of errors found (0 = valid)
 */
int uft_dfs_validate(const uint8_t *data, size_t size,
                      char *errors, size_t err_size);

/*===========================================================================
 * API Functions - ADFS
 *===========================================================================*/

/**
 * @brief Read ADFS disk info
 * @param data Disk image data
 * @param size Data size
 * @param info Output disk information
 * @return 0 on success, -1 on error
 */
int uft_adfs_read_info(const uint8_t *data, size_t size,
                        uft_adfs_info_t *info);

/**
 * @brief Read ADFS directory
 * @param data Disk image data
 * @param size Data size
 * @param dir_addr Directory address (0 for root)
 * @param entries Output entry array
 * @param max_entries Maximum entries to return
 * @return Number of entries, or -1 on error
 */
int uft_adfs_read_dir(const uint8_t *data, size_t size,
                       uint32_t dir_addr,
                       uft_adfs_entry_t *entries,
                       size_t max_entries);

/**
 * @brief Find file in ADFS by path
 * @param data Disk image data
 * @param size Data size
 * @param path Full path (e.g., "$.MyDir.MyFile")
 * @param entry Output entry
 * @return 0 on success, -1 if not found
 */
int uft_adfs_find_path(const uint8_t *data, size_t size,
                        const char *path,
                        uft_adfs_entry_t *entry);

/**
 * @brief Extract ADFS file
 */
int uft_adfs_extract_file(const uint8_t *data, size_t size,
                           const uft_adfs_entry_t *entry,
                           uint8_t *buffer, size_t buf_size);

/**
 * @brief Read ADFS free space map
 * @param data Disk image
 * @param size Image size
 * @param free_map Output bitmap (caller allocates)
 * @param map_size Bitmap size in bytes
 * @return Number of free sectors, or -1 on error
 */
int uft_adfs_read_freemap(const uint8_t *data, size_t size,
                           uint8_t *free_map, size_t map_size);

/*===========================================================================
 * API Functions - Conversion
 *===========================================================================*/

/**
 * @brief Convert DFS to SSD format
 * @param data Input disk data (any format)
 * @param size Input size
 * @param ssd_out Output buffer for SSD
 * @param ssd_size SSD buffer size
 * @return SSD size, or -1 on error
 * 
 * @note SSD is raw sector dump, 256 bytes/sector
 */
int uft_bbc_to_ssd(const uint8_t *data, size_t size,
                   uint8_t *ssd_out, size_t ssd_size);

/**
 * @brief Convert DFS to DSD format (double-sided)
 */
int uft_bbc_to_dsd(const uint8_t *data, size_t size,
                   uint8_t *dsd_out, size_t dsd_size);

/**
 * @brief Convert DFS to MMB format (Tube host bundle)
 */
int uft_bbc_to_mmb(const uint8_t **images, size_t *sizes, int count,
                   uint8_t *mmb_out, size_t mmb_size);

/**
 * @brief Convert ADFS to ADF format
 */
int uft_bbc_to_adf(const uint8_t *data, size_t size,
                   uint8_t *adf_out, size_t adf_size);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Decode BBC load/exec address
 * Handles the top 2 bits stored in catalog byte
 */
uint32_t uft_bbc_decode_addr(uint16_t base, uint8_t extra);

/**
 * @brief Encode BBC load/exec address
 */
void uft_bbc_encode_addr(uint32_t addr, uint16_t *base, uint8_t *extra);

/**
 * @brief Convert BBC string to ASCII
 * BBC strings have bit 7 set on last character
 */
void uft_bbc_string_decode(const uint8_t *src, char *dst, size_t max_len);

/**
 * @brief Encode ASCII to BBC string format
 */
void uft_bbc_string_encode(const char *src, uint8_t *dst, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BBC_FS_H */
