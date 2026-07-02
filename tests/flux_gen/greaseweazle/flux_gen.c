/**
 * @file tests/flux_gen/greaseweazle/flux_gen.c
 * @brief Synthetic flux generator core for the GW USB stream format.
 *
 * Output is byte-for-byte identical to what the GW firmware streams
 * during Cmd.ReadFlux. The decoder in src/hal/uft_greaseweazle_full.c
 * (uft_gw_decode_flux_stream) is the canonical reference for the
 * encoding — we matched our encoder against its decoder in tests.
 */

#include "flux_gen.h"

#include <stdlib.h>
#include <string.h>

/* ─── Deterministic RNG (xorshift64*) — identical to SCP gen ────────── */

static uint64_t rng_next(uint64_t *s)
{
    uint64_t x = *s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1Dull;
}

static void rng_seed(uint64_t *s, uint64_t seed)
{
    *s = (seed == 0) ? 1 : seed;
}

/* ─── N28 encoding (matches gw_write_n28 in the C HAL) ──────────────── */

static void n28_write(uint8_t *p, uint32_t val)
{
    p[0] = 1 | ((val << 1) & 0xFF);
    p[1] = 1 | ((val >> 6) & 0xFF);
    p[2] = 1 | ((val >> 13) & 0xFF);
    p[3] = 1 | ((val >> 20) & 0xFF);
}

static uint32_t n28_read(const uint8_t *p)
{
    uint32_t val  = (p[0] & 0xFE) >> 1;
    val += (uint32_t)(p[1] & 0xFE) << 6;
    val += (uint32_t)(p[2] & 0xFE) << 13;
    val += (uint32_t)(p[3] & 0xFE) << 20;
    return val;
}

/* ─── Encode one flux interval (in ticks) into the GW stream ─────────
 *
 * Mirrors the encoding logic in uft_gw_encode_flux_stream() but with
 * a fixed NFA threshold instead of caller-supplied (we leave NFA
 * generation to the explicit defect path). Returns bytes appended,
 * or 0 if buffer would overflow. */
static size_t encode_tick(uint8_t *out, size_t out_cap, uint32_t ticks)
{
    if (ticks == 0) return 0;
    if (ticks < 250) {
        if (out_cap < 1) return 0;
        out[0] = (uint8_t)ticks;
        return 1;
    }
    uint32_t high = (ticks - 250) / 255;
    if (high < 5) {
        if (out_cap < 2) return 0;
        out[0] = (uint8_t)(250 + high);
        out[1] = (uint8_t)(1 + (ticks - 250) % 255);
        return 2;
    }
    /* Reference: Space opcode for large values (val - 249) + literal 249. */
    if (out_cap < 7) return 0;
    out[0] = 0xFF;
    out[1] = UFT_GW_FLUXOP_SPACE;
    n28_write(out + 2, ticks - 249);
    out[6] = 249;
    return 7;
}

/* ─── Index opcode encode ────────────────────────────────────────── */
static size_t encode_index(uint8_t *out, size_t out_cap, uint32_t ticks_since_index)
{
    if (out_cap < 6) return 0;
    out[0] = 0xFF;
    out[1] = UFT_GW_FLUXOP_INDEX;
    n28_write(out + 2, ticks_since_index);
    return 6;
}

/* ─── Param validation ─────────────────────────────────────────────── */

static uft_gw_flux_gen_err_t validate_params(const uft_gw_flux_params_t *p)
{
    if (!p) return UFT_GW_FLUX_GEN_ERR_NULL;
    if (p->revolutions < 1 || p->revolutions > 5) {
        return UFT_GW_FLUX_GEN_ERR_INVALID;
    }
    if (p->transitions_per_rev < 10 || p->transitions_per_rev > 200000) {
        return UFT_GW_FLUX_GEN_ERR_INVALID;
    }
    if (p->weak_jitter_pct > 50) return UFT_GW_FLUX_GEN_ERR_INVALID;
    if (p->index_period_ns < 50000000u || p->index_period_ns > 500000000u) {
        /* 50..500 ms covers 120..600 RPM. */
        return UFT_GW_FLUX_GEN_ERR_OUT_OF_SPEC;
    }
    if (p->sample_freq_hz < 10000000u || p->sample_freq_hz > 200000000u) {
        return UFT_GW_FLUX_GEN_ERR_OUT_OF_SPEC;
    }
    return UFT_GW_FLUX_GEN_OK;
}

