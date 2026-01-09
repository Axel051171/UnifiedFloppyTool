/**
 * @file uft_axdf.h
 * @brief AXDF - Advanced Extended Amiga Disk Format
 * 
 * AXDF is a forensic container format for Amiga disks that preserves:
 * - Full flux-level data (optional)
 * - MFM-decoded track data
 * - Sector-level data with error maps
 * - Filesystem metadata
 * - Protection detection results
 * - Repair audit trail
 * 
 * File structure:
 * ┌──────────────────────────────────────────────┐
 * │ AXDF Header (512 bytes)                      │
 * ├──────────────────────────────────────────────┤
 * │ Track Table (variable, 4KB aligned)          │
 * ├──────────────────────────────────────────────┤
 * │ Track Data Blocks                            │
 * │   - Flux data (optional)                     │
 * │   - MFM data                                 │
 * │   - Decoded sectors                          │
 * ├──────────────────────────────────────────────┤
 * │ Metadata Block                               │
 * │   - Source info                              │
 * │   - Protection analysis                      │
 * │   - Repair history                           │
 * └──────────────────────────────────────────────┘
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_AXDF_H
#define UFT_AXDF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define AXDF_MAGIC              "AXDF"      /**< File magic */
#define AXDF_VERSION_MAJOR      1
#define AXDF_VERSION_MINOR      0
#define AXDF_HEADER_SIZE        512
#define AXDF_ALIGNMENT          4096        /**< Block alignment */

/** Track data flags */
#define AXDF_TRK_HAS_FLUX       0x0001      /**< Flux data present */
#define AXDF_TRK_HAS_MFM        0x0002      /**< MFM data present */
#define AXDF_TRK_HAS_SECTORS    0x0004      /**< Decoded sectors present */
#define AXDF_TRK_HAS_ERRORS     0x0008      /**< Error map present */
#define AXDF_TRK_PROTECTED      0x0010      /**< Protection detected */
#define AXDF_TRK_REPAIRED       0x0020      /**< Track was repaired */
#define AXDF_TRK_WEAK_BITS      0x0040      /**< Weak bits detected */
#define AXDF_TRK_MULTIPLE_REVS  0x0080      /**< Multiple revolutions stored */

/** Sector status flags */
#define AXDF_SEC_OK             0x00        /**< Sector OK */
#define AXDF_SEC_CRC_ERROR      0x01        /**< CRC error (original) */
#define AXDF_SEC_CRC_REPAIRED   0x02        /**< CRC repaired */
#define AXDF_SEC_HEADER_ERROR   0x04        /**< Header CRC error */
#define AXDF_SEC_MISSING        0x08        /**< Sector not found */
#define AXDF_SEC_WEAK           0x10        /**< Weak bits in sector */
#define AXDF_SEC_FUZZY          0x20        /**< Fuzzy bits (protection) */

/** Disk types */
typedef enum {
    AXDF_DISK_DD_OFS = 0,       /**< DD OFS (880KB) */
    AXDF_DISK_DD_FFS = 1,       /**< DD FFS (880KB) */
    AXDF_DISK_HD_OFS = 2,       /**< HD OFS (1.76MB) */
    AXDF_DISK_HD_FFS = 3,       /**< HD FFS (1.76MB) */
    AXDF_DISK_CUSTOM = 0xFF,    /**< Custom format */
} axdf_disk_type_t;

/*===========================================================================
 * File Header (512 bytes)
 *===========================================================================*/

