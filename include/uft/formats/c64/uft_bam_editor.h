/**
 * @file uft_bam_editor.h
 * @brief BAM (Block Allocation Map) Editor for C64 D64 Images
 * 
 * Complete BAM manipulation for 1541/1571 disk images:
 * - Read/write BAM entries
 * - Allocate/free blocks
 * - Disk validation and repair
 * - Directory operations
 * - Disk formatting
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_BAM_EDITOR_H
#define UFT_BAM_EDITOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** D64 sizes */
#define BAM_D64_35_TRACKS       174848      /**< 35 tracks, no errors */
#define BAM_D64_35_ERRORS       175531      /**< 35 tracks, with errors */
#define BAM_D64_40_TRACKS       196608      /**< 40 tracks, no errors */
#define BAM_D64_40_ERRORS       197376      /**< 40 tracks, with errors */

/** BAM location */
#define BAM_TRACK               18          /**< BAM is on track 18 */
#define BAM_SECTOR              0           /**< BAM is sector 0 */
#define BAM_OFFSET              0x16500     /**< BAM offset in D64 (track 18, sector 0) */

/** Directory location */
#define DIR_TRACK               18          /**< Directory track */
#define DIR_FIRST_SECTOR        1           /**< First directory sector */

/** Disk limits */
#define BAM_MAX_TRACKS          42          /**< Maximum tracks (extended) */
#define BAM_SECTORS_MAX         21          /**< Maximum sectors per track */
#define BAM_TOTAL_BLOCKS_35     683         /**< Total blocks (35 tracks) */
#define BAM_TOTAL_BLOCKS_40     768         /**< Total blocks (40 tracks) */

/** Directory entry size */
#define DIR_ENTRY_SIZE          32          /**< Bytes per directory entry */
#define DIR_ENTRIES_PER_SECTOR  8           /**< Directory entries per sector */
#define DIR_MAX_ENTRIES         144         /**< Maximum directory entries */

/** File types */
#define FILE_TYPE_DEL           0x00        /**< Deleted */
#define FILE_TYPE_SEQ           0x01        /**< Sequential */
#define FILE_TYPE_PRG           0x02        /**< Program */
#define FILE_TYPE_USR           0x03        /**< User */
#define FILE_TYPE_REL           0x04        /**< Relative */
#define FILE_TYPE_LOCKED        0x40        /**< Locked flag */
#define FILE_TYPE_CLOSED        0x80        /**< Properly closed flag */

/** DOS types */
#define DOS_TYPE_2A             "2A"        /**< Standard 1541 DOS */
#define DOS_TYPE_2C             "2C"        /**< 1571 DOS */

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Sectors per track table
 */
static const int bam_sectors_per_track[43] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /*  1 - 10 */
    21, 21, 21, 21, 21, 21, 21, 19, 19, 19,  /* 11 - 20 */
    19, 19, 19, 19, 18, 18, 18, 18, 18, 18,  /* 21 - 30 */
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17,  /* 31 - 40 */
    17, 17                                   /* 41 - 42 */
};

/**
 * @brief BAM entry for one track
 */
typedef struct {
    uint8_t     free_sectors;       /**< Number of free sectors */
    uint8_t     bitmap[3];          /**< Sector allocation bitmap */
} bam_track_entry_t;

/**
 * @brief Complete BAM structure
 */
typedef struct {
    uint8_t             dir_track;          /**< Directory track (18) */
    uint8_t             dir_sector;         /**< Directory first sector (1) */
    uint8_t             dos_version;        /**< DOS version ('A') */
    uint8_t             unused1;            /**< Unused (0x00) */
    bam_track_entry_t   tracks[BAM_MAX_TRACKS + 1];  /**< Track entries */
    char                disk_name[16];      /**< Disk name (PETSCII) */
    uint8_t             padding1[2];        /**< Padding (0xA0) */
    char                disk_id[2];         /**< Disk ID */
    uint8_t             padding2;           /**< Padding (0xA0) */
    char                dos_type[2];        /**< DOS type ("2A") */
    uint8_t             padding3[4];        /**< Padding (0xA0) */
} bam_t;

/**
 * @brief Directory entry
 */
typedef struct {
    uint8_t     next_track;         /**< Next directory track (0 = last) */
    uint8_t     next_sector;        /**< Next directory sector */
    uint8_t     file_type;          /**< File type and flags */
    uint8_t     first_track;        /**< First data track */
    uint8_t     first_sector;       /**< First data sector */
    char        filename[16];       /**< Filename (PETSCII, padded with 0xA0) */
    uint8_t     rel_side_track;     /**< REL side-sector track */
    uint8_t     rel_side_sector;    /**< REL side-sector sector */
    uint8_t     rel_record_len;     /**< REL record length */
    uint8_t     unused[6];          /**< Unused */
    uint16_t    file_size;          /**< File size in blocks (LE) */
} dir_entry_t;

