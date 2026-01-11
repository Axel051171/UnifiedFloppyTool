/**
 * @file uft_xdf_mxdf.h
 * @brief MXDF - Multi-Format eXtended Disk Bundle
 * 
 * Meta-container for bundling multiple disk images across platforms.
 * Use cases:
 * - Multi-disk games (Disk 1, 2, 3...)
 * - Cross-platform releases (Amiga + Atari ST versions)
 * - Complete software collections
 * - Preservation projects
 * 
 * Features:
 * - Contains any combination of AXDF/DXDF/PXDF/TXDF/ZXDF
 * - Shared metadata and relationships
 * - Bundle-level validation
 * - Collective repair tracking
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_XDF_MXDF_H
#define UFT_XDF_MXDF_H

#include "uft_xdf_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * MXDF Constants
 *===========================================================================*/

#define MXDF_MAX_DISKS          64      /**< Maximum disks in bundle */
#define MXDF_MAX_RELATIONS      256     /**< Maximum relationships */

/*===========================================================================
 * Disk Entry
 *===========================================================================*/

#pragma pack(push, 1)
typedef struct {
    /* Identity */
    uint32_t disk_id;           /**< Unique ID within bundle */
    char name[64];              /**< Disk name */
    char label[32];             /**< Volume label */
    
    /* Type */
    uint8_t platform;           /**< xdf_platform_t */
    uint8_t format_type;        /**< Platform-specific format */
    uint8_t disk_number;        /**< Disk N of M */
    uint8_t total_disks;        /**< Total disks in set */
    
    /* Location */
    uint32_t data_offset;       /**< Offset in bundle */
    uint32_t data_size;         /**< Size of XDF data */
    uint32_t header_offset;     /**< Offset to XDF header */
    
    /* Quality */
    xdf_confidence_t confidence;/**< Disk confidence */
    uint8_t status;             /**< xdf_status_t */
    uint8_t flags;
    
    /* Relationships */
    uint32_t related_to;        /**< Related disk ID (0 = none) */
    uint8_t relation_type;      /**< Relation type */
    uint8_t reserved[7];
    
    /* Checksums */
    uint32_t crc32;             /**< CRC32 of disk data */
    uint8_t sha256[32];         /**< SHA-256 (optional) */
} mxdf_disk_entry_t;
#pragma pack(pop)

/*===========================================================================
 * Relationship Types
 *===========================================================================*/

typedef enum {
    MXDF_REL_NONE = 0,
    MXDF_REL_NEXT_DISK,         /**< Next disk in sequence */
    MXDF_REL_PREV_DISK,         /**< Previous disk in sequence */
    MXDF_REL_ALTERNATE,         /**< Alternate version (same content) */
    MXDF_REL_PLATFORM_PORT,     /**< Same game, different platform */
    MXDF_REL_UPDATE,            /**< Updated/patched version */
    MXDF_REL_ORIGINAL,          /**< Original of a copy */
    MXDF_REL_SAVE_DISK,         /**< Save/data disk */
    MXDF_REL_BOOT_DISK,         /**< Boot disk for this set */
} mxdf_relation_t;

/*===========================================================================
 * Bundle Metadata
 *===========================================================================*/

#pragma pack(push, 1)
typedef struct {
    /* Title info */
    char title[128];            /**< Game/software title */
    char publisher[64];         /**< Publisher name */
    char developer[64];         /**< Developer name */
    char release_year[8];       /**< Release year */
    char region[16];            /**< Region (PAL/NTSC/World) */
    char language[16];          /**< Language(s) */
    
    /* Classification */
    char genre[32];             /**< Game genre */
    char category[32];          /**< Software category */
    uint32_t tags;              /**< Tag bitmask */
    
    /* Preservation info */
    char dumper[64];            /**< Who dumped it */
    char dump_date[24];         /**< When dumped */
    char verified_by[64];       /**< Verified by */
    char verify_date[24];       /**< Verification date */
    
    /* External references */
    char whdload_slave[64];     /**< WHDLoad slave name */
    char tosec_name[128];       /**< TOSEC name */
    char caps_id[32];           /**< SPS/CAPS ID */
    char mobygames_id[16];      /**< MobyGames ID */
    
    /* Notes */
    char notes[512];            /**< Free-form notes */
} mxdf_metadata_t;
#pragma pack(pop)

/*===========================================================================
 * MXDF Header
 *===========================================================================*/

#pragma pack(push, 1)
typedef struct {
    /* Magic & Version (16 bytes) */
    char magic[4];              /**< "MXDF" */
    uint8_t version_major;
    uint8_t version_minor;
    uint16_t header_size;
    uint32_t file_size;
    uint32_t file_crc32;
    
    /* Bundle info (16 bytes) */
    uint16_t disk_count;        /**< Number of disks */
    uint16_t platform_mask;     /**< Platforms present (bitmask) */
    uint32_t total_size;        /**< Total uncompressed size */
    uint8_t compression;        /**< 0=none, 1=zlib, 2=lz4 */
    uint8_t flags;
    uint8_t reserved1[6];
    
    /* Quality (16 bytes) */
    xdf_confidence_t overall_confidence;
    uint16_t good_disks;
    uint16_t weak_disks;
    uint16_t bad_disks;
    uint16_t repaired_disks;
    uint16_t protected_disks;
    uint8_t reserved2[4];
    
    /* Table offsets (32 bytes) */
    uint32_t disk_table_offset;
    uint32_t metadata_offset;
    uint32_t relation_offset;
    uint32_t relation_count;
    uint32_t data_offset;
    uint32_t data_size;
    uint8_t reserved3[8];
    
    /* Metadata preview (inline, 512 bytes) */
    mxdf_metadata_t metadata;
    
    /* Padding to 1024 bytes */
    uint8_t padding[432];
} mxdf_header_t;
#pragma pack(pop)

