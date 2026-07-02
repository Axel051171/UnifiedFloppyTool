/**
 * @file tests/flux_gen/greaseweazle/flux_gen.h
 * @brief Synthetic flux generator for the Greaseweazle USB stream format.
 *
 * Produces a byte stream in the GW capture format the firmware streams
 * over USB (reference: keirf/greaseweazle v1.23 usb.py + UFT's
 * src/hal/uft_greaseweazle_full.c::uft_gw_encode_flux_stream).
 *
 * Wire format (one direction: device → host):
 *   0x00         → End of stream (terminator)
 *   0x01..0xF9   → 1-byte flux delta (1..249 ticks)
 *   0xFA..0xFE   → 2-byte: 250 + (val-250)*255 + next - 1   (delta 250..1524)
 *   0xFF         → Opcode prefix; next byte selects:
 *       0x01    Index   + N28 (index pulse timing)
 *       0x02    Space   + N28 (gap, no flux transition)
 *       0x03    Astable + N28 (astable marker)
 *
 * Tick clock: 72 MHz (F7 default) or 84 MHz (F7-Plus); selectable per
 * call. All "clean MFM" flux is generated at multiples of the cell time
 * (4 µs at 250 kbps DD).
 *
 * Defect classes (opt-in bitmask):
 *   - UFT_DEFECT_NONE         — clean MFM
 *   - UFT_DEFECT_WEAK_BITS    — randomised flux intervals
 *   - UFT_DEFECT_CRC_ERROR    — flip one byte so MFM CRC fails
 *   - UFT_DEFECT_NFA_BURST    — inject a Space+Astable NFA (no-flux-area)
 *                                used by some PC copy-protection
 *   - UFT_DEFECT_INDEX_JITTER — index pulse timing varies rev-to-rev
 *
 * Determinism: every generator takes a seed. xorshift64* RNG → same
 * seed yields byte-identical output across x86-64, ARM, STM32.
 *
 * Forensic-medium-safety: the generator REFUSES to emit intervals
 *   - < UFT_GW_FLUX_GEN_MIN_NS  (2 µs — below damages 5.25" media)
 *   - > UFT_GW_FLUX_GEN_MAX_NS  (10 ms — unphysical above this)
 *   Out-of-spec params return UFT_GW_FLUX_GEN_ERR_OUT_OF_SPEC.
 */
#ifndef UFT_TESTS_GW_FLUX_GEN_H
#define UFT_TESTS_GW_FLUX_GEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants ─────────────────────────────────────────────────────── */

/** Default sample frequencies (F7 = 72 MHz, F7-Plus = 84 MHz). */
#define UFT_GW_FLUX_GEN_FREQ_F7_HZ      72000000u
#define UFT_GW_FLUX_GEN_FREQ_F7P_HZ     84000000u

/** Default MFM cell time at 250 kbps (3.5" DD): 4 µs = 4000 ns. */
#define UFT_GW_FLUX_GEN_CELL_NS_DD      4000u

/** Forensic-safety guards. The generator REFUSES to emit timing
 *  outside this range. */
#define UFT_GW_FLUX_GEN_MIN_NS          2000u      /* 2 µs */
#define UFT_GW_FLUX_GEN_MAX_NS          10000000u  /* 10 ms */

/** GW flux-opcode codes (matches GW_FLUXOP_* in uft_greaseweazle_full.c). */
#define UFT_GW_FLUXOP_INDEX             1
#define UFT_GW_FLUXOP_SPACE             2
#define UFT_GW_FLUXOP_ASTABLE           3

/* ─── Error codes ───────────────────────────────────────────────────── */
typedef enum {
    UFT_GW_FLUX_GEN_OK              = 0,
    UFT_GW_FLUX_GEN_ERR_NULL        = -1,
    UFT_GW_FLUX_GEN_ERR_BUF_SMALL   = -2,
    UFT_GW_FLUX_GEN_ERR_OUT_OF_SPEC = -3,
    UFT_GW_FLUX_GEN_ERR_INVALID     = -4,
} uft_gw_flux_gen_err_t;

