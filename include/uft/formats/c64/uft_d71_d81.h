/**
 * @file uft_d71_d81.h
 * @brief 1571/1581 Disk Image Support (D71/D81 formats)
 * 
 * Extended disk format support for C64/C128:
 * - D71: 1571 double-sided (70 tracks, 349696 bytes)
 * - D81: 1581 3.5" HD (80 tracks, 819200 bytes)
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_D71_D81_H
#define UFT_D71_D81_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - D71 (1571 Double-Sided)
 * ============================================================================ */

/** D71 sizes */
#define D71_SIZE_STANDARD       349696      /**< 70 tracks, no errors */
#define D71_SIZE_ERRORS         351062      /**< 70 tracks, with errors */

/** D71 track layout */
#define D71_TRACKS              70          /**< 35 tracks x 2 sides */
#define D71_TRACKS_PER_SIDE     35
#define D71_TOTAL_SECTORS       1366        /**< Total sectors (683 x 2) */

/** D71 BAM location */
#define D71_BAM_TRACK           18          /**< BAM on track 18 */
#define D71_BAM_SECTOR          0           /**< BAM in sector 0 */
#define D71_BAM2_TRACK          53          /**< Second BAM (track 18 + 35) */
#define D71_BAM2_SECTOR         0

/** D71 directory */
#define D71_DIR_TRACK           18
#define D71_DIR_SECTOR          1

/* ============================================================================
 * Constants - D81 (1581 3.5" HD)
 * ============================================================================ */

/** D81 sizes */
#define D81_SIZE_STANDARD       819200      /**< 80 tracks, no errors */
#define D81_SIZE_ERRORS         822400      /**< 80 tracks, with errors */

/** D81 track layout */
#define D81_TRACKS              80          /**< 80 tracks */
#define D81_SECTORS_PER_TRACK   40          /**< 40 sectors per track */
#define D81_TOTAL_SECTORS       3200        /**< 80 x 40 sectors */
#define D81_SECTOR_SIZE         256         /**< Bytes per sector */

/** D81 BAM/Header location */
#define D81_HEADER_TRACK        40          /**< Header on track 40 */
#define D81_HEADER_SECTOR       0           /**< Header in sector 0 */
#define D81_BAM_TRACK           40          /**< BAM on track 40 */
#define D81_BAM_SECTOR_1        1           /**< BAM sectors 1-2 */
#define D81_BAM_SECTOR_2        2

/** D81 directory */
#define D81_DIR_TRACK           40
#define D81_DIR_SECTOR          3           /**< First directory sector */

/** D81 partition support */
#define D81_MAX_PARTITIONS      31          /**< Maximum partitions */

/* ============================================================================
 * Data Structures - D71
 * ============================================================================ */

/**
 * @brief D71 BAM entry (4 bytes per track)
 */
typedef struct {
    uint8_t     free_sectors;       /**< Number of free sectors */
    uint8_t     bitmap[3];          /**< Sector allocation bitmap */
} d71_bam_entry_t;

/**
 * @brief D71 disk info
 */
typedef struct {
    char        disk_name[17];      /**< Disk name (null terminated) */
    char        disk_id[3];         /**< Disk ID */
    char        dos_type[3];        /**< DOS type ("2C") */
    int         free_blocks;        /**< Free blocks */
    int         used_blocks;        /**< Used blocks */
    int         total_blocks;       /**< Total blocks */
    int         num_files;          /**< Number of files */
    bool        double_sided;       /**< Double-sided flag */
} d71_info_t;

/**
 * @brief D71 editor context
 */
typedef struct {
    uint8_t     *data;              /**< D71 data */
    size_t      size;               /**< Data size */
    bool        has_errors;         /**< Error info present */
    bool        modified;           /**< Modified flag */
} d71_editor_t;

/* ============================================================================
 * Data Structures - D81
 * ============================================================================ */

/**
 * @brief D81 header structure (sector 40/0)
 */
typedef struct {
    uint8_t     dir_track;          /**< Directory track (40) */
    uint8_t     dir_sector;         /**< Directory first sector (3) */
    uint8_t     disk_format;        /**< Disk format ID ('D') */
    uint8_t     reserved1;          /**< Reserved */
    char        disk_name[16];      /**< Disk name (PETSCII) */
    uint8_t     padding1[2];        /**< Padding (0xA0) */
    char        disk_id[2];         /**< Disk ID */
    uint8_t     padding2;           /**< Padding (0xA0) */
    char        dos_version[2];     /**< DOS version ("3D") */
    uint8_t     padding3[2];        /**< Padding (0xA0) */
} d81_header_t;

/**
 * @brief D81 BAM entry (6 bytes per track)
 */
