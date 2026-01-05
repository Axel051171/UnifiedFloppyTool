/**
 * @file uft_disk.h
 * @brief Unified Disk Structure (P2-ARCH-005)
 * 
 * Central disk image structure for all UFT subsystems.
 * Ties together: track_base, sector, encoding, CRC.
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_DISK_H
#define UFT_DISK_H

#include "uft_track_base.h"
#include "uft_sector.h"
#include "uft_encoding.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_DISK_MAX_TRACKS         168     /**< Max tracks (84 cyl × 2 heads) */
#define UFT_DISK_MAX_SIDES          2
#define UFT_DISK_MAX_METADATA       16      /**< Max metadata entries */

/*===========================================================================
 * Disk Type Enumeration
 *===========================================================================*/

/**
 * @brief Disk physical type
 */
typedef enum uft_disk_type {
    UFT_DTYPE_UNKNOWN       = 0,
    
    /* 5.25" */
    UFT_DTYPE_525_SS_SD     = 1,    /**< 5.25" Single-Sided Single-Density */
    UFT_DTYPE_525_SS_DD     = 2,    /**< 5.25" Single-Sided Double-Density */
    UFT_DTYPE_525_DS_DD     = 3,    /**< 5.25" Double-Sided Double-Density */
    UFT_DTYPE_525_DS_HD     = 4,    /**< 5.25" Double-Sided High-Density */
    UFT_DTYPE_525_DS_QD     = 5,    /**< 5.25" Double-Sided Quad-Density */
    
    /* 3.5" */
    UFT_DTYPE_35_SS_DD      = 10,   /**< 3.5" Single-Sided Double-Density */
    UFT_DTYPE_35_DS_DD      = 11,   /**< 3.5" Double-Sided Double-Density (720KB) */
    UFT_DTYPE_35_DS_HD      = 12,   /**< 3.5" Double-Sided High-Density (1.44MB) */
    UFT_DTYPE_35_DS_ED      = 13,   /**< 3.5" Double-Sided Extra-Density (2.88MB) */
    
    /* 8" */
    UFT_DTYPE_8_SS_SD       = 20,   /**< 8" Single-Sided Single-Density */
    UFT_DTYPE_8_DS_SD       = 21,   /**< 8" Double-Sided Single-Density */
    UFT_DTYPE_8_DS_DD       = 22,   /**< 8" Double-Sided Double-Density */
    
    /* Special */
    UFT_DTYPE_HARD_SECTOR   = 30,   /**< Hard-sectored disk */
    UFT_DTYPE_CUSTOM        = 99    /**< Custom/non-standard */
} uft_disk_type_t;

/*===========================================================================
 * Disk Flags
 *===========================================================================*/

/**
 * @brief Disk status flags
 */
typedef enum uft_disk_flags {
    UFT_DF_NONE             = 0,
    UFT_DF_READ_ONLY        = (1 << 0),  /**< Disk is read-only */
    UFT_DF_MODIFIED         = (1 << 1),  /**< Disk has been modified */
    UFT_DF_PROTECTED        = (1 << 2),  /**< Copy protection detected */
    UFT_DF_BAD_SECTORS      = (1 << 3),  /**< Has unreadable sectors */
    UFT_DF_FLUX_SOURCE      = (1 << 4),  /**< From flux capture */
    UFT_DF_SECTOR_IMAGE     = (1 << 5),  /**< From sector image */
    UFT_DF_HALF_TRACKS      = (1 << 6),  /**< Contains half-tracks */
    UFT_DF_VARIABLE_DENSITY = (1 << 7),  /**< Variable density zones */
    UFT_DF_MULTI_REV        = (1 << 8),  /**< Multi-revolution data */
    UFT_DF_VERIFIED         = (1 << 9),  /**< Verified against source */
    UFT_DF_FORENSIC         = (1 << 10)  /**< Forensic-mode capture */
} uft_disk_flags_t;

/*===========================================================================
 * Metadata
 *===========================================================================*/

/**
 * @brief Disk metadata entry
 */
typedef struct uft_disk_meta {
    char    key[32];
    char    value[256];
} uft_disk_meta_t;

/*===========================================================================
 * Disk Geometry
 *===========================================================================*/

/**
 * @brief Disk geometry
 */