/**
 * @brief Disk info structure
 */
typedef struct {
    char        disk_name[17];      /**< Disk name (null terminated) */
    char        disk_id[3];         /**< Disk ID (null terminated) */
    char        dos_type[3];        /**< DOS type (null terminated) */
    int         total_blocks;       /**< Total blocks */
    int         free_blocks;        /**< Free blocks */
    int         used_blocks;        /**< Used blocks */
    int         num_tracks;         /**< Number of tracks */
    int         num_files;          /**< Number of files */
    bool        has_errors;         /**< Error info present */
} disk_info_t;

/**
 * @brief File info structure
 */
typedef struct {
    char        filename[17];       /**< Filename (null terminated) */
    uint8_t     file_type;          /**< File type */
    uint16_t    blocks;             /**< Size in blocks */
    uint8_t     first_track;        /**< First data track */
    uint8_t     first_sector;       /**< First data sector */
    bool        locked;             /**< Locked flag */
    bool        closed;             /**< Properly closed */
    int         dir_index;          /**< Directory entry index */
} file_info_t;

/**
 * @brief BAM editor context
 */
typedef struct {
    uint8_t     *d64_data;          /**< D64 image data */
    size_t      d64_size;           /**< D64 size */
    bam_t       *bam;               /**< BAM pointer (into d64_data) */
    int         num_tracks;         /**< Number of tracks (35 or 40) */
    bool        has_errors;         /**< Error info present */
    bool        modified;           /**< Image modified */
} bam_editor_t;

/* ============================================================================
 * API Functions - Editor Management
 * ============================================================================ */

/**
 * @brief Create BAM editor from D64 data
 * @param d64_data D64 image data (will be modified in place)
 * @param d64_size D64 size
 * @return New editor, or NULL on error
 */
bam_editor_t *bam_editor_create(uint8_t *d64_data, size_t d64_size);

/**
 * @brief Free BAM editor (does not free D64 data)
 * @param editor Editor to free
 */
void bam_editor_free(bam_editor_t *editor);

/**
 * @brief Load D64 file into editor
 * @param filename D64 file path
 * @param editor Output editor
 * @return 0 on success
 */
int bam_editor_load(const char *filename, bam_editor_t **editor);

/**
 * @brief Save D64 to file
 * @param editor Editor
 * @param filename Output file path
 * @return 0 on success
 */
int bam_editor_save(const bam_editor_t *editor, const char *filename);

/* ============================================================================
 * API Functions - Disk Info
 * ============================================================================ */

/**
 * @brief Get disk information
 * @param editor Editor
 * @param info Output info
 * @return 0 on success
 */
int bam_get_disk_info(const bam_editor_t *editor, disk_info_t *info);

/**
 * @brief Set disk name
 * @param editor Editor
 * @param name New disk name (max 16 chars)
 * @return 0 on success
 */
int bam_set_disk_name(bam_editor_t *editor, const char *name);

/**
 * @brief Set disk ID
 * @param editor Editor
 * @param id New disk ID (2 chars)
 * @return 0 on success
 */
int bam_set_disk_id(bam_editor_t *editor, const char *id);

/**
 * @brief Get free blocks count
 * @param editor Editor
 * @return Free blocks, or -1 on error
 */
int bam_get_free_blocks(const bam_editor_t *editor);

/* ============================================================================
 * API Functions - Block Allocation
 * ============================================================================ */

/**
 * @brief Check if block is free
 * @param editor Editor
 * @param track Track number (1-42)
 * @param sector Sector number
 * @return true if free
 */
bool bam_is_block_free(const bam_editor_t *editor, int track, int sector);

/**
 * @brief Allocate block
 * @param editor Editor
 * @param track Track number
 * @param sector Sector number
 * @return 0 on success
 */
int bam_allocate_block(bam_editor_t *editor, int track, int sector);

/**
 * @brief Free block
 * @param editor Editor
 * @param track Track number
 * @param sector Sector number
 * @return 0 on success
 */
int bam_free_block(bam_editor_t *editor, int track, int sector);

/**
 * @brief Find and allocate next free block
 * @param editor Editor
 * @param start_track Starting track (or 0 for any)
 * @param track Output: allocated track
 * @param sector Output: allocated sector
 * @return 0 on success, -1 if disk full
 */
int bam_allocate_next_free(bam_editor_t *editor, int start_track,
                           int *track, int *sector);

/**
 * @brief Get free sectors on track
 * @param editor Editor
 * @param track Track number
 * @return Free sectors count, or -1 on error
 */
int bam_get_track_free(const bam_editor_t *editor, int track);

/* ============================================================================
 * API Functions - Directory Operations
 * ============================================================================ */

/**
 * @brief Get file count
 * @param editor Editor
 * @return Number of files
 */
int bam_get_file_count(const bam_editor_t *editor);

