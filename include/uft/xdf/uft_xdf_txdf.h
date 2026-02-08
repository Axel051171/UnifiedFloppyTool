/**
 * @file uft_xdf_txdf.h
 * @brief TXDF - Atari ST/STE/TT eXtended Disk Format
 * 
 * Forensic container for Atari ST disk images.
 * Supports ST, MSA, STX, Pasti formats.
 * 
 * Atari ST Specifics:
 * - MFM encoding (same as PC)
 * - 3.5" drives (DD and HD)
 * - WD1772 FDC quirks
 * 
 * Formats:
 * - ST: Raw sector dump
 * - MSA: Compressed ST
 * - STX: Pasti (protection-aware)
 * 
 * Protection Types:
 * - Rob Northen Copylock
 * - Macrodos
 * - Fuzzy sectors
 * - Long tracks
 * - Flaschel (FDC bug)
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_XDF_TXDF_H
#define UFT_XDF_TXDF_H

#include "uft_xdf_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Atari ST Constants
 *===========================================================================*/

#define TXDF_TRACKS_DD          80      /**< DD tracks */
#define TXDF_TRACKS_HD          80      /**< HD tracks */
#define TXDF_SECTORS_DD         9       /**< DD sectors/track */
#define TXDF_SECTORS_HD         18      /**< HD sectors/track */
#define TXDF_SECTOR_SIZE        512     /**< Sector size */

/** Standard sizes */
#define TXDF_SIZE_SS_DD         (80 * 9 * 512)      /**< 360KB */
#define TXDF_SIZE_DS_DD         (80 * 2 * 9 * 512)  /**< 720KB */
#define TXDF_SIZE_DS_HD         (80 * 2 * 18 * 512) /**< 1.44MB */

/** Long track threshold */
#define TXDF_LONG_TRACK_MIN     6500    /**< Bytes for long track */

/*===========================================================================
 * Atari ST Format Types
 *===========================================================================*/

typedef enum {
    TXDF_FORMAT_UNKNOWN = 0,
    TXDF_FORMAT_ST,             /**< Raw ST image */
    TXDF_FORMAT_MSA,            /**< MSA compressed */
    TXDF_FORMAT_STX,            /**< Pasti STX */
    TXDF_FORMAT_DIM,            /**< DIM format */
} txdf_format_t;

/*===========================================================================
 * Atari ST Protection Types
 *===========================================================================*/

typedef enum {
    TXDF_PROT_NONE = 0,
    TXDF_PROT_COPYLOCK,         /**< Rob Northen Copylock */
    TXDF_PROT_COPYLOCK_OLD,     /**< Copylock 1988 */
    TXDF_PROT_COPYLOCK_NEW,     /**< Copylock 1989+ */
    TXDF_PROT_MACRODOS,         /**< Macrodos */
    TXDF_PROT_FUZZY_SECTOR,     /**< Fuzzy/weak sectors */
    TXDF_PROT_LONG_TRACK,       /**< Extended track */
    TXDF_PROT_FLASCHEL,         /**< FDC bug exploit */
    TXDF_PROT_NO_FLUX,          /**< No-flux area */
    TXDF_PROT_SECTOR_GAP,       /**< Modified gaps */
    TXDF_PROT_HIDDEN_DATA,      /**< Inter-sector data */
    TXDF_PROT_ANTIBITOS,        /**< Illegal Anti-bitos */
    TXDF_PROT_TOXIC,            /**< NTM/Cameo Toxic */
} txdf_prot_type_t;

/*===========================================================================
 * MSA Header
 *===========================================================================*/

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;             /**< 0x0E0F */
    uint16_t sectors_per_track;
    uint16_t sides;             /**< 0 = single, 1 = double */
    uint16_t start_track;
    uint16_t end_track;
} txdf_msa_header_t;
#pragma pack(pop)

#define TXDF_MSA_MAGIC          0x0F0E  /**< MSA magic (little-endian) */

/*===========================================================================
 * STX/Pasti Structures
 *===========================================================================*/

#pragma pack(push, 1)
typedef struct {
    char magic[4];              /**< "RSY\0" */
    uint16_t version;           /**< Format version */
    uint16_t tool_version;      /**< Tool that created it */
    uint16_t reserved1;
    uint8_t track_count;        /**< Number of track records */
    uint8_t revision;           /**< Revision number */
    uint32_t reserved2;
} txdf_stx_header_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint32_t block_size;        /**< Size of this record */
    uint32_t fuzzy_size;        /**< Fuzzy mask size */
    uint16_t sector_count;      /**< Sectors in track */
    uint16_t flags;             /**< Track flags */
    uint16_t track_length;      /**< MFM track length */
    uint8_t track_number;       /**< Track number */
    uint8_t track_type;         /**< Track type */
} txdf_stx_track_t;
#pragma pack(pop)

/*===========================================================================
 * TXDF Header Extension
 *===========================================================================*/

#pragma pack(push, 1)
typedef struct {
    /* Format info */
    txdf_format_t format;
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors_per_track;
    
    /* Protection */
    uint32_t protection_flags;  /**< txdf_prot_type_t bitmask */
    uint8_t protection_track;
    char protection_name[32];
    
    /* Copylock specific */
    uint32_t copylock_key;      /**< Copylock key value */
    uint8_t copylock_track;
    uint8_t copylock_sector;
    
    /* Fuzzy sectors */
    uint8_t fuzzy_tracks[10];   /**< Tracks with fuzzy sectors */
    uint8_t fuzzy_count;
    
    /* Long tracks */
    uint8_t long_tracks[10];    /**< Extended tracks */
    uint8_t long_count;
    
    /* Per-track info */
    uint8_t track_status[160];
    uint16_t track_length[160]; /**< Actual track lengths */
    
    uint8_t reserved[64];
} txdf_extension_t;
#pragma pack(pop)

/*===========================================================================
 * TXDF API
 *===========================================================================*/

/** Create TXDF context */
xdf_context_t* txdf_create(void);

/** Import ST */
int txdf_import_st(xdf_context_t *ctx, const char *path);

/** Import MSA */
int txdf_import_msa(xdf_context_t *ctx, const char *path);

/** Import STX (Pasti) */
int txdf_import_stx(xdf_context_t *ctx, const char *path);

/** Export to ST */
int txdf_export_st(xdf_context_t *ctx, const char *path);

/** Export to MSA */
int txdf_export_msa(xdf_context_t *ctx, const char *path);

/** Detect Copylock */
int txdf_detect_copylock(xdf_context_t *ctx, uint32_t *key);

/** Check for fuzzy sectors */
int txdf_find_fuzzy_sectors(xdf_context_t *ctx, uint8_t *tracks, size_t max);

/** Check for long tracks */
int txdf_find_long_tracks(xdf_context_t *ctx, uint8_t *tracks, size_t max);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XDF_TXDF_H */
