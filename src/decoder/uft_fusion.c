/**
 * @file uft_fusion.c
 * @brief Multi-Revolution Fusion Implementation
 * 
 * Combines multiple disk revolutions using confidence-weighted voting
 * to recover data from degraded or copy-protected media.
 * 
 * @version 2.0.0
 * @date 2025-01-08
 */

#include "uft/decoder/uft_fusion.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * Constants
 *============================================================================*/

#define MAX_REVOLUTIONS 20

/*============================================================================
 * Internal Structures
 *============================================================================*/

typedef struct {
    uint8_t* data;
    size_t length;
    double* confidence;
    double quality;
    uint32_t index_pos;
    int offset;  /* Alignment offset */
} revolution_data_t;

struct uft_fusion_s {
    uft_fusion_config_t config;
    
    revolution_data_t revolutions[MAX_REVOLUTIONS];
    int num_revolutions;
    
    /* Fused result */
    uint8_t* fused;
    double* fused_confidence;
    uint8_t* weak_map;
    size_t fused_length;
    
    bool processed;
};

/*============================================================================
 * Configuration
 *============================================================================*/

void uft_fusion_default_config(uft_fusion_config_t* config) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    config->min_revolutions = 2;
    config->max_revolutions = 5;
    config->alignment_tolerance = 0.05;
    config->consensus_threshold = 0.66;
    config->detect_weak_bits = true;
    config->use_quality_weights = true;
}

/*============================================================================
 * Lifecycle
 *============================================================================*/

uft_fusion_t* uft_fusion_create(const uft_fusion_config_t* config) {
    uft_fusion_t* fusion = calloc(1, sizeof(uft_fusion_t));
    if (!fusion) return NULL;
    
    if (config) {
        fusion->config = *config;
    } else {
        uft_fusion_default_config(&fusion->config);
    }
    
    return fusion;
}

uft_fusion_t* uft_fusion_create_with_config(const uft_fusion_config_t* config) {
    return uft_fusion_create(config);
}

void uft_fusion_destroy(uft_fusion_t* fusion) {
    if (!fusion) return;
    
    uft_fusion_reset(fusion);
    free(fusion->fused);
    free(fusion->fused_confidence);
    free(fusion->weak_map);
    free(fusion);
}

void uft_fusion_reset(uft_fusion_t* fusion) {
    if (!fusion) return;
    
    for (int i = 0; i < fusion->num_revolutions; i++) {
        free(fusion->revolutions[i].data);
        free(fusion->revolutions[i].confidence);
    }
    
    fusion->num_revolutions = 0;
    fusion->processed = false;
}

int uft_fusion_set_config(uft_fusion_t* fusion, const uft_fusion_config_t* config) {
    if (!fusion || !config) return -1;
    fusion->config = *config;
    return 0;
}

/*============================================================================
 * Revolution Input
 *============================================================================*/

int uft_fusion_add_revolution(
    uft_fusion_t* fusion,
    const uint8_t* data,
    size_t length,
    double quality,
    uint32_t index_pos)
{
    if (!fusion || !data || length == 0) return -1;
    if (fusion->num_revolutions >= MAX_REVOLUTIONS) return -1;
    if (fusion->num_revolutions >= fusion->config.max_revolutions) return -1;
    
    revolution_data_t* rev = &fusion->revolutions[fusion->num_revolutions];
    
    rev->data = malloc(length);
    if (!rev->data) return -1;
    
    memcpy(rev->data, data, length);
    rev->length = length;
    rev->quality = quality;
    rev->index_pos = index_pos;
    rev->offset = 0;
    rev->confidence = NULL;
    
    fusion->num_revolutions++;
    fusion->processed = false;
    
    return 0;
}

int uft_fusion_add_revolution_with_confidence(
    uft_fusion_t* fusion,
    const uint8_t* data,
    size_t length,
    const double* confidence,
    uint32_t index_pos)
{
    if (!fusion || !data || length == 0) return -1;
    if (fusion->num_revolutions >= MAX_REVOLUTIONS) return -1;
    
    revolution_data_t* rev = &fusion->revolutions[fusion->num_revolutions];
    
    rev->data = malloc(length);
    if (!rev->data) return -1;
    
    memcpy(rev->data, data, length);
    rev->length = length;
    rev->quality = 1.0;
    rev->index_pos = index_pos;
    rev->offset = 0;
    
    if (confidence) {
        rev->confidence = malloc(length * sizeof(double));
        if (rev->confidence) {
            memcpy(rev->confidence, confidence, length * sizeof(double));
        }
    } else {
        rev->confidence = NULL;
    }
    
    fusion->num_revolutions++;
    fusion->processed = false;
    
    return 0;
}

/*============================================================================
 * Alignment
 *============================================================================*/

static int find_best_offset(const uint8_t* ref, size_t ref_len,
                            const uint8_t* data, size_t data_len,
                            int max_offset) {
    int best_offset = 0;
    int best_matches = 0;
    
    size_t compare_len = (ref_len < data_len) ? ref_len : data_len;
    
    for (int offset = -max_offset; offset <= max_offset; offset++) {
        int matches = 0;
        
        for (size_t i = 0; i < compare_len; i++) {
            size_t ri = i;
            size_t di = (size_t)((int)i + offset);
            
            if (di < data_len && ri < ref_len) {
                if (ref[ri] == data[di]) {
                    matches++;
                }
            }
        }
        
        if (matches > best_matches) {
            best_matches = matches;
            best_offset = offset;
        }
    }
    
    return best_offset;
}

