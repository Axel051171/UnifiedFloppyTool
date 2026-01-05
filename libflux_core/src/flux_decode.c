#include "uft/uft_error.h"
#include "flux_decode.h"

#include <string.h>
#include <limits.h>

static inline void bit_put(uint8_t *buf, size_t bitpos, int bit)
{
    const size_t byte = bitpos >> 3;
    const size_t sh   = 7u - (bitpos & 7u);
    if (bit) buf[byte] |= (uint8_t)(1u << sh);
}

int flux_mfm_decode_quantized(
    const uint32_t *dt_ns, size_t dt_count,
    uint32_t bitcell_ns,
    uint8_t *out_bytes, size_t out_bytes_cap,
    size_t *out_bits_written,
    flux_decode_stats_t *stats_out)
{
    if (!dt_ns || !dt_count || !bitcell_ns || !out_bytes || !out_bytes_cap)
        return -22; /* EINVAL */

    memset(out_bytes, 0, out_bytes_cap);

    flux_decode_stats_t st = {0};
    st.min_dt_ns = 0xffffffffu;

    size_t bitpos = 0;

    for (size_t i = 0; i < dt_count; i++)
    {
        const uint32_t dt = dt_ns[i];
        st.total_ns += dt;
        if (dt < st.min_dt_ns) st.min_dt_ns = dt;
        if (dt > st.max_dt_ns) st.max_dt_ns = dt;

        /* Round-to-nearest. */
        uint32_t cells = (dt + (bitcell_ns >> 1)) / bitcell_ns;
        if (cells == 0) cells = 1;
        if (cells > 32) { cells = 32; st.overrun_events++; }

        const uint32_t zeros = cells - 1;
        const size_t need_bits = (size_t)zeros + 1u;
        const size_t need_bytes = (bitpos + need_bits + 7u) >> 3;
        if (need_bytes > out_bytes_cap)
            return -28; /* ENOSPC */

        for (uint32_t z = 0; z < zeros; z++)
            bit_put(out_bytes, bitpos++, 0);
        bit_put(out_bytes, bitpos++, 1);
    }

    if (out_bits_written) *out_bits_written = bitpos;
    if (stats_out) *stats_out = st;
    return 0;
}

/* -------------------- PLL-basierte Bitcell-Rekonstruktion -------------------- */

