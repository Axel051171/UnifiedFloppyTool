/**
 * @file ufm_c64_metrics.h
 * @brief Per-track protection metrics extracted from raw C64 GCR bitstreams
 *
 * Closes the gap documented as PROT-2: `ufm_c64_prot_analyze()` consumes
 * `ufm_c64_track_metrics_t` per track, but nothing in the tree ever produced
 * one, so every C64 protection scheme was unreachable regardless of how good
 * the classification was.
 *
 * Input is a raw GCR track image as stored in G64 (and as delivered by the
 * G64 plugin in `uft_track_t::raw_data`). Everything is derived from the
 * bitstream itself — no heuristics with invented thresholds.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFM_C64_METRICS_H
#define UFM_C64_METRICS_H

#include "uft/protection/ufm_c64_protection_taxonomy.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Speed zone is unknown and should be derived from the track number. */
#define UFM_C64_SPEED_ZONE_AUTO (-1)

/**
 * @brief Nominal track capacity in bytes for a 1541 speed zone
 *
 * Values are the standard 1541 zone capacities (zone 3 = tracks 1-17 down to
 * zone 0 = tracks 31-35); a VICE-written G64 reproduces them exactly, which is
 * what the reference test pins.
 *
 * @param zone Speed zone 0..3
 * @return Nominal capacity in bytes, or 0 for an out-of-range zone
 */
uint32_t ufm_c64_zone_nominal_bytes(int zone);

/**
 * @brief Speed zone for a standard 1541 track number
 * @param track Track number, 1-based
 * @return Zone 0..3, or -1 when the track is outside 1..42
 */
int ufm_c64_zone_for_track(int track);

/**
 * @brief Extract protection metrics from one raw GCR track image
 *
 * Derived directly and only from the bitstream:
 *   - `bitcell_count`, `track_length_ratio` (actual/nominal for the zone)
 *   - `sync_count`, `max_sync_run_bits` (sync = >= 10 consecutive 1-bits at any
 *     bit position, the 1541 hardware sync condition — definition (B) in
 *     include/uft/formats/c64/uft_gcr_ops.h). This is NOT the same count as
 *     gcr_count_syncs_bytealigned(), which requires byte alignment and only 9
 *     one-bits; the two agree on well-formed disks and diverge by up to +-4 per
 *     track on protected ones. See KNOWN_ISSUES FMT-14.
 *   - `sector_count` (GCR block id 0x08 = header), `duplicate_ids`
 *   - `bad_gcr_count` (5-bit groups outside the 16-code GCR alphabet)
 *   - `has_half_track` / `is_half_track` / `track_x2`, `has_meaningful_data`
 *   - `has_custom_sync`: the track carries sync marks but none of them
 *     introduces a standard 1541 header block, and it is not a killer track.
 *     This is the condition the reference tool (nibtools) calls a "track
 *     w/non-standard headers"; killer tracks (sync end to end, nibtools
 *     BM_FF_TRACK) are deliberately excluded, since nibtools keeps the two
 *     apart as well. A normally formatted track always yields 17..21 headers
 *     and therefore never sets this.
 *
 * NOT set by this function, on purpose: `density_deviation`, `jitter_rms`,
 * `weak_region_*`, `revolutions`, `bitlen_*`. Those need multi-revolution flux
 * (not available from a single G64 track image) or a definition this project
 * cannot ground in an authoritative source — and inventing one is exactly the
 * failure mode documented in KNOWN_ISSUES PROT-3. They stay zero/false so a
 * caller cannot mistake a guess for a measurement.
 *
 * @param gcr        Raw GCR bitstream, MSB first
 * @param gcr_len    Length of @p gcr in bytes
 * @param track_x2   Half-track index (track*2 for a whole track, odd = half)
 * @param speed_zone Speed zone 0..3, or UFM_C64_SPEED_ZONE_AUTO to derive it
 * @param out        Metrics, fully zeroed before being filled
 * @return true on success, false on invalid arguments
 */
bool ufm_c64_metrics_from_gcr(const uint8_t *gcr, size_t gcr_len,
                              int track_x2, int speed_zone,
                              ufm_c64_track_metrics_t *out);

#ifdef __cplusplus
}
#endif

#endif /* UFM_C64_METRICS_H */
