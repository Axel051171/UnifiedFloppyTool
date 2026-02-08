/**
 * @file uft_multi_rev_fusion.c
 * @brief GOD MODE: Multi-Revolution Confidence Fusion
 * 
 * Combines multiple flux revolutions to improve data recovery.
 * Identifies weak bits through inconsistency analysis.
 * 
 * @author Superman QA - GOD MODE
 * @date 2026
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// ============================================================================
// Types
// ============================================================================

typedef struct {
    uint8_t value;              // 0 or 1
    float confidence;           // 0.0 - 1.0
    bool weak_bit;              // Inconsistent across revolutions
    uint8_t vote_ones;          // How many revolutions had 1
    uint8_t vote_zeros;         // How many revolutions had 0
} fused_bit_t;

typedef struct {
    fused_bit_t* bits;
    size_t bit_count;
    
    // Statistics
    float overall_confidence;
    size_t weak_bit_count;
    size_t total_votes;
    float avg_agreement;
} fused_bitstream_t;

typedef struct {
    float weak_threshold;       // Below this = weak bit (default 0.8)
    float strong_threshold;     // Above this = high confidence (default 0.95)
    bool weight_by_timing;      // Weight revolutions by PLL quality
    float* revolution_weights;  // Per-revolution weights (optional)
} fusion_config_t;

// ============================================================================
// Default Configuration
// ============================================================================

static const fusion_config_t DEFAULT_FUSION_CONFIG = {
    .weak_threshold = 0.8f,
    .strong_threshold = 0.95f,
    .weight_by_timing = false,
    .revolution_weights = NULL
};

// ============================================================================
// Core Fusion
// ============================================================================

/**
 * @brief Fuse multiple bitstream revolutions
 * 
 * @param revolutions Array of bitstream arrays
 * @param num_revolutions Number of revolutions
 * @param bits_per_rev Bits in each revolution
 * @param config Fusion configuration
 * @param result Output fused bitstream (caller frees)
 * @return true on success
 */
bool fuse_revolutions(const uint8_t** revolutions, size_t num_revolutions,
                      size_t bits_per_rev, const fusion_config_t* config,
                      fused_bitstream_t* result) {
    if (!revolutions || num_revolutions == 0 || bits_per_rev == 0) {
        return false;
    }
    
    if (!config) {
        config = &DEFAULT_FUSION_CONFIG;
    }
    
    // Allocate result
    result->bits = calloc(bits_per_rev, sizeof(fused_bit_t));
    if (!result->bits) return false;
    result->bit_count = bits_per_rev;
    
    float total_confidence = 0;
    size_t weak_count = 0;
    
    // Process each bit position
    for (size_t i = 0; i < bits_per_rev; i++) {
        fused_bit_t* fb = &result->bits[i];
        float weighted_ones = 0, weighted_zeros = 0;
        float total_weight = 0;
        
        // Count votes from each revolution
        for (size_t r = 0; r < num_revolutions; r++) {
            float weight = 1.0f;
            if (config->weight_by_timing && config->revolution_weights) {
                weight = config->revolution_weights[r];
            }
            
            // Get bit from this revolution
            size_t byte_idx = i / 8;
            int bit_idx = 7 - (i % 8);
            uint8_t bit = (revolutions[r][byte_idx] >> bit_idx) & 1;
            
            if (bit) {
                weighted_ones += weight;
                fb->vote_ones++;
            } else {
                weighted_zeros += weight;
                fb->vote_zeros++;
            }
            total_weight += weight;
        }
        
        // Determine value by weighted majority
        fb->value = (weighted_ones > weighted_zeros) ? 1 : 0;
        
        // Calculate confidence
        float majority = (weighted_ones > weighted_zeros) ? 
                         weighted_ones : weighted_zeros;
        fb->confidence = majority / total_weight;
        
        // Determine if weak bit
        fb->weak_bit = (fb->confidence < config->weak_threshold);
        if (fb->weak_bit) {
            weak_count++;
        }
        
        total_confidence += fb->confidence;
    }
    
    // Calculate statistics
    result->overall_confidence = total_confidence / bits_per_rev;
    result->weak_bit_count = weak_count;
    result->total_votes = num_revolutions * bits_per_rev;
    result->avg_agreement = result->overall_confidence;
    
    return true;
}

