#ifndef UFT_TRIAGE_H
#define UFT_TRIAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UFT_TRIAGE_GREEN,    /* Good — standard format, readable */
    UFT_TRIAGE_YELLOW,   /* Warning — some CRC errors, DeepRead recommended */
    UFT_TRIAGE_RED,      /* Critical — severe damage, professional recovery */
    UFT_TRIAGE_WHITE     /* Copy protection detected — flux archiving recommended */
} uft_triage_level_t;

typedef struct {
    uft_triage_level_t level;
    int    quality_score;        /* 0-100 */
    int    tracks_sampled;
    int    sectors_ok;
    int    sectors_bad;
    int    sectors_weak;
    bool   protection_detected;
    char   protection_name[64];
    char   format_name[32];
    float  format_confidence;
    char   summary[256];         /* Human-readable summary */
    char   recommendation[256];  /* What to do next */
} uft_triage_result_t;

/* Run triage on a flux/image file — reads only 3 key tracks */
int uft_triage_analyze(const char *path, uft_triage_result_t *result);

/* Run triage on in-memory data */
int uft_triage_analyze_buffer(const uint8_t *data, size_t size,
                               const char *format_hint,
                               uft_triage_result_t *result);

const char *uft_triage_level_name(uft_triage_level_t level);
const char *uft_triage_level_emoji(uft_triage_level_t level);

#ifdef __cplusplus
}
#endif
#endif
