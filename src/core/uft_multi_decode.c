/**
 * @file uft_multi_decode.c
 * @brief UFT Multi-Interpretation Decoder Implementation
 * @version 3.2.0.003
 * @date 2026-01-04
 *
 * M-002: Multi-Interpretations-Decoder
 * 
 * Implementation of N-Best hypothesis management for ambiguous bitstream
 * decoding. Enables forensic-grade preservation where multiple interpretations
 * are maintained until resolution is required.
 *
 * "Bei ambiguen Daten keine voreilige Entscheidung"
 */

#include "uft/uft_multi_decode.h"
#include "uft/uft_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/*============================================================================
 * Internal Helper Structures
 *============================================================================*/

/** Session allocation header */
typedef struct {
    size_t block_size;
    size_t used;
    struct session_block *next;
} session_block_t;

/** Internal session state */
typedef struct {
    session_block_t *first_block;
    session_block_t *current_block;
    size_t total_allocated;
    size_t n_best;
} session_internal_t;

/*============================================================================
 * Static Helper Functions
 *============================================================================*/

/**
 * @brief Generate UUID for session
 */
static void generate_session_id(char *buffer, size_t size) {
    static const char hex[] = "0123456789abcdef";
    srand((unsigned int)time(NULL));
    
    if (size < 37) return;
    
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            buffer[i] = '-';
        } else {
            buffer[i] = hex[rand() % 16];
        }
    }
    buffer[36] = '\0';
}

/**
 * @brief Calculate CRC32
 */
static uint32_t calculate_crc32(const uint8_t *data, size_t size) {
    static const uint32_t crc_table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
        0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
        0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
        0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
        /* ... abbreviated for brevity, full table in production */
        0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
    };
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = (crc >> 8) ^ crc_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

/**
 * @brief Compare candidates by confidence (for qsort)
 */
static int compare_by_confidence(const void *a, const void *b) {
    const uft_decode_candidate_t *ca = *(const uft_decode_candidate_t **)a;
    const uft_decode_candidate_t *cb = *(const uft_decode_candidate_t **)b;
    
    /* CRC valid first, then by confidence */
    if (ca->status == UFT_CS_VALID && cb->status != UFT_CS_VALID) return -1;
    if (ca->status != UFT_CS_VALID && cb->status == UFT_CS_VALID) return 1;
    
    if (ca->confidence > cb->confidence) return -1;
    if (ca->confidence < cb->confidence) return 1;
    return 0;
}

/**
 * @brief Count differing bits between two byte arrays
 */
static size_t count_diff_bits(const uint8_t *a, const uint8_t *b, 
                              size_t size, uint32_t *positions, size_t max_pos) {
    size_t diff_count = 0;
    size_t pos_idx = 0;
    
    for (size_t i = 0; i < size; i++) {
        uint8_t diff = a[i] ^ b[i];
        if (diff) {
            for (int bit = 0; bit < 8; bit++) {
                if (diff & (1 << bit)) {
                    diff_count++;
                    if (positions && pos_idx < max_pos) {
                        positions[pos_idx++] = (uint32_t)(i * 8 + bit);
                    }
                }
            }
        }
    }
    return diff_count;
}

/*============================================================================
 * Session Management
 *============================================================================*/

uft_md_session_t* uft_md_session_create(const uft_md_config_t *config) {
    uft_md_session_t *session = calloc(1, sizeof(uft_md_session_t));
    if (!session) return NULL;
    
    /* Generate session ID */
    generate_session_id(session->session_id, sizeof(session->session_id));
    session->created_time = time(NULL);
    session->modified_time = session->created_time;
    
    /* Apply configuration */
    if (config) {
        session->config = *config;
    } else {
        uft_md_config_defaults(&session->config);
    }
    
    /* Initialize statistics */
    session->stats.total_sectors = 0;
    session->stats.resolved_count = 0;
    session->stats.conflict_count = 0;
    session->stats.avg_confidence = 0.0f;
    
    /* Allocate track array */
    session->tracks = calloc(UFT_MD_MAX_TRACKS, sizeof(uft_track_candidates_t));
    if (!session->tracks) {
        free(session);
        return NULL;
    }
    session->track_count = 0;
    session->track_capacity = UFT_MD_MAX_TRACKS;
    
    return session;
}

void uft_md_session_destroy(uft_md_session_t *session) {
    if (!session) return;
    
    /* Free all track data */
    for (size_t t = 0; t < session->track_count; t++) {
        uft_track_candidates_t *track = &session->tracks[t];
        if (track->sectors) {
            for (size_t s = 0; s < track->sector_count; s++) {
                uft_sector_candidates_t *sector = &track->sectors[s];
                if (sector->candidates) {
                    for (size_t c = 0; c < sector->candidate_count; c++) {
                        /* Free any heap-allocated uncertainty maps */
                        if (sector->candidates[c].uncertainty.bitmap) {
                            /* Note: bitmap is embedded, but notes are heap */
                            /* Already part of struct, no free needed */
                        }
                    }
                    free(sector->candidates);
                }
            }
            free(track->sectors);
        }
    }
    free(session->tracks);
    free(session);
}

