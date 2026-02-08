/**
 * @file uft_fbasic_fs.h
 * @brief Fujitsu F-BASIC Filesystem for FM-7/FM-77 Series
 * 
 * Implements the F-BASIC disk format used by Fujitsu FM-7, FM-77, FM-77AV
 * series computers. Based on D77/D88 disk images.
 * 
 * Disk Layout (2D format, 40 tracks, 16 sectors/track, 256 bytes/sector):
 *   Track 0, Sector 1-2:   IPL (Initial Program Loader)
 *   Track 0, Sector 3:     ID sector ('SYS' signature)
 *   Track 0, Sector 4-16:  Reserved
 *   Track 1, Sector 1-16:  Disk BASIC code
 *   Track 2, Sector 1:     FAT (File Allocation Table)
 *   Track 2, Sector 2-3:   Reserved
 *   Track 2, Sector 4-16:  Directory (13 sectors)
 *   Track 3, Sector 1-16:  Directory (continued, 16 sectors)
 *   Track 4+:              Data area (clusters)
 * 
 * @copyright MIT License
 * @author UFT Team
 */

#ifndef UFT_FBASIC_FS_H
#define UFT_FBASIC_FS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_FBASIC_SECTOR_SIZE      256     /**< Bytes per sector */
#define UFT_FBASIC_SECTORS_TRACK    16      /**< Sectors per track */
#define UFT_FBASIC_TRACKS_2D        40      /**< Tracks for 2D disk */
#define UFT_FBASIC_TRACKS_2DD       80      /**< Tracks for 2DD disk */
#define UFT_FBASIC_HEADS            2       /**< Number of heads */

#define UFT_FBASIC_CLUSTER_SECTORS  8       /**< Sectors per cluster */
#define UFT_FBASIC_CLUSTER_SIZE     (UFT_FBASIC_CLUSTER_SECTORS * UFT_FBASIC_SECTOR_SIZE)

#define UFT_FBASIC_DIR_ENTRY_SIZE   32      /**< Directory entry size */
#define UFT_FBASIC_MAX_FILENAME     8       /**< Max filename length */
#define UFT_FBASIC_MAX_DIR_ENTRIES  ((13 + 16) * UFT_FBASIC_SECTOR_SIZE / UFT_FBASIC_DIR_ENTRY_SIZE)

#define UFT_FBASIC_FAT_OFFSET       5       /**< FAT starts at byte 5 */
#define UFT_FBASIC_FAT_SIZE         152     /**< FAT entries (clusters) */

/* Track/Sector locations */
#define UFT_FBASIC_IPL_TRACK        0
#define UFT_FBASIC_IPL_SECTOR       1
#define UFT_FBASIC_ID_TRACK         0
#define UFT_FBASIC_ID_SECTOR        3
#define UFT_FBASIC_FAT_TRACK        2
#define UFT_FBASIC_FAT_SECTOR       1
#define UFT_FBASIC_DIR_TRACK        2
#define UFT_FBASIC_DIR_SECTOR       4
#define UFT_FBASIC_DATA_START_TRACK 4

/* FAT special values */
#define UFT_FBASIC_FAT_FREE         0xFF    /**< Cluster is free */
#define UFT_FBASIC_FAT_RESERVED     0xFE    /**< Reserved for system */
#define UFT_FBASIC_FAT_UNUSED       0xFD    /**< No sectors used */
#define UFT_FBASIC_FAT_LAST_MASK    0xC0    /**< Last cluster marker */
#define UFT_FBASIC_FAT_LAST_BASE    0xC0    /**< 0xC0-0xC7: last cluster */

/* File types */
typedef enum uft_fbasic_file_type {
    UFT_FBASIC_TYPE_BASIC_TEXT  = 0x00,     /**< BASIC program (tokenized) */
    UFT_FBASIC_TYPE_BASIC_DATA  = 0x01,     /**< BASIC data file */
    UFT_FBASIC_TYPE_MACHINE     = 0x02,     /**< Machine code */
} uft_fbasic_file_type_t;

/* File flags */
#define UFT_FBASIC_FLAG_BINARY      0x00    /**< Binary format */
#define UFT_FBASIC_FLAG_ASCII       0xFF    /**< ASCII format */
#define UFT_FBASIC_FLAG_SEQUENTIAL  0x00    /**< Sequential access */
#define UFT_FBASIC_FLAG_RANDOM      0xFF    /**< Random access */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Directory entry (32 bytes)
 */
