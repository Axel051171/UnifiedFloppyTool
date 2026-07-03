/**
 * @file tests/flux_gen/fc5025/flux_gen.h
 * @brief Synthetic SECTOR generator for the FC5025 emulator.
 *
 * FC5025 is a sector-only device — CLAUDE.md hard rule: "FC5025 kann
 * keinen Flux lesen — nur Sektordaten". The device firmware decodes the
 * disk internally and delivers decoded sector payloads via USB CBW/CSW
 * (or the fcimage CLI). So unlike the flux controllers, this "generator"
 * produces per-track SECTOR data with per-sector CRC status, not flux.
 *
 * The forensically load-bearing property (provider Rule F-3): a
 * CRC-error sector must PRESERVE its divergent reads — re-reading a
 * marginal sector yields DIFFERENT bytes, and those divergent copies are
 * kept (SectorMarginal::divergent_reads.size() >= 2), never collapsed to
 * one "resolved" value. This generator models that: a weak sector's
 * bytes differ on each read via uft_fc_gen_read_sector().
 *
 * Determinism: xorshift64* seeded per call; a weak sector diverges
 * deterministically across read attempts (seed folds in the attempt
 * index), so tests are reproducible.
 */
#ifndef UFT_TESTS_FC_SECTOR_GEN_H
#define UFT_TESTS_FC_SECTOR_GEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Formats (subset of uft_fc_format_t, with real geometry) ───────── */
typedef enum {
    UFT_FC_GEN_APPLE_DOS33 = 0,  /* 35 trk × 16 sec × 256 B */
    UFT_FC_GEN_APPLE_DOS32 = 1,  /* 35 trk × 13 sec × 256 B */
    UFT_FC_GEN_IBM_FM_8    = 2,  /* 77 trk × 26 sec × 128 B (3740 SSSD) */
    UFT_FC_GEN_TRS80_SSSD  = 3,  /* 35 trk × 10 sec × 256 B */
    UFT_FC_GEN_FORMAT_COUNT
} uft_fc_gen_format_t;

/* ─── Per-sector status flags ───────────────────────────────────────── */
typedef enum {
    UFT_FC_SEC_OK        = 0,
    UFT_FC_SEC_CRC_ERROR = 1,   /* CSW reported a CRC error */
    UFT_FC_SEC_MISSING   = 2,   /* address mark not found   */
} uft_fc_sec_status_t;

/* ─── Defect classes (bitmask) ──────────────────────────────────────── */
typedef enum {
    UFT_FC_DEFECT_NONE        = 0,
    UFT_FC_DEFECT_CRC_SECTOR  = 1u << 0,  /* one sector CRC-bad + weak (diverges) */
    UFT_FC_DEFECT_MISSING_SEC = 1u << 1,  /* one sector missing (no address mark) */
    UFT_FC_DEFECT_WEAK_SECTOR = 1u << 2,  /* one sector diverges but CSW OK */
} uft_fc_defect_flags_t;

/* ─── Geometry ──────────────────────────────────────────────────────── */
typedef struct {
    int      tracks;
    int      sectors_per_track;
    uint16_t sector_size;
    double   nominal_rpm;   /* 300 for 5.25", 360 for 8" */
} uft_fc_geometry_t;

/* ─── Params ────────────────────────────────────────────────────────── */
typedef struct {
    uint64_t            seed;
    uft_fc_gen_format_t format;
    int                 track;       /* 0..geometry.tracks-1 */
    int                 side;        /* 0/1 */
    uint32_t            defects;     /* uft_fc_defect_flags_t bitmask */
    int                 defect_sector; /* which sector the defect hits (0-based) */
} uft_fc_gen_params_t;

/* ─── One generated track (decoded sectors) ─────────────────────────── */
typedef struct {
    uft_fc_gen_format_t format;
    int                 track;
    int                 side;
    int                 sector_count;
    uint16_t            sector_size;
    uint8_t            *data;          /* sector_count × sector_size bytes */
    uint8_t            *status;        /* sector_count × uft_fc_sec_status_t */
    uint64_t            base_seed;     /* for divergent re-reads */
    uint32_t            defects;
    int                 defect_sector;
} uft_fc_gen_track_t;

/* ─── Errors ────────────────────────────────────────────────────────── */
typedef enum {
    UFT_FC_GEN_OK              = 0,
    UFT_FC_GEN_ERR_NULL        = -1,
    UFT_FC_GEN_ERR_OUT_OF_SPEC = -2,
    UFT_FC_GEN_ERR_NOMEM       = -3,
} uft_fc_gen_err_t;

/* ─── Public API ────────────────────────────────────────────────────── */

/** Geometry for a format. Returns false on an unknown format. */
bool uft_fc_gen_geometry(uft_fc_gen_format_t fmt, uft_fc_geometry_t *out);

/** Build one decoded track. Allocates out->data / out->status; free via
 *  uft_fc_gen_free_track. */
uft_fc_gen_err_t uft_fc_gen_track(const uft_fc_gen_params_t *params,
                                  uft_fc_gen_track_t *out);

void uft_fc_gen_free_track(uft_fc_gen_track_t *t);

/** Read one sector's bytes into `buf` (>= sector_size). `attempt` selects
 *  the read attempt: for a WEAK/CRC sector the bytes DIVERGE with the
 *  attempt index (models a marginal re-read); for a clean sector every
 *  attempt returns the identical bytes. Returns the sector's status. */
uft_fc_sec_status_t uft_fc_gen_read_sector(const uft_fc_gen_track_t *t,
                                           int sector, int attempt,
                                           uint8_t *buf, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_FC_SECTOR_GEN_H */