static inline uint32_t clamp_u32(uint32_t v, uint32_t lo, uint32_t hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

/* Q16.16 multiply: (a_q16 * b_int) >> 16 */
static inline int32_t q16_mul_i32(uint32_t a_q16, int32_t b)
{
    return (int32_t)((((int64_t)(int32_t)a_q16) * (int64_t)b) >> 16);
}

int flux_mfm_decode_pll_raw(
    const uint32_t *dt_ns, size_t dt_count,
    const flux_pll_params_t *pll,
    uint8_t *out_bytes, size_t out_bytes_cap,
    size_t *out_bits_written,
    flux_pll_stats_t *stats_out)
{
    if (!dt_ns || !dt_count || !pll || !pll->nominal_bitcell_ns || !out_bytes || !out_bytes_cap)
        return -22; /* EINVAL */

    memset(out_bytes, 0, out_bytes_cap);

    flux_pll_stats_t st;
    memset(&st, 0, sizeof(st));
    st.min_dt_ns = 0xffffffffu;

    const uint32_t nom = pll->nominal_bitcell_ns;
    const uint32_t clamp = (uint32_t)pll->clamp_permille;
    const uint32_t lo = (clamp > 0) ? (uint32_t)(((uint64_t)nom * (1000u - clamp)) / 1000u) : 1u;
    const uint32_t hi = (clamp > 0) ? (uint32_t)(((uint64_t)nom * (1000u + clamp)) / 1000u) : UINT32_MAX;

    uint32_t period = nom;
    int32_t phase_ns = 0;

    size_t bitpos = 0;

    for (size_t i = 0; i < dt_count; i++)
    {
        const uint32_t dt = dt_ns[i];
        st.total_ns += dt;
        if (dt < st.min_dt_ns) st.min_dt_ns = dt;
        if (dt > st.max_dt_ns) st.max_dt_ns = dt;

        int32_t dt_eff_s = (int32_t)dt + phase_ns;
        if (dt_eff_s < 1) dt_eff_s = 1;
        const uint32_t dt_eff = (uint32_t)dt_eff_s;

        uint32_t cells = (dt_eff + (period >> 1)) / period;
        if (cells == 0) cells = 1;

        if (pll->max_bitcells && st.cells_emitted + cells > pll->max_bitcells)
            return -28;

        const size_t need_bits = (size_t)cells;
        const size_t need_bytes = (bitpos + need_bits + 7u) >> 3;
        if (need_bytes > out_bytes_cap)
            return -28;

        for (uint32_t c = 0; c + 1 < cells; c++)
            bit_put(out_bytes, bitpos++, 0);
        bit_put(out_bytes, bitpos++, 1);
        st.cells_emitted += cells;

        const int32_t pred = (int32_t)((uint64_t)cells * (uint64_t)period);
        const int32_t err  = (int32_t)dt_eff - pred;

        if (pll->alpha_q16)
        {
            const int32_t dPh = q16_mul_i32(pll->alpha_q16, err);
            phase_ns -= dPh;
            const int32_t bound = (int32_t)period;
            if (phase_ns > bound) { phase_ns -= bound; st.phase_wraps++; }
            if (phase_ns < -bound) { phase_ns += bound; st.phase_wraps++; }
        }

        if (pll->beta_q16)
        {
            const int32_t err_per_cell = (cells > 0) ? (err / (int32_t)cells) : err;
            const int32_t dP = q16_mul_i32(pll->beta_q16, err_per_cell);
            int64_t newP = (int64_t)period + dP;
            if (newP < 1) newP = 1;
            uint32_t p = (uint32_t)newP;
            const uint32_t clamped = clamp_u32(p, lo, hi);
            if (clamped != p) st.clamped_period_events++;
            period = clamped;
        }

        if ((err > (int32_t)(nom * 6)) || (err < -(int32_t)(nom * 6)))
        {
            st.resync_events++;
            period = (uint32_t)(((uint64_t)period * 3u + nom) / 4u);
            period = clamp_u32(period, lo, hi);
            phase_ns = 0;
        }
    }

    if (out_bits_written) *out_bits_written = bitpos;
    if (stats_out) *stats_out = st;
    return 0;
}

/* -------------------- Raw-MFM helpers -------------------- */

static inline int bit_get(const uint8_t *buf, size_t bitpos)
{
    const size_t byte = bitpos >> 3;
    const size_t sh   = 7u - (bitpos & 7u);
    return (buf[byte] >> sh) & 1u;
}

int64_t mfm_find_sync_4489(const uint8_t *raw_bits, size_t raw_bits_len)
{
    if (!raw_bits || raw_bits_len < 16) return -1;
    const uint16_t pat = 0x4489u;
    for (size_t p = 0; p + 16 <= raw_bits_len; p++)
    {
        uint16_t w = 0;
        for (size_t k = 0; k < 16; k++)
            w = (uint16_t)((w << 1) | (uint16_t)bit_get(raw_bits, p + k));
        if (w == pat) return (int64_t)p;
    }
    return -1;
}

int mfm_raw_to_data_bits(
    const uint8_t *raw_bits, size_t raw_bits_len,
    int phase,
    uint8_t *out_bytes, size_t out_bytes_cap,
    size_t *out_data_bits,
    mfm_decode_stats_t *stats_out)
{
    if (!raw_bits || raw_bits_len < 2 || !out_bytes || !out_bytes_cap)
        return -22;
    if (phase != 0 && phase != 1)
        return -22;

    memset(out_bytes, 0, out_bytes_cap);
    mfm_decode_stats_t st;
    memset(&st, 0, sizeof(st));

    int prev_data = 1;

    size_t outpos = 0;
    const size_t pairs = raw_bits_len / 2;
    for (size_t n = 0; n < pairs; n++)
    {
        const size_t i0 = 2*n;
        const int b0 = bit_get(raw_bits, i0);
        const int b1 = bit_get(raw_bits, i0 + 1);
        const int clk = (phase == 0) ? b0 : b1;
        const int dat = (phase == 0) ? b1 : b0;
        const int data = dat ? 1 : 0;

        if (data == 1)
        {
            if (clk != 0) st.clock_violations++;
        }
        else
        {
            const int expected_clk = (prev_data == 0) ? 1 : 0;
            if (clk != expected_clk) st.clock_violations++;
        }
        prev_data = data;

        const size_t need_bytes = (outpos + 1u + 7u) >> 3;
        if (need_bytes > out_bytes_cap)
            return -28;
        bit_put(out_bytes, outpos++, data);
    }

    if (out_data_bits) *out_data_bits = outpos;
    if (stats_out) *stats_out = st;
    return 0;
}
