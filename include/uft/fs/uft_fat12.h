/**
 * @file uft_fat12.h
 * @brief FAT12/FAT16 Filesystem Layer for Floppy Disk Preservation
 * @version 3.7.0
 * 
 * Complete FAT12/FAT16 filesystem implementation for floppy disk images:
 * - All standard PC floppy formats (160KB - 2.88MB)
 * - MSX-DOS, Atari ST, PC-98, Human68K variants
 * - Directory operations (list, find, create, delete)
 * - File operations (extract, inject, rename)
 * - FAT table management and repair
 * - Long Filename (LFN) support
 * - Validation and forensic analysis
 * 
 * @note Part of UnifiedFloppyTool preservation suite
 */

#ifndef UFT_FAT12_H
#define UFT_FAT12_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include "uft/uft_compiler.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants and Limits
 *===========================================================================*/

/** Maximum path length */
#define UFT_FAT_MAX_PATH            260

/** Maximum LFN length (255 UTF-16 chars) */
#define UFT_FAT_MAX_LFN             255

/** Maximum 8.3 filename length */
#define UFT_FAT_MAX_SFN             12

/** Boot signature */
#define UFT_FAT_BOOT_SIG            0xAA55

/** Extended boot signature */
#define UFT_FAT_EXT_BOOT_SIG        0x29

/** Sector size (always 512 for floppies) */
#define UFT_FAT_SECTOR_SIZE         512

/*===========================================================================
 * FAT Types
 *===========================================================================*/

/** FAT filesystem type */
typedef enum {
    UFT_FAT_TYPE_UNKNOWN = 0,
    UFT_FAT_TYPE_FAT12   = 12,
    UFT_FAT_TYPE_FAT16   = 16,
    UFT_FAT_TYPE_FAT32   = 32      /**< Detected but not supported for floppies */
} uft_fat_type_t;

/** Media descriptor byte */
typedef enum {
    UFT_FAT_MEDIA_FIXED     = 0xF8,   /**< Fixed disk */
    UFT_FAT_MEDIA_1440K     = 0xF0,   /**< 3.5" HD 1.44MB */
    UFT_FAT_MEDIA_2880K     = 0xF0,   /**< 3.5" ED 2.88MB */
    UFT_FAT_MEDIA_720K      = 0xF9,   /**< 3.5" DD 720KB */
    UFT_FAT_MEDIA_1200K     = 0xF9,   /**< 5.25" HD 1.2MB */
    UFT_FAT_MEDIA_360K      = 0xFD,   /**< 5.25" DD 360KB */
    UFT_FAT_MEDIA_320K      = 0xFF,   /**< 5.25" DD 320KB */
    UFT_FAT_MEDIA_180K      = 0xFC,   /**< 5.25" SS 180KB */
    UFT_FAT_MEDIA_160K      = 0xFE    /**< 5.25" SS 160KB */
} uft_fat_media_t;

/** Platform variant */
typedef enum {
    UFT_FAT_PLATFORM_PC     = 0,      /**< IBM PC compatible */
    UFT_FAT_PLATFORM_MSX    = 1,      /**< MSX-DOS */
    UFT_FAT_PLATFORM_ATARI  = 2,      /**< Atari ST */
    UFT_FAT_PLATFORM_PC98   = 3,      /**< NEC PC-98 */
    UFT_FAT_PLATFORM_H68K   = 4,      /**< Sharp X68000 Human68K */
    UFT_FAT_PLATFORM_FM     = 5,      /**< Fujitsu FM Towns */
    UFT_FAT_PLATFORM_ACORN  = 6       /**< Acorn ADFS hybrid */
} uft_fat_platform_t;

/*===========================================================================
 * FAT Entry Values
 *===========================================================================*/

/** FAT12 special values */
#define UFT_FAT12_FREE              0x000
#define UFT_FAT12_RESERVED_MIN      0xFF0
#define UFT_FAT12_RESERVED_MAX      0xFF6
#define UFT_FAT12_BAD               0xFF7
#define UFT_FAT12_EOF_MIN           0xFF8
#define UFT_FAT12_EOF_MAX           0xFFF
#define UFT_FAT12_EOF               0xFFF

/** FAT16 special values */
#define UFT_FAT16_FREE              0x0000
#define UFT_FAT16_RESERVED_MIN      0xFFF0
#define UFT_FAT16_RESERVED_MAX      0xFFF6
#define UFT_FAT16_BAD               0xFFF7
#define UFT_FAT16_EOF_MIN           0xFFF8
#define UFT_FAT16_EOF_MAX           0xFFFF
#define UFT_FAT16_EOF               0xFFFF

/** First valid data cluster */
#define UFT_FAT_FIRST_CLUSTER       2

/*===========================================================================
 * File Attributes
 *===========================================================================*/

