/**
 * @file uft_pll.c
 * @brief Real implementation of the standalone PLL helper from uft_pll.h.
 *
 * UFT-A07 history: prior to this commit, `uft_pll_cfg_default_mfm_dd`,
 * `uft_pll_cfg_default_mfm_hd`, and `uft_flux_to_bits_pll` lived in
 * src/core/uft_core_stubs.c — but with a LOCALLY duplicated struct
 * (`uft_pll_cfg_stub_t`) and with `cfg` typed as `const void*` instead
 * of `const uft_pll_cfg_t*`. The "stubs" file's comment claimed the
 * "real impl" was in `src/core/uft_pll.c`, but that file did not exist;
 * the stub IS the only implementation.
 *
 * Risk before A07: the duplicate struct's field layout happens to
 * match `uft_pll_cfg_t` today (5× uint32_t), so the void*-cast worked
 * by accident. Any future change to `uft_pll.h`'s struct silently
 * produced garbage at every caller that included the header — a
 * classic ABI-bomb (no compiler warning, no link error, just wrong
 * bits at runtime).
 *
 * Fix: implementation moved here, struct comes from the public header,
 * signatures match exactly, duplicate removed from the stub file.
 */

#include "uft/uft_pll.h"

#include <stdint.h>
#include <stddef.h>

uft_pll_cfg_t uft_pll_cfg_default_mfm_dd(void) {
    /* 4 µs cell = 250 kbps DD */
    uft_pll_cfg_t cfg = { 4000, 3000, 5600, 3277, 4 };
    return cfg;
}

uft_pll_cfg_t uft_pll_cfg_default_mfm_hd(void) {
    /* 2 µs cell = 500 kbps HD */
    uft_pll_cfg_t cfg = { 2000, 1500, 2800, 3277, 4 };
    return cfg;
}

size_t uft_flux_to_bits_pll(
    const uint64_t *timestamps_ns,
    size_t count,
    const uft_pll_cfg_t *cfg,
    uint8_t *out_bits,
    size_t out_bits_capacity_bits,
    uint32_t *out_final_cell_ns,
    size_t *out_dropped_transitions,
    size_t *out_consumed_transitions)
{
    /* Ausgaben ZUERST setzen. Die Fruehabsage unten sprang vorher heraus,
     * ohne einen einzigen Ausgabezeiger anzufassen — der Aufrufer las
     * dann, was zufaellig auf seinem Stapel lag. Heute folgenlos, weil der
     * einzige Aufrufer seine Variablen nullt; das ist kein Grund, es so
     * zu lassen. */
    if (out_final_cell_ns)        *out_final_cell_ns = cfg ? cfg->cell_ns : 0u;
    if (out_dropped_transitions)  *out_dropped_transitions = 0u;
    if (out_consumed_transitions) *out_consumed_transitions = 0u;

    if (!timestamps_ns || count < 2 || !cfg || !out_bits)
        return 0;

    uint32_t cell = cfg->cell_ns;
    uint32_t alpha = cfg->alpha_q16;
    size_t bits = 0;
    size_t dropped = 0;

    /* `max_run_cells == 0` waere eine Lauflaenge von null Bit. Die alte
     * Fassung rechnete daraus `n - 1 == UINT32_MAX` und lief die
     * Nullbit-Schleife, bis der Puffer voll war; hier waere es eine
     * Bereichsunterschreitung. Eine Konfiguration ohne Lauflaenge gibt es
     * im Baum nicht — beide Vorgaben nennen 4 —, gerechnet wird trotzdem
     * mit mindestens 1, statt sich darauf zu verlassen. */
    const uint32_t maxrun = cfg->max_run_cells ? cfg->max_run_cells : 1u;

    /* Wie weit sind wir gekommen? Optimistisch: bis ans Ende. Bricht die
     * Schleife wegen des Puffers ab, wird es berichtigt. */
    size_t consumed = count;

    for (size_t i = 1; i < count; i++) {
        uint64_t delta = timestamps_ns[i] - timestamps_ns[i - 1];
        if (delta == 0) { dropped++; continue; }

        /* How many cells fit in this interval? */
        uint32_t n = (uint32_t)((delta + cell / 2) / cell);
        if (n == 0) n = 1;
        if (n > maxrun) { n = maxrun; dropped++; }

        /* GANZ oder GAR NICHT. Vorher pruefte jede einzelne Bitstelle
         * gegen die Kapazitaet, wodurch ein Lauf mittendrin enden konnte:
         * seine Nullbits standen da, sein abschliessendes Eins-Bit nicht.
         * Das ist ein Muster, das die Diskette nie getragen hat — also
         * erfundene Daten, und die sind teurer als fehlende (Prinzip 3).
         * `bits <= out_bits_capacity_bits` gilt immer, die Differenz kann
         * also nicht unterlaufen. */
        if ((size_t)n > out_bits_capacity_bits - bits) {
            consumed = i;               /* Wechsel i ist NICHT verarbeitet */
            break;
        }

        /* n-1 Nullbits (der Puffer ist genullt, siehe Kopf) + ein Eins-Bit */
        bits += (size_t)n - 1u;
        out_bits[bits >> 3] |= (uint8_t)(1u << (7u - (bits & 7u)));
        bits++;

        /* Adjust cell size (PI loop, Q16 fixed point) */
        int32_t err = (int32_t)(delta - (uint64_t)n * cell);
        int32_t adj = (int32_t)((int64_t)err * alpha >> 16);
        cell = (uint32_t)((int32_t)cell + adj);
        if (cell < cfg->cell_ns_min) cell = cfg->cell_ns_min;
        if (cell > cfg->cell_ns_max) cell = cfg->cell_ns_max;
    }

    if (out_final_cell_ns)        *out_final_cell_ns = cell;
    if (out_dropped_transitions)  *out_dropped_transitions = dropped;
    if (out_consumed_transitions) *out_consumed_transitions = consumed;
    return bits;
}
