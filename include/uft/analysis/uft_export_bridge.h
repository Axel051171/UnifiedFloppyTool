/**
 * @file uft_export_bridge.h
 * @brief UFT ↔ OTDR v12 Export/Integration Bridge
 *
 * Final integration layer providing:
 *   - Single-call end-to-end analysis (all inputs → report)
 *   - Export to JSON/CSV/Binary
 *   - Golden vector regression tests
 *   - Version registry for all modules
 *
 * This is the top-level API for the complete UFT-NX analysis pipeline.
 */

#ifndef UFT_EXPORT_BRIDGE_H
#define UFT_EXPORT_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Export formats
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_EXPORT_JSON   = 0,
    UFT_EXPORT_CSV    = 1,
    UFT_EXPORT_BINARY = 2
} uft_export_format_t;

/* ═══════════════════════════════════════════════════════════════════
 * Analysis result
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Integrity */
    float   integrity_score;
    size_t  flagged_samples;
    size_t  dropout_count;
    size_t  saturated_count;
    size_t  stuck_count;

    /* Confidence */
    float   mean_confidence;
    float   median_confidence;
    float   min_confidence;
    float   max_confidence;
    size_t  high_conf_count;
    size_t  mid_conf_count;
    size_t  low_conf_count;

    /* Events & segments */
    size_t  n_events;
    size_t  n_segments;

    /* Overall */
    float   overall_quality;
    size_t  n_samples;
} uft_export_report_t;

/* ═══════════════════════════════════════════════════════════════════
 * Context (opaque)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct uft_export_ctx uft_export_ctx_t;

/* ═══════════════════════════════════════════════════════════════════
 * Error codes
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_EXP_OK           =  0,
    UFT_EXP_ERR_NULL     = -1,
    UFT_EXP_ERR_NOMEM    = -2,
    UFT_EXP_ERR_SMALL    = -3,
    UFT_EXP_ERR_FORMAT   = -4,
    UFT_EXP_ERR_INTERNAL = -5
} uft_export_error_t;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

uft_export_error_t uft_export_create(uft_export_ctx_t **ctx);
void               uft_export_destroy(uft_export_ctx_t *ctx);

/* Analyze inputs */
uft_export_error_t uft_export_analyze_float(uft_export_ctx_t *ctx,
                                              const float *signal, size_t n);
uft_export_error_t uft_export_analyze_flux_ns(uft_export_ctx_t *ctx,
                                                const uint32_t *flux, size_t n);
uft_export_error_t uft_export_analyze_analog(uft_export_ctx_t *ctx,
                                               const int16_t *samples, size_t n);

/* Export */
uft_export_error_t uft_export_to_buffer(const uft_export_ctx_t *ctx,
                                          uft_export_format_t fmt,
                                          char *buf, size_t buflen,
                                          size_t *bytes_written);

/* Results */
uft_export_report_t uft_export_get_report(const uft_export_ctx_t *ctx);
bool                uft_export_has_result(const uft_export_ctx_t *ctx);

/* Golden vectors */
size_t uft_export_golden_count(void);
int    uft_export_golden_run(size_t idx);       /* 0=pass, >0=fail reason */
int    uft_export_golden_run_all(void);         /* 0=all pass */

/* Version info */
const char *uft_export_version(void);
const char *uft_export_pipeline_version(void);
size_t      uft_export_module_count(void);

/* Utilities */
const char *uft_export_error_str(uft_export_error_t e);
const char *uft_export_format_str(uft_export_format_t f);

#ifdef __cplusplus
}
#endif
#endif /* UFT_EXPORT_BRIDGE_H */
