#ifndef OTDR_EVENT_CORE_V12_H
#define OTDR_EVENT_CORE_V12_H
/*
 * OTDR Event Core v12 (C99)
 * =========================
 * Export / Integration — Final Module
 *
 * Provides:
 *   1) EXPORT FORMATS: Serialize analysis results to JSON, CSV, binary
 *   2) GOLDEN VECTORS: Built-in reference traces with known-good results
 *      for regression testing and validation
 *   3) END-TO-END API: Single-call interface that runs the full pipeline
 *      (v9 → v8 → v10) and returns a comprehensive report
 *   4) VERSION REGISTRY: Tracks all module versions for reproducibility
 *
 * Build:
 *   cc -O2 -std=c99 -Wall -Wextra -pedantic otdr_event_core_v12.c -lm -c
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Export format
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    OTDR12_FMT_JSON   = 0,
    OTDR12_FMT_CSV    = 1,
    OTDR12_FMT_BINARY = 2
} otdr12_format_t;

/* Binary export header (fixed 64 bytes) */
typedef struct {
    char     magic[4];       /* "UFTx" */
    uint32_t version;        /* 12 */
    uint32_t flags;          /* bit 0: has_events, bit 1: has_confidence, bit 2: has_integrity */
    uint32_t n_samples;
    uint32_t n_events;
    uint32_t n_segments;
    float    mean_confidence;
    float    integrity_score;
    float    overall_quality;
    uint32_t reserved[5];
} otdr12_bin_header_t;

/* ═══════════════════════════════════════════════════════════════════
 * Golden vector
 * ═══════════════════════════════════════════════════════════════════ */

#define OTDR12_GOLDEN_MAX   8
#define OTDR12_GOLDEN_SIZE  4096

typedef struct {
    const char *name;
    const char *description;
    size_t      n;
    float       expected_integrity;   /* ±tolerance */
    float       expected_confidence;
    size_t      expected_min_events;
    size_t      expected_max_events;
    size_t      expected_min_flagged;
    float       tolerance;
} otdr12_golden_info_t;

/* ═══════════════════════════════════════════════════════════════════
 * End-to-end result
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t  type;
    uint32_t start;
    uint32_t end;
    float    confidence;
    float    severity;
    uint8_t  flags;
} otdr12_event_t;

typedef struct {
    size_t  start;
    size_t  end;
    float   mean_confidence;
    size_t  rank;
} otdr12_segment_t;

typedef struct {
    /* Dimensions */
    size_t  n_samples;
    size_t  n_events;
    size_t  n_segments;

    /* Integrity (v9) */
    float   integrity_score;
    size_t  flagged_samples;
    size_t  dropout_count;
    size_t  saturated_count;
    size_t  stuck_count;
    size_t  deadzone_count;

    /* Confidence (v10) */
    float   mean_confidence;
    float   median_confidence;
    float   min_confidence;
    float   max_confidence;
    size_t  high_conf_count;
    size_t  mid_conf_count;
    size_t  low_conf_count;

    /* Overall */
    float   overall_quality;    /* 0..1 composite */

    /* Arrays (owned by result, freed by otdr12_free_result) */
    uint8_t           *flags;       /* per-sample integrity flags */
    float             *confidence;  /* per-sample confidence */
    otdr12_event_t    *events;
    otdr12_segment_t  *segments;
} otdr12_result_t;

/* ═══════════════════════════════════════════════════════════════════
 * Version registry
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *module;
    const char *version;
    int         major;
    int         minor;
} otdr12_module_ver_t;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Run full end-to-end analysis on a float signal.
 * @param signal  Input samples
 * @param n       Number of samples (minimum 16)
 * @param result  Output (caller provides pointer, internals allocated)
 * @return 0 on success, <0 on error
 */
int otdr12_analyze(const float *signal, size_t n, otdr12_result_t *result);

/** Free result internals */
void otdr12_free_result(otdr12_result_t *result);

/**
 * Export result to buffer.
 * @param result  Analysis result
 * @param fmt     Export format
 * @param buf     Output buffer (caller-allocated)
 * @param buflen  Buffer capacity
 * @return Bytes written, or <0 on error. If buf==NULL, returns required size.
 */
int otdr12_export(const otdr12_result_t *result, otdr12_format_t fmt,
                  char *buf, size_t buflen);

/**
 * Golden vectors.
 */
size_t                     otdr12_golden_count(void);
const otdr12_golden_info_t *otdr12_golden_info(size_t idx);
int                        otdr12_golden_generate(size_t idx, float *out, size_t n);
int                        otdr12_golden_validate(size_t idx, const otdr12_result_t *result);

/**
 * Version registry.
 */
size_t                      otdr12_module_count(void);
const otdr12_module_ver_t  *otdr12_module_version(size_t idx);
const char                 *otdr12_full_version(void);

#ifdef __cplusplus
}
#endif
#endif /* OTDR_EVENT_CORE_V12_H */