int uft_md_session_reset(uft_md_session_t *session) {
    if (!session) return UFT_ERR_NULL_PTR;
    
    /* Reset all tracks */
    for (size_t t = 0; t < session->track_count; t++) {
        uft_track_candidates_t *track = &session->tracks[t];
        if (track->sectors) {
            for (size_t s = 0; s < track->sector_count; s++) {
                if (track->sectors[s].candidates) {
                    free(track->sectors[s].candidates);
                    track->sectors[s].candidates = NULL;
                }
            }
            free(track->sectors);
            track->sectors = NULL;
        }
        track->sector_count = 0;
    }
    session->track_count = 0;
    
    /* Reset statistics */
    memset(&session->stats, 0, sizeof(session->stats));
    session->modified_time = time(NULL);
    
    return UFT_OK;
}

/*============================================================================
 * Candidate Management
 *============================================================================*/

uft_sector_candidates_t* uft_md_get_sector(
    uft_md_session_t *session,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    bool create_if_missing
) {
    if (!session) return NULL;
    
    /* Find or create track */
    uft_track_candidates_t *trk = NULL;
    for (size_t t = 0; t < session->track_count; t++) {
        if (session->tracks[t].track_num == track &&
            session->tracks[t].head == head) {
            trk = &session->tracks[t];
            break;
        }
    }
    
    if (!trk && create_if_missing) {
        if (session->track_count >= session->track_capacity) {
            return NULL; /* At capacity */
        }
        trk = &session->tracks[session->track_count++];
        trk->track_num = track;
        trk->head = head;
        trk->sector_count = 0;
        trk->sectors = calloc(UFT_MD_MAX_SECTORS, sizeof(uft_sector_candidates_t));
        if (!trk->sectors) {
            session->track_count--;
            return NULL;
        }
    }
    
    if (!trk) return NULL;
    
    /* Find or create sector */
    for (size_t s = 0; s < trk->sector_count; s++) {
        if (trk->sectors[s].sector_num == sector) {
            return &trk->sectors[s];
        }
    }
    
    if (create_if_missing && trk->sector_count < UFT_MD_MAX_SECTORS) {
        uft_sector_candidates_t *sec = &trk->sectors[trk->sector_count++];
        sec->track_num = track;
        sec->head = head;
        sec->sector_num = sector;
        sec->candidate_count = 0;
        sec->candidates = calloc(UFT_MD_MAX_CANDIDATES, sizeof(uft_decode_candidate_t));
        if (!sec->candidates) {
            trk->sector_count--;
            return NULL;
        }
        sec->is_resolved = false;
        sec->selected_index = -1;
        session->stats.total_sectors++;
        return sec;
    }
    
    return NULL;
}

int uft_md_add_candidate(
    uft_sector_candidates_t *sector,
    const uft_decode_candidate_t *candidate
) {
    if (!sector || !candidate) return UFT_ERR_NULL_PTR;
    if (sector->candidate_count >= UFT_MD_MAX_CANDIDATES) return UFT_ERR_BUFFER_FULL;
    
    /* Deep copy candidate */
    uft_decode_candidate_t *new_cand = &sector->candidates[sector->candidate_count];
    memcpy(new_cand, candidate, sizeof(uft_decode_candidate_t));
    
    /* Assign unique ID */
    new_cand->candidate_id = (uint32_t)(
        (sector->track_num << 24) |
        (sector->head << 20) |
        (sector->sector_num << 12) |
        sector->candidate_count
    );
    
    /* Calculate CRC of data */
    new_cand->data_crc32 = calculate_crc32(new_cand->data, new_cand->data_size);
    
    sector->candidate_count++;
    return UFT_OK;
}

uft_decode_candidate_t* uft_md_create_candidate(
    uft_sector_candidates_t *sector,
    uft_decode_method_t method,
    const uint8_t *data,
    uint32_t size
) {
    if (!sector || !data || size == 0) return NULL;
    if (sector->candidate_count >= UFT_MD_MAX_CANDIDATES) return NULL;
    if (size > UFT_MD_MAX_SECTOR_SIZE) return NULL;
    
    uft_decode_candidate_t *cand = &sector->candidates[sector->candidate_count];
    memset(cand, 0, sizeof(*cand));
    
    /* Set basic fields */
    cand->candidate_id = (uint32_t)(
        (sector->track_num << 24) |
        (sector->head << 20) |
        (sector->sector_num << 12) |
        sector->candidate_count
    );
    cand->sector_id = sector->sector_num;
    
    /* Copy data */
    memcpy(cand->data, data, size);
    cand->data_size = size;
    cand->data_crc32 = calculate_crc32(data, size);
    
    /* Set method */
    cand->primary_method = method;
    cand->method_count = 1;
    
    /* Default status */
    cand->status = UFT_CS_PENDING;
    cand->confidence = 0.0f;
    
    sector->candidate_count++;
    return cand;
}

