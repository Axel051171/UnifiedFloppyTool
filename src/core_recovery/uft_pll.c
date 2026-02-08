/**
 * @file uft_pll.c
 * @brief PLL timing recovery
 * @version 3.8.0
 */
#include "uft/uft_pll.h"

#include <string.h>

static inline uint32_t clamp_u32(uint32_t v, uint32_t lo, uint32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

uft_pll_cfg_t uft_pll_cfg_default_mfm_dd(void)
{
    uft_pll_cfg_t c;
    c.cell_ns = 2000;              /* ~2µs */
    c.cell_ns_min = 1600;
    c.cell_ns_max = 2400;
    c.alpha_q16 = 3277;            /* ~0.05 */
    c.max_run_cells = 8;
    return c;
}

uft_pll_cfg_t uft_pll_cfg_default_mfm_hd(void)
{
    uft_pll_cfg_t c;
    c.cell_ns = 1000;              /* ~1µs */
    c.cell_ns_min = 800;
    c.cell_ns_max = 1200;
    c.alpha_q16 = 3277;            /* ~0.05 */
    c.max_run_cells = 8;
    return c;
}

static inline void set_bit(uint8_t *bits, size_t bitpos, uint8_t v)
{
    const size_t byte_i = bitpos >> 3;
    const uint8_t mask = (uint8_t)(0x80u >> (bitpos & 7u));
    if (v) bits[byte_i] |= mask;
    else  bits[byte_i] &= (uint8_t)~mask;
}

size_t uft_flux_to_bits_pll(
    const uint64_t *timestamps_ns,
    size_t count,
    const uft_pll_cfg_t *cfg_in,
    uint8_t *out_bits,
    size_t out_bits_capacity_bits,
    uint32_t *out_final_cell_ns,
    size_t *out_dropped_transitions)
{
    if (!timestamps_ns || count < 2 || !cfg_in || !out_bits || out_bits_capacity_bits == 0) {
        if (out_final_cell_ns) *out_final_cell_ns = 0;
        if (out_dropped_transitions) *out_dropped_transitions = 0;
        return 0;
    }

    memset(out_bits, 0, (out_bits_capacity_bits + 7) / 8);

    uft_pll_cfg_t cfg = *cfg_in;
    uint32_t cell = cfg.cell_ns ? cfg.cell_ns : 2000;
    cell = clamp_u32(cell, cfg.cell_ns_min, cfg.cell_ns_max);

    size_t dropped = 0;
    size_t bitpos = 0;

    /*
     * Model:
     * - Each flux transition marks a '1' at the end of a run of cells.
     * - We approximate run length as round(delta / cell).
     * - PLL: cell += alpha * (delta - run*cell) / run
     */
    for (size_t i = 0; i + 1 < count; ++i) {
        uint64_t a = timestamps_ns[i];
        uint64_t b = timestamps_ns[i + 1];
        if (b <= a) {
            ++dropped;
            continue;
        }

        uint64_t delta = b - a;

        /* Reject clearly insane deltas (spikes). */
        if (delta < (uint64_t)(cell / 4u)) {
            ++dropped;
            continue;
        }

        uint32_t run = (uint32_t)((delta + (cell / 2u)) / cell); /* rounded */
        if (run == 0) run = 1;
        if (cfg.max_run_cells && run > cfg.max_run_cells) run = cfg.max_run_cells;

        /* Emit (run-1) zeros then one '1'. */
        if (bitpos + run > out_bits_capacity_bits) break;
        for (uint32_t k = 0; k + 1 < run; ++k) {
            set_bit(out_bits, bitpos++, 0);
        }
        set_bit(out_bits, bitpos++, 1);

        /* PLL update */
        int64_t expected = (int64_t)run * (int64_t)cell;
        int64_t err = (int64_t)delta - expected;
        /* distribute error per cell to avoid overcorrecting long runs */
        int64_t err_per_cell = err / (int64_t)run;

        int64_t adj = (err_per_cell * (int64_t)cfg.alpha_q16) >> 16;
        int64_t new_cell = (int64_t)cell + adj;
        cell = clamp_u32((uint32_t)(new_cell < 1 ? 1 : new_cell), cfg.cell_ns_min, cfg.cell_ns_max);
    }

    if (out_final_cell_ns) *out_final_cell_ns = cell;
    if (out_dropped_transitions) *out_dropped_transitions = dropped;
    return bitpos;
}