/* ─── Clean MFM generator ──────────────────────────────────────────── */

uft_gw_flux_gen_err_t uft_gw_flux_gen_clean(
    const uft_gw_flux_params_t *params,
    uft_gw_flux_capture_t      *out_capture)
{
    if (!out_capture) return UFT_GW_FLUX_GEN_ERR_NULL;
    memset(out_capture, 0, sizeof(*out_capture));

    uft_gw_flux_gen_err_t v = validate_params(params);
    if (v != UFT_GW_FLUX_GEN_OK) return v;

    uint64_t rng;
    rng_seed(&rng, params->seed);

    /* Buffer sizing: each transition can be up to 7 bytes (Space-opcode
     * variant). Reserve a generous worst-case + Index opcodes + EOS. */
    size_t cap_bytes = (size_t)params->transitions_per_rev *
                       (size_t)params->revolutions * 8u + 64u;
    uint8_t *buf = (uint8_t *)calloc(1, cap_bytes);
    if (!buf) return UFT_GW_FLUX_GEN_ERR_NULL;

    size_t pos = 0;

    /* index period in ticks */
    uint32_t freq = params->sample_freq_hz;
    uint64_t idx_ticks_64 = (uint64_t)params->index_period_ns *
                            (uint64_t)freq / 1000000000ull;
    uint32_t base_index_ticks = (uint32_t)idx_ticks_64;

    /* Emit an Index marker at the very start of the stream (matches
     * what the firmware does when index_sync is true). The N28 payload
     * is the ticks since prev index — for the first one, 0. */
    {
        size_t n = encode_index(buf + pos, cap_bytes - pos, 0);
        if (n == 0) { free(buf); return UFT_GW_FLUX_GEN_ERR_BUF_SMALL; }
        pos += n;
    }

    for (int rev = 0; rev < params->revolutions; rev++) {
        uint32_t rev_index_ticks = base_index_ticks;

        if (params->defects & UFT_GW_DEFECT_INDEX_JITTER) {
            /* ±0.5% jitter typical of mechanical photo-interrupters. */
            uint64_t r = rng_next(&rng);
            int32_t  jit_max = (int32_t)(base_index_ticks / 200u); /* 0.5% */
            int32_t  jit = (int32_t)((r % (uint64_t)(jit_max * 2 + 1))) - jit_max;
            int64_t  jittered = (int64_t)base_index_ticks + jit;
            if (jittered < 0) jittered = base_index_ticks;
            rev_index_ticks = (uint32_t)jittered;
        }

        if (params->defects & UFT_GW_DEFECT_LONG_TRACK) {
            /* Long-track copy-protection: stretch one revolution ~3%. */
            if (rev == 0) {
                rev_index_ticks = (uint32_t)((uint64_t)rev_index_ticks * 103u / 100u);
            }
        }

        uint64_t ticks_in_rev = 0;
        for (uint32_t t = 0; t < params->transitions_per_rev; t++) {
            uint64_t r = rng_next(&rng);
            uint8_t cells = (uint8_t)((r % 3) + 2);   /* 2..4 cells */
            uint32_t cell_ns = UFT_GW_FLUX_GEN_CELL_NS_DD * cells;

            if (params->defects & UFT_GW_DEFECT_WEAK_BITS) {
                uft_gw_defect_weak_bits_apply(&rng,
                                              params->weak_jitter_pct,
                                              &cell_ns);
            }

            if (cell_ns < UFT_GW_FLUX_GEN_MIN_NS ||
                cell_ns > UFT_GW_FLUX_GEN_MAX_NS) {
                free(buf);
                return UFT_GW_FLUX_GEN_ERR_OUT_OF_SPEC;
            }

            uint64_t cell_ticks_64 = (uint64_t)cell_ns *
                                     (uint64_t)freq / 1000000000ull;
            if (cell_ticks_64 == 0) cell_ticks_64 = 1;
            ticks_in_rev += cell_ticks_64;

            size_t n = encode_tick(buf + pos, cap_bytes - pos,
                                    (uint32_t)cell_ticks_64);
            if (n == 0) {
                free(buf);
                return UFT_GW_FLUX_GEN_ERR_BUF_SMALL;
            }
            pos += n;
        }

        /* Optional NFA burst near end of rev. */
        if ((params->defects & UFT_GW_DEFECT_NFA_BURST) && rev == 0) {
            uint32_t space_ticks   = freq / 5000u;   /* ~200 µs */
            uint32_t astable_ticks = freq / 8000u;   /* ~125 µs */
            uft_gw_defect_nfa_burst_emit(buf, cap_bytes, &pos,
                                          space_ticks, astable_ticks);
        }

        /* Index opcode at end of revolution. The N28 payload is the
         * ticks since the previous index — the running rev tick count. */
        size_t n = encode_index(buf + pos, cap_bytes - pos,
                                 rev_index_ticks);
        if (n == 0) {
            free(buf);
            return UFT_GW_FLUX_GEN_ERR_BUF_SMALL;
        }
        pos += n;

        out_capture->rev_index_ticks[rev] = rev_index_ticks;
        (void)ticks_in_rev; /* informative only — Index payload is exact */
    }

    /* EOS terminator. */
    if (pos >= cap_bytes) {
        free(buf);
        return UFT_GW_FLUX_GEN_ERR_BUF_SMALL;
    }
    buf[pos++] = 0x00;

    out_capture->bytes          = buf;
    out_capture->bytes_len      = pos;
    out_capture->rev_count      = params->revolutions;
    out_capture->sample_freq_hz = freq;

    /* CRC defect — apply post-encode so the byte offset is deterministic. */
    if (params->defects & UFT_GW_DEFECT_CRC_ERROR) {
        uft_gw_defect_crc_error_apply(out_capture);
    }

    return UFT_GW_FLUX_GEN_OK;
}

