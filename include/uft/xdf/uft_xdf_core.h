/**
 * @file uft_xdf_core.h
 * @brief XDF Core - Universal Forensic Disk Container Specification
 * 
 * XDF (eXtended Disk Format) is a forensic container family for
 * preserving floppy disk data with full metadata, confidence scores,
 * and repair audit trails.
 * 
 * Container Family:
 * - AXDF: Amiga (ADF/ADZ) Extended
 * - DXDF: C64 (D64/G64) Extended  
 * - PXDF: PC (IMG/IMA) Extended
 * - TXDF: Atari ST (ST/MSA) Extended
 * - ZXDF: ZX Spectrum (TRD/DSK) Extended
 * - MXDF: Multi-Format Bundle (mixed platforms)
 * 
 * Design Principles:
 * 1. No assumptions without measurement
 * 2. No repair without justification
 * 3. No "OK/Error" - only confidence scores
 * 4. Copy protection ≠ defect
 * 5. Everything explicit, nothing implicit
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_XDF_CORE_H
#define UFT_XDF_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Magic Numbers & Version
 *===========================================================================*/

#define XDF_MAGIC_CORE      "XDF!"          /**< Core magic */
#define XDF_MAGIC_AXDF      "AXDF"          /**< Amiga */
#define XDF_MAGIC_DXDF      "DXDF"          /**< C64 */
#define XDF_MAGIC_PXDF      "PXDF"          /**< PC */
#define XDF_MAGIC_TXDF      "TXDF"          /**< Atari ST */
#define XDF_MAGIC_ZXDF      "ZXDF"          /**< ZX Spectrum */
#define XDF_MAGIC_MXDF      "MXDF"          /**< Multi-Format */

#define XDF_VERSION_MAJOR   1
#define XDF_VERSION_MINOR   0
#define XDF_ALIGNMENT       4096            /**< Block alignment */

/*===========================================================================
 * Platform Types
 *===========================================================================*/

typedef enum {
    XDF_PLATFORM_UNKNOWN = 0,
    XDF_PLATFORM_AMIGA,         /**< Commodore Amiga */
    XDF_PLATFORM_C64,           /**< Commodore 64/128 */
    XDF_PLATFORM_PC,            /**< IBM PC Compatible */
    XDF_PLATFORM_ATARIST,       /**< Atari ST/STE/TT */
    XDF_PLATFORM_SPECTRUM,      /**< ZX Spectrum */
    XDF_PLATFORM_APPLE2,        /**< Apple II */
    XDF_PLATFORM_BBC,           /**< BBC Micro */
    XDF_PLATFORM_MSX,           /**< MSX */
    XDF_PLATFORM_CPC,           /**< Amstrad CPC */
    XDF_PLATFORM_MIXED = 0xFF,  /**< Multi-platform bundle */
} xdf_platform_t;

/*===========================================================================
 * Encoding Types
 *===========================================================================*/

typedef enum {
    XDF_ENCODING_UNKNOWN = 0,
    XDF_ENCODING_MFM,           /**< Modified Frequency Modulation */
    XDF_ENCODING_FM,            /**< Frequency Modulation (single density) */
    XDF_ENCODING_GCR_C64,       /**< Commodore GCR */
    XDF_ENCODING_GCR_APPLE,     /**< Apple GCR (6-and-2) */
    XDF_ENCODING_GCR_AMIGA,     /**< Amiga GCR (rare) */
    XDF_ENCODING_RAW_FLUX,      /**< Raw flux transitions */
} xdf_encoding_t;

/*===========================================================================
 * Confidence & Status Types
 *===========================================================================*/

/**
 * @brief Confidence score (0-10000 = 0.00% - 100.00%)
 * 
 * Precision: 0.01%
 * - 0-1000: Very low confidence
 * - 1000-5000: Low confidence  
 * - 5000-8000: Medium confidence
 * - 8000-9500: High confidence
 * - 9500-10000: Very high confidence
 */
typedef uint16_t xdf_confidence_t;

