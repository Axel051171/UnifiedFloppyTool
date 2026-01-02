/*
 * uft_c64_scheme_detect.h
 *
 * C64 disk protection scheme detector (preservation-oriented).
 *
 * Scope:
 * - Detect/flag likely *schemes* (Rapidlok family, GEOS gap data, EA "fat track", Vorpal, V-MAX)
 *   from per-track metrics/markers produced by a flux->bitstream->decoder pipeline.
 *
 * Safety:
 * - No cracking/bypass/patching. This module only helps:
 *   1) Identify "what you are looking at"
 *   2) Recommend capture settings (multi-rev, half-tracks, preserve gaps, do not "repair" weak bits)
 *
 * Source references:
 * - GEOS gap-data protection description & track 21 rule set:
 *   https://www.c64-wiki.de/wiki/GEOS-Kopierschutz citeturn2view3
 * - Vorpal is a named C64 protection/loader family (e.g., California Games):
 *   https://codetapper.com/c64/copy_protection/?id=4 citeturn14view1
 * - V-MAX is a named C64 copy-protection scheme discussed in preservation community:
 *   https://www.c64copyprotection.com/category/copy-protection/page/2/ citeturn16search18
 *   https://groups.google.com/g/comp.sys.cbm/c/gATfQPhxMs4 citeturn16search17
 *
 * Integration:
 * - Feed uft_c64_track_sig_t[] collected from your decoder/importer.
 *   For some schemes (GEOS/RL6) you may additionally run the dedicated per-scheme modules
 *   created earlier (uft_c64_geos_gap_protection, uft_c64_rapidlok6, etc.).
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_c64_scheme_detect.c
 */

#ifndef UFT_C64_SCHEME_DETECT_H
#define UFT_C64_SCHEME_DETECT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_c64_scheme {
    UFT_C64_SCHEME_UNKNOWN = 0,

    /* concrete named schemes/families */
    UFT_C64_SCHEME_GEOS_GAPDATA,
    UFT_C64_SCHEME_RAPIDLOK,
    UFT_C64_SCHEME_RAPIDLOK2,
    UFT_C64_SCHEME_RAPIDLOK6,
    UFT_C64_SCHEME_EA_FATTRACK,
    UFT_C64_SCHEME_VORPAL,
    UFT_C64_SCHEME_VMAX
} uft_c64_scheme_t;

/* Lightweight per-track signature inputs.
 * You can fill these from:
 * - flux decode
 * - G64 importer
 * - nib/stream formats
 */
typedef struct uft_c64_track_sig {
    int      track_x2;              /* track*2 (34.5 => 69) */
    uint8_t  revolutions;           /* revs captured */

    /* Bit length range across revolutions (if you have it) */
    uint32_t bitlen_min;
    uint32_t bitlen_max;

    /* "weak bits" estimate: unstable bits across revs (optional) */
    uint32_t weak_bits_total;
    uint32_t weak_bits_max_run;

    /* Decoder error counters (optional) */
    uint32_t illegal_gcr_events;

    /* Sync metrics (optional) */
    uint32_t max_sync_run_bits;

    /* Marker hints from byte-level decoding (optional).
     * For C64 schemes, these are common marker bytes seen in decoded streams:
     * - 0x52, 0x75, 0x6B, 0x7B for RapidLok family (if your decoder emits them)
     * - 0x00 in gaps for RapidLok (bad GCR bytes) (again: decoder-dependent)
     */
    uint32_t count_52;
    uint32_t count_75;
    uint32_t count_6b;
    uint32_t count_7b;
    uint32_t count_00;
} uft_c64_track_sig_t;

typedef struct uft_c64_scheme_hit {
    uft_c64_scheme_t scheme;
    int confidence_0_100;
} uft_c64_scheme_hit_t;

typedef struct uft_c64_scheme_report {
    uft_c64_scheme_hit_t hits[8];
    size_t hits_n;

    char summary[1024];
} uft_c64_scheme_report_t;

/* Detect likely schemes based on metrics/markers.
 * This is heuristic: "unknown" is a valid outcome.
 */
bool uft_c64_detect_schemes(const uft_c64_track_sig_t* tracks, size_t track_count,
                           uft_c64_scheme_report_t* out);

/* name helper */
const char* uft_c64_scheme_name(uft_c64_scheme_t s);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UFT_C64_SCHEME_DETECT_H */
