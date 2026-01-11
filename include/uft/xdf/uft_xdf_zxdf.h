/**
 * @file uft_xdf_zxdf.h
 * @brief ZXDF - ZX Spectrum TRD/DSK eXtended Disk Format
 * 
 * Forensic container for ZX Spectrum disk images.
 * Supports TRD (TR-DOS), SCL, DSK (CPC-style), and FDI formats.
 * 
 * ZX Spectrum Disk Systems:
 * - Beta Disk (TR-DOS)
 * - +3 DOS (CPC-compatible)
 * - Opus Discovery
 * - DISCiPLE/+D
 * 
 * Formats:
 * - TRD: Beta Disk / TR-DOS
 * - SCL: Compressed TRD
 * - DSK: Extended DSK (CPC/+3)
 * - FDI: UKV Spectrum Debugger
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_XDF_ZXDF_H
#define UFT_XDF_ZXDF_H

#include "uft_xdf_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * ZX Spectrum Constants
 *===========================================================================*/

/** TR-DOS parameters */
#define ZXDF_TRDOS_TRACKS       80
#define ZXDF_TRDOS_SIDES        2
#define ZXDF_TRDOS_SECTORS      16      /**< Sectors per track */
#define ZXDF_TRDOS_SECTOR_SIZE  256     /**< Sector size */

/** +3 DOS parameters (CPC-compatible) */
#define ZXDF_PLUS3_TRACKS       40      /**< Or 80 */
#define ZXDF_PLUS3_SIDES        1       /**< Or 2 */
#define ZXDF_PLUS3_SECTORS      9       /**< Sectors per track */
#define ZXDF_PLUS3_SECTOR_SIZE  512     /**< Sector size */

/** Standard sizes */
#define ZXDF_SIZE_TRDOS         (80 * 2 * 16 * 256)     /**< 640KB */
#define ZXDF_SIZE_PLUS3_SS      (40 * 9 * 512)          /**< 180KB */
#define ZXDF_SIZE_PLUS3_DS      (40 * 2 * 9 * 512)      /**< 360KB */

/*===========================================================================
 * ZX Format Types
 *===========================================================================*/

typedef enum {
    ZXDF_FORMAT_UNKNOWN = 0,
    ZXDF_FORMAT_TRD,            /**< TR-DOS raw image */
    ZXDF_FORMAT_SCL,            /**< SCL container */
    ZXDF_FORMAT_DSK,            /**< Extended DSK (CPC/+3) */
    ZXDF_FORMAT_FDI,            /**< FDI format */
    ZXDF_FORMAT_TD0,            /**< Teledisk */
    ZXDF_FORMAT_UDI,            /**< Ultra Disk Image */
} zxdf_format_t;

/*===========================================================================
 * TR-DOS Structures
 *===========================================================================*/

/**
 * @brief TR-DOS catalog entry
 */
#pragma pack(push, 1)
typedef struct {
    char name[8];               /**< Filename */
    uint8_t type;               /**< File type: B/C/D/# */
    uint16_t start;             /**< Start address */
    uint16_t length;            /**< File length */
    uint8_t sector_count;       /**< Sectors occupied */
    uint8_t first_sector;       /**< First sector */
    uint8_t first_track;        /**< First track */
} zxdf_trdos_entry_t;
#pragma pack(pop)

/**
 * @brief TR-DOS disk info (sector 9, track 0)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t zero;               /**< Always 0 */
    uint8_t reserved[224];      /**< Unused */
    uint8_t first_free_sector;  /**< First free sector */
    uint8_t first_free_track;   /**< First free track */
    uint8_t disk_type;          /**< Disk type */
    uint8_t file_count;         /**< Number of files */
    uint16_t free_sectors;      /**< Free sectors (little-endian) */
    uint8_t trdos_id;           /**< 0x10 = TR-DOS */
    uint8_t reserved2[2];
    uint8_t password[9];        /**< Password (space-padded) */
    uint8_t reserved3;
    uint8_t deleted_files;      /**< Deleted file count */
    char label[8];              /**< Disk label */
    uint8_t reserved4[3];
} zxdf_trdos_info_t;
#pragma pack(pop)

/** TR-DOS disk types */
#define ZXDF_TRDOS_DS_80        0x16    /**< Double-sided 80 track */
#define ZXDF_TRDOS_DS_40        0x17    /**< Double-sided 40 track */
#define ZXDF_TRDOS_SS_80        0x18    /**< Single-sided 80 track */
#define ZXDF_TRDOS_SS_40        0x19    /**< Single-sided 40 track */