typedef struct {
    uint8_t     free_sectors;       /**< Free sectors on track */
    uint8_t     bitmap[5];          /**< 40-bit bitmap (40 sectors) */
} d81_bam_entry_t;

/**
 * @brief D81 partition entry
 */
typedef struct {
    uint8_t     partition_type;     /**< Partition type */
    uint8_t     start_track;        /**< Starting track */
    uint8_t     start_sector;       /**< Starting sector */
    uint8_t     end_track;          /**< Ending track */
    uint8_t     end_sector;         /**< Ending sector */
    char        name[16];           /**< Partition name */
} d81_partition_t;

/**
 * @brief D81 disk info
 */
typedef struct {
    char        disk_name[17];      /**< Disk name */
    char        disk_id[3];         /**< Disk ID */
    char        dos_version[3];     /**< DOS version */
    int         free_blocks;        /**< Free blocks */
    int         used_blocks;        /**< Used blocks */
    int         total_blocks;       /**< Total (3160 usable) */
    int         num_files;          /**< Number of files */
    int         num_partitions;     /**< Number of partitions */
} d81_info_t;

/**
 * @brief D81 editor context
 */
typedef struct {
    uint8_t     *data;              /**< D81 data */
    size_t      size;               /**< Data size */
    d81_header_t *header;           /**< Header pointer */
    bool        has_errors;         /**< Error info present */
    bool        modified;           /**< Modified flag */
} d81_editor_t;

/* ============================================================================
 * API Functions - D71 (1571)
 * ============================================================================ */

/**
 * @brief Create D71 editor from data
 * @param data D71 data (will be modified in place)
 * @param size Data size
 * @return Editor, or NULL on error
 */
d71_editor_t *d71_editor_create(uint8_t *data, size_t size);

/**
 * @brief Free D71 editor
 * @param editor Editor to free
 */
void d71_editor_free(d71_editor_t *editor);

/**
 * @brief Create empty D71 image
 * @param disk_name Disk name
 * @param disk_id Disk ID
 * @param data Output: allocated data
 * @param size Output: data size
 * @return 0 on success
 */
int d71_create(const char *disk_name, const char *disk_id,
               uint8_t **data, size_t *size);

/**
 * @brief Format D71 disk
 * @param editor Editor
 * @param disk_name Disk name
 * @param disk_id Disk ID
 * @return 0 on success
 */
int d71_format(d71_editor_t *editor, const char *disk_name, const char *disk_id);

/**
 * @brief Get D71 disk info
 * @param editor Editor
 * @param info Output info
 * @return 0 on success
 */
int d71_get_info(const d71_editor_t *editor, d71_info_t *info);

/**
 * @brief Check if block is free
 * @param editor Editor
 * @param track Track (1-70)
 * @param sector Sector
 * @return true if free
 */
bool d71_is_block_free(const d71_editor_t *editor, int track, int sector);

/**
 * @brief Allocate block
 * @param editor Editor
 * @param track Track
 * @param sector Sector
 * @return 0 on success
 */
int d71_allocate_block(d71_editor_t *editor, int track, int sector);

/**
 * @brief Free block
 * @param editor Editor
 * @param track Track
 * @param sector Sector
 * @return 0 on success
 */
int d71_free_block(d71_editor_t *editor, int track, int sector);

/**
 * @brief Get free blocks count
 * @param editor Editor
 * @return Free blocks
 */
int d71_get_free_blocks(const d71_editor_t *editor);

/**
 * @brief Read sector
 * @param editor Editor
 * @param track Track (1-70)
 * @param sector Sector
 * @param buffer Output buffer (256 bytes)
 * @return 0 on success
 */
int d71_read_sector(const d71_editor_t *editor, int track, int sector,
                    uint8_t *buffer);

/**
 * @brief Write sector
 * @param editor Editor
 * @param track Track
 * @param sector Sector
 * @param buffer Input buffer
 * @return 0 on success
 */
int d71_write_sector(d71_editor_t *editor, int track, int sector,
                     const uint8_t *buffer);

/**
 * @brief Get sector offset in D71
 * @param track Track (1-70)
 * @param sector Sector
 * @return Byte offset, or -1 on error
 */
int d71_sector_offset(int track, int sector);

/**
 * @brief Get sectors per track
 * @param track Track number (1-70)
 * @return Sectors on track
 */
int d71_sectors_per_track(int track);

/**
 * @brief Validate D71 format
 * @param data D71 data
 * @param size Data size
 * @return true if valid D71
 */
bool d71_validate(const uint8_t *data, size_t size);

/**
 * @brief Print D71 directory
 * @param editor Editor
 * @param fp Output file
 */
void d71_print_directory(const d71_editor_t *editor, FILE *fp);

/* ============================================================================
 * API Functions - D81 (1581)
 * ============================================================================ */

