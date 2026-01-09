/**
 * @file uft_xdf_pxdf.h
 * @brief PXDF - PC IMG/IMA eXtended Disk Format
 * 
 * Forensic container for IBM PC compatible disk images.
 * Supports IMG, IMA, DSK, XDF (Microsoft), DMF formats.
 * 
 * PC Specifics:
 * - MFM encoding
 * - Multiple density (DD/HD/ED)
 * - 3.5" and 5.25" support
 * - Variable sector sizes (128-1024)
 * 
 * Formats:
 * - 360KB (5.25" DD)
 * - 720KB (3.5" DD)
 * - 1.2MB (5.25" HD)
 * - 1.44MB (3.5" HD)
 * - 2.88MB (3.5" ED)
 * - DMF (1.68MB)
 * - XDF (1.84MB)
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_XDF_PXDF_H
#define UFT_XDF_PXDF_H

#include "uft_xdf_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * PC Format Types
 *===========================================================================*/

typedef enum {
    PXDF_FORMAT_UNKNOWN = 0,
    
    /* 5.25" formats */
    PXDF_FORMAT_160K,           /**< 160KB SS/DD (8 sectors) */
    PXDF_FORMAT_180K,           /**< 180KB SS/DD (9 sectors) */
    PXDF_FORMAT_320K,           /**< 320KB DS/DD (8 sectors) */
    PXDF_FORMAT_360K,           /**< 360KB DS/DD (9 sectors) */
    PXDF_FORMAT_1200K,          /**< 1.2MB DS/HD (15 sectors) */
    
    /* 3.5" formats */
    PXDF_FORMAT_720K,           /**< 720KB DS/DD (9 sectors) */
    PXDF_FORMAT_1440K,          /**< 1.44MB DS/HD (18 sectors) */
    PXDF_FORMAT_2880K,          /**< 2.88MB DS/ED (36 sectors) */
    
    /* Extended formats */
    PXDF_FORMAT_DMF,            /**< 1.68MB DMF (21 sectors) */
    PXDF_FORMAT_XDF,            /**< 1.84MB XDF (variable) */
    PXDF_FORMAT_FDFORMAT,       /**< 1.72MB fdformat */
    
    /* Special */
    PXDF_FORMAT_CUSTOM,         /**< Non-standard format */
} pxdf_format_t;

/*===========================================================================
 * PC Constants
 *===========================================================================*/

/** Common parameters */
#define PXDF_SECTOR_SIZE_128    128
#define PXDF_SECTOR_SIZE_256    256
#define PXDF_SECTOR_SIZE_512    512
#define PXDF_SECTOR_SIZE_1024   1024

/** MFM parameters */
#define PXDF_GAP3_5_25_DD       80      /**< Gap 3 for 5.25" DD */
#define PXDF_GAP3_5_25_HD       84      /**< Gap 3 for 5.25" HD */
#define PXDF_GAP3_3_5_DD        80      /**< Gap 3 for 3.5" DD */
#define PXDF_GAP3_3_5_HD        108     /**< Gap 3 for 3.5" HD */

/** Data rates (kbps) */
#define PXDF_RATE_250           250     /**< DD */
#define PXDF_RATE_300           300     /**< DD in HD drive */
#define PXDF_RATE_500           500     /**< HD */
#define PXDF_RATE_1000          1000    /**< ED */

/*===========================================================================
 * PC Sector ID (CHRN)
 *===========================================================================*/

typedef struct __attribute__((packed)) {
    uint8_t cylinder;           /**< C - Physical cylinder */
    uint8_t head;               /**< H - Head (0/1) */
    uint8_t sector;             /**< R - Sector number */
    uint8_t size;               /**< N - Size code (0=128, 1=256, 2=512, 3=1024) */
} pxdf_sector_id_t;

/*===========================================================================
 * PC Format Descriptor
 *===========================================================================*/

typedef struct {
    pxdf_format_t format;       /**< Format type */
    
    /* Geometry */
    uint8_t cylinders;          /**< Number of cylinders */
    uint8_t heads;              /**< Number of heads */
    uint8_t sectors;            /**< Sectors per track */
    uint16_t sector_size;       /**< Bytes per sector */
    
    /* Physical */
    uint16_t data_rate;         /**< Data rate (kbps) */
    uint8_t gap3;               /**< Gap 3 length */
    uint8_t fill_byte;          /**< Format fill byte */
    
    /* Derived */
    uint32_t total_size;        /**< Total disk size */
    uint16_t total_sectors;     /**< Total sector count */
} pxdf_format_desc_t;

/*===========================================================================
 * PC Boot Sector
 *===========================================================================*/

typedef struct __attribute__((packed)) {
    uint8_t jump[3];            /**< Jump instruction */
    char oem_name[8];           /**< OEM name */
    
    /* BPB (BIOS Parameter Block) */
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    /* Extended BPB */
    uint8_t drive_number;
    uint8_t reserved;
    uint8_t boot_signature;
    uint32_t volume_serial;
    char volume_label[11];
    char filesystem[8];
} pxdf_boot_sector_t;

/*===========================================================================
 * PXDF Header Extension
 *===========================================================================*/

typedef struct __attribute__((packed)) {
    /* Format info */
    pxdf_format_t format;       /**< Detected format */
    uint8_t cylinders;
    uint8_t heads;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    uint16_t data_rate;
    
    /* Boot sector info */
    char oem_name[8];
    char volume_label[11];
    char filesystem[8];
    uint32_t volume_serial;
    uint8_t media_type;
    
    /* FAT info */
    uint8_t fat_type;           /**< 12, 16, or 32 */
    uint8_t fat_count;
    uint16_t root_entries;
    uint32_t total_clusters;
    
    /* Quality per track */
    uint8_t track_status[160];  /**< Up to 80 cyl × 2 heads */
    
    uint8_t reserved[64];
} pxdf_extension_t;

/*===========================================================================
 * PXDF API
 *===========================================================================*/

/** Create PXDF context */
xdf_context_t* pxdf_create(void);

/** Import IMG/IMA */
int pxdf_import_img(xdf_context_t *ctx, const char *path);

/** Export to IMG */
int pxdf_export_img(xdf_context_t *ctx, const char *path);

/** Detect format from size */
pxdf_format_t pxdf_detect_format(size_t size);

/** Get format descriptor */
int pxdf_get_format_desc(pxdf_format_t format, pxdf_format_desc_t *desc);

/** Parse boot sector */
int pxdf_parse_boot_sector(const uint8_t *data, pxdf_boot_sector_t *boot);

/** Validate FAT filesystem */
int pxdf_validate_fat(xdf_context_t *ctx, int *errors);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XDF_PXDF_H */
