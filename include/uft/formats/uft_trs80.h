/**
 * @file uft_trs80.h
 * @brief TRS-80 Disk Format Support
 * @version 3.7.0
 * 
 * Comprehensive TRS-80 format support including:
 * - JV1: Simple sector image (35×10×256)
 * - JV3: Full sector map with mixed density
 * - JVC: Extended JV1 with optional header
 * - DMK: Raw track format (integrated via uft_dmk.h)
 * - Multiple DOS variants detection
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_TRS80_H
#define UFT_TRS80_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Return Codes
 *============================================================================*/

typedef enum uft_trs80_rc {
    UFT_TRS80_SUCCESS       =  0,
    UFT_TRS80_ERR_ARG       = -1,
    UFT_TRS80_ERR_IO        = -2,
    UFT_TRS80_ERR_NOMEM     = -3,
    UFT_TRS80_ERR_FORMAT    = -4,
    UFT_TRS80_ERR_GEOMETRY  = -5,
    UFT_TRS80_ERR_NOTFOUND  = -6,
    UFT_TRS80_ERR_RANGE     = -7,
    UFT_TRS80_ERR_READONLY  = -8,
    UFT_TRS80_ERR_CRC       = -9,
    UFT_TRS80_ERR_DENSITY   = -10
} uft_trs80_rc_t;

/*============================================================================
 * Model Types
 *============================================================================*/

typedef enum uft_trs80_model {
    UFT_TRS80_MODEL_UNKNOWN = 0,
    UFT_TRS80_MODEL_I       = 1,   /* TRS-80 Model I */
    UFT_TRS80_MODEL_II      = 2,   /* TRS-80 Model II */
    UFT_TRS80_MODEL_III     = 3,   /* TRS-80 Model III */
    UFT_TRS80_MODEL_4       = 4,   /* TRS-80 Model 4 */
    UFT_TRS80_MODEL_4P      = 5,   /* TRS-80 Model 4P */
    UFT_TRS80_MODEL_4D      = 6,   /* TRS-80 Model 4D */
    UFT_TRS80_MODEL_12      = 7,   /* TRS-80 Model 12 */
    UFT_TRS80_MODEL_16      = 8,   /* TRS-80 Model 16 */
    UFT_TRS80_MODEL_COCO    = 9,   /* Color Computer */
    UFT_TRS80_MODEL_MC10    = 10   /* MC-10 */
} uft_trs80_model_t;

/*============================================================================
 * Format Types
 *============================================================================*/

typedef enum uft_trs80_format {
    UFT_TRS80_FMT_UNKNOWN   = 0,
    UFT_TRS80_FMT_JV1       = 1,   /* Jeff Vavasour format 1 */
    UFT_TRS80_FMT_JV3       = 2,   /* Jeff Vavasour format 3 */
    UFT_TRS80_FMT_JVC       = 3,   /* JV1 with optional header */
    UFT_TRS80_FMT_DMK       = 4,   /* David Keil format */
    UFT_TRS80_FMT_VDK       = 5,   /* VDK format (CoCo) */
    UFT_TRS80_FMT_DSK       = 6,   /* Raw sector dump */
    UFT_TRS80_FMT_HFE       = 7,   /* UFT HFE Format */
    UFT_TRS80_FMT_IMD       = 8    /* ImageDisk */
} uft_trs80_format_t;

/*============================================================================
 * DOS Types
 *============================================================================*/