/**
 * @brief Get file info by index
 * @param editor Editor
 * @param index File index (0-based)
 * @param info Output file info
 * @return 0 on success
 */
int bam_get_file_info(const bam_editor_t *editor, int index, file_info_t *info);

/**
 * @brief Find file by name
 * @param editor Editor
 * @param filename Filename to find
 * @param info Output file info
 * @return 0 if found, -1 if not found
 */
int bam_find_file(const bam_editor_t *editor, const char *filename, file_info_t *info);

/**
 * @brief Delete file
 * @param editor Editor
 * @param filename Filename to delete
 * @return 0 on success
 */
int bam_delete_file(bam_editor_t *editor, const char *filename);

/**
 * @brief Rename file
 * @param editor Editor
 * @param old_name Current filename
 * @param new_name New filename
 * @return 0 on success
 */
int bam_rename_file(bam_editor_t *editor, const char *old_name, const char *new_name);

/**
 * @brief Lock/unlock file
 * @param editor Editor
 * @param filename Filename
 * @param locked true to lock, false to unlock
 * @return 0 on success
 */
int bam_set_file_locked(bam_editor_t *editor, const char *filename, bool locked);

/* ============================================================================
 * API Functions - Validation and Repair
 * ============================================================================ */

/**
 * @brief Validate BAM
 * @param editor Editor
 * @param errors Output: error count
 * @param messages Output buffer for messages (can be NULL)
 * @param msg_size Message buffer size
 * @return 0 if valid, number of errors otherwise
 */
int bam_validate(const bam_editor_t *editor, int *errors,
                 char *messages, size_t msg_size);

/**
 * @brief Repair BAM (recalculate from directory)
 * @param editor Editor
 * @param fixed Output: blocks fixed
 * @return 0 on success
 */
int bam_repair(bam_editor_t *editor, int *fixed);

/**
 * @brief Check for cross-linked files
 * @param editor Editor
 * @param messages Output buffer for messages
 * @param msg_size Message buffer size
 * @return Number of cross-links found
 */
int bam_check_crosslinks(const bam_editor_t *editor,
                         char *messages, size_t msg_size);

/* ============================================================================
 * API Functions - Formatting
 * ============================================================================ */

/**
 * @brief Format disk (initialize BAM and directory)
 * @param editor Editor
 * @param disk_name Disk name
 * @param disk_id Disk ID (2 chars)
 * @return 0 on success
 */
int bam_format_disk(bam_editor_t *editor, const char *disk_name, const char *disk_id);

/**
 * @brief Create empty D64 image
 * @param tracks Number of tracks (35 or 40)
 * @param disk_name Disk name
 * @param disk_id Disk ID
 * @param d64_data Output: allocated D64 data
 * @param d64_size Output: D64 size
 * @return 0 on success
 */
int bam_create_d64(int tracks, const char *disk_name, const char *disk_id,
                   uint8_t **d64_data, size_t *d64_size);

/* ============================================================================
 * API Functions - Sector I/O
 * ============================================================================ */

/**
 * @brief Read sector
 * @param editor Editor
 * @param track Track number
 * @param sector Sector number
 * @param buffer Output buffer (256 bytes)
 * @return 0 on success
 */
int bam_read_sector(const bam_editor_t *editor, int track, int sector,
                    uint8_t *buffer);

/**
 * @brief Write sector
 * @param editor Editor
 * @param track Track number
 * @param sector Sector number
 * @param buffer Input buffer (256 bytes)
 * @return 0 on success
 */
int bam_write_sector(bam_editor_t *editor, int track, int sector,
                     const uint8_t *buffer);

/**
 * @brief Get sector offset in D64
 * @param track Track number
 * @param sector Sector number
 * @return Byte offset, or -1 on error
 */
int bam_sector_offset(int track, int sector);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Convert ASCII to PETSCII
 * @param ascii ASCII string
 * @param petscii Output PETSCII buffer
 * @param len Maximum length
 */
void bam_ascii_to_petscii(const char *ascii, char *petscii, size_t len);

/**
 * @brief Convert PETSCII to ASCII
 * @param petscii PETSCII string
 * @param ascii Output ASCII buffer
 * @param len Maximum length
 */
void bam_petscii_to_ascii(const char *petscii, char *ascii, size_t len);

/**
 * @brief Get file type name
 * @param file_type File type byte
 * @return Type name string
 */
const char *bam_file_type_name(uint8_t file_type);

/**
 * @brief Get sectors per track
 * @param track Track number
 * @return Sectors per track
 */
int bam_sectors_for_track(int track);

/**
 * @brief Print directory listing
 * @param editor Editor
 * @param fp Output file
 */
void bam_print_directory(const bam_editor_t *editor, FILE *fp);

/**
 * @brief Print BAM map
 * @param editor Editor
 * @param fp Output file
 */
void bam_print_map(const bam_editor_t *editor, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BAM_EDITOR_H */
