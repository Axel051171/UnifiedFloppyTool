#ifndef OTDR_EVENT_CORE_V2_H
#define OTDR_EVENT_CORE_V2_H

/*
 * OTDR Event Core v2 (C99)
 * ------------------------
 * v2 adds:
 *   - LOCAL sigma estimation (robust MAD) on Δ-trace using a window + stride
 *   - MERGE logic: REFLECTION spike + nearby ATTENUATION step => one combined event
 *
 * Pipeline:
 *   amp[] -> delta[] -> features (env_rms, local noise_sigma, snr_db) -> labels -> segments (+merge)
 *
 * Build:
 *   cc -O2 -std=c99 -Wall -Wextra -pedantic otdr_event_core_v2.c example_main.c -lm -o otdr_v2_demo
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OTDR_EVT_NONE = 0,
    OTDR_EVT_REFLECTION = 1,
    OTDR_EVT_ATTENUATION = 2,
    /* v2: merged event (typical connector: Fresnel reflection + insertion loss step) */
    OTDR_EVT_REFLECT_LOSS = 3
} otdr_event_t;

typedef struct {
    float amp;
    float delta;

    float env_rms;

    /* v2: local sigma per sample (robust MAD on Δ in a window) */
    float noise_sigma;

    float snr_db;
} otdr_features_t;

typedef struct {
    otdr_event_t label;
    float confidence;    /* 0..1 */
} otdr_event_result_t;

typedef struct {
    size_t window;          /* envelope RMS window */
    float  snr_floor_db;
    float  snr_ceil_db;

    /* baseline thresholds */
    float  thr_reflect_snr_db;
    float  thr_atten_snr_db;
    float  min_env_rms;

    /* MAD scale factor: sigma ≈ mad_scale * MAD */
    float  mad_scale;

    /* v2: local sigma settings */
    int    local_sigma_enable;   /* 1 = use local sigma, 0 = global sigma */
    size_t sigma_window;         /* samples in sigma window (trailing) */
    size_t sigma_stride;         /* recompute sigma every stride samples */
    float  sigma_min;            /* clamp sigma >= sigma_min */
} otdr_config;

/* Segment output (RLE + merge) */
enum {
    OTDR_SEG_FLAG_NONE = 0u,
    OTDR_SEG_FLAG_MERGED = 1u << 0
};

typedef struct {
    size_t start;       /* inclusive */
    size_t end;         /* inclusive */
    otdr_event_t label;
    float mean_conf;
    uint32_t flags;
} otdr_segment_t;

typedef struct {
    /* merge a REFLECTION with a nearby ATTENUATION */
    size_t merge_gap_max;        /* max NONE-gap between spike and step */
    size_t min_reflection_len;   /* min run length to treat as reflection */
    size_t min_atten_len;        /* min run length to treat as attenuation */
} otdr_merge_config;

otdr_config otdr_default_config(void);
otdr_merge_config otdr_default_merge_config(void);

int otdr_extract_features(const float *amp, size_t n,
                          const otdr_config *cfg,
                          otdr_features_t *out_feat);

int otdr_classify_baseline(const otdr_features_t *feat, size_t n,
                           const otdr_config *cfg,
                           otdr_event_result_t *out_res);

int otdr_detect_events_baseline(const float *amp, size_t n,
                                const otdr_config *cfg,
                                otdr_features_t *out_feat,        /* optional, may be NULL */
                                otdr_event_result_t *out_res);    /* required */

/* RLE segmentation (no merge). Returns number of segments written. */
size_t otdr_rle_segments(const otdr_event_result_t *res, size_t n,
                         otdr_segment_t *seg_out, size_t max_seg);

/* v2: RLE segmentation + merge (REFLECTION + nearby ATTENUATION => REFLECT_LOSS).
 * Returns number of segments written.
 */
size_t otdr_rle_segments_merged(const otdr_event_result_t *res, size_t n,
                                const otdr_merge_config *mcfg,
                                otdr_segment_t *seg_out, size_t max_seg);

#ifdef __cplusplus
}
#endif

#endif /* OTDR_EVENT_CORE_V2_H */
