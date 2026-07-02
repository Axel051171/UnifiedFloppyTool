/**
 * @file tests/flux_gen/xum1541/flux_gen.h
 * @brief Synthetic GCR track generator for the XUM1541/ZoomFloppy path.
 *
 * The XUM1541 does NOT sample flux — it is an IEC-bus bridge. What a
 * host receives from a 1541-class drive over the parallel-cable nibbler
 * is a byte-aligned raw-GCR track image (the drive's read channel
 * hardware byte-syncs on the sync mark, so nibbler output IS byte-
 * aligned). This generator produces exactly that: one revolution of
 * raw GCR bytes per Commodore density zone.
 *
 * Reference (SPEC_STATUS: SOURCE-AUTHORITATIVE for the GCR layout):
 *   - Commodore 1541 service manual + VIC-1541 DOS ROM listings:
 *     header block  = GCR(0x08, hdr_cks, sector, track, id2, id1, 0x0F, 0x0F)
 *     data block    = GCR(0x07, data[256], data_cks, 0x00, 0x00)
 *     hdr_cks       = sector ^ track ^ id2 ^ id1
 *     data_cks      = XOR of the 256 data bytes
 *     sync          = >= 10 consecutive '1' bits (formatter writes 5 x 0xFF)
 *   - Density zones (300 rpm, bytes per revolution):
 *     zone 3: tracks  1-17, 21 sectors, 7692 bytes (307.7 kbps)
 *     zone 2: tracks 18-24, 19 sectors, 7142 bytes (285.7 kbps)
 *     zone 1: tracks 25-30, 18 sectors, 6666 bytes (266.7 kbps)
 *     zone 0: tracks 31-42, 17 sectors, 6250 bytes (250.0 kbps)
 *   Zone/sector tables are cross-checked in the test suite against the
 *   HAL SSOT `uft_xum_sectors_for_track()` (src/hal/uft_xum1541.c).
 *
 * Defect classes (opt-in bitmask, individually toggleable):
 *   - WEAK_BITS         degauss-style unstable region in one data block
 *   - CHECKSUM_ERROR    1541 DOS error 23 (data checksum mismatch)
 *   - SYNC_LONG         copy-protection long sync (40-byte 0xFF run)
 *   - SYNC_SHORT        sub-minimum sync (1 byte < 10-bit sync threshold)
 *   - KILLER_TRACK      no sync at all (RapidLok-style killer track)
 *   - DENSITY_MISMATCH  zone-3 bit density written on an inner track
 *   - HALF_TRACK        crosstalk-degraded read at track N + 0.5
 *
 * Determinism: xorshift64* RNG, seeded per call. Same seed = byte-
 * identical output on every platform (no host-endian leak: all output
 * is byte-serial).
 *
 * Forensic-medium-safety guards (the generator REFUSES to emit):
 *   - tracks outside stepper bounds (< 1 or > 42, half-track > 41.5) —
 *     out-of-bounds step sequences bang the 1541 head against the stop
 *   - track images longer than the density zone's byte budget — an
 *     overlong track written back to real media would overwrite its
 *     own beginning (write-splice destruction)
 */
#ifndef UFT_TESTS_XUM_FLUX_GEN_H
#define UFT_TESTS_XUM_FLUX_GEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants ─────────────────────────────────────────────────────── */

#define UFT_XUM_GCR_MIN_TRACK        1
#define UFT_XUM_GCR_MAX_TRACK        42   /* matches HAL sectors_for_track bound */
#define UFT_XUM_GCR_SYNC_LEN         5    /* formatter-standard sync: 5 x 0xFF  */
#define UFT_XUM_GCR_LONG_SYNC_LEN    40   /* SYNC_LONG defect run length        */
#define UFT_XUM_GCR_HEADER_GCR_LEN   10   /* 8 plain bytes -> 10 GCR            */
#define UFT_XUM_GCR_DATA_GCR_LEN     325  /* 260 plain bytes -> 325 GCR         */
#define UFT_XUM_GCR_GAP_LEN          9    /* header + inter-sector gap (0x55)   */

/* ─── Error codes ───────────────────────────────────────────────────── */
typedef enum {
    UFT_XUM_GCR_OK              = 0,
    UFT_XUM_GCR_ERR_NULL        = -1,
    UFT_XUM_GCR_ERR_BUF_SMALL   = -2,
    UFT_XUM_GCR_ERR_OUT_OF_SPEC = -3,
    UFT_XUM_GCR_ERR_INVALID     = -4,
} uft_xum_gcr_err_t;

