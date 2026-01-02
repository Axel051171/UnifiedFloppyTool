#include "uft/uft_error.h"
#include "flux_consensus.h"

#include <string.h>

/* Keep helpers local: MSB-first */
static inline int bit_get(const uint8_t *buf, size_t bitpos)
{
    const size_t byte = bitpos >> 3;
    const size_t sh   = 7u - (bitpos & 7u);
    return (buf[byte] >> sh) & 1u;
}

static inline void bit_put(uint8_t *buf, size_t bitpos, int bit)
{
    const size_t byte = bitpos >> 3;
    const size_t sh   = 7u - (bitpos & 7u);
    if (bit) buf[byte] |= (uint8_t)(1u << sh);
}

int flux_find_mfm_sync_anchor_4489(
    const uint8_t *raw_bits,
    size_t raw_len_bits,
    size_t *anchor_bit)
{
    if (!raw_bits || raw_len_bits < 16) return -1;
    const uint16_t pat = 0x4489u;

    for (size_t p = 0; p + 16u <= raw_len_bits; p++)
    {
        uint16_t w = 0;
        for (size_t k = 0; k < 16u; k++)
            w = (uint16_t)((w << 1) | (uint16_t)bit_get(raw_bits, p + k));
        if (w == pat) {
            if (anchor_bit) *anchor_bit = p;
            return 0;
        }
    }
    return -1;
}

int flux_rotate_bits(
    const uint8_t *in_bits,
    size_t in_len_bits,
    size_t rot,
    uint8_t *out_bits,
    size_t out_bytes_cap)
{
    if (!in_bits || !in_len_bits || !out_bits) return -22;
    const size_t need = (in_len_bits + 7u) >> 3;
    if (out_bytes_cap < need) return -28;

    memset(out_bits, 0, need);
    rot %= in_len_bits;

    /* out[i] = in[(i+rot) % len] */
    for (size_t i = 0; i < in_len_bits; i++)
    {
        const size_t src = i + rot;
        const size_t j = (src >= in_len_bits) ? (src - in_len_bits) : src;
        bit_put(out_bits, i, bit_get(in_bits, j));
    }
    return 0;
}

int flux_build_consensus_bits(
    const uint8_t * const *revs_bits,
    const size_t *revs_len_bits,
    uint32_t nrevs,
    uint8_t *out_bits,
    size_t out_bytes_cap,
    size_t *out_len_bits,
    uint8_t *weak_mask,
    size_t weak_bytes_cap,
    flux_consensus_stats_t *stats_out)
{
    if (!revs_bits || !revs_len_bits || !nrevs || !out_bits) return -22;

    size_t min_len = (size_t)-1;
    for (uint32_t i = 0; i < nrevs; i++)
    {
        if (!revs_bits[i] || revs_len_bits[i] == 0) return -22;
        if (revs_len_bits[i] < min_len) min_len = revs_len_bits[i];
    }
    if (min_len == (size_t)-1 || min_len == 0) return -22;

    const size_t need_bytes = (min_len + 7u) >> 3;
    if (out_bytes_cap < need_bytes) return -28;
    if (weak_mask) {
        if (weak_bytes_cap < need_bytes) return -28;
        memset(weak_mask, 0, need_bytes);
    }
    memset(out_bits, 0, need_bytes);

    flux_consensus_stats_t st;
    memset(&st, 0, sizeof(st));
    st.revs_in = nrevs;
    st.revs_used = nrevs;
    st.total_bits = (uint32_t)min_len;

    /* Majority vote. For even n, ties -> 0 (conservative). */
    for (size_t b = 0; b < min_len; b++)
    {
        uint32_t ones = 0;
        for (uint32_t r = 0; r < nrevs; r++)
            ones += (uint32_t)bit_get(revs_bits[r], b);
        const uint32_t zeros = nrevs - ones;

        if (ones == 0 || zeros == 0) st.unanimous_bits++;
        else st.weak_bits++;

        const int bit = (ones > zeros) ? 1 : 0;
        bit_put(out_bits, b, bit);
        if (weak_mask && ones != 0 && zeros != 0)
            bit_put(weak_mask, b, 1);
    }

    if (out_len_bits) *out_len_bits = min_len;
    if (stats_out) *stats_out = st;
    return 0;
}
