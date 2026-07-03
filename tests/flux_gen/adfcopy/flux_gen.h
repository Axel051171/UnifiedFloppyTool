/**
 * @file tests/flux_gen/adfcopy/flux_gen.h
 * @brief Synthetic Amiga-MFM flux generator (ADFCopy read payload format).
 *
 * ADFCopy is a Teensy-based (PJRC VID:PID 0x16C0:0x0483) USB-CDC serial
 * controller for Amiga 3.5" drives. Its CMD_READ_FLUX (0x06) reply is a
 * 3-byte header (status + LE16 length) followed by N flux bytes. Flux is
 * timing at 40 MHz / 25 ns per tick — the same resolution as SCP (the
 * runner header states this explicitly). This generator emits that wire
 * payload so the emulator's read path returns exactly what the firmware
 * would.
 *
 * Amiga DD geometry: 80 cylinders × 2 heads × 11 sectors × 512 bytes =
 * 880 KB, MFM, ~300 RPM (200 ms/revolution). Flux bytes here are a
 * simplified MFM cell stream (2/3/4 µs intervals) quantised to 25 ns
 * ticks and packed one byte per interval (values clamp to 255 ticks =
 * 6.375 µs, covering the DD cell family).
 *
 * Determinism: xorshift64*, caller seed; explicit byte serialisation.
 * Medium-safety: intervals kept within [1 µs, 12 µs].
 */
#ifndef UFT_TESTS_ADFC_FLUX_GEN_H
#define UFT_TESTS_ADFC_FLUX_GEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants ─────────────────────────────────────────────────────── */

/** ADFCopy sample clock: 40 MHz = 25 ns/tick (matches SCP). */
#define UFT_ADFC_SAMPLE_HZ    40000000u
#define UFT_ADFC_TICK_NS      25u
/** Amiga DD MFM cell = 2 µs. */
#define UFT_ADFC_CELL_NS      2000u
/** Forensic-safety interval window. */
#define UFT_ADFC_MIN_NS       1000u
#define UFT_ADFC_MAX_NS       12000u
/** Amiga DD geometry. */
#define UFT_ADFC_CYLINDERS    80
#define UFT_ADFC_HEADS        2
#define UFT_ADFC_MAX_TRACK    (UFT_ADFC_CYLINDERS * UFT_ADFC_HEADS - 1)  /* 159 */
#define UFT_ADFC_MIN_REVS     1
#define UFT_ADFC_MAX_REVS     5

/* Read-flux reply header status byte values. */
#define UFT_ADFC_FLUX_OK      0x00u
#define UFT_ADFC_FLUX_NODISK  0x01u
#define UFT_ADFC_FLUX_ERROR   0x02u

/* ─── Error codes ───────────────────────────────────────────────────── */
typedef enum {
    UFT_ADFC_GEN_OK              = 0,
    UFT_ADFC_GEN_ERR_NULL        = -1,
    UFT_ADFC_GEN_ERR_OUT_OF_SPEC = -2,
    UFT_ADFC_GEN_ERR_NOMEM       = -3,
} uft_adfc_gen_err_t;

/* ─── Defect classes (bitmask) ──────────────────────────────────────── */
typedef enum {
    UFT_ADFC_DEFECT_NONE      = 0,
    UFT_ADFC_DEFECT_WEAK_BITS = 1u << 0,  /* per-cell jitter (<=10%)     */
    UFT_ADFC_DEFECT_SHORT     = 1u << 1,  /* fewer flux bytes than a rev */
    UFT_ADFC_DEFECT_LONG_CELL = 1u << 2,  /* one over-long damaged gap   */
} uft_adfc_defect_flags_t;

/* ─── Params ────────────────────────────────────────────────────────── */
typedef struct {
    uint64_t seed;
    int      track;            /* 0..159 (cyl*2 + head) */
    int      revolutions;      /* 1..5 */
    uint32_t defects;          /* uft_adfc_defect_flags_t bitmask */
    uint8_t  weak_jitter_pct;  /* 1..10 when WEAK_BITS set */
} uft_adfc_gen_params_t;

/* ─── Output (the ADFCopy read-flux wire payload) ───────────────────── */
typedef struct {
    uint8_t  *bytes;            /* 3-byte header + flux bytes, malloc'd */
    size_t    bytes_len;
    uint8_t   status;           /* header status byte */
    uint16_t  flux_len;         /* header LE16 length (flux byte count) */
    uint32_t  revolution_ticks; /* nominal index-to-index, 25 ns ticks */
    double    rpm;
} uft_adfc_gen_flux_t;

/* ─── Public API ────────────────────────────────────────────────────── */

/** Build the ADFCopy CMD_READ_FLUX reply for one track: 3-byte header
 *  (status, LE16 length) + flux bytes. Allocates out->bytes; free via
 *  uft_adfc_gen_free. */
uft_adfc_gen_err_t uft_adfc_gen_flux(const uft_adfc_gen_params_t *params,
                                     uft_adfc_gen_flux_t *out);

void uft_adfc_gen_free(uft_adfc_gen_flux_t *s);

/** Count flux intervals outside the medium-safe [MIN_NS, MAX_NS] window
 *  (reads the flux bytes after the 3-byte header). 0 == safe. */
size_t uft_adfc_gen_count_unsafe(const uft_adfc_gen_flux_t *s);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_ADFC_FLUX_GEN_H */
