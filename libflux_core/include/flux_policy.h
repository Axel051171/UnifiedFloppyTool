#pragma once
/*
 * flux_policy.h — Capture/Decode/Write policy knobs
 *
 * Goal: collect "tool-like" options (think Alcohol 120% style) into one
 * structured configuration that can be applied consistently across
 *
 * "Wir bewahren Information – wir entscheiden nicht vorschnell, was wichtig ist."
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum flux_speed_mode {
    FLUX_SPEED_MINIMUM = 0,   /* safest, slowest */
    FLUX_SPEED_NORMAL  = 1,
    FLUX_SPEED_MAXIMUM = 2    /* fastest; may reduce retries/dwell */
} flux_speed_mode_t;

typedef enum flux_error_policy {
    FLUX_ERR_STRICT = 0,      /* first error aborts */
    FLUX_ERR_TOLERANT = 1,    /* continue, mark errors */
    FLUX_ERR_IGNORE = 2       /* try hard to continue; never abort on CRC */
} flux_error_policy_t;

typedef enum flux_scan_mode {
    FLUX_SCAN_STANDARD = 0,   /* decode once, accept */
    FLUX_SCAN_ADVANCED = 1    /* multi-rev + consensus + extra windowing */
} flux_scan_mode_t;

typedef enum flux_index_phase_precision {
    FLUX_DPM_OFF = 0,
    FLUX_DPM_NORMAL = 1,
    FLUX_DPM_HIGH = 2
} flux_index_phase_precision_t;

typedef struct flux_retry_policy {
    uint8_t max_revs;         /* max revolutions captured per track */
    uint8_t max_resyncs;      /* PLL resync budget */
    uint8_t max_retries;      /* retry budget for read/write ops */
    uint8_t settle_ms;        /* head settle delay */
} flux_retry_policy_t;

typedef struct flux_read_policy {
    flux_speed_mode_t speed;
    flux_error_policy_t errors;
    flux_scan_mode_t scan;

    /* UI-like options */
    bool ignore_read_errors;          /* continue even when decode fails */
    bool fast_error_skip;             /* skip quickly after an error */
    uint16_t advanced_scan_factor;    /* e.g. 100, 200; backend hint */

    /* "Sub-channel" is CD-specific; keep as generic side-channel hook. */
    bool read_sidechannel;            /* e.g. extra sensor/drive telemetry */

    /* DPM: index/position measurement for track alignment */
    flux_index_phase_precision_t dpm;

    /* Retry / capture behaviour */
    flux_retry_policy_t retry;
} flux_read_policy_t;

typedef struct flux_write_policy {
    flux_speed_mode_t speed;
    flux_error_policy_t errors;
    uint8_t max_retries;
    uint8_t settle_ms;
    bool verify_after_write;
    bool close_session;               /* finalize disk/session if applicable */
    bool underrun_protect;            /* enable write underrun tech if any */
} flux_write_policy_t;

typedef struct flux_policy {
    flux_read_policy_t  read;
    flux_write_policy_t write;
} flux_policy_t;

/* Reasonable defaults: preservation-friendly but not absurdly slow. */
void flux_policy_init_default(flux_policy_t *p);

#ifdef __cplusplus
}
#endif
