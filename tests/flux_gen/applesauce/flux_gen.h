/**
 * @file tests/flux_gen/applesauce/flux_gen.h
 * @brief Synthetic Apple-domain GCR flux generator (Applesauce payload format).
 *
 * Produces the binary flux payload the Applesauce device streams after a
 * `disk:readx` capture, per the in-tree transport contract
 * (src/hardware_providers/applesauce_provider_v2.h ApplesauceReadResult):
 * each 32-bit LITTLE-ENDIAN value is one flux-transition delta in 8 MHz
 * ticks (125 ns per tick). See DIVERGENCES.md D-2 for why this framing
 * is itself only in-tree-contract-verified, not bench-verified.
 *
 * Two track families:
 *   - Apple II 5.25" DOS 3.3: 16-sector 6-and-2 GCR, 4 µs bit cell,
 *     D5 AA 96 address prolog (volume/track/sector/checksum in 4-and-4),
 *     D5 AA AD data prolog (342 six-bit values + XOR-chain checksum),
 *     DE AA epilogs, 10-bit self-sync FF gaps.
 *   - Mac 3.5" 400K/800K GCR: 2 µs bit cell, 5 speed zones
 *     (12/11/10/9/8 sectors per track), zone-nominal spindle RPM.
 *     Data-field payload is simplified — see DIVERGENCES.md D-8.
 *
 * Defect classes (bitmask, opt-in per call; default = clean):
 *   - UFT_AS_DEFECT_WEAK_BITS        — per-interval timing jitter (<=10%)
 *   - UFT_AS_DEFECT_CHECKSUM_ERROR   — corrupt one data-field checksum
 *   - UFT_AS_DEFECT_SYNC_ANOMALY     — replace one self-sync gap with
 *                                       non-sync nibbles
 *   - UFT_AS_DEFECT_MISSING_ADDRESS  — omit one sector's address field
 *   - UFT_AS_DEFECT_CROSS_ZONE_SPEED — Mac: track mastered at the
 *                                       neighbouring zone's RPM
 *   - UFT_AS_DEFECT_FAKE_BITS        — protection-style long zero-run
 *                                       (>=5 bit cells without transition;
 *                                       MC3470 AGC reads garbage there)
 *
 * Determinism: xorshift64* RNG, caller-supplied seed. Same seed =>
 * byte-identical output on every platform (no host-endian leak — the
 * LE32 serialisation is explicit byte-by-byte).
 *
 * Forensic-medium-safety: the generator REFUSES to emit
 *   - intervals < UFT_AS_FLUX_GEN_MIN_NS (1.8 µs = 2 µs cell − 10%)
 *   - intervals > UFT_AS_FLUX_GEN_MAX_NS (10 ms — unphysical)
 *   - weak-bit jitter > UFT_AS_FLUX_GEN_MAX_JITTER_PCT (10% — beyond
 *     that a write-back would push transitions out of spec)
 *   - out-of-bounds track positions (5.25": 0..34, 3.5": 0..79)
 * Out-of-spec params return UFT_AS_FLUX_GEN_ERR_OUT_OF_SPEC.
 */
#ifndef UFT_TESTS_AS_FLUX_GEN_H
#define UFT_TESTS_AS_FLUX_GEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants ─────────────────────────────────────────────────────── */

/** Applesauce sample clock: 8 MHz = 125 ns/tick. Cross-checked against
 *  the HAL SSOT (uft_as_get_sample_clock / uft_as_ticks_to_ns) in
 *  test_applesauce_emulator.c. */
#define UFT_AS_FLUX_GEN_SAMPLE_HZ    8000000u

/** Apple II 5.25" GCR bit cell: 4 µs (250 kbit/s) = 32 ticks. */
#define UFT_AS_FLUX_GEN_CELL_525_NS  4000u

/** Mac 3.5" GCR bit cell: 2 µs (500 kbit/s) = 16 ticks. */
#define UFT_AS_FLUX_GEN_CELL_35_NS   2000u

/** Forensic-safety guards (see file header). */
#define UFT_AS_FLUX_GEN_MIN_NS          1800u
#define UFT_AS_FLUX_GEN_MAX_NS          10000000u
#define UFT_AS_FLUX_GEN_MAX_JITTER_PCT  10u

/** Track bounds (write-back stepper safety). */
#define UFT_AS_FLUX_GEN_MAX_TRACK_525   34
#define UFT_AS_FLUX_GEN_MAX_TRACK_35    79

/** Nominal 5.25" spindle speed. */
#define UFT_AS_FLUX_GEN_RPM_525         300.0

/* ─── Error codes ───────────────────────────────────────────────────── */
typedef enum {
    UFT_AS_FLUX_GEN_OK              = 0,
    UFT_AS_FLUX_GEN_ERR_NULL        = -1,
    UFT_AS_FLUX_GEN_ERR_BUF_SMALL   = -2,
    UFT_AS_FLUX_GEN_ERR_OUT_OF_SPEC = -3,
    UFT_AS_FLUX_GEN_ERR_INVALID     = -4,
} uft_as_flux_gen_err_t;

