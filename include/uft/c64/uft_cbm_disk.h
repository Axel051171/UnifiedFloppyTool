/**
 * @file uft_cbm_disk.h
 * @brief Unified CBM Disk Format Handler (D64/D71/D81/G64/G71)
 * 
 * Supports:
 * - D64: 1541 single-sided (35/40 tracks)
 * - D71: 1571 double-sided (70/80 tracks)
 * - D81: 1581 3.5" (80 tracks, 40 sectors)
 * - G64: GCR stream format (1541)
 * - G71: GCR stream format (1571)
 * 
 * Features:
 * - Directory listing with file info
 * - PRG extraction with analysis
 * - BAM inspection
 * - Tool/fastloader detection
 * - Disk-level forensics
 * 
 * @author UFT Team
 * @version 1.0.0
 * @date 2025
 */

#ifndef UFT_CBM_DISK_H
#define UFT_CBM_DISK_H

#include <stddef.h>
#include <stdint.h>
#include "uft/c64/uft_c64_prg.h"
#include "uft/c64/uft_cbm_drive_scan.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Disk format types */
typedef enum uft_cbm_disk_format {
    UFT_CBM_DISK_UNKNOWN = 0,
    UFT_CBM_DISK_D64,           /**< 1541 - 35 tracks, 683 sectors */
    UFT_CBM_DISK_D64_40,        /**< 1541 extended - 40 tracks, 768 sectors */
    UFT_CBM_DISK_D71,           /**< 1571 - 70 tracks, 1366 sectors */
    UFT_CBM_DISK_D71_80,        /**< 1571 extended - 80 tracks */
    UFT_CBM_DISK_D81,           /**< 1581 - 80 tracks, 3200 sectors */
    UFT_CBM_DISK_G64,           /**< GCR stream (1541) */
    UFT_CBM_DISK_G71,           /**< GCR stream (1571) */
    UFT_CBM_DISK_D80,           /**< 8050 - 77 tracks */
    UFT_CBM_DISK_D82            /**< 8250 - 154 tracks (double-sided) */
} uft_cbm_disk_format_t;

/** File types in CBM DOS */
typedef enum uft_cbm_file_type {
    UFT_CBM_FILE_DEL = 0,       /**< Deleted */
    UFT_CBM_FILE_SEQ = 1,       /**< Sequential */
    UFT_CBM_FILE_PRG = 2,       /**< Program */
    UFT_CBM_FILE_USR = 3,       /**< User */
    UFT_CBM_FILE_REL = 4,       /**< Relative */
    UFT_CBM_FILE_CBM = 5        /**< CBM partition (D81) */
} uft_cbm_file_type_t;

/** Maximum values */
#define UFT_CBM_MAX_FILENAME    16
#define UFT_CBM_MAX_DISKNAME    16
#define UFT_CBM_MAX_DISKID      5
#define UFT_CBM_MAX_DIR_ENTRIES 296     /* D81 max */
#define UFT_CBM_SECTOR_SIZE     256

/** D64 geometry */
#define UFT_D64_TRACKS_STD      35
#define UFT_D64_TRACKS_EXT      40
#define UFT_D64_SECTORS_STD     683
#define UFT_D64_SECTORS_EXT     768
#define UFT_D64_SIZE_STD        174848
#define UFT_D64_SIZE_ERR        175531
#define UFT_D64_SIZE_EXT        196608
#define UFT_D64_SIZE_EXT_ERR    197376

/** D71 geometry (double-sided 1571) */
#define UFT_D71_TRACKS          70
#define UFT_D71_SECTORS         1366
#define UFT_D71_SIZE            349696
#define UFT_D71_SIZE_ERR        351062

/** D81 geometry (3.5" 1581) */
#define UFT_D81_TRACKS          80
#define UFT_D81_SECTORS_TRACK   40
#define UFT_D81_SECTORS         3200
#define UFT_D81_SIZE            819200

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Directory entry
 */
