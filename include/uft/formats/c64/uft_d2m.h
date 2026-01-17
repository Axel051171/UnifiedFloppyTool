/**
 * @file uft_d2m.h
 * @brief CMD FD2000/FD4000 Disk Image Support (D2M/D4M)
 * 
 * Support for CMD FD-Series 3.5" HD disk images:
 * - D2M: FD2000 (1.6MB, 81 tracks, 10 sectors/track)
 * - D4M: FD4000 (3.2MB, 81 tracks, 20 sectors/track)
 * - DNP: CMD Native Partition format
 * 
 * These drives use a modified 1581 filesystem with:
 * - Larger capacity (1.6MB or 3.2MB vs 800KB)
 * - Native partition support
 * - Subdirectory support
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_D2M_H
#define UFT_D2M_H

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
#define D2M_SECTOR_SIZE         256

/** D2M (FD2000) constants */
#define D2M_TRACKS              81
#define D2M_SECTORS_PER_TRACK   10
#define D2M_HEADS               2
#define D2M_TOTAL_SECTORS       (D2M_TRACKS * D2M_SECTORS_PER_TRACK * D2M_HEADS)
#define D2M_SIZE                (D2M_TOTAL_SECTORS * D2M_SECTOR_SIZE)  /* 414720 */

/** D4M (FD4000) constants */
#define D4M_TRACKS              81
#define D4M_SECTORS_PER_TRACK   20
#define D4M_HEADS               2
#define D4M_TOTAL_SECTORS       (D4M_TRACKS * D4M_SECTORS_PER_TRACK * D4M_HEADS)
#define D4M_SIZE                (D4M_TOTAL_SECTORS * D4M_SECTOR_SIZE)  /* 829440 */

/** Header/BAM track */
#define D2M_HEADER_TRACK        1
#define D2M_HEADER_SECTOR       0
#define D2M_BAM_TRACK           1
#define D2M_BAM_SECTOR          1
#define D2M_DIR_TRACK           1
#define D2M_DIR_SECTOR          3

/** Partition types */
#define D2M_PART_NATIVE         0x01    /**< Native partition */
#define D2M_PART_1541           0x02    /**< 1541 emulation */
#define D2M_PART_1571           0x03    /**< 1571 emulation */
#define D2M_PART_1581           0x04    /**< 1581 emulation */

/** Directory entry size */
#define D2M_DIR_ENTRY_SIZE      32

/** File types (same as D64) */
#define D2M_TYPE_DEL            0x00
#define D2M_TYPE_SEQ            0x01
#define D2M_TYPE_PRG            0x02
#define D2M_TYPE_USR            0x03
#define D2M_TYPE_REL            0x04
#define D2M_TYPE_CBM            0x05    /**< Partition/subdirectory */

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Disk type
 */
typedef enum {
    D2M_TYPE_D2M = 0,           /**< FD2000 (1.6MB) */
    D2M_TYPE_D4M = 1,           /**< FD4000 (3.2MB) */
    D2M_TYPE_UNKNOWN = 255
} d2m_disk_type_t;

/**
 * @brief D2M/D4M header block
 */
typedef struct {
    uint8_t     dir_track;          /**< Directory track */
    uint8_t     dir_sector;         /**< Directory sector */
    char        format_id[2];       /**< Format ID "2A" or "4A" */
    uint8_t     _unused1;
    uint8_t     bam_track;          /**< BAM track */
    uint8_t     bam_sector;         /**< BAM sector */
    uint8_t     dos_version;        /**< DOS version */
    char        disk_name[16];      /**< Disk name (PETSCII) */
    uint8_t     _fill1[2];          /**< $A0 fill */
    char        disk_id[2];         /**< Disk ID */
    uint8_t     _fill2;             /**< $A0 */
    char        dos_type[2];        /**< DOS type "2A"/"4A" */
    uint8_t     _fill3[4];          /**< $A0 fill */
} d2m_header_t;

/**
 * @brief Partition info
 */
typedef struct {
    uint8_t     type;               /**< Partition type */
    uint16_t    start_track;        /**< Start track */
    uint16_t    start_sector;       /**< Start sector */
    uint16_t    end_track;          /**< End track */
    uint16_t    end_sector;         /**< End sector */
    char        name[16];           /**< Partition name */
    uint32_t    blocks;             /**< Total blocks */
    uint32_t    free_blocks;        /**< Free blocks */
} d2m_partition_t;

/**
 * @brief Directory entry
 */
typedef struct {
    uint8_t     next_track;         /**< Next dir track (0 = end) */
    uint8_t     next_sector;        /**< Next dir sector */
    uint8_t     file_type;          /**< File type + flags */
    uint8_t     start_track;        /**< File start track */
    uint8_t     start_sector;       /**< File start sector */
    char        filename[16];       /**< Filename (PETSCII, $A0 padded) */
    uint8_t     side_track;         /**< Side-sector track (REL) */
    uint8_t     side_sector;        /**< Side-sector sector (REL) */
    uint8_t     rel_record_len;     /**< REL record length */
    uint8_t     _unused[4];         /**< Unused */
    uint8_t     replace_track;      /**< Replace track (@SAVE) */
    uint8_t     replace_sector;     /**< Replace sector */
    uint16_t    blocks;             /**< File size in blocks (LE) */
} d2m_dir_entry_t;

/**
 * @brief Disk info
 */