typedef struct uft_fbasic_dir_entry {
    char        name[8];            /**< Filename (space-padded) */
    uint8_t     reserved1[3];       /**< Reserved */
    uint8_t     file_type;          /**< 0=BASIC, 1=Data, 2=Machine */
    uint8_t     ascii_flag;         /**< 0x00=Binary, 0xFF=ASCII */
    uint8_t     random_flag;        /**< 0x00=Sequential, 0xFF=Random */
    uint8_t     first_cluster;      /**< First cluster number */
    uint8_t     reserved2[17];      /**< Reserved */
} uft_fbasic_dir_entry_t;

/**
 * @brief Parsed file info
 */
typedef struct uft_fbasic_file_info {
    char        name[9];            /**< Null-terminated filename */
    uint8_t     file_type;          /**< File type */
    bool        is_ascii;           /**< ASCII flag */
    bool        is_random;          /**< Random access flag */
    uint8_t     first_cluster;      /**< First cluster */
    size_t      size;               /**< File size in bytes */
    uint8_t     dir_index;          /**< Directory index */
    bool        deleted;            /**< Entry is deleted */
} uft_fbasic_file_info_t;

/**
 * @brief Disk info
 */
typedef struct uft_fbasic_disk_info {
    char        id_string[4];       /**< "SYS" or similar */
    uint8_t     disk_type;          /**< 0x00=2D, 0x10=2DD */
    uint16_t    total_clusters;     /**< Total clusters */
    uint16_t    free_clusters;      /**< Free clusters */
    uint16_t    used_clusters;      /**< Used clusters */
    uint16_t    file_count;         /**< Number of files */
} uft_fbasic_disk_info_t;

/**
 * @brief F-BASIC filesystem context
 */
typedef struct uft_fbasic_fs {
    uint8_t    *disk_data;          /**< Raw disk image data */
    size_t      disk_size;          /**< Disk image size */
    uint8_t     tracks;             /**< Number of tracks */
    uint8_t     heads;              /**< Number of heads */
    bool        modified;           /**< Disk modified flag */
    
    /* Cached data */
    uint8_t     fat[256];           /**< FAT cache */
    uft_fbasic_dir_entry_t dir[UFT_FBASIC_MAX_DIR_ENTRIES]; /**< Directory cache */
    uint16_t    dir_count;          /**< Valid directory entries */
    
    /* Disk info */
    uft_fbasic_disk_info_t info;    /**< Disk information */
} uft_fbasic_fs_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open F-BASIC filesystem from disk image
 * @param fs Filesystem context to initialize
 * @param data Raw disk image data (D77/D88 sector data extracted)
 * @param size Size of disk data
 * @return 0 on success, error code on failure
 */
int uft_fbasic_open(uft_fbasic_fs_t *fs, uint8_t *data, size_t size);

/**
 * @brief Close filesystem and free resources
 */
void uft_fbasic_close(uft_fbasic_fs_t *fs);

/**
 * @brief Check if disk has valid F-BASIC format
 */
