/**
 * @file tests/flux_gen/fluxengine/flux_gen.h
 * @brief Synthetic SCP-file generator for the FluxEngine emulator.
 *
 * FluxEngine has no wire protocol in UFT — the provider
 * (src/hardware_providers/fluxengine_provider_v2.cpp) runs the
 * `fluxengine` CLI, which on read writes an SCP file that UFT then
 * decodes with its own vetted SCP parser (src/flux/uft_scp_parser.c,
 * "do_read_raw_flux asks fluxengine to write an *SCP* file"). So this
 * generator emits a minimal but REAL SCP container, and the emulator
 * test decodes it with the PRODUCTION parser uft_scp_read_track_memory()
 * — every defect class maps to a specific uft_scp_err code. That makes
 * the generator a live conformance fixture for the SCP reader on the
 * FluxEngine path.
 *
 * SCP FILE LAYOUT (matches uft_scp_parser.c):
 *   [0..15]   16-byte header: "SCP", version, disk_type, revolutions@5,
 *             start_track@6, end_track@7, flags@8, bit_cell_width@9,
 *             heads@10, resolution@11, checksum(LE32)@12
 *   [16..687] 168 × LE32 track-offset table (only the target track set)
 *   TRK hdr   "TRK"(3) + track_number(1)
 *   per rev   index_time(LE32), track_length(LE32), data_offset(LE32,
 *             relative to the TRK header)
 *   flux      track_length × 16-bit BIG-ENDIAN samples
 *
 * period_ns = 25 ns × (1 + resolution). One 300-RPM revolution = 200 ms.
 *
 * Determinism: xorshift64*, caller seed; explicit byte serialisation, no
 * host-endian leak. Medium-safety: intervals kept within [1 µs, 200 µs].
 */
#ifndef UFT_TESTS_FE_FLUX_GEN_H
#define UFT_TESTS_FE_FLUX_GEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants ─────────────────────────────────────────────────────── */

/** SCP base period: 25 ns per tick at resolution 0. */
#define UFT_FE_SCP_BASE_PERIOD_NS  25u
/** DD MFM nominal cell = 4 µs. */
#define UFT_FE_CELL_DD_NS          4000u
/** Forensic-safety interval window. */
#define UFT_FE_MIN_NS              1000u
#define UFT_FE_MAX_NS              200000u
#define UFT_FE_MAX_TRACK           167
#define UFT_FE_MIN_REVS            1
#define UFT_FE_MAX_REVS            5

/* ─── Error codes ───────────────────────────────────────────────────── */
typedef enum {
    UFT_FE_GEN_OK              = 0,
    UFT_FE_GEN_ERR_NULL        = -1,
    UFT_FE_GEN_ERR_OUT_OF_SPEC = -2,
    UFT_FE_GEN_ERR_NOMEM       = -3,
} uft_fe_gen_err_t;

/* ─── Defect classes (bitmask) ───────────────────────────────────────
 *
 * flux-domain first, then container-level defects that map to a specific
 * SCP parser error the test asserts. Set at most one container defect. */
typedef enum {
    UFT_FE_DEFECT_NONE        = 0,
    UFT_FE_DEFECT_WEAK_BITS   = 1u << 0,  /* per-cell jitter (<=10%)     */
    UFT_FE_DEFECT_BAD_SIG     = 1u << 1,  /* corrupt "SCP" → ERR_SIGNATURE */
    UFT_FE_DEFECT_BAD_TRK_SIG = 1u << 2,  /* corrupt "TRK" → ERR_SIGNATURE */
    UFT_FE_DEFECT_ZERO_OFFSET = 1u << 3,  /* track offset 0 → ERR_TRACK  */
    UFT_FE_DEFECT_TRUNCATED   = 1u << 4,  /* cut flux short → ERR_READ   */
} uft_fe_defect_flags_t;

/* ─── Params ────────────────────────────────────────────────────────── */
typedef struct {
    uint64_t seed;
    int      track;            /* 0..167 */
    int      revolutions;      /* 1..5   */
    uint32_t cell_ns;          /* 0 => DD default */
    uint32_t defects;          /* uft_fe_defect_flags_t bitmask */
    uint8_t  weak_jitter_pct;  /* 1..10 when WEAK_BITS set */
} uft_fe_gen_params_t;

/* ─── Output ────────────────────────────────────────────────────────── */
typedef struct {
    uint8_t  *bytes;            /* the SCP file, malloc'd */
    size_t    bytes_len;
    uint32_t  flux_count;       /* flux transitions in the target track */
    uint32_t  index_time_ns;    /* per-rev index period (nominal) */
    double    rpm;              /* spindle speed mastered at */
    int       target_track;
} uft_fe_gen_scp_t;

/* ─── Public API ────────────────────────────────────────────────────── */

/** Build a minimal single-track SCP file the production parser accepts.
 *  Allocates out->bytes; free with uft_fe_gen_free. */
uft_fe_gen_err_t uft_fe_gen_scp(const uft_fe_gen_params_t *params,
                                uft_fe_gen_scp_t *out);

void uft_fe_gen_free(uft_fe_gen_scp_t *s);

/** Count flux intervals outside the medium-safe window (decodes the SCP
 *  track itself). 0 == safe to hypothetically write back. */
size_t uft_fe_gen_count_unsafe(const uft_fe_gen_scp_t *s);

/* ─── fluxengine `rpm` stdout formatting (protocol-contract fixture) ──
 *
 * The provider parses RPM from fluxengine's stdout with the regex
 * (\d+\.?\d*)\s*rpm (icase). These helpers emit the three documented
 * output shapes so a test (or the C++ provider) has a reference the
 * parser must accept. `variant` 0..2 selects the shape. */
void uft_fe_format_rpm_line(double rpm, int variant, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_FE_FLUX_GEN_H */
