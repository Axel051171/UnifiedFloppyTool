/**
 * @file uft_otdr_bridge.h
 * @brief UFT ↔ OTDR Integration Bridge
 *
 * Connects UFT flux format parsers (KryoFlux, SCP, Greaseweazle, HFE)
 * to the OTDR signal analysis engine. Provides unified API for:
 *   - Track/disk quality assessment
 *   - Multi-read consensus weighting
 *   - Copy protection characterization
 *   - Media health scoring
 *
 * Usage:
 *   uft_otdr_context_t ctx;
 *   uft_otdr_init(&ctx, "atari_st");
 *   // for each track:
 *   uft_otdr_feed_kryoflux(&ctx, stream_data, stream_len, cyl, head);
 *   // or:
 *   uft_otdr_feed_scp(&ctx, scp_track, cyl, head);
 *   uft_otdr_analyze(&ctx);
 *   // results:
 *   uft_otdr_report_t report = uft_otdr_get_report(&ctx);
 *   uft_otdr_free(&ctx);
 */

#ifndef UFT_OTDR_BRIDGE_H
#define UFT_OTDR_BRIDGE_H

#include "uft/analysis/floppy_otdr.h"
#include "uft/analysis/tdfc.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════ */

/** Maximum revolutions to collect for multi-read analysis */
#define UFT_OTDR_MAX_REVOLUTIONS  8

/** Integration context */
typedef struct {
    otdr_disk_t        *disk;
    otdr_config_t       config;
    uint8_t             max_cylinders;
    uint8_t             max_heads;
    bool                analyzed;

    /* TDFC config for envelope profiling */
    uint32_t            tdfc_env_window;
    uint32_t            tdfc_snr_window;
    uint32_t            tdfc_step;
} uft_otdr_context_t;

/** Per-track summary (user-friendly) */
typedef struct {
    uint8_t             cylinder;
    uint8_t             head;
    otdr_quality_t      quality;
    float               jitter_rms_pct;
    float               snr_db;
    int                 health_score;   /**< 0-100 from TDFC envelope */
    uint32_t            event_count;
    uint32_t            weak_bitcells;
    bool                has_protection;
} uft_otdr_track_summary_t;

/** Disk-level report */
typedef struct {
    /* Overall */
    otdr_quality_t      overall_quality;
    float               overall_jitter_pct;
    int                 health_score;       /**< 0-100 average */
    uint32_t            total_tracks;
    uint32_t            analyzed_tracks;

    /* Sectors */
    uint32_t            total_sectors;
    uint32_t            good_sectors;
    uint32_t            bad_sectors;

    /* Events */
    uint32_t            total_events;
    uint32_t            critical_events;

    /* Protection */
    bool                has_protection;
    char                protection_type[64];
    uint32_t            protected_tracks;

    /* Worst track */
    uint8_t             worst_track_cyl;
    uint8_t             worst_track_head;
    float               worst_track_jitter;

    /* Per-track summaries */
    uft_otdr_track_summary_t *tracks;
    uint32_t            track_count;
} uft_otdr_report_t;

/* ═══════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Initialize OTDR context for a specific platform.
 * @param platform One of: "atari_st", "atari_st_11", "atari_falcon_hd",
 *                 "amiga", "pc_dd", "pc_hd", "c64", or NULL for auto.
 * @return 0 on success
 */
int uft_otdr_init(uft_otdr_context_t *ctx, const char *platform);

/**
 * Set disk geometry explicitly (overrides platform defaults).
 */
void uft_otdr_set_geometry(uft_otdr_context_t *ctx,
                           uint8_t cylinders, uint8_t heads);

/* ── Feed flux data from various sources ─────────────────── */

/**
 * Feed raw flux intervals (in nanoseconds) for a track.
 * Accepts multiple revolutions for multi-read weak-bit analysis.
 */
int uft_otdr_feed_flux_ns(uft_otdr_context_t *ctx,
                          const uint32_t *flux_ns, uint32_t count,
                          uint8_t cylinder, uint8_t head,
                          uint8_t revolution);

