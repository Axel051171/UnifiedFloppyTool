/*
 * uft_c64_rapidlok.h
 *
 * C64 Rapidlok preservation-analysis helpers.
 *
 * What this is:
 * - A small, *clean-room* analysis module to identify Rapidlok traits in a dump
 *   and to provide "what to capture" guidance (sync length sensitivity, key track).
 *
 * What this is NOT:
 * - Not a cracker.
 * - Not a bypass.
 * - Not a full emulator.
 *
 * Source reference (behavioural description):
 * - Pete Rittwage, "Rapidlok" page (updated 2010-04-04)
 *   https://rittwage.com/c64pp/dp.php?pg=rapidlok (accessed 2025-12-26)
 *
 * Design:
 * - Works on already-decoded per-track "GCR-ish byte stream" plus per-track sync-run metrics.
 *   (In UFM this would come from your flux decoder / G64 importer.)
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_c64_rapidlok.c
 */

#ifndef UFT_C64_RAPIDLOK_H
#define UFT_C64_RAPIDLOK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Callback API so this module stays independent from your container formats. */
typedef struct uft_rl_track_view {
    /* 1..42 typical for 1541; may include half-tracks expressed as 34.5 etc via index param */
    int   track_x2;     /* track*2 (so 34.5 => 69). */
    const uint8_t* gcr; /* decoded bytes, INCLUDING gaps (if you keep them) */
    size_t gcr_len;

    /* sync run lengths measured in bits (best-effort; derive from raw bitstream if possible).
     * - start_sync_bits: first sync mark at start of track (expected ~320 bits)
     * - sector0_sync_bits: sync directly before sector 0 header (expected ~480 bits)
     * If unknown, set to 0.
     */
    uint32_t start_sync_bits;
    uint32_t sector0_sync_bits;

    /* Revolutions captured for this track (multi-rev helps stability/weak-bit analysis). */
    uint8_t revolutions;
} uft_rl_track_view_t;

typedef struct uft_rl_observation {
    /* key track is 36 (72 in x2 units). */
    bool key_track36_present;

    /* gap contains bad GCR ($00) bytes */
    bool gap_has_bad_gcr00;

    /* sync sensitivity indicators */
    bool start_sync_near_320;
    bool sector0_sync_near_480;

    /* if we can observe: track34/35 special sync check (some variants) */
    bool trk34_35_sync_sensitive;

    /* multi-rev best practice */
    bool has_multi_rev_capture;

    /* Overall score and a textual summary */
    int  confidence_0_100;
    char summary[512];
} uft_rl_observation_t;

/* Analyze a set of tracks (full disk or partial) for Rapidlok traits.
 *
 * tracks: array of track views (can be just a few tracks).
 * out: filled with indicators and a human-readable summary.
 */
bool uft_rl_analyze(const uft_rl_track_view_t* tracks, size_t track_count,
                    uft_rl_observation_t* out);

/* Utility: check if a value is within +/- tolerance. */
static inline bool uft_rl_within(uint32_t v, uint32_t target, uint32_t tol){
    return (v >= target - tol) && (v <= target + tol);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UFT_C64_RAPIDLOK_H */
