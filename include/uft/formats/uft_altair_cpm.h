/**
 * @file uft_altair_cpm.h
 * @brief Altair CP/M Disk Format Support
 * 
 * Based on altair_tools by Paul Hatchman
 * License: MIT
 * 
 * Supports MITS 8" floppy, 5MB HDD, and Tarbell formats.
 */

#ifndef UFT_ALTAIR_CPM_H
#define UFT_ALTAIR_CPM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_CPM_MAX_DIRS        1024    /**< Maximum directory entries */
#define UFT_CPM_MAX_ALLOCS      2048    /**< Maximum allocation blocks */
#define UFT_CPM_DIR_ENTRY_LEN   32      /**< Directory entry size */
#define UFT_CPM_ALLOCS_PER_EXT  16      /**< Allocations per extent */
#define UFT_CPM_MAX_RECORDS     128     /**< Records per extent */
#define UFT_CPM_FILENAME_LEN    8       /**< Filename length */
#define UFT_CPM_TYPE_LEN        3       /**< Extension length */
#define UFT_CPM_DELETED_FLAG    0xE5    /**< Deleted entry marker */
#define UFT_CPM_MAX_USER        15      /**< Maximum user number */

/*===========================================================================
 * Raw Sector Offset Structure (MITS 8" format)
 *===========================================================================*/

/**
 * @brief Byte offsets within raw MITS 8" sectors
 */
typedef struct {
    int start_track;    /**< First track this applies to */
    int end_track;      /**< Last track this applies to */
    int off_data;       /**< Offset to data portion */
    int off_track_nr;   /**< Offset to track number */
    int off_sect_nr;    /**< Offset to sector number */
    int off_stop;       /**< Offset to stop byte */
    int off_zero;       /**< Offset to zero byte */
    int off_csum;       /**< Offset to checksum */
    int csum_method;    /**< Checksum algorithm (0 or 1) */
} uft_cpm_sector_offsets_t;

/*===========================================================================
 * Disk Format Parameters
 *===========================================================================*/

/**
 * @brief CP/M disk format definition
 */
typedef struct {
    const char *name;           /**< Format name string */
    uint16_t sector_len;        /**< Total sector length (bytes) */
    uint16_t sector_data_len;   /**< Data portion length (128) */
    uint16_t num_tracks;        /**< Total tracks */
    uint16_t reserved_tracks;   /**< OS reserved tracks */
    uint16_t sectors_per_track; /**< Sectors per track */
    uint16_t block_size;        /**< CP/M allocation block size */
    uint16_t num_directories;   /**< Maximum directory entries */
    uint16_t directory_allocs;  /**< Blocks reserved for directory */
    uint32_t image_size;        /**< Expected image file size */
    const int *skew_table;      /**< Sector interleave table */
    int skew_table_size;        /**< Number of entries in skew table */
    bool has_raw_offsets;       /**< True if MITS 8" style */
    uft_cpm_sector_offsets_t offsets[2]; /**< Raw sector byte offsets */
} uft_cpm_format_t;

/*===========================================================================
 * Skew Tables
 *===========================================================================*/

/** MITS 8" sector skew (32 sectors, 1-based) */
static const int uft_mits_skew_table[] = {
     1,  9, 17, 25,  3, 11, 19, 27,
     5, 13, 21, 29,  7, 15, 23, 31,
     2, 10, 18, 26,  4, 12, 20, 28,
     6, 14, 22, 30,  8, 16, 24, 32
};

/** 5MB HDD skew (96 CP/M sectors, 0-based) */
static const int uft_hd5mb_skew_table[] = {
     0,  1, 14, 15, 28, 29, 42, 43,  8,  9, 22, 23,
    36, 37,  2,  3, 16, 17, 30, 31, 44, 45, 10, 11,
    24, 25, 38, 39,  4,  5, 18, 19, 32, 33, 46, 47,
    12, 13, 26, 27, 40, 41,  6,  7, 20, 21, 34, 35,
    48, 49, 62, 63, 76, 77, 90, 91, 56, 57, 70, 71,
    84, 85, 50, 51, 64, 65, 78, 79, 92, 93, 58, 59,
    72, 73, 86, 87, 52, 53, 66, 67, 80, 81, 94, 95,
    60, 61, 74, 75, 88, 89, 54, 55, 68, 69, 82, 83
};

/** Tarbell skew (26 sectors, 0-based) */
static const int uft_tarbell_skew_table[] = {
     0,  6, 12, 18, 24,  4,
    10, 16, 22,  2,  8, 14,
    20,  1,  7, 13, 19, 25,
     5, 11, 17, 23,  3,  9,
    15, 21
};

