/**
 * @file uft_cmd.h
 * @brief CMD FD2000/FD4000 Disk Image Support
 * 
 * Support for CMD (Creative Micro Designs) drive images:
 * - D2M: FD2000 (3.5" DD, 1.6MB native partitions)
 * - D4M: FD4000 (3.5" HD, 3.2MB native partitions)
 * - D1M: FD2000 1581 emulation mode
 * - DHD: CMD HD (hard drive partitions)
 * 
 * CMD Native Partition Format:
 * - 256 bytes per sector
 * - 81 tracks (FD2000) or 161 tracks (FD4000)
 * - 10 sectors per track (DD) or 20 sectors per track (HD)
 * - Supports multiple partitions on single disk
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_CMD_H
#define UFT_CMD_H

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

/** Sector size */
#define CMD_SECTOR_SIZE         256

/** D2M (FD2000) constants */
#define D2M_TRACKS              81
#define D2M_SECTORS_PER_TRACK   10
#define D2M_SECTORS_TOTAL       (D2M_TRACKS * D2M_SECTORS_PER_TRACK)
#define D2M_SIZE                (D2M_SECTORS_TOTAL * CMD_SECTOR_SIZE)  /* 207360 */

/** D4M (FD4000) constants */
#define D4M_TRACKS              81
#define D4M_SECTORS_PER_TRACK   20
#define D4M_SECTORS_TOTAL       (D4M_TRACKS * D4M_SECTORS_PER_TRACK)
#define D4M_SIZE                (D4M_SECTORS_TOTAL * CMD_SECTOR_SIZE)  /* 414720 */

/** D1M (1581 emulation) constants */
#define D1M_TRACKS              80
#define D1M_SECTORS_PER_TRACK   10
#define D1M_SECTORS_TOTAL       (D1M_TRACKS * D1M_SECTORS_PER_TRACK)
#define D1M_SIZE                (D1M_SECTORS_TOTAL * CMD_SECTOR_SIZE)  /* 204800 */

/** DHD (Hard Drive) constants */
#define DHD_MAX_PARTITIONS      254
#define DHD_PARTITION_HEADER    256

/** Directory track */
#define CMD_DIR_TRACK           1
#define CMD_DIR_SECTOR          1

/** BAM location */
#define CMD_BAM_TRACK           1
#define CMD_BAM_SECTOR          0

/** Partition types */
typedef enum {
    CMD_PART_NATIVE     = 1,    /**< CMD Native partition */
    CMD_PART_1541       = 2,    /**< 1541 emulation */
    CMD_PART_1571       = 3,    /**< 1571 emulation */
    CMD_PART_1581       = 4,    /**< 1581 emulation */
    CMD_PART_SYSTEM     = 255   /**< System partition */
} cmd_partition_type_t;

/** Image types */
typedef enum {
    CMD_TYPE_D1M = 0,           /**< 1581 emulation disk */
    CMD_TYPE_D2M = 1,           /**< FD2000 native */
    CMD_TYPE_D4M = 2,           /**< FD4000 native */
    CMD_TYPE_DHD = 3,           /**< Hard drive image */
    CMD_TYPE_UNKNOWN = 255
} cmd_image_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief CMD BAM entry
 */
typedef struct {
    uint8_t     free_sectors;       /**< Free sectors in track */
    uint8_t     bitmap[3];          /**< Allocation bitmap */
} cmd_bam_entry_t;

/**
 * @brief CMD directory entry (32 bytes)
 */
typedef struct {
    uint8_t     next_track;         /**< Next dir block track */
    uint8_t     next_sector;        /**< Next dir block sector */
    uint8_t     file_type;          /**< File type */
    uint8_t     start_track;        /**< First data track */
    uint8_t     start_sector;       /**< First data sector */
    char        filename[16];       /**< Filename (PETSCII) */
    uint8_t     side_track;         /**< Side sector track (REL) */
    uint8_t     side_sector;        /**< Side sector (REL) */
    uint8_t     record_size;        /**< Record size (REL) */
    uint8_t     reserved[4];        /**< Reserved */
    uint8_t     replace_track;      /**< @SAVE replacement track */
    uint8_t     replace_sector;     /**< @SAVE replacement sector */
    uint16_t    blocks;             /**< File size in blocks */
} cmd_dir_entry_t;

/**
 * @brief CMD partition info
 */
typedef struct {
    uint8_t     number;             /**< Partition number */
    cmd_partition_type_t type;      /**< Partition type */
    uint16_t    start_track;        /**< Starting track */
    uint16_t    end_track;          /**< Ending track */
    char        name[17];           /**< Partition name */
    size_t      size;               /**< Size in bytes */
    uint16_t    free_blocks;        /**< Free blocks */
} cmd_partition_info_t;

/**
 * @brief CMD disk info
 */
typedef struct {
    cmd_image_type_t type;          /**< Image type */
    char        disk_name[17];      /**< Disk name */
    char        disk_id[3];         /**< Disk ID */
    uint16_t    total_tracks;       /**< Total tracks */
    uint16_t    sectors_per_track;  /**< Sectors per track */
    size_t      total_size;         /**< Total image size */
    uint16_t    free_blocks;        /**< Free blocks */
    uint16_t    used_blocks;        /**< Used blocks */
    int         num_partitions;     /**< Number of partitions */
} cmd_disk_info_t;

