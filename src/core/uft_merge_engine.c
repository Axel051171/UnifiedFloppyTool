/**
 * @file uft_merge_engine.c
 * @brief Multi-Read Merge Engine Implementation
 */

#include "uft/uft_merge_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_CANDIDATES_PER_SECTOR 20
#define MAX_SECTORS_PER_TRACK 64

typedef struct {
    uft_sector_candidate_t candidates[MAX_CANDIDATES_PER_SECTOR];
    int count;
    int sector_id;  /* Sector number */
} sector_bucket_t;

struct uft_merge_engine {
    uft_merge_config_t config;
    sector_bucket_t buckets[MAX_SECTORS_PER_TRACK];
    int bucket_count;
    int cylinder;
    int head;
};

uft_merge_engine_t* uft_merge_engine_create(const uft_merge_config_t *config)
{
    uft_merge_engine_t *engine = calloc(1, sizeof(uft_merge_engine_t));
    if (!engine) return NULL;
    
    if (config) {
        engine->config = *config;
    } else {
        engine->config = UFT_MERGE_CONFIG_DEFAULT;
    }
    
    return engine;
}

void uft_merge_engine_destroy(uft_merge_engine_t *engine)
{
    if (!engine) return;
    uft_merge_reset(engine);
    free(engine);
}

static sector_bucket_t* find_or_create_bucket(uft_merge_engine_t *engine, int sector)
{
    /* Find existing bucket */
    for (int i = 0; i < engine->bucket_count; i++) {
        if (engine->buckets[i].sector_id == sector) {
            return &engine->buckets[i];
        }
    }
    
    /* Create new bucket */
    if (engine->bucket_count >= MAX_SECTORS_PER_TRACK) {
        return NULL;
    }
    
    sector_bucket_t *bucket = &engine->buckets[engine->bucket_count++];
    bucket->sector_id = sector;
    bucket->count = 0;
    return bucket;
}

int uft_merge_add_candidate(
    uft_merge_engine_t *engine,
    const uft_sector_candidate_t *candidate)
{
    if (!engine || !candidate) return -1;
    
    sector_bucket_t *bucket = find_or_create_bucket(engine, candidate->sector);
    if (!bucket || bucket->count >= MAX_CANDIDATES_PER_SECTOR) {
        return -1;
    }
    
    /* Copy candidate */
    uft_sector_candidate_t *dest = &bucket->candidates[bucket->count];
    *dest = *candidate;
    
    /* Copy data if present */
    if (candidate->data && candidate->data_size > 0) {
        dest->data = malloc(candidate->data_size);
        if (dest->data) {
            memcpy(dest->data, candidate->data, candidate->data_size);
        }
    }
    
    bucket->count++;
    
    /* Track cylinder/head from first candidate */
    if (engine->cylinder == 0 && engine->head == 0) {
        engine->cylinder = candidate->cylinder;
        engine->head = candidate->head;
    }
    
    return 0;
}