typedef enum uft_trs80_dos {
    UFT_TRS80_DOS_UNKNOWN   = 0,
    UFT_TRS80_DOS_TRSDOS_23 = 1,   /* TRSDOS 2.3 (Model I) */
    UFT_TRS80_DOS_TRSDOS_13 = 2,   /* TRSDOS 1.3 (Model III) */
    UFT_TRS80_DOS_TRSDOS_6  = 3,   /* TRSDOS 6.x / LS-DOS */
    UFT_TRS80_DOS_NEWDOS80  = 4,   /* NewDOS/80 */
    UFT_TRS80_DOS_LDOS      = 5,   /* LDOS */
    UFT_TRS80_DOS_DOSPLUS   = 6,   /* DOS+ */
    UFT_TRS80_DOS_MULTIDOS  = 7,   /* MultiDOS */
    UFT_TRS80_DOS_DOUBLEDOS = 8,   /* DoubleDOS */
    UFT_TRS80_DOS_CPM       = 9,   /* CP/M */
    UFT_TRS80_DOS_FLEX      = 10,  /* FLEX */
    UFT_TRS80_DOS_OS9       = 11,  /* OS-9 */
    UFT_TRS80_DOS_RSDOS     = 12   /* RS-DOS (CoCo) */
} uft_trs80_dos_t;

/*============================================================================
 * Geometry Presets
 *============================================================================*/

typedef enum uft_trs80_geometry_type {
    UFT_TRS80_GEOM_UNKNOWN      = 0,
    UFT_TRS80_GEOM_M1_SSSD      = 1,   /* Model I: 35T×1H×10S×256B = 89.6KB */
    UFT_TRS80_GEOM_M1_DSSD      = 2,   /* Model I: 35T×2H×10S×256B = 179.2KB */
    UFT_TRS80_GEOM_M1_SSDD      = 3,   /* Model I: 35T×1H×18S×256B = 161.3KB */
    UFT_TRS80_GEOM_M1_DSDD      = 4,   /* Model I: 35T×2H×18S×256B = 322.6KB */
    UFT_TRS80_GEOM_M3_SSDD      = 5,   /* Model III: 40T×1H×18S×256B = 184.3KB */
    UFT_TRS80_GEOM_M3_DSDD      = 6,   /* Model III: 40T×2H×18S×256B = 368.6KB */
    UFT_TRS80_GEOM_M4_SSDD      = 7,   /* Model 4: 40T×1H×18S×256B */
    UFT_TRS80_GEOM_M4_DSDD      = 8,   /* Model 4: 40T×2H×18S×256B */
    UFT_TRS80_GEOM_M4_DS80      = 9,   /* Model 4: 80T×2H×18S×256B = 737.3KB */
    UFT_TRS80_GEOM_M4_DS80_HD   = 10,  /* Model 4: 80T×2H×36S×256B = 1.4MB */
    UFT_TRS80_GEOM_COCO_SSSD    = 11,  /* CoCo: 35T×1H×18S×256B = 161.3KB */
    UFT_TRS80_GEOM_COCO_DSDD    = 12,  /* CoCo: 40T×2H×18S×256B = 368.6KB */
    UFT_TRS80_GEOM_COCO_80T     = 13,  /* CoCo: 80T×2H×18S×256B = 737.3KB */
    UFT_TRS80_GEOM_COUNT        = 14
} uft_trs80_geometry_type_t;

/*============================================================================
 * Density Types
 *============================================================================*/

typedef enum uft_trs80_density {
    UFT_TRS80_DENSITY_UNKNOWN   = 0,
    UFT_TRS80_DENSITY_FM        = 1,   /* Single density (FM) */
    UFT_TRS80_DENSITY_MFM       = 2,   /* Double density (MFM) */
    UFT_TRS80_DENSITY_MIXED     = 3    /* Mixed FM/MFM */
} uft_trs80_density_t;

/*============================================================================
 * Geometry Structure
 *============================================================================*/

typedef struct uft_trs80_geometry {
    uft_trs80_geometry_type_t type;
    uft_trs80_model_t model;
    uint16_t tracks;
    uint8_t  heads;
    uint8_t  sectors_per_track;
    uint16_t sector_size;
    uint32_t total_bytes;
    uft_trs80_density_t density;
    const char* name;
} uft_trs80_geometry_t;

/*============================================================================
 * JV3 Sector Descriptor
 *============================================================================*/

