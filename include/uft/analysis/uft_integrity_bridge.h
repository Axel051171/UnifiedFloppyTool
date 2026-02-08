/**
 * @file uft_integrity_bridge.h
 * @brief UFT ↔ OTDR v9 Signal Integrity Bridge
 *
 * Maps OTDR signal-integrity concepts to floppy domain:
 *
 *   OTDR fiber domain            →  UFT floppy domain
 *   ──────────────────────────      ──────────────────────────
 *   Amplitude dropout           →  Missing flux transitions (head lift-off)
 *   Saturation / clipping       →  Preamp overload / AGC failure
 *   Stuck-at fault              →  DMA freeze / hardware glitch
 *   Dead zone (low SNR)         →  Media damage / worn-out region
 *   Repair (interpolation)      →  Gap-fill for decode retry
 *
 * Pipeline position:
 *   raw flux → [v9 integrity] → flags[] → [v8 detect] → [v10 confidence]
 *                             → repaired signal (optional)
 */

#ifndef UFT_INTEGRITY_BRIDGE_H
#define UFT_INTEGRITY_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Anomaly types (UFT domain)
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_INT_NORMAL      = 0,
    UFT_INT_DROPOUT     = 1,   /**< Missing transitions / head lift-off */
    UFT_INT_SATURATED   = 2,   /**< Preamp overload */
    UFT_INT_STUCK       = 3,   /**< DMA/hardware freeze */
    UFT_INT_DEADZONE    = 4    /**< Worn-out / damaged media */
} uft_integrity_type_t;

/* Per-sample flags */
enum {
    UFT_INT_FLAG_OK         = 0u,
    UFT_INT_FLAG_DROPOUT    = 1u << 0,
    UFT_INT_FLAG_CLIP_HIGH  = 1u << 1,
    UFT_INT_FLAG_CLIP_LOW   = 1u << 2,
    UFT_INT_FLAG_STUCK      = 1u << 3,
    UFT_INT_FLAG_DEADZONE   = 1u << 4,
    UFT_INT_FLAG_REPAIRED   = 1u << 5,
    UFT_INT_FLAG_EXCLUDE    = 1u << 6
};

/* ═══════════════════════════════════════════════════════════════════
 * Anomaly region
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_integrity_type_t type;
    size_t   start;
    size_t   end;
    size_t   length;
    float    severity;      /**< 0..1 */
    float    mean_value;
    float    stuck_value;   /**< for STUCK: the constant */
    float    snr_db;        /**< for DEADZONE: local SNR */
} uft_integrity_region_t;

/* ═══════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Dropout */
    float   dropout_threshold;
    size_t  dropout_min_run;

    /* Clipping */
    float   clip_high;
    float   clip_low;
    size_t  clip_min_run;
    bool    clip_auto_detect;   /**< auto-detect rails from data percentile */

    /* Stuck-at */
    float   stuck_max_delta;
    size_t  stuck_min_run;

    /* Dead zone */
    float   deadzone_snr_db;
    size_t  deadzone_min_run;

    /* Repair */
    bool    auto_repair;
    bool    mark_exclude;
} uft_integrity_config_t;

/* ═══════════════════════════════════════════════════════════════════
 * Report
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    size_t  total_regions;
    size_t  dropout_count;
    size_t  saturated_count;
    size_t  stuck_count;
    size_t  deadzone_count;

    size_t  dropout_samples;
    size_t  saturated_samples;
    size_t  stuck_samples;
    size_t  deadzone_samples;

    size_t  flagged_samples;
    float   flagged_fraction;
    size_t  repaired_samples;

    float   integrity_score;   /**< 1.0 = perfect, 0.0 = all bad */
    size_t  samples_analyzed;
} uft_integrity_report_t;

/* ═══════════════════════════════════════════════════════════════════
 * Context
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_integrity_config_t cfg;
    uft_integrity_report_t report;

    uft_integrity_region_t *regions;
    size_t  region_count;
    size_t  region_capacity;

    uint8_t *flags;
    size_t   flags_len;

    bool initialized;
} uft_integrity_ctx_t;

/* ═══════════════════════════════════════════════════════════════════
 * Error codes
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_INT_OK           =  0,
    UFT_INT_ERR_NULL     = -1,
    UFT_INT_ERR_NOMEM    = -2,
    UFT_INT_ERR_SMALL    = -3,
    UFT_INT_ERR_INTERNAL = -4
} uft_integrity_error_t;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

uft_integrity_config_t uft_integrity_default_config(void);

uft_integrity_error_t uft_integrity_init(uft_integrity_ctx_t *ctx,
                                          const uft_integrity_config_t *cfg);
void uft_integrity_free(uft_integrity_ctx_t *ctx);

/* Scan inputs */
uft_integrity_error_t uft_integrity_scan_float(uft_integrity_ctx_t *ctx,
                                                 const float *signal, size_t n);
uft_integrity_error_t uft_integrity_scan_flux_ns(uft_integrity_ctx_t *ctx,
                                                   const uint32_t *flux, size_t n);
uft_integrity_error_t uft_integrity_scan_analog(uft_integrity_ctx_t *ctx,
                                                  const int16_t *samples, size_t n);

/* Repair: modifies signal in-place, returns repaired count */
size_t uft_integrity_repair(uft_integrity_ctx_t *ctx, float *signal, size_t n);

/* Results */
size_t                         uft_integrity_count(const uft_integrity_ctx_t *ctx);
const uft_integrity_region_t * uft_integrity_get(const uft_integrity_ctx_t *ctx, size_t idx);
const uint8_t *                uft_integrity_flags(const uft_integrity_ctx_t *ctx, size_t *out_len);
uft_integrity_report_t         uft_integrity_get_report(const uft_integrity_ctx_t *ctx);

/* Utilities */
const char *uft_integrity_type_str(uft_integrity_type_t t);
const char *uft_integrity_flag_str(uint8_t flag);
const char *uft_integrity_error_str(uft_integrity_error_t e);
const char *uft_integrity_version(void);

#ifdef __cplusplus
}
#endif
#endif /* UFT_INTEGRITY_BRIDGE_H */
