/**
 * @file uft_decode_score.h
 * @brief Unified Decode Score System
 * 
 * Provides consistent scoring across all decoders.
 */

#ifndef UFT_DECODE_SCORE_H
#define UFT_DECODE_SCORE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Score weights (configurable)
 */
typedef struct {
    uint8_t crc_weight;         /* Default: 40 */
    uint8_t id_weight;          /* Default: 15 */
    uint8_t sequence_weight;    /* Default: 15 */
    uint8_t header_weight;      /* Default: 10 */
    uint8_t timing_weight;      /* Default: 15 */
    uint8_t protection_weight;  /* Default: 5 */
} uft_score_weights_t;

/**
 * @brief Decode score result
 */
typedef struct {
    /* Component scores (0 to weight max) */
    uint8_t crc_score;          /* 0-40: CRC/checksum correctness */
    uint8_t id_score;           /* 0-15: Track/sector ID validity */
    uint8_t sequence_score;     /* 0-15: Sector sequence correctness */
    uint8_t header_score;       /* 0-10: Header/sync structure */
    uint8_t timing_score;       /* 0-15: PLL/timing quality */
    uint8_t protection_score;   /* 0-5:  Protection pattern match */
    
    /* Aggregated */
    uint8_t total;              /* 0-100: Sum of components */
    uint8_t confidence;         /* 0-100: Statistical confidence */
    
    /* Diagnostic */
    char    reason[256];        /* Human-readable explanation */
    
    /* Flags */
    bool    crc_ok;
    bool    id_valid;
    bool    sequence_ok;
    bool    has_protection;
} uft_decode_score_t;

/**
 * @brief Default score weights
 */
static const uft_score_weights_t UFT_SCORE_WEIGHTS_DEFAULT = {
    .crc_weight = 40,
    .id_weight = 15,
    .sequence_weight = 15,
    .header_weight = 10,
    .timing_weight = 15,
    .protection_weight = 5
};

/**
 * @brief Initialize score to zero
 */
static inline void uft_score_init(uft_decode_score_t *score) {
    if (score) {
        *score = (uft_decode_score_t){0};
    }
}

/**
 * @brief Calculate total from components
 */
static inline void uft_score_calculate_total(uft_decode_score_t *score) {
    if (!score) return;
    score->total = score->crc_score + score->id_score + 
                   score->sequence_score + score->header_score +
                   score->timing_score + score->protection_score;
    if (score->total > 100) score->total = 100;
}

/**
 * @brief Score comparison (for sorting)
 * @return >0 if a better than b, <0 if b better, 0 if equal
 */
static inline int uft_score_compare(const uft_decode_score_t *a, 
                                     const uft_decode_score_t *b) {
    if (!a || !b) return 0;
    
    /* Primary: total score */
    if (a->total != b->total) return a->total - b->total;
    
    /* Secondary: CRC wins ties */
    if (a->crc_ok != b->crc_ok) return a->crc_ok ? 1 : -1;
    
    /* Tertiary: confidence */
    return a->confidence - b->confidence;
}

/**
 * @brief Score a sector decode result
 */
void uft_score_sector(
    uft_decode_score_t *score,
    bool crc_ok,
    int cylinder, int head, int sector,  /* For ID validation */
    int max_cylinder, int max_sector,
    double timing_jitter_ns,
    double timing_threshold_ns,
    bool protection_expected,
    bool protection_found);

/**
 * @brief Format score as string
 */
const char* uft_score_to_string(const uft_decode_score_t *score);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DECODE_SCORE_H */