typedef struct {
    d2m_disk_type_t type;           /**< Disk type */
    char        name[17];           /**< Disk name */
    char        id[3];              /**< Disk ID */
    char        dos_type[3];        /**< DOS type */
    uint32_t    total_blocks;       /**< Total blocks */
    uint32_t    free_blocks;        /**< Free blocks */
    int         num_partitions;     /**< Number of partitions */
    int         num_files;          /**< Number of files in root */
} d2m_info_t;

/**
 * @brief D2M/D4M image context
 */
typedef struct {
    uint8_t     *data;              /**< Image data */
    size_t      size;               /**< Image size */
    d2m_disk_type_t type;           /**< Disk type */
    bool        owns_data;          /**< true if we allocated */
    bool        modified;           /**< Modified flag */
} d2m_image_t;

/* ============================================================================
 * API Functions - Detection & Validation
 * ============================================================================ */

/**
 * @brief Detect if data is D2M/D4M format
 * @param data Image data
 * @param size Data size
 * @return true if D2M/D4M
 */
bool d2m_detect(const uint8_t *data, size_t size);

/**
 * @brief Detect disk type from size
 * @param size Image size
 * @return Disk type
 */
d2m_disk_type_t d2m_detect_type(size_t size);

/**
 * @brief Validate D2M/D4M format
 * @param data Image data
 * @param size Data size
 * @return true if valid
 */
bool d2m_validate(const uint8_t *data, size_t size);

/**
 * @brief Get type name
 * @param type Disk type
 * @return Type name string
 */
const char *d2m_type_name(d2m_disk_type_t type);

/* ============================================================================
 * API Functions - Image Management
 * ============================================================================ */

/**
 * @brief Open D2M/D4M image from data
 * @param data Image data
 * @param size Data size
 * @param image Output image context
 * @return 0 on success
 */
int d2m_open(const uint8_t *data, size_t size, d2m_image_t *image);

/**
 * @brief Load D2M/D4M from file
 * @param filename File path
 * @param image Output image context
 * @return 0 on success
 */
int d2m_load(const char *filename, d2m_image_t *image);

/**
 * @brief Save D2M/D4M to file
 * @param image D2M/D4M image
 * @param filename Output path
 * @return 0 on success
 */
int d2m_save(const d2m_image_t *image, const char *filename);

/**
 * @brief Close D2M/D4M image
 * @param image Image to close
 */
void d2m_close(d2m_image_t *image);

/**
 * @brief Create new D2M image
 * @param name Disk name
 * @param id Disk ID (2 chars)
 * @param image Output image
 * @return 0 on success
 */
int d2m_create(const char *name, const char *id, d2m_image_t *image);

/**
 * @brief Create new D4M image
 * @param name Disk name
 * @param id Disk ID (2 chars)
 * @param image Output image
 * @return 0 on success
 */
int d4m_create(const char *name, const char *id, d2m_image_t *image);

/* ============================================================================
 * API Functions - Disk Info
 * ============================================================================ */

/**
 * @brief Get disk info
 * @param image D2M/D4M image
 * @param info Output info
 * @return 0 on success
 */
int d2m_get_info(const d2m_image_t *image, d2m_info_t *info);

/**
 * @brief Get disk name
 * @param image D2M/D4M image
 * @param name Output buffer (17 bytes min)
 */
void d2m_get_name(const d2m_image_t *image, char *name);

/**
 * @brief Get free blocks count
 * @param image D2M/D4M image
 * @return Free blocks
 */
uint32_t d2m_get_free_blocks(const d2m_image_t *image);

/* ============================================================================
 * API Functions - Sector Access
 * ============================================================================ */

/**
 * @brief Calculate sector offset
 * @param image D2M/D4M image
 * @param track Track number
 * @param sector Sector number
 * @return Byte offset, or -1 on error
 */
int d2m_sector_offset(const d2m_image_t *image, int track, int sector);

/**
 * @brief Read sector
 * @param image D2M/D4M image
 * @param track Track number
 * @param sector Sector number
 * @param buffer Output buffer (256 bytes)
 * @return 0 on success
 */
int d2m_read_sector(const d2m_image_t *image, int track, int sector,
                    uint8_t *buffer);

/**
 * @brief Write sector
 * @param image D2M/D4M image
 * @param track Track number
 * @param sector Sector number
 * @param buffer Input buffer (256 bytes)
 * @return 0 on success
 */
int d2m_write_sector(d2m_image_t *image, int track, int sector,
                     const uint8_t *buffer);

/* ============================================================================
 * API Functions - Directory
 * ============================================================================ */

/**
 * @brief Get number of directory entries
 * @param image D2M/D4M image
 * @return Number of entries
 */
int d2m_get_dir_count(const d2m_image_t *image);

/**
 * @brief Get directory entry
 * @param image D2M/D4M image
 * @param index Entry index
 * @param entry Output entry
 * @return 0 on success
 */
int d2m_get_dir_entry(const d2m_image_t *image, int index,
                      d2m_dir_entry_t *entry);

/**
 * @brief Find file by name
 * @param image D2M/D4M image
 * @param filename Filename to find
 * @param entry Output entry
 * @return 0 if found
 */
int d2m_find_file(const d2m_image_t *image, const char *filename,
                  d2m_dir_entry_t *entry);

/**
 * @brief Print directory listing
 * @param image D2M/D4M image
 * @param fp Output file
 */
void d2m_print_directory(const d2m_image_t *image, FILE *fp);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get file type name
 * @param type File type
 * @return Type name
 */
const char *d2m_file_type_name(uint8_t type);

/**
 * @brief Print disk info
 * @param image D2M/D4M image
 * @param fp Output file
 */
void d2m_print_info(const d2m_image_t *image, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D2M_H */
