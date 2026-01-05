#pragma once
/*
 * flux_decode.h — Roh-Flux -> Bitcell-Stream (PLL oder Quantize) + Hilfen.
 *
 * Protokoll-Satz: "Wir bewahren Information – wir entscheiden nicht vorschnell, was wichtig ist."
 *
 * Wichtig:
 *  - Output ist ein Bitcell-Stream ("raw MFM" im Flux-Sinn):
 *      bit=1 => Transition am Bitcell-Ende
 *      bit=0 => keine Transition
 *  - Das ist NICHT der "Datenbitstream". Für Sektor-Parser (IBM MFM) brauchst du
 *    weiterhin das 16-bit raw-Layout (0x4489 Sync etc.), d.h. du fütterst den
 *    Parser mit raw-bits direkt.
 *
 * Pipeline (CPC/IBM MFM):
 *   dt_ns[] (UFM) -> flux_mfm_decode_pll_raw() -> raw_bits -> cpc_mfm_decode_track_bits()
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct flux_decode_stats {
    uint64_t total_ns;
    uint32_t min_dt_ns;
    uint32_t max_dt_ns;
    uint32_t overrun_events; /* intervals that map to >32 bitcells (clamped) */
} flux_decode_stats_t;

/* Quantize: deterministisch, schnell, aber ohne Taktanpassung. */
int flux_mfm_decode_quantized(
    const uint32_t *dt_ns, size_t dt_count,
    uint32_t bitcell_ns,
    uint8_t *out_bytes, size_t out_bytes_cap,
    size_t *out_bits_written,
    flux_decode_stats_t *stats_out
);

/* -------------------- PLL-basierte Bitcell-Rekonstruktion -------------------- */

typedef struct flux_pll_params {
    /* Nominal bitcell in ns (DD MFM ~ 4000, HD MFM ~ 2000). */
    uint32_t nominal_bitcell_ns;
    /* Clamp range around nominal (permille). 100 = ±10%. */
    uint16_t clamp_permille;
    /* Loop gains as fixed-point Q16.16 fractions.
       Typical: alpha≈0.20, beta≈0.02  => alpha_q=0.20*(1<<16). */
    uint32_t alpha_q16;
    uint32_t beta_q16;
    /* Max output bitcells to prevent runaway on garbage input. 0 = no extra limit. */
    uint32_t max_bitcells;
} flux_pll_params_t;

typedef struct flux_pll_stats {
    uint64_t total_ns;
    uint32_t min_dt_ns;
    uint32_t max_dt_ns;
    uint32_t clamped_period_events;
    uint32_t resync_events;
    uint32_t phase_wraps;
    uint32_t cells_emitted;
} flux_pll_stats_t;

/* PLL: robustes Bitcell-Rekonstruktions-Frontend. */
int flux_mfm_decode_pll_raw(
    const uint32_t *dt_ns, size_t dt_count,
    const flux_pll_params_t *pll,
    uint8_t *out_bytes, size_t out_bytes_cap,
    size_t *out_bits_written,
    flux_pll_stats_t *stats_out
);

/* -------------------- Raw-MFM -> Data bitstream (optional helper) -------------------- */

typedef struct mfm_decode_stats {
    uint32_t sync_hits_4489;
    uint32_t clock_violations;
    uint32_t data_violations;
} mfm_decode_stats_t;

/* Find raw MFM sync pattern 0x4489 in a bitstream (MSB-first bytes).
 * Returns bit position of the *start* of the 16-bit pattern, or -1 if not found.
 */
int64_t mfm_find_sync_4489(const uint8_t *raw_bits, size_t raw_bits_len);

/* Convert raw MFM (clock+data bitcells) to pure data bitstream.
 * phase:
 *   0 => even cells are clock, odd cells are data
 *   1 => even cells are data,  odd cells are clock
 */
int mfm_raw_to_data_bits(
    const uint8_t *raw_bits, size_t raw_bits_len,
    int phase,
    uint8_t *out_bytes, size_t out_bytes_cap,
    size_t *out_data_bits,
    mfm_decode_stats_t *stats_out
);

#ifdef __cplusplus
}
#endif
