/*
 * uft_c64_rapidlok6.h
 *
 * RapidLok 6 (C64) preservation-oriented track-structure analyzer.
 *
 * What you get:
 * - Clean-room heuristic detection of RapidLok 6 on a decoded "GCR byte stream"
 *   for individual tracks (plus optional sync-run metrics).
 * - A simple per-track structural report: presence of the $7B extra sector,
 *   $52 DOS reference header, $75 data headers, $6B data blocks, and parity hints.
 * - Guidance fields for "professional standard" capture (multi-rev, track 36 key track).
 *
 * What you do NOT get:
 * - No patching, no bypass, no cracking logic.
 *
 * Source reference (structural details / markers / sector counts / bitrates):
 * - RL6 Handbook, "RapidLok Track Information" and related sections. (RL6Handbook/I_TRACKS.HTM)
 *   Provided by user as RL6Handbook.zip.
 *
 * Integration idea in UnifiedFloppyTool:
 * - After you decode flux -> GCR-like bytes (or load a .g64), call uft_rl6_analyze_track()
 *   per track and aggregate. This lets you:
 *   - Flag disks that need multi-rev capture
 *   - Identify the key track (36) as mandatory
 *   - Report if a dump lost the extra sector or sync structure
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_c64_rapidlok6.c
 */

#ifndef UFT_C64_RAPIDLOK6_H
#define UFT_C64_RAPIDLOK6_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Marker bytes described in RL6 handbook */
#define UFT_RL6_MARK_EXTRA   0x7B  /* "extra sector" (starts with $55 then mostly $7B) */
#define UFT_RL6_MARK_DOSREF  0x52  /* DOS reference sector header */
#define UFT_RL6_MARK_HDR     0x75  /* RL data sector header */
#define UFT_RL6_MARK_DATA    0x6B  /* RL data sector block */

/* Track-group expectations (from RL6 handbook):
 * - Tracks 1-17 : 12 data sectors @ ~307692 bit/s
 * - Track 18   : typically DOS / loader track (special; not RL format like others)
 * - Tracks 19-35: 11 data sectors @ ~285714 bit/s
 * - Track 36   : key track (contains keys to validate extra sector lengths/contents)
 */
typedef enum uft_rl6_track_group {
    UFT_RL6_TRK_UNKNOWN = 0,
    UFT_RL6_TRK_1_17,
    UFT_RL6_TRK_19_35,
    UFT_RL6_TRK_18_SPECIAL,
    UFT_RL6_TRK_36_KEY
} uft_rl6_track_group_t;

/* Caller-provided per-track view. */
typedef struct uft_rl6_track_view {
    int track_num;              /* 1..42 (1541) */
    const uint8_t* bytes;       /* decoded byte stream (GCR-decoded or "raw track bytes") */
    size_t bytes_len;

    /* Optional metrics:
     * - start_sync_ff_run: length in bytes of the very first 0xFF sync run on track.
     * - dosref_sync_ff_run: length in bytes of the 0xFF sync run before the DOS reference header.
     * - first_data_hdr_sync_ff_run: length in bytes of the sync run before first $75 header
     * If unknown, set to 0.
     */
    uint16_t start_sync_ff_run;
    uint16_t dosref_sync_ff_run;
    uint16_t first_data_hdr_sync_ff_run;

    /* revolutions captured for this track (>=3 is preferred for weak-bit / stability checks) */
    uint8_t revolutions;
} uft_rl6_track_view_t;

typedef struct uft_rl6_track_report {
    uft_rl6_track_group_t group;

    bool has_extra_sector_7b;
    bool has_dosref_52;
    bool has_hdr_75;
    bool has_data_6b;

    /* Heuristic: if many 0x6B blocks exist, parity embedded bytes should make random bitflips noticeable.
       We cannot validate parity without a full RL6 decoder, so this is advisory only. */
    bool suggests_parity_present;

    /* Sync expectations from RL6 handbook (approx; allow tolerance):
       - start sync: ~0x14 bytes (20x 0xFF)
       - sync before DOS ref header: ~0x29 bytes (41x 0xFF)
       - sync before first data header after DOS ref: ~0x3E bytes (62x 0xFF)
     */
    bool start_sync_reasonable;
    bool dosref_sync_reasonable;
    bool first_data_sync_long;

    /* Confidence score for "this track looks RL6-formatted" */
    int confidence_0_100;

    char summary[512];
} uft_rl6_track_report_t;

/* Analyze a single track view and fill report.
 * Returns false on invalid args.
 */
bool uft_rl6_analyze_track(const uft_rl6_track_view_t* tv, uft_rl6_track_report_t* out);

/* Helpers */
uft_rl6_track_group_t uft_rl6_group_for_track(int track_num);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UFT_C64_RAPIDLOK6_H */
