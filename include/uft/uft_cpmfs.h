/**
 * @file uft_cpmfs.h
 * @brief CP/M Filesystem Support
 * 
 * Based on cpmtools by Michael Haardt
 * License: GPL
 * 
 * Supports CP/M 2.2, CP/M 3, P2DOS, ZSDOS filesystems.
 */

#ifndef UFT_CPMFS_H
#define UFT_CPMFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_CPM_DIR_ENTRY_SIZE    32      /**< Directory entry size */
#define UFT_CPM_FILENAME_LEN      8       /**< Filename length */
#define UFT_CPM_EXTENSION_LEN     3       /**< Extension length */
#define UFT_CPM_EXTENT_SIZE       16384   /**< Bytes per logical extent */
#define UFT_CPM_RECORD_SIZE       128     /**< CP/M record size */
#define UFT_CPM_MAX_USER          15      /**< Maximum user number */
#define UFT_CPM_DELETED           0xE5    /**< Deleted entry marker */

/*===========================================================================
 * File Attributes
 *===========================================================================*/

#define UFT_CPM_ATTR_F1       0x0001    /**< User attribute F1 */
#define UFT_CPM_ATTR_F2       0x0002    /**< User attribute F2 */
#define UFT_CPM_ATTR_F3       0x0004    /**< User attribute F3 */
#define UFT_CPM_ATTR_F4       0x0008    /**< User attribute F4 */
#define UFT_CPM_ATTR_RO       0x0100    /**< Read-only */
#define UFT_CPM_ATTR_SYS      0x0200    /**< System file */
#define UFT_CPM_ATTR_ARCV     0x0400    /**< Archive */
#define UFT_CPM_ATTR_PWDEL    0x0800    /**< Password to delete */
#define UFT_CPM_ATTR_PWWRITE  0x1000    /**< Password to write */
#define UFT_CPM_ATTR_PWREAD   0x2000    /**< Password to read */

/*===========================================================================
 * Filesystem Types
 *===========================================================================*/

#define UFT_CPMFS_DR22      0x01    /**< CP/M 2.2 with high user numbers */
#define UFT_CPMFS_P2DOS     0x03    /**< P2DOS with timestamps */
#define UFT_CPMFS_DR3       0x07    /**< CP/M 3 with passwords */
#define UFT_CPMFS_ISX       0x10    /**< ISX with exact file size */
#define UFT_CPMFS_ZSYS      0x01    /**< ZSDOS */

/*===========================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief CP/M directory entry (on-disk format)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t user;              /**< User number (0-15, 0xE5=deleted) */
    char    name[8];           /**< Filename (space-padded) */
    char    ext[3];            /**< Extension (space-padded, high bits = attr) */
    uint8_t extent_l;          /**< Extent low byte (0-31) */
    uint8_t s1;                /**< Reserved / exact size high byte */
    uint8_t extent_h;          /**< Extent high byte */
    uint8_t records;           /**< Records in this extent (0-128) */
    uint8_t alloc[16];         /**< Allocation block numbers */
} uft_cpm_dirent_t;
#pragma pack(pop)

/**
 * @brief DateStamper timestamp entry
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t year;    /**< Year (BCD, 0-99) */
    uint8_t month;   /**< Month (BCD, 1-12) */
    uint8_t day;     /**< Day (BCD, 1-31) */
    uint8_t hour;    /**< Hour (BCD, 0-23) */
    uint8_t minute;  /**< Minute (BCD, 0-59) */
} uft_cpm_timestamp_t;
#pragma pack(pop)

/**
 * @brief DateStamper date record
 */
#pragma pack(push, 1)
typedef struct {
    uft_cpm_timestamp_t create;   /**< Creation time */
    uft_cpm_timestamp_t access;   /**< Access time */
    uft_cpm_timestamp_t modify;   /**< Modification time */
    uint8_t checksum;             /**< XOR checksum */
} uft_cpm_dsdate_t;
#pragma pack(pop)

/**
 * @brief CP/M disk parameters
 */