bool uft_fbasic_is_valid(const uint8_t *data, size_t size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Directory Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read directory from disk
 * @return Number of entries found, -1 on error
 */
int uft_fbasic_read_directory(uft_fbasic_fs_t *fs);

/**
 * @brief Get file info by index
 * @param fs Filesystem context
 * @param index Directory index (0-based)
 * @param info Output file info
 * @return 0 on success, -1 if not found
 */
int uft_fbasic_get_file_info(uft_fbasic_fs_t *fs, int index, 
                              uft_fbasic_file_info_t *info);

/**
 * @brief Find file by name
 * @param fs Filesystem context
 * @param name Filename to search (case-insensitive)
 * @param info Output file info
 * @return Directory index, -1 if not found
 */
int uft_fbasic_find_file(uft_fbasic_fs_t *fs, const char *name,
                          uft_fbasic_file_info_t *info);

/**
 * @brief Format directory listing as text
 * @param fs Filesystem context
 * @param out Output buffer
 * @param out_cap Buffer capacity
 * @return Number of bytes written
 */
size_t uft_fbasic_format_directory(uft_fbasic_fs_t *fs, char *out, size_t out_cap);

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read file data
 * @param fs Filesystem context
 * @param index Directory index
 * @param data Output: allocated buffer with file data (caller must free)
 * @param size Output: file size
 * @return 0 on success, error code on failure
 */
int uft_fbasic_read_file(uft_fbasic_fs_t *fs, int index,
                          uint8_t **data, size_t *size);

/**
 * @brief Read file by name
 */
int uft_fbasic_read_file_by_name(uft_fbasic_fs_t *fs, const char *name,
                                  uint8_t **data, size_t *size);

/**
 * @brief Write file to disk
 * @param fs Filesystem context
 * @param name Filename (max 8 chars)
 * @param data File data
 * @param size Data size
 * @param file_type File type (BASIC/Data/Machine)
 * @param is_ascii ASCII flag
 * @return 0 on success, error code on failure
 */
int uft_fbasic_write_file(uft_fbasic_fs_t *fs, const char *name,
                           const uint8_t *data, size_t size,
                           uft_fbasic_file_type_t file_type, bool is_ascii);

/**
 * @brief Delete file
 * @param fs Filesystem context
 * @param index Directory index
 * @return 0 on success, error code on failure
 */
int uft_fbasic_delete_file(uft_fbasic_fs_t *fs, int index);

/* ═══════════════════════════════════════════════════════════════════════════════
 * FAT Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read FAT from disk
 */
int uft_fbasic_read_fat(uft_fbasic_fs_t *fs);

/**
 * @brief Write FAT to disk
 */
int uft_fbasic_write_fat(uft_fbasic_fs_t *fs);

/**
 * @brief Get next cluster in chain
 * @return Next cluster, or -1 if end of chain
 */
int uft_fbasic_fat_next(uft_fbasic_fs_t *fs, uint8_t cluster);

/**
 * @brief Allocate free cluster
 * @return Cluster number, or -1 if disk full
 */
int uft_fbasic_fat_alloc(uft_fbasic_fs_t *fs);

/**
 * @brief Free cluster chain
 */
void uft_fbasic_fat_free_chain(uft_fbasic_fs_t *fs, uint8_t first_cluster);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Format disk with F-BASIC filesystem
 * @param fs Filesystem context (with allocated disk_data)
 * @param disk_name Disk name (optional, max 16 chars)
 * @return 0 on success
 */
int uft_fbasic_format(uft_fbasic_fs_t *fs, const char *disk_name);

/**
 * @brief Get disk information
 */
int uft_fbasic_get_info(uft_fbasic_fs_t *fs, uft_fbasic_disk_info_t *info);

/* ═══════════════════════════════════════════════════════════════════════════════
 * BASIC Program Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Decode BASIC tokenized program to text
 * @param tokens Tokenized BASIC data
 * @param token_size Size of token data
 * @param text Output text buffer
 * @param text_cap Text buffer capacity
 * @return Number of bytes written, -1 on error
 */
int uft_fbasic_decode_basic(const uint8_t *tokens, size_t token_size,
                             char *text, size_t text_cap);

/**
 * @brief Convert machine code to Motorola S-Record
 * @param data Machine code data
 * @param size Data size
 * @param load_addr Load address
 * @param srec Output S-Record buffer
 * @param srec_cap Buffer capacity
 * @return Number of bytes written
 */
int uft_fbasic_to_srec(const uint8_t *data, size_t size, uint16_t load_addr,
                        char *srec, size_t srec_cap);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sector I/O Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read sector from disk image
 * @param fs Filesystem context
 * @param track Track number
 * @param sector Sector number (1-based)
 * @param buffer Output buffer (256 bytes)
 * @return 0 on success
 */
int uft_fbasic_read_sector(uft_fbasic_fs_t *fs, uint8_t track, uint8_t sector,
                            uint8_t *buffer);

/**
 * @brief Write sector to disk image
 */
int uft_fbasic_write_sector(uft_fbasic_fs_t *fs, uint8_t track, uint8_t sector,
                             const uint8_t *buffer);

/**
 * @brief Convert cluster to track/sector
 */
void uft_fbasic_cluster_to_ts(uint8_t cluster, uint8_t *track, uint8_t *sector);

/**
 * @brief Convert track/sector to linear offset
 */
size_t uft_fbasic_ts_to_offset(uint8_t track, uint8_t sector);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FBASIC_FS_H */
