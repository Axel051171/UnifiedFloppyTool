#ifndef OTDR_EVENT_CORE_V9_H
#define OTDR_EVENT_CORE_V9_H
/*
 * OTDR Event Core v9 (C99)
 * ========================
 * Signal-quality / integrity module — detects hardware-level anomalies
 * that the event detector (v2/v8) is not designed to catch:
 *
 *   1) DROPOUT: amplitude falls below noise floor (≈0) for ≥ min_run samples
 *      → head lift-off, media hole, missing flux transitions
 *
 *   2) SATURATION / CLIPPING: amplitude stuck at ADC rail (max or min)
 *      → preamp overload, strong reflection, AGC failure
 *
 *   3) STUCK-AT: amplitude constant (Δ≈0) for ≥ min_run samples
 *      → ADC fault, frozen DMA, hardware glitch
 *
 *   4) DEAD ZONE: extended region where SNR < threshold
 *      → post-reflection dead zone, media damage, end-of-fiber
 *
 *   5) REPAIR FLAGS: per-sample bitmask indicating data quality
 *      → enables downstream stages to exclude/interpolate bad samples
 *
 * Pipeline position:
 *   raw signal → [v9 integrity check] → repair_flags[]
 *                                      → [v8 event detect] (can skip flagged)
 *                                      → [v10 confidence] (incorporates flags)
 *
 * Build:
 *   cc -O2 -std=c99 -Wall -Wextra -pedantic otdr_event_core_v9.c -lm -c
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Anomaly types
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    OTDR9_ANOM_NONE       = 0,
    OTDR9_ANOM_DROPOUT    = 1,   /* signal near zero */
    OTDR9_ANOM_SATURATED  = 2,   /* clipped at rail */
    OTDR9_ANOM_STUCK      = 3,   /* constant value (delta≈0) */
    OTDR9_ANOM_DEADZONE   = 4    /* extended low-SNR region */
} otdr9_anomaly_t;

/* Per-sample repair flags (bitmask) */
enum {
    OTDR9_FLAG_OK            = 0u,
    OTDR9_FLAG_DROPOUT       = 1u << 0,   /* amplitude dropout */
    OTDR9_FLAG_CLIPPED_HIGH  = 1u << 1,   /* saturated high */
    OTDR9_FLAG_CLIPPED_LOW   = 1u << 2,   /* saturated low */
    OTDR9_FLAG_STUCK         = 1u << 3,   /* stuck-at fault */
    OTDR9_FLAG_DEADZONE      = 1u << 4,   /* in dead zone */
    OTDR9_FLAG_REPAIRED      = 1u << 5,   /* was repaired (interpolated) */
    OTDR9_FLAG_EXCLUDE       = 1u << 6    /* should be excluded from analysis */
};

/* ═══════════════════════════════════════════════════════════════════
 * Anomaly region descriptor
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    otdr9_anomaly_t type;
    size_t          start;       /* inclusive */
    size_t          end;         /* inclusive */
    float           severity;    /* 0..1: how bad */
    float           mean_value;  /* mean amplitude in region */
    float           stuck_value; /* for STUCK: the constant value */
    float           snr_db;      /* for DEADZONE: mean SNR */
} otdr9_region_t;

/* ═══════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Dropout detection */
    float   dropout_threshold;   /* abs amplitude below this = dropout (default 1e-4) */
    size_t  dropout_min_run;     /* minimum consecutive samples (default 3) */

    /* Saturation / clipping */
    float   clip_high;           /* upper clipping rail (default +0.99) */
    float   clip_low;            /* lower clipping rail (default -0.99) */
    size_t  clip_min_run;        /* minimum consecutive clipped samples (default 2) */
    float   clip_auto_range;     /* auto-detect rails from data range (0=off, 0.99=top 1%) */

    /* Stuck-at detection */
    float   stuck_max_delta;     /* max |Δ| to consider "stuck" (default 1e-6) */
    size_t  stuck_min_run;       /* minimum consecutive stuck samples (default 5) */

    /* Dead zone */
    float   deadzone_snr_db;     /* SNR below this = dead zone (default 3.0) */
    size_t  deadzone_min_run;    /* minimum consecutive low-SNR samples (default 64) */
    float   deadzone_sigma_win;  /* noise window for local SNR (default 1024) */

    /* Repair */
    int     auto_repair;         /* 1 = interpolate dropouts/stuck (default 0) */
    int     mark_exclude;        /* 1 = set EXCLUDE flag on all anomalies (default 1) */

    /* Noise estimation */
    float   mad_scale;           /* MAD→σ scale factor (default 1.4826) */
} otdr9_config_t;

/* ═══════════════════════════════════════════════════════════════════
 * Summary
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    size_t  total_regions;
    size_t  dropout_count;
    size_t  saturated_count;
    size_t  stuck_count;
    size_t  deadzone_count;

    size_t  dropout_samples;     /* total samples in dropouts */
    size_t  saturated_samples;
    size_t  stuck_samples;
    size_t  deadzone_samples;

    size_t  flagged_samples;     /* total samples with any flag */
    float   flagged_fraction;
    size_t  repaired_samples;

    float   integrity_score;     /* 1.0 = perfect, 0.0 = all bad */
    size_t  samples_analyzed;
} otdr9_summary_t;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

otdr9_config_t otdr9_default_config(void);

/**
 * Scan signal for integrity anomalies.
 * @param amp        Input amplitude array (length n)
 * @param n          Number of samples
 * @param cfg        Configuration
 * @param flags_out  Per-sample repair flags (caller-allocated, length n)
 * @param regions    Output anomaly regions (caller-allocated)
 * @param max_regions  Capacity of regions array
 * @param summary    Output summary (optional, can be NULL)
 * @return Number of regions found, or <0 on error
 */
int otdr9_scan(const float *amp, size_t n,
               const otdr9_config_t *cfg,
               uint8_t *flags_out,
               otdr9_region_t *regions, size_t max_regions,
               otdr9_summary_t *summary);

/**
 * Auto-repair flagged samples by linear interpolation.
 * Modifies amp_inout in-place. Only repairs DROPOUT and STUCK regions.
 * flags are updated with OTDR9_FLAG_REPAIRED.
 * @return Number of samples repaired
 */
size_t otdr9_repair(float *amp_inout, size_t n, uint8_t *flags);

/** String helpers */
const char *otdr9_anomaly_str(otdr9_anomaly_t a);
const char *otdr9_flag_str(uint8_t flag);  /* returns first set flag name */

#ifdef __cplusplus
}
#endif
#endif /* OTDR_EVENT_CORE_V9_H */
