/**
 * @file uft_fusion.c
 * @brief Multi-Revolution Fusion Implementation
 * 
 * ROADMAP F2.3 - Priority P1
 */

#include "uft/decoder/uft_fusion.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// Constants
// ============================================================================

#define MAX_REVOLUTIONS 20
#define MIN_CONSENSUS 0.5

// ============================================================================
// Internal Structures
// ============================================================================

typedef struct {
    uint8_t* data;
    size_t length;
    double quality;
    uint32_t index_pos;
    double rpm;
    int offset;             // Alignment offset
} revolution_t;

struct uft_fusion_s {
    uft_fusion_config_t config;
    
    revolution_t revolutions[MAX_REVOLUTIONS];
    int num_revolutions;
    
    // Fused data
    uint8_t* fused;
    double* confidence;
    uint8_t* weak_map;
    size_t fused_length;
    
    bool processed;
};

// ============================================================================
// Config
// ============================================================================

void uft_fusion_config_default(uft_fusion_config_t* config) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    
    config->min_revolutions = 2;
    config->max_revolutions = 5;
    config->alignment_tolerance = 0.05;
    config->consensus_threshold = 0.66;
    config->detect_weak_bits = true;
    config->use_quality_weights = true;
    config->auto_align = true;
}

// ============================================================================
// Create/Destroy
// ============================================================================

uft_fusion_t* uft_fusion_create(const uft_fusion_config_t* config) {
    uft_fusion_t* fusion = calloc(1, sizeof(uft_fusion_t));
    if (!fusion) return NULL;
    
    if (config) {
        fusion->config = *config;
    } else {
        uft_fusion_config_default(&fusion->config);
    }
    
    return fusion;
}

void uft_fusion_destroy(uft_fusion_t* fusion) {
    if (!fusion) return;
    
    for (int i = 0; i < fusion->num_revolutions; i++) {
        free(fusion->revolutions[i].data);
    }
    
    free(fusion->fused);
    free(fusion->confidence);
    free(fusion->weak_map);
    free(fusion);
}

void uft_fusion_reset(uft_fusion_t* fusion) {
    if (!fusion) return;
    
    for (int i = 0; i < fusion->num_revolutions; i++) {
        free(fusion->revolutions[i].data);
    }
    
    fusion->num_revolutions = 0;
    
    free(fusion->fused);
    free(fusion->confidence);
    free(fusion->weak_map);
    fusion->fused = NULL;
    fusion->confidence = NULL;
    fusion->weak_map = NULL;
    fusion->fused_length = 0;
    fusion->processed = false;
}

// ============================================================================
// Add Revolution
// ============================================================================

int uft_fusion_add_revolution(uft_fusion_t* fusion,
                               const uint8_t* data, size_t length,
                               double quality) {
    uft_fusion_revolution_t rev = {
        .data = data,
        .length = length,
        .quality = quality,
        .index_pos = 0,
        .rpm = 300.0
    };
    
    return uft_fusion_add_revolution_ex(fusion, &rev);
}

int uft_fusion_add_revolution_ex(uft_fusion_t* fusion,
                                  const uft_fusion_revolution_t* rev) {
    if (!fusion || !rev || !rev->data) return -1;
    if (fusion->num_revolutions >= MAX_REVOLUTIONS) return -1;
    if (fusion->num_revolutions >= fusion->config.max_revolutions) return -1;
    
    int idx = fusion->num_revolutions;
    revolution_t* r = &fusion->revolutions[idx];
    
    r->data = malloc(rev->length);
    if (!r->data) return -1;
    
    memcpy(r->data, rev->data, rev->length);
    r->length = rev->length;
    r->quality = rev->quality;
    r->index_pos = rev->index_pos;
    r->rpm = rev->rpm;
    r->offset = 0;
    
    fusion->num_revolutions++;
    fusion->processed = false;
    
    return idx;
}

// ============================================================================
// Alignment
// ============================================================================

