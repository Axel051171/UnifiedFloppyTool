/**
 * @file uft_cpm_diskdef.h
 * @brief CP/M Disk Definition support
 * @version 3.9.0
 * 
 * Support for CP/M disk definitions compatible with cpmtools.
 * Defines disk parameter blocks (DPB) for various CP/M systems.
 * 
 * Reference: libdsk diskdefs, cpmtools
 */

#ifndef UFT_CPM_DISKDEF_H
#define UFT_CPM_DISKDEF_H

#include "uft/uft_format_common.h"
#include "uft/core/uft_disk_image_compat.h"
#include "uft/core/uft_error_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum definitions */
#define CPM_MAX_DISKDEFS        128
#define CPM_MAX_DISKDEF_NAME    32
#define CPM_MAX_SKEW_TABLE      64

/* Boot sector types */
typedef enum {
    CPM_BOOT_NONE = 0,           /* No boot sector */
    CPM_BOOT_CPM22,              /* CP/M 2.2 boot */
    CPM_BOOT_CPM3,               /* CP/M 3.0 boot */
    CPM_BOOT_SYSTEM,             /* System tracks reserved */
} cpm_boot_type_t;

/* Block shift calculation: BLS = 128 << BSH */
typedef enum {
    CPM_BLS_1024 = 3,            /* 1K blocks */
    CPM_BLS_2048 = 4,            /* 2K blocks */
    CPM_BLS_4096 = 5,            /* 4K blocks */
    CPM_BLS_8192 = 6,            /* 8K blocks */
    CPM_BLS_16384 = 7,           /* 16K blocks */
} cpm_block_shift_t;

/**
 * @brief CP/M Disk Parameter Block (DPB)
 * 
 * Standard CP/M DPB structure with extensions
 */
typedef struct {
    /* Standard DPB fields */
    uint16_t spt;                /* Sectors Per Track (logical 128-byte records) */
    uint8_t  bsh;                /* Block SHift (BLS = 128 << BSH) */
    uint8_t  blm;                /* BLock Mask (BLS/128 - 1) */
    uint8_t  exm;                /* EXtent Mask */
    uint16_t dsm;                /* Disk Size Maximum (total blocks - 1) */
    uint16_t drm;                /* DiRectory Max (directory entries - 1) */
    uint8_t  al0;                /* ALlocation 0 (directory allocation bitmap high) */
    uint8_t  al1;                /* ALlocation 1 (directory allocation bitmap low) */
    uint16_t cks;                /* ChecK Size (directory check vector size) */
    uint16_t off;                /* OFFset (reserved tracks for system) */
    
    /* Extended fields (CP/M 3.0) */
    uint8_t  psh;                /* Physical Sector sHift */
    uint8_t  phm;                /* PHysical sector Mask */
    
} cpm_dpb_t;

/**
 * @brief CP/M Disk Definition
 * 
 * Complete disk definition including physical parameters
 */
typedef struct {
    /* Identification */
    char     name[CPM_MAX_DISKDEF_NAME];
    char     description[64];
    
    /* Physical geometry */
    uint16_t cylinders;          /* Physical cylinders */
    uint8_t  heads;              /* Physical heads (sides) */
    uint8_t  sectors;            /* Physical sectors per track */
    uint16_t sector_size;        /* Physical sector size (bytes) */
    
    /* Logical layout */
    uint8_t  first_sector;       /* First sector number (0 or 1) */
    uint8_t  skew;               /* Sector skew factor */
    uint8_t  skew_table[CPM_MAX_SKEW_TABLE];  /* Explicit skew table (0 = use linear) */
    bool     has_skew_table;     /* Use explicit skew table? */
    
    /* Boot/system */
    cpm_boot_type_t boot_type;
    uint16_t system_tracks;      /* Reserved tracks for system */
    
    /* Disk Parameter Block */
    cpm_dpb_t dpb;
    
    /* Encoding */
    uft_encoding_t encoding;     /* FM or MFM */
    bool     double_step;        /* 40-track drive on 80-track media */
    
    /* Options */
    bool     uppercase_only;     /* Force uppercase filenames */
    uint8_t  extent_bytes;       /* 8 or 16 byte directory extents */
    
} cpm_diskdef_t;

/**
 * @brief CP/M disk read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    const cpm_diskdef_t *diskdef;
    
    /* Directory info */
    uint32_t total_entries;
    uint32_t used_entries;
    uint32_t deleted_entries;
    
    /* Space info */
    uint32_t total_blocks;
    uint32_t used_blocks;
    size_t   total_bytes;
    size_t   used_bytes;
    
} cpm_read_result_t;

/* ============================================================================
 * Predefined CP/M Disk Definitions
 * ============================================================================ */

/* Standard 8" formats */
extern const cpm_diskdef_t cpm_diskdef_ibm_8ss;      /* IBM 8" SS SD (250K) */
extern const cpm_diskdef_t cpm_diskdef_ibm_8ds;      /* IBM 8" DS SD (500K) */

