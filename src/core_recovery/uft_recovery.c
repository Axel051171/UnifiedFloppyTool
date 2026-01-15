/**
 * @file uft_recovery.c
 * @brief Data recovery algorithms
 * @version 3.8.0
 */
#include "uft/uft_recovery.h"

#include "uft/uft_pll.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *bits;
    size_t bit_count;
    size_t sync_pos; /* (size_t)-1 if not found */
    uint32_t final_cell;
    size_t dropped;
} pass_bits_t;

uft_recovery_cfg_t uft_recovery_cfg_default(void)
{
    uft_recovery_cfg_t c;
    c.mfm_sync = 0x4489;
    c.max_bits = 200000; /* enough for a typical track */
    c.min_passes = 2;
    return c;
}

static inline uint8_t get_bit(const uint8_t *bits, size_t pos)
{
    return (bits[pos >> 3] >> (7 - (pos & 7))) & 1;
}

static inline void set_bit(uint8_t *bits, size_t pos, uint8_t v)
{
    const size_t i = pos >> 3;
    const uint8_t m = (uint8_t)(0x80u >> (pos & 7));
    if (v) bits[i] |= m;
    else bits[i] &= (uint8_t)~m;
}

static size_t find_sync16(const uint8_t *bits, size_t bit_count, uint16_t sync)
{
    if (!bits || bit_count < 16) return (size_t)-1;
    uint16_t w = 0;
    for (size_t i = 0; i < bit_count; ++i) {
        w = (uint16_t)((w << 1) | get_bit(bits, i));
        if (i >= 15 && w == sync) return i - 15;
    }
    return (size_t)-1;
}

static uint32_t estimate_cell_ns_from_track(const flux_track_t *t)
{
    if (!t || !t->samples || t->sample_count < 2) return 2000;
    /* crude: average first 32 deltas, clamp to DD/HD bucket */
    uint64_t sum = 0;
    size_t n = 0;
    for (size_t i = 0; i + 1 < t->sample_count && n < 32; ++i, ++n) {
        uint64_t a = t->samples[i].timestamp_ns;
        uint64_t b = t->samples[i + 1].timestamp_ns;
        if (b > a) sum += (b - a);
    }
    if (n == 0) return 2000;
    uint64_t avg = sum / n;
    return (avg < 1500) ? 1000 : 2000;
}

static int decode_pass(const flux_track_t *t, pass_bits_t *out, size_t max_bits)
{
    memset(out, 0, sizeof(*out));
    out->sync_pos = (size_t)-1;

    if (!t || !t->samples || t->sample_count < 2) return 0;

    uint64_t *timestamps = (uint64_t *)calloc(t->sample_count, sizeof(uint64_t));
    if (!timestamps) return 0;
    for (size_t i = 0; i < t->sample_count; ++i) timestamps[i] = t->samples[i].timestamp_ns;

    out->bits = (uint8_t *)calloc((max_bits + 7) / 8, 1);
    if (!out->bits) {
        free(timestamps);
        return 0;
    }

    const uint32_t est_cell = estimate_cell_ns_from_track(t);
    uft_pll_cfg_t cfg = (est_cell <= 1100) ? uft_pll_cfg_default_mfm_hd() : uft_pll_cfg_default_mfm_dd();

    size_t dropped = 0;
    uint32_t final_cell = 0;
    size_t bits = uft_flux_to_bits_pll(
        timestamps,
        t->sample_count,
        &cfg,
        out->bits,
        max_bits,
        &final_cell,
        &dropped
    );

    out->bit_count = bits;
    out->final_cell = final_cell;
    out->dropped = dropped;

    free(timestamps);
    return (bits > 0);
}

static void free_pass(pass_bits_t *p)
{
    if (!p) return;
    free(p->bits);
    memset(p, 0, sizeof(*p));
}