typedef struct {
    const char *name;        /**< Format name */
    int sec_length;          /**< Sector size in bytes */
    int tracks;              /**< Total tracks */
    int sec_trk;             /**< Sectors per track */
    int blk_size;            /**< Block size (1K, 2K, 4K, 8K, 16K) */
    int max_dir;             /**< Maximum directory entries */
    int skew;                /**< Sector skew */
    int boot_trk;            /**< Reserved boot tracks */
    int offset;              /**< Byte offset to start */
    int type;                /**< Filesystem type flags */
} uft_cpm_dpb_t;

/**
 * @brief CP/M file info
 */
typedef struct {
    char     name[9];        /**< Null-terminated filename */
    char     ext[4];         /**< Null-terminated extension */
    uint8_t  user;           /**< User number */
    uint16_t attr;           /**< File attributes */
    uint32_t size;           /**< File size in bytes */
    time_t   atime;          /**< Access time */
    time_t   mtime;          /**< Modification time */
    time_t   ctime;          /**< Creation time */
    int      extents;        /**< Number of extents */
} uft_cpm_fileinfo_t;

/**
 * @brief CP/M filesystem context
 */
typedef struct {
    const uft_cpm_dpb_t *dpb;    /**< Disk parameter block */
    uint8_t *image;              /**< Disk image data */
    size_t image_size;           /**< Image size */
    
    /* Computed parameters */
    int dir_blocks;              /**< Directory blocks */
    int total_blocks;            /**< Total allocation blocks */
    int extents_per_entry;       /**< Logical extents per physical */
    int block_entries;           /**< Entries per block (8 or 16) */
    
    /* Directory cache */
    uft_cpm_dirent_t *directory; /**< Directory entries */
    int dir_count;               /**< Directory entry count */
    
    /* Allocation map */
    uint8_t *alloc_map;          /**< Block allocation bitmap */
    
    /* DateStamper */
    uft_cpm_dsdate_t *ds_dates;  /**< DateStamper records */
    bool has_datestamper;        /**< DateStamper present */
} uft_cpm_fs_t;

/*===========================================================================
 * Predefined Disk Parameters
 *===========================================================================*/

/** Standard 8" SSSD (IBM 3740) */
static const uft_cpm_dpb_t UFT_CPM_DPB_IBM3740 = {
    .name = "ibm-3740",
    .sec_length = 128,
    .tracks = 77,
    .sec_trk = 26,
    .blk_size = 1024,
    .max_dir = 64,
    .skew = 6,
    .boot_trk = 2,
    .offset = 0,
    .type = 0
};

/** Standard 5.25" DSDD (Osborne 1) */
static const uft_cpm_dpb_t UFT_CPM_DPB_OSBORNE = {
    .name = "osborne1",
    .sec_length = 256,
    .tracks = 40,
    .sec_trk = 10,
    .blk_size = 1024,
    .max_dir = 64,
    .skew = 0,
    .boot_trk = 3,
    .offset = 0,
    .type = 0
};

/** Amstrad PCW/CPC 3" */
static const uft_cpm_dpb_t UFT_CPM_DPB_PCW = {
    .name = "pcw",
    .sec_length = 512,
    .tracks = 80,
    .sec_trk = 9,
    .blk_size = 1024,
    .max_dir = 64,
    .skew = 0,
    .boot_trk = 1,
    .offset = 0,
    .type = 0
};

/** Kaypro II */
static const uft_cpm_dpb_t UFT_CPM_DPB_KAYPRO2 = {
    .name = "kaypro2",
    .sec_length = 512,
    .tracks = 40,
    .sec_trk = 10,
    .blk_size = 2048,
    .max_dir = 64,
    .skew = 0,
    .boot_trk = 1,
    .offset = 0,
    .type = 0
};

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Convert BCD to decimal
 */