static int find_best_offset(const uint8_t* ref, size_t ref_len,
                            const uint8_t* target, size_t target_len,
                            int max_shift) {
    int best_offset = 0;
    int best_match = 0;
    
    for (int offset = -max_shift; offset <= max_shift; offset++) {
        int matches = 0;
        
        for (size_t i = 0; i < ref_len && i < target_len; i++) {
            int ti = (int)i + offset;
            if (ti >= 0 && (size_t)ti < target_len) {
                if (ref[i] == target[ti]) matches++;
            }
        }
        
        if (matches > best_match) {
            best_match = matches;
            best_offset = offset;
        }
    }
    
    return best_offset;
}

int uft_fusion_align(uft_fusion_t* fusion) {
    if (!fusion || fusion->num_revolutions < 2) return -1;
    
    // Use first revolution as reference
    revolution_t* ref = &fusion->revolutions[0];
    ref->offset = 0;
    
    int max_shift = (int)(ref->length * fusion->config.alignment_tolerance);
    
    for (int i = 1; i < fusion->num_revolutions; i++) {
        revolution_t* r = &fusion->revolutions[i];
        r->offset = find_best_offset(ref->data, ref->length,
                                     r->data, r->length, max_shift);
    }
    
    return 0;
}

// ============================================================================
// Process
// ============================================================================

static uint8_t get_bit_at(const revolution_t* r, size_t global_pos) {
    int local_pos = (int)global_pos - r->offset;
    if (local_pos < 0 || (size_t)local_pos >= r->length * 8) {
        return 0xFF;  // Invalid
    }
    
    return (r->data[local_pos / 8] >> (7 - (local_pos % 8))) & 1;
}

int uft_fusion_process(uft_fusion_t* fusion, uft_fusion_result_t* result) {
    if (!fusion || !result) return -1;
    if (fusion->num_revolutions < fusion->config.min_revolutions) return -1;
    
    memset(result, 0, sizeof(*result));
    
    // Auto-align if enabled
    if (fusion->config.auto_align) {
        uft_fusion_align(fusion);
    }
    
    // Find common length
    size_t min_len = fusion->revolutions[0].length;
    for (int i = 1; i < fusion->num_revolutions; i++) {
        if (fusion->revolutions[i].length < min_len) {
            min_len = fusion->revolutions[i].length;
        }
    }
    
    size_t bit_count = min_len * 8;
    size_t byte_count = min_len;
    
    // Allocate output
    result->fused_data = calloc(1, byte_count);
    result->weak_map = calloc(1, byte_count);
    result->bit_confidence = calloc(bit_count, sizeof(double));
    result->weak_positions = calloc(bit_count, sizeof(int));
    
    if (!result->fused_data || !result->weak_map || 
        !result->bit_confidence || !result->weak_positions) {
        uft_fusion_result_free(result);
        return -1;
    }
    
    result->data_length = byte_count;
    
    // Process each bit
    double total_confidence = 0;
    
    for (size_t bit = 0; bit < bit_count; bit++) {
        double weighted_sum = 0;
        double total_weight = 0;
        int ones = 0;
        int zeros = 0;
        int valid_revs = 0;
        
        for (int r = 0; r < fusion->num_revolutions; r++) {
            uint8_t b = get_bit_at(&fusion->revolutions[r], bit);
            if (b == 0xFF) continue;
            
            double weight = fusion->config.use_quality_weights ?
                           fusion->revolutions[r].quality : 1.0;
            
            weighted_sum += b * weight;
            total_weight += weight;
            
            if (b) ones++;
            else zeros++;
            
            valid_revs++;
        }
        
        if (valid_revs == 0) continue;
        
        // Calculate consensus
        double consensus = weighted_sum / total_weight;
        uint8_t fused_bit = (consensus >= 0.5) ? 1 : 0;
        
        // Calculate confidence
        double conf = (fused_bit) ? consensus : (1.0 - consensus);
        result->bit_confidence[bit] = conf;
        total_confidence += conf;
        
        // Store fused bit
        if (fused_bit) {
            result->fused_data[bit / 8] |= (1 << (7 - (bit % 8)));
        }
        
        // Detect weak bits
        if (fusion->config.detect_weak_bits) {
            int disagreements = (fused_bit) ? zeros : ones;
            double agreement_ratio = 1.0 - (double)disagreements / valid_revs;
            
            if (agreement_ratio < fusion->config.consensus_threshold) {
                result->weak_map[bit / 8] |= (1 << (7 - (bit % 8)));
                result->weak_positions[result->weak_count++] = (int)bit;
            }
        }
    }
    
    // Overall statistics
    result->overall_confidence = total_confidence / bit_count;
    result->revolutions_used = fusion->num_revolutions;
    
    // Calculate RPM stats
    double rpm_sum = 0;
    for (int i = 0; i < fusion->num_revolutions; i++) {
        rpm_sum += fusion->revolutions[i].rpm;
    }
    result->rpm_average = rpm_sum / fusion->num_revolutions;
    
    double rpm_var = 0;
    for (int i = 0; i < fusion->num_revolutions; i++) {
        double diff = fusion->revolutions[i].rpm - result->rpm_average;
        rpm_var += diff * diff;
    }
    result->rpm_deviation = sqrt(rpm_var / fusion->num_revolutions);
    
    // Store internally
    fusion->fused = malloc(byte_count);
    if (fusion->fused) {
        memcpy(fusion->fused, result->fused_data, byte_count);
    }
    fusion->fused_length = byte_count;
    fusion->processed = true;
    
    return 0;
}