#define XDF_CONF_ZERO       0       /**< No confidence */
#define XDF_CONF_LOW        2500    /**< 25% */
#define XDF_CONF_MEDIUM     5000    /**< 50% */
#define XDF_CONF_HIGH       7500    /**< 75% */
#define XDF_CONF_VERY_HIGH  9000    /**< 90% */
#define XDF_CONF_PERFECT    10000   /**< 100% */

/**
 * @brief Element status classification
 */
typedef enum {
    XDF_STATUS_UNKNOWN = 0,
    XDF_STATUS_OK,              /**< Verified good */
    XDF_STATUS_WEAK,            /**< Weak/unstable bits */
    XDF_STATUS_PROTECTED,       /**< Intentional protection */
    XDF_STATUS_DEFECT,          /**< Physical defect */
    XDF_STATUS_REPAIRED,        /**< Was defect, now repaired */
    XDF_STATUS_UNREADABLE,      /**< Cannot recover */
    XDF_STATUS_MISSING,         /**< Data not present */
} xdf_status_t;

/**
 * @brief Error classification
 */
typedef enum {
    XDF_ERROR_NONE = 0,
    XDF_ERROR_CRC,              /**< CRC mismatch */
    XDF_ERROR_SYNC,             /**< Sync pattern error */
    XDF_ERROR_HEADER,           /**< Header CRC error */
    XDF_ERROR_DATA,             /**< Data area error */
    XDF_ERROR_TIMING,           /**< Timing anomaly */
    XDF_ERROR_DENSITY,          /**< Density mismatch */
    XDF_ERROR_MISSING,          /**< Missing sector */
    XDF_ERROR_DUPLICATE,        /**< Duplicate sector ID */
    XDF_ERROR_GAP,              /**< Abnormal gap */
} xdf_error_t;

/*===========================================================================
 * Track Zone Types (Signal Analysis)
 *===========================================================================*/

/**
 * @brief Track zone classification
 * 
 * A track is treated as a SIGNAL, not a byte array.
 * Each zone has distinct characteristics.
 */
typedef enum {
    XDF_ZONE_UNKNOWN = 0,
    XDF_ZONE_SYNC,              /**< Sync pattern (stable) */
    XDF_ZONE_HEADER,            /**< Sector header */
    XDF_ZONE_DATA,              /**< Sector data */
    XDF_ZONE_GAP,               /**< Inter-sector gap */
    XDF_ZONE_WEAK,              /**< Weak bit region */
    XDF_ZONE_NOISE,             /**< Undefined/noise */
    XDF_ZONE_PROTECTION,        /**< Protection area */
    XDF_ZONE_TIMING_ANOMALY,    /**< Timing-based protection */
} xdf_zone_type_t;

/**
 * @brief Zone descriptor (within a track)
 */
typedef struct __attribute__((packed)) {
    uint32_t offset;            /**< Bit offset in track */
    uint32_t length;            /**< Length in bits */
    uint8_t type;               /**< xdf_zone_type_t */
    uint8_t status;             /**< xdf_status_t */
    xdf_confidence_t confidence;/**< Zone confidence */
    
    /* Stability metrics */
    uint8_t stability;          /**< Reproducibility (0-100) */
    uint8_t variance;           /**< Bit variance (0-100) */
    uint8_t reserved[2];
} xdf_zone_t;

/*===========================================================================
 * Read Capture (Phase 1: Multi-Read)
 *===========================================================================*/

/**
 * @brief Single read capture
 */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;         /**< Capture timestamp */
    uint16_t revolution;        /**< Revolution number */
    uint16_t flags;             /**< Capture flags */
    
    uint32_t data_offset;       /**< Offset to raw data */
    uint32_t data_size;         /**< Raw data size */
    
    /* Quality metrics */
    xdf_confidence_t confidence;/**< Read confidence */
    uint16_t errors;            /**< Error count */
    
    /* Timing info */
    uint32_t bitcell_avg;       /**< Average bitcell (ns) */
    uint16_t bitcell_jitter;    /**< Bitcell jitter (ns) */
    uint16_t reserved;
} xdf_read_capture_t;

