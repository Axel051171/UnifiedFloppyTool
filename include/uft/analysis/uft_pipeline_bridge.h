/**
 * @file uft_pipeline_bridge.h
 * @brief UFT ↔ OTDR v11 Pipeline/Streaming Bridge
 *
 * Unified streaming interface for floppy flux analysis:
 *
 *   Input source          Bridge method
 *   ─────────────────     ───────────────────────
 *   float amplitude       uft_pipe_push_float()
 *   uint32 flux ns        uft_pipe_push_flux_ns()
 *   int16 analog          uft_pipe_push_analog()
 *
 * Pipeline stages (configurable):
 *   [v9 integrity] → [denoise] → [v8 detect] → [v10 confidence]
 *
 * Output via callbacks + final report.
 */

#ifndef UFT_PIPELINE_BRIDGE_H
#define UFT_PIPELINE_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Event (emitted per chunk)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t abs_start;
    uint32_t abs_end;
    uint32_t length;
    uint8_t  type;
    float    confidence;
    float    severity;
    uint8_t  flags;
} uft_pipe_event_t;

/* ═══════════════════════════════════════════════════════════════════
 * Chunk result (from callback)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t chunk_id;
    size_t   chunk_offset;
    size_t   chunk_len;

    size_t   integrity_regions;
    size_t   flagged_samples;
    float    integrity_score;

    const uft_pipe_event_t *events;
    size_t   event_count;

    float    mean_confidence;
    float    min_confidence;
} uft_pipe_chunk_t;

/* ═══════════════════════════════════════════════════════════════════
 * Callbacks
 * ═══════════════════════════════════════════════════════════════════ */

typedef void (*uft_pipe_cb_chunk_t)(const uft_pipe_chunk_t *chunk, void *ud);
typedef void (*uft_pipe_cb_event_t)(const uft_pipe_event_t *event, void *ud);

/* ═══════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    size_t  chunk_size;         /**< samples per processing chunk (default 8192) */
    size_t  overlap;            /**< overlap between chunks (default 256) */
    size_t  ring_capacity;      /**< ring buffer size (default 65536) */

    bool    enable_integrity;
    bool    enable_detect;
    bool    enable_confidence;

    bool    auto_repair;

    float   detect_threshold;   /**< detection SNR threshold */

    uft_pipe_cb_chunk_t on_chunk;
    uft_pipe_cb_event_t on_event;
    void               *user_data;
} uft_pipe_config_t;

/* ═══════════════════════════════════════════════════════════════════
 * Report
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    size_t   total_samples;
    uint32_t chunks_processed;
    size_t   total_events;
    size_t   total_flagged;
    float    mean_integrity;
    float    mean_confidence;
    float    min_confidence;
    float    overall_quality;   /**< composite 0..1 */
    bool     is_done;
} uft_pipe_report_t;

/* ═══════════════════════════════════════════════════════════════════
 * Context
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct uft_pipe_ctx uft_pipe_ctx_t;  /* opaque */

/* ═══════════════════════════════════════════════════════════════════
 * Error codes
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_PIPE_OK           =  0,
    UFT_PIPE_ERR_NULL     = -1,
    UFT_PIPE_ERR_NOMEM    = -2,
    UFT_PIPE_ERR_SMALL    = -3,
    UFT_PIPE_ERR_STATE    = -4,
    UFT_PIPE_ERR_INTERNAL = -5
} uft_pipe_error_t;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

uft_pipe_config_t uft_pipe_default_config(void);

uft_pipe_error_t uft_pipe_create(uft_pipe_ctx_t **ctx, const uft_pipe_config_t *cfg);
void             uft_pipe_destroy(uft_pipe_ctx_t *ctx);

/* Push data (streaming) */
uft_pipe_error_t uft_pipe_push_float(uft_pipe_ctx_t *ctx,
                                       const float *samples, size_t n);
uft_pipe_error_t uft_pipe_push_flux_ns(uft_pipe_ctx_t *ctx,
                                         const uint32_t *flux, size_t n);
uft_pipe_error_t uft_pipe_push_analog(uft_pipe_ctx_t *ctx,
                                        const int16_t *samples, size_t n);

/* Finalize */
uft_pipe_error_t uft_pipe_flush(uft_pipe_ctx_t *ctx);
uft_pipe_error_t uft_pipe_reset(uft_pipe_ctx_t *ctx);

/* Results */
uft_pipe_report_t uft_pipe_get_report(const uft_pipe_ctx_t *ctx);
uint32_t          uft_pipe_chunks_processed(const uft_pipe_ctx_t *ctx);
size_t            uft_pipe_total_events(const uft_pipe_ctx_t *ctx);

/* Utilities */
const char *uft_pipe_error_str(uft_pipe_error_t e);
const char *uft_pipe_version(void);

#ifdef __cplusplus
}
#endif
#endif /* UFT_PIPELINE_BRIDGE_H */