/** Directory entry attributes */
#define UFT_FAT_ATTR_READONLY       0x01
#define UFT_FAT_ATTR_HIDDEN         0x02
#define UFT_FAT_ATTR_SYSTEM         0x04
#define UFT_FAT_ATTR_VOLUME_ID      0x08
#define UFT_FAT_ATTR_DIRECTORY      0x10
#define UFT_FAT_ATTR_ARCHIVE        0x20
#define UFT_FAT_ATTR_LFN            0x0F    /**< Long filename entry */
#define UFT_FAT_ATTR_LFN_MASK       0x3F    /**< Mask for LFN detection */

/** Directory entry markers */
#define UFT_FAT_DIRENT_FREE         0xE5    /**< Deleted entry */
#define UFT_FAT_DIRENT_END          0x00    /**< End of directory */
#define UFT_FAT_DIRENT_KANJI        0x05    /**< First char is 0xE5 (Kanji) */

/** LFN entry markers */
#define UFT_FAT_LFN_LAST            0x40    /**< Last LFN entry flag */
#define UFT_FAT_LFN_SEQ_MASK        0x1F    /**< Sequence number mask */

/*===========================================================================
 * Structures - Boot Sector
 *===========================================================================*/

/**
 * @brief FAT12/16 Boot Sector (BPB - BIOS Parameter Block)
 * 
 * Standard PC boot sector layout
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  jmp_boot[3];           /**< 0x00: Jump instruction */
    char     oem_name[8];           /**< 0x03: OEM name */
    uint16_t bytes_per_sector;      /**< 0x0B: Bytes per sector (512) */
    uint8_t  sectors_per_cluster;   /**< 0x0D: Sectors per cluster */
    uint16_t reserved_sectors;      /**< 0x0E: Reserved sectors (1 for FAT12) */
    uint8_t  num_fats;              /**< 0x10: Number of FATs (usually 2) */
    uint16_t root_entry_count;      /**< 0x11: Root directory entries */
    uint16_t total_sectors_16;      /**< 0x13: Total sectors (16-bit) */
    uint8_t  media_type;            /**< 0x15: Media descriptor */
    uint16_t fat_size_16;           /**< 0x16: Sectors per FAT */
    uint16_t sectors_per_track;     /**< 0x18: Sectors per track */
    uint16_t num_heads;             /**< 0x1A: Number of heads */
    uint32_t hidden_sectors;        /**< 0x1C: Hidden sectors */
    uint32_t total_sectors_32;      /**< 0x20: Total sectors (32-bit) */
    
    /* Extended boot record (FAT12/16) */
    uint8_t  drive_number;          /**< 0x24: Drive number */
    uint8_t  reserved1;             /**< 0x25: Reserved */
    uint8_t  boot_signature;        /**< 0x26: Extended boot signature (0x29) */
    uint32_t volume_serial;         /**< 0x27: Volume serial number */
    char     volume_label[11];      /**< 0x2B: Volume label */
    char     fs_type[8];            /**< 0x36: Filesystem type string */
    uint8_t  boot_code[448];        /**< 0x3E: Boot code */
    uint16_t signature;             /**< 0x1FE: Boot signature (0xAA55) */
} uft_fat_bootsect_t;
UFT_PACK_END

/*===========================================================================
 * Structures - Directory Entry
 *===========================================================================*/

/**
 * @brief FAT Short Directory Entry (8.3 format, 32 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    char     name[8];               /**< 0x00: Filename (space-padded) */
    char     ext[3];                /**< 0x08: Extension (space-padded) */
    uint8_t  attributes;            /**< 0x0B: File attributes */
    uint8_t  nt_reserved;           /**< 0x0C: Reserved for NT */
    uint8_t  create_time_tenth;     /**< 0x0D: Creation time (10ms units) */
    uint16_t create_time;           /**< 0x0E: Creation time */
    uint16_t create_date;           /**< 0x10: Creation date */
    uint16_t access_date;           /**< 0x12: Last access date */
    uint16_t cluster_high;          /**< 0x14: High word of cluster (FAT32) */
    uint16_t modify_time;           /**< 0x16: Last modification time */
    uint16_t modify_date;           /**< 0x18: Last modification date */
    uint16_t cluster_low;           /**< 0x1A: Low word of first cluster */
    uint32_t file_size;             /**< 0x1C: File size in bytes */
} uft_fat_sfn_t;
UFT_PACK_END

/**
 * @brief FAT Long Filename Entry (32 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  sequence;              /**< 0x00: Sequence number (1-20) | 0x40 for last */
    uint16_t name1[5];              /**< 0x01: Characters 1-5 (UCS-2) */
    uint8_t  attributes;            /**< 0x0B: Always 0x0F */
    uint8_t  type;                  /**< 0x0C: Always 0x00 */
    uint8_t  checksum;              /**< 0x0D: Checksum of SFN */
    uint16_t name2[6];              /**< 0x0E: Characters 6-11 (UCS-2) */
    uint16_t cluster;               /**< 0x1A: Always 0x0000 */
    uint16_t name3[2];              /**< 0x1C: Characters 12-13 (UCS-2) */
} uft_fat_lfn_t;
UFT_PACK_END

