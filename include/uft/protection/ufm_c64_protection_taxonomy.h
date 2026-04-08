/**
 * @file ufm_c64_protection_taxonomy.h
 * @brief C64 Copy Protection Taxonomy — types and classification
 */
#ifndef UFM_C64_PROTECTION_TAXONOMY_H
#define UFM_C64_PROTECTION_TAXONOMY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Protection scheme type classification */
typedef enum {
    UFM_PROT_NONE = 0,
    UFM_PROT_VMAX,              /**< V-MAX! */
    UFM_PROT_RAPIDLOK,          /**< RapidLok */
    UFM_PROT_COPYLOCK,          /**< CopyLock */
    UFM_PROT_SPEEDLOCK,         /**< Speedlock */
    UFM_PROT_FATBITS,           /**< FatBits / Super Nibble */
    UFM_PROT_PIRATE_SLAYER,     /**< Pirate Slayer */
    UFM_PROT_VORPAL,            /**< Vorpal */
    UFM_PROT_GEOS,              /**< GEOS protection */
    UFM_PROT_ECA,               /**< Electronic Arts */
    UFM_PROT_CUSTOM_SYNC,       /**< Custom sync patterns */
    UFM_PROT_HALF_TRACK,        /**< Half-track data */
    UFM_PROT_LONG_TRACK,        /**< Extended track length */
    UFM_PROT_DENSITY_MISMATCH,  /**< Wrong density zone */
    UFM_PROT_DUPLICATE_ID,      /**< Duplicate sector IDs */
    UFM_PROT_BAD_GCR,           /**< Invalid GCR sequences */
    UFM_PROT_TIMING,            /**< Timing-based checks */
    UFM_PROT_UNKNOWN,           /**< Unknown scheme */
    UFM_PROT_COUNT
} ufm_c64_prot_type_t;

/** Per-track analysis metrics */
typedef struct {
    uint8_t  track;
    uint8_t  side;
    uint32_t bitcell_count;
    uint32_t sync_count;
    uint32_t sector_count;
    uint32_t bad_gcr_count;
    uint32_t duplicate_ids;
    float    track_length_ratio;  /**< Actual/nominal (>1.0 = long track) */
    float    density_deviation;   /**< Deviation from expected density */
    float    jitter_rms;
    bool     has_half_track;
    bool     has_custom_sync;
} ufm_c64_track_metrics_t;

/** Single protection detection hit */
typedef struct {
    ufm_c64_prot_type_t type;
    uint8_t  track;
    uint8_t  side;
    uint8_t  sector;
    uint8_t  confidence;     /**< 0-100 */
    uint32_t offset;
    uint32_t length;
    char     description[128];
} ufm_c64_prot_hit_t;

/** Full analysis report */
typedef struct {
    int      confidence_0_100;
    uint32_t hits_written;
    char     summary[256];
    char     scheme_name[64];
    ufm_c64_prot_type_t primary_scheme;
} ufm_c64_prot_report_t;

/** Get human-readable name for a protection type */
const char *ufm_c64_prot_type_name(ufm_c64_prot_type_t type);

#ifdef __cplusplus
}
#endif
#endif
