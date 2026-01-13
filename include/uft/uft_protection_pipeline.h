/**
 * @file uft_protection_pipeline.h
 * @brief Copy Protection Preserve Pipeline
 * 
 * P2-002: Protection Preserve Pipeline
 * 
 * Pipeline for preserving copy protection features during:
 * - Disk reading (capture all protection artifacts)
 * - Disk writing (recreate protection on new disk)
 * - Format conversion (preserve across formats)
 * 
 * Protection artifacts supported:
 * - Weak bits / Flakey bits
 * - Timing variations (long/short tracks)
 * - Bad sectors (intentional)
 * - Non-standard sector IDs
 * - Duplicate sectors
 * - Missing sectors
 * - Non-standard gap lengths
 * - Track density variations
 * - Sync patterns (long/short)
 * - Half tracks / Quarter tracks
 */

#ifndef UFT_PROTECTION_PIPELINE_H
#define UFT_PROTECTION_PIPELINE_H

#include "uft_types.h"
#include "uft_error.h"
#include "uft_copy_protection.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protection Artifact Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_ARTIFACT_NONE           = 0,
    UFT_ARTIFACT_WEAK_BITS      = (1 << 0),   /* Flakey/unstable bits */
    UFT_ARTIFACT_BAD_SECTOR     = (1 << 1),   /* Intentionally bad sector */
    UFT_ARTIFACT_TIMING_VAR     = (1 << 2),   /* Non-standard timing */
    UFT_ARTIFACT_DUP_SECTOR     = (1 << 3),   /* Duplicate sector ID */
    UFT_ARTIFACT_MISSING_SECTOR = (1 << 4),   /* Intentionally missing */
    UFT_ARTIFACT_EXTRA_SECTOR   = (1 << 5),   /* Extra sector on track */
    UFT_ARTIFACT_LONG_TRACK     = (1 << 6),   /* Track longer than normal */
    UFT_ARTIFACT_SHORT_TRACK    = (1 << 7),   /* Track shorter than normal */
    UFT_ARTIFACT_HALF_TRACK     = (1 << 8),   /* Half-track data */
    UFT_ARTIFACT_SYNC_PATTERN   = (1 << 9),   /* Non-standard sync */
    UFT_ARTIFACT_GAP_LENGTH     = (1 << 10),  /* Custom gap length */
    UFT_ARTIFACT_DENSITY_VAR    = (1 << 11),  /* Variable bit density */
    UFT_ARTIFACT_SECTOR_ID      = (1 << 12),  /* Non-standard sector ID */
    UFT_ARTIFACT_CRC_ERROR      = (1 << 13),  /* Intentional CRC error */
    UFT_ARTIFACT_DATA_MARK      = (1 << 14),  /* Non-standard data mark */
    UFT_ARTIFACT_ALL            = 0x7FFF
} uft_artifact_flags_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protection Element - Single artifact record
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Location */
    int cylinder;
    int head;
    int sector;              /* -1 if track-level */
    size_t bit_offset;       /* Position in track (bits) */
    
    /* Type and data */
    uft_artifact_flags_t type;
    uint32_t flags;
    
    /* For weak bits */
    uint8_t *weak_mask;      /* Bit mask of unstable bits */
    size_t weak_mask_size;
    int weak_bit_count;
    
    /* For timing */
    double timing_ns;        /* Actual timing in ns */
    double expected_ns;      /* Expected timing */
    double variance_pct;     /* Percentage variance */
    
    /* For sector anomalies */
    uint8_t sector_id_cyl;   /* Physical sector ID cylinder */
    uint8_t sector_id_head;  /* Physical sector ID head */
    uint8_t sector_id_sec;   /* Physical sector ID sector */
    uint8_t sector_id_size;  /* Physical sector size code */
    
    /* For data */
    uint8_t *original_data;
    size_t data_size;
    
    /* Metadata */
    int confidence;          /* Detection confidence 0-100 */
    char description[128];
} uft_protection_element_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Protection Info
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    int cylinder;
    int head;
    
    /* Track-level artifacts */
    uft_artifact_flags_t artifacts;
    
    /* Timing */
    double track_length_bits;
    double expected_length_bits;
    double rpm_variance;
    
    /* Sync */
    size_t sync_offset;      /* Start of sync pattern */
    size_t sync_length;      /* Sync pattern length */
    uint8_t sync_byte;       /* Sync byte value */
    
    /* Gaps */
    size_t gap1_length;
    size_t gap2_length;
    size_t gap3_length;
    size_t gap4a_length;
    
    /* Elements on this track */
    uft_protection_element_t *elements;
    int element_count;
    int element_capacity;
} uft_track_protection_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk Protection Map (complete disk analysis)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Overall info */
    uft_copy_protection_t scheme;
    const char *scheme_name;
    int confidence;
    
    /* Summary flags */
    uft_artifact_flags_t artifacts_present;
    
    /* Statistics */
    int total_weak_bits;
    int total_bad_sectors;
    int total_timing_anomalies;
    int total_duplicate_sectors;
    int half_track_count;
    
    /* Track map */
    uft_track_protection_t *tracks;
    int track_count;
    int cylinders;
    int heads;
    
    /* Raw protection data (for format conversion) */
    uint8_t *raw_data;
    size_t raw_data_size;
    
    /* Metadata */
    char detection_log[1024];
    double analysis_time_ms;
} uft_protection_map_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Pipeline Options
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* What to detect */
    uft_artifact_flags_t detect_flags;
    
    /* Weak bit detection */
    bool detect_weak_bits;
    int weak_bit_revolutions;     /* 2-16 */
    float weak_bit_threshold;     /* 0.1-0.5 */
    
    /* Timing analysis */
    bool analyze_timing;
    float timing_tolerance_pct;   /* Default 5% */
    
    /* Half-track analysis */
    bool scan_half_tracks;
    
    /* Output options */
    bool generate_report;
    bool verbose_log;
    
    /* Preservation mode */
    bool preserve_on_write;
    bool preserve_on_convert;
    
    /* Platform-specific */
    uft_format_t source_format;
    uft_format_t target_format;
} uft_protection_options_t;