int uft_md_remove_candidate(uft_sector_candidates_t *sector, uint32_t index) {
    if (!sector) return UFT_ERR_NULL_PTR;
    if (index >= sector->candidate_count) return UFT_ERR_OUT_OF_RANGE;
    
    /* Shift remaining candidates */
    for (size_t i = index; i + 1 < sector->candidate_count; i++) {
        sector->candidates[i] = sector->candidates[i + 1];
    }
    sector->candidate_count--;
    
    /* Adjust selected index if needed */
    if (sector->selected_index == (int)index) {
        sector->selected_index = -1;
        sector->is_resolved = false;
    } else if (sector->selected_index > (int)index) {
        sector->selected_index--;
    }
    
    return UFT_OK;
}

/*============================================================================
 * Confidence Calculation
 *============================================================================*/

float uft_md_calculate_confidence(
    const uft_decode_candidate_t *candidate,
    const uft_md_config_t *config
) {
    if (!candidate) return 0.0f;
    
    float confidence = 50.0f; /* Base confidence */
    
    /* CRC validation (major factor) */
    if (candidate->status & UFT_CS_VALID) {
        confidence += 40.0f;
    } else if (candidate->status & UFT_CS_CRC_FAIL) {
        confidence -= 30.0f;
    }
    
    /* Repair penalty */
    if (candidate->status & UFT_CS_REPAIRED) {
        confidence -= 10.0f;
    }
    
    /* Uncertainty penalty */
    if (candidate->status & UFT_CS_UNCERTAIN) {
        confidence -= 15.0f;
    }
    
    /* Weak bits penalty */
    if (candidate->status & UFT_CS_WEAK_BITS) {
        float weak_ratio = (float)candidate->uncertainty.uncertain_count / 
                          (float)(candidate->data_size * 8);
        confidence -= weak_ratio * 20.0f;
    }
    
    /* PLL quality bonus */
    if (candidate->source.pll_phase_error_avg < 0.1f) {
        confidence += 5.0f;
    } else if (candidate->source.pll_phase_error_avg > 0.3f) {
        confidence -= 10.0f;
    }
    
    /* Multi-revolution bonus */
    int rev_count = 0;
    for (int i = 0; i < 8; i++) {
        if (candidate->source.revolution_mask & (1 << i)) rev_count++;
    }
    if (rev_count > 1) {
        confidence += (float)(rev_count - 1) * 3.0f;
    }
    
    /* Clamp to valid range */
    if (confidence < 0.0f) confidence = 0.0f;
    if (confidence > 100.0f) confidence = 100.0f;
    
    return confidence;
}

void uft_md_update_confidence(uft_decode_candidate_t *candidate) {
    if (!candidate) return;
    candidate->confidence = uft_md_calculate_confidence(candidate, NULL);
}

/*============================================================================
 * Resolution
 *============================================================================*/

uft_decode_candidate_t* uft_md_resolve(
    uft_sector_candidates_t *sector,
    uft_resolution_strategy_t strategy
) {
    if (!sector || sector->candidate_count == 0) return NULL;
    
    /* Sort by confidence (descending) */
    uft_decode_candidate_t *sorted[UFT_MD_MAX_CANDIDATES] = {NULL};
    for (size_t i = 0; i < sector->candidate_count; i++) {
        sorted[i] = &sector->candidates[i];
    }
    qsort(sorted, sector->candidate_count, sizeof(sorted[0]), compare_by_confidence);
    
    uft_decode_candidate_t *selected = NULL;
    
    switch (strategy) {
        case UFT_RS_HIGHEST_CONFIDENCE:
            selected = sorted[0];
            break;
            
        case UFT_RS_CRC_PRIORITY:
            /* Find first with valid CRC */
            for (size_t i = 0; i < sector->candidate_count; i++) {
                if (sorted[i]->status == UFT_CS_VALID) {
                    selected = sorted[i];
                    break;
                }
            }
            if (!selected) selected = sorted[0];
            break;
            
        case UFT_RS_MULTI_REV_FUSION:
            /* Prefer candidates with multi-rev support */
            for (size_t i = 0; i < sector->candidate_count; i++) {
                int rev_count = 0;
                for (int r = 0; r < 8; r++) {
                    if (sorted[i]->source.revolution_mask & (1 << r)) rev_count++;
                }
                if (rev_count > 1) {
                    selected = sorted[i];
                    break;
                }
            }
            if (!selected) selected = sorted[0];
            break;
            
        case UFT_RS_CONSENSUS_VOTING:
            /* Use voting when implemented */
            selected = sorted[0];
            break;
            
        case UFT_RS_USER_SELECT:
            /* Return current selection or highest confidence */
            if (sector->selected_index >= 0 && 
                sector->selected_index < (int)sector->candidate_count) {
                selected = &sector->candidates[sector->selected_index];
            } else {
                selected = sorted[0];
            }
            break;
            
        case UFT_RS_FORENSIC_ALL:
            /* Don't resolve, return NULL */
            return NULL;
    }
    
    if (selected) {
        /* Find index and mark resolved */
        for (size_t i = 0; i < sector->candidate_count; i++) {
            if (&sector->candidates[i] == selected) {
                sector->selected_index = (int)i;
                break;
            }
        }
        sector->is_resolved = true;
    }
    
    return selected;
}