static int merge_bucket(
    sector_bucket_t *bucket,
    uft_merge_strategy_t strategy,
    uft_merged_sector_t *out)
{
    if (!bucket || bucket->count == 0 || !out) return -1;
    
    memset(out, 0, sizeof(*out));
    
    uft_sector_candidate_t *winner = NULL;
    int best_score = -1;
    int crc_ok_count = 0;
    
    /* Count CRC-OK candidates */
    for (int i = 0; i < bucket->count; i++) {
        if (bucket->candidates[i].crc_ok) {
            crc_ok_count++;
        }
    }
    
    /* Select winner based on strategy */
    switch (strategy) {
        case UFT_MERGE_CRC_WINS:
            /* First CRC-OK wins */
            for (int i = 0; i < bucket->count; i++) {
                if (bucket->candidates[i].crc_ok) {
                    winner = &bucket->candidates[i];
                    snprintf(out->merge_reason, sizeof(out->merge_reason),
                             "CRC OK (rev %d)", winner->source_revolution);
                    break;
                }
            }
            if (!winner && bucket->count > 0) {
                winner = &bucket->candidates[0];
                snprintf(out->merge_reason, sizeof(out->merge_reason),
                         "No CRC OK, using first");
            }
            break;
            
        case UFT_MERGE_HIGHEST_SCORE:
            /* Highest score wins */
            for (int i = 0; i < bucket->count; i++) {
                if (bucket->candidates[i].score.total > best_score) {
                    best_score = bucket->candidates[i].score.total;
                    winner = &bucket->candidates[i];
                }
            }
            if (winner) {
                snprintf(out->merge_reason, sizeof(out->merge_reason),
                         "Highest score %d (rev %d)", 
                         winner->score.total, winner->source_revolution);
            }
            break;
            
        case UFT_MERGE_MAJORITY:
            /* TODO: Implement byte-by-byte voting */
            /* Fallback to highest score for now */
            for (int i = 0; i < bucket->count; i++) {
                if (bucket->candidates[i].score.total > best_score) {
                    best_score = bucket->candidates[i].score.total;
                    winner = &bucket->candidates[i];
                }
            }
            snprintf(out->merge_reason, sizeof(out->merge_reason),
                     "Majority (simplified)");
            break;
            
        case UFT_MERGE_LATEST:
            /* Last revolution wins */
            winner = &bucket->candidates[bucket->count - 1];
            snprintf(out->merge_reason, sizeof(out->merge_reason),
                     "Latest (rev %d)", winner->source_revolution);
            break;
    }
    
    if (!winner) return -1;
    
    /* Copy winner to output */
    out->cylinder = winner->cylinder;
    out->head = winner->head;
    out->sector = winner->sector;
    out->data_size = winner->data_size;
    out->source_revolution = winner->source_revolution;
    out->final_score = winner->score;
    out->agreement_count = crc_ok_count > 0 ? crc_ok_count : 1;
    out->total_candidates = bucket->count;
    out->weak_bit_positions = winner->weak_bit_mask;
    
    /* Copy data */
    if (winner->data && winner->data_size > 0) {
        out->data = malloc(winner->data_size);
        if (out->data) {
            memcpy(out->data, winner->data, winner->data_size);
        }
    }
    
    return 0;
}

int uft_merge_execute(
    uft_merge_engine_t *engine,
    uft_merged_track_t *out_track)
{
    if (!engine || !out_track) return -1;
    
    memset(out_track, 0, sizeof(*out_track));
    out_track->cylinder = engine->cylinder;
    out_track->head = engine->head;
    
    if (engine->bucket_count == 0) return 0;
    
    /* Allocate output sectors */
    out_track->sectors = calloc(engine->bucket_count, sizeof(uft_merged_sector_t));
    if (!out_track->sectors) return -1;
    
    int merged_count = 0;
    int total_score = 0;
    
    /* Merge each bucket */
    for (int i = 0; i < engine->bucket_count; i++) {
        if (merge_bucket(&engine->buckets[i], engine->config.strategy,
                         &out_track->sectors[merged_count]) == 0) {
            
            if (out_track->sectors[merged_count].final_score.crc_ok) {
                out_track->good_sectors++;
            } else if (out_track->sectors[merged_count].agreement_count > 1) {
                out_track->recovered_sectors++;
            } else {
                out_track->failed_sectors++;
            }
            
            total_score += out_track->sectors[merged_count].final_score.total;
            merged_count++;
        }
    }
    
    out_track->sector_count = merged_count;
    
    /* Calculate track score */
    if (merged_count > 0) {
        out_track->track_score.total = total_score / merged_count;
        out_track->track_score.confidence = 
            (out_track->good_sectors * 100) / merged_count;
    }
    
    return merged_count;
}

void uft_merge_reset(uft_merge_engine_t *engine)
{
    if (!engine) return;
    
    /* Free candidate data */
    for (int i = 0; i < engine->bucket_count; i++) {
        for (int j = 0; j < engine->buckets[i].count; j++) {
            free(engine->buckets[i].candidates[j].data);
        }
        engine->buckets[i].count = 0;
    }
    
    engine->bucket_count = 0;
    engine->cylinder = 0;
    engine->head = 0;
}

void uft_merged_track_free(uft_merged_track_t *track)
{
    if (!track) return;
    
    if (track->sectors) {
        for (int i = 0; i < track->sector_count; i++) {
            free(track->sectors[i].data);
        }
        free(track->sectors);
    }
    
    memset(track, 0, sizeof(*track));
}

int uft_merge_sector_simple(
    const uft_sector_candidate_t *candidates,
    int count,
    uft_merge_strategy_t strategy,
    uft_merged_sector_t *out)
{
    if (!candidates || count <= 0 || !out) return -1;
    
    /* Create temporary bucket */
    sector_bucket_t bucket = {0};
    bucket.sector_id = candidates[0].sector;
    
    for (int i = 0; i < count && i < MAX_CANDIDATES_PER_SECTOR; i++) {
        bucket.candidates[bucket.count++] = candidates[i];
    }
    
    return merge_bucket(&bucket, strategy, out);
}
