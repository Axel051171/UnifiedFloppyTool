/**
 * @file uft_cpm_defs.h
 * @brief CP/M disk format definitions
 * @version 3.9.0
 * 
 * Standard CP/M disk format definitions derived from libdsk diskdefs.
 * Supports over 50 different CP/M disk formats including:
 * - Standard 8" and 5.25" formats
 * - IBM PC CP/M-86 formats
 * - Amstrad CP/M formats
 * - Epson, Kaypro, Osborne, etc.
 * 
 * Reference: libdsk diskdefs file
 */

#ifndef UFT_CPM_DEFS_H
#define UFT_CPM_DEFS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum values */
#define CPM_MAX_NAME_LEN        32
#define CPM_MAX_FORMATS         100

/* Block size options */
#define CPM_BSH_1K              3    /* 1024 byte blocks */
#define CPM_BSH_2K              4    /* 2048 byte blocks */
#define CPM_BSH_4K              5    /* 4096 byte blocks */
#define CPM_BSH_8K              6    /* 8192 byte blocks */
#define CPM_BSH_16K             7    /* 16384 byte blocks */

/* Directory entry size */
#define CPM_DIR_ENTRY_SIZE      32

/* Sector skew types */
typedef enum {
    CPM_SKEW_NONE = 0,
    CPM_SKEW_SEQUENTIAL,
    CPM_SKEW_2_1,
    CPM_SKEW_3_1,
    CPM_SKEW_6_1,
    CPM_SKEW_CUSTOM,
} cpm_skew_type_t;

/**
 * @brief CP/M Disk Parameter Block (DPB)
 * 
 * Standard CP/M 2.2/3.0 disk parameter block.
 */
typedef struct {
    uint16_t spt;       /* Sectors per track (logical 128-byte sectors) */
    uint8_t  bsh;       /* Block shift (log2(block_size/128)) */
    uint8_t  blm;       /* Block mask (block_size/128 - 1) */
    uint8_t  exm;       /* Extent mask */
    uint16_t dsm;       /* Total blocks - 1 */
    uint16_t drm;       /* Directory entries - 1 */
    uint8_t  al0;       /* Directory allocation bitmap byte 0 */
    uint8_t  al1;       /* Directory allocation bitmap byte 1 */
    uint16_t cks;       /* Checksum vector size */
    uint16_t off;       /* Reserved tracks */
    uint8_t  psh;       /* Physical sector shift (CP/M 3.0) */
    uint8_t  phm;       /* Physical sector mask (CP/M 3.0) */
} cpm_dpb_t;

/**
 * @brief CP/M disk format definition
 */
typedef struct {
    char     name[CPM_MAX_NAME_LEN];  /* Format name */
    char     description[64];         /* Human-readable description */
    
    /* Physical geometry */
    uint8_t  cylinders;
    uint8_t  heads;
    uint8_t  sectors;                 /* Physical sectors per track */
    uint16_t sector_size;             /* Physical sector size */
    uint8_t  first_sector;            /* First sector number (0 or 1) */
    
    /* Encoding */
    bool     mfm;                     /* true=MFM, false=FM */
    bool     double_step;             /* true for 40T in 80T drive */
    
    /* CP/M DPB */
    cpm_dpb_t dpb;
    
    /* Sector skew */
    cpm_skew_type_t skew_type;
    const uint8_t *skew_table;        /* Custom skew table (NULL if not used) */
    
    /* Boot tracks */
    uint8_t  boot_tracks;             /* System tracks */
    
} cpm_format_def_t;

/* ============================================================================
 * Standard CP/M Format Definitions
 * ============================================================================ */

/* 8" Standard Formats */
extern const cpm_format_def_t cpm_ibm_8_sssd;    /* IBM 8" SS SD */
extern const cpm_format_def_t cpm_ibm_8_ssdd;    /* IBM 8" SS DD */
extern const cpm_format_def_t cpm_ibm_8_dsdd;    /* IBM 8" DS DD */