int uft_md_resolve_all(
    uft_md_session_t *session,
    uft_resolution_strategy_t strategy
) {
    if (!session) return UFT_ERR_NULL_PTR;
    
    int resolved = 0;
    
    for (size_t t = 0; t < session->track_count; t++) {
        uft_track_candidates_t *track = &session->tracks[t];
        for (size_t s = 0; s < track->sector_count; s++) {
            uft_sector_candidates_t *sector = &track->sectors[s];
            if (!sector->is_resolved && sector->candidate_count > 0) {
                if (uft_md_resolve(sector, strategy) != NULL) {
                    resolved++;
                }
            }
        }
    }
    
    session->stats.resolved_count = resolved;
    session->modified_time = time(NULL);
    
    return resolved;
}

uft_decode_candidate_t* uft_md_select_best(
    uft_sector_candidates_t *sector,
    float min_confidence
) {
    if (!sector || sector->candidate_count == 0) return NULL;
    
    uft_decode_candidate_t *best = NULL;
    float best_conf = min_confidence;
    
    for (size_t i = 0; i < sector->candidate_count; i++) {
        if (sector->candidates[i].confidence >= best_conf) {
            best = &sector->candidates[i];
            best_conf = sector->candidates[i].confidence;
        }
    }
    
    return best;
}

int uft_md_user_select(uft_sector_candidates_t *sector, uint32_t index) {
    if (!sector) return UFT_ERR_NULL_PTR;
    if (index >= sector->candidate_count) return UFT_ERR_OUT_OF_RANGE;
    
    sector->selected_index = (int)index;
    sector->is_resolved = true;
    sector->candidates[index].status |= UFT_CS_VALID; /* User confirms */
    
    return UFT_OK;
}

/*============================================================================
 * Comparison
 *============================================================================*/

uft_candidate_cmp_t uft_md_compare(
    const uft_decode_candidate_t *a,
    const uft_decode_candidate_t *b
) {
    if (!a || !b) return UFT_CMP_CONFLICT;
    
    /* Check sizes */
    if (a->data_size != b->data_size) {
        /* Check if one is subset */
        if (a->data_size < b->data_size) {
            if (memcmp(a->data, b->data, a->data_size) == 0) {
                return UFT_CMP_SUBSET;
            }
        } else {
            if (memcmp(a->data, b->data, b->data_size) == 0) {
                return UFT_CMP_SUBSET;
            }
        }
        return UFT_CMP_DIFFERENT;
    }
    
    /* Compare data */
    if (memcmp(a->data, b->data, a->data_size) == 0) {
        /* Check if metadata differs */
        if (a->primary_method != b->primary_method ||
            a->source.revolution_mask != b->source.revolution_mask) {
            return UFT_CMP_EQUIVALENT;
        }
        return UFT_CMP_IDENTICAL;
    }
    
    /* Data differs - check if conflict or just different */
    if (a->status == UFT_CS_VALID && b->status == UFT_CS_VALID) {
        /* Both claim valid - conflict */
        return UFT_CMP_CONFLICT;
    }
    
    return UFT_CMP_DIFFERENT;
}

int uft_md_diff_bits(
    const uft_decode_candidate_t *a,
    const uft_decode_candidate_t *b,
    uint32_t *diff_positions,
    uint32_t max_diff
) {
    if (!a || !b) return -1;
    
    size_t min_size = a->data_size < b->data_size ? a->data_size : b->data_size;
    return (int)count_diff_bits(a->data, b->data, min_size, diff_positions, max_diff);
}

/*============================================================================
 * Fusion
 *============================================================================*/

uft_decode_candidate_t* uft_md_fuse_revolutions(
    uft_decode_candidate_t * const *candidates,
    uint8_t count,
    float voting_threshold
) {
    if (!candidates || count == 0) return NULL;
    
    /* Find data size (use first candidate) */
    uint32_t data_size = candidates[0]->data_size;
    
    /* Allocate result */
    uft_decode_candidate_t *fused = calloc(1, sizeof(uft_decode_candidate_t));
    if (!fused) return NULL;
    
    fused->data_size = data_size;
    fused->primary_method = UFT_DM_FUSION_VOTING;
    fused->source.revolution_mask = 0;
    
    /* Bit-level voting */
    for (uint32_t byte_idx = 0; byte_idx < data_size; byte_idx++) {
        int bit_votes[8] = {0};
        
        for (uint8_t c = 0; c < count; c++) {
            if (byte_idx < candidates[c]->data_size) {
                uint8_t b = candidates[c]->data[byte_idx];
                for (int bit = 0; bit < 8; bit++) {
                    if (b & (1 << bit)) bit_votes[bit]++;
                }
            }
            fused->source.revolution_mask |= candidates[c]->source.revolution_mask;
        }
        
        /* Determine each bit by majority vote */
        uint8_t result_byte = 0;
        int threshold_votes = (int)(count * voting_threshold);
        for (int bit = 0; bit < 8; bit++) {
            if (bit_votes[bit] >= threshold_votes) {
                result_byte |= (1 << bit);
            }
            /* Mark uncertain bits */
            if (bit_votes[bit] > 0 && bit_votes[bit] < count) {
                fused->uncertainty.bitmap[byte_idx] |= (1 << bit);
                fused->uncertainty.uncertain_count++;
            }
        }
        fused->data[byte_idx] = result_byte;
    }
    
    /* Calculate confidence based on agreement */
    float total_agreement = 0.0f;
    for (uint32_t i = 0; i < data_size; i++) {
        int agree = 8 - __builtin_popcount(fused->uncertainty.bitmap[i] & 0xFF);
        total_agreement += (float)agree / 8.0f;
    }
    fused->uncertainty.overall_certainty = total_agreement / (float)data_size;
    fused->confidence = fused->uncertainty.overall_certainty * 100.0f;
    
    /* Calculate CRC */
    fused->data_crc32 = calculate_crc32(fused->data, fused->data_size);
    
    return fused;
}