/*===========================================================================
 * Structures - Geometry
 *===========================================================================*/

/**
 * @brief Standard floppy disk geometry
 */
typedef struct {
    const char *name;               /**< Format name */
    uint32_t total_sectors;         /**< Total sectors */
    uint16_t sectors_per_track;     /**< Sectors per track */
    uint16_t heads;                 /**< Number of heads */
    uint16_t tracks;                /**< Number of tracks */
    uint8_t  sectors_per_cluster;   /**< Sectors per cluster */
    uint16_t root_entries;          /**< Root directory entries */
    uint16_t fat_sectors;           /**< Sectors per FAT */
    uint8_t  media_type;            /**< Media descriptor */
    uft_fat_platform_t platform;    /**< Platform */
} uft_fat_geometry_t;

/*===========================================================================
 * Structures - Runtime Context
 *===========================================================================*/

/**
 * @brief Volume information (calculated from BPB)
 */
typedef struct {
    /* From BPB */
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint32_t total_sectors;
    uint16_t fat_size;
    uint8_t  media_type;
    
    /* Calculated */
    uint32_t fat_start_sector;      /**< First FAT sector */
    uint32_t root_dir_sector;       /**< First root dir sector */
    uint32_t root_dir_sectors;      /**< Root directory size in sectors */
    uint32_t data_start_sector;     /**< First data sector */
    uint32_t data_clusters;         /**< Total data clusters */
    uint32_t last_cluster;          /**< Last valid cluster number */
    
    /* Type detection */
    uft_fat_type_t fat_type;        /**< FAT12 or FAT16 */
    uft_fat_platform_t platform;    /**< Platform variant */
    
    /* Volume info */
    uint32_t serial;
    char     label[12];             /**< Null-terminated */
    char     oem_name[9];           /**< Null-terminated */
} uft_fat_volume_t;

/**
 * @brief FAT filesystem context
 */
typedef struct {
    /* Image data */
    uint8_t *data;                  /**< Image data */
    size_t   data_size;             /**< Image size */
    bool     owns_data;             /**< True if we allocated data */
    bool     modified;              /**< True if modified */
    
    /* Volume info */
    uft_fat_volume_t vol;
    
    /* FAT cache (first FAT only) */
    uint8_t *fat_cache;             /**< Cached FAT table */
    bool     fat_dirty;             /**< FAT needs writeback */
    
    /* Options */
    bool     strict_mode;           /**< Strict validation */
    bool     read_only;             /**< No modifications allowed */
} uft_fat_ctx_t;

/*===========================================================================
 * Structures - Detection Result
 *===========================================================================*/

/**
 * @brief FAT detection result
 */
typedef struct {
    bool valid;                     /**< Valid FAT filesystem */
    uft_fat_type_t type;            /**< FAT type */
    uft_fat_platform_t platform;    /**< Platform variant */
    const uft_fat_geometry_t *geometry;  /**< Matching geometry */
    int confidence;                 /**< Detection confidence (0-100) */
    char description[64];           /**< Human-readable description */
    
    /* Warnings */
    bool boot_sig_missing;          /**< 0xAA55 missing */
    bool bpb_inconsistent;          /**< BPB values inconsistent */
    bool fat_mismatch;              /**< FAT copies don't match */
} uft_fat_detect_t;

/*===========================================================================
 * Structures - Directory Entry (parsed)
 *===========================================================================*/

/**
 * @brief Parsed directory entry
 */
typedef struct {
    char     sfn[13];               /**< Short filename (8.3 + null) */
    char     lfn[UFT_FAT_MAX_LFN + 1];  /**< Long filename (UTF-8) */
    uint8_t  attributes;            /**< File attributes */
    uint32_t cluster;               /**< First cluster */
    uint32_t size;                  /**< File size */
    
    /* Timestamps */
    time_t   create_time;
    time_t   modify_time;
    time_t   access_time;
    
    /* Directory position */
    uint32_t dir_cluster;           /**< Parent directory cluster (0=root) */
    uint32_t dir_entry_index;       /**< Entry index in directory */
    uint32_t lfn_start_index;       /**< First LFN entry index */
    uint8_t  lfn_count;             /**< Number of LFN entries */
    
    /* Status */
    bool     is_deleted;            /**< Entry is deleted */
    bool     is_directory;          /**< Entry is directory */
    bool     is_volume_label;       /**< Entry is volume label */
    bool     has_lfn;               /**< Has long filename */
} uft_fat_entry_t;

/**
 * @brief Directory listing
 */
typedef struct {
    uft_fat_entry_t *entries;       /**< Array of entries */
    size_t           count;         /**< Number of entries */
    size_t           capacity;      /**< Allocated capacity */
    uint32_t         cluster;       /**< Directory cluster (0=root) */
    char             path[UFT_FAT_MAX_PATH];  /**< Directory path */
} uft_fat_dir_t;

