/**
 * @file uft_c64_bam.h
 * @brief C64 Block Allocation Map (BAM) Editor
 * 
 * Complete BAM manipulation for D64/D71/D81 disk images:
 * - BAM reading and writing
 * - Block allocation/deallocation
 * - Disk directory operations
 * - Free space calculation
 * - BAM validation and repair
 * 
 * BAM Location:
 * - D64: Track 18, Sector 0
 * - D71: Track 18 (side 0) + Track 53 (side 1)
 * - D81: Track 40, Sectors 1-2
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_C64_BAM_H
#define UFT_C64_BAM_H

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

/** D64 Format */
#define BAM_D64_TRACK           18      /**< BAM track for D64 */
#define BAM_D64_SECTOR          0       /**< BAM sector for D64 */
#define BAM_D64_TRACKS_35       35      /**< Standard D64 tracks */
#define BAM_D64_TRACKS_40       40      /**< Extended D64 tracks */
#define BAM_D64_SIZE_35         174848  /**< D64 35-track size */
#define BAM_D64_SIZE_40         196608  /**< D64 40-track size */

/** D71 Format (double-sided 1571) */
#define BAM_D71_TRACK_0         18      /**< BAM track side 0 */
#define BAM_D71_TRACK_1         53      /**< BAM track side 1 */
#define BAM_D71_TRACKS          70      /**< Total D71 tracks */

/** D81 Format (3.5" 1581) */
#define BAM_D81_TRACK           40      /**< BAM track for D81 */
#define BAM_D81_SECTOR_1        1       /**< First BAM sector */
#define BAM_D81_SECTOR_2        2       /**< Second BAM sector */
#define BAM_D81_TRACKS          80      /**< D81 tracks */
#define BAM_D81_SECTORS         40      /**< Sectors per track */

/** Sector size */
#define BAM_SECTOR_SIZE         256     /**< Bytes per sector */

/** Directory */
#define BAM_DIR_TRACK           18      /**< Directory track */
#define BAM_DIR_SECTOR          1       /**< First directory sector */
#define BAM_DIR_ENTRIES         8       /**< Entries per sector */
#define BAM_DIR_ENTRY_SIZE      32      /**< Bytes per entry */

/** File types */
#define BAM_FILE_DEL            0x00    /**< Deleted */
#define BAM_FILE_SEQ            0x01    /**< Sequential */
#define BAM_FILE_PRG            0x02    /**< Program */
#define BAM_FILE_USR            0x03    /**< User */
#define BAM_FILE_REL            0x04    /**< Relative */
#define BAM_FILE_CBM            0x05    /**< CBM partition (D81) */
#define BAM_FILE_LOCKED         0x40    /**< Locked flag */
#define BAM_FILE_CLOSED         0x80    /**< Properly closed flag */

/** DOS type */
#define BAM_DOS_TYPE_2A         0x41    /**< '2A' - Standard 1541 */
#define BAM_DOS_TYPE_2C         0x43    /**< '2C' - 1571 */
#define BAM_DOS_TYPE_3D         0x44    /**< '3D' - 1581 */

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Disk format type
 */
typedef enum {
    BAM_FORMAT_D64_35,      /**< D64 35 tracks */
    BAM_FORMAT_D64_40,      /**< D64 40 tracks */
    BAM_FORMAT_D71,         /**< D71 70 tracks */
    BAM_FORMAT_D81,         /**< D81 80 tracks */
    BAM_FORMAT_UNKNOWN      /**< Unknown format */
} bam_format_t;

/**
 * @brief D64 BAM structure (Track 18, Sector 0)
 */
typedef struct {
    uint8_t     dir_track;          /**< 0x00: Directory track (18) */
    uint8_t     dir_sector;         /**< 0x01: Directory sector (1) */
    uint8_t     dos_version;        /**< 0x02: DOS version ('A') */
    uint8_t     unused_03;          /**< 0x03: Unused */
    uint8_t     bam_entries[140];   /**< 0x04-0x8F: BAM for tracks 1-35 */
    char        disk_name[16];      /**< 0x90-0x9F: Disk name (PETSCII) */
    uint8_t     fill_a0[2];         /**< 0xA0-0xA1: Filled with 0xA0 */
    char        disk_id[2];         /**< 0xA2-0xA3: Disk ID */
    uint8_t     fill_a4;            /**< 0xA4: Filled with 0xA0 */
    char        dos_type[2];        /**< 0xA5-0xA6: DOS type ('2A') */
    uint8_t     fill_a7[4];         /**< 0xA7-0xAA: Filled with 0xA0 */
    uint8_t     ext_bam[85];        /**< 0xAB-0xFF: Extended BAM (tracks 36-40) */
} bam_d64_t;

/**
 * @brief BAM entry for single track
 */
typedef struct {
    uint8_t     free_sectors;       /**< Number of free sectors */
    uint8_t     bitmap[3];          /**< Sector allocation bitmap */
} bam_track_entry_t;

/**
 * @brief Directory entry
 */
