/**
 * @file uft_merge_engine.h
 * @brief Multi-Read Merge Engine
 * 
 * Combines multiple read revolutions into best-of result.
 */

#ifndef UFT_MERGE_ENGINE_H
#define UFT_MERGE_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft_decode_score.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Merge strategies
 */
typedef enum {
    UFT_MERGE_MAJORITY,     /* Majority voting */
    UFT_MERGE_CRC_WINS,     /* CRC-OK sectors have priority */
    UFT_MERGE_HIGHEST_SCORE,/* Highest scored sector wins */
    UFT_MERGE_LATEST,       /* Last read wins (for debugging) */
} uft_merge_strategy_t;

/**
 * @brief Merge configuration
 */
typedef struct {
    uft_merge_strategy_t strategy;
    int min_agreements;      /* Minimum revs that must agree (default: 2) */
    bool preserve_weak_bits; /* Keep weak bit info in output */
    bool preserve_timing;    /* Keep timing info in output */
    int max_revolutions;     /* Max revs to consider (default: 10) */
} uft_merge_config_t;

/**
 * @brief Default merge config
 */
static const uft_merge_config_t UFT_MERGE_CONFIG_DEFAULT = {
    .strategy = UFT_MERGE_HIGHEST_SCORE,
    .min_agreements = 2,
    .preserve_weak_bits = true,
    .preserve_timing = true,
    .max_revolutions = 10
};

/**
 * @brief Sector candidate for merging
 */
typedef struct {
    int cylinder;
    int head;
    int sector;
    uint8_t *data;
    size_t data_size;
    uft_decode_score_t score;
    int source_revolution;
    bool crc_ok;
    uint32_t weak_bit_mask;  /* Bits that varied across reads */
} uft_sector_candidate_t;

/**
 * @brief Merge result for one sector
 */
typedef struct {
    int cylinder;
    int head;
    int sector;
    uint8_t *data;
    size_t data_size;
    uft_decode_score_t final_score;
    int source_revolution;      /* Which rev won */
    int agreement_count;        /* How many revs agreed */
    int total_candidates;       /* How many were available */
    uint32_t weak_bit_positions;/* Bits that are uncertain */
    char merge_reason[128];     /* Why this candidate won */
} uft_merged_sector_t;

/**
 * @brief Track merge result
 */
typedef struct {
    int cylinder;
    int head;
    uft_merged_sector_t *sectors;
    int sector_count;
    int good_sectors;           /* Sectors with CRC OK */
    int recovered_sectors;      /* Sectors recovered via merge */
    int failed_sectors;         /* Sectors that couldn't be merged */
    uft_decode_score_t track_score; /* Aggregate score */
} uft_merged_track_t;

/**
 * @brief Merge engine context
 */
typedef struct uft_merge_engine uft_merge_engine_t;

/**
 * @brief Create merge engine
 */
uft_merge_engine_t* uft_merge_engine_create(const uft_merge_config_t *config);

/**
 * @brief Destroy merge engine
 */
void uft_merge_engine_destroy(uft_merge_engine_t *engine);

/**
 * @brief Add sector candidate from a revolution
 */
int uft_merge_add_candidate(
    uft_merge_engine_t *engine,
    const uft_sector_candidate_t *candidate);

/**
 * @brief Perform merge for all added candidates
 * @param engine Engine with candidates
 * @param out_track Output merged track
 * @return Number of successfully merged sectors
 */
int uft_merge_execute(
    uft_merge_engine_t *engine,
    uft_merged_track_t *out_track);

/**
 * @brief Clear all candidates (for next track)
 */
void uft_merge_reset(uft_merge_engine_t *engine);

/**
 * @brief Free merged track resources
 */
void uft_merged_track_free(uft_merged_track_t *track);

/**
 * @brief Simple sector merge (convenience function)
 * 
 * Merges N candidates for same sector, returns best.
 */
int uft_merge_sector_simple(
    const uft_sector_candidate_t *candidates,
    int count,
    uft_merge_strategy_t strategy,
    uft_merged_sector_t *out);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MERGE_ENGINE_H */