typedef struct uft_cbm_dir_entry {
    char filename[UFT_CBM_MAX_FILENAME + 1];
    uft_cbm_file_type_t type;
    uint8_t start_track;
    uint8_t start_sector;
    uint16_t blocks;
    uint8_t flags;              /**< Locked, closed, etc. */
    
    /* Extended info */
    uint32_t size_bytes;        /**< Calculated actual size */
    uint8_t side_track;         /**< For REL files */
    uint8_t side_sector;
    uint8_t record_length;      /**< For REL files */
    
    /* GEOS extensions (if present) */
    int is_geos;
    uint8_t geos_type;
    uint8_t geos_struct;
} uft_cbm_dir_entry_t;

/**
 * @brief BAM (Block Availability Map) info
 */
typedef struct uft_cbm_bam_info {
    uint16_t blocks_total;
    uint16_t blocks_free;
    uint16_t blocks_used;
    char disk_name[UFT_CBM_MAX_DISKNAME + 1];
    char disk_id[UFT_CBM_MAX_DISKID + 1];
    uint8_t dos_type;
    uint8_t dos_version;
    
    /* Per-track free blocks (max 80 tracks * 2 sides) */
    uint8_t track_free[160];
} uft_cbm_bam_info_t;

/**
 * @brief Loaded disk image
 */
typedef struct uft_cbm_disk {
    uft_cbm_disk_format_t format;
    const uint8_t *data;
    size_t data_size;
    int owns_data;              /**< Should we free data? */
    
    /* Geometry */
    uint8_t tracks;
    uint8_t sides;
    uint16_t total_sectors;
    
    /* Directory */
    uft_cbm_dir_entry_t *directory;
    size_t dir_count;
    
    /* BAM */
    uft_cbm_bam_info_t bam;
    
    /* Error map (if present) */
    const uint8_t *error_map;
    size_t error_map_size;
    
    /* Analysis results */
    uft_scan_result_t disk_scan;
    int has_copy_protection;
    int has_fastloader;
    const char *detected_tool;
} uft_cbm_disk_t;

/**
 * @brief Extracted file
 */
typedef struct uft_cbm_file {
    uft_cbm_dir_entry_t entry;
    uint8_t *data;
    size_t data_size;
    
    /* PRG analysis (if type == PRG) */
    uft_c64_prg_info_t prg_info;
    uft_scan_result_t scan_result;
} uft_cbm_file_t;

/**
 * @brief Disk analysis result
 */
typedef struct uft_cbm_disk_analysis {
    uft_cbm_disk_format_t format;
    int valid;
    
    /* Statistics */
    int total_files;
    int prg_count;
    int seq_count;
    int other_count;
    int deleted_count;
    
    /* Tool detection */
    int has_copy_tools;
    int has_fastloaders;
    int has_protection;
    int tool_score;
    
    /* Identified tools */
    char tool_names[8][32];
    int tool_count;
    
    /* Issues */
    int bam_errors;
    int chain_errors;
    int duplicate_files;
} uft_cbm_disk_analysis_t;

/* ============================================================================
 * Disk Loading
 * ============================================================================ */

/**
 * @brief Detect disk format from file size
 */
uft_cbm_disk_format_t uft_cbm_detect_format(size_t file_size);

/**
 * @brief Load disk image from memory
 * 
 * @param data      Disk image data
 * @param size      Data size
 * @param disk      Output disk structure
 * @return          0 on success, negative on error
 */
int uft_cbm_disk_load(const uint8_t *data, size_t size, uft_cbm_disk_t *disk);

/**
 * @brief Load disk image from file
 */
int uft_cbm_disk_load_file(const char *filename, uft_cbm_disk_t *disk);

/**
 * @brief Free disk resources
 */
void uft_cbm_disk_free(uft_cbm_disk_t *disk);

/* ============================================================================
 * Directory Operations
 * ============================================================================ */

/**
 * @brief Read directory from disk
 */