/**
 * @brief Create D81 editor from data
 * @param data D81 data
 * @param size Data size
 * @return Editor, or NULL on error
 */
d81_editor_t *d81_editor_create(uint8_t *data, size_t size);

/**
 * @brief Free D81 editor
 * @param editor Editor
 */
void d81_editor_free(d81_editor_t *editor);

/**
 * @brief Create empty D81 image
 * @param disk_name Disk name
 * @param disk_id Disk ID
 * @param data Output: allocated data
 * @param size Output: data size
 * @return 0 on success
 */
int d81_create(const char *disk_name, const char *disk_id,
               uint8_t **data, size_t *size);

/**
 * @brief Format D81 disk
 * @param editor Editor
 * @param disk_name Disk name
 * @param disk_id Disk ID
 * @return 0 on success
 */
int d81_format(d81_editor_t *editor, const char *disk_name, const char *disk_id);

/**
 * @brief Get D81 disk info
 * @param editor Editor
 * @param info Output info
 * @return 0 on success
 */
int d81_get_info(const d81_editor_t *editor, d81_info_t *info);

/**
 * @brief Check if block is free
 * @param editor Editor
 * @param track Track (1-80)
 * @param sector Sector (0-39)
 * @return true if free
 */
bool d81_is_block_free(const d81_editor_t *editor, int track, int sector);

/**
 * @brief Allocate block
 * @param editor Editor
 * @param track Track
 * @param sector Sector
 * @return 0 on success
 */
int d81_allocate_block(d81_editor_t *editor, int track, int sector);

/**
 * @brief Free block
 * @param editor Editor
 * @param track Track
 * @param sector Sector
 * @return 0 on success
 */
int d81_free_block(d81_editor_t *editor, int track, int sector);

/**
 * @brief Get free blocks count
 * @param editor Editor
 * @return Free blocks
 */
int d81_get_free_blocks(const d81_editor_t *editor);

/**
 * @brief Read sector
 * @param editor Editor
 * @param track Track (1-80)
 * @param sector Sector (0-39)
 * @param buffer Output buffer (256 bytes)
 * @return 0 on success
 */
int d81_read_sector(const d81_editor_t *editor, int track, int sector,
                    uint8_t *buffer);

/**
 * @brief Write sector
 * @param editor Editor
 * @param track Track
 * @param sector Sector
 * @param buffer Input buffer
 * @return 0 on success
 */
int d81_write_sector(d81_editor_t *editor, int track, int sector,
                     const uint8_t *buffer);

/**
 * @brief Get sector offset in D81
 * @param track Track (1-80)
 * @param sector Sector (0-39)
 * @return Byte offset, or -1 on error
 */
int d81_sector_offset(int track, int sector);

/**
 * @brief Validate D81 format
 * @param data D81 data
 * @param size Data size
 * @return true if valid D81
 */
bool d81_validate(const uint8_t *data, size_t size);

/**
 * @brief Print D81 directory
 * @param editor Editor
 * @param fp Output file
 */
void d81_print_directory(const d81_editor_t *editor, FILE *fp);

/* ============================================================================
 * API Functions - D81 Partitions
 * ============================================================================ */

/**
 * @brief Get partition count
 * @param editor Editor
 * @return Number of partitions
 */
int d81_get_partition_count(const d81_editor_t *editor);

/**
 * @brief Get partition info
 * @param editor Editor
 * @param index Partition index
 * @param partition Output partition info
 * @return 0 on success
 */
int d81_get_partition(const d81_editor_t *editor, int index,
                      d81_partition_t *partition);

/**
 * @brief Create partition
 * @param editor Editor
 * @param name Partition name
 * @param start_track Starting track
 * @param end_track Ending track
 * @return 0 on success
 */
int d81_create_partition(d81_editor_t *editor, const char *name,
                         int start_track, int end_track);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Detect disk type from data
 * @param data Disk data
 * @param size Data size
 * @return 'd' for D64, '7' for D71, '8' for D81, 0 for unknown
 */
char detect_cbm_disk_type(const uint8_t *data, size_t size);

/**
 * @brief Convert D64 to D71 (single-sided to double-sided)
 * @param d64_data D64 data
 * @param d64_size D64 size
 * @param d71_data Output: D71 data
 * @param d71_size Output: D71 size
 * @return 0 on success
 */
int d64_to_d71(const uint8_t *d64_data, size_t d64_size,
               uint8_t **d71_data, size_t *d71_size);

/**
 * @brief Convert D71 to D64 (extract side 0)
 * @param d71_data D71 data
 * @param d71_size D71 size
 * @param d64_data Output: D64 data
 * @param d64_size Output: D64 size
 * @return 0 on success
 */
int d71_to_d64(const uint8_t *d71_data, size_t d71_size,
               uint8_t **d64_data, size_t *d64_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D71_D81_H */