/*===========================================================================
 * Structures - Cluster Chain
 *===========================================================================*/

/**
 * @brief Cluster chain
 */
typedef struct {
    uint32_t *clusters;             /**< Array of cluster numbers */
    size_t    count;                /**< Number of clusters */
    size_t    capacity;             /**< Allocated capacity */
    bool      complete;             /**< Chain ends with EOF */
    bool      has_bad;              /**< Chain contains bad clusters */
    bool      has_loops;            /**< Chain contains loops */
} uft_fat_chain_t;

/*===========================================================================
 * Structures - Validation
 *===========================================================================*/

/** Validation issue severity */
typedef enum {
    UFT_FAT_SEV_INFO    = 0,
    UFT_FAT_SEV_WARNING = 1,
    UFT_FAT_SEV_ERROR   = 2,
    UFT_FAT_SEV_FATAL   = 3
} uft_fat_severity_t;

/**
 * @brief Single validation issue
 */
typedef struct {
    uft_fat_severity_t severity;
    uint32_t cluster;               /**< Related cluster (or 0) */
    char message[128];
} uft_fat_issue_t;

/**
 * @brief Validation result
 */
typedef struct {
    bool valid;                     /**< Overall validity */
    bool repairable;                /**< Can be repaired */
    
    /* Statistics */
    uint32_t total_clusters;
    uint32_t used_clusters;
    uint32_t free_clusters;
    uint32_t bad_clusters;
    uint32_t lost_clusters;         /**< Allocated but unreferenced */
    uint32_t cross_linked;          /**< Used by multiple chains */
    
    /* Directory stats */
    uint32_t total_files;
    uint32_t total_dirs;
    uint32_t deleted_entries;
    
    /* Issues */
    uft_fat_issue_t *issues;
    size_t issue_count;
    size_t issue_capacity;
} uft_fat_validation_t;

/*===========================================================================
 * Geometry Table
 *===========================================================================*/

/**
 * @brief Standard floppy geometries
 */