int uft_cbm_read_directory(uft_cbm_disk_t *disk);

/**
 * @brief Get directory entry by index
 */
const uft_cbm_dir_entry_t *uft_cbm_get_entry(const uft_cbm_disk_t *disk, size_t index);

/**
 * @brief Find file by name
 */
int uft_cbm_find_file(const uft_cbm_disk_t *disk, const char *name, 
                      uft_cbm_dir_entry_t *entry);

/**
 * @brief Format directory listing as text
 */
size_t uft_cbm_format_directory(const uft_cbm_disk_t *disk, char *out, size_t out_cap);

/* ============================================================================
 * File Extraction
 * ============================================================================ */

/**
 * @brief Extract file from disk
 * 
 * @param disk      Disk image
 * @param entry     Directory entry
 * @param file      Output file structure
 * @return          0 on success
 */
int uft_cbm_extract_file(const uft_cbm_disk_t *disk, 
                         const uft_cbm_dir_entry_t *entry,
                         uft_cbm_file_t *file);

/**
 * @brief Extract file by name
 */
int uft_cbm_extract_file_by_name(const uft_cbm_disk_t *disk, const char *name,
                                  uft_cbm_file_t *file);

/**
 * @brief Free extracted file
 */
void uft_cbm_file_free(uft_cbm_file_t *file);

/**
 * @brief Extract and analyze all PRG files
 * 
 * @param disk      Disk image
 * @param files     Output array of files
 * @param max_files Maximum files to extract
 * @return          Number of PRG files found
 */
int uft_cbm_extract_all_prg(const uft_cbm_disk_t *disk, 
                             uft_cbm_file_t *files, size_t max_files);

/* ============================================================================
 * BAM Operations
 * ============================================================================ */

/**
 * @brief Read BAM from disk
 */
int uft_cbm_read_bam(uft_cbm_disk_t *disk);

/**
 * @brief Check if sector is allocated
 */
int uft_cbm_is_sector_allocated(const uft_cbm_disk_t *disk, 
                                 uint8_t track, uint8_t sector);

/**
 * @brief Validate BAM against actual usage
 */
int uft_cbm_validate_bam(const uft_cbm_disk_t *disk);

/* ============================================================================
 * Sector Access
 * ============================================================================ */

/**
 * @brief Get sector data pointer
 */
const uint8_t *uft_cbm_get_sector(const uft_cbm_disk_t *disk, 
                                   uint8_t track, uint8_t sector);

/**
 * @brief Get sector offset in image
 */
int uft_cbm_sector_offset(uft_cbm_disk_format_t format, 
                          uint8_t track, uint8_t sector,
                          uint32_t *offset);

/**
 * @brief Get sectors per track for format
 */
uint8_t uft_cbm_sectors_per_track(uft_cbm_disk_format_t format, uint8_t track);

/* ============================================================================
 * Analysis
 * ============================================================================ */

/**
 * @brief Analyze disk for tools/protection
 */
int uft_cbm_analyze_disk(uft_cbm_disk_t *disk, uft_cbm_disk_analysis_t *analysis);

/**
 * @brief Scan disk for fastloaders/copy tools
 */
int uft_cbm_scan_for_tools(const uft_cbm_disk_t *disk, uft_scan_result_t *result);

/**
 * @brief Check for copy protection indicators
 */
int uft_cbm_check_protection(const uft_cbm_disk_t *disk);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get format name
 */
const char *uft_cbm_format_name(uft_cbm_disk_format_t format);

/**
 * @brief Get file type name
 */
const char *uft_cbm_file_type_name(uft_cbm_file_type_t type);

/**
 * @brief Convert PETSCII filename to ASCII
 */
void uft_cbm_petscii_to_ascii(const uint8_t *petscii, size_t len, 
                               char *ascii, size_t ascii_cap);

/**
 * @brief Calculate disk checksum
 */
uint32_t uft_cbm_disk_checksum(const uft_cbm_disk_t *disk);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CBM_DISK_H */
