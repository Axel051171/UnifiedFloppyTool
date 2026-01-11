/**
 * @file uft_msx.h
 * @brief MSX Disk Format Support
 * @version 3.6.0
 * 
 * Comprehensive MSX disk format support including:
 * - MSX-DOS 1.x/2.x filesystem parsing
 * - Multiple geometries (360KB, 720KB, 1.44MB)
 * - Copy protection detection
 * - Nextor compatibility
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_MSX_H
#define UFT_MSX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Return Codes
 *============================================================================*/

typedef enum uft_msx_rc {
    UFT_MSX_SUCCESS         =  0,
    UFT_MSX_ERR_ARG         = -1,
    UFT_MSX_ERR_IO          = -2,
    UFT_MSX_ERR_NOMEM       = -3,
    UFT_MSX_ERR_FORMAT      = -4,
    UFT_MSX_ERR_GEOMETRY    = -5,
    UFT_MSX_ERR_NOTFOUND    = -6,
    UFT_MSX_ERR_RANGE       = -7,
    UFT_MSX_ERR_READONLY    = -8,
    UFT_MSX_ERR_FULL        = -9
} uft_msx_rc_t;

/*============================================================================
 * Geometry Types
 *============================================================================*/

typedef enum uft_msx_geometry_type {
    UFT_MSX_GEOM_UNKNOWN    = 0,
    UFT_MSX_GEOM_1DD_360    = 1,   /* 80T x 1H x 9S x 512B = 360KB SS 3.5" */
    UFT_MSX_GEOM_2DD_720    = 2,   /* 80T x 2H x 9S x 512B = 720KB DS 3.5" */
    UFT_MSX_GEOM_1DD_180    = 3,   /* 40T x 1H x 9S x 512B = 180KB SS 5.25" */
    UFT_MSX_GEOM_2DD_360_5  = 4,   /* 40T x 2H x 9S x 512B = 360KB DS 5.25" */
    UFT_MSX_GEOM_2HD_1440   = 5,   /* 80T x 2H x 18S x 512B = 1.44MB Turbo-R */
    UFT_MSX_GEOM_CUSTOM     = 6,
    UFT_MSX_GEOM_COUNT      = 7
} uft_msx_geometry_type_t;

/*============================================================================
 * DOS Version Detection
 *============================================================================*/

typedef enum uft_msx_dos_version {
    UFT_MSX_DOS_UNKNOWN     = 0,
    UFT_MSX_DOS_1           = 1,   /* MSX-DOS 1.x */
    UFT_MSX_DOS_2           = 2,   /* MSX-DOS 2.x */
    UFT_MSX_NEXTOR          = 3,   /* Nextor */
    UFT_MSX_BASIC           = 4,   /* Disk BASIC only */
    UFT_MSX_CPM             = 5    /* CP/M-80 */
} uft_msx_dos_version_t;

/*============================================================================
 * Copy Protection Types
 *============================================================================*/

typedef enum uft_msx_protection {
    UFT_MSX_PROT_NONE           = 0,
    UFT_MSX_PROT_EXTRA_TRACKS   = (1 << 0),  /* Tracks beyond 80 */
    UFT_MSX_PROT_EXTRA_SECTORS  = (1 << 1),  /* Sectors beyond standard */
    UFT_MSX_PROT_BAD_SECTORS    = (1 << 2),  /* Intentional bad sectors */
    UFT_MSX_PROT_CUSTOM_FORMAT  = (1 << 3),  /* Non-standard format */
    UFT_MSX_PROT_WEAK_BITS      = (1 << 4),  /* Weak/unstable bits */
    UFT_MSX_PROT_TIMING         = (1 << 5),  /* Timing-based protection */
    UFT_MSX_PROT_MEDIA_DESC     = (1 << 6)   /* Non-standard media descriptor */
} uft_msx_protection_t;

/*============================================================================
 * Geometry Structure
 *============================================================================*/

typedef struct uft_msx_geometry {
    uft_msx_geometry_type_t type;
    uint16_t tracks;
    uint8_t  heads;
    uint8_t  sectors_per_track;
    uint16_t sector_size;
    uint32_t total_bytes;
    uint8_t  media_descriptor;
    const char* name;
} uft_msx_geometry_t;

/*============================================================================
 * BIOS Parameter Block (BPB)
 *============================================================================*/