/** TR-DOS file types */
#define ZXDF_TRDOS_TYPE_BASIC   'B'     /**< BASIC program */
#define ZXDF_TRDOS_TYPE_CODE    'C'     /**< Machine code */
#define ZXDF_TRDOS_TYPE_DATA    'D'     /**< Data array */
#define ZXDF_TRDOS_TYPE_PRINT   '#'     /**< Print file */

/*===========================================================================
 * SCL Container
 *===========================================================================*/

#pragma pack(push, 1)
typedef struct {
    char magic[8];              /**< "SINCLAIR" */
    uint8_t file_count;         /**< Number of files */
} zxdf_scl_header_t;
#pragma pack(pop)

#define ZXDF_SCL_MAGIC          "SINCLAIR"

/*===========================================================================
 * Extended DSK (CPC/+3 compatible)
 *===========================================================================*/

#pragma pack(push, 1)
typedef struct {
    char magic[34];             /**< "EXTENDED CPC DSK File\r\nDisk-Info\r\n" or
                                     "MV - CPCEMU Disk-File\r\nDisk-Info\r\n" */
    char creator[14];           /**< Creator name */
    uint8_t tracks;             /**< Number of tracks */
    uint8_t sides;              /**< Number of sides */
    uint16_t unused;            /**< Unused (standard DSK: track size) */
    uint8_t track_sizes[204];   /**< Track size table (high bytes) */
} zxdf_dsk_header_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char magic[13];             /**< "Track-Info\r\n\0" */
    uint8_t unused[3];
    uint8_t track;              /**< Track number */
    uint8_t side;               /**< Side number */
    uint8_t unused2[2];
    uint8_t sector_size;        /**< Sector size code (2 = 512) */
    uint8_t sector_count;       /**< Sectors on this track */
    uint8_t gap3;               /**< Gap 3 length */
    uint8_t filler;             /**< Filler byte */
    /* Followed by sector info blocks */
} zxdf_dsk_track_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t track;              /**< C - Cylinder */
    uint8_t side;               /**< H - Head */
    uint8_t sector_id;          /**< R - Sector ID */
    uint8_t size;               /**< N - Size code */
    uint8_t fdc_status1;        /**< FDC status 1 */
    uint8_t fdc_status2;        /**< FDC status 2 */
    uint16_t data_length;       /**< Actual data length */
} zxdf_dsk_sector_t;
#pragma pack(pop)

/*===========================================================================
 * ZXDF Header Extension
 *===========================================================================*/

#pragma pack(push, 1)
typedef struct {
    /* Format info */
    zxdf_format_t format;
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    
    /* TR-DOS info */
    uint8_t trdos_type;
    uint8_t file_count;
    uint16_t free_sectors;
    char disk_label[8];
    
    /* File list (first 16 entries) */
    zxdf_trdos_entry_t files[16];
    
    /* Quality */
    uint8_t track_status[160];
    
    uint8_t reserved[64];
} zxdf_extension_t;
#pragma pack(pop)

/*===========================================================================
 * ZXDF API
 *===========================================================================*/

/** Create ZXDF context */
xdf_context_t* zxdf_create(void);

/** Import TRD */
int zxdf_import_trd(xdf_context_t *ctx, const char *path);

/** Import SCL */
int zxdf_import_scl(xdf_context_t *ctx, const char *path);

/** Import DSK */
int zxdf_import_dsk(xdf_context_t *ctx, const char *path);

/** Export to TRD */
int zxdf_export_trd(xdf_context_t *ctx, const char *path);

/** Export to SCL */
int zxdf_export_scl(xdf_context_t *ctx, const char *path);

/** Export to DSK */
int zxdf_export_dsk(xdf_context_t *ctx, const char *path);

/** Parse TR-DOS catalog */
int zxdf_parse_trdos_catalog(xdf_context_t *ctx, 
                               zxdf_trdos_entry_t *entries, 
                               size_t max_entries);

/** Get TR-DOS disk info */
int zxdf_get_trdos_info(xdf_context_t *ctx, zxdf_trdos_info_t *info);

/** Validate TR-DOS structure */
int zxdf_validate_trdos(xdf_context_t *ctx, int *errors);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XDF_ZXDF_H */
