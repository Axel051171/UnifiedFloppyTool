/*
 * uft_c64_geos_gap_protection.h
 *
 * GEOS (C64/1541) "Lückendaten" copy-protection – preservation helpers.
 *
 * Summary (from C64-Wiki):
 * - GEOS protected disks are standard Commodore DOS *except* for special gap bytes ("GEOS-Lückendaten").
 * - The protection check reads the gap bytes on track 21:
 *     * all 19 header pre-gaps (sector header caps)
 *     * the first 18 data block gaps (data caps)  => 37 checks total
 * - Allowed bytes in those gaps: $55 or $67 only.
 * - Critical detail: each checked gap must end with a $67 byte right before the next SYNC,
 *   otherwise the check can "spill" into $FF sync bytes and fail. citeturn2view3
 *
 * What this module provides:
 * - Detection/validation of GEOS gap rules on a decoded track stream.
 * - A conservative "reconstruction helper" that can rewrite gap bytes to the allowed set
 *   for restoration/preservation workflows (disabled by default in your app UI).
 *
 * What this module does NOT provide:
 * - No cracking, bypass patching, or step-by-step circumvention.
 * - It operates on track images you already own/captured to validate preservation quality.
 *
 * Input model:
 * - Provide a per-track byte stream where:
 *     * SYNC marks are represented by runs of 0xFF (common after GCR->byte decoding),
 *     * gap bytes are present as raw bytes in between blocks.
 * - If your decoder represents SYNC differently, you can adapt the "is_sync_byte" function.
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_c64_geos_gap_protection.c
 */

#ifndef UFT_C64_GEOS_GAP_PROTECTION_H
#define UFT_C64_GEOS_GAP_PROTECTION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uft_geos_gap_rule_cfg {
    /* How many consecutive 0xFF bytes count as "SYNC run start". */
    uint8_t sync_run_min;

    /* Allowed gap bytes (default: 0x55 and 0x67). */
    uint8_t allowed_a;
    uint8_t allowed_b;

    /* Require last non-0xFF byte before each SYNC run to be 0x67 (default true). */
    bool require_trailing_67;

    /* Track number used for GEOS check (default 21, per article). */
    int track_number;
} uft_geos_gap_rule_cfg_t;

typedef struct uft_geos_gap_findings {
    int track_number;

    /* Number of gap regions detected (based on SYNC run splitting). */
    uint32_t gaps_found;

    /* Number of gap regions that violate allowed bytes. */
    uint32_t gaps_bad_bytes;

    /* Number of gap regions that fail trailing 0x67 rule. */
    uint32_t gaps_bad_trailing;

    /* Total count of offending bytes (not 0x55/0x67). */
    uint32_t bad_byte_count;

    /* Confidence score (0..100) that the track meets GEOS gap protection rules. */
    int confidence_0_100;

    char summary[768];
} uft_geos_gap_findings_t;

/* Validate GEOS gap rules on a track byte stream.
 *
 * track_bytes: decoded bytes for the track (ideally full track including gaps and sync runs)
 * cfg: config (if NULL, defaults used)
 * out: findings
 *
 * Returns false on invalid args.
 */
bool uft_geos_gap_validate_track(const uint8_t* track_bytes, size_t track_len,
                                 const uft_geos_gap_rule_cfg_t* cfg,
                                 uft_geos_gap_findings_t* out);

/* Optional: reconstruct gap bytes *in-place*:
 * - For each detected gap region, rewrite all gap bytes to 0x55,
 *   and force the last byte before SYNC to 0x67 (if enabled).
 *
 * This is meant for *restoration/preservation* of owned images when exporting to G64-like streams.
 * Your GUI should make this an explicit "repair for GEOS compatibility" action.
 *
 * Returns number of gap regions modified.
 */
uint32_t uft_geos_gap_rewrite_inplace(uint8_t* track_bytes, size_t track_len,
                                      const uft_geos_gap_rule_cfg_t* cfg);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UFT_C64_GEOS_GAP_PROTECTION_H */