/*===========================================================================
 * Sector Header
 *===========================================================================*/

typedef struct __attribute__((packed)) {
    /* Identity */
    uint8_t sector;             /**< Sector number */
    uint8_t head;               /**< Head (0/1) */
    uint16_t size;              /**< Data size in bytes */
    
    /* Status */
    uint8_t status;             /**< xdf_status_t */
    uint8_t error;              /**< xdf_error_t */
    xdf_confidence_t confidence;/**< Sector confidence */
    
    /* Checksums */
    uint32_t stored_crc;        /**< CRC from disk */
    uint32_t computed_crc;      /**< Computed CRC */
    
    /* Multi-read stats */
    uint8_t read_count;         /**< Number of reads */
    uint8_t stable_reads;       /**< Consistent reads */
    uint8_t weak_bits;          /**< Weak bit count */
    uint8_t repair_flags;       /**< Repair actions taken */
    
    /* Zone info */
    uint32_t zone_offset;       /**< Offset in zone table */
    uint8_t zone_count;         /**< Number of zones */
    uint8_t reserved[3];
} xdf_sector_t;

/*===========================================================================
 * Track Header
 *===========================================================================*/

/** Track flags */
#define XDF_TRK_HAS_FLUX        0x0001  /**< Raw flux present */
#define XDF_TRK_HAS_DECODED     0x0002  /**< Decoded data present */
#define XDF_TRK_HAS_ZONES       0x0004  /**< Zone map present */
#define XDF_TRK_HAS_TIMING      0x0008  /**< Timing data present */
#define XDF_TRK_HAS_MULTI_READ  0x0010  /**< Multiple reads stored */
#define XDF_TRK_PROTECTED       0x0020  /**< Protection detected */
#define XDF_TRK_REPAIRED        0x0040  /**< Track was repaired */
#define XDF_TRK_WEAK_BITS       0x0080  /**< Weak bits detected */
#define XDF_TRK_TIMING_ANOMALY  0x0100  /**< Timing protection */
#define XDF_TRK_LONG_TRACK      0x0200  /**< Extended track length */
#define XDF_TRK_DENSITY_ERROR   0x0400  /**< Density mismatch */

typedef struct __attribute__((packed)) {
    /* Identity */
    uint8_t cylinder;           /**< Cylinder number */
    uint8_t head;               /**< Head (0/1) */
    uint16_t flags;             /**< Track flags */
    
    /* Encoding */
    uint8_t encoding;           /**< xdf_encoding_t */
    uint8_t density;            /**< Density zone */
    uint8_t sectors_expected;   /**< Expected sectors */
    uint8_t sectors_found;      /**< Actually found */
    
    /* Data offsets */
    uint32_t flux_offset;       /**< Raw flux data */
    uint32_t flux_size;         /**< Flux data size */
    uint32_t decoded_offset;    /**< Decoded sectors */
    uint32_t decoded_size;      /**< Decoded size */
    
    /* Zone map */
    uint32_t zone_offset;       /**< Zone table offset */
    uint16_t zone_count;        /**< Number of zones */
    
    /* Multi-read */
    uint16_t read_count;        /**< Number of captures */
    uint32_t reads_offset;      /**< Read captures table */
    
    /* Quality */
    xdf_confidence_t confidence;/**< Overall confidence */
    uint8_t status;             /**< xdf_status_t */
    uint8_t reproducibility;    /**< 0-100% */
    
    /* Timing */
    uint32_t track_length;      /**< Track length (bits) */
    uint32_t bitcell_time;      /**< Nominal bitcell (ns) */
    
    /* Checksums */
    uint32_t data_crc32;        /**< CRC32 of decoded data */
    uint32_t flux_crc32;        /**< CRC32 of flux data */
} xdf_track_t;

/*===========================================================================
 * Protection Detection
 *===========================================================================*/