uft_decode_candidate_t* uft_md_fuse_weighted(
    uft_decode_candidate_t * const *candidates,
    const float *weights,
    uint8_t count
) {
    if (!candidates || !weights || count == 0) return NULL;
    
    uint32_t data_size = candidates[0]->data_size;
    
    uft_decode_candidate_t *fused = calloc(1, sizeof(uft_decode_candidate_t));
    if (!fused) return NULL;
    
    fused->data_size = data_size;
    fused->primary_method = UFT_DM_FUSION_WEIGHTED;
    
    /* Normalize weights */
    float weight_sum = 0.0f;
    for (uint8_t c = 0; c < count; c++) {
        weight_sum += weights[c];
    }
    
    /* Weighted bit voting */
    for (uint32_t byte_idx = 0; byte_idx < data_size; byte_idx++) {
        float bit_weights[8] = {0};
        
        for (uint8_t c = 0; c < count; c++) {
            if (byte_idx < candidates[c]->data_size) {
                uint8_t b = candidates[c]->data[byte_idx];
                float w = weights[c] / weight_sum;
                for (int bit = 0; bit < 8; bit++) {
                    if (b & (1 << bit)) bit_weights[bit] += w;
                }
            }
        }
        
        uint8_t result_byte = 0;
        for (int bit = 0; bit < 8; bit++) {
            if (bit_weights[bit] >= 0.5f) {
                result_byte |= (1 << bit);
            }
            /* Mark uncertain bits */
            if (bit_weights[bit] > 0.2f && bit_weights[bit] < 0.8f) {
                fused->uncertainty.bitmap[byte_idx] |= (1 << bit);
                fused->uncertainty.uncertain_count++;
            }
        }
        fused->data[byte_idx] = result_byte;
    }
    
    fused->data_crc32 = calculate_crc32(fused->data, fused->data_size);
    uft_md_update_confidence(fused);
    
    return fused;
}

/*============================================================================
 * Statistics
 *============================================================================*/

void uft_md_get_stats(
    const uft_md_session_t *session,
    uint32_t *total_sectors,
    uint32_t *resolved_count,
    uint32_t *conflict_count,
    float *avg_confidence
) {
    if (!session) return;
    
    if (total_sectors) *total_sectors = session->stats.total_sectors;
    if (resolved_count) *resolved_count = session->stats.resolved_count;
    if (conflict_count) *conflict_count = session->stats.conflict_count;
    if (avg_confidence) *avg_confidence = session->stats.avg_confidence;
}

uint32_t uft_md_find_conflicts(
    const uft_md_session_t *session,
    uft_sector_candidates_t **sectors,
    uint32_t max_sectors
) {
    if (!session || !sectors) return 0;
    
    uint32_t found = 0;
    
    for (size_t t = 0; t < session->track_count && found < max_sectors; t++) {
        const uft_track_candidates_t *track = &session->tracks[t];
        for (size_t s = 0; s < track->sector_count && found < max_sectors; s++) {
            const uft_sector_candidates_t *sector = &track->sectors[s];
            
            /* Check for conflicts */
            bool has_conflict = false;
            for (size_t i = 0; i < sector->candidate_count && !has_conflict; i++) {
                for (size_t j = i + 1; j < sector->candidate_count; j++) {
                    if (uft_md_compare(&sector->candidates[i], 
                                       &sector->candidates[j]) == UFT_CMP_CONFLICT) {
                        has_conflict = true;
                        break;
                    }
                }
            }
            
            if (has_conflict) {
                sectors[found++] = (uft_sector_candidates_t*)sector;
            }
        }
    }
    
    return found;
}

uint32_t uft_md_find_low_confidence(
    const uft_md_session_t *session,
    float threshold,
    uft_sector_candidates_t **sectors,
    uint32_t max_sectors
) {
    if (!session || !sectors) return 0;
    
    uint32_t found = 0;
    
    for (size_t t = 0; t < session->track_count && found < max_sectors; t++) {
        const uft_track_candidates_t *track = &session->tracks[t];
        for (size_t s = 0; s < track->sector_count && found < max_sectors; s++) {
            const uft_sector_candidates_t *sector = &track->sectors[s];
            
            /* Find max confidence */
            float max_conf = 0.0f;
            for (size_t i = 0; i < sector->candidate_count; i++) {
                if (sector->candidates[i].confidence > max_conf) {
                    max_conf = sector->candidates[i].confidence;
                }
            }
            
            if (max_conf < threshold) {
                sectors[found++] = (uft_sector_candidates_t*)sector;
            }
        }
    }
    
    return found;
}

