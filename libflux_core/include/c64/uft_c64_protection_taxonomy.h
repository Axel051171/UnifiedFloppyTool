/*
 * uft_c64_protection_taxonomy.h
 *
 * C64 disk protection *preservation* taxonomy + heuristic detectors.
 *
 * Why:
 * - The provided archive "C64_Software_Protection_Revealed_jp2.zip" appears to be a scanned
 *   book/article as JPEG2000 page images (JP2). It's useful as reference, but it is not
 *   source code we can "extract".
 * - What we *can* do for UnifiedFloppyTool: provide a clean-room, professional module that
 *   classifies common C64/1541 protection traits from flux/bitstream captures:
 *     - weak bits / fuzzy areas (variance across revolutions)
 *     - long/short tracks (track length anomalies)
 *     - half-track data presence
 *     - invalid GCR / illegal nibbles
 *     - sync-length sensitivity (very long sync runs)
 *     - intentional checksum/ID anomalies (sector structure irregularities)
 *
 * This module does NOT:
 * - crack, bypass, patch, or provide step-by-step circumvention guidance.
 * - attempt to recreate keys.
 *
 * Integration:
 * - Feed per-track metrics from your decoder (or from G64/KF stream import):
 *   * track length in bits (per revolution)
 *   * weak-bit statistics / per-bit stability
 *   * counts of illegal GCR bytes (decoder error events)
 *   * half-track presence flags (e.g., 34.5 has meaningful data)
 *   * sync-run maxima (longest continuous "sync" region in bits)
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_c64_protection_taxonomy.c
 */

#ifndef UFT_C64_PROTECTION_TAXONOMY_H
#define UFT_C64_PROTECTION_TAXONOMY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_c64_prot_type {
    UFT_C64_PROT_NONE = 0,

    /* Preservation-significant traits */
    UFT_C64_PROT_WEAK_BITS,
    UFT_C64_PROT_LONG_TRACK,
    UFT_C64_PROT_SHORT_TRACK,
    UFT_C64_PROT_HALF_TRACK_DATA,
    UFT_C64_PROT_ILLEGAL_GCR,
    UFT_C64_PROT_LONG_SYNC,

    /* Generic "weird sectoring" (not a specific scheme) */
    UFT_C64_PROT_SECTOR_ANOMALY
} uft_c64_prot_type_t;

typedef struct uft_c64_track_metrics {
    int      track_x2;          /* track*2 (e.g. 34.5 => 69) */
    uint8_t  revolutions;       /* how many revs captured */
    uint32_t bitlen_min;        /* shortest rev in bits */
    uint32_t bitlen_max;        /* longest rev in bits */

    /* Weak-bit estimate:
     * - weak_region_bits: how many bits are unstable across revs (best-effort)
     * - weak_region_max_run: longest contiguous unstable run
     */
    uint32_t weak_region_bits;
    uint32_t weak_region_max_run;

    /* Decoder health */
    uint32_t illegal_gcr_events; /* count of illegal/undecodable symbols */

    /* Sync info */
    uint32_t max_sync_run_bits;

    /* If you already know the track is a half-track and contains meaningful data */
    bool     is_half_track;
    bool     has_meaningful_data;
} uft_c64_track_metrics_t;

typedef struct uft_c64_prot_hit {
    uft_c64_prot_type_t type;
    int track_x2;
    int severity_0_100;
} uft_c64_prot_hit_t;

typedef struct uft_c64_prot_report {
    int confidence_0_100; /* "some protection traits likely present" */

    /* hits is a caller-provided array; module writes up to hits_cap */
    size_t hits_written;

    char summary[1024];
} uft_c64_prot_report_t;

/* Analyze a set of track metrics.
 *
 * tracks: per-track metrics (can be partial)
 * hits: output array (optional). If hits==NULL or hits_cap==0, only summary/confidence is produced.
 */
bool uft_c64_prot_analyze(const uft_c64_track_metrics_t* tracks, size_t track_count,
                          uft_c64_prot_hit_t* hits, size_t hits_cap,
                          uft_c64_prot_report_t* out);

/* Convenience: human-readable name for type. */
const char* uft_c64_prot_type_name(uft_c64_prot_type_t t);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UFT_C64_PROTECTION_TAXONOMY_H */
