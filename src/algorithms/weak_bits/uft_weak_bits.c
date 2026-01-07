/**
 * @file uft_weak_bits.c
 * @brief Weak bit detection and handling implementation
 */

#include "uft_weak_bits.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Single Bit Operations
 * ============================================================================ */

uft_prob_bit_t uft_prob_bit_from_timing(bool pulse_in_window,
                                        double distance_to_center,
                                        double window_size) {
    uft_prob_bit_t result;
    result.value = pulse_in_window ? 1 : 0;
    
    double half_window = window_size / 2.0;
    
    if (distance_to_center < half_window * 0.3) {
        /* Very close to center - high confidence */
        result.confidence = 255;
        result.is_weak = false;
    } else if (distance_to_center < half_window * 0.6) {
        /* Good position */
        result.confidence = 200;
        result.is_weak = false;
    } else if (distance_to_center < half_window * 0.85) {
        /* Edge of window - medium confidence */
        result.confidence = 128;
        result.is_weak = false;
    } else if (distance_to_center < half_window) {
        /* Very edge - low confidence, potentially weak */
        result.confidence = 80;
        result.is_weak = true;
    } else {
        /* Outside window */
        result.confidence = 40;
        result.is_weak = true;
    }
    
    return result;
}

void uft_fusion_add_sample(uft_bit_fusion_t *fusion,
                           uint8_t value,
                           uint8_t confidence) {
    if (!fusion || fusion->sample_count >= UFT_WEAK_MAX_REVISIONS) return;
    
    fusion->samples[fusion->sample_count].value = value;
    fusion->samples[fusion->sample_count].confidence = confidence;
    fusion->samples[fusion->sample_count].timing_error = 0;
    fusion->sample_count++;
}

uft_prob_bit_t uft_fusion_fuse(const uft_bit_fusion_t *fusion) {
    uft_prob_bit_t result = {0, 0, true};
    
    if (!fusion || fusion->sample_count == 0) {
        return result;
    }
    
    /* Weighted voting based on confidence */
    int weighted_0 = 0;
    int weighted_1 = 0;
    int total_weight = 0;
    
    /* Track consistency */
    bool all_same = true;
    uint8_t first_value = fusion->samples[0].value;
    
    for (size_t i = 0; i < fusion->sample_count; i++) {
        const uft_bit_sample_t *s = &fusion->samples[i];
        int weight = s->confidence;
        
        if (s->value == 0) {
            weighted_0 += weight;
        } else {
            weighted_1 += weight;
        }
        total_weight += weight;
        
        if (s->value != first_value) {
            all_same = false;
        }
    }
    
    /* Determine value */
    if (weighted_1 > weighted_0) {
        result.value = 1;
        result.confidence = (uint8_t)(weighted_1 * 255 / total_weight);
    } else {
        result.value = 0;
        result.confidence = (uint8_t)(weighted_0 * 255 / total_weight);
    }
    
    /* Weak if samples disagree or overall low confidence */
    result.is_weak = !all_same || (result.confidence < UFT_CONF_WEAK_THRESHOLD);
    
    return result;
}

void uft_fusion_clear(uft_bit_fusion_t *fusion) {
    if (!fusion) return;
    fusion->sample_count = 0;
    memset(fusion->samples, 0, sizeof(fusion->samples));
}

/* ============================================================================
 * Track Operations
 * ============================================================================ */

int uft_weak_track_init(uft_weak_track_t *track, size_t bit_count) {
    if (!track) return -1;
    memset(track, 0, sizeof(*track));
    
    track->bit_count = bit_count;
    size_t byte_count = (bit_count + 7) / 8;
    
    track->bits = calloc(byte_count, 1);
    track->confidence = calloc(bit_count, 1);
    track->weak_flags = calloc(bit_count, sizeof(bool));
    
    if (!track->bits || !track->confidence || !track->weak_flags) {
        uft_weak_track_free(track);
        return -2;
    }
    
    /* Initial region capacity */
    track->region_capacity = 16;
    track->regions = calloc(track->region_capacity, sizeof(uft_weak_region_t));
    if (!track->regions) {
        uft_weak_track_free(track);
        return -3;
    }
    
    return 0;
}

void uft_weak_track_free(uft_weak_track_t *track) {
    if (!track) return;
    
    free(track->bits);
    free(track->confidence);
    free(track->weak_flags);
    free(track->regions);
    
    memset(track, 0, sizeof(*track));
}

void uft_weak_track_set_bit(uft_weak_track_t *track,
                            size_t index,
                            uint8_t value,
                            uint8_t confidence) {
    if (!track || index >= track->bit_count) return;
    
    size_t byte_idx = index / 8;
    uint8_t bit_mask = 0x80 >> (index % 8);
    
    if (value) {
        track->bits[byte_idx] |= bit_mask;
    } else {
        track->bits[byte_idx] &= ~bit_mask;
    }
    
    track->confidence[index] = confidence;
    track->weak_flags[index] = (confidence < UFT_CONF_WEAK_THRESHOLD);
}

void uft_weak_track_set_prob_bit(uft_weak_track_t *track,
                                 size_t index,
                                 const uft_prob_bit_t *bit) {
    if (!track || !bit || index >= track->bit_count) return;
    
    uft_weak_track_set_bit(track, index, bit->value, bit->confidence);
    track->weak_flags[index] = bit->is_weak;
}

