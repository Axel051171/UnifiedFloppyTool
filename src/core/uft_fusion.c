/**
 * @file uft_fusion.c
 * @brief Multi-revision data fusion implementation
 * 
 * P1-007: Weighted confidence merge replacing simple majority voting
 */

#include "uft/core/uft_fusion.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static inline int get_bit(const uint8_t *data, size_t bit_pos) {
    return (data[bit_pos / 8] >> (7 - (bit_pos % 8))) & 1;
}

static inline void set_bit(uint8_t *data, size_t bit_pos, int value) {
    size_t byte_idx = bit_pos / 8;
    int bit_idx = 7 - (bit_pos % 8);
    
    if (value) {
        data[byte_idx] |= (1 << bit_idx);
    } else {
        data[byte_idx] &= ~(1 << bit_idx);
    }
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

void uft_fusion_options_init(uft_fusion_options_t *opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    opts->method = UFT_FUSION_WEIGHTED;
    opts->crc_valid_bonus = 50;
    opts->recent_bonus = 10;
    opts->weak_threshold = 2;
    opts->confidence_min = 0.3f;
    opts->use_timing = false;
    opts->timing_tolerance = 100.0;  /* 100ns */
    opts->generate_weak_map = true;
    opts->generate_confidence = true;
}

/* ============================================================================
 * Single Bit Fusion
 * ============================================================================ */

uft_fused_bit_t uft_fusion_bit(const uft_revision_input_t *revisions,
                               size_t rev_count,
                               size_t bit_pos,
                               const uft_fusion_options_t *opts) {
    uft_fused_bit_t result = {0};
    
    if (!revisions || rev_count == 0) {
        return result;
    }
    
    /* Collect votes with weights */
    int weight_0 = 0, weight_1 = 0;
    int count_0 = 0, count_1 = 0;
    int first_value = -1;
    bool values_differ = false;
    double timing_sum = 0, timing_sq_sum = 0;
    int timing_count = 0;
    
    for (size_t r = 0; r < rev_count; r++) {
        const uft_revision_input_t *rev = &revisions[r];
        
        if (bit_pos >= rev->bit_count) continue;
        
        int bit_val = get_bit(rev->data, bit_pos);
        
        /* Calculate weight */
        int weight = 100;  /* Base weight */
        
        /* Confidence bonus */
        if (rev->confidence) {
            weight = weight * rev->confidence[bit_pos / 8] / 255;
        }
        
        /* CRC bonus */
        if (rev->crc_valid && opts && opts->crc_valid_bonus > 0) {
            weight += opts->crc_valid_bonus;
        }
        
        /* Quality bonus */
        if (rev->quality > 0) {
            weight = weight * rev->quality / 100;
        }
        
        /* Recent revision bonus */
        if (opts && opts->recent_bonus > 0 && rev_count > 1) {
            int recency = (int)(rev_count - 1 - r);
            weight += opts->recent_bonus * recency / (int)(rev_count - 1);
        }
        
        /* Accumulate */
        if (bit_val) {
            weight_1 += weight;
            count_1++;
        } else {
            weight_0 += weight;
            count_0++;
        }
        
        /* Track differences */
        if (first_value < 0) {
            first_value = bit_val;
        } else if (bit_val != first_value) {
            values_differ = true;
        }
        
        /* Timing variance */
        if (rev->timing && opts && opts->use_timing) {
            double t = rev->timing[bit_pos];
            timing_sum += t;
            timing_sq_sum += t * t;
            timing_count++;
        }
    }
    
    /* Determine result */
    if (weight_1 > weight_0) {
        result.value = 1;
        result.agreement = count_1;
    } else {
        result.value = 0;
        result.agreement = count_0;
    }
    
    /* Calculate confidence */
    int total_weight = weight_0 + weight_1;
    if (total_weight > 0) {
        int winner_weight = (result.value) ? weight_1 : weight_0;
        result.confidence = (uint8_t)(winner_weight * 255 / total_weight);
    }
    
    /* Weak bit detection */
    int total_count = count_0 + count_1;
    int disagreement = (result.value) ? count_0 : count_1;
    
    if (opts) {
        result.is_weak = values_differ && 
                        (disagreement >= opts->weak_threshold);
    } else {
        result.is_weak = values_differ && (disagreement >= 2);
    }
    
    /* Timing variance */
    if (timing_count > 1) {
        double mean = timing_sum / timing_count;
        result.timing_variance = (timing_sq_sum / timing_count) - (mean * mean);
    }
    
    return result;
}

/* ============================================================================
 * Full Merge
 * ============================================================================ */

uft_error_t uft_fusion_merge(const uft_revision_input_t *revisions,
                             size_t rev_count,
                             uint8_t *out_data,
                             size_t *out_bits,
                             uint8_t *out_confidence,
                             bool *out_weak,
                             const uft_fusion_options_t *opts,
                             uft_fusion_result_t *result) {
    if (!revisions || rev_count == 0 || !out_data || !out_bits) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Use defaults if no options */
    uft_fusion_options_t default_opts;
    if (!opts) {
        uft_fusion_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Find maximum bit count */
    size_t max_bits = 0;
    for (size_t r = 0; r < rev_count; r++) {
        if (revisions[r].bit_count > max_bits) {
            max_bits = revisions[r].bit_count;
        }
    }
    
    if (max_bits == 0) {
        *out_bits = 0;
        return UFT_OK;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->total_bits = max_bits;
        result->revision_count = rev_count;
    }
    
    /* Initialize output */
    size_t byte_count = (max_bits + 7) / 8;
    memset(out_data, 0, byte_count);
    
    if (out_confidence) {
        memset(out_confidence, 0, byte_count);
    }
    if (out_weak) {
        memset(out_weak, 0, max_bits * sizeof(bool));
    }
    
    /* Process each bit */
    size_t unanimous = 0, majority = 0, weak = 0, uncertain = 0;
    float confidence_sum = 0;
    
    for (size_t bit = 0; bit < max_bits; bit++) {
        uft_fused_bit_t fused = uft_fusion_bit(revisions, rev_count, bit, opts);
        
        /* Set output bit */
        set_bit(out_data, bit, fused.value);
        
        /* Set confidence */
        if (out_confidence && opts->generate_confidence) {
            out_confidence[bit / 8] = fused.confidence;
        }
        
        /* Set weak flag */
        if (out_weak && opts->generate_weak_map) {
            out_weak[bit] = fused.is_weak;
        }
        
        /* Statistics */
        confidence_sum += fused.confidence / 255.0f;
        
        if (fused.agreement == rev_count) {
            unanimous++;
        } else if (fused.agreement > rev_count / 2) {
            majority++;
        }
        
        if (fused.is_weak) {
            weak++;
        }
        
        if (fused.confidence < 128) {
            uncertain++;
        }
    }
    
    /* Fill result */
    if (result) {
        result->success = true;
        result->unanimous_bits = unanimous;
        result->majority_bits = majority;
        result->weak_bits = weak;
        result->uncertain_bits = uncertain;
        result->overall_confidence = confidence_sum / max_bits;
        result->agreement_ratio = (float)(unanimous + majority) / max_bits;
        
        /* Per-revision stats */
        for (size_t r = 0; r < rev_count && r < UFT_FUSION_MAX_REVISIONS; r++) {
            result->revision_stats[r].quality = revisions[r].quality;
            result->revision_stats[r].crc_valid = revisions[r].crc_valid;
            result->revision_stats[r].bits_contributed = revisions[r].bit_count;
        }
    }
    
    *out_bits = max_bits;
    return UFT_OK;
}

/* ============================================================================
 * Track Fusion
 * ============================================================================ */

uft_error_t uft_fusion_tracks(const uft_track_t *const *tracks,
                              size_t track_count,
                              uft_track_t *out_track,
                              const uft_fusion_options_t *opts,
                              uft_fusion_result_t *result) {
    if (!tracks || track_count == 0 || !out_track) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Prepare revision inputs from tracks */
    uft_revision_input_t *revisions = calloc(track_count, 
                                             sizeof(uft_revision_input_t));
    if (!revisions) {
        return UFT_ERR_MEMORY;
    }
    
    for (size_t i = 0; i < track_count; i++) {
        if (tracks[i] && tracks[i]->raw_data) {
            revisions[i].data = tracks[i]->raw_data;
            revisions[i].bit_count = tracks[i]->raw_bits;
            revisions[i].confidence = tracks[i]->confidence;
            revisions[i].quality = tracks[i]->quality;
            revisions[i].crc_valid = tracks[i]->complete;
            revisions[i].revision_index = (uint8_t)i;
        }
    }
    
    /* Find max bits */
    size_t max_bits = 0;
    for (size_t i = 0; i < track_count; i++) {
        if (revisions[i].bit_count > max_bits) {
            max_bits = revisions[i].bit_count;
        }
    }
    
    /* Allocate output buffers */
    size_t byte_count = (max_bits + 7) / 8;
    
    if (!out_track->raw_data || out_track->raw_capacity < byte_count) {
        free(out_track->raw_data);
        out_track->raw_data = malloc(byte_count);
        if (!out_track->raw_data) {
            free(revisions);
            return UFT_ERR_MEMORY;
        }
        out_track->raw_capacity = byte_count;
    }
    
    if (!out_track->confidence) {
        out_track->confidence = malloc(byte_count);
        if (!out_track->confidence) {
            free(revisions);
            return UFT_ERR_MEMORY;
        }
    }
    
    if (!out_track->weak_mask) {
        out_track->weak_mask = malloc(max_bits * sizeof(bool));
        if (!out_track->weak_mask) {
            free(revisions);
            return UFT_ERR_MEMORY;
        }
    }
    
    /* Perform fusion */
    size_t out_bits;
    uft_error_t err = uft_fusion_merge(revisions, track_count,
                                       out_track->raw_data, &out_bits,
                                       out_track->confidence,
                                       out_track->weak_mask,
                                       opts, result);
    
    out_track->raw_bits = out_bits;
    
    /* Copy metadata from first valid track */
    for (size_t i = 0; i < track_count; i++) {
        if (tracks[i]) {
            out_track->track_num = tracks[i]->track_num;
            out_track->head = tracks[i]->head;
            out_track->encoding = tracks[i]->encoding;
            break;
        }
    }
    
    /* Set quality based on result */
    if (result) {
        out_track->quality = (uint8_t)(result->overall_confidence * 100);
    }
    
    free(revisions);
    return err;
}

/* ============================================================================
 * Sector Fusion
 * ============================================================================ */

uft_error_t uft_fusion_sectors(const uft_sector_t *const *sectors,
                               size_t sector_count,
                               uft_sector_t *out_sector,
                               const uft_fusion_options_t *opts,
                               uft_fusion_result_t *result) {
    if (!sectors || sector_count == 0 || !out_sector) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Find largest sector */
    size_t max_size = 0;
    const uft_sector_t *template_sector = NULL;
    
    for (size_t i = 0; i < sector_count; i++) {
        if (sectors[i] && sectors[i]->data_len > max_size) {
            max_size = sectors[i]->data_len;
            template_sector = sectors[i];
        }
    }
    
    if (!template_sector || max_size == 0) {
        return UFT_ERR_NO_DATA;
    }
    
    /* Copy ID from template */
    out_sector->id = template_sector->id;
    
    /* Allocate output */
    if (!out_sector->data || out_sector->data_len < max_size) {
        free(out_sector->data);
        out_sector->data = malloc(max_size);
        if (!out_sector->data) {
            return UFT_ERR_MEMORY;
        }
        out_sector->data_len = max_size;
    }
    
    if (!out_sector->confidence) {
        out_sector->confidence = malloc(max_size);
        if (!out_sector->confidence) {
            return UFT_ERR_MEMORY;
        }
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->revision_count = sector_count;
    }
    
    /* Per-byte weighted merge */
    for (size_t byte = 0; byte < max_size; byte++) {
        int weights[256] = {0};
        int total_weight = 0;
        
        for (size_t s = 0; s < sector_count; s++) {
            if (!sectors[s] || byte >= sectors[s]->data_len) continue;
            
            uint8_t val = sectors[s]->data[byte];
            int weight = 100;
            
            /* CRC bonus */
            if (sectors[s]->crc_valid && opts && opts->crc_valid_bonus > 0) {
                weight += opts->crc_valid_bonus;
            }
            
            /* Confidence */
            if (sectors[s]->confidence) {
                weight = weight * sectors[s]->confidence[byte] / 255;
            }
            
            weights[val] += weight;
            total_weight += weight;
        }
        
        /* Find winner */
        int best_val = 0;
        int best_weight = 0;
        
        for (int v = 0; v < 256; v++) {
            if (weights[v] > best_weight) {
                best_weight = weights[v];
                best_val = v;
            }
        }
        
        out_sector->data[byte] = (uint8_t)best_val;
        
        if (out_sector->confidence && total_weight > 0) {
            out_sector->confidence[byte] = 
                (uint8_t)(best_weight * 255 / total_weight);
        }
    }
    
    /* Check if result would have valid CRC */
    /* (CRC calculation would go here if we had the algorithm reference) */
    
    if (result) {
        result->success = true;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Analysis
 * ============================================================================ */

void uft_fusion_analyze(const uft_revision_input_t *revisions,
                        size_t rev_count,
                        uft_fusion_result_t *result) {
    if (!revisions || !result || rev_count == 0) return;
    
    memset(result, 0, sizeof(*result));
    result->revision_count = rev_count;
    
    /* Find common bit count */
    size_t min_bits = SIZE_MAX;
    for (size_t r = 0; r < rev_count; r++) {
        if (revisions[r].bit_count < min_bits) {
            min_bits = revisions[r].bit_count;
        }
    }
    
    if (min_bits == 0 || min_bits == SIZE_MAX) return;
    
    result->total_bits = min_bits;
    
    /* Analyze agreement */
    size_t unanimous = 0;
    
    for (size_t bit = 0; bit < min_bits; bit++) {
        int first = get_bit(revisions[0].data, bit);
        bool all_agree = true;
        
        for (size_t r = 1; r < rev_count; r++) {
            if (get_bit(revisions[r].data, bit) != first) {
                all_agree = false;
                break;
            }
        }
        
        if (all_agree) unanimous++;
    }
    
    result->unanimous_bits = unanimous;
    result->agreement_ratio = (float)unanimous / min_bits;
    
    /* Per-revision stats */
    for (size_t r = 0; r < rev_count && r < UFT_FUSION_MAX_REVISIONS; r++) {
        result->revision_stats[r].quality = revisions[r].quality;
        result->revision_stats[r].crc_valid = revisions[r].crc_valid;
        result->revision_stats[r].bits_contributed = revisions[r].bit_count;
    }
    
    result->success = true;
}

uft_fusion_method_t uft_fusion_recommend_method(
    const uft_revision_input_t *revisions,
    size_t rev_count) {
    
    if (!revisions || rev_count == 0) {
        return UFT_FUSION_MAJORITY;
    }
    
    /* Check if we have timing data */
    bool has_timing = false;
    for (size_t r = 0; r < rev_count; r++) {
        if (revisions[r].timing) {
            has_timing = true;
            break;
        }
    }
    
    /* Check if we have confidence data */
    bool has_confidence = false;
    for (size_t r = 0; r < rev_count; r++) {
        if (revisions[r].confidence) {
            has_confidence = true;
            break;
        }
    }
    
    /* Check CRC validity variation */
    int crc_valid_count = 0;
    for (size_t r = 0; r < rev_count; r++) {
        if (revisions[r].crc_valid) crc_valid_count++;
    }
    
    /* Recommendation logic */
    if (has_timing && rev_count >= 4) {
        return UFT_FUSION_TIMING;
    }
    
    if (has_confidence || crc_valid_count > 0) {
        return UFT_FUSION_WEIGHTED;
    }
    
    if (rev_count <= 2) {
        return UFT_FUSION_MAJORITY;
    }
    
    return UFT_FUSION_ADAPTIVE;
}