void uft_gw_flux_gen_free(uft_gw_flux_capture_t *capture)
{
    if (!capture) return;
    if (capture->bytes) free(capture->bytes);
    capture->bytes     = NULL;
    capture->bytes_len = 0;
    capture->rev_count = 0;
}

/* ─── Decode helper (mirrors uft_gw_decode_flux_stream → ns) ─────────
 *
 * The GW HAL's decoder accumulates ticks across consecutive Space
 * opcodes / overflow markers and emits one sample per "true" transition.
 * We mirror that exactly so a roundtrip encode→decode produces the
 * same interval count we encoded (modulo Index opcodes which decode
 * separately). */
size_t uft_gw_flux_gen_decode_ns(
    const uft_gw_flux_capture_t *capture,
    uint32_t *out_ns, size_t out_cap)
{
    if (!capture || !capture->bytes || !out_ns) return 0;
    size_t count = 0;
    size_t i = 0;
    uint32_t ticks_acc = 0;
    uint32_t freq = capture->sample_freq_hz;
    if (freq == 0) freq = UFT_GW_FLUX_GEN_FREQ_F7_HZ;

    while (i < capture->bytes_len && count < out_cap) {
        uint8_t v = capture->bytes[i++];
        if (v == 0x00) break;
        if (v == 0xFF) {
            if (i >= capture->bytes_len) break;
            uint8_t op = capture->bytes[i++];
            if (op == UFT_GW_FLUXOP_INDEX) {
                if (i + 4 > capture->bytes_len) break;
                i += 4;
            } else if (op == UFT_GW_FLUXOP_SPACE) {
                if (i + 4 > capture->bytes_len) break;
                ticks_acc += n28_read(capture->bytes + i);
                i += 4;
            } else if (op == UFT_GW_FLUXOP_ASTABLE) {
                if (i + 4 > capture->bytes_len) break;
                i += 4;
            } else {
                break;
            }
        } else if (v <= 249) {
            uint64_t ticks = ticks_acc + v;
            uint64_t ns = ticks * 1000000000ull / freq;
            out_ns[count++] = (uint32_t)ns;
            ticks_acc = 0;
        } else {
            if (i >= capture->bytes_len) break;
            uint32_t dec = 250u + (uint32_t)(v - 250u) * 255u +
                            (uint32_t)capture->bytes[i++] - 1u;
            uint64_t ticks = ticks_acc + dec;
            uint64_t ns = ticks * 1000000000ull / freq;
            out_ns[count++] = (uint32_t)ns;
            ticks_acc = 0;
        }
    }
    return count;
}