static const uft_fat_geometry_t uft_fat_std_geometries[] = {
    /* 3.5" formats - PC */
    { "3.5\" HD 1.44MB",   2880, 18, 2, 80, 1, 224, 9, 0xF0, UFT_FAT_PLATFORM_PC },
    { "3.5\" DD 720KB",    1440,  9, 2, 80, 2, 112, 3, 0xF9, UFT_FAT_PLATFORM_PC },
    { "3.5\" ED 2.88MB",   5760, 36, 2, 80, 2, 240, 9, 0xF0, UFT_FAT_PLATFORM_PC },
    
    /* 5.25" formats - PC */
    { "5.25\" HD 1.2MB",   2400, 15, 2, 80, 1, 224, 7, 0xF9, UFT_FAT_PLATFORM_PC },
    { "5.25\" DD 360KB",    720,  9, 2, 40, 2, 112, 2, 0xFD, UFT_FAT_PLATFORM_PC },
    { "5.25\" DD 320KB",    640,  8, 2, 40, 2, 112, 1, 0xFF, UFT_FAT_PLATFORM_PC },
    { "5.25\" SS 180KB",    360,  9, 1, 40, 1,  64, 2, 0xFC, UFT_FAT_PLATFORM_PC },
    { "5.25\" SS 160KB",    320,  8, 1, 40, 1,  64, 1, 0xFE, UFT_FAT_PLATFORM_PC },
    
    /* MSX-DOS formats */
    { "MSX 720KB DD",      1440,  9, 2, 80, 2, 112, 3, 0xF9, UFT_FAT_PLATFORM_MSX },
    { "MSX 360KB SS",       720,  9, 1, 80, 2, 112, 3, 0xF8, UFT_FAT_PLATFORM_MSX },
    
    /* Atari ST formats */
    { "Atari ST SS",        720,  9, 1, 80, 2, 112, 3, 0xF8, UFT_FAT_PLATFORM_ATARI },
    { "Atari ST DS",       1440,  9, 2, 80, 2, 112, 3, 0xF9, UFT_FAT_PLATFORM_ATARI },
    { "Atari ST HD",       2880, 18, 2, 80, 1, 224, 9, 0xF0, UFT_FAT_PLATFORM_ATARI },
    
    /* PC-98 formats */
    { "PC-98 640KB",       1280,  8, 2, 80, 1, 192, 2, 0xFE, UFT_FAT_PLATFORM_PC98 },
    { "PC-98 1.25MB",      2560, 16, 2, 80, 1, 224, 7, 0xFE, UFT_FAT_PLATFORM_PC98 },
    { "PC-98 1.44MB",      2880, 18, 2, 80, 1, 224, 9, 0xF0, UFT_FAT_PLATFORM_PC98 },
    
    /* X68000 Human68K */
    { "X68K 1.23MB",       2464, 16, 2, 77, 2, 256, 5, 0xFE, UFT_FAT_PLATFORM_H68K },
    { "X68K 640KB",        1232,  8, 2, 77, 1, 192, 2, 0xFE, UFT_FAT_PLATFORM_H68K },
    
    /* Sentinel */
    { NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/*===========================================================================
 * Error Codes
 *===========================================================================*/

#define UFT_FAT_OK                  0
#define UFT_FAT_ERR_INVALID        -1
#define UFT_FAT_ERR_NOMEM          -2
#define UFT_FAT_ERR_IO             -3
#define UFT_FAT_ERR_NOTFOUND       -4
#define UFT_FAT_ERR_EXISTS         -5
#define UFT_FAT_ERR_FULL           -6
#define UFT_FAT_ERR_NOTEMPTY       -7
#define UFT_FAT_ERR_READONLY       -8
#define UFT_FAT_ERR_BADCHAIN       -9
#define UFT_FAT_ERR_TOOLONG        -10
#define UFT_FAT_ERR_BADNAME        -11

/*===========================================================================
 * API - Lifecycle
 *===========================================================================*/

/**
 * @brief Create FAT context
 * @return New context or NULL on error
 */
uft_fat_ctx_t *uft_fat_create(void);

/**
 * @brief Destroy FAT context
 */
void uft_fat_destroy(uft_fat_ctx_t *ctx);

/**
 * @brief Open FAT image from memory
 * @param ctx Context
 * @param data Image data
 * @param size Image size
 * @param copy If true, make internal copy
 * @return 0 on success
 */
int uft_fat_open(uft_fat_ctx_t *ctx, const uint8_t *data, size_t size, bool copy);

/**
 * @brief Open FAT image from file
 * @param ctx Context
 * @param filename Path to image file
 * @return 0 on success
 */
int uft_fat_open_file(uft_fat_ctx_t *ctx, const char *filename);

/**
 * @brief Save changes to file
 * @param ctx Context
 * @param filename Output path (NULL = overwrite original)
 * @return 0 on success
 */
int uft_fat_save(uft_fat_ctx_t *ctx, const char *filename);

/**
 * @brief Get raw image data
 */
const uint8_t *uft_fat_get_data(const uft_fat_ctx_t *ctx, size_t *size);

/*===========================================================================
 * API - Detection
 *===========================================================================*/

/**
 * @brief Detect FAT filesystem
 * @param data Image data
 * @param size Image size
 * @param result Detection result
 * @return 0 on success
 */
int uft_fat_detect(const uint8_t *data, size_t size, uft_fat_detect_t *result);

/**
 * @brief Get geometry from image size
 * @param size Image size in bytes
 * @return Matching geometry or NULL
 */
const uft_fat_geometry_t *uft_fat_geometry_from_size(size_t size);

/**
 * @brief Detect platform variant
 * @param boot Boot sector
 * @return Platform type
 */
uft_fat_platform_t uft_fat_detect_platform(const uft_fat_bootsect_t *boot);

/*===========================================================================
 * API - Volume Info
 *===========================================================================*/

/**
 * @brief Get volume information
 */
const uft_fat_volume_t *uft_fat_get_volume(const uft_fat_ctx_t *ctx);

/**
 * @brief Get volume label
 * @param ctx Context
 * @param label Output buffer (at least 12 bytes)
 * @return 0 on success
 */
int uft_fat_get_label(const uft_fat_ctx_t *ctx, char *label);

/**
 * @brief Set volume label
 * @param ctx Context
 * @param label New label (max 11 chars)
 * @return 0 on success
 */
int uft_fat_set_label(uft_fat_ctx_t *ctx, const char *label);

/**
 * @brief Get free space
 * @param ctx Context
 * @return Free space in bytes
 */
uint64_t uft_fat_get_free_space(const uft_fat_ctx_t *ctx);

/**
 * @brief Get used space
 */
uint64_t uft_fat_get_used_space(const uft_fat_ctx_t *ctx);

/*===========================================================================
 * API - FAT Table
 *===========================================================================*/

/**
 * @brief Get FAT entry value
 * @param ctx Context
 * @param cluster Cluster number
 * @return Next cluster, EOF marker, or error
 */
int32_t uft_fat_get_entry(const uft_fat_ctx_t *ctx, uint32_t cluster);

/**
 * @brief Set FAT entry value
 * @param ctx Context
 * @param cluster Cluster number
 * @param value New value
 * @return 0 on success
 */
int uft_fat_set_entry(uft_fat_ctx_t *ctx, uint32_t cluster, uint32_t value);

/**
 * @brief Check if cluster is free
 */
bool uft_fat_cluster_is_free(const uft_fat_ctx_t *ctx, uint32_t cluster);

/**
 * @brief Check if cluster is EOF
 */
bool uft_fat_cluster_is_eof(const uft_fat_ctx_t *ctx, uint32_t cluster);

/**
 * @brief Check if cluster is bad
 */
bool uft_fat_cluster_is_bad(const uft_fat_ctx_t *ctx, uint32_t cluster);

/**
 * @brief Allocate cluster
 * @param ctx Context
 * @param hint Preferred cluster (0 = any)
 * @return Allocated cluster or -1 on error
 */
int32_t uft_fat_alloc_cluster(uft_fat_ctx_t *ctx, uint32_t hint);

/**
 * @brief Allocate cluster chain
 * @param ctx Context
 * @param count Number of clusters
 * @param chain Output chain
 * @return 0 on success
 */
int uft_fat_alloc_chain(uft_fat_ctx_t *ctx, size_t count, uft_fat_chain_t *chain);

/**
 * @brief Free cluster chain
 * @param ctx Context
 * @param start First cluster
 * @return 0 on success
 */
int uft_fat_free_chain(uft_fat_ctx_t *ctx, uint32_t start);

/**
 * @brief Get cluster chain
 * @param ctx Context
 * @param start First cluster
 * @param chain Output chain (caller must init/free)
 * @return 0 on success
 */
int uft_fat_get_chain(const uft_fat_ctx_t *ctx, uint32_t start, uft_fat_chain_t *chain);

/**
 * @brief Initialize chain structure
 */
void uft_fat_chain_init(uft_fat_chain_t *chain);

/**
 * @brief Free chain structure
 */
void uft_fat_chain_free(uft_fat_chain_t *chain);

/*===========================================================================
 * API - Cluster I/O
 *===========================================================================*/

/**
 * @brief Read cluster data
 * @param ctx Context
 * @param cluster Cluster number
 * @param buffer Output buffer (cluster size bytes)
 * @return 0 on success
 */
int uft_fat_read_cluster(const uft_fat_ctx_t *ctx, uint32_t cluster, uint8_t *buffer);

/**
 * @brief Write cluster data
 * @param ctx Context
 * @param cluster Cluster number
 * @param buffer Input buffer (cluster size bytes)
 * @return 0 on success
 */
int uft_fat_write_cluster(uft_fat_ctx_t *ctx, uint32_t cluster, const uint8_t *buffer);

/**
 * @brief Get cluster byte offset
 * @param ctx Context
 * @param cluster Cluster number
 * @return Byte offset or -1 on error
 */
int64_t uft_fat_cluster_offset(const uft_fat_ctx_t *ctx, uint32_t cluster);

/**
 * @brief Get cluster size in bytes
 */
size_t uft_fat_cluster_size(const uft_fat_ctx_t *ctx);

/*===========================================================================
 * API - Root Directory
 *===========================================================================*/

/**
 * @brief Read root directory sector
 * @param ctx Context
 * @param index Sector index (0 to root_dir_sectors-1)
 * @param buffer Output buffer (sector size bytes)
 * @return 0 on success
 */
int uft_fat_read_root_sector(const uft_fat_ctx_t *ctx, uint32_t index, uint8_t *buffer);

/**
 * @brief Write root directory sector
 */
int uft_fat_write_root_sector(uft_fat_ctx_t *ctx, uint32_t index, const uint8_t *buffer);

/*===========================================================================
 * API - Directory Operations
 *===========================================================================*/

/**
 * @brief Initialize directory structure
 */
void uft_fat_dir_init(uft_fat_dir_t *dir);

/**
 * @brief Free directory structure
 */
void uft_fat_dir_free(uft_fat_dir_t *dir);

/**
 * @brief Read directory (root or subdirectory)
 * @param ctx Context
 * @param cluster Directory cluster (0 for root)
 * @param dir Output directory
 * @return 0 on success
 */
int uft_fat_read_dir(const uft_fat_ctx_t *ctx, uint32_t cluster, uft_fat_dir_t *dir);

/**
 * @brief Read directory by path
 * @param ctx Context
 * @param path Directory path (e.g., "/SUBDIR" or "\\SUBDIR")
 * @param dir Output directory
 * @return 0 on success
 */
int uft_fat_read_dir_path(const uft_fat_ctx_t *ctx, const char *path, uft_fat_dir_t *dir);

/**
 * @brief Find entry in directory
 * @param ctx Context
 * @param cluster Directory cluster (0 for root)
 * @param name Filename to find
 * @param entry Output entry
 * @return 0 on success, UFT_FAT_ERR_NOTFOUND if not found
 */
int uft_fat_find_entry(const uft_fat_ctx_t *ctx, uint32_t cluster,
                       const char *name, uft_fat_entry_t *entry);

/**
 * @brief Find entry by path
 * @param ctx Context
 * @param path Full path (e.g., "/SUBDIR/FILE.TXT")
 * @param entry Output entry
 * @return 0 on success
 */
int uft_fat_find_path(const uft_fat_ctx_t *ctx, const char *path, uft_fat_entry_t *entry);

/**
 * @brief Iterate over directory entries
 * @param ctx Context
 * @param cluster Directory cluster (0 for root)
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return 0 on success
 */
typedef int (*uft_fat_dir_callback_t)(const uft_fat_entry_t *entry, void *user_data);
int uft_fat_foreach_entry(const uft_fat_ctx_t *ctx, uint32_t cluster,
                          uft_fat_dir_callback_t callback, void *user_data);

/**
 * @brief Iterate recursively over all files
 */
int uft_fat_foreach_file(const uft_fat_ctx_t *ctx, uint32_t cluster,
                         uft_fat_dir_callback_t callback, void *user_data);

/*===========================================================================
 * API - File Operations
 *===========================================================================*/

/**
 * @brief Extract file to memory
 * @param ctx Context
 * @param entry File entry
 * @param buffer Output buffer (NULL to allocate)
 * @param size In: buffer size, Out: actual size
 * @return Pointer to data or NULL on error
 */
uint8_t *uft_fat_extract(const uft_fat_ctx_t *ctx, const uft_fat_entry_t *entry,
                         uint8_t *buffer, size_t *size);

/**
 * @brief Extract file by path
 * @param ctx Context
 * @param path Full path
 * @param buffer Output buffer (NULL to allocate)
 * @param size In: buffer size, Out: actual size
 * @return Pointer to data or NULL on error
 */
uint8_t *uft_fat_extract_path(const uft_fat_ctx_t *ctx, const char *path,
                              uint8_t *buffer, size_t *size);

/**
 * @brief Extract file to disk
 * @param ctx Context
 * @param path Source path in image
 * @param dest_path Destination file path
 * @return 0 on success
 */
int uft_fat_extract_to_file(const uft_fat_ctx_t *ctx, const char *path,
                            const char *dest_path);

/**
 * @brief Inject file from memory
 * @param ctx Context
 * @param dir_cluster Directory cluster (0 for root)
 * @param name Filename
 * @param data File data
 * @param size File size
 * @return 0 on success
 */
int uft_fat_inject(uft_fat_ctx_t *ctx, uint32_t dir_cluster,
                   const char *name, const uint8_t *data, size_t size);

/**
 * @brief Inject file by path
 * @param ctx Context
 * @param path Full path (directory must exist)
 * @param data File data
 * @param size File size
 * @return 0 on success
 */
int uft_fat_inject_path(uft_fat_ctx_t *ctx, const char *path,
                        const uint8_t *data, size_t size);

/**
 * @brief Inject file from disk
 * @param ctx Context
 * @param path Destination path in image
 * @param src_path Source file path
 * @return 0 on success
 */
int uft_fat_inject_from_file(uft_fat_ctx_t *ctx, const char *path,
                             const char *src_path);

/**
 * @brief Delete file
 * @param ctx Context
 * @param path Full path
 * @return 0 on success
 */
int uft_fat_delete(uft_fat_ctx_t *ctx, const char *path);

/**
 * @brief Rename file
 * @param ctx Context
 * @param old_path Current path
 * @param new_path New path
 * @return 0 on success
 */
int uft_fat_rename(uft_fat_ctx_t *ctx, const char *old_path, const char *new_path);

/**
 * @brief Create directory
 * @param ctx Context
 * @param path Directory path
 * @return 0 on success
 */
int uft_fat_mkdir(uft_fat_ctx_t *ctx, const char *path);

/**
 * @brief Remove empty directory
 * @param ctx Context
 * @param path Directory path
 * @return 0 on success
 */
int uft_fat_rmdir(uft_fat_ctx_t *ctx, const char *path);

/**
 * @brief Set file attributes
 * @param ctx Context
 * @param path File path
 * @param attr New attributes
 * @return 0 on success
 */
int uft_fat_set_attr(uft_fat_ctx_t *ctx, const char *path, uint8_t attr);

/**
 * @brief Set file timestamp
 * @param ctx Context
 * @param path File path
 * @param mtime New modification time
 * @return 0 on success
 */
int uft_fat_set_time(uft_fat_ctx_t *ctx, const char *path, time_t mtime);

/*===========================================================================
 * API - LFN Support
 *===========================================================================*/

/**
 * @brief Generate short filename from long name
 * @param lfn Long filename
 * @param sfn Output short name (8+3 format)
 * @param sfn_display Output display form (8.3 format, 13 bytes)
 * @return 0 on success
 */
int uft_fat_lfn_to_sfn(const char *lfn, char sfn[11], char sfn_display[13]);

/**
 * @brief Generate unique short filename
 * @param ctx Context
 * @param dir_cluster Directory cluster
 * @param lfn Long filename
 * @param sfn Output short name
 * @return 0 on success
 */
int uft_fat_generate_sfn(const uft_fat_ctx_t *ctx, uint32_t dir_cluster,
                         const char *lfn, char sfn[11]);

/**
 * @brief Calculate LFN checksum
 * @param sfn Short filename (11 bytes)
 * @return Checksum byte
 */
uint8_t uft_fat_lfn_checksum(const char sfn[11]);

/**
 * @brief Check if name requires LFN
 * @param name Filename
 * @return true if LFN needed
 */
bool uft_fat_needs_lfn(const char *name);

/*===========================================================================
 * API - Validation and Repair
 *===========================================================================*/

/**
 * @brief Initialize validation structure
 */
void uft_fat_validation_init(uft_fat_validation_t *val);

/**
 * @brief Free validation structure
 */
void uft_fat_validation_free(uft_fat_validation_t *val);

/**
 * @brief Validate filesystem
 * @param ctx Context
 * @param val Output validation result
 * @return 0 on success (check val->valid for result)
 */
int uft_fat_validate(const uft_fat_ctx_t *ctx, uft_fat_validation_t *val);

/**
 * @brief Repair filesystem issues
 * @param ctx Context
 * @param val Validation result (from uft_fat_validate)
 * @return Number of issues fixed
 */
int uft_fat_repair(uft_fat_ctx_t *ctx, const uft_fat_validation_t *val);

/**
 * @brief Rebuild FAT from directory tree
 * @param ctx Context
 * @return 0 on success
 */
int uft_fat_rebuild_fat(uft_fat_ctx_t *ctx);

/**
 * @brief Compare FAT copies
 * @param ctx Context
 * @return 0 if identical, >0 if differences found
 */
int uft_fat_compare_fats(const uft_fat_ctx_t *ctx);

/**
 * @brief Sync FAT copies (copy FAT1 to FAT2)
 * @param ctx Context
 * @return 0 on success
 */
int uft_fat_sync_fats(uft_fat_ctx_t *ctx);

/**
 * @brief Recover deleted files
 * @param ctx Context
 * @param dir Directory to search (NULL for all)
 * @param callback Called for each recoverable file
 * @param user_data User data for callback
 * @return Number of recoverable files found
 */
typedef int (*uft_fat_recover_callback_t)(const uft_fat_entry_t *entry,
                                          bool can_recover, void *user_data);
int uft_fat_find_deleted(const uft_fat_ctx_t *ctx, uft_fat_dir_t *dir,
                         uft_fat_recover_callback_t callback, void *user_data);

/**
 * @brief Attempt to recover deleted file
 * @param ctx Context
 * @param entry Deleted entry
 * @param output Output buffer
 * @param size In: buffer size, Out: recovered size
 * @return 0 on success
 */
int uft_fat_recover_file(const uft_fat_ctx_t *ctx, const uft_fat_entry_t *entry,
                         uint8_t *output, size_t *size);

/*===========================================================================
 * API - Formatting
 *===========================================================================*/

/**
 * @brief Format options
 */
typedef struct {
    const uft_fat_geometry_t *geometry;  /**< Disk geometry (required) */
    const char *label;                   /**< Volume label (optional) */
    uint32_t serial;                     /**< Serial number (0 = generate) */
    const char *oem_name;                /**< OEM name (optional) */
    bool quick_format;                   /**< Don't zero data area */
    bool bootable;                       /**< Include boot code */
} uft_fat_format_opts_t;

/**
 * @brief Format new FAT image
 * @param ctx Context
 * @param opts Format options
 * @return 0 on success
 */
int uft_fat_format(uft_fat_ctx_t *ctx, const uft_fat_format_opts_t *opts);

/**
 * @brief Create new FAT image file
 * @param filename Output filename
 * @param opts Format options
 * @return 0 on success
 */
int uft_fat_create_image(const char *filename, const uft_fat_format_opts_t *opts);

/*===========================================================================
 * API - Utilities
 *===========================================================================*/

/**
 * @brief Convert FAT time/date to Unix time
 */
time_t uft_fat_to_unix_time(uint16_t fat_time, uint16_t fat_date);

/**
 * @brief Convert Unix time to FAT time/date
 */
void uft_fat_from_unix_time(time_t unix_time, uint16_t *fat_time, uint16_t *fat_date);

/**
 * @brief Format entry as string
 * @param entry Directory entry
 * @param buffer Output buffer (at least 80 bytes)
 * @return buffer
 */
char *uft_fat_entry_to_string(const uft_fat_entry_t *entry, char *buffer);

/**
 * @brief Format attributes as string
 * @param attr Attributes
 * @param buffer Output buffer (at least 8 bytes: "RHSVDA\0")
 * @return buffer
 */
char *uft_fat_attr_to_string(uint8_t attr, char *buffer);

/**
 * @brief Get error message
 * @param error Error code
 * @return Error description
 */
const char *uft_fat_strerror(int error);

/**
 * @brief Generate JSON report
 * @param ctx Context
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes written
 */
size_t uft_fat_to_json(const uft_fat_ctx_t *ctx, char *buffer, size_t size);

/**
 * @brief Print directory listing
 * @param ctx Context
 * @param cluster Directory cluster (0 for root)
 * @param fp Output file (NULL for stdout)
 */
void uft_fat_print_dir(const uft_fat_ctx_t *ctx, uint32_t cluster, FILE *fp);

/**
 * @brief Print directory tree
 * @param ctx Context
 * @param fp Output file (NULL for stdout)
 */
void uft_fat_print_tree(const uft_fat_ctx_t *ctx, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT12_H */
