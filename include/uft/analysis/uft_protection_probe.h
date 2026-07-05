/**
 * @file uft_protection_probe.h
 * @brief Content-based protection-anomaly scan for lossy-export warnings.
 *
 * The conversion preflight (src/core/uft_preflight.c) decides OK/ABORT from
 * the static format-pair round-trip matrix, and the sidecar writer emits
 * generic loss entries with count=0 — so an unprotected SCP and a heavily
 * copy-protected SCP produce the SAME "weak bits might be lost" warning.
 *
 * This probe closes that gap for the flux SOURCE side (Phase-4 Klasse-3 work
 * package): it scans the actual image bytes and reports what protection data
 * THIS file really carries, so the export warning can state the real numbers
 * — or correctly stay silent about weak bits when there are none. Both
 * directions serve DESIGN_PRINCIPLES: never hide a real loss, never invent
 * one that did not happen ("Keine erfundenen Daten").
 */
#ifndef UFT_PROTECTION_PROBE_H
#define UFT_PROTECTION_PROBE_H

#include "uft/uft_error.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Protection-relevant content summary of a flux source.
 *
 * All counts are derived from the bytes, not from the format class. A field
 * of 0 means the scan found none of that artifact in this image.
 */
typedef struct uft_protection_summary {
    uint32_t track_count;             ///< tracks carrying flux (robust)
    uint32_t max_revolutions;         ///< highest revolution count over all tracks (robust)
    uint32_t weak_track_count;        ///< tracks with cross-revolution divergence — COARSE,
                                      ///< over-reports on real disks (motor jitter), never
                                      ///< under-reports; NOT a precise weak-bit count
    uint64_t total_flux_transitions;  ///< sum of transitions (timing a sector target drops)
    bool     has_multi_revolution;    ///< any track with >1 revolution (robust)
    bool     has_weak_regions;        ///< weak_track_count > 0 (coarse over-report, safe direction)
} uft_protection_summary_t;

/**
 * @brief Scan an in-memory SCP image for protection-relevant anomalies.
 *
 * Heuristic (documented, track-granular — not sub-track region precision):
 * a track is "weak" when its revolutions disagree, either by a differing
 * flux-transition count between revolutions (transitions appear/vanish =
 * fuzzy bits) or by matching-index timings diverging beyond an internal
 * tolerance well above the 25ns SCP quantisation floor (so stable data never
 * trips it). Bounds-safe: delegates to the parser's validated memory reader,
 * so a malformed/truncated SCP yields an error, never an out-of-bounds read.
 *
 * @param data  SCP file bytes
 * @param size  Length of @p data
 * @param out   Populated summary (zeroed first)
 * @return UFT_OK on success (even with zero anomalies), UFT_ERR_NULL_POINTER
 *         on bad args, UFT_ERR_CORRUPTED if no track could be read.
 */
uft_error_t uft_protection_probe_scp(const uint8_t *data, size_t size,
                                     uft_protection_summary_t *out);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROTECTION_PROBE_H */
