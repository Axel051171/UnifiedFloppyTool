#pragma once
/*
 * flux_window.h — Track-Windowing / Index-Pulse-Slicing
 *
 * Ziel:
 *  - Ein Captured-Stream kann mehrere Umdrehungen enthalten.
 *  - Index-Pulse Offsets (in Flux-Sample-Indizes) erlauben "harte" Slice-Grenzen.
 *
 * Wir schneiden NUR entlang dieser Grenzen. Kein Resampling, keine Glättung.
 */

#include <stdint.h>

#include "flux_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Slice a revolution by its index pulses.
 *
 * Input:
 *  - in->dt_ns[] is the full interval stream
 *  - in->index.offsets[] are sample indices into dt_ns (0..count)
 *
 * Output:
 *  - *out_revs points to a heap array of ufm_revolution_t (count = *out_n)
 *  - Each slice owns its own dt_ns copy.
 *  - Each slice has index.count=1 with index.offsets[0]=0 ("this slice starts at index").
 *
 * Caller must free each slice via ufm_revolution_free_contents() and then free(*out_revs).
 */
int ufm_slice_by_index(
    const ufm_revolution_t *in,
    ufm_revolution_t **out_revs,
    uint32_t *out_n);

#ifdef __cplusplus
}
#endif