typedef struct __attribute__((packed)) {
    /* Identification (16 bytes) */
    char     magic[4];              /**< "AXDF" */
    uint8_t  version_major;         /**< Major version */
    uint8_t  version_minor;         /**< Minor version */
    uint16_t header_size;           /**< Header size (512) */
    uint32_t file_size;             /**< Total file size */
    uint32_t checksum;              /**< CRC32 of entire file */
    
    /* Disk info (32 bytes) */
    uint8_t  disk_type;             /**< axdf_disk_type_t */
    uint8_t  num_tracks;            /**< Number of tracks (80/84) */
    uint8_t  num_sides;             /**< Number of sides (1/2) */
    uint8_t  sectors_per_track;     /**< Sectors per track (11/22) */
    uint16_t sector_size;           /**< Sector size (512) */
    uint16_t track_length;          /**< Standard track length */
    uint32_t data_offset;           /**< Offset to track data */
    uint32_t data_size;             /**< Total data size */
    uint8_t  reserved1[16];         /**< Reserved */
    
    /* Source info (64 bytes) */
    char     source_device[32];     /**< Capture device name */
    char     source_date[20];       /**< ISO 8601 date */
    uint8_t  source_revolutions;    /**< Revolutions captured */
    uint8_t  source_flags;          /**< Capture flags */
    uint8_t  reserved2[10];         /**< Reserved */
    
    /* Content info (64 bytes) */
    char     disk_name[32];         /**< Disk name (from filesystem) */
    char     disk_label[20];        /**< Volume label */
    uint32_t creation_date;         /**< AmigaDOS creation date */
    uint32_t modification_date;     /**< AmigaDOS modification date */
    uint8_t  reserved3[4];          /**< Reserved */
    
    /* Protection info (32 bytes) */
    uint32_t protection_type;       /**< Protection type flags */
    uint8_t  protection_track;      /**< Primary protection track */
    uint8_t  protection_sector;     /**< Primary protection sector */
    uint16_t protection_confidence; /**< Detection confidence (0-10000) */
    char     protection_name[24];   /**< Protection name */
    
    /* Recovery info (32 bytes) */
    uint32_t repair_flags;          /**< Repair operations performed */
    uint16_t sectors_repaired;      /**< Number of sectors repaired */
    uint16_t sectors_unreadable;    /**< Number of unreadable sectors */
    uint32_t repair_date;           /**< Repair timestamp */
    uint8_t  reserved4[20];         /**< Reserved */
    
    /* Offsets (32 bytes) */
    uint32_t track_table_offset;    /**< Offset to track table */
    uint32_t track_table_size;      /**< Track table size */
    uint32_t metadata_offset;       /**< Offset to metadata */
    uint32_t metadata_size;         /**< Metadata size */
    uint32_t flux_offset;           /**< Offset to flux data (0 = none) */
    uint32_t flux_size;             /**< Flux data size */
    uint8_t  reserved5[8];          /**< Reserved */
    
    /* Padding to 512 bytes */
    uint8_t  padding[256];
} axdf_header_t;

/*===========================================================================
 * Track Table Entry (32 bytes each)
 *===========================================================================*/

typedef struct __attribute__((packed)) {
    uint8_t  track;                 /**< Track number */
    uint8_t  side;                  /**< Side (0/1) */
    uint16_t flags;                 /**< Track flags */
    
    uint32_t data_offset;           /**< Offset in data block */
    uint32_t data_size;             /**< Track data size */
    uint32_t mfm_offset;            /**< MFM data offset (relative) */
    uint32_t mfm_size;              /**< MFM data size */
    uint32_t sector_offset;         /**< Sector data offset (relative) */
    
    uint16_t sector_count;          /**< Number of sectors */
    uint16_t error_count;           /**< Number of errors */
    
    uint32_t checksum;              /**< Track data CRC32 */
} axdf_track_entry_t;

/*===========================================================================
 * Sector Header (16 bytes each, before sector data)
 *===========================================================================*/

typedef struct __attribute__((packed)) {
    uint8_t  sector;                /**< Sector number */
    uint8_t  status;                /**< Sector status flags */
    uint16_t size;                  /**< Sector data size */
    uint32_t original_crc;          /**< Original CRC from disk */
    uint32_t computed_crc;          /**< Computed CRC */
    uint8_t  confidence;            /**< Decode confidence (0-100) */
    uint8_t  revisions;             /**< Number of revisions used */
    uint8_t  weak_bits;             /**< Number of weak bits */
    uint8_t  reserved;
} axdf_sector_header_t;

/*===========================================================================
 * Repair Log Entry (64 bytes each)
 *===========================================================================*/

typedef struct __attribute__((packed)) {
    uint32_t timestamp;             /**< Repair timestamp */
    uint8_t  track;                 /**< Track number */
    uint8_t  side;                  /**< Side */
    uint8_t  sector;                /**< Sector (0xFF = whole track) */
    uint8_t  repair_type;           /**< Repair type */
    uint32_t bits_corrected;        /**< Number of bits corrected */
    uint32_t original_crc;          /**< Original CRC */
    uint32_t repaired_crc;          /**< Repaired CRC */
    char     method[32];            /**< Repair method description */
    uint8_t  reserved[16];
} axdf_repair_entry_t;

