/**
 * @file uft_forensic_types.h
 * @brief Type definitions for forensic recovery (sessions, sectors,
 *        tracks, configuration).
 *
 * Designed by reverse-engineering field accesses in v3.7.0's
 * uft_forensic_recovery.c and uft_forensic_track.c. The v3.7 release
 * shipped those .c files but never publicly declared the types they
 * use — they must have lived in a private header that was never
 * included in the source archive.
 *
 * This header provides the struct shapes those .c files require.
 *
 * STATUS (2026-04-25): full extension complete. Covers
 *   - uft_error_info_t with all fields used by v3.7 (.class,
 *     .bit_offset, .bit_length, .probability, .diagnosis,
 *     .recovery_attempted, .recovery_successful, .correction_confidence)
 *   - uft_error_class_t enum (SOFT_SINGLE_BIT, SOFT_MULTI_BIT,
 *     HARD_WEAK_BITS)
 *   - sector->quality as uft_sector_quality_t sub-struct with
 *     overall, data, checksum, bits_certain, bits_uncertain,
 *     passes_used, corrections_applied
 * Allows uft_forensic_recovery.c + uft_forensic_track.c to compile
 * against this header without modification.
 *
 * Part of the v3.7 → v4.1 module restoration effort. See
 * docs/HARDENING_AUDIT.md and per-batch DEFERRED.md files.
 */
#ifndef UFT_FORENSIC_TYPES_H
#define UFT_FORENSIC_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Configuration
 *===========================================================================*/

/**
 * Forensic-recovery tuning knobs. Use uft_forensic_config_default() or
 * uft_forensic_config_paranoid() to populate; do not zero-init.
 */
typedef struct uft_forensic_config {
    /* Multi-pass strategy. */
    int   min_passes;                    /**< minimum read passes */
    int   max_passes;                    /**< maximum read passes */
    int   passes_after_success;          /**< extra passes after first OK */

    /* Confidence thresholds (0.0..1.0). */
    float accept_threshold;              /**< above → accept sector */
    float retry_threshold;               /**< below → keep retrying */
    float manual_review_threshold;       /**< below → flag for human */

    /* Bit-level correction. */
    bool  attempt_single_bit_correction;
    bool  attempt_multi_bit_correction;
    int   max_corrections_per_sector;

    /* Hint sources. */
    bool  use_format_hints;              /**< exploit known format layout */
    bool  use_interleave_hints;          /**< exploit interleave pattern */
    bool  use_cross_track_hints;         /**< exploit neighbour tracks */

    /* Preservation flags — Prinzip 1 controls. */
    bool  preserve_all_passes;           /**< keep every pass, not just best */
    bool  preserve_flux_timing;
    bool  preserve_weak_bit_map;

    /* Diagnostics. */
    void *audit_log;                     /**< opaque sink, may be NULL */
    int   verbosity;                     /**< 0=silent, 5=trace */
} uft_forensic_config_t;

/*===========================================================================
 * Error classification + record
 *===========================================================================*/

typedef enum {
    UFT_ERROR_NONE             = 0,
    UFT_ERROR_SOFT_SINGLE_BIT,    /**< 1 bit error, recoverable via CRC */
    UFT_ERROR_SOFT_MULTI_BIT,     /**< 2-3 bit errors, recoverable */
    UFT_ERROR_HARD_WEAK_BITS,     /**< intentional weak bits (protection) */
    UFT_ERROR_HARD_SECTOR_LOST,   /**< sector unrecoverable */
    UFT_ERROR_FORMAT_ANOMALY,     /**< format-spec violation */
    UFT_ERROR_TIMING_ANOMALY,     /**< unexpected timing */
} uft_error_class_t;

typedef struct uft_error_info {
    uft_error_class_t  class;            /**< category of error */
    uint32_t           bit_offset;       /**< bit position within sector */
    uint32_t           bit_length;       /**< how many bits affected */
    float              probability;      /**< confidence in classification */
    const char        *diagnosis;        /**< human-readable description */
    bool               recovery_attempted;
    bool               recovery_successful;
    float              correction_confidence;
} uft_error_info_t;

/* Legacy alias — v3.7 used uft_forensic_sector_error_t in places. */
typedef uft_error_info_t uft_forensic_sector_error_t;

/*===========================================================================
 * Sector quality (sub-struct embedded in uft_forensic_sector_t)
 *===========================================================================*/

typedef struct uft_sector_quality {
    float    overall;                    /**< 0.0..1.0 — composite score */
    float    data;                       /**< 0.0..1.0 — payload confidence */
    float    checksum;                   /**< 1.0 if CRC OK, 0.0 if not */
    uint32_t bits_certain;               /**< bits with high confidence */
    uint32_t bits_uncertain;             /**< bits with low confidence */
    uint8_t  passes_used;                /**< how many passes contributed */
    uint16_t corrections_applied;        /**< total bit corrections */
} uft_sector_quality_t;

/*===========================================================================
 * Sector — recovered sector with provenance
 *===========================================================================*/