size_t uft_recover_mfm_track_multipass(
    const flux_track_t *const *passes,
    size_t pass_count,
    const uft_recovery_cfg_t *cfg_in,
    uint8_t *out_bits,
    size_t out_capacity_bits,
    float *out_quality)
{
    if (out_quality) *out_quality = 0.0f;
    if (!passes || pass_count == 0 || !out_bits || out_capacity_bits == 0) return 0;

    uft_recovery_cfg_t cfg = cfg_in ? *cfg_in : uft_recovery_cfg_default();
    if (cfg.min_passes && pass_count < cfg.min_passes) return 0;

    const size_t max_bits = (cfg.max_bits && cfg.max_bits < out_capacity_bits) ? cfg.max_bits : out_capacity_bits;

    pass_bits_t *pb = (pass_bits_t *)calloc(pass_count, sizeof(pass_bits_t));
    if (!pb) return 0;

    size_t ok_passes = 0;
    for (size_t i = 0; i < pass_count; ++i) {
        if (decode_pass(passes[i], &pb[i], max_bits)) {
            pb[i].sync_pos = find_sync16(pb[i].bits, pb[i].bit_count, cfg.mfm_sync);
            ++ok_passes;
        }
    }

    if (ok_passes == 0) {
        free(pb);
        return 0;
    }

    /* Pick reference pass: first with sync, else first decoded. */
    size_t ref = 0;
    for (size_t i = 0; i < pass_count; ++i) {
        if (pb[i].bits && pb[i].sync_pos != (size_t)-1) { ref = i; break; }
        if (pb[i].bits && pb[ref].bits == NULL) ref = i;
    }

    const size_t ref_sync = (pb[ref].sync_pos == (size_t)-1) ? 0 : pb[ref].sync_pos;

    /* Determine voted length: shortest usable aligned range. */
    size_t voted_len = pb[ref].bit_count;
    for (size_t i = 0; i < pass_count; ++i) {
        if (!pb[i].bits) continue;
        size_t s = (pb[i].sync_pos == (size_t)-1) ? 0 : pb[i].sync_pos;
        if (pb[i].bit_count > s && pb[ref].bit_count > ref_sync) {
            size_t avail = pb[i].bit_count - s;
            size_t ref_avail = pb[ref].bit_count - ref_sync;
            if (avail < voted_len) voted_len = avail;
            if (ref_avail < voted_len) voted_len = ref_avail;
        }
    }
    if (voted_len > max_bits) voted_len = max_bits;

    memset(out_bits, 0, (voted_len + 7) / 8);

    size_t unanimous = 0;
    size_t considered = 0;

    for (size_t b = 0; b < voted_len; ++b) {
        int ones = 0;
        int zeros = 0;
        int voters = 0;
        for (size_t i = 0; i < pass_count; ++i) {
            if (!pb[i].bits) continue;
            size_t s = (pb[i].sync_pos == (size_t)-1) ? 0 : pb[i].sync_pos;
            if (s + b >= pb[i].bit_count) continue;
            uint8_t v = get_bit(pb[i].bits, s + b);
            if (v) ++ones; else ++zeros;
            ++voters;
        }
        if (voters == 0) break;

        uint8_t refbit = get_bit(pb[ref].bits, ref_sync + b);
        uint8_t out = (ones == zeros) ? refbit : (ones > zeros ? 1 : 0);
        set_bit(out_bits, b, out);

        ++considered;
        if (ones == voters || zeros == voters) ++unanimous;
    }

    /* quality: unanimity minus penalty for dropped transitions */
    float q = (considered == 0) ? 0.0f : (float)unanimous / (float)considered;
    size_t total_dropped = 0;
    for (size_t i = 0; i < pass_count; ++i) if (pb[i].bits) total_dropped += pb[i].dropped;
    if (total_dropped > 0) {
        float penalty = (float)total_dropped / (float)(total_dropped + 200.0f);
        q *= (1.0f - 0.5f * penalty);
    }
    if (out_quality) *out_quality = q;

    for (size_t i = 0; i < pass_count; ++i) free_pass(&pb[i]);
    free(pb);

    return voted_len;
}