/**
 * @brief CMD image editor context
 */
typedef struct {
    uint8_t     *data;              /**< Image data */
    size_t      size;               /**< Image size */
    cmd_image_type_t type;          /**< Image type */
    uint16_t    tracks;             /**< Number of tracks */
    uint16_t    sectors_per_track;  /**< Sectors per track */
    bool        modified;           /**< Modified flag */
    bool        owns_data;          /**< true if we allocated */
} cmd_editor_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect CMD image type from size
 * @param size Image size
 * @return Image type
 */
cmd_image_type_t cmd_detect_type(size_t size);

/**
 * @brief Get type name
 * @param type Image type
 * @return Type name string
 */
const char *cmd_type_name(cmd_image_type_t type);

/**
 * @brief Validate CMD image
 * @param data Image data
 * @param size Image size
 * @return true if valid
 */
bool cmd_validate(const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - Editor
 * ============================================================================ */

/**
 * @brief Create editor from data
 * @param data Image data
 * @param size Image size
 * @param editor Output editor
 * @return 0 on success
 */
int cmd_editor_create(const uint8_t *data, size_t size, cmd_editor_t *editor);

/**
 * @brief Create new CMD image
 * @param type Image type
 * @param editor Output editor
 * @return 0 on success
 */
int cmd_create(cmd_image_type_t type, cmd_editor_t *editor);

/**
 * @brief Format CMD disk
 * @param editor Editor context
 * @param name Disk name
 * @param id Disk ID (2 chars)
 * @return 0 on success
 */
int cmd_format(cmd_editor_t *editor, const char *name, const char *id);

/**
 * @brief Close editor
 * @param editor Editor to close
 */
void cmd_editor_close(cmd_editor_t *editor);

/**
 * @brief Get disk info
 * @param editor Editor context
 * @param info Output info
 * @return 0 on success
 */
int cmd_get_info(const cmd_editor_t *editor, cmd_disk_info_t *info);

/* ============================================================================
 * API Functions - Sector Operations
 * ============================================================================ */

/**
 * @brief Calculate sector offset
 * @param editor Editor context
 * @param track Track number (1-based)
 * @param sector Sector number (0-based)
 * @return Byte offset, or -1 on error
 */
int cmd_sector_offset(const cmd_editor_t *editor, int track, int sector);

/**
 * @brief Read sector
 * @param editor Editor context
 * @param track Track number
 * @param sector Sector number
 * @param buffer Output buffer (256 bytes)
 * @return 0 on success
 */
int cmd_read_sector(const cmd_editor_t *editor, int track, int sector,
                    uint8_t *buffer);

/**
 * @brief Write sector
 * @param editor Editor context
 * @param track Track number
 * @param sector Sector number
 * @param buffer Input buffer (256 bytes)
 * @return 0 on success
 */
int cmd_write_sector(cmd_editor_t *editor, int track, int sector,
                     const uint8_t *buffer);

/* ============================================================================
 * API Functions - BAM Operations
 * ============================================================================ */

/**
 * @brief Check if block is free
 * @param editor Editor context
 * @param track Track number
 * @param sector Sector number
 * @return true if free
 */
bool cmd_is_block_free(const cmd_editor_t *editor, int track, int sector);

/**
 * @brief Allocate block
 * @param editor Editor context
 * @param track Track number
 * @param sector Sector number
 * @return 0 on success
 */
int cmd_allocate_block(cmd_editor_t *editor, int track, int sector);

/**
 * @brief Free block
 * @param editor Editor context
 * @param track Track number
 * @param sector Sector number
 * @return 0 on success
 */
int cmd_free_block(cmd_editor_t *editor, int track, int sector);

/**
 * @brief Get free block count
 * @param editor Editor context
 * @return Number of free blocks
 */
int cmd_get_free_blocks(const cmd_editor_t *editor);

/* ============================================================================
 * API Functions - Directory Operations
 * ============================================================================ */

/**
 * @brief Get directory entry count
 * @param editor Editor context
 * @return Number of entries
 */
int cmd_get_dir_entry_count(const cmd_editor_t *editor);

/**
 * @brief Get directory entry
 * @param editor Editor context
 * @param index Entry index
 * @param entry Output entry
 * @return 0 on success
 */
int cmd_get_dir_entry(const cmd_editor_t *editor, int index,
                      cmd_dir_entry_t *entry);

/**
 * @brief Print directory
 * @param editor Editor context
 * @param fp Output file
 */
void cmd_print_directory(const cmd_editor_t *editor, FILE *fp);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print disk info
 * @param editor Editor context
 * @param fp Output file
 */
void cmd_print_info(const cmd_editor_t *editor, FILE *fp);

/**
 * @brief Get image size for type
 * @param type Image type
 * @return Size in bytes
 */
size_t cmd_type_size(cmd_image_type_t type);

/**
 * @brief Get tracks for type
 * @param type Image type
 * @return Number of tracks
 */
int cmd_type_tracks(cmd_image_type_t type);

/**
 * @brief Get sectors per track for type
 * @param type Image type
 * @return Sectors per track
 */
int cmd_type_sectors(cmd_image_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CMD_H */
