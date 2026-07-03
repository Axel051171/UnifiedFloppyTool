/**
 * @file tests/flux_gen/adfcopy/flux_gen.c
 * @brief Implementation of the synthetic ADFCopy Amiga-MFM flux generator.
 *
 * See flux_gen.h. Emits the CMD_READ_FLUX reply wire format: a 3-byte
 * header (status + LE16 length) followed by flux bytes (25 ns ticks).
 */

#include "flux_gen.h"

#include <stdlib.h>
#include <string.h>

static uint64_t rng_next(uint64_t *s) {
    uint64_t x = *s;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static uint32_t ns_to_ticks(uint32_t ns) {
    return (ns + UFT_ADFC_TICK_NS / 2) / UFT_ADFC_TICK_NS;
}

uft_adfc_gen_err_t uft_adfc_gen_flux(const uft_adfc_gen_params_t *params,
                                     uft_adfc_gen_flux_t *out) {
    if (!params || !out) return UFT_ADFC_GEN_ERR_NULL;
    memset(out, 0, sizeof(*out));

    if (params->track < 0 || params->track > UFT_ADFC_MAX_TRACK ||
        params->revolutions < UFT_ADFC_MIN_REVS ||
        params->revolutions > UFT_ADFC_MAX_REVS)
        return UFT_ADFC_GEN_ERR_OUT_OF_SPEC;

    uint8_t jitter = 0;
    if (params->defects & UFT_ADFC_DEFECT_WEAK_BITS) {
        jitter = params->weak_jitter_pct;
        if (jitter < 1 || jitter > 10) return UFT_ADFC_GEN_ERR_OUT_OF_SPEC;
    }

    const uint32_t rev_ns    = 200000000u;              /* 200 ms, ~300 RPM */
    const uint32_t rev_ticks = ns_to_ticks(rev_ns);

    /* Worst case ~1 byte per cell (shortest 2 µs / 25 ns = 80 ticks fits
     * in a byte only up to 255 = 6.375 µs; DD family 2/3/4 µs all fit). */
    size_t max_cells = (size_t)(rev_ns / UFT_ADFC_CELL_NS) + 16u;
    size_t cap = max_cells * (size_t)params->revolutions + 8u;
    uint8_t *flux = (uint8_t *)malloc(cap);
    if (!flux) return UFT_ADFC_GEN_ERR_NOMEM;

    uint64_t rng = params->seed ? params->seed ^ ((uint64_t)params->track << 8)
                                : 0xADF00DULL;
    size_t n = 0;
    int long_rev = (params->defects & UFT_ADFC_DEFECT_LONG_CELL) ? 0 : -1;

    /* The reply length field is 16-bit, so a single CMD_READ_FLUX reply
     * carries at most 0xFFFF flux bytes — a real protocol limit. A full
     * DD revolution has more transitions than that, so the firmware
     * returns a bounded window (see DIVERGENCES ADFC-2). We stop at the
     * 16-bit cap. */
    const size_t flux_cap = 0xFFFFu;
    for (int rev = 0; rev < params->revolutions && n < flux_cap; rev++) {
        uint32_t accum = 0;
        bool injected = false;
        while (accum < rev_ticks && n < cap && n < flux_cap) {
            /* Amiga MFM 2/3/4 µs cell family. */
            uint32_t this_ns;
            uint64_t r = rng_next(&rng) % 100;
            if (r < 50)      this_ns = UFT_ADFC_CELL_NS;         /* 2 µs */
            else if (r < 80) this_ns = UFT_ADFC_CELL_NS * 3 / 2; /* 3 µs */
            else             this_ns = UFT_ADFC_CELL_NS * 2;     /* 4 µs */
            if (jitter) {
                int32_t span = (int32_t)(this_ns * jitter / 100);
                int32_t d = (int32_t)(rng_next(&rng) % (uint32_t)(2 * span + 1)) - span;
                this_ns = (uint32_t)((int32_t)this_ns + d);
            }
            if (long_rev == rev && !injected && accum >= rev_ticks / 2) {
                this_ns = 10000u;   /* 10 µs damaged gap (< MAX) */
                injected = true;
            }
            uint32_t ticks = ns_to_ticks(this_ns);
            if (ticks == 0) ticks = 1;
            if (ticks > 255u) ticks = 255u;   /* one byte per interval */
            flux[n++] = (uint8_t)ticks;
            accum += ticks;
        }
    }

    /* SHORT defect: chop half the flux so a decoder sees a truncated
     * revolution (fewer bytes than the header will still claim below —
     * intentional mismatch modelled as the header length matching the
     * actual bytes; SHORT just yields a sub-revolution flux count). */
    if (params->defects & UFT_ADFC_DEFECT_SHORT) {
        n = n / 2;
    }
    /* n is already <= 0xFFFF (capped during generation). */

    /* Assemble the wire payload: [status, len_lo, len_hi] + flux. */
    size_t total = 3u + n;
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) { free(flux); return UFT_ADFC_GEN_ERR_NOMEM; }
    buf[0] = UFT_ADFC_FLUX_OK;
    buf[1] = (uint8_t)(n & 0xFF);
    buf[2] = (uint8_t)((n >> 8) & 0xFF);
    memcpy(buf + 3, flux, n);
    free(flux);

    out->bytes           = buf;
    out->bytes_len       = total;
    out->status          = UFT_ADFC_FLUX_OK;
    out->flux_len        = (uint16_t)n;
    out->revolution_ticks = rev_ticks;
    out->rpm             = 300.0;
    return UFT_ADFC_GEN_OK;
}

void uft_adfc_gen_free(uft_adfc_gen_flux_t *s) {
    if (!s) return;
    free(s->bytes);
    s->bytes = NULL;
    s->bytes_len = 0;
}

size_t uft_adfc_gen_count_unsafe(const uft_adfc_gen_flux_t *s) {
    if (!s || !s->bytes || s->bytes_len < 3) return 0;
    const uint8_t *flux = s->bytes + 3;
    size_t n = s->bytes_len - 3;
    size_t unsafe = 0;
    for (size_t i = 0; i < n; i++) {
        double t_ns = (double)flux[i] * UFT_ADFC_TICK_NS;
        if (t_ns < (double)UFT_ADFC_MIN_NS || t_ns > (double)UFT_ADFC_MAX_NS)
            unsafe++;
    }
    return unsafe;
}