#define UFT_PROTECTION_OPTIONS_DEFAULT { \
    .detect_flags = UFT_ARTIFACT_ALL, \
    .detect_weak_bits = true, \
    .weak_bit_revolutions = 3, \
    .weak_bit_threshold = 0.15f, \
    .analyze_timing = true, \
    .timing_tolerance_pct = 5.0f, \
    .scan_half_tracks = false, \
    .generate_report = true, \
    .verbose_log = false, \
    .preserve_on_write = true, \
    .preserve_on_convert = true, \
    .source_format = UFT_FORMAT_UNKNOWN, \
    .target_format = UFT_FORMAT_UNKNOWN \
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Pipeline Handle
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_protection_pipeline uft_protection_pipeline_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create protection pipeline
 */
uft_protection_pipeline_t* uft_protection_pipeline_create(
    const uft_protection_options_t *options);

/**
 * @brief Destroy pipeline
 */
void uft_protection_pipeline_destroy(uft_protection_pipeline_t *pipe);

/**
 * @brief Analyze disk image for protection
 */
uft_error_t uft_protection_analyze_file(
    uft_protection_pipeline_t *pipe,
    const char *path,
    uft_protection_map_t **map_out);

/**
 * @brief Analyze flux data for protection
 */
uft_error_t uft_protection_analyze_flux(
    uft_protection_pipeline_t *pipe,
    const uint8_t *flux_data,
    size_t flux_size,
    int revolutions,
    uft_protection_map_t **map_out);

/**
 * @brief Analyze single track
 */
uft_error_t uft_protection_analyze_track(
    uft_protection_pipeline_t *pipe,
    int cylinder, int head,
    const uint8_t *track_data,
    size_t track_size,
    const uint8_t **multi_rev_data,  /* NULL or array of revolution data */
    int rev_count,
    uft_track_protection_t *track_out);