#define UFT_JV3_SECTORS_MAX     2901    /* Maximum sectors in JV3 */
#define UFT_JV3_FREE            0xFF    /* Free sector marker */
#define UFT_JV3_FLAG_NDAM       0x80    /* Non-IBM data address mark */
#define UFT_JV3_FLAG_SIDES      0x10    /* Side 1 if set */
#define UFT_JV3_FLAG_ERROR      0x08    /* CRC error if set */
#define UFT_JV3_FLAG_DDEN       0x04    /* Double density if set */
#define UFT_JV3_FLAG_SIDEONE    0x02    /* Alternate side 1 bit */
#define UFT_JV3_FLAG_SIZE_MASK  0x03    /* Sector size code */

#pragma pack(push, 1)
typedef struct uft_jv3_sector_header {
    uint8_t track;
    uint8_t sector;
    uint8_t flags;
} uft_jv3_sector_header_t;
#pragma pack(pop)

/*============================================================================
 * JVC Header (optional)
 *============================================================================*/

typedef struct uft_jvc_header {
    bool    present;
    uint8_t header_size;        /* 0-5 bytes */
    uint8_t sectors_per_track;  /* Byte 0 if present */
    uint8_t side_count;         /* Byte 1 if present */
    uint8_t sector_size_code;   /* Byte 2 if present */
    uint8_t first_sector;       /* Byte 3 if present */
    uint8_t sector_attr_flag;   /* Byte 4 if present */
} uft_jvc_header_t;

/*============================================================================
 * Disk Context
 *============================================================================*/

typedef struct uft_trs80_ctx {
    char*   path;
    bool    writable;
    uint64_t file_size;
    
    /* Format */
    uft_trs80_format_t format;
    uft_trs80_geometry_t geometry;
    
    /* JV3 specific */
    uft_jv3_sector_header_t jv3_sectors[UFT_JV3_SECTORS_MAX];
    uint16_t jv3_sector_count;
    uint8_t  jv3_write_protected;
    
    /* JVC specific */
    uft_jvc_header_t jvc_header;
    
    /* Detection */
    uft_trs80_dos_t dos_type;
    uft_trs80_model_t model;
    uint8_t format_confidence;
} uft_trs80_ctx_t;

/*============================================================================
 * Copy Protection
 *============================================================================*/

typedef enum uft_trs80_protection {
    UFT_TRS80_PROT_NONE         = 0,
    UFT_TRS80_PROT_CRC_ERRORS   = (1 << 0),  /* Intentional CRC errors */
    UFT_TRS80_PROT_MIXED_DENSITY = (1 << 1), /* Mixed FM/MFM on track */
    UFT_TRS80_PROT_EXTRA_SECTORS = (1 << 2), /* Extra sectors on track */
    UFT_TRS80_PROT_DAM_VARIANTS = (1 << 3),  /* Non-standard DAMs */
    UFT_TRS80_PROT_TIMING       = (1 << 4)   /* Timing-based */
} uft_trs80_protection_t;

typedef struct uft_trs80_protection_result {
    uint32_t flags;
    uint8_t  confidence;
    uint8_t  crc_error_count;
    uint8_t  mixed_density_tracks;
    char     description[128];
} uft_trs80_protection_result_t;

/*============================================================================
 * Analysis Report
 *============================================================================*/

typedef struct uft_trs80_report {
    uft_trs80_format_t format;
    uft_trs80_geometry_t geometry;
    uft_trs80_dos_t dos_type;
    uft_trs80_model_t model;
    
    /* Statistics */
    uint32_t total_sectors;
    uint32_t used_sectors;
    uint32_t free_sectors;
    uint32_t error_sectors;
    
    /* Features */
    bool     is_bootable;
    bool     has_directory;
    char     disk_name[16];
    
    /* Protection */
    uft_trs80_protection_result_t protection;
} uft_trs80_report_t;

/*============================================================================
 * Geometry API
 *============================================================================*/

const uft_trs80_geometry_t* uft_trs80_get_geometry(uft_trs80_geometry_type_t type);
uft_trs80_geometry_type_t uft_trs80_detect_geometry_by_size(uint64_t file_size, 
                                                            uint8_t* confidence);