static void align_revolutions(uft_fusion_t* fusion) {
    if (fusion->num_revolutions < 2) return;
    
    /* Use first revolution as reference */
    revolution_data_t* ref = &fusion->revolutions[0];
    int max_offset = (int)(ref->length * fusion->config.alignment_tolerance);
    
    for (int i = 1; i < fusion->num_revolutions; i++) {
        revolution_data_t* rev = &fusion->revolutions[i];
        rev->offset = find_best_offset(ref->data, ref->length,
                                        rev->data, rev->length,
                                        max_offset);
    }
}

/*============================================================================
 * Fusion Processing
 *============================================================================*/

int uft_fusion_process(uft_fusion_t* fusion, uft_fusion_result_t* result) {
    if (!fusion || !result) return -1;
    if (fusion->num_revolutions < 1) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* Align revolutions */
    align_revolutions(fusion);
    
    /* Find minimum common length */
    size_t min_len = fusion->revolutions[0].length;
    for (int i = 1; i < fusion->num_revolutions; i++) {
        size_t effective_len = fusion->revolutions[i].length;
        if (fusion->revolutions[i].offset > 0) {
            effective_len -= (size_t)fusion->revolutions[i].offset;
        }
        if (effective_len < min_len) {
            min_len = effective_len;
        }
    }
    
    /* Allocate result buffers */
    result->data = calloc(min_len, 1);
    result->confidence = calloc(min_len, sizeof(double));
    result->weak_bits = calloc((min_len + 7) / 8, 1);
    
    if (!result->data || !result->confidence || !result->weak_bits) {
        free(result->data);
        free(result->confidence);
        free(result->weak_bits);
        memset(result, 0, sizeof(*result));
        return -1;
    }
    
    result->length = min_len;
    
    int weak_count = 0;
    double total_confidence = 0.0;
    
    /* Process each byte position */
    for (size_t pos = 0; pos < min_len; pos++) {
        /* Count votes for each possible byte value */
        int votes[256] = {0};
        double weights[256] = {0};
        double total_weight = 0.0;
        
        for (int r = 0; r < fusion->num_revolutions; r++) {
            revolution_data_t* rev = &fusion->revolutions[r];
            size_t src_pos = pos + (size_t)rev->offset;
            
            if (src_pos < rev->length) {
                uint8_t byte = rev->data[src_pos];
                double weight = rev->quality;
                
                if (rev->confidence && src_pos < rev->length) {
                    weight *= rev->confidence[src_pos];
                }
                
                votes[byte]++;
                weights[byte] += weight;
                total_weight += weight;
            }
        }
        
        /* Find winning byte value */
        int best_byte = 0;
        double best_weight = 0.0;
        
        for (int b = 0; b < 256; b++) {
            if (weights[b] > best_weight) {
                best_weight = weights[b];
                best_byte = b;
            }
        }
        
        result->data[pos] = (uint8_t)best_byte;
        
        /* Calculate confidence */
        double consensus = total_weight > 0 ? best_weight / total_weight : 0;
        result->confidence[pos] = consensus;
        total_confidence += consensus;
        
        /* Mark weak bits */
        if (consensus < fusion->config.consensus_threshold) {
            result->weak_bits[pos / 8] |= (1 << (7 - (pos % 8)));
            weak_count++;
        }
    }
    
    result->average_confidence = min_len > 0 ? total_confidence / min_len : 0;
    result->weak_count = weak_count;
    
    fusion->processed = true;
    
    return 0;
}

int uft_fusion_get_result(uft_fusion_t* fusion, uft_fusion_result_t* result) {
    if (!fusion || !result) return -1;
    
    if (!fusion->processed) {
        return uft_fusion_process(fusion, result);
    }
    
    /* Copy cached result */
    result->data = malloc(fusion->fused_length);
    result->confidence = malloc(fusion->fused_length * sizeof(double));
    result->weak_bits = malloc((fusion->fused_length + 7) / 8);
    
    if (!result->data || !result->confidence || !result->weak_bits) {
        free(result->data);
        free(result->confidence);
        free(result->weak_bits);
        memset(result, 0, sizeof(*result));
        return -1;
    }
    
    memcpy(result->data, fusion->fused, fusion->fused_length);
    memcpy(result->confidence, fusion->fused_confidence, 
           fusion->fused_length * sizeof(double));
    memcpy(result->weak_bits, fusion->weak_map, (fusion->fused_length + 7) / 8);
    result->length = fusion->fused_length;
    
    return 0;
}

/*============================================================================
 * Cleanup
 *============================================================================*/

void uft_fusion_result_free(uft_fusion_result_t* result) {
    if (!result) return;
    
    free(result->data);
    free(result->confidence);
    free(result->weak_bits);
    memset(result, 0, sizeof(*result));
}