/** Repair types */
typedef enum {
    AXDF_REPAIR_CRC_1BIT = 1,       /**< Single-bit CRC correction */
    AXDF_REPAIR_CRC_2BIT = 2,       /**< Two-bit CRC correction */
    AXDF_REPAIR_MULTI_REV = 3,      /**< Multi-revolution fusion */
    AXDF_REPAIR_INTERPOLATION = 4,  /**< Weak bit interpolation */
    AXDF_REPAIR_PATTERN = 5,        /**< Pattern-based reconstruction */
    AXDF_REPAIR_MANUAL = 6,         /**< Manual correction */
} axdf_repair_type_t;

/*===========================================================================
 * Context Structures
 *===========================================================================*/

typedef struct axdf_context_s axdf_context_t;

typedef struct {
    /* Callbacks */
    void (*on_track_read)(int track, int side, bool success, void *user);
    void (*on_sector_error)(int track, int side, int sector, uint8_t status, void *user);
    void (*on_repair)(const axdf_repair_entry_t *entry, void *user);
    void *user_data;
    
    /* Options */
    bool include_flux;              /**< Include raw flux data */
    bool include_mfm;               /**< Include MFM-level data */
    bool enable_repair;             /**< Enable automatic repair */
    int max_repair_bits;            /**< Maximum bits to repair (1-2) */
    int max_revolutions;            /**< Revolutions for fusion */
} axdf_options_t;

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Create AXDF context
 */
axdf_context_t* axdf_create(void);

/**
 * @brief Destroy AXDF context
 */
void axdf_destroy(axdf_context_t *ctx);

/**
 * @brief Set options
 */
int axdf_set_options(axdf_context_t *ctx, const axdf_options_t *options);

/**
 * @brief Import from ADF file
 */
int axdf_import_adf(axdf_context_t *ctx, const char *adf_path);

/**
 * @brief Import from flux capture (SCP, KryoFlux, etc.)
 */
int axdf_import_flux(axdf_context_t *ctx, const char *flux_path);

/**
 * @brief Export to AXDF file
 */
int axdf_export(axdf_context_t *ctx, const char *axdf_path);

/**
 * @brief Export to standard ADF (best-effort)
 */
int axdf_export_adf(axdf_context_t *ctx, const char *adf_path);

/**
 * @brief Validate disk structure
 */
int axdf_validate(axdf_context_t *ctx);

/**
 * @brief Run repair pipeline
 */
int axdf_repair(axdf_context_t *ctx);

/**
 * @brief Get track data
 */
int axdf_get_track(axdf_context_t *ctx, int track, int side,
                   axdf_track_entry_t *info, uint8_t **data, size_t *size);

/**
 * @brief Get sector data
 */
int axdf_get_sector(axdf_context_t *ctx, int track, int side, int sector,
                    axdf_sector_header_t *info, uint8_t **data, size_t *size);

/**
 * @brief Get repair log
 */
int axdf_get_repairs(axdf_context_t *ctx, axdf_repair_entry_t **entries, 
                     size_t *count);

/**
 * @brief Get error summary
 */
int axdf_get_error_summary(axdf_context_t *ctx, int *total_errors,
                           int *repaired, int *unreadable);

/**
 * @brief Get header info
 */
const axdf_header_t* axdf_get_header(const axdf_context_t *ctx);

/**
 * @brief Get last error message
 */
const char* axdf_get_error(const axdf_context_t *ctx);

/*===========================================================================
 * Recovery Pipeline Functions
 *===========================================================================*/

/**
 * @brief Attempt single-bit CRC correction
 */
int axdf_repair_crc_1bit(uint8_t *sector_data, size_t size,
                          uint32_t original_crc, uint32_t *new_crc);

/**
 * @brief Attempt two-bit CRC correction
 */
int axdf_repair_crc_2bit(uint8_t *sector_data, size_t size,
                          uint32_t original_crc, uint32_t *new_crc);

/**
 * @brief Fuse multiple revolutions
 */
int axdf_repair_multi_rev(const uint8_t **revolutions, int count,
                           const float *confidence, size_t size,
                           uint8_t *output, float *output_confidence);

/**
 * @brief Interpolate weak bits from surrounding data
 */
int axdf_repair_interpolate(uint8_t *data, size_t size,
                             const uint8_t *weak_mask);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AXDF_H */