size_t uft_gw_flux_gen_count_unsafe(const uft_gw_flux_capture_t *capture)
{
    if (!capture || !capture->bytes) return 0;
    /* Decode and check every interval against the medium-safe range. */
    enum { MAX_DECODE = 64 * 1024 };
    uint32_t *buf = (uint32_t *)calloc(MAX_DECODE, sizeof(uint32_t));
    if (!buf) return 0;
    size_t n = uft_gw_flux_gen_decode_ns(capture, buf, MAX_DECODE);
    size_t unsafe = 0;
    for (size_t i = 0; i < n; i++) {
        if (buf[i] < UFT_GW_FLUX_GEN_MIN_NS ||
            buf[i] > UFT_GW_FLUX_GEN_MAX_NS) {
            unsafe++;
        }
    }
    free(buf);
    return unsafe;
}

/* ─── Defect: WEAK_BITS ───────────────────────────────────────────── */

uft_gw_flux_gen_err_t uft_gw_defect_weak_bits_apply(
    uint64_t *rng_state, uint8_t weak_jitter_pct,
    uint32_t *cell_ns_inout)
{
    if (!rng_state || !cell_ns_inout) return UFT_GW_FLUX_GEN_ERR_NULL;
    if (weak_jitter_pct > 50) return UFT_GW_FLUX_GEN_ERR_INVALID;
    if (weak_jitter_pct == 0) return UFT_GW_FLUX_GEN_OK;

    uint32_t cell = *cell_ns_inout;
    uint64_t r = rng_next(rng_state);

    int32_t jitter_max = (int32_t)((uint64_t)cell *
                                    (uint64_t)weak_jitter_pct / 100u);
    int32_t jitter     = (int32_t)((r % (uint64_t)(jitter_max * 2 + 1))) -
                          jitter_max;

    int64_t new_cell = (int64_t)cell + jitter;
    if (new_cell < (int64_t)UFT_GW_FLUX_GEN_MIN_NS) {
        new_cell = (int64_t)UFT_GW_FLUX_GEN_MIN_NS;
    }
    *cell_ns_inout = (uint32_t)new_cell;
    return UFT_GW_FLUX_GEN_OK;
}

/* ─── Defect: CRC_ERROR ─────────────────────────────────────────────
 *
 * Flips one byte at a deterministic position past the leading Index
 * opcode (so we corrupt a real flux byte, not the metadata). Choose
 * offset 16 — past the initial Index(6 bytes) and a handful of
 * leading flux bytes, comfortably inside rev 0. */
uft_gw_flux_gen_err_t uft_gw_defect_crc_error_apply(
    uft_gw_flux_capture_t *cap)
{
    if (!cap || !cap->bytes) return UFT_GW_FLUX_GEN_ERR_NULL;
    /* Find a byte that is a 1-byte flux delta (1..249) so the corruption
     * stays in valid encoding range (and the decoder will still parse
     * but yield a wrong interval). */
    for (size_t off = 16; off < cap->bytes_len - 2; off++) {
        uint8_t b = cap->bytes[off];
        if (b >= 1 && b <= 100) {
            /* XOR with 0x40 to push it into a clearly-different but
             * still legal 1-byte range. Bounded so we don't accidentally
             * land in the 250..254 (2-byte) opcode zone. */
            cap->bytes[off] = (uint8_t)(b ^ 0x40);
            return UFT_GW_FLUX_GEN_OK;
        }
    }
    return UFT_GW_FLUX_GEN_ERR_INVALID;
}

/* ─── Defect: NFA_BURST ─────────────────────────────────────────────
 *
 * Emit a Space + Astable opcode pair. This is the wire-format for a
 * "no-flux area" (NFA): the firmware uses it to mark a section of
 * track with no real transitions (long-format PC copy-protection,
 * weak-bit regions in Amiga, etc.). */
uft_gw_flux_gen_err_t uft_gw_defect_nfa_burst_emit(
    uint8_t *out, size_t out_cap, size_t *pos,
    uint32_t space_ticks, uint32_t astable_ticks)
{
    if (!out || !pos) return UFT_GW_FLUX_GEN_ERR_NULL;
    if (*pos + 12 > out_cap) return UFT_GW_FLUX_GEN_ERR_BUF_SMALL;
    out[*pos + 0] = 0xFF;
    out[*pos + 1] = UFT_GW_FLUXOP_SPACE;
    n28_write(out + *pos + 2, space_ticks);
    out[*pos + 6] = 0xFF;
    out[*pos + 7] = UFT_GW_FLUXOP_ASTABLE;
    n28_write(out + *pos + 8, astable_ticks);
    *pos += 12;
    return UFT_GW_FLUX_GEN_OK;
}