/**
 * @brief Apply protection to write operation
 */
uft_error_t uft_protection_apply_to_write(
    uft_protection_pipeline_t *pipe,
    const uft_protection_map_t *map,
    int cylinder, int head,
    uint8_t *track_buffer,
    size_t *track_size,
    uint8_t *weak_mask_out);

/**
 * @brief Preserve protection during format conversion
 */
uft_error_t uft_protection_convert(
    uft_protection_pipeline_t *pipe,
    const uft_protection_map_t *source_map,
    uft_format_t target_format,
    uft_protection_map_t **target_map_out);

/**
 * @brief Free protection map
 */
void uft_protection_map_free(uft_protection_map_t *map);

/**
 * @brief Free track protection data
 */
void uft_track_protection_free(uft_track_protection_t *track);

/**
 * @brief Generate protection analysis report
 */
uft_error_t uft_protection_generate_report(
    const uft_protection_map_t *map,
    char *buffer,
    size_t buffer_size);

/**
 * @brief Get artifact type name
 */
const char* uft_artifact_name(uft_artifact_flags_t type);

/**
 * @brief Check if format supports protection preservation
 */
bool uft_format_supports_protection(uft_format_t format, uft_artifact_flags_t artifact);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Weak Bit Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect weak bits from multiple revolutions
 */
int uft_detect_weak_bits_multirev(
    const uint8_t **rev_data,
    int rev_count,
    size_t data_size,
    float threshold,
    uint8_t *weak_mask_out);

/**
 * @brief Generate random data for weak bit positions
 */
void uft_weak_bits_randomize(
    uint8_t *data,
    const uint8_t *weak_mask,
    size_t size);

/**
 * @brief Count weak bits in mask
 */
int uft_weak_bits_count(const uint8_t *mask, size_t size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Platform-Specific Protection Schemes
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Amiga protection schemes
 */
typedef enum {
    UFT_AMIGA_PROT_NONE = 0,
    UFT_AMIGA_PROT_RNC_COPYLOCK,
    UFT_AMIGA_PROT_ROB_NORTHERN,
    UFT_AMIGA_PROT_DUNGEON_MASTER,
    UFT_AMIGA_PROT_PSYGNOSIS,
    UFT_AMIGA_PROT_GREMLIN,
    UFT_AMIGA_PROT_RAINBIRD,
    UFT_AMIGA_PROT_CUSTOM
} uft_amiga_protection_t;

/**
 * @brief C64 protection schemes  
 */
typedef enum {
    UFT_C64_PROT_NONE = 0,
    UFT_C64_PROT_RAPIDLOK,
    UFT_C64_PROT_V_MAX,
    UFT_C64_PROT_VORPAL,
    UFT_C64_PROT_FAT_TRACK,
    UFT_C64_PROT_GCR_SYNC,
    UFT_C64_PROT_CUSTOM
} uft_c64_protection_t;

/**
 * @brief Apple II protection schemes
 */
typedef enum {
    UFT_APPLE_PROT_NONE = 0,
    UFT_APPLE_PROT_LOCKSMITH,
    UFT_APPLE_PROT_SPIRADISK,
    UFT_APPLE_PROT_HALF_TRACK,
    UFT_APPLE_PROT_QUARTER_TRACK,
    UFT_APPLE_PROT_SYNC_COUNT,
    UFT_APPLE_PROT_CUSTOM
} uft_apple_protection_t;

/**
 * @brief Detect Amiga protection
 */
uft_amiga_protection_t uft_detect_amiga_protection(
    const uint8_t *track_data,
    size_t track_size,
    int cylinder,
    int head);

/**
 * @brief Detect C64 protection
 */
uft_c64_protection_t uft_detect_c64_protection(
    const uint8_t *track_data,
    size_t track_size,
    int track_number);

/**
 * @brief Detect Apple II protection
 */
uft_apple_protection_t uft_detect_apple_protection(
    const uint8_t *track_data,
    size_t track_size,
    int track_number);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROTECTION_PIPELINE_H */