const char* uft_trs80_model_name(uft_trs80_model_t model);
const char* uft_trs80_dos_name(uft_trs80_dos_t dos);
const char* uft_trs80_format_name(uft_trs80_format_t format);

/*============================================================================
 * Disk Operations
 *============================================================================*/

uft_trs80_rc_t uft_trs80_open(uft_trs80_ctx_t* ctx, const char* path, bool writable);
void uft_trs80_close(uft_trs80_ctx_t* ctx);

/*============================================================================
 * JV1 Operations
 *============================================================================*/

#define UFT_JV1_TRACKS          35
#define UFT_JV1_SECTORS         10
#define UFT_JV1_SECTOR_SIZE     256
#define UFT_JV1_FILE_SIZE       (UFT_JV1_TRACKS * UFT_JV1_SECTORS * UFT_JV1_SECTOR_SIZE)

bool uft_jv1_detect(uint64_t file_size, const uint8_t* data, size_t data_size, 
                    uint8_t* confidence);
uft_trs80_rc_t uft_jv1_read_sector(uft_trs80_ctx_t* ctx, uint8_t track, 
                                    uint8_t sector, uint8_t* buffer, size_t size);
uft_trs80_rc_t uft_jv1_write_sector(uft_trs80_ctx_t* ctx, uint8_t track,
                                     uint8_t sector, const uint8_t* data, size_t size);
uft_trs80_rc_t uft_jv1_create_blank(const char* path);

/*============================================================================
 * JV3 Operations
 *============================================================================*/

#define UFT_JV3_HEADER_SIZE     (UFT_JV3_SECTORS_MAX * 3 + 1)

bool uft_jv3_detect(const uint8_t* data, size_t data_size, uint8_t* confidence);
uft_trs80_rc_t uft_jv3_read_header(uft_trs80_ctx_t* ctx);
uft_trs80_rc_t uft_jv3_find_sector(uft_trs80_ctx_t* ctx, uint8_t track, 
                                    uint8_t side, uint8_t sector,
                                    uint16_t* index, uint16_t* size);
uft_trs80_rc_t uft_jv3_read_sector(uft_trs80_ctx_t* ctx, uint8_t track,
                                    uint8_t side, uint8_t sector,
                                    uint8_t* buffer, size_t buffer_size);
uint16_t uft_jv3_sector_size(uint8_t flags);

/*============================================================================
 * JVC Operations
 *============================================================================*/

bool uft_jvc_detect(uint64_t file_size, const uint8_t* data, size_t data_size,
                    uft_jvc_header_t* header, uint8_t* confidence);
uft_trs80_rc_t uft_jvc_read_sector(uft_trs80_ctx_t* ctx, uint8_t track,
                                    uint8_t side, uint8_t sector,
                                    uint8_t* buffer, size_t size);

/*============================================================================
 * DOS Detection
 *============================================================================*/

uft_trs80_dos_t uft_trs80_detect_dos(const uint8_t* boot_sector, size_t size);

/*============================================================================
 * Copy Protection
 *============================================================================*/

uft_trs80_rc_t uft_trs80_detect_protection(uft_trs80_ctx_t* ctx,
                                            uft_trs80_protection_result_t* result);

/*============================================================================
 * Format Creation
 *============================================================================*/

uft_trs80_rc_t uft_trs80_create_blank(const char* path, uft_trs80_format_t format,
                                       uft_trs80_geometry_type_t geometry);

/*============================================================================
 * Format Conversion
 *============================================================================*/

uft_trs80_rc_t uft_trs80_jv1_to_jv3(const char* jv1_path, const char* jv3_path);
uft_trs80_rc_t uft_trs80_to_raw(uft_trs80_ctx_t* ctx, const char* output_path);

/*============================================================================
 * Analysis and Reporting
 *============================================================================*/

uft_trs80_rc_t uft_trs80_analyze(const char* path, uft_trs80_report_t* report);
int uft_trs80_report_to_json(const uft_trs80_report_t* report, char* json_out, size_t capacity);
int uft_trs80_report_to_markdown(const uft_trs80_report_t* report, char* md_out, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRS80_H */