/*===========================================================================
 * Relationship Entry
 *===========================================================================*/

#pragma pack(push, 1)
typedef struct {
    uint32_t source_id;         /**< Source disk ID */
    uint32_t target_id;         /**< Target disk ID */
    uint8_t relation;           /**< mxdf_relation_t */
    uint8_t bidirectional;      /**< Applies both ways? */
    uint8_t reserved[6];
    char description[48];       /**< Relationship description */
} mxdf_relation_entry_t;
#pragma pack(pop)

/*===========================================================================
 * MXDF Context
 *===========================================================================*/

typedef struct mxdf_context_s mxdf_context_t;

typedef struct {
    /* What to include */
    bool include_flux;          /**< Include flux data */
    bool include_zones;         /**< Include zone maps */
    bool include_repairs;       /**< Include repair logs */
    
    /* Compression */
    int compression_level;      /**< 0-9 (0=none) */
    
    /* Validation */
    bool validate_on_add;       /**< Validate when adding disk */
    bool require_checksums;     /**< Require SHA-256 */
    
    /* Callbacks */
    void (*on_disk_add)(uint32_t id, const char *name, void *user);
    void (*on_progress)(int current, int total, void *user);
    void *user_data;
} mxdf_options_t;

/*===========================================================================
 * MXDF API
 *===========================================================================*/

/** Create empty bundle */
mxdf_context_t* mxdf_create(void);

/** Destroy bundle */
void mxdf_destroy(mxdf_context_t *ctx);

/** Set options */
int mxdf_set_options(mxdf_context_t *ctx, const mxdf_options_t *opts);

/** Set bundle metadata */
int mxdf_set_metadata(mxdf_context_t *ctx, const mxdf_metadata_t *meta);

/*---------------------------------------------------------------------------
 * Disk Management
 *---------------------------------------------------------------------------*/

/** Add XDF file to bundle */
int mxdf_add_xdf(mxdf_context_t *ctx, const char *path, uint32_t *disk_id);

/** Add XDF context to bundle */
int mxdf_add_context(mxdf_context_t *ctx, xdf_context_t *xdf, 
                      const char *name, uint32_t *disk_id);

/** Add classic format (auto-converts to XDF) */
int mxdf_add_classic(mxdf_context_t *ctx, const char *path,
                      xdf_platform_t platform, uint32_t *disk_id);

/** Remove disk from bundle */
int mxdf_remove_disk(mxdf_context_t *ctx, uint32_t disk_id);

/** Get disk count */
int mxdf_get_disk_count(const mxdf_context_t *ctx);

/** Get disk entry */
int mxdf_get_disk(mxdf_context_t *ctx, uint32_t disk_id, 
                   mxdf_disk_entry_t *entry);

/** Get disk XDF context */
xdf_context_t* mxdf_get_disk_context(mxdf_context_t *ctx, uint32_t disk_id);

/*---------------------------------------------------------------------------
 * Relationships
 *---------------------------------------------------------------------------*/

/** Add relationship */
int mxdf_add_relation(mxdf_context_t *ctx, uint32_t source, uint32_t target,
                       mxdf_relation_t type, const char *description);

/** Get relationships for disk */
int mxdf_get_relations(mxdf_context_t *ctx, uint32_t disk_id,
                        mxdf_relation_entry_t *rels, size_t max);

/** Find related disks */
int mxdf_find_related(mxdf_context_t *ctx, uint32_t disk_id,
                       mxdf_relation_t type, uint32_t *ids, size_t max);

/*---------------------------------------------------------------------------
 * Import/Export
 *---------------------------------------------------------------------------*/

/** Save bundle to file */
int mxdf_save(mxdf_context_t *ctx, const char *path);

/** Load bundle from file */
int mxdf_load(mxdf_context_t *ctx, const char *path);

/** Extract single disk */
int mxdf_extract_disk(mxdf_context_t *ctx, uint32_t disk_id, 
                       const char *path);

/** Extract all disks */
int mxdf_extract_all(mxdf_context_t *ctx, const char *directory);

/** Export disk to classic format */
int mxdf_export_classic(mxdf_context_t *ctx, uint32_t disk_id,
                         const char *path);

/*---------------------------------------------------------------------------
 * Validation
 *---------------------------------------------------------------------------*/

/** Validate entire bundle */
int mxdf_validate(mxdf_context_t *ctx, int *errors);

/** Verify checksums */
int mxdf_verify_checksums(mxdf_context_t *ctx, int *mismatches);

/** Get bundle quality summary */
int mxdf_get_quality(mxdf_context_t *ctx, xdf_confidence_t *overall,
                      int *good, int *weak, int *bad);

/*---------------------------------------------------------------------------
 * Query Functions
 *---------------------------------------------------------------------------*/

/** Get header */
const mxdf_header_t* mxdf_get_header(const mxdf_context_t *ctx);

/** Get metadata */
const mxdf_metadata_t* mxdf_get_metadata(const mxdf_context_t *ctx);

/** Find disks by platform */
int mxdf_find_by_platform(mxdf_context_t *ctx, xdf_platform_t platform,
                           uint32_t *ids, size_t max);

/** Find disks by name pattern */
int mxdf_find_by_name(mxdf_context_t *ctx, const char *pattern,
                       uint32_t *ids, size_t max);

/** Get error message */
const char* mxdf_get_error(const mxdf_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XDF_MXDF_H */