/**
 * @brief Convert fused bitstream to byte array
 */
size_t fused_to_bytes(const fused_bitstream_t* fused, uint8_t* output,
                      size_t max_bytes) {
    size_t bytes = (fused->bit_count + 7) / 8;
    if (bytes > max_bytes) bytes = max_bytes;
    
    memset(output, 0, bytes);
    
    for (size_t i = 0; i < fused->bit_count && i < max_bytes * 8; i++) {
        if (fused->bits[i].value) {
            output[i / 8] |= (1 << (7 - (i % 8)));
        }
    }
    
    return bytes;
}

/**
 * @brief Get weak bit positions
 */
size_t get_weak_bit_positions(const fused_bitstream_t* fused,
                              size_t* positions, size_t max_positions) {
    size_t count = 0;
    
    for (size_t i = 0; i < fused->bit_count && count < max_positions; i++) {
        if (fused->bits[i].weak_bit) {
            positions[count++] = i;
        }
    }
    
    return count;
}

/**
 * @brief Free fused bitstream
 */
void fused_bitstream_free(fused_bitstream_t* fused) {
    free(fused->bits);
    fused->bits = NULL;
    fused->bit_count = 0;
}

// ============================================================================
// Advanced: Weighted Fusion with Timing Quality
// ============================================================================

typedef struct {
    float pll_confidence;       // From PLL statistics
    float sync_quality;         // From sync detection
    float crc_rate;             // CRC pass rate for this rev
} revolution_quality_t;

/**
 * @brief Calculate revolution weights from quality metrics
 */
void calculate_revolution_weights(const revolution_quality_t* qualities,
                                   size_t num_revs, float* weights) {
    float total = 0;
    
    // Combine quality metrics
    for (size_t r = 0; r < num_revs; r++) {
        weights[r] = qualities[r].pll_confidence * 0.3f +
                     qualities[r].sync_quality * 0.3f +
                     qualities[r].crc_rate * 0.4f;
        total += weights[r];
    }
    
    // Normalize
    if (total > 0) {
        for (size_t r = 0; r < num_revs; r++) {
            weights[r] /= total;
            weights[r] *= num_revs;  // Scale so sum = num_revs
        }
    }
}

// ============================================================================
// Weak Bit Pattern Analysis
// ============================================================================

typedef struct {
    size_t start_bit;
    size_t end_bit;
    size_t length;
    float avg_confidence;
} weak_region_t;

/**
 * @brief Find contiguous weak bit regions
 */
size_t find_weak_regions(const fused_bitstream_t* fused,
                          weak_region_t* regions, size_t max_regions) {
    size_t count = 0;
    size_t region_start = SIZE_MAX;
    
    for (size_t i = 0; i <= fused->bit_count && count < max_regions; i++) {
        bool is_weak = (i < fused->bit_count) && fused->bits[i].weak_bit;
        
        if (is_weak && region_start == SIZE_MAX) {
            // Start new region
            region_start = i;
        } else if (!is_weak && region_start != SIZE_MAX) {
            // End region
            weak_region_t* r = &regions[count++];
            r->start_bit = region_start;
            r->end_bit = i;
            r->length = i - region_start;
            
            // Calculate average confidence in region
            float sum = 0;
            for (size_t j = region_start; j < i; j++) {
                sum += fused->bits[j].confidence;
            }
            r->avg_confidence = sum / r->length;
            
            region_start = SIZE_MAX;
        }
    }
    
    return count;
}