/* Standard 5.25" formats */
extern const cpm_diskdef_t cpm_diskdef_kaypro2;      /* Kaypro II (191K) */
extern const cpm_diskdef_t cpm_diskdef_kaypro4;      /* Kaypro 4 (390K) */
extern const cpm_diskdef_t cpm_diskdef_osborne1;     /* Osborne 1 (92K) */
extern const cpm_diskdef_t cpm_diskdef_morrow_md2;   /* Morrow MD2 (384K) */
extern const cpm_diskdef_t cpm_diskdef_morrow_md3;   /* Morrow MD3 (384K) */
extern const cpm_diskdef_t cpm_diskdef_epson_qx10;   /* Epson QX-10 */
extern const cpm_diskdef_t cpm_diskdef_cromemco;     /* Cromemco */

/* Standard 3.5" formats */
extern const cpm_diskdef_t cpm_diskdef_amstrad_pcw;  /* Amstrad PCW (173K) */
extern const cpm_diskdef_t cpm_diskdef_amstrad_cpc;  /* Amstrad CPC (178K) */
extern const cpm_diskdef_t cpm_diskdef_spectrum_p3;  /* Spectrum +3 (173K) */
extern const cpm_diskdef_t cpm_diskdef_pcw_720;      /* Amstrad PCW 720K */

/* Z80 systems */
extern const cpm_diskdef_t cpm_diskdef_rc2014;       /* RC2014 CF format */
extern const cpm_diskdef_t cpm_diskdef_rcbus;        /* RCBus format */

/* Japanese systems */
extern const cpm_diskdef_t cpm_diskdef_nec_pc8001;   /* NEC PC-8001 */
extern const cpm_diskdef_t cpm_diskdef_sharp_mz80;   /* Sharp MZ-80 */

/* Array of all definitions */
extern const cpm_diskdef_t *cpm_diskdefs[];
extern const size_t cpm_diskdef_count;

/* ============================================================================
 * Disk Definition Functions
 * ============================================================================ */

/**
 * @brief Find disk definition by name
 */
const cpm_diskdef_t* uft_cpm_find_diskdef(const char *name);

/**
 * @brief Find disk definition by geometry
 */
const cpm_diskdef_t* uft_cpm_find_diskdef_by_geometry(
    uint16_t cylinders, uint8_t heads, uint8_t sectors, uint16_t sector_size);

/**
 * @brief Auto-detect disk definition from image
 */
const cpm_diskdef_t* uft_cpm_detect_diskdef(const uint8_t *data, size_t size);

/**
 * @brief List all available disk definitions
 */
size_t uft_cpm_list_diskdefs(const cpm_diskdef_t **defs, size_t max);

/**
 * @brief Calculate block size from DPB
 */
static inline uint16_t cpm_block_size(const cpm_dpb_t *dpb) {
    return 128 << dpb->bsh;
}

/**
 * @brief Calculate total disk capacity
 */
static inline size_t cpm_disk_capacity(const cpm_diskdef_t *def) {
    return (size_t)(def->dpb.dsm + 1) * cpm_block_size(&def->dpb);
}

/* ============================================================================
 * CP/M Directory Operations
 * ============================================================================ */

/* Directory entry structure (32 bytes) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  user;               /* User number (0-15, 0xE5 = deleted) */
    char     name[8];            /* Filename (uppercase, space-padded) */
    char     ext[3];             /* Extension (uppercase, space-padded) */
    uint8_t  extent_lo;          /* Extent low byte */
    uint8_t  s1;                 /* Reserved (S1) */
    uint8_t  s2;                 /* Reserved (S2) / extent high bits */
    uint8_t  record_count;       /* Record count in this extent */
    uint8_t  alloc[16];          /* Block allocation (8 or 16-bit entries) */
} cpm_dirent_t;
#pragma pack(pop)

/**
 * @brief CP/M file entry (parsed directory entry)
 */
typedef struct {
    uint8_t  user;
    char     filename[13];       /* "FILENAME.EXT" format */
    bool     read_only;          /* R/O attribute */
    bool     system;             /* SYS attribute */
    bool     archived;           /* ARC attribute */
    size_t   size;               /* Total file size */
    uint16_t extents;            /* Number of extents */
    uint16_t first_block;        /* First allocation block */
} cpm_file_t;

/**
 * @brief Read CP/M directory
 */
uft_error_t uft_cpm_read_directory(const uft_disk_image_t *disk,
                                   const cpm_diskdef_t *def,
                                   cpm_file_t *files,
                                   size_t max_files,
                                   size_t *file_count);

/**
 * @brief Extract file from CP/M disk
 */
uft_error_t uft_cpm_extract_file(const uft_disk_image_t *disk,
                                 const cpm_diskdef_t *def,
                                 const char *filename,
                                 uint8_t user,
                                 uint8_t **out_data,
                                 size_t *out_size);

/**
 * @brief Write file to CP/M disk
 */
uft_error_t uft_cpm_write_file(uft_disk_image_t *disk,
                               const cpm_diskdef_t *def,
                               const char *filename,
                               uint8_t user,
                               const uint8_t *data,
                               size_t size);

/**
 * @brief Delete file from CP/M disk
 */
uft_error_t uft_cpm_delete_file(uft_disk_image_t *disk,
                                const cpm_diskdef_t *def,
                                const char *filename,
                                uint8_t user);

/**
 * @brief Format disk with CP/M filesystem
 */
uft_error_t uft_cpm_format(uft_disk_image_t *disk,
                           const cpm_diskdef_t *def);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CPM_DISKDEF_H */