size_t uft_weak_track_detect_regions(uft_weak_track_t *track,
                                     size_t min_region_size) {
    if (!track) return 0;
    
    track->region_count = 0;
    track->total_weak_bits = 0;
    track->total_strong_bits = 0;
    
    bool in_region = false;
    size_t region_start = 0;
    size_t region_weak_count = 0;
    uint8_t region_min_conf = 255;
    size_t region_conf_sum = 0;
    
    for (size_t i = 0; i <= track->bit_count; i++) {
        bool is_weak = (i < track->bit_count) ? track->weak_flags[i] : false;
        
        if (is_weak) {
            track->total_weak_bits++;
            
            if (!in_region) {
                /* Start new region */
                in_region = true;
                region_start = i;
                region_weak_count = 0;
                region_min_conf = 255;
                region_conf_sum = 0;
            }
            
            /* Update region stats */
            region_weak_count++;
            if (track->confidence[i] < region_min_conf) {
                region_min_conf = track->confidence[i];
            }
            region_conf_sum += track->confidence[i];
            
        } else {
            track->total_strong_bits++;
            
            if (in_region) {
                /* End region */
                size_t length = i - region_start;
                
                if (length >= min_region_size) {
                    /* Store region */
                    if (track->region_count >= track->region_capacity) {
                        track->region_capacity *= 2;
                        track->regions = realloc(track->regions,
                            track->region_capacity * sizeof(uft_weak_region_t));
                    }
                    
                    uft_weak_region_t *r = &track->regions[track->region_count++];
                    r->start_bit = region_start;
                    r->end_bit = i;
                    r->length = length;
                    r->min_confidence = region_min_conf;
                    r->avg_confidence = (uint8_t)(region_conf_sum / length);
                    r->weak_count = region_weak_count;
                }
                
                in_region = false;
            }
        }
    }
    
    /* Calculate ratio */
    if (track->bit_count > 0) {
        track->weak_ratio = (double)track->total_weak_bits / track->bit_count;
    }
    
    return track->region_count;
}

int uft_weak_track_merge(uft_weak_track_t *out,
                         const uft_weak_track_t **tracks,
                         size_t track_count) {
    if (!out || !tracks || track_count == 0) return -1;
    
    /* Use first track's size */
    size_t bit_count = tracks[0]->bit_count;
    
    /* Initialize output if needed */
    if (out->bit_count == 0) {
        int rc = uft_weak_track_init(out, bit_count);
        if (rc != 0) return rc;
    }
    
    /* Merge each bit */
    for (size_t i = 0; i < bit_count; i++) {
        uft_bit_fusion_t fusion;
        uft_fusion_clear(&fusion);
        
        for (size_t t = 0; t < track_count; t++) {
            if (i >= tracks[t]->bit_count) continue;
            
            size_t byte_idx = i / 8;
            uint8_t bit_mask = 0x80 >> (i % 8);
            uint8_t value = (tracks[t]->bits[byte_idx] & bit_mask) ? 1 : 0;
            
            uft_fusion_add_sample(&fusion, value, tracks[t]->confidence[i]);
        }
        
        uft_prob_bit_t fused = uft_fusion_fuse(&fusion);
        uft_weak_track_set_prob_bit(out, i, &fused);
    }
    
    /* Detect regions in merged result */
    uft_weak_track_detect_regions(out, 4);
    
    return 0;
}

size_t uft_weak_track_compare(const uft_weak_track_t *a,
                              const uft_weak_track_t *b,
                              size_t *out_diff_positions,
                              size_t diff_capacity) {
    if (!a || !b) return 0;
    
    size_t min_len = (a->bit_count < b->bit_count) ? a->bit_count : b->bit_count;
    size_t diff_count = 0;
    
    for (size_t i = 0; i < min_len; i++) {
        size_t byte_idx = i / 8;
        uint8_t bit_mask = 0x80 >> (i % 8);
        
        uint8_t val_a = (a->bits[byte_idx] & bit_mask) ? 1 : 0;
        uint8_t val_b = (b->bits[byte_idx] & bit_mask) ? 1 : 0;
        
        if (val_a != val_b) {
            if (out_diff_positions && diff_count < diff_capacity) {
                out_diff_positions[diff_count] = i;
            }
            diff_count++;
        }
    }
    
    return diff_count;
}

size_t uft_weak_track_get_mask(const uft_weak_track_t *track,
                               uint8_t *out_mask,
                               size_t mask_size) {
    if (!track || !out_mask) return 0;
    
    memset(out_mask, 0, mask_size);
    size_t weak_count = 0;
    
    size_t max_bits = (mask_size * 8 < track->bit_count) ? 
                      mask_size * 8 : track->bit_count;
    
    for (size_t i = 0; i < max_bits; i++) {
        if (track->weak_flags[i]) {
            size_t byte_idx = i / 8;
            uint8_t bit_mask = 0x80 >> (i % 8);
            out_mask[byte_idx] |= bit_mask;
            weak_count++;
        }
    }
    
    return weak_count;
}

void uft_weak_track_dump(const uft_weak_track_t *track) {
    if (!track) {
        printf("Weak Track: NULL\n");
        return;
    }
    
    printf("=== Weak Track Analysis ===\n");
    printf("Bits: %zu\n", track->bit_count);
    printf("Weak bits: %zu (%.2f%%)\n", 
           track->total_weak_bits, track->weak_ratio * 100);
    printf("Strong bits: %zu\n", track->total_strong_bits);
    printf("Weak regions: %zu\n", track->region_count);
    
    for (size_t i = 0; i < track->region_count; i++) {
        const uft_weak_region_t *r = &track->regions[i];
        printf("  Region %zu: bits %zu-%zu (len=%zu, min_conf=%u, weak=%zu)\n",
               i, r->start_bit, r->end_bit, r->length,
               r->min_confidence, r->weak_count);
    }
}