#pragma pack(push, 1)
typedef struct uft_msx_bpb {
    uint8_t  jump[3];              /* Jump instruction */
    uint8_t  oem_name[8];          /* OEM name */
    uint16_t bytes_per_sector;     /* Bytes per sector */
    uint8_t  sectors_per_cluster;  /* Sectors per cluster */
    uint16_t reserved_sectors;     /* Reserved sectors */
    uint8_t  num_fats;             /* Number of FATs */
    uint16_t root_entries;         /* Root directory entries */
    uint16_t total_sectors_16;     /* Total sectors (16-bit) */
    uint8_t  media_descriptor;     /* Media descriptor */
    uint16_t sectors_per_fat;      /* Sectors per FAT */
    uint16_t sectors_per_track;    /* Sectors per track */
    uint16_t num_heads;            /* Number of heads */
    uint32_t hidden_sectors;       /* Hidden sectors */
    uint32_t total_sectors_32;     /* Total sectors (32-bit) */
} uft_msx_bpb_t;
#pragma pack(pop)

/*============================================================================
 * Directory Entry
 *============================================================================*/

#define UFT_MSX_ATTR_READONLY   0x01
#define UFT_MSX_ATTR_HIDDEN     0x02
#define UFT_MSX_ATTR_SYSTEM     0x04
#define UFT_MSX_ATTR_VOLUME     0x08
#define UFT_MSX_ATTR_DIRECTORY  0x10
#define UFT_MSX_ATTR_ARCHIVE    0x20

#pragma pack(push, 1)
typedef struct uft_msx_dirent {
    uint8_t  name[8];              /* Filename */
    uint8_t  ext[3];               /* Extension */
    uint8_t  attributes;           /* File attributes */
    uint8_t  reserved[10];         /* Reserved */
    uint16_t time;                 /* Last modified time */
    uint16_t date;                 /* Last modified date */
    uint16_t start_cluster;        /* Starting cluster */
    uint32_t file_size;            /* File size in bytes */
} uft_msx_dirent_t;
#pragma pack(pop)

/*============================================================================
 * Directory Entry (Expanded)
 *============================================================================*/

typedef struct uft_msx_file_info {
    char     filename[13];         /* 8.3 format with dot and NUL */
    uint8_t  attributes;
    uint32_t size;
    uint16_t start_cluster;
    uint16_t date;
    uint16_t time;
    bool     is_directory;
    bool     is_hidden;
    bool     is_system;
    bool     is_readonly;
} uft_msx_file_info_t;

/*============================================================================
 * Disk Context
 *============================================================================*/

typedef struct uft_msx_ctx {
    char*    path;
    bool     writable;
    uint64_t file_size;
    
    /* Geometry */
    uft_msx_geometry_t geometry;
    
    /* BPB info */
    uft_msx_bpb_t bpb;
    bool     has_valid_bpb;
    
    /* FAT info */
    uint32_t fat_start_sector;
    uint32_t fat_sectors;
    uint32_t root_dir_sector;
    uint32_t root_dir_sectors;
    uint32_t data_start_sector;
    uint32_t total_clusters;
    
    /* Detection */
    uft_msx_dos_version_t dos_version;
    uint32_t protection_flags;
    uint8_t  protection_confidence;
} uft_msx_ctx_t;

/*============================================================================
 * Protection Detection Result
 *============================================================================*/

typedef struct uft_msx_protection_result {
    uint32_t flags;                /* UFT_MSX_PROT_* flags */
    uint8_t  confidence;           /* 0-100% */
    uint8_t  extra_tracks;         /* Number of extra tracks */
    uint8_t  extra_sectors;        /* Extra sectors per track */
    uint8_t  bad_sector_count;     /* Number of bad sectors */
    char     description[128];     /* Human-readable description */
} uft_msx_protection_result_t;

/*============================================================================
 * Analysis Report
 *============================================================================*/

typedef struct uft_msx_report {
    uft_msx_geometry_t geometry;
    uft_msx_dos_version_t dos_version;
    char     volume_label[12];
    char     oem_name[9];
    
    /* Statistics */
    uint32_t total_sectors;
    uint32_t used_clusters;
    uint32_t free_clusters;
    uint32_t bytes_per_cluster;
    uint32_t total_space;
    uint32_t free_space;
    uint32_t file_count;
    uint32_t dir_count;
    
    /* Features */
    bool     has_autoexec;
    bool     is_bootable;
    bool     has_subdirs;
    
    /* Protection */
    uft_msx_protection_result_t protection;
} uft_msx_report_t;

