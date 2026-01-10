#pragma once
/*
 * UFT Flux Core — minimal, portable flux stream model
 *
 * Goal:
 *  - represent flux time deltas (ticks) without guessing format-specific semantics
 *  - provide analysis and a deterministic "raw-bits" projection for TrackGrid visualization
 *
 * This is NOT a full PLL/MFM decoder yet. It is an honest base layer.
 */
#include <stddef.h>
#include <stdint.h>
#include "uft/core/uft_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uft_flux_stream {
    uint32_t *dt_ns;   /* time deltas in nanoseconds between flux transitions */
    uint32_t count;
    uint32_t nominal_rpm;        /* 0 if unknown */
    uint32_t nominal_bitrate;    /* 0 if unknown */
} uft_flux_stream_t;

typedef struct uft_flux_stats {
    uint32_t min_ns, max_ns;
    uint64_t sum_ns;
    double   mean_ns;
    double   jitter_ratio; /* (max-min)/mean */
} uft_flux_stats_t;

void     uft_flux_init(uft_flux_stream_t *fs);
void     uft_flux_free(uft_flux_stream_t *fs);

uft_rc_t uft_flux_append(uft_flux_stream_t *fs, uint32_t dt_ns, uft_diag_t *diag);
uft_rc_t uft_flux_compute_stats(const uft_flux_stream_t *fs, uft_flux_stats_t *out, uft_diag_t *diag);

uft_rc_t uft_flux_project_to_rawbits(const uft_flux_stream_t *fs,
                                    uint32_t cell_ns,
                                    uint8_t **out_bits, uint32_t *out_bitlen,
                                    uft_diag_t *diag);

#ifdef __cplusplus
}
#endif
