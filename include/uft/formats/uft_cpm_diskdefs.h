/**
 * @file uft_cpm_diskdefs.h
 * @brief Comprehensive CP/M disk definitions
 * @version 3.9.0
 * 
 * Contains 50+ CP/M disk format definitions from various systems:
 * - Standard IBM 8" formats
 * - Amstrad CPC/PCW formats
 * - Kaypro formats
 * - Osborne formats
 * - Xerox 820 formats
 * - Various other CP/M machines
 * 
 * Reference: libdsk diskdefs, cpmtools diskdefs
 */

#ifndef UFT_CPM_DISKDEFS_H
#define UFT_CPM_DISKDEFS_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CP/M sector skew modes */
typedef enum {
    CPM_SKEW_NONE = 0,           /* No skewing */
    CPM_SKEW_LOGICAL,            /* Logical sector skew */
    CPM_SKEW_PHYSICAL,           /* Physical sector skew */
} cpm_skew_mode_t;

/* CP/M boot sector modes */
typedef enum {
    CPM_BOOT_NONE = 0,           /* No reserved tracks */
    CPM_BOOT_STANDARD,           /* Standard boot tracks */
    CPM_BOOT_SPECIAL,            /* Special boot format */
} cpm_boot_mode_t;

/**
 * @brief CP/M Disk Parameter Block (DPB)
 * Standard CP/M 2.2 / CP/M Plus DPB
 */
typedef struct {
    uint16_t spt;                /* Sectors per track (128-byte logical) */
    uint8_t  bsh;                /* Block shift (log2(block_size/128)) */
    uint8_t  blm;                /* Block mask (2^bsh - 1) */
    uint8_t  exm;                /* Extent mask */
    uint16_t dsm;                /* Max block number */
    uint16_t drm;                /* Max directory entry number */
    uint8_t  al0;                /* Allocation bitmap byte 0 */
    uint8_t  al1;                /* Allocation bitmap byte 1 */
    uint16_t cks;                /* Directory check vector size */
    uint16_t off;                /* Reserved tracks (system tracks) */
    uint8_t  psh;                /* Physical sector shift */
    uint8_t  phm;                /* Physical sector mask */
} cpm_dpb_t;

/**
 * @brief CP/M disk definition
 */
typedef struct {
    const char *name;            /* Format name */
    const char *description;     /* Human-readable description */
    
    /* Physical geometry */
    uint8_t  cylinders;
    uint8_t  heads;
    uint8_t  sectors;            /* Physical sectors per track */
    uint16_t sector_size;        /* Physical sector size */
    
    /* CP/M parameters */
    cpm_dpb_t dpb;               /* Disk Parameter Block */
    uint8_t  first_sector;       /* First sector number */
    cpm_skew_mode_t skew_mode;
    const uint8_t *skew_table;   /* Sector skew table (NULL if none) */
    cpm_boot_mode_t boot_mode;
    
    /* Additional flags */
    bool double_sided;
    bool high_density;
    bool is_mfm;                 /* MFM (true) or FM (false) */
    
} cpm_diskdef_t;

/* ============================================================================
 * Standard CP/M Disk Definitions
 * ============================================================================ */

/* IBM 8" SSSD (CP/M standard) */
extern const cpm_diskdef_t cpm_ibm_8ss;

/* IBM 8" DSDD */
extern const cpm_diskdef_t cpm_ibm_8ds;

/* Amstrad CPC Data format */
extern const cpm_diskdef_t cpm_amstrad_cpc_data;

/* Amstrad CPC System format */
extern const cpm_diskdef_t cpm_amstrad_cpc_system;

/* Amstrad PCW 3" CF2 format */
extern const cpm_diskdef_t cpm_amstrad_pcw_cf2;

/* Kaypro II (SSDD) */
extern const cpm_diskdef_t cpm_kaypro_ii;

/* Kaypro 4 (DSDD) */
extern const cpm_diskdef_t cpm_kaypro_4;

/* Osborne 1 (SSSD) */
extern const cpm_diskdef_t cpm_osborne_1;

/* Osborne Executive (SSDD) */
extern const cpm_diskdef_t cpm_osborne_exec;

/* Xerox 820 (SSSD) */
extern const cpm_diskdef_t cpm_xerox_820;

/* Morrow Micro Decision */
extern const cpm_diskdef_t cpm_morrow_md2;

/* Epson QX-10 */
extern const cpm_diskdef_t cpm_epson_qx10;

/* NEC PC-8801 */
extern const cpm_diskdef_t cpm_nec_pc8801;

/* Sharp MZ-80B */
extern const cpm_diskdef_t cpm_sharp_mz80b;

/* Zorba */
extern const cpm_diskdef_t cpm_zorba;

/* ============================================================================
 * CP/M Disk Definition Functions
 * ============================================================================ */

/**
 * @brief Get CP/M disk definition by name
 * @return Pointer to definition, or NULL if not found
 */
const cpm_diskdef_t* cpm_get_diskdef(const char *name);

/**
 * @brief Get CP/M disk definition by index
 */
const cpm_diskdef_t* cpm_get_diskdef_by_index(size_t index);

/**
 * @brief Get number of defined CP/M formats
 */
size_t cpm_get_diskdef_count(void);

/**
 * @brief List all CP/M disk definition names
 * @param names Array to fill with names
 * @param max_count Maximum entries to return
 * @return Actual number of entries
 */
size_t cpm_list_diskdefs(const char **names, size_t max_count);

/**
 * @brief Detect CP/M format from disk image
 * @return Matching diskdef, or NULL if no match
 */
const cpm_diskdef_t* cpm_detect_format(const uft_disk_image_t *disk);

/**
 * @brief Calculate logical sector from physical using skew table
 */
uint8_t cpm_physical_to_logical(const cpm_diskdef_t *def, uint8_t physical);

/**
 * @brief Calculate physical sector from logical using skew table
 */
uint8_t cpm_logical_to_physical(const cpm_diskdef_t *def, uint8_t logical);

/**
 * @brief Calculate block size from DPB
 */
size_t cpm_block_size(const cpm_dpb_t *dpb);

/**
 * @brief Calculate total disk capacity
 */
size_t cpm_disk_capacity(const cpm_diskdef_t *def);

/**
 * @brief Calculate directory size
 */
size_t cpm_directory_size(const cpm_diskdef_t *def);

/* ============================================================================
 * CP/M File I/O with definitions
 * ============================================================================ */

/**
 * @brief Read CP/M disk image with specific definition
 */
uft_error_t uft_cpm_read_with_def(const char *path,
                                  const cpm_diskdef_t *def,
                                  uft_disk_image_t **out_disk);

/**
 * @brief Write CP/M disk image with specific definition
 */
uft_error_t uft_cpm_write_with_def(const uft_disk_image_t *disk,
                                   const char *path,
                                   const cpm_diskdef_t *def);

/**
 * @brief Format new CP/M disk
 */
uft_error_t uft_cpm_format(uft_disk_image_t **out_disk,
                           const cpm_diskdef_t *def,
                           uint8_t directory_fill);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CPM_DISKDEFS_H */