/*===========================================================================
 * Format Definitions
 *===========================================================================*/

/** MITS 8" standard floppy */
static const uft_cpm_format_t UFT_CPM_MITS8IN = {
    .name = "FDD_8IN",
    .sector_len = 137,
    .sector_data_len = 128,
    .num_tracks = 77,
    .reserved_tracks = 2,
    .sectors_per_track = 32,
    .block_size = 2048,
    .num_directories = 64,
    .directory_allocs = 2,
    .image_size = 337568,
    .skew_table = uft_mits_skew_table,
    .skew_table_size = 32,
    .has_raw_offsets = true,
    .offsets = {
        { 0,  5, 3, 0, 0, 131, 133, 132, 0 },
        { 6, 77, 7, 0, 1, 135, 136,   4, 1 }
    }
};

/** MITS 8" 8MB extended format */
static const uft_cpm_format_t UFT_CPM_MITS8IN_8MB = {
    .name = "FDD_8IN_8MB",
    .sector_len = 137,
    .sector_data_len = 128,
    .num_tracks = 2048,
    .reserved_tracks = 2,
    .sectors_per_track = 32,
    .block_size = 4096,
    .num_directories = 512,
    .directory_allocs = 4,
    .image_size = 8978432,
    .skew_table = uft_mits_skew_table,
    .skew_table_size = 32,
    .has_raw_offsets = true,
    .offsets = {
        { 0,  5, 3, 0, 0, 131, 133, 132, 0 },
        { 6, 77, 7, 0, 1, 135, 136,   4, 1 }
    }
};

/** MITS 5MB hard disk */
static const uft_cpm_format_t UFT_CPM_MITS5MB_HDD = {
    .name = "HDD_5MB",
    .sector_len = 128,
    .sector_data_len = 128,
    .num_tracks = 406,
    .reserved_tracks = 1,
    .sectors_per_track = 96,
    .block_size = 4096,
    .num_directories = 256,
    .directory_allocs = 2,
    .image_size = 4988928,
    .skew_table = uft_hd5mb_skew_table,
    .skew_table_size = 96,
    .has_raw_offsets = false
};

/** MITS 5MB HDD with 1024 directories */
static const uft_cpm_format_t UFT_CPM_MITS5MB_HDD_1024 = {
    .name = "HDD_5MB_1024",
    .sector_len = 128,
    .sector_data_len = 128,
    .num_tracks = 406,
    .reserved_tracks = 1,
    .sectors_per_track = 96,
    .block_size = 4096,
    .num_directories = 1024,
    .directory_allocs = 8,
    .image_size = 4988928,
    .skew_table = uft_hd5mb_skew_table,
    .skew_table_size = 96,
    .has_raw_offsets = false
};

/** Tarbell floppy */
static const uft_cpm_format_t UFT_CPM_TARBELL = {
    .name = "FDD_TAR",
    .sector_len = 128,
    .sector_data_len = 128,
    .num_tracks = 77,
    .reserved_tracks = 2,
    .sectors_per_track = 26,
    .block_size = 1024,
    .num_directories = 64,
    .directory_allocs = 2,
    .image_size = 256256,
    .skew_table = uft_tarbell_skew_table,
    .skew_table_size = 26,
    .has_raw_offsets = false
};

/*===========================================================================
 * Directory Entry Structures
 *===========================================================================*/

/**
 * @brief Raw CP/M directory entry (on-disk format)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t user;                           /**< User (0-15), 0xE5 = deleted */
    char    filename[UFT_CPM_FILENAME_LEN]; /**< Filename (space padded) */
    char    type[UFT_CPM_TYPE_LEN];         /**< Extension (space padded) */
    uint8_t extent_l;                       /**< Extent low (0-31) */
    uint8_t reserved;                       /**< Reserved byte */
    uint8_t extent_h;                       /**< Extent high (32x) */
    uint8_t num_records;                    /**< Records in extent (0-128) */
    uint8_t allocation[UFT_CPM_ALLOCS_PER_EXT]; /**< Block allocations */
} uft_cpm_raw_dir_entry_t;
#pragma pack(pop)

/**
 * @brief Parsed CP/M directory entry
 */