/* 5.25" DD Formats */
extern const cpm_format_def_t cpm_ibm_525_ssdd;  /* IBM 5.25" SS DD */
extern const cpm_format_def_t cpm_ibm_525_dsdd;  /* IBM 5.25" DS DD */
extern const cpm_format_def_t cpm_ibm_525_dsqd;  /* IBM 5.25" DS QD (96tpi) */

/* 3.5" Formats */
extern const cpm_format_def_t cpm_ibm_35_dsdd;   /* IBM 3.5" DS DD */
extern const cpm_format_def_t cpm_ibm_35_dshd;   /* IBM 3.5" DS HD */

/* Amstrad Formats */
extern const cpm_format_def_t cpm_amstrad_pcw;   /* Amstrad PCW */
extern const cpm_format_def_t cpm_amstrad_cpc;   /* Amstrad CPC System */
extern const cpm_format_def_t cpm_amstrad_data;  /* Amstrad CPC Data */

/* Kaypro Formats */
extern const cpm_format_def_t cpm_kaypro_ii;     /* Kaypro II */
extern const cpm_format_def_t cpm_kaypro_4;      /* Kaypro 4 */
extern const cpm_format_def_t cpm_kaypro_10;     /* Kaypro 10 */

/* Osborne Formats */
extern const cpm_format_def_t cpm_osborne_1;     /* Osborne 1 */
extern const cpm_format_def_t cpm_osborne_dd;    /* Osborne DD */

/* Epson Formats */
extern const cpm_format_def_t cpm_epson_qx10;    /* Epson QX-10 */
extern const cpm_format_def_t cpm_epson_px8;     /* Epson PX-8 */

/* Morrow Formats */
extern const cpm_format_def_t cpm_morrow_md2;    /* Morrow MD2 */
extern const cpm_format_def_t cpm_morrow_md3;    /* Morrow MD3 */

/* Bondwell Formats */
extern const cpm_format_def_t cpm_bondwell;      /* Bondwell */

/* Sanyo Formats */
extern const cpm_format_def_t cpm_sanyo_mbc55x;  /* Sanyo MBC-55x */

/* NEC Formats */
extern const cpm_format_def_t cpm_nec_pc8801;    /* NEC PC-8801 */

/* Superbrain Formats */
extern const cpm_format_def_t cpm_superbrain;    /* Intertec Superbrain */
extern const cpm_format_def_t cpm_superbrain_dd; /* Superbrain DD */

/* Televideo Formats */
extern const cpm_format_def_t cpm_televideo_803; /* Televideo 803 */

/* Zorba Formats */
extern const cpm_format_def_t cpm_zorba;         /* Telcon Zorba */

/* ============================================================================
 * Format Registry Functions
 * ============================================================================ */

/**
 * @brief Get all CP/M format definitions
 */
const cpm_format_def_t** uft_cpm_get_all_formats(size_t *count);

/**
 * @brief Find CP/M format by name
 */
const cpm_format_def_t* uft_cpm_find_format(const char *name);

/**
 * @brief Find CP/M format by geometry
 */
const cpm_format_def_t* uft_cpm_find_by_geometry(
    uint8_t cyls, uint8_t heads, uint8_t spt, uint16_t secsize);

/**
 * @brief Calculate block size from DPB
 */
static inline uint16_t cpm_block_size(const cpm_dpb_t *dpb) {
    return 128 << dpb->bsh;
}

/**
 * @brief Calculate total disk capacity
 */
static inline uint32_t cpm_disk_capacity(const cpm_format_def_t *fmt) {
    return (uint32_t)fmt->cylinders * fmt->heads * fmt->sectors * fmt->sector_size;
}

/**
 * @brief Calculate directory size in bytes
 */
static inline uint16_t cpm_directory_size(const cpm_dpb_t *dpb) {
    return (dpb->drm + 1) * CPM_DIR_ENTRY_SIZE;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_CPM_DEFS_H */
