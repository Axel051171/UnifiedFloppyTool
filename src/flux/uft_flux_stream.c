#include "uft/flux/flux_stream.h"
#include <stdlib.h>

void uft_flux_init(uft_flux_stream_t *fs)
{
    if (!fs) return;
    fs->dt_ns = NULL;
    fs->count = 0;
    fs->nominal_rpm = 0;
    fs->nominal_bitrate = 0;
}

void uft_flux_free(uft_flux_stream_t *fs)
{
    if (!fs) return;
    free(fs->dt_ns);
    uft_flux_init(fs);
}

uft_rc_t uft_flux_append(uft_flux_stream_t *fs, uint32_t dt_ns, uft_diag_t *diag)
{
    if (!fs || dt_ns == 0) { uft_diag_set(diag, "flux_append: invalid dt"); return UFT_EINVAL; }
    uint32_t n = fs->count + 1u;
    uint32_t *p = (uint32_t*)realloc(fs->dt_ns, (size_t)n * sizeof(uint32_t));
    if (!p) { uft_diag_set(diag, "flux_append: OOM"); return UFT_ENOMEM; }
    fs->dt_ns = p;
    fs->dt_ns[fs->count] = dt_ns;
    fs->count = n;
    return UFT_OK;
}

uft_rc_t uft_flux_compute_stats(const uft_flux_stream_t *fs, uft_flux_stats_t *out, uft_diag_t *diag)
{
    if (!fs || !out) { uft_diag_set(diag, "flux_stats: invalid args"); return UFT_EINVAL; }
    if (fs->count == 0) { uft_diag_set(diag, "flux_stats: empty"); return UFT_EFORMAT; }

    uint32_t mn = fs->dt_ns[0], mx = fs->dt_ns[0];
    uint64_t sum = 0;
    for (uint32_t i = 0; i < fs->count; i++) {
        uint32_t v = fs->dt_ns[i];
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += v;
    }
    out->min_ns = mn;
    out->max_ns = mx;
    out->sum_ns = sum;
    out->mean_ns = (double)sum / (double)fs->count;
    out->jitter_ratio = (out->mean_ns > 0.0) ? ((double)(mx - mn) / out->mean_ns) : 0.0;
    return UFT_OK;
}

static void bit_set(uint8_t *b, uint32_t bit_index, int v)
{
    uint32_t byte = bit_index / 8u;
    uint32_t bit  = 7u - (bit_index % 8u);
    if (v) b[byte] |= (uint8_t)(1u << bit);
}

uft_rc_t uft_flux_project_to_rawbits(const uft_flux_stream_t *fs,
                                    uint32_t cell_ns,
                                    uint8_t **out_bits, uint32_t *out_bitlen,
                                    uft_diag_t *diag)
{
    if (!fs || !out_bits || !out_bitlen || cell_ns == 0) { uft_diag_set(diag, "flux_project: invalid args"); return UFT_EINVAL; }
    *out_bits = NULL; *out_bitlen = 0;
    if (fs->count == 0) { uft_diag_set(diag, "flux_project: empty"); return UFT_EFORMAT; }

    uint64_t bits_est = 0;
    for (uint32_t i = 0; i < fs->count; i++) {
        uint32_t cells = fs->dt_ns[i] / cell_ns;
        if (cells == 0) cells = 1;
        bits_est += (uint64_t)cells;
        if (bits_est > 200000000u) { uft_diag_set(diag, "flux_project: unbounded size"); return UFT_EBOUNDS; }
    }

    uint32_t bitlen = (uint32_t)bits_est;
    uint32_t bylen = (bitlen + 7u) / 8u;
    uint8_t *bits = (uint8_t*)calloc(bylen, 1);
    if (!bits) { uft_diag_set(diag, "flux_project: OOM"); return UFT_ENOMEM; }

    uint32_t bi = 0;
    for (uint32_t i = 0; i < fs->count; i++) {
        uint32_t cells = fs->dt_ns[i] / cell_ns;
        if (cells == 0) cells = 1;
        uint32_t last = bi + (cells - 1u);
        if (last >= bitlen) { free(bits); uft_diag_set(diag, "flux_project: bounds"); return UFT_EBOUNDS; }
        bit_set(bits, last, 1);
        bi += cells;
    }

    *out_bits = bits;
    *out_bitlen = bitlen;
    uft_diag_set(diag, "flux_project: ok (diagnostic rawbits)");
    return UFT_OK;
}