/**
 * Feed KryoFlux raw stream data.
 * Extracts flux intervals from stream chunks and feeds to OTDR.
 * @param stream   Raw KryoFlux stream data
 * @param length   Stream data length in bytes
 */
int uft_otdr_feed_kryoflux(uft_otdr_context_t *ctx,
                           const uint8_t *stream, uint32_t length,
                           uint8_t cylinder, uint8_t head);

/**
 * Feed SCP track data.
 * Extracts flux intervals from SCP revolution data.
 * @param scp_data   SCP track data (revolution headers + flux data)
 * @param length     SCP track data length
 * @param revolutions Number of revolutions in SCP data
 */
int uft_otdr_feed_scp(uft_otdr_context_t *ctx,
                      const uint8_t *scp_data, uint32_t length,
                      uint8_t cylinder, uint8_t head,
                      uint8_t revolutions);

/**
 * Feed Greaseweazle flux data.
 * Decodes variable-length encoding to flux intervals.
 */
int uft_otdr_feed_greaseweazle(uft_otdr_context_t *ctx,
                               const uint8_t *gw_data, uint32_t length,
                               uint8_t cylinder, uint8_t head);

/**
 * Feed flux data from int16 samples (e.g. oversampled analog capture).
 * Uses TDFC's i16-to-float conversion.
 */
int uft_otdr_feed_analog(uft_otdr_context_t *ctx,
                         const int16_t *samples, uint32_t count,
                         float sample_rate_hz,
                         uint8_t cylinder, uint8_t head);

/* ── Analysis ────────────────────────────────────────────── */

/**
 * Run full analysis on all loaded tracks.
 * @return 0 on success
 */
int uft_otdr_analyze(uft_otdr_context_t *ctx);

/**
 * Get analysis report.
 * Report is valid until uft_otdr_free() is called.
 */
uft_otdr_report_t uft_otdr_get_report(const uft_otdr_context_t *ctx);

/**
 * Get raw OTDR disk structure for advanced access.
 */
const otdr_disk_t *uft_otdr_get_disk(const uft_otdr_context_t *ctx);

/**
 * Get raw OTDR track structure.
 */
const otdr_track_t *uft_otdr_get_track(const uft_otdr_context_t *ctx,
                                        uint8_t cylinder, uint8_t head);

/* ── SNR-weighted multi-read consensus ────────────────────── */

/**
 * Compute SNR weights for multi-read alignment.
 * Returns array of weights (one per revolution) for the given track.
 * Higher SNR = higher weight in consensus decoding.
 *
 * @param weights    Output array (caller provides, size >= num_revolutions)
 * @param n_weights  Output: actual number of weights
 * @return 0 on success
 */
int uft_otdr_snr_weights(const uft_otdr_context_t *ctx,
                         uint8_t cylinder, uint8_t head,
                         float *weights, uint8_t *n_weights);

/**
 * Get per-region SNR profile for adaptive decoding.
 * Returns SNR values for equidistant regions along the track.
 *
 * @param n_regions  Number of regions to divide track into
 * @param snr_out    Output array (caller provides, size >= n_regions)
 * @return 0 on success
 */
int uft_otdr_region_snr(const uft_otdr_context_t *ctx,
                        uint8_t cylinder, uint8_t head,
                        uint32_t n_regions, float *snr_out);

/* ── Export ───────────────────────────────────────────────── */

/** Export full analysis report as text */
int uft_otdr_export_report(const uft_otdr_context_t *ctx, const char *path);

/** Export disk heatmap as PGM image */
int uft_otdr_export_heatmap(const uft_otdr_context_t *ctx, const char *path);

/** Export per-track CSV data */
int uft_otdr_export_track_csv(const uft_otdr_context_t *ctx,
                              uint8_t cylinder, uint8_t head,
                              const char *path);

/* ── Cleanup ─────────────────────────────────────────────── */

/** Free all resources */
void uft_otdr_free(uft_otdr_context_t *ctx);

/** Free report track summaries */
void uft_otdr_report_free(uft_otdr_report_t *report);

#ifdef __cplusplus
}
#endif

#endif /* UFT_OTDR_BRIDGE_H */