/** Protection type flags */
#define XDF_PROT_WEAK_BITS      0x00000001
#define XDF_PROT_FUZZY_BITS     0x00000002
#define XDF_PROT_LONG_TRACK     0x00000004
#define XDF_PROT_SHORT_TRACK    0x00000008
#define XDF_PROT_DENSITY_CHANGE 0x00000010
#define XDF_PROT_TIMING         0x00000020
#define XDF_PROT_EXTRA_SECTORS  0x00000040
#define XDF_PROT_MISSING_SECTOR 0x00000080
#define XDF_PROT_DUPLICATE_ID   0x00000100
#define XDF_PROT_BAD_CRC        0x00000200
#define XDF_PROT_SYNC_PATTERN   0x00000400
#define XDF_PROT_GAP_ENCODING   0x00000800
#define XDF_PROT_HALF_TRACKS    0x00001000
#define XDF_PROT_NO_FLUX        0x00002000
#define XDF_PROT_CUSTOM         0x80000000

typedef struct __attribute__((packed)) {
    uint32_t type_flags;        /**< Protection types detected */
    xdf_confidence_t confidence;/**< Detection confidence */
    
    uint8_t primary_track;      /**< Main protection track */
    uint8_t primary_sector;     /**< Main protection sector */
    
    char name[32];              /**< Protection name */
    char publisher[32];         /**< Publisher/cracker */
    
    /* Pattern match info */
    char matched_pattern[64];   /**< Known pattern matched */
    uint32_t pattern_offset;    /**< Pattern location */
    
    /* Decision */
    uint8_t is_intentional;     /**< true = protection, false = defect */
    uint8_t reserved[7];
} xdf_protection_t;

/*===========================================================================
 * Repair Log Entry
 *===========================================================================*/

/** Repair action types */
typedef enum {
    XDF_REPAIR_NONE = 0,
    XDF_REPAIR_CRC_1BIT,        /**< Single-bit CRC fix */
    XDF_REPAIR_CRC_2BIT,        /**< Two-bit CRC fix */
    XDF_REPAIR_MULTI_REV,       /**< Multi-revolution fusion */
    XDF_REPAIR_INTERPOLATE,     /**< Weak bit interpolation */
    XDF_REPAIR_PATTERN,         /**< Pattern-based reconstruction */
    XDF_REPAIR_REFERENCE,       /**< Reference image comparison */
    XDF_REPAIR_MANUAL,          /**< Manual correction */
    XDF_REPAIR_UNDO,            /**< Repair was undone */
} xdf_repair_action_t;

typedef struct __attribute__((packed)) {
    uint32_t timestamp;         /**< When repaired */
    
    /* Location */
    uint8_t track;
    uint8_t head;
    uint8_t sector;             /**< 0xFF = whole track */
    uint8_t action;             /**< xdf_repair_action_t */
    
    /* Details */
    uint32_t bit_offset;        /**< Bit position */
    uint32_t bits_changed;      /**< Number of bits modified */
    
    /* Before/After */
    uint32_t original_crc;      
    uint32_t repaired_crc;
    xdf_confidence_t before_conf;
    xdf_confidence_t after_conf;
    
    /* Justification */
    char reason[64];            /**< Why this repair */
    char method[32];            /**< How it was done */
    
    /* Reversibility */
    uint32_t undo_offset;       /**< Offset to undo data */
    uint16_t undo_size;         /**< Size of undo data */
    uint8_t reversible;         /**< Can be undone? */
    uint8_t reserved;
} xdf_repair_entry_t;

/*===========================================================================
 * Decision Matrix Entry
 *===========================================================================*/

/**
 * @brief Explains WHY something is classified as it is
 */
typedef struct __attribute__((packed)) {
    /* Location */
    uint8_t track;
    uint8_t head;
    uint8_t sector;
    uint8_t zone;
    
    /* Classification */
    uint8_t status;             /**< xdf_status_t */
    uint8_t error;              /**< xdf_error_t */
    xdf_confidence_t confidence;
    
    /* Evidence */
    uint32_t evidence_flags;    /**< What was observed */
    
    /* Reasoning */
    char observation[64];       /**< What was measured */
    char interpretation[64];    /**< What it means */
    char decision[64];          /**< Final classification */
    
    /* Supporting data */
    uint32_t measurement_offset;/**< Raw measurement data */
    uint16_t measurement_size;
    uint16_t reserved;
} xdf_decision_t;