typedef struct uft_cpm_dir_entry {
    int index;                              /**< Directory entry number */
    bool valid;                             /**< Entry is in use */
    uft_cpm_raw_dir_entry_t raw;            /**< Raw on-disk data */
    int extent_nr;                          /**< Combined extent number */
    int user;                               /**< User number */
    char filename[UFT_CPM_FILENAME_LEN + 1];/**< Null-terminated filename */
    char type[UFT_CPM_TYPE_LEN + 1];        /**< Null-terminated extension */
    char full_filename[UFT_CPM_FILENAME_LEN + UFT_CPM_TYPE_LEN + 2];
    bool read_only;                         /**< Read-only attribute */
    bool system;                            /**< System file attribute */
    bool archived;                          /**< Archived attribute */
    int num_records;                        /**< Records in this extent */
    int num_allocs;                         /**< Active allocations */
    int allocation[8];                      /**< Parsed allocation blocks */
    struct uft_cpm_dir_entry *next_extent;  /**< Next extent of file */
} uft_cpm_dir_entry_t;

/*===========================================================================
 * Disk Context
 *===========================================================================*/

/**
 * @brief CP/M disk context
 */
typedef struct {
    const uft_cpm_format_t *format;         /**< Disk format */
    uint8_t *image;                         /**< Raw image data */
    size_t image_size;                      /**< Image size */
    uft_cpm_dir_entry_t directory[UFT_CPM_MAX_DIRS];
    int directory_count;                    /**< Parsed entries */
    uint8_t alloc_map[UFT_CPM_MAX_ALLOCS];  /**< Allocation bitmap */
} uft_cpm_disk_t;

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Calculate total allocation blocks
 */
static inline int uft_cpm_total_allocs(const uft_cpm_format_t *fmt) {
    return (fmt->num_tracks - fmt->reserved_tracks) *
           fmt->sectors_per_track * fmt->sector_data_len /
           fmt->block_size;
}

/**
 * @brief Calculate records per allocation block
 */
static inline int uft_cpm_recs_per_alloc(const uft_cpm_format_t *fmt) {
    return fmt->block_size / fmt->sector_data_len;
}

/**
 * @brief Calculate records per extent
 */
static inline int uft_cpm_recs_per_extent(const uft_cpm_format_t *fmt) {
    int rpa = uft_cpm_recs_per_alloc(fmt);
    return ((rpa * 8) + 127) / 128 * 128;
}

/**
 * @brief Calculate directory entries per sector
 */
static inline int uft_cpm_dirs_per_sector(const uft_cpm_format_t *fmt) {
    return fmt->sector_data_len / UFT_CPM_DIR_ENTRY_LEN;
}

/**
 * @brief Apply MITS 8" sector skew
 * @param track Track number
 * @param logical_sector Logical sector (0-based)
 * @return Physical sector number (1-based)
 */
static inline int uft_cpm_mits_skew(int track, int logical_sector) {
    int base = uft_mits_skew_table[logical_sector];
    if (track < 6) {
        return base;
    }
    /* Additional track 6+ skew */
    return (((base - 1) * 17) % 32) + 1;
}

/**
 * @brief Get sector byte offsets for MITS 8" format
 */
static inline const uft_cpm_sector_offsets_t *
uft_cpm_get_offsets(const uft_cpm_format_t *fmt, int track) {
    if (!fmt->has_raw_offsets) return NULL;
    if (track >= fmt->offsets[0].start_track &&
        track <= fmt->offsets[0].end_track) {
        return &fmt->offsets[0];
    }
    return &fmt->offsets[1];
}

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Detect format by image size
 * @param size Image file size
 * @return Matching format or NULL
 */
const uft_cpm_format_t *uft_cpm_detect_format(size_t size);

/**
 * @brief Initialize disk context
 * @param disk Context to initialize
 * @param format Disk format
 * @param image Raw image data
 * @param size Image size
 * @return 0 on success, -1 on error
 */
int uft_cpm_init(uft_cpm_disk_t *disk, const uft_cpm_format_t *format,
                 uint8_t *image, size_t size);

/**
 * @brief Read directory from disk image
 * @param disk Initialized disk context
 * @return Number of entries read, or -1 on error
 */
int uft_cpm_read_directory(uft_cpm_disk_t *disk);

/**
 * @brief Extract file from disk image
 * @param disk Disk context
 * @param entry Directory entry to extract
 * @param buffer Output buffer (must be large enough)
 * @param max_size Maximum bytes to extract
 * @return Bytes extracted, or -1 on error
 */
int uft_cpm_extract_file(const uft_cpm_disk_t *disk,
                         const uft_cpm_dir_entry_t *entry,
                         uint8_t *buffer, size_t max_size);

/**
 * @brief Get file size in bytes
 * @param disk Disk context
 * @param entry First extent of file
 * @return File size in bytes
 */
size_t uft_cpm_file_size(const uft_cpm_disk_t *disk,
                         const uft_cpm_dir_entry_t *entry);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ALTAIR_CPM_H */