// ============================================================================
// Query
// ============================================================================

int uft_fusion_get_bit(const uft_fusion_t* fusion, size_t pos,
                       double* confidence) {
    if (!fusion || !fusion->processed || pos >= fusion->fused_length * 8) {
        return -1;
    }
    
    uint8_t bit = (fusion->fused[pos / 8] >> (7 - (pos % 8))) & 1;
    
    if (confidence && fusion->confidence) {
        *confidence = fusion->confidence[pos];
    }
    
    return bit;
}

bool uft_fusion_is_weak(const uft_fusion_t* fusion, size_t pos) {
    if (!fusion || !fusion->weak_map || pos >= fusion->fused_length * 8) {
        return false;
    }
    
    return (fusion->weak_map[pos / 8] >> (7 - (pos % 8))) & 1;
}

// ============================================================================
// Result
// ============================================================================

void uft_fusion_result_free(uft_fusion_result_t* result) {
    if (!result) return;
    
    free(result->fused_data);
    free(result->weak_map);
    free(result->bit_confidence);
    free(result->weak_positions);
    
    memset(result, 0, sizeof(*result));
}

// ============================================================================
// Analysis
// ============================================================================

double uft_fusion_analyze_quality(const uint8_t* data, size_t length) {
    if (!data || length == 0) return 0;
    
    // Simple quality metric: bit transitions
    int transitions = 0;
    int total_bits = length * 8;
    
    for (size_t i = 1; i < total_bits; i++) {
        int prev = (data[(i-1) / 8] >> (7 - ((i-1) % 8))) & 1;
        int curr = (data[i / 8] >> (7 - (i % 8))) & 1;
        if (prev != curr) transitions++;
    }
    
    // Good quality = ~25-50% transitions (MFM)
    double ratio = (double)transitions / total_bits;
    
    if (ratio < 0.1 || ratio > 0.7) return 0.5;  // Poor
    if (ratio >= 0.2 && ratio <= 0.5) return 0.95;  // Good
    return 0.75;  // OK
}

void uft_fusion_get_weak_stats(const uft_fusion_t* fusion,
                                uft_fusion_weak_stats_t* stats) {
    if (!fusion || !stats) return;
    
    memset(stats, 0, sizeof(*stats));
    
    if (!fusion->weak_map || fusion->fused_length == 0) return;
    
    size_t total_bits = fusion->fused_length * 8;
    int in_zone = 0;
    int zone_len = 0;
    
    for (size_t i = 0; i < total_bits; i++) {
        int weak = (fusion->weak_map[i / 8] >> (7 - (i % 8))) & 1;
        
        if (weak) {
            stats->total_weak++;
            zone_len++;
            
            if (!in_zone) {
                stats->weak_zones++;
                in_zone = 1;
            }
        } else {
            if (in_zone && zone_len > stats->max_zone_length) {
                stats->max_zone_length = zone_len;
            }
            in_zone = 0;
            zone_len = 0;
        }
    }
    
    if (zone_len > stats->max_zone_length) {
        stats->max_zone_length = zone_len;
    }
    
    stats->weak_ratio = (double)stats->total_weak / total_bits;
}