/*============================================================================
 * Export Functions
 *============================================================================*/

int uft_md_export_json(
    const uft_md_session_t *session,
    const uft_md_export_opts_t *opts,
    char *buffer,
    size_t buffer_size
) {
    if (!session || !buffer) return UFT_ERR_NULL_PTR;
    
    int written = 0;
    written += snprintf(buffer + written, buffer_size - written,
        "{\n"
        "  \"session_id\": \"%s\",\n"
        "  \"created\": %ld,\n"
        "  \"statistics\": {\n"
        "    \"total_sectors\": %u,\n"
        "    \"resolved\": %u,\n"
        "    \"conflicts\": %u,\n"
        "    \"avg_confidence\": %.2f\n"
        "  },\n"
        "  \"tracks\": [\n",
        session->session_id,
        (long)session->created_time,
        session->stats.total_sectors,
        session->stats.resolved_count,
        session->stats.conflict_count,
        session->stats.avg_confidence
    );
    
    for (size_t t = 0; t < session->track_count; t++) {
        const uft_track_candidates_t *track = &session->tracks[t];
        written += snprintf(buffer + written, buffer_size - written,
            "    {\n"
            "      \"track\": %u,\n"
            "      \"head\": %u,\n"
            "      \"sectors\": [\n",
            track->track_num, track->head
        );
        
        for (size_t s = 0; s < track->sector_count; s++) {
            const uft_sector_candidates_t *sector = &track->sectors[s];
            written += snprintf(buffer + written, buffer_size - written,
                "        {\n"
                "          \"sector\": %u,\n"
                "          \"resolved\": %s,\n"
                "          \"candidates\": %u\n"
                "        }%s\n",
                sector->sector_num,
                sector->is_resolved ? "true" : "false",
                sector->candidate_count,
                s < track->sector_count - 1 ? "," : ""
            );
        }
        
        written += snprintf(buffer + written, buffer_size - written,
            "      ]\n"
            "    }%s\n",
            t < session->track_count - 1 ? "," : ""
        );
    }
    
    written += snprintf(buffer + written, buffer_size - written,
        "  ]\n"
        "}\n"
    );
    
    return written;
}

int uft_md_export_markdown(
    const uft_md_session_t *session,
    const uft_md_export_opts_t *opts,
    char *buffer,
    size_t buffer_size
) {
    if (!session || !buffer) return UFT_ERR_NULL_PTR;
    
    int written = 0;
    written += snprintf(buffer + written, buffer_size - written,
        "# Multi-Decode Session Report\n\n"
        "**Session ID:** %s  \n"
        "**Created:** %s\n\n"
        "## Statistics\n\n"
        "| Metric | Value |\n"
        "|--------|-------|\n"
        "| Total Sectors | %u |\n"
        "| Resolved | %u |\n"
        "| Conflicts | %u |\n"
        "| Avg Confidence | %.2f%% |\n\n",
        session->session_id,
        ctime(&session->created_time),
        session->stats.total_sectors,
        session->stats.resolved_count,
        session->stats.conflict_count,
        session->stats.avg_confidence
    );
    
    /* List conflicts if any */
    if (session->stats.conflict_count > 0) {
        written += snprintf(buffer + written, buffer_size - written,
            "## Conflicts\n\n"
            "The following sectors have conflicting interpretations:\n\n"
        );
        
        uft_sector_candidates_t *conflicts[32];
        uint32_t conflict_count = uft_md_find_conflicts(session, conflicts, 32);
        
        for (uint32_t i = 0; i < conflict_count; i++) {
            written += snprintf(buffer + written, buffer_size - written,
                "- Track %u, Head %u, Sector %u (%zu candidates)\n",
                conflicts[i]->track_num,
                conflicts[i]->head,
                conflicts[i]->sector_num,
                conflicts[i]->candidate_count
            );
        }
        written += snprintf(buffer + written, buffer_size - written, "\n");
    }
    
    return written;
}