/* ─── Defect-class flags (bitmask, opt-in per call) ─────────────────── */
typedef enum {
    UFT_AS_DEFECT_NONE             = 0,
    UFT_AS_DEFECT_WEAK_BITS        = 1u << 0,
    UFT_AS_DEFECT_CHECKSUM_ERROR   = 1u << 1,
    UFT_AS_DEFECT_SYNC_ANOMALY     = 1u << 2,
    UFT_AS_DEFECT_MISSING_ADDRESS  = 1u << 3,
    UFT_AS_DEFECT_CROSS_ZONE_SPEED = 1u << 4,
    UFT_AS_DEFECT_FAKE_BITS        = 1u << 5,
} uft_as_defect_flags_t;

/* ─── Generator params ──────────────────────────────────────────────── */
typedef struct {
    uint64_t seed;             /* deterministic RNG seed */
    int      track;            /* 0..34 (5.25") / 0..79 (3.5") */
    int      side;             /* 0 (5.25") / 0..1 (3.5") */
    uint8_t  volume;           /* 5.25" address-field volume (DOS: 254) */
    uint32_t defects;          /* uft_as_defect_flags_t bitmask */
    uint8_t  weak_jitter_pct;  /* 1..10 when WEAK_BITS set */
} uft_as_flux_params_t;

/* ─── Output container ──────────────────────────────────────────────── */
typedef struct {
    uint8_t  *bytes;             /* LE32 tick deltas, malloc'd */
    size_t    bytes_len;         /* == 4 * transition_count */
    uint32_t  transition_count;
    uint32_t  revolution_ticks;  /* index-to-index, 8 MHz ticks */
    double    rpm;               /* spindle speed the track was mastered at */
    int       sector_count;      /* sectors encoded on the track */
} uft_as_flux_capture_t;

/* ─── Public API ────────────────────────────────────────────────────── */

/** Generate one full Apple II 5.25" DOS 3.3 16-sector GCR track.
 *  Track is gap-padded with self-sync to one nominal 300-RPM
 *  revolution. Allocates capture->bytes; free via uft_as_flux_gen_free. */
uft_as_flux_gen_err_t uft_as_flux_gen_a2_525(
    const uft_as_flux_params_t *params,
    uft_as_flux_capture_t      *out);

/** Generate one Mac 3.5" GCR track in the speed zone for params->track.
 *  Sector count + spindle RPM come from the zone table below. */
uft_as_flux_gen_err_t uft_as_flux_gen_mac_35(
    const uft_as_flux_params_t *params,
    uft_as_flux_capture_t      *out);

/** Free buffer allocated by the generators. Safe on NULL / empty. */
void uft_as_flux_gen_free(uft_as_flux_capture_t *capture);

/** Deserialize the LE32 payload back into tick deltas. Returns count
 *  written to out_ticks (<= out_cap). */
size_t uft_as_flux_gen_decode_ticks(const uft_as_flux_capture_t *capture,
                                    uint32_t *out_ticks, size_t out_cap);

/** Count intervals outside the medium-safe [MIN_NS, MAX_NS] window.
 *  0 == safe to (hypothetically) feed to a write path. */
size_t uft_as_flux_gen_count_unsafe(const uft_as_flux_capture_t *capture);

/** Decode flux back into disk nibbles the way an Apple controller's
 *  shift register would: each interval is rounded to bit cells of
 *  cell_ns; bits shift in MSB-first; when bit 7 of the register is set
 *  a nibble is complete and the register resets. Returns nibble count
 *  written to out (<= out_cap). Used by tests to assert prolog /
 *  checksum structure. */
size_t uft_as_flux_gen_decode_nibbles(const uft_as_flux_capture_t *capture,
                                      uint32_t cell_ns,
                                      uint8_t *out, size_t out_cap);

/* ─── 6-and-2 GCR table access (for test-side verification) ─────────── */

/** 6-bit value (0..63) → disk nibble (0x96..0xFF). Returns 0 on
 *  out-of-range input. */
uint8_t uft_as_flux_gcr62_nibble(uint8_t six);

/** Disk nibble → 6-bit value, or -1 if not a valid 6-and-2 nibble. */
int uft_as_flux_gcr62_detrans(uint8_t nibble);

/* ─── Mac 3.5" speed-zone table (SSOT for the Mac generator) ────────── */

/** Zone index (0..4) for a track (0..79); -1 out of range. */
int uft_as_flux_mac_zone_for_track(int track);

/** Sectors per track for a track (12/11/10/9/8); -1 out of range. */
int uft_as_flux_mac_sectors_for_track(int track);

/** Nominal spindle RPM for a zone (0..4); 0.0 out of range.
 *  Values are the commonly documented Sony-drive nominals
 *  (~393.38 / 429.17 / 472.14 / 524.57 / 590.11) — see
 *  DIVERGENCES.md D-9 for tolerance honesty. */
double uft_as_flux_mac_rpm_for_zone(int zone);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_AS_FLUX_GEN_H */
