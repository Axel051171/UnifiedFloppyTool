/**
 * @file uft_xdf_dxdf.h
 * @brief DXDF - C64 D64/G64 eXtended Disk Format
 * 
 * Forensic container for Commodore 64/128 disk images.
 * Supports D64, G64, NIB and raw GCR formats.
 * 
 * C64 Specifics:
 * - GCR encoding (5-bit to 4-bit)
 * - 4 density zones (21/19/18/17 sectors)
 * - Half-track support
 * - 300 RPM drive speed
 * 
 * Protection Types:
 * - V-MAX!, RapidLok, Vorpal
 * - Fat tracks, half-tracks
 * - Density manipulation
 * - Sync patterns
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_XDF_DXDF_H
#define UFT_XDF_DXDF_H

#include "uft_xdf_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * C64 Constants
 *===========================================================================*/

#define DXDF_TRACKS_STANDARD    35      /**< Standard 1541 tracks */
#define DXDF_TRACKS_EXTENDED    40      /**< Extended (40 tracks) */
#define DXDF_TRACKS_MAX         42      /**< Maximum with half-tracks: 84 */
#define DXDF_SIDES              1       /**< Single-sided */

/** Sectors per zone */
#define DXDF_ZONE1_SECTORS      21      /**< Tracks 1-17 */
#define DXDF_ZONE2_SECTORS      19      /**< Tracks 18-24 */
#define DXDF_ZONE3_SECTORS      18      /**< Tracks 25-30 */
#define DXDF_ZONE4_SECTORS      17      /**< Tracks 31-35+ */

/** Track lengths by zone (GCR bytes) */
#define DXDF_ZONE1_LENGTH       7692
#define DXDF_ZONE2_LENGTH       7142
#define DXDF_ZONE3_LENGTH       6666
#define DXDF_ZONE4_LENGTH       6250

/** GCR sync byte */
#define DXDF_SYNC_BYTE          0xFF

/** Sector sizes */
#define DXDF_SECTOR_SIZE        256     /**< Decoded sector */
#define DXDF_GCR_SECTOR_SIZE    325     /**< GCR-encoded sector */

/*===========================================================================
 * C64 Disk Types
 *===========================================================================*/

typedef enum {
    DXDF_TYPE_UNKNOWN = 0,
    DXDF_TYPE_D64,              /**< Standard D64 (683 sectors) */
    DXDF_TYPE_D64_40,           /**< Extended D64 (768 sectors) */
    DXDF_TYPE_D64_ERRORS,       /**< D64 with error table */
    DXDF_TYPE_G64,              /**< GCR image */
    DXDF_TYPE_NIB,              /**< Nibtools raw */
    DXDF_TYPE_NBZ,              /**< Compressed NIB */
} dxdf_type_t;

/*===========================================================================
 * C64 Protection Types
 *===========================================================================*/

typedef enum {
    DXDF_PROT_NONE = 0,
    DXDF_PROT_VMAX,             /**< V-MAX! */
    DXDF_PROT_VMAX2,            /**< V-MAX! V2 */
    DXDF_PROT_VMAX3,            /**< V-MAX! V3 */
    DXDF_PROT_RAPIDLOK,         /**< RapidLok */
    DXDF_PROT_VORPAL,           /**< Vorpal */
    DXDF_PROT_GMA,              /**< GMA format */
    DXDF_PROT_PIRATESLAYER,     /**< Pirate Slayer */
    DXDF_PROT_FAT_TRACK,        /**< Fat track (extra data) */
    DXDF_PROT_HALF_TRACK,       /**< Half-track data */
    DXDF_PROT_SYNC_LENGTH,      /**< Non-standard sync */
    DXDF_PROT_DENSITY_MISMATCH, /**< Wrong density zone */
    DXDF_PROT_TIMING,           /**< Timing-based */
    DXDF_PROT_CUSTOM,           /**< Unknown protection */
} dxdf_prot_type_t;

/*===========================================================================
 * C64 Specific Structures
 *===========================================================================*/

/**
 * @brief C64 sector header (from disk)
 */