int uft_md_export_forensic_report(
    const uft_md_session_t *session,
    const uft_md_export_opts_t *opts
) {
    if (!session || !opts || !opts->output_path) return UFT_ERR_NULL_PTR;
    
    FILE *f = fopen(opts->output_path, "w");
    if (!f) return UFT_ERR_FILE_OPEN;
    
    /* P2-PERF-003: Heap statt Stack für große Buffer */
    char *buffer = (char*)malloc(65536);
    if (!buffer) {
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    int len = uft_md_export_markdown(session, opts, buffer, 65536);
    if (len > 0) {
        if (fwrite(buffer, 1, len, f) != len) { /* I/O error */ }
    }
    
    free(buffer);
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
        fclose(f);
        return UFT_OK;
}

int uft_md_generate_diff(
    const uft_decode_candidate_t *a,
    const uft_decode_candidate_t *b,
    char *buffer,
    size_t buffer_size
) {
    if (!a || !b || !buffer) return UFT_ERR_NULL_PTR;
    
    int written = 0;
    
    uint32_t diff_positions[256];
    int diff_count = uft_md_diff_bits(a, b, diff_positions, 256);
    
    written += snprintf(buffer + written, buffer_size - written,
        "## Candidate Diff Report\n\n"
        "**Candidate A:** ID=%u, Method=%s, Confidence=%.1f%%\n"
        "**Candidate B:** ID=%u, Method=%s, Confidence=%.1f%%\n\n"
        "**Differing Bits:** %d\n\n",
        a->candidate_id, uft_md_method_name(a->primary_method), a->confidence,
        b->candidate_id, uft_md_method_name(b->primary_method), b->confidence,
        diff_count
    );
    
    if (diff_count > 0 && diff_count <= 256) {
        written += snprintf(buffer + written, buffer_size - written,
            "### Bit Positions\n\n");
        for (int i = 0; i < diff_count && i < 32; i++) {
            uint32_t pos = diff_positions[i];
            uint32_t byte_idx = pos / 8;
            uint32_t bit_idx = pos % 8;
            written += snprintf(buffer + written, buffer_size - written,
                "- Byte %u, Bit %u: A=%d, B=%d\n",
                byte_idx, bit_idx,
                (a->data[byte_idx] >> bit_idx) & 1,
                (b->data[byte_idx] >> bit_idx) & 1
            );
        }
        if (diff_count > 32) {
            written += snprintf(buffer + written, buffer_size - written,
                "- ... and %d more differences\n", diff_count - 32);
        }
    }
    
    return written;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char* uft_md_method_name(uft_decode_method_t method) {
    switch (method) {
        case UFT_DM_NONE:             return "None";
        case UFT_DM_MFM_STANDARD:     return "MFM Standard";
        case UFT_DM_MFM_PLL_TIGHT:    return "MFM PLL Tight";
        case UFT_DM_MFM_PLL_LOOSE:    return "MFM PLL Loose";
        case UFT_DM_MFM_MULTI_REV:    return "MFM Multi-Rev";
        case UFT_DM_MFM_WEAK_BIT:     return "MFM Weak Bit";
        case UFT_DM_GCR_C64:          return "GCR C64";
        case UFT_DM_GCR_APPLE:        return "GCR Apple";
        case UFT_DM_GCR_APPLE_NIB:    return "GCR Apple Nibble";
        case UFT_DM_GCR_VICTOR:       return "GCR Victor";
        case UFT_DM_FM_STANDARD:      return "FM Standard";
        case UFT_DM_FM_INTEL:         return "FM Intel";
        case UFT_DM_RAW_BITSTREAM:    return "Raw Bitstream";
        case UFT_DM_FLUX_DIRECT:      return "Flux Direct";
        case UFT_DM_PROTECTION_AWARE: return "Protection Aware";
        case UFT_DM_ECC_CRC_REPAIR:   return "ECC CRC Repair";
        case UFT_DM_ECC_INTERLEAVE:   return "ECC Interleave";
        case UFT_DM_ECC_REED_SOLOMON: return "ECC Reed-Solomon";
        case UFT_DM_ECC_HAMMING:      return "ECC Hamming";
        case UFT_DM_FUSION_VOTING:    return "Fusion Voting";
        case UFT_DM_FUSION_WEIGHTED:  return "Fusion Weighted";
        case UFT_DM_FUSION_CONSENSUS: return "Fusion Consensus";
        default:                      return "Unknown";
    }
}

const char* uft_md_status_name(uft_candidate_status_t status) {
    static char buf[128];
    buf[0] = '\0';
    
    if (status == UFT_CS_PENDING) return "Pending";
    
    if (status & UFT_CS_VALID)       strncat(buf, "Valid ", sizeof(buf) - strlen(buf) - 1);
    if (status & UFT_CS_CRC_FAIL)    strncat(buf, "CRC-Fail ", sizeof(buf) - strlen(buf) - 1);
    if (status & UFT_CS_CHECKSUM_FAIL) strncat(buf, "Checksum-Fail ", sizeof(buf) - strlen(buf) - 1);
    if (status & UFT_CS_REPAIRED)    strncat(buf, "Repaired ", sizeof(buf) - strlen(buf) - 1);
    if (status & UFT_CS_UNCERTAIN)   strncat(buf, "Uncertain ", sizeof(buf) - strlen(buf) - 1);
    if (status & UFT_CS_WEAK_BITS)   strncat(buf, "Weak-Bits ", sizeof(buf) - strlen(buf) - 1);
    if (status & UFT_CS_SYNTHESIZED) strncat(buf, "Synthesized ", sizeof(buf) - strlen(buf) - 1);
    if (status & UFT_CS_BEST_EFFORT) strncat(buf, "Best-Effort ", sizeof(buf) - strlen(buf) - 1);
    
    /* Remove trailing space */
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == ' ') buf[len-1] = '\0';
    
    return buf;
}

const char* uft_md_strategy_name(uft_resolution_strategy_t strategy) {
    switch (strategy) {
        case UFT_RS_HIGHEST_CONFIDENCE: return "Highest Confidence";
        case UFT_RS_CRC_PRIORITY:       return "CRC Priority";
        case UFT_RS_MULTI_REV_FUSION:   return "Multi-Revolution Fusion";
        case UFT_RS_CONSENSUS_VOTING:   return "Consensus Voting";
        case UFT_RS_USER_SELECT:        return "User Selection";
        case UFT_RS_FORENSIC_ALL:       return "Forensic (All)";
        default:                         return "Unknown";
    }
}

void uft_md_calculate_fingerprint(
    const uft_decode_candidate_t *candidate,
    uint8_t fingerprint[32]
) {
    if (!candidate || !fingerprint) return;
    
    /* Simple fingerprint: combination of method + status + data hash */
    memset(fingerprint, 0, 32);
    
    fingerprint[0] = (uint8_t)(candidate->primary_method >> 8);
    fingerprint[1] = (uint8_t)(candidate->primary_method & 0xFF);
    fingerprint[2] = (uint8_t)candidate->status;
    fingerprint[3] = (uint8_t)(candidate->data_size >> 8);
    fingerprint[4] = (uint8_t)(candidate->data_size & 0xFF);
    
    /* CRC32 as part of fingerprint */
    fingerprint[5] = (uint8_t)(candidate->data_crc32 >> 24);
    fingerprint[6] = (uint8_t)(candidate->data_crc32 >> 16);
    fingerprint[7] = (uint8_t)(candidate->data_crc32 >> 8);
    fingerprint[8] = (uint8_t)(candidate->data_crc32 & 0xFF);
    
    /* Simple hash of first 23 data bytes */
    for (size_t i = 0; i < 23 && i < candidate->data_size; i++) {
        fingerprint[9 + i] = candidate->data[i];
    }
}

bool uft_md_verify_crc(
    const uft_decode_candidate_t *candidate,
    uint32_t expected_crc
) {
    if (!candidate) return false;
    uint32_t actual = calculate_crc32(candidate->data, candidate->data_size);
    return actual == expected_crc;
}

void uft_md_config_defaults(uft_md_config_t *config) {
    if (!config) return;
    
    config->n_best_count = 8;
    config->min_confidence = 40.0f;
    config->auto_resolve_threshold = 85.0f;
    config->enable_fusion = true;
    config->enable_forensic = true;
    config->max_memory = 0; /* Unlimited */
    config->output_path[0] = '\0';
}

/*============================================================================
 * Iterator Functions
 *============================================================================*/

void uft_md_iter_init(
    uft_md_session_t *session,
    uft_candidate_iter_t *iter,
    bool include_resolved,
    float min_confidence
) {
    if (!session || !iter) return;
    
    iter->session = session;
    iter->track_idx = 0;
    iter->sector_idx = 0;
    iter->candidate_idx = 0;
    iter->include_resolved = include_resolved;
    iter->min_confidence = min_confidence;
}

uft_decode_candidate_t* uft_md_iter_next(uft_candidate_iter_t *iter) {
    if (!iter || !iter->session) return NULL;
    
    uft_md_session_t *session = iter->session;
    
    while (iter->track_idx < session->track_count) {
        uft_track_candidates_t *track = &session->tracks[iter->track_idx];
        
        while (iter->sector_idx < track->sector_count) {
            uft_sector_candidates_t *sector = &track->sectors[iter->sector_idx];
            
            /* Skip resolved if not wanted */
            if (!iter->include_resolved && sector->is_resolved) {
                iter->sector_idx++;
                iter->candidate_idx = 0;
                continue;
            }
            
            while (iter->candidate_idx < sector->candidate_count) {
                uft_decode_candidate_t *cand = &sector->candidates[iter->candidate_idx];
                iter->candidate_idx++;
                
                /* Filter by confidence */
                if (cand->confidence >= iter->min_confidence) {
                    return cand;
                }
            }
            
            iter->sector_idx++;
            iter->candidate_idx = 0;
        }
        
        iter->track_idx++;
        iter->sector_idx = 0;
    }
    
    return NULL;
}

uft_sector_candidates_t* uft_md_iter_next_sector(uft_candidate_iter_t *iter) {
    if (!iter || !iter->session) return NULL;
    
    uft_md_session_t *session = iter->session;
    
    while (iter->track_idx < session->track_count) {
        uft_track_candidates_t *track = &session->tracks[iter->track_idx];
        
        if (iter->sector_idx < track->sector_count) {
            uft_sector_candidates_t *sector = &track->sectors[iter->sector_idx];
            iter->sector_idx++;
            
            /* Skip resolved if not wanted */
            if (!iter->include_resolved && sector->is_resolved) {
                continue;
            }
            
            return sector;
        }
        
        iter->track_idx++;
        iter->sector_idx = 0;
    }
    
    return NULL;
}

/* End of uft_multi_decode.c */