/*===========================================================================
 * Stability Map (Phase 2: Compare)
 *===========================================================================*/

/**
 * @brief Per-bit stability across multiple reads
 */
typedef struct __attribute__((packed)) {
    uint8_t track;
    uint8_t head;
    uint16_t bit_count;         /**< Number of bits */
    
    /* Bitmap: 2 bits per bit position */
    /* 00 = stable 0, 01 = stable 1, 10 = unstable, 11 = unknown */
    uint32_t data_offset;       /**< Stability bitmap */
    uint32_t data_size;
    
    /* Statistics */
    uint32_t stable_bits;       /**< Count of stable bits */
    uint32_t unstable_bits;     /**< Count of unstable bits */
    float reproducibility;      /**< 0.0 - 1.0 */
} xdf_stability_map_t;

/*===========================================================================
 * Knowledge Base Match (Phase 4)
 *===========================================================================*/

typedef enum {
    XDF_KB_SOURCE_UNKNOWN = 0,
    XDF_KB_SOURCE_WHDLOAD,      /**< WHDLoad slave */
    XDF_KB_SOURCE_CAPS,         /**< SPS/CAPS database */
    XDF_KB_SOURCE_TOSEC,        /**< TOSEC */
    XDF_KB_SOURCE_SCENE,        /**< Scene documentation */
    XDF_KB_SOURCE_PUBLISHER,    /**< Original publisher */
    XDF_KB_SOURCE_USER,         /**< User-provided */
} xdf_kb_source_t;

typedef struct __attribute__((packed)) {
    uint8_t source;             /**< xdf_kb_source_t */
    uint8_t match_type;         /**< 0=exact, 1=similar, 2=partial */
    xdf_confidence_t confidence;/**< Match confidence */
    
    char pattern_name[64];      /**< Name of matched pattern */
    char reference_id[32];      /**< External reference ID */
    char notes[128];            /**< Additional notes */
    
    /* What was matched */
    uint32_t matched_offset;    /**< Offset in disk */
    uint32_t matched_size;      /**< Size of match */
    uint8_t track;
    uint8_t sector;
    uint8_t reserved[2];
} xdf_kb_match_t;

/*===========================================================================
 * File Header (Universal)
 *===========================================================================*/