typedef struct {
    uint8_t     next_track;         /**< Next dir sector track */
    uint8_t     next_sector;        /**< Next dir sector */
    uint8_t     file_type;          /**< File type */
    uint8_t     start_track;        /**< First data track */
    uint8_t     start_sector;       /**< First data sector */
    char        filename[16];       /**< Filename (PETSCII, padded 0xA0) */
    uint8_t     side_track;         /**< REL: Side-sector track */
    uint8_t     side_sector;        /**< REL: Side-sector sector */
    uint8_t     record_len;         /**< REL: Record length */
    uint8_t     unused[4];          /**< Unused (GEOS uses this) */
    uint8_t     replace_track;      /**< @SAVE replacement track */
    uint8_t     replace_sector;     /**< @SAVE replacement sector */
    uint16_t    file_size;          /**< File size in sectors (LE) */
} bam_dir_entry_t;

/**
 * @brief BAM context
 */
typedef struct {
    uint8_t         *data;          /**< Disk image data */
    size_t          size;           /**< Image size */
    bam_format_t    format;         /**< Disk format */
    int             num_tracks;     /**< Number of tracks */
    int             total_sectors;  /**< Total sectors */
    int             free_sectors;   /**< Free sectors */
    bool            modified;       /**< BAM modified flag */
    char            disk_name[17];  /**< Disk name (null-terminated) */
    char            disk_id[3];     /**< Disk ID (null-terminated) */
} bam_context_t;

/**
 * @brief Block allocation result
 */
typedef struct {
    bool        success;            /**< Allocation successful */
    int         track;              /**< Allocated track */
    int         sector;             /**< Allocated sector */
    int         free_before;        /**< Free sectors before */
    int         free_after;         /**< Free sectors after */
} bam_alloc_result_t;

/**
 * @brief File info
 */
typedef struct {
    char        filename[17];       /**< Filename (null-terminated) */
    uint8_t     file_type;          /**< File type */
    int         start_track;        /**< Start track */
    int         start_sector;       /**< Start sector */
    int         size_sectors;       /**< Size in sectors */
    int         size_bytes;         /**< Estimated size in bytes */
    bool        locked;             /**< File locked */
    bool        closed;             /**< File properly closed */
} bam_file_info_t;

/**
 * @brief Directory listing
 */
typedef struct {
    int             num_files;      /**< Number of files */
    bam_file_info_t files[144];     /**< File entries (max 144 for D64) */
    int             blocks_free;    /**< Blocks free */
    char            disk_name[17];  /**< Disk name */
    char            disk_id[3];     /**< Disk ID */
} bam_directory_t;

/* ============================================================================
 * API Functions - Context Management
 * ============================================================================ */

/**
 * @brief Create BAM context from disk image
 * @param data Disk image data (will not be copied)
 * @param size Image size
 * @return New context, or NULL on error
 */
bam_context_t *bam_create_context(uint8_t *data, size_t size);

/**
 * @brief Free BAM context
 * @param ctx Context to free (does not free data)
 */
void bam_free_context(bam_context_t *ctx);

/**
 * @brief Detect disk format from image
 * @param data Disk image data
 * @param size Image size
 * @return Detected format
 */
