#ifndef OTDR_EVENT_CORE_V11_H
#define OTDR_EVENT_CORE_V11_H
/*
 * OTDR Event Core v11 (C99)
 * =========================
 * Pipeline API + Streaming
 *
 * Wraps v7–v10 modules into a unified chunked-processing pipeline:
 *
 *   1) RING BUFFER: Fixed-capacity sample ring for streaming input
 *      → Accepts chunks of any size; internally manages overlap
 *      → Configurable capacity and overlap (for context at chunk edges)
 *
 *   2) PIPELINE STAGES: Ordered sequence of processing steps
 *      STAGE_INTEGRITY  (v9) → per-sample flags + repair
 *      STAGE_DENOISE    (ext) → placeholder for φ-OTDR denoise
 *      STAGE_DETECT     (v8) → multi-scale event detection
 *      STAGE_CONFIDENCE (v10) → per-sample confidence map
 *
 *   3) CALLBACKS: User-provided functions called at stage completion
 *      → on_integrity(flags, n, user_data)
 *      → on_events(events, count, user_data)
 *      → on_confidence(conf, n, user_data)
 *      → on_chunk_done(chunk_id, user_data)
 *
 *   4) ZERO-COPY INTERFACE: Stages read from shared ring buffer
 *      → No intermediate allocations per chunk
 *      → Downstream stages receive pointers into ring
 *
 * Build:
 *   cc -O2 -std=c99 -Wall -Wextra -pedantic otdr_event_core_v11.c -lm -c
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════ */

#define OTDR11_MAX_STAGES     8
#define OTDR11_MAX_EVENTS_PER_CHUNK 1024

/* ═══════════════════════════════════════════════════════════════════
 * Pipeline stages
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    OTDR11_STAGE_INTEGRITY  = 0,   /* v9: dropout/clip/stuck/deadzone */
    OTDR11_STAGE_DENOISE    = 1,   /* placeholder: wavelet denoise */
    OTDR11_STAGE_DETECT     = 2,   /* v8: multi-scale event detection */
    OTDR11_STAGE_CONFIDENCE = 3    /* v10: confidence map */
} otdr11_stage_t;

typedef enum {
    OTDR11_STATE_IDLE      = 0,
    OTDR11_STATE_RUNNING   = 1,
    OTDR11_STATE_FLUSHING  = 2,
    OTDR11_STATE_DONE      = 3
} otdr11_state_t;

/* ═══════════════════════════════════════════════════════════════════
 * Lightweight event (emitted per chunk)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t  type;           /* event type code */
    uint32_t abs_start;      /* absolute sample offset */
    uint32_t abs_end;
    float    confidence;
    float    severity;
    uint8_t  flags;          /* integrity flags at event center */
} otdr11_event_t;

/* ═══════════════════════════════════════════════════════════════════
 * Chunk result (passed to callbacks)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t       chunk_id;
    size_t         chunk_offset;   /* absolute offset of this chunk */
    size_t         chunk_len;      /* samples in this chunk */

    /* Integrity (v9) */
    const uint8_t *integrity_flags;   /* per-sample flags (length = chunk_len) */
    size_t         integrity_regions;
    size_t         flagged_samples;
    float          integrity_score;

    /* Events (v8) */
    const otdr11_event_t *events;
    size_t               event_count;

    /* Confidence (v10) */
    const float   *confidence;        /* per-sample confidence (length = chunk_len) */
    float          mean_confidence;
    float          min_confidence;
} otdr11_chunk_result_t;

/* ═══════════════════════════════════════════════════════════════════
 * Callbacks
 * ═══════════════════════════════════════════════════════════════════ */

typedef void (*otdr11_cb_chunk_t)(const otdr11_chunk_result_t *result,
                                   void *user_data);

typedef void (*otdr11_cb_event_t)(const otdr11_event_t *event,
                                   void *user_data);

/* ═══════════════════════════════════════════════════════════════════
 * Ring buffer
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    float  *buf;
    size_t  capacity;      /* total ring capacity */
    size_t  len;           /* current samples in ring */
    size_t  head;          /* write position */
    size_t  tail;          /* read position */
} otdr11_ring_t;

/* ═══════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Ring buffer */
    size_t  ring_capacity;      /* default 65536 */
    size_t  chunk_size;         /* processing chunk size (default 8192) */
    size_t  overlap;            /* overlap between chunks (default 256) */

    /* Stage enables */
    int     enable_integrity;   /* default 1 */
    int     enable_denoise;     /* default 0 (placeholder) */
    int     enable_detect;      /* default 1 */
    int     enable_confidence;  /* default 1 */

    /* v9 integrity params */
    float   dropout_threshold;
    size_t  dropout_min_run;
    float   clip_high;
    float   clip_low;
    float   stuck_max_delta;
    size_t  stuck_min_run;
    int     auto_repair;

    /* v8 detection params */
    float   detect_snr_threshold;

    /* v10 confidence params */
    float   conf_w_agreement;
    float   conf_w_snr;
    float   conf_w_integrity;

    /* Callbacks */
    otdr11_cb_chunk_t on_chunk;
    otdr11_cb_event_t on_event;
    void             *user_data;
} otdr11_config_t;

/* ═══════════════════════════════════════════════════════════════════
 * Pipeline statistics
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    size_t   total_samples;
    uint32_t chunks_processed;
    size_t   total_events;
    size_t   total_flagged;
    float    mean_integrity;
    float    mean_confidence;
    float    min_confidence;
    otdr11_state_t state;
} otdr11_stats_t;

/* ═══════════════════════════════════════════════════════════════════
 * Pipeline context (opaque internals)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    otdr11_config_t cfg;
    otdr11_ring_t   ring;
    otdr11_stats_t  stats;

    /* Internal work buffers (allocated once) */
    uint8_t        *work_flags;
    float          *work_conf;
    otdr11_event_t *work_events;
    float          *work_chunk;     /* linearized chunk from ring */

    int initialized;
} otdr11_pipeline_t;

/* ═══════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════ */

otdr11_config_t otdr11_default_config(void);

int  otdr11_init(otdr11_pipeline_t *p, const otdr11_config_t *cfg);
void otdr11_free(otdr11_pipeline_t *p);

/**
 * Push samples into the ring buffer. Pipeline processes automatically
 * when enough data is available (chunk_size samples).
 * @return Number of chunks processed during this push, or <0 on error.
 */
int otdr11_push(otdr11_pipeline_t *p, const float *samples, size_t n);

/**
 * Flush remaining data in ring buffer (final partial chunk).
 * @return Number of chunks processed, or <0 on error.
 */
int otdr11_flush(otdr11_pipeline_t *p);

/**
 * Reset pipeline to initial state (reuse without free/init).
 */
void otdr11_reset(otdr11_pipeline_t *p);

/**
 * Get current pipeline statistics.
 */
otdr11_stats_t otdr11_get_stats(const otdr11_pipeline_t *p);

/** String helpers */
const char *otdr11_stage_str(otdr11_stage_t s);
const char *otdr11_state_str(otdr11_state_t s);

#ifdef __cplusplus
}
#endif
#endif /* OTDR_EVENT_CORE_V11_H */