/* ─── Defect-class flags (bitmask, opt-in per call) ─────────────────── */
typedef enum {
    UFT_GW_DEFECT_NONE          = 0,
    UFT_GW_DEFECT_WEAK_BITS     = 1u << 0,
    UFT_GW_DEFECT_CRC_ERROR     = 1u << 1,
    UFT_GW_DEFECT_NFA_BURST     = 1u << 2,
    UFT_GW_DEFECT_INDEX_JITTER  = 1u << 3,
    UFT_GW_DEFECT_LONG_TRACK    = 1u << 4,
} uft_gw_defect_flags_t;

/* ─── Output container ─────────────────────────────────────────────── */
typedef struct {
    uint8_t  *bytes;            /* full GW stream incl. 0x00 EOS */
    size_t    bytes_len;
    uint32_t  rev_index_ticks[8];  /* per-rev expected index timing */
    int       rev_count;
    uint32_t  sample_freq_hz;      /* what tick clock the stream targets */
} uft_gw_flux_capture_t;

/* ─── Generator params ──────────────────────────────────────────── */
typedef struct {
    uint64_t                seed;          /* deterministic RNG seed */
    int                     revolutions;   /* 1..5 */
    uint32_t                index_period_ns;  /* e.g. 200_000_000 = 200 ms */
    uint32_t                transitions_per_rev;
    uint32_t                sample_freq_hz; /* 72 MHz (F7) or 84 MHz (F7+) */
    uft_gw_defect_flags_t   defects;
    uint8_t                 weak_jitter_pct;   /* 0..50 */
} uft_gw_flux_params_t;

/* ─── Public API ───────────────────────────────────────────────────── */

/** Generate a clean MFM-style flux stream (2/3/4-cell intervals at
 *  4 µs cell time, with per-rev Index opcodes). Honours params->defects.
 *  Allocates capture->bytes via malloc; caller frees via
 *  uft_gw_flux_gen_free(). */
uft_gw_flux_gen_err_t uft_gw_flux_gen_clean(
    const uft_gw_flux_params_t *params,
    uft_gw_flux_capture_t      *out_capture);

/** Free buffer allocated by uft_gw_flux_gen_clean. Safe on NULL. */
void uft_gw_flux_gen_free(uft_gw_flux_capture_t *capture);

/** Verify a generated capture is medium-safe — every emitted ns
 *  interval is within [UFT_GW_FLUX_GEN_MIN_NS, MAX_NS]. Returns
 *  number of out-of-spec intervals (0 = safe). */
size_t uft_gw_flux_gen_count_unsafe(const uft_gw_flux_capture_t *capture);

/** Decode the GW stream into ns intervals (helper for tests). Returns
 *  count of intervals written to out_ns. Mirrors the firmware-side
 *  decode in uft_gw_decode_flux_stream() but expresses results as
 *  ns rather than ticks (so tests can assert independent of tick freq). */
size_t uft_gw_flux_gen_decode_ns(
    const uft_gw_flux_capture_t *capture,
    uint32_t *out_ns, size_t out_cap);

/* ─── Defect-injector hooks (internal use, exposed for unit tests) ─── */

/** WEAK_BITS: jitter `cell_ns_inout` by ±weak_jitter_pct percent. */
uft_gw_flux_gen_err_t uft_gw_defect_weak_bits_apply(
    uint64_t *rng_state, uint8_t weak_jitter_pct,
    uint32_t *cell_ns_inout);

/** CRC_ERROR: corrupt one byte in the stream (deterministic offset). */
uft_gw_flux_gen_err_t uft_gw_defect_crc_error_apply(
    uft_gw_flux_capture_t *cap);

/** NFA_BURST: append a Space + Astable opcode pair to `out` at `*pos`. */
uft_gw_flux_gen_err_t uft_gw_defect_nfa_burst_emit(
    uint8_t *out, size_t out_cap, size_t *pos,
    uint32_t space_ticks, uint32_t astable_ticks);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_GW_FLUX_GEN_H */