typedef struct __attribute__((packed)) {
    /* Magic & Version (16 bytes) */
    char magic[4];              /**< XDF_MAGIC_* */
    uint8_t version_major;
    uint8_t version_minor;
    uint16_t header_size;       /**< Header size */
    uint32_t file_size;         /**< Total file size */
    uint32_t file_crc32;        /**< CRC32 of entire file */
    
    /* Platform (16 bytes) */
    uint8_t platform;           /**< xdf_platform_t */
    uint8_t encoding;           /**< Primary xdf_encoding_t */
    uint8_t num_heads;          /**< 1 or 2 */
    uint8_t num_cylinders;      /**< Number of cylinders */
    uint8_t sectors_per_track;  /**< Typical sectors */
    uint8_t sector_size_shift;  /**< log2(sector_size) */
    uint16_t flags;             /**< Global flags */
    uint8_t reserved1[8];
    
    /* Capture Info (64 bytes) */
    char capture_device[32];    /**< Device name */
    char capture_date[24];      /**< ISO 8601 */
    uint8_t capture_revs;       /**< Revolutions captured */
    uint8_t capture_flags;
    uint8_t reserved2[6];
    
    /* Content Info (64 bytes) */
    char disk_name[32];         /**< Disk name */
    char disk_label[24];        /**< Volume label */
    uint32_t creation_date;     /**< Original creation */
    uint32_t modification_date; /**< Last modified */
    
    /* Protection (64 bytes) */
    uint32_t protection_flags;  /**< Protection types */
    xdf_confidence_t prot_confidence;
    uint8_t prot_track;
    uint8_t prot_sector;
    char protection_name[32];
    char protection_publisher[24];
    
    /* Quality Summary (32 bytes) */
    xdf_confidence_t overall_confidence;
    uint16_t total_tracks;
    uint16_t good_tracks;
    uint16_t weak_tracks;
    uint16_t bad_tracks;
    uint16_t repaired_tracks;
    uint16_t protected_tracks;
    uint16_t total_sectors;
    uint16_t good_sectors;
    uint16_t bad_sectors;
    uint16_t repaired_sectors;
    uint8_t reserved3[6];
    
    /* Table Offsets (64 bytes) */
    uint32_t track_table_offset;
    uint32_t track_table_count;
    uint32_t sector_table_offset;
    uint32_t sector_table_count;
    uint32_t zone_table_offset;
    uint32_t zone_table_count;
    uint32_t repair_log_offset;
    uint32_t repair_log_count;
    uint32_t decision_table_offset;
    uint32_t decision_table_count;
    uint32_t kb_match_offset;
    uint32_t kb_match_count;
    uint32_t stability_offset;
    uint32_t stability_count;
    uint32_t data_offset;
    uint32_t data_size;
    
    /* Padding to 512 bytes */
    uint8_t padding[192];
} xdf_header_t;

/*===========================================================================
 * API Types
 *===========================================================================*/

typedef struct xdf_context_s xdf_context_t;

typedef struct {
    /* Phase 1: Read options */
    int read_count;             /**< Reads per track (default: 3) */
    int max_revolutions;        /**< Max revolutions (default: 5) */
    bool capture_flux;          /**< Store raw flux */
    bool capture_timing;        /**< Store timing data */
    
    /* Phase 2: Compare options */
    bool generate_stability_map;/**< Generate bit stability */
    float stability_threshold;  /**< 0.0-1.0 */
    
    /* Phase 3: Analysis options */
    bool analyze_zones;         /**< Generate zone map */
    bool detect_protection;     /**< Detect copy protection */
    
    /* Phase 4: Knowledge options */
    bool use_whdload_db;        /**< Match WHDLoad patterns */
    bool use_caps_db;           /**< Match CAPS patterns */
    const char *pattern_dir;    /**< Custom pattern directory */
    
    /* Phase 5: Validation options */
    float min_confidence;       /**< Minimum acceptable (default: 0.5) */
    
    /* Phase 6: Repair options */
    bool enable_repair;         /**< Enable auto-repair */
    int max_repair_bits;        /**< Max bits to correct */
    bool repair_only_defects;   /**< Don't touch protection */
    bool require_confirmation;  /**< Confirm each repair */
    
    /* Phase 7: Export options */
    bool export_classic;        /**< Generate ADF/D64/etc */
    bool include_flux;          /**< Include flux in XDF */
    bool include_zones;         /**< Include zone map */
    bool include_decisions;     /**< Include decision matrix */
    
    /* Callbacks */
    void (*on_track)(int cyl, int head, xdf_status_t status, void *user);
    void (*on_sector)(int cyl, int head, int sector, xdf_status_t status, void *user);
    void (*on_repair)(const xdf_repair_entry_t *entry, void *user);
    void (*on_decision)(const xdf_decision_t *decision, void *user);
    void *user_data;
} xdf_options_t;

/*===========================================================================
 * Pipeline Phase Results
 *===========================================================================*/

typedef struct {
    /* Phase 1: Read */
    int total_reads;
    int successful_reads;
    int failed_reads;
    
    /* Phase 2: Compare */
    float average_stability;
    int unstable_regions;
    
    /* Phase 3: Analyze */
    int zones_identified;
    int protection_zones;
    int weak_zones;
    
    /* Phase 4: Knowledge */
    int patterns_matched;
    const char *best_match;
    float match_confidence;
    
    /* Phase 5: Validate */
    xdf_confidence_t overall_confidence;
    int ok_count;
    int weak_count;
    int defect_count;
    int protected_count;
    
    /* Phase 6: Repair */
    int repairs_attempted;
    int repairs_successful;
    int repairs_failed;
    
    /* Phase 7: Rebuild */
    bool classic_exported;
    bool xdf_exported;
} xdf_pipeline_result_t;