/*============================================================================
 * Directory Callback
 *============================================================================*/

typedef bool (*uft_msx_dir_callback_t)(const uft_msx_file_info_t* file, void* user_data);

/*============================================================================
 * Geometry API
 *============================================================================*/

const uft_msx_geometry_t* uft_msx_get_geometry(uft_msx_geometry_type_t type);
uft_msx_geometry_type_t uft_msx_detect_geometry_by_size(uint64_t file_size, uint8_t* confidence);
uft_msx_rc_t uft_msx_validate_geometry(uint16_t tracks, uint8_t heads, 
                                        uint8_t sectors, uint16_t sector_size);

/*============================================================================
 * Disk Operations
 *============================================================================*/

uft_msx_rc_t uft_msx_open(uft_msx_ctx_t* ctx, const char* path, bool writable);
void uft_msx_close(uft_msx_ctx_t* ctx);
uft_msx_rc_t uft_msx_read_sector(uft_msx_ctx_t* ctx, uint32_t lba, 
                                  uint8_t* buffer, size_t buffer_size);
uft_msx_rc_t uft_msx_write_sector(uft_msx_ctx_t* ctx, uint32_t lba,
                                   const uint8_t* data, size_t data_size);

/*============================================================================
 * Filesystem Operations
 *============================================================================*/

uft_msx_rc_t uft_msx_read_root_dir(uft_msx_ctx_t* ctx, 
                                    uft_msx_dir_callback_t callback, void* user_data);
uft_msx_rc_t uft_msx_find_file(uft_msx_ctx_t* ctx, const char* filename,
                                uft_msx_file_info_t* info);
uft_msx_rc_t uft_msx_extract_file(uft_msx_ctx_t* ctx, const char* filename,
                                   const char* output_path);
uft_msx_rc_t uft_msx_get_volume_label(uft_msx_ctx_t* ctx, char* label, size_t label_size);

/*============================================================================
 * DOS Version Detection
 *============================================================================*/

uft_msx_dos_version_t uft_msx_detect_dos_version(const uft_msx_ctx_t* ctx);
const char* uft_msx_dos_version_name(uft_msx_dos_version_t version);

/*============================================================================
 * Copy Protection
 *============================================================================*/

uft_msx_rc_t uft_msx_detect_protection(const char* path, 
                                        uft_msx_protection_result_t* result);

/*============================================================================
 * Format Creation
 *============================================================================*/

uft_msx_rc_t uft_msx_create_blank(const char* path, uft_msx_geometry_type_t geometry,
                                   const char* volume_label);

/*============================================================================
 * Conversion
 *============================================================================*/

uft_msx_rc_t uft_msx_to_raw(uft_msx_ctx_t* ctx, const char* output_path);
uft_msx_rc_t uft_msx_from_raw(const char* raw_path, const char* output_path,
                               uft_msx_geometry_type_t geometry);

/*============================================================================
 * Analysis and Reporting
 *============================================================================*/

uft_msx_rc_t uft_msx_analyze(const char* path, uft_msx_report_t* report);
int uft_msx_report_to_json(const uft_msx_report_t* report, char* json_out, size_t capacity);
int uft_msx_report_to_markdown(const uft_msx_report_t* report, char* md_out, size_t capacity);

/*============================================================================
 * FAT Operations
 *============================================================================*/

uint16_t uft_msx_fat_get_entry(uft_msx_ctx_t* ctx, uint16_t cluster);
uft_msx_rc_t uft_msx_fat_set_entry(uft_msx_ctx_t* ctx, uint16_t cluster, uint16_t value);
uint16_t uft_msx_fat_find_free(uft_msx_ctx_t* ctx);
uint32_t uft_msx_fat_count_free(uft_msx_ctx_t* ctx);

/*============================================================================
 * Media Descriptors
 *============================================================================*/

#define UFT_MSX_MEDIA_1DD_360   0xF8   /* 360KB single-sided */
#define UFT_MSX_MEDIA_2DD_720   0xF9   /* 720KB double-sided */
#define UFT_MSX_MEDIA_1DD_180   0xFC   /* 180KB single-sided 5.25" */
#define UFT_MSX_MEDIA_2DD_360   0xFD   /* 360KB double-sided 5.25" */
#define UFT_MSX_MEDIA_2HD_1440  0xF0   /* 1.44MB */

#ifdef __cplusplus
}
#endif

#endif /* UFT_MSX_H */