typedef struct uft_disk_geometry {
    uint8_t  cylinders;         /**< Number of cylinders */
    uint8_t  heads;             /**< Number of heads (1 or 2) */
    uint8_t  sectors;           /**< Sectors per track (if uniform) */
    uint16_t sector_size;       /**< Sector size (if uniform) */
    
    uint8_t  step_rate;         /**< Step rate (if known) */
    uint16_t rpm;               /**< Nominal RPM (300 or 360) */
    
    bool     variable_sectors;  /**< Sectors vary by track */
    bool     variable_density;  /**< Density varies */
} uft_disk_geometry_t;

/*===========================================================================
 * Unified Disk Structure
 *===========================================================================*/

/**
 * @brief Unified disk structure
 */
typedef struct uft_disk_unified {
    /* ═══ Identity ═══ */
    char    name[64];           /**< Disk name/label */
    char    source_path[256];   /**< Original file path */
    char    format_name[32];    /**< Format name (ADF, D64, etc.) */
    
    /* ═══ Type & Status ═══ */
    uft_disk_type_t type;       /**< Physical disk type */
    uint16_t flags;             /**< uft_disk_flags_t */
    uft_disk_encoding_t encoding; /**< Primary encoding */
    
    /* ═══ Geometry ═══ */
    uft_disk_geometry_t geometry;
    
    /* ═══ Tracks ═══ */
    uint8_t track_count;        /**< Number of tracks loaded */
    uft_track_base_t *tracks[UFT_DISK_MAX_TRACKS];
    
    /* ═══ Raw Data (optional) ═══ */
    uint8_t *raw_data;          /**< Raw image data (if sector image) */
    size_t   raw_size;          /**< Raw data size */
    
    /* ═══ Quality Metrics ═══ */
    uint32_t total_sectors;     /**< Total sectors on disk */
    uint32_t good_sectors;      /**< Sectors with valid CRC */
    uint32_t bad_sectors;       /**< Sectors with CRC errors */
    uint32_t missing_sectors;   /**< Expected but not found */
    float    overall_quality;   /**< Overall quality (0.0-1.0) */
    
    /* ═══ Protection Info ═══ */
    uint32_t protection_type;   /**< Detected protection scheme */
    char     protection_name[32]; /**< Protection scheme name */
    
    /* ═══ Metadata ═══ */
    uft_disk_meta_t metadata[UFT_DISK_MAX_METADATA];
    uint8_t  metadata_count;
    
    /* ═══ Timestamps ═══ */
    uint64_t created_time;      /**< Creation timestamp (Unix) */
    uint64_t modified_time;     /**< Last modification (Unix) */
    
    /* ═══ User Data ═══ */
    void    *user_data;         /**< Format-specific extension */
    
} uft_disk_unified_t;

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

/* Lifecycle */
uft_disk_unified_t* uft_disk_create(void);
void uft_disk_free(uft_disk_unified_t *disk);
uft_disk_unified_t* uft_disk_clone(const uft_disk_unified_t *src);

/* Track Management */
int uft_disk_add_track(uft_disk_unified_t *disk, uft_track_base_t *track);
uft_track_base_t* uft_disk_get_track(uft_disk_unified_t *disk, 
                                      uint8_t cyl, uint8_t head);
int uft_disk_remove_track(uft_disk_unified_t *disk, uint8_t cyl, uint8_t head);

/* Sector Access */
uft_sector_unified_t* uft_disk_get_sector(uft_disk_unified_t *disk,
                                           uint8_t cyl, uint8_t head, 
                                           uint8_t sector);
int uft_disk_read_sector(uft_disk_unified_t *disk,
                         uint8_t cyl, uint8_t head, uint8_t sector,
                         uint8_t *buffer, size_t size);

/* Metadata */
int uft_disk_set_meta(uft_disk_unified_t *disk, 
                      const char *key, const char *value);
const char* uft_disk_get_meta(uft_disk_unified_t *disk, const char *key);

/* Statistics */
void uft_disk_update_stats(uft_disk_unified_t *disk);
float uft_disk_calc_quality(const uft_disk_unified_t *disk);

/* Info */
int uft_disk_get_info(const uft_disk_unified_t *disk, 
                      char *buffer, size_t size);
const char* uft_disk_type_name(uft_disk_type_t type);
const char* uft_disk_flags_str(uint16_t flags);

/* I/O Helpers */
int uft_disk_alloc_raw(uft_disk_unified_t *disk, size_t size);
int uft_disk_set_geometry(uft_disk_unified_t *disk,
                          uint8_t cyls, uint8_t heads, 
                          uint8_t spt, uint16_t sector_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_H */