typedef struct uft_forensic_sector {
    /* Identity. */
    uint8_t  cylinder;
    uint8_t  head;
    uint16_t sector_id;
    uint16_t size;                       /**< bytes — 128/256/512/1024 */

    /* Payload. */
    uint8_t *data;                       /**< heap, owned by sector */

    /* CRC. */
    uint32_t crc_stored;                 /**< CRC as read from disk */
    uint32_t crc_computed;               /**< CRC of the recovered data */
    bool     crc_valid;                  /**< stored == computed */

    /* Quality / provenance. */
    uft_sector_quality_t quality;        /**< composite quality metrics */
    uint8_t *confidence_map;             /**< per-byte confidence, may be NULL */
    uint8_t *source_pass;                /**< per-byte: which pass produced it */
    uint32_t flags;                      /**< bitfield: weak/deleted/etc. */

    /* Errors / corrections applied. */
    uft_error_info_t *errors;
    size_t   error_count;
    size_t   error_capacity;
} uft_forensic_sector_t;

/*===========================================================================
 * Track — collection of recovered sectors
 *===========================================================================*/

typedef struct uft_forensic_track {
    uint8_t                cylinder;
    uint8_t                head;

    uft_forensic_sector_t *sectors;     /**< heap array */
    size_t                 sector_count;
    size_t                 sector_capacity;

    /* Track-level statistics. */
    uint16_t expected_sectors;          /**< from format spec */
    uint16_t found_sectors;
    uint16_t recovered_sectors;
    float    completeness;              /**< found / expected */
    float    quality_score;             /**< average sector quality */

    /* Timing. */
    uint32_t rotation_time_ms;          /**< observed rotation */
    uint32_t timing_anomalies;          /**< count of irregularities */
} uft_forensic_track_t;

/*===========================================================================
 * Session — top-level recovery context
 *===========================================================================*/

typedef struct uft_forensic_session {
    uft_forensic_config_t config;

    /* Aggregate statistics. */
    uint32_t total_sectors_expected;
    uint32_t total_sectors_found;
    uint32_t total_sectors_recovered;
    uint32_t total_sectors_perfect;     /**< CRC OK, no corrections */
    uint32_t total_sectors_partial;     /**< CRC OK with corrections */
    uint32_t total_sectors_failed;      /**< unrecoverable */
    uint32_t total_corrections;
    uint32_t total_weak_bits;

    /* Audit trail. */
    void   *audit_entries;              /**< opaque list, may be NULL */
    size_t  audit_count;

    /* Timing. */
    uint64_t start_time_ms;
    uint64_t end_time_ms;
} uft_forensic_session_t;

/*===========================================================================
 * Recover (per-sector recovery state)
 *===========================================================================*/

typedef struct uft_forensic_recover {
    uft_forensic_sector_t *target;
    uft_forensic_sector_t *passes;      /**< array of all read passes */
    size_t                 pass_count;
    size_t                 pass_capacity;
} uft_forensic_recover_t;

/*===========================================================================
 * Public API (subset implemented in uft_forensic_recovery.c +
 * uft_forensic_track.c)
 *===========================================================================*/

uft_forensic_config_t uft_forensic_config_default(void);
uft_forensic_config_t uft_forensic_config_paranoid(void);

int  uft_forensic_session_init(uft_forensic_session_t *session,
                                const uft_forensic_config_t *config);
int  uft_forensic_session_finish(uft_forensic_session_t *session);

void uft_forensic_log(uft_forensic_session_t *session,
                       int level, const char *fmt, ...);

/* Allocate a sector with `size`-byte payload + per-byte confidence
 * + per-byte source-pass + initial 32-error capacity. */
uft_forensic_sector_t *uft_forensic_sector_alloc(uint16_t size);

void uft_forensic_sector_free(uft_forensic_sector_t *sector);
int  uft_forensic_sector_add_error(uft_forensic_sector_t *sector,
                                     const uft_error_info_t *error);

int  uft_forensic_correct_sector(uft_forensic_sector_t *sector,
                                   uft_forensic_session_t *session);

/* Multi-pass sector recovery — passes is array of bitstreams. */
int  uft_forensic_recover_sector(const uint8_t **passes,
                                   const size_t *pass_bit_counts,
                                   size_t pass_count,
                                   uint16_t cylinder,
                                   uint8_t head,
                                   uint16_t sector_id,
                                   uint16_t sector_size,
                                   uft_forensic_session_t *session,
                                   uft_forensic_sector_t *out_sector);

/* Sector flag bits stored in uft_forensic_sector_t.flags. */
#define UFT_FSEC_FLAG_DATA_OK         (1u << 0)
#define UFT_FSEC_FLAG_CRC_OK          (1u << 1)
#define UFT_FSEC_FLAG_RECOVERED       (1u << 2)  /**< restored via recovery */
#define UFT_FSEC_FLAG_WEAK_BITS       (1u << 3)
#define UFT_FSEC_FLAG_DELETED         (1u << 4)
#define UFT_FSEC_FLAG_PARTIAL         (1u << 5)
#define UFT_FSEC_FLAG_CORRECTED       (1u << 6)
#define UFT_FSEC_FLAG_MANUAL_REVIEW   (1u << 7)
#define UFT_FSEC_FLAG_PROTECTED       (1u << 8)

/* Track-level recovery — feeds flux from N revolutions, populates out_track. */
int uft_forensic_recover_track(const uint64_t **flux_timestamps,
                                 const size_t *flux_counts,
                                 size_t revolution_count,
                                 uint16_t cylinder,
                                 uint8_t head,
                                 const void *format_hint,
                                 uft_forensic_session_t *session,
                                 uft_forensic_track_t *out_track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORENSIC_TYPES_H */
