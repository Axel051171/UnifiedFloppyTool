/**
 * @file uft_protection_probe.c
 * @brief Implementation of the content-based protection-anomaly scan.
 *
 * See include/uft/analysis/uft_protection_probe.h for the contract and the
 * forensic rationale. This unit only READS; it never mutates the source and
 * never fabricates data — an image with no anomalies reports zeros.
 */

#include "uft/analysis/uft_protection_probe.h"
#include "uft/flux/uft_scp_parser.h"
#include "uft/flux/uft_kryoflux.h"

#include <string.h>

/* Divergence beyond this (ns) at a matching transition index counts as weak.
 * 75ns = 3x the 25ns SCP tick — three quantisation steps, far above rounding
 * noise, so identical revolutions of stable data never trip it while a
 * genuine fuzzy/weak region does. */
#define UFT_WEAK_TOLERANCE_NS 75u

/* A track is weak if any revolution disagrees with revolution 0 — either a
 * differing transition count (bits appear/vanish) or a matching-index timing
 * divergence beyond the tolerance.
 *
 * PRECISION LIMIT (important, honest): this compares revolutions POSITIONALLY
 * (flux index i of rev0 vs rev r). On real hardware, motor-speed jitter makes
 * revolutions drift so their transition counts differ by ~0.5% and the same
 * index maps to a physically different spot — so this heuristic OVER-reports:
 * it flags most real multi-revolution captures, not only genuinely weak ones.
 * That is deliberately the safe direction — it never marks a weak capture as
 * clean, so the export warning is never suppressed for a disk that could carry
 * weak bits (see the dispatch logic in uft_format_convert_dispatch.c: the
 * WEAK_BITS entry is dropped ONLY when this returns weak_track_count == 0).
 * It is NOT a precise weak-bit count. Sub-track weak-region precision needs
 * bit-cell decoding + inter-revolution alignment (the DeepRead multi-rev
 * fusion path), which is out of this lightweight probe's scope. Validated on
 * aligned synthetic flux only; real-disk precision requires a ground-truth
 * corpus (KNOWN_ISSUES FMT-9). */
static bool track_is_weak(const uft_scp_track_data_t *t) {
    if (t->revolution_count < 2) return false;
    const uft_scp_rev_data_t *r0 = &t->revolutions[0];
    for (uint8_t r = 1; r < t->revolution_count; r++) {
        const uft_scp_rev_data_t *rr = &t->revolutions[r];
        if (rr->flux_count != r0->flux_count) return true;
        if (!r0->flux_data || !rr->flux_data) continue;
        for (uint32_t i = 0; i < r0->flux_count; i++) {
            uint32_t a = r0->flux_data[i], b = rr->flux_data[i];
            uint32_t d = (a > b) ? (a - b) : (b - a);
            if (d > UFT_WEAK_TOLERANCE_NS) return true;
        }
    }
    return false;
}

uft_error_t uft_protection_probe_scp(const uint8_t *data, size_t size,
                                     uft_protection_summary_t *out) {
    if (!data || !out) return UFT_ERR_NULL_POINTER;
    memset(out, 0, sizeof(*out));

    bool any_track = false;
    for (int trk = 0; trk < UFT_SCP_MAX_TRACKS; trk++) {
        uft_scp_track_data_t td;
        memset(&td, 0, sizeof(td));
        if (uft_scp_read_track_memory(data, size, trk, &td) != UFT_SCP_OK)
            continue;                       /* absent/invalid track — skip */
        if (td.revolution_count == 0) {
            uft_scp_free_track(&td);
            continue;
        }

        any_track = true;
        out->track_count++;
        if (td.revolution_count > out->max_revolutions)
            out->max_revolutions = td.revolution_count;
        if (td.revolution_count > 1)
            out->has_multi_revolution = true;
        for (uint8_t r = 0; r < td.revolution_count; r++)
            out->total_flux_transitions += td.revolutions[r].flux_count;
        if (track_is_weak(&td))
            out->weak_track_count++;

        uft_scp_free_track(&td);
    }

    out->has_weak_regions = (out->weak_track_count > 0);
    out->weak_detection_reliable = true;   /* SCP revs are pre-split + aligned */
    if (!any_track) return UFT_ERR_CORRUPTED;   /* no readable track at all */
    return UFT_OK;
}

uft_error_t uft_protection_probe_kryoflux(const uint8_t *data, size_t size,
                                          uft_protection_summary_t *out) {
    if (!data || !out) return UFT_ERR_NULL_POINTER;
    memset(out, 0, sizeof(*out));

    uft_kf_stream_t stream;
    if (uft_kf_init(&stream) != UFT_UFT_KF_STATUS_OK)
        return UFT_ERR_CORRUPTED;
    /* A stream missing its StreamEnd block (UFT_UFT_KF_STATUS_MISSING_END)
     * still decoded valid flux — accept anything that yielded transitions,
     * reject only when nothing decoded. */
    (void)uft_kf_decode(&stream, data, size);
    if (stream.flux_count == 0) {
        uft_kf_free(&stream);
        return UFT_ERR_CORRUPTED;
    }

    /* One stream = one track. Robust signals only. */
    out->track_count = 1;
    out->total_flux_transitions = stream.flux_count;
    /* N index pulses delimit N-1 complete revolutions; clamp to >=1 since the
     * captured flux is at least one (possibly partial) revolution. */
    uint32_t revs = (stream.index_count > 1) ? (stream.index_count - 1) : 1;
    out->max_revolutions = revs;
    out->has_multi_revolution = (revs > 1);

    /* Weak detection from a raw unaligned stream is NOT reliable here — leave
     * weak_track_count 0 and mark it unreliable so the caller keeps the
     * conservative weak-bit warning instead of suppressing it. */
    out->has_weak_regions = false;
    out->weak_detection_reliable = false;

    uft_kf_free(&stream);
    return UFT_OK;
}
