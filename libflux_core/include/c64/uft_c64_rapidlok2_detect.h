/*
 * uft_c64_rapidlok2_detect.h
 *
 * RapidLok2 (C64) preservation-oriented detection helpers.
 *
 * Why this exists:
 * - The provided text (pasted.txt) describes *patches to disable protection checks*.
 *   We do NOT implement patching or bypass logic.
 * - What we CAN do safely: detect likely RapidLok2 disk structure in an image/bitstream
 *   and tell the user what to capture for preservation-grade dumps (multi-rev, sync runs,
 *   special sectors like $7B/$75/$6B patterns).
 *
 * Input model:
 * - This module works on a decoded per-track byte stream (GCR-decoded or "raw track bytes")
 *   plus optional per-track sync-run metrics. It does NOT require an emulator.
 *
 * Integration:
 * - Use with your existing UFM C64 track decoding pipeline (e.g., G64 importer).
 * - Feed it track-by-track views; it returns a structured detection report.
 *
 * Source context:
 * - The text mentions RapidLok2's special sector/header IDs ($52, $75, $6B, $7B) and
 *   alignment/sync sensitivity characteristics. fileciteturn9file0
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_c64_rapidlok2_detect.c
 */

#ifndef UFT_C64_RAPIDLOK2_DETECT_H
#define UFT_C64_RAPIDLOK2_DETECT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uft_rl2_track_view {
    int track_x2;              /* track*2 (so 18.0 => 36, 18.5 => 37) */
    const uint8_t* bytes;      /* decoded track bytes (post-decoder) */
    size_t bytes_len;

    /* Optional metrics (set 0 if unknown) */
    uint32_t max_sync_bits;    /* longest sync run seen on this track */
    uint8_t  revolutions;      /* how many revolutions captured for analysis */
} uft_rl2_track_view_t;

typedef struct uft_rl2_report {
    int confidence_0_100;

    /* Observed traits (preservation oriented) */
    bool has_52_dos_headers;   /* DOS reference header marker (common in RL) */
    bool has_75_headers;       /* RL data sector header marker */
    bool has_6b_data;          /* RL data sector marker */
    bool has_7b_extras;        /* extra sectors marker (often used by RL2) */
    bool has_unusual_sync;     /* very long sync runs detected */
    bool suggests_track18_anchor; /* track 18 contains RL2 loader routines marker-set */

    bool has_multi_rev;        /* at least one track had >=3 revs */

    char summary[768];
} uft_rl2_report_t;

/* Analyze given tracks for RapidLok2 structural traits.
 * This is a heuristic; it returns confidence + a summary.
 */
bool uft_rl2_analyze(const uft_rl2_track_view_t* tracks, size_t track_count,
                     uft_rl2_report_t* out);

/* Utility: count occurrences of a byte value in a buffer (capped). */
size_t uft_rl2_count_byte(const uint8_t* buf, size_t len, uint8_t v, size_t cap);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UFT_C64_RAPIDLOK2_DETECT_H */