/*===========================================================================
 * Core API Functions
 *===========================================================================*/

/** Create context for specific platform */
xdf_context_t* xdf_create(xdf_platform_t platform);

/** Destroy context */
void xdf_destroy(xdf_context_t *ctx);

/** Set options */
int xdf_set_options(xdf_context_t *ctx, const xdf_options_t *opts);

/** Get default options */
xdf_options_t xdf_options_default(void);

/*---------------------------------------------------------------------------
 * Pipeline Execution
 *---------------------------------------------------------------------------*/

/** Run complete 7-phase pipeline */
int xdf_run_pipeline(xdf_context_t *ctx, xdf_pipeline_result_t *result);

/** Run individual phases */
int xdf_phase_read(xdf_context_t *ctx);
int xdf_phase_compare(xdf_context_t *ctx);
int xdf_phase_analyze(xdf_context_t *ctx);
int xdf_phase_knowledge(xdf_context_t *ctx);
int xdf_phase_validate(xdf_context_t *ctx);
int xdf_phase_repair(xdf_context_t *ctx);
int xdf_phase_rebuild(xdf_context_t *ctx);

/*---------------------------------------------------------------------------
 * Import/Export
 *---------------------------------------------------------------------------*/

/** Import from classic format */
int xdf_import(xdf_context_t *ctx, const char *path);

/** Import from flux capture */
int xdf_import_flux(xdf_context_t *ctx, const char *path);

/** Export to XDF */
int xdf_export(xdf_context_t *ctx, const char *path);

/** Export to classic format */
int xdf_export_classic(xdf_context_t *ctx, const char *path);

/*---------------------------------------------------------------------------
 * Query Functions
 *---------------------------------------------------------------------------*/

/** Get header */
const xdf_header_t* xdf_get_header(const xdf_context_t *ctx);

/** Get track info */
int xdf_get_track(xdf_context_t *ctx, int cyl, int head, xdf_track_t *info);

/** Get sector data */
int xdf_get_sector(xdf_context_t *ctx, int cyl, int head, int sector,
                    xdf_sector_t *info, uint8_t **data, size_t *size);

/** Get stability map */
int xdf_get_stability(xdf_context_t *ctx, int cyl, int head,
                       xdf_stability_map_t *map);

/** Get zone info */
int xdf_get_zones(xdf_context_t *ctx, int cyl, int head,
                   xdf_zone_t **zones, size_t *count);

/** Get protection info */
int xdf_get_protection(xdf_context_t *ctx, xdf_protection_t *prot);

/** Get repair log */
int xdf_get_repairs(xdf_context_t *ctx, xdf_repair_entry_t **repairs,
                     size_t *count);

/** Get decision matrix */
int xdf_get_decisions(xdf_context_t *ctx, xdf_decision_t **decisions,
                       size_t *count);

/** Get knowledge matches */
int xdf_get_kb_matches(xdf_context_t *ctx, xdf_kb_match_t **matches,
                        size_t *count);

/*---------------------------------------------------------------------------
 * Utility Functions
 *---------------------------------------------------------------------------*/

/** Get platform name */
const char* xdf_platform_name(xdf_platform_t platform);

/** Get encoding name */
const char* xdf_encoding_name(xdf_encoding_t encoding);

/** Get status name */
const char* xdf_status_name(xdf_status_t status);

/** Get error name */
const char* xdf_error_name(xdf_error_t error);

/** Get last error message */
const char* xdf_get_error(const xdf_context_t *ctx);

/** Format confidence as string (e.g., "95.50%") */
void xdf_format_confidence(xdf_confidence_t conf, char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XDF_CORE_H */