bam_format_t bam_detect_format(const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - BAM Reading
 * ============================================================================ */

/**
 * @brief Read BAM from image
 * @param ctx BAM context
 * @return 0 on success
 */
int bam_read(bam_context_t *ctx);

/**
 * @brief Get BAM sector data
 * @param ctx BAM context
 * @return Pointer to BAM sector
 */
const uint8_t *bam_get_sector(const bam_context_t *ctx);

/**
 * @brief Get disk name
 * @param ctx BAM context
 * @param name Output buffer (17 bytes)
 */
void bam_get_disk_name(const bam_context_t *ctx, char *name);

/**
 * @brief Get disk ID
 * @param ctx BAM context
 * @param id Output buffer (3 bytes)
 */
void bam_get_disk_id(const bam_context_t *ctx, char *id);

/* ============================================================================
 * API Functions - Block Operations
 * ============================================================================ */

/**
 * @brief Check if block is free
 * @param ctx BAM context
 * @param track Track number (1-based)
 * @param sector Sector number (0-based)
 * @return true if free
 */
bool bam_is_block_free(const bam_context_t *ctx, int track, int sector);

/**
 * @brief Allocate a block
 * @param ctx BAM context
 * @param track Track number
 * @param sector Sector number
 * @return 0 on success
 */
int bam_allocate_block(bam_context_t *ctx, int track, int sector);

/**
 * @brief Free a block
 * @param ctx BAM context
 * @param track Track number
 * @param sector Sector number
 * @return 0 on success
 */
int bam_free_block(bam_context_t *ctx, int track, int sector);

/**
 * @brief Find and allocate first free block
 * @param ctx BAM context
 * @param result Output allocation result
 * @return 0 on success
 */
int bam_allocate_first_free(bam_context_t *ctx, bam_alloc_result_t *result);

/**
 * @brief Find and allocate free block near track
 * @param ctx BAM context
 * @param near_track Preferred track
 * @param result Output allocation result
 * @return 0 on success
 */
int bam_allocate_near(bam_context_t *ctx, int near_track, bam_alloc_result_t *result);

/**
 * @brief Get number of free sectors on track
 * @param ctx BAM context
 * @param track Track number
 * @return Free sectors, or -1 on error
 */
int bam_free_on_track(const bam_context_t *ctx, int track);

/**
 * @brief Get total free sectors
 * @param ctx BAM context
 * @return Total free sectors
 */
int bam_total_free(const bam_context_t *ctx);

/**
 * @brief Get sectors per track
 * @param track Track number (1-based)
 * @param format Disk format
 * @return Sectors on track
 */
int bam_sectors_per_track(int track, bam_format_t format);

/* ============================================================================
 * API Functions - Directory Operations
 * ============================================================================ */

/**
 * @brief Read directory listing
 * @param ctx BAM context
 * @param dir Output directory
 * @return Number of files, or -1 on error
 */
int bam_read_directory(const bam_context_t *ctx, bam_directory_t *dir);

/**
 * @brief Find file by name
 * @param ctx BAM context
 * @param filename Filename to find (PETSCII)
 * @param info Output file info
 * @return true if found
 */
bool bam_find_file(const bam_context_t *ctx, const char *filename, bam_file_info_t *info);

/**
 * @brief Get file type name
 * @param file_type File type byte
 * @return Type name string
 */
const char *bam_file_type_name(uint8_t file_type);

/**
 * @brief Delete file
 * @param ctx BAM context
 * @param filename Filename to delete
 * @return Number of blocks freed, or -1 on error
 */
int bam_delete_file(bam_context_t *ctx, const char *filename);

/* ============================================================================
 * API Functions - BAM Writing
 * ============================================================================ */

/**
 * @brief Write BAM to image
 * @param ctx BAM context
 * @return 0 on success
 */
int bam_write(bam_context_t *ctx);

/**
 * @brief Set disk name
 * @param ctx BAM context
 * @param name New disk name
 * @return 0 on success
 */
int bam_set_disk_name(bam_context_t *ctx, const char *name);

/**
 * @brief Set disk ID
 * @param ctx BAM context
 * @param id New disk ID (2 chars)
 * @return 0 on success
 */
int bam_set_disk_id(bam_context_t *ctx, const char *id);

/* ============================================================================
 * API Functions - Validation and Repair
 * ============================================================================ */

/**
 * @brief Validate BAM
 * @param ctx BAM context
 * @param errors Output: number of errors found
 * @return true if valid
 */
bool bam_validate(const bam_context_t *ctx, int *errors);

/**
 * @brief Repair BAM from directory
 * @param ctx BAM context
 * @param blocks_recovered Output: blocks recovered
 * @return 0 on success
 */
int bam_repair(bam_context_t *ctx, int *blocks_recovered);

/**
 * @brief Recalculate free block count
 * @param ctx BAM context
 * @return New free count
 */
int bam_recalculate_free(bam_context_t *ctx);

/* ============================================================================
 * API Functions - Sector Access
 * ============================================================================ */

/**
 * @brief Get sector offset in image
 * @param track Track number (1-based)
 * @param sector Sector number (0-based)
 * @param format Disk format
 * @return Byte offset, or -1 on error
 */
int bam_sector_offset(int track, int sector, bam_format_t format);

/**
 * @brief Read sector data
 * @param ctx BAM context
 * @param track Track number
 * @param sector Sector number
 * @param buffer Output buffer (256 bytes)
 * @return 0 on success
 */
int bam_read_sector(const bam_context_t *ctx, int track, int sector, uint8_t *buffer);

/**
 * @brief Write sector data
 * @param ctx BAM context
 * @param track Track number
 * @param sector Sector number
 * @param buffer Input buffer (256 bytes)
 * @return 0 on success
 */
int bam_write_sector(bam_context_t *ctx, int track, int sector, const uint8_t *buffer);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get format name
 * @param format Disk format
 * @return Format name string
 */
const char *bam_format_name(bam_format_t format);

/**
 * @brief Convert ASCII to PETSCII
 * @param ascii ASCII string
 * @param petscii Output PETSCII buffer
 * @param len Maximum length
 */
void bam_ascii_to_petscii(const char *ascii, uint8_t *petscii, size_t len);

/**
 * @brief Convert PETSCII to ASCII
 * @param petscii PETSCII data
 * @param ascii Output ASCII buffer
 * @param len Maximum length
 */
void bam_petscii_to_ascii(const uint8_t *petscii, char *ascii, size_t len);

/**
 * @brief Print BAM summary
 * @param ctx BAM context
 * @param fp Output file
 */
void bam_print_summary(const bam_context_t *ctx, FILE *fp);

/**
 * @brief Print directory listing
 * @param dir Directory listing
 * @param fp Output file
 */
void bam_print_directory(const bam_directory_t *dir, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_C64_BAM_H */
