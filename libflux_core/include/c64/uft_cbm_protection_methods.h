/*
 * uft_cbm_protection_methods.h
 *
 * Commodore 1541 / C64 copy-protection methods taxonomy + preservation detectors.
 *
 * Source (method descriptions):
 * - Peter Rittwage, "Protection Methods" (CBM)
 *   https://rittwage.com/floppy/dp.php?pg=protection_cbm citeturn2view0
 *
 * IMPORTANT (safety / scope):
 * - This module is strictly *preservation-oriented*:
 *   - classify protection traits seen in a disk capture (flux/bitstream/nibble)
 *   - recommend capture settings (multi-rev, include half-tracks, preserve gaps/sync)
 * - It does NOT implement cracking, bypass patching, or instructions to defeat protection.
 *
 * Integration:
 * - Fill uft_cbm_track_metrics_t per track from your decoder/importer.
 * - Call uft_cbm_prot_analyze() to get a ranked list of likely methods present.
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_cbm_protection_methods.c
 */

#ifndef UFT_CBM_PROTECTION_METHODS_H
#define UFT_CBM_PROTECTION_METHODS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_cbm_prot_method {
    UFT_CBM_PROT_UNKNOWN = 0,

    /* Rittwage method list (CBM) */
    UFT_CBM_PROT_INTENTIONAL_ERRORS,
    UFT_CBM_PROT_TRACK_SKEW,
    UFT_CBM_PROT_FAT_TRACKS,
    UFT_CBM_PROT_HALF_TRACKS,
    UFT_CBM_PROT_EXTRA_TRACKS,
    UFT_CBM_PROT_CHANGED_BITRATES,
    UFT_CBM_PROT_GAP_SIGNATURES,
    UFT_CBM_PROT_LONG_SECTORS,
    UFT_CBM_PROT_CUSTOM_FORMATS,
    UFT_CBM_PROT_LONG_TRACKS,
    UFT_CBM_PROT_SYNC_COUNTING,
    UFT_CBM_PROT_TRACK_SYNCHRONIZATION,
    UFT_CBM_PROT_WEAK_BITS_UNFORMATTED,
    UFT_CBM_PROT_SIGNATURE_KEY_TRACKS,
    UFT_CBM_PROT_NO_SYNC,
    UFT_CBM_PROT_SPIRADISC_LIKE
} uft_cbm_prot_method_t;

/* Per-track metrics (keep it cheap).
 * You can compute most of these from flux:
 * - bitlen per revolution (min/max)
 * - sync-run max (longest run of sync bits or decoded 0xFF sync bytes)
 * - illegal decode events
 * - sector read error counts / checksum mismatches
 * - presence of data on half-tracks / tracks >35
 */
typedef struct uft_cbm_track_metrics {
    int track_x2;             /* track*2 (34.5 => 69) */
    uint8_t revolutions;

    /* Track length in bits across revs (0 if unknown) */
    uint32_t bitlen_min;
    uint32_t bitlen_max;

    /* Longest sync run (in bits) if known */
    uint32_t max_sync_bits;

    /* Decode health */
    uint32_t illegal_gcr_events;

    /* Error-like signals */
    uint32_t sector_crc_failures;    /* checksum/CRC failures detected by your parser */
    uint32_t sector_missing;         /* expected sector headers not found */
    uint32_t sector_count_observed;  /* number of sector-like blocks you detected */

    /* If your importer knows the density/bitrate zone or non-standard bitrate */
    bool nonstandard_bitrate;

    /* Gap / tail gap signature hints (if your byte-level view exposes it) */
    uint32_t gap_non55_bytes;  /* count of gap bytes not equal to 0x55 (coarse) */
    bool     gap_length_weird; /* gap lengths differ strongly from DOS-ish expectations */

    /* Track alignment indicators (requires index/absolute timing support; optional) */
    bool has_index_reference;     /* hardware with index available */
    bool track_alignment_locked;  /* capture suggests mastered alignment between tracks */

    /* "No sync" indicator (no sync runs meeting threshold) */
    bool no_sync_detected;

    /* Meaningful data on half-tracks / extra tracks */
    bool has_meaningful_data;
} uft_cbm_track_metrics_t;

typedef struct uft_cbm_method_hit {
    uft_cbm_prot_method_t method;
    int track_x2;            /* 0 if disk-wide */
    int confidence_0_100;
} uft_cbm_method_hit_t;

typedef struct uft_cbm_report {
    int overall_0_100;       /* “some protection method likely present” */
    size_t hits_written;
    char summary[1400];
} uft_cbm_report_t;

/* Analyze a disk (or subset) and return likely methods.
 * hits may be NULL if you only want summary/score.
 */
bool uft_cbm_prot_analyze(const uft_cbm_track_metrics_t* tracks, size_t track_count,
                          uft_cbm_method_hit_t* hits, size_t hits_cap,
                          uft_cbm_report_t* out);

const char* uft_cbm_method_name(uft_cbm_prot_method_t m);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UFT_CBM_PROTECTION_METHODS_H */