/* ─── Defect-class flags (bitmask, opt-in per call) ─────────────────── */
typedef enum {
    UFT_XUM_DEFECT_NONE             = 0,
    UFT_XUM_DEFECT_WEAK_BITS        = 1u << 0,
    UFT_XUM_DEFECT_CHECKSUM_ERROR   = 1u << 1,
    UFT_XUM_DEFECT_SYNC_LONG        = 1u << 2,
    UFT_XUM_DEFECT_SYNC_SHORT       = 1u << 3,
    UFT_XUM_DEFECT_KILLER_TRACK     = 1u << 4,
    UFT_XUM_DEFECT_DENSITY_MISMATCH = 1u << 5,
    UFT_XUM_DEFECT_HALF_TRACK       = 1u << 6,
} uft_xum_defect_flags_t;

/* ─── Generator params / output ─────────────────────────────────────── */
typedef struct {
    uint64_t seed;               /* deterministic RNG seed */
    int      track;              /* 1..42; with HALF_TRACK: data at N+0.5 */
    uint8_t  id1, id2;           /* disk ID bytes (as in the BAM) */
    uint32_t defects;            /* uft_xum_defect_flags_t bitmask */
    uint8_t  weak_region_bytes;  /* WEAK_BITS region size; 0 => 32 */
} uft_xum_gcr_params_t;

typedef struct {
    uint8_t *bytes;              /* one revolution of raw GCR, malloc'd */
    size_t   len;                /* == zone byte budget */
    int      sector_count;       /* 0 for KILLER_TRACK */
    int      speed_zone;         /* zone actually used (density mismatch!) */
    size_t   budget;             /* zone byte budget the track must fit */
} uft_xum_gcr_track_t;

/* Parsed sector header (for test assertions). */
typedef struct {
    uint8_t sector, track, id1, id2;
    bool    checksum_ok;
} uft_xum_gcr_header_t;

/* ─── Zone tables (mirrors of the Commodore spec; SSOT cross-checked
 *     against uft_xum_sectors_for_track in the test suite) ───────────── */
int    uft_xum_gcr_speed_zone(int track);        /* 3..0, -1 = out of bounds */
int    uft_xum_gcr_sectors_for_zone(int zone);   /* 21/19/18/17, 0 = invalid */
size_t uft_xum_gcr_zone_budget(int zone);        /* bytes/rev, 0 = invalid   */

/* ─── GCR codec (CBM 4-to-5-bit group code) ─────────────────────────── */
void uft_xum_gcr_encode4(const uint8_t in[4], uint8_t out[5]);
/** Returns 0 on success, -1 if any 5-bit group is not a legal GCR code. */
int  uft_xum_gcr_decode5(const uint8_t in[5], uint8_t out[4]);

/* ─── Track generation ──────────────────────────────────────────────── */

/** Generate one revolution of raw GCR. Allocates out->bytes via malloc;
 *  caller frees via uft_xum_gcr_free(). Honours params->defects. */
uft_xum_gcr_err_t uft_xum_gcr_gen_track(const uft_xum_gcr_params_t *params,
                                        uft_xum_gcr_track_t *out);

/** Free buffer allocated by uft_xum_gcr_gen_track. Safe on NULL. */
void uft_xum_gcr_free(uft_xum_gcr_track_t *track);

/* ─── Analysis helpers (used by the test suite) ─────────────────────── */

/** Count byte-aligned sync runs (>= 2 consecutive 0xFF bytes). Valid GCR
 *  data cannot contain 2 consecutive 0xFF bytes (max legal run of '1'
 *  bits inside data is 8), so this detector has no false positives on
 *  clean tracks. */
size_t uft_xum_gcr_count_syncs(const uint8_t *bytes, size_t len);

/** Longest run of consecutive 0xFF bytes. */
size_t uft_xum_gcr_max_sync_run(const uint8_t *bytes, size_t len);

/** Parse all sector headers (sync -> GCR block starting with 0x08).
 *  Returns the number of headers found (<= max_out written to out). */
int uft_xum_gcr_parse_headers(const uint8_t *bytes, size_t len,
                              uft_xum_gcr_header_t *out, int max_out);

/** Parse all data blocks (sync -> GCR block starting with 0x07).
 *  ok_flags[k] = 1 if block k fully decodes AND its checksum is valid.
 *  Returns the number of data blocks found. */
int uft_xum_gcr_parse_data_blocks(const uint8_t *bytes, size_t len,
                                  uint8_t *ok_flags, int max_out);

/** Medium-safety validator: number of safety violations in the track
 *  (0 = safe to hypothetically write back). Checks: len <= budget,
 *  budget is a known zone budget. */
size_t uft_xum_gcr_count_unsafe(const uft_xum_gcr_track_t *track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_XUM_FLUX_GEN_H */