typedef struct __attribute__((packed)) {
    uint8_t block_id;           /**< 0x08 for header */
    uint8_t checksum;           /**< Header XOR checksum */
    uint8_t sector;             /**< Sector number */
    uint8_t track;              /**< Track number */
    uint8_t disk_id[2];         /**< Disk ID */
    uint8_t padding[2];         /**< Usually 0x0F */
} dxdf_sector_header_t;

/**
 * @brief C64 track analysis
 */
typedef struct {
    int track;                  /**< Track number (1-42) */
    int half_track;             /**< Half track flag */
    
    /* GCR data */
    uint8_t *gcr_data;          /**< Raw GCR bytes */
    size_t gcr_size;            /**< GCR data size */
    
    /* Decoded */
    uint8_t sectors[21][256];   /**< Decoded sector data */
    uint8_t sector_status[21];  /**< Per-sector status */
    
    /* Density */
    int expected_zone;          /**< Expected density zone */
    int actual_zone;            /**< Detected zone */
    bool density_mismatch;      /**< Zone doesn't match position */
    
    /* Sync analysis */
    int sync_count;             /**< Number of syncs */
    uint32_t *sync_offsets;     /**< Sync positions */
    int *sync_lengths;          /**< Sync lengths */
    
    /* Quality */
    xdf_confidence_t confidence;
    int bad_gcr_count;          /**< Invalid GCR nibbles */
    bool has_weak_bits;
    bool has_protection;
} dxdf_track_analysis_t;

/**
 * @brief DXDF header extension
 */
typedef struct __attribute__((packed)) {
    /* C64-specific info */
    uint8_t disk_id[2];         /**< Disk ID from BAM */
    uint8_t dos_type[2];        /**< DOS type (2A, etc.) */
    char disk_name[16];         /**< Disk name from BAM */
    
    /* BAM info */
    uint8_t bam_track;          /**< BAM track (usually 18) */
    uint8_t bam_sector;         /**< BAM sector (usually 0) */
    uint16_t free_blocks;       /**< Free blocks */
    
    /* Track info */
    uint8_t num_tracks;         /**< 35 or 40 */
    uint8_t has_half_tracks;    /**< Half-track data present */
    uint8_t has_errors;         /**< Error table present */
    uint8_t reserved1;
    
    /* Protection */
    uint32_t protection_type;   /**< dxdf_prot_type_t flags */
    uint8_t protection_track;   /**< Primary protection track */
    char protection_name[32];   /**< Protection identifier */
    
    /* Quality */
    uint8_t track_density[42];  /**< Density per track */
    uint8_t track_status[42];   /**< Status per track */
    
    uint8_t reserved2[32];
} dxdf_extension_t;

/*===========================================================================
 * DXDF API
 *===========================================================================*/

/** Create DXDF context */
xdf_context_t* dxdf_create(void);

/** Import D64 */
int dxdf_import_d64(xdf_context_t *ctx, const char *path);

/** Import G64 */
int dxdf_import_g64(xdf_context_t *ctx, const char *path);

/** Import NIB */
int dxdf_import_nib(xdf_context_t *ctx, const char *path);

/** Export to D64 */
int dxdf_export_d64(xdf_context_t *ctx, const char *path);

/** Export to G64 */
int dxdf_export_g64(xdf_context_t *ctx, const char *path);

/** Get density zone for track */
int dxdf_get_zone(int track);

/** Get sectors for zone */
int dxdf_sectors_for_zone(int zone);

/** Get track length for zone */
int dxdf_track_length(int zone);

/** Analyze track GCR */
int dxdf_analyze_track(xdf_context_t *ctx, int track, 
                        dxdf_track_analysis_t *result);

/** Decode GCR to bytes */
int dxdf_decode_gcr(const uint8_t *gcr, size_t gcr_len,
                     uint8_t *output, size_t *out_len);

/** Encode bytes to GCR */
int dxdf_encode_gcr(const uint8_t *data, size_t data_len,
                     uint8_t *gcr, size_t *gcr_len);

/** Detect protection */
int dxdf_detect_protection(xdf_context_t *ctx, dxdf_prot_type_t *type,
                            xdf_confidence_t *confidence);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XDF_DXDF_H */