static inline int uft_cpm_bcd_to_dec(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/**
 * @brief Convert decimal to BCD
 */
static inline uint8_t uft_cpm_dec_to_bcd(int dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

/**
 * @brief Check if directory entry is deleted
 */
static inline bool uft_cpm_is_deleted(const uft_cpm_dirent_t *entry) {
    return entry->user == UFT_CPM_DELETED;
}

/**
 * @brief Get extent number from directory entry
 */
static inline int uft_cpm_get_extent(const uft_cpm_dirent_t *entry) {
    return ((int)entry->extent_h << 5) | entry->extent_l;
}

/**
 * @brief Extract file attributes from extension bytes
 */
static inline uint16_t uft_cpm_get_attr(const uft_cpm_dirent_t *entry) {
    uint16_t attr = 0;
    if (entry->ext[0] & 0x80) attr |= UFT_CPM_ATTR_RO;
    if (entry->ext[1] & 0x80) attr |= UFT_CPM_ATTR_SYS;
    if (entry->ext[2] & 0x80) attr |= UFT_CPM_ATTR_ARCV;
    return attr;
}

/**
 * @brief Calculate block number from allocation entry
 * @param dpb Disk parameters
 * @param alloc Allocation array
 * @param index Entry index
 * @return Block number (0 = not allocated)
 */
static inline int uft_cpm_get_block(const uft_cpm_dpb_t *dpb,
                                    const uint8_t *alloc, int index) {
    /* If total blocks <= 256, use 8-bit entries */
    int total = (dpb->tracks - dpb->boot_trk) * dpb->sec_trk * 
                dpb->sec_length / dpb->blk_size;
    
    if (total <= 256) {
        return alloc[index];
    } else {
        /* 16-bit entries (little-endian) */
        return alloc[index * 2] | (alloc[index * 2 + 1] << 8);
    }
}

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize CP/M filesystem from disk image
 * @param fs Filesystem context to initialize
 * @param dpb Disk parameter block
 * @param image Disk image data
 * @param size Image size
 * @return 0 on success, -1 on error
 */
int uft_cpm_init(uft_cpm_fs_t *fs, const uft_cpm_dpb_t *dpb,
                 uint8_t *image, size_t size);

/**
 * @brief Free filesystem context
 */
void uft_cpm_free(uft_cpm_fs_t *fs);

/**
 * @brief Read directory from filesystem
 * @param fs Initialized filesystem
 * @return Number of entries, or -1 on error
 */
int uft_cpm_read_dir(uft_cpm_fs_t *fs);

/**
 * @brief Get file information
 * @param fs Filesystem
 * @param user User number (0-15)
 * @param name Filename (8 chars max, space-padded)
 * @param ext Extension (3 chars max, space-padded)
 * @param info Output file info
 * @return 0 on success, -1 if not found
 */
int uft_cpm_stat(const uft_cpm_fs_t *fs, int user,
                 const char *name, const char *ext,
                 uft_cpm_fileinfo_t *info);

/**
 * @brief Read file contents
 * @param fs Filesystem
 * @param user User number
 * @param name Filename
 * @param ext Extension
 * @param buffer Output buffer
 * @param max_size Maximum bytes to read
 * @return Bytes read, or -1 on error
 */
int uft_cpm_read_file(const uft_cpm_fs_t *fs, int user,
                      const char *name, const char *ext,
                      uint8_t *buffer, size_t max_size);

/**
 * @brief Get filesystem statistics
 * @param fs Filesystem
 * @param total_blocks Output: total blocks
 * @param free_blocks Output: free blocks
 * @param total_files Output: total files
 */
void uft_cpm_statfs(const uft_cpm_fs_t *fs,
                    int *total_blocks, int *free_blocks, int *total_files);

/**
 * @brief Iterate directory entries
 * @param fs Filesystem
 * @param callback Called for each file (return 0 to continue, non-zero to stop)
 * @param user_data Passed to callback
 */
typedef int (*uft_cpm_dir_callback_t)(const uft_cpm_fileinfo_t *info, void *user_data);
int uft_cpm_iterate_dir(const uft_cpm_fs_t *fs,
                        uft_cpm_dir_callback_t callback,
                        void *user_data);

/**
 * @brief Convert CP/M timestamp to time_t
 * @param ts CP/M timestamp
 * @return Unix timestamp
 */
time_t uft_cpm_timestamp_to_time(const uft_cpm_timestamp_t *ts);

/**
 * @brief Detect CP/M filesystem type from image
 * @param image Disk image data
 * @param size Image size
 * @return Best matching DPB or NULL
 */
const uft_cpm_dpb_t *uft_cpm_detect_format(const uint8_t *image, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CPMFS_H */
