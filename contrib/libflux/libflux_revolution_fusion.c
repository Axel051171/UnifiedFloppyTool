/**
 * @file libflux_revolution_fusion.c
 * @brief Multi-Revolution Fusion Implementation
 * 
 * CLEAN-ROOM IMPLEMENTATION
 * 
 * Copyright (C) 2025 UFT Project Contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "libflux_revolution_fusion.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static int count_bits_set(uint8_t byte) {
    int count = 0;
    while (byte) {
        count += byte & 1;
        byte >>= 1;
    }
    return count;
}

static double calculate_jitter(const uint8_t *data, int size, int bitrate) {
    if (!data || size < 10) return 0.0;
    
    int transitions = 0;
    int total_bits = size * 8;
    
    for (int i = 1; i < size; i++) {
        transitions += count_bits_set(data[i] ^ data[i-1]);
    }
    
    double expected_density = (bitrate > 300000) ? 0.5 : 0.33;
    double actual_density = (double)transitions / total_bits;
    
    return fabs(actual_density - expected_density) * 1000.0;
}

/* ============================================================================
 * API IMPLEMENTATION
 * ============================================================================ */

void libflux_fusion_config_init(libflux_fusion_config_t *config) {
    if (!config) return;
    
    config->method = LIBFLUX_FUSION_MAJORITY;
    config->timing_tolerance_ns = LIBFLUX_DEFAULT_TOLERANCE;
    config->min_revolutions = 2;
    config->max_revolutions = LIBFLUX_MAX_REVOLUTIONS;
    config->preserve_weak_bits = true;
    config->generate_report = false;
}

int libflux_analyze_revolution(
    const uint8_t *data,
    int size,
    int bitrate,
    int encoding,
    libflux_revolution_quality_t *quality
) {
    if (!data || !quality || size <= 0) return 0;
    
    memset(quality, 0, sizeof(*quality));
    quality->total_bits = size * 8;
    
    int long_runs = 0;
    int run_length = 0;
    uint8_t last_bit = 0;
    
    for (int i = 0; i < size; i++) {
        for (int b = 7; b >= 0; b--) {
            uint8_t bit = (data[i] >> b) & 1;
            
            if (bit == last_bit) {
                run_length++;
                if (run_length > 8) long_runs++;
            } else {
                run_length = 1;
            }
            last_bit = bit;
        }
    }
    
    double error_ratio = (double)long_runs / (size * 8);
    double jitter = calculate_jitter(data, size, bitrate);
    
    quality->timing_jitter = jitter;
    quality->error_bits = long_runs;
    
    int score = 100;
    score -= (int)(error_ratio * 50);
    score -= (int)(jitter / 10);
    
    if (score < 0) score = 0;
    if (score > 100) score = 100;
    
    quality->quality_score = (uint8_t)score;
    
    (void)encoding;
    
    return score;
}

int libflux_select_best_revolution(
    uint8_t **revolutions,
    int *rev_sizes,
    int rev_count
) {
    if (!revolutions || !rev_sizes || rev_count <= 0) return -1;
    
    int best_index = 0;
    int best_score = 0;
    
    for (int i = 0; i < rev_count; i++) {
        libflux_revolution_quality_t quality;
        int score = libflux_analyze_revolution(
            revolutions[i], rev_sizes[i], 250000, 0, &quality);
        
        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }
    
    return best_index;
}

int libflux_detect_weak_bits(
    const uint8_t *rev1,
    const uint8_t *rev2,
    int size,
    uint8_t *weak_mask
) {
    if (!rev1 || !rev2 || size <= 0) return 0;
    
    int weak_count = 0;
    
    for (int i = 0; i < size; i++) {
        uint8_t diff = rev1[i] ^ rev2[i];
        
        if (weak_mask) {
            weak_mask[i] = diff;
        }
        
        weak_count += count_bits_set(diff);
    }
    
    return weak_count;
}

int libflux_fuse_revolutions(
    uint8_t **revolutions,
    int *rev_sizes,
    int *rev_bitrates,
    int rev_count,
    const libflux_fusion_config_t *config,
    libflux_fusion_result_t *result
) {
    if (!revolutions || !rev_sizes || !result || rev_count <= 0) {
        return -1;
    }
    
    libflux_fusion_config_t default_config;
    if (!config) {
        libflux_fusion_config_init(&default_config);
        config = &default_config;
    }
    
    memset(result, 0, sizeof(*result));
    
    int max_size = 0;
    for (int i = 0; i < rev_count; i++) {
        if (rev_sizes[i] > max_size) {
            max_size = rev_sizes[i];
        }
    }
    
    result->fused_data = (uint8_t *)calloc(max_size, 1);
    result->bit_confidence = (uint8_t *)calloc(max_size, 1);
    
    if (!result->fused_data || !result->bit_confidence) {
        free(result->fused_data);
        free(result->bit_confidence);
        return -1;
    }
    
    result->fused_size = max_size;
    result->fused_bitrate = rev_bitrates ? rev_bitrates[0] : 250000;
    result->rev_count = rev_count;
    result->revolutions_used = rev_count;
    
    for (int i = 0; i < rev_count && i < LIBFLUX_MAX_REVOLUTIONS; i++) {
        libflux_analyze_revolution(
            revolutions[i],
            rev_sizes[i],
            result->fused_bitrate,
            0,
            &result->rev_quality[i]
        );
        result->rev_quality[i].revolution_index = i;
    }
    
    switch (config->method) {
        case LIBFLUX_FUSION_MAJORITY:
        default:
            for (int byte = 0; byte < max_size; byte++) {
                uint8_t fused_byte = 0;
                
                for (int bit = 7; bit >= 0; bit--) {
                    int ones = 0;
                    int valid_revs = 0;
                    
                    for (int r = 0; r < rev_count; r++) {
                        if (byte < rev_sizes[r]) {
                            valid_revs++;
                            if ((revolutions[r][byte] >> bit) & 1) {
                                ones++;
                            }
                        }
                    }
                    
                    if (valid_revs > 0) {
                        if (ones > valid_revs / 2) {
                            fused_byte |= (1 << bit);
                        }
                        
                        int agreement = (ones > valid_revs / 2) ? ones : (valid_revs - ones);
                        int bit_conf = (agreement * 100) / valid_revs;
                        
                        if (agreement == valid_revs) {
                            result->bits_from_single_rev++;
                        } else {
                            result->bits_from_fusion++;
                        }
                        
                        if (bit_conf < 70) {
                            result->weak_bits_detected++;
                        }
                    }
                }
                
                result->fused_data[byte] = fused_byte;
            }
            break;
            
        case LIBFLUX_FUSION_BEST_SECTOR:
            {
                int best = libflux_select_best_revolution(revolutions, rev_sizes, rev_count);
                if (best >= 0 && best < rev_count) {
                    memcpy(result->fused_data, revolutions[best], rev_sizes[best]);
                    result->fused_size = rev_sizes[best];
                    memset(result->bit_confidence, 100, result->fused_size);
                }
            }
            break;
            
        case LIBFLUX_FUSION_WEIGHTED:
            for (int byte = 0; byte < max_size; byte++) {
                for (int bit = 7; bit >= 0; bit--) {
                    double weighted_sum = 0;
                    double total_weight = 0;
                    
                    for (int r = 0; r < rev_count; r++) {
                        if (byte < rev_sizes[r]) {
                            double weight = result->rev_quality[r].quality_score / 100.0;
                            int bit_val = (revolutions[r][byte] >> bit) & 1;
                            weighted_sum += bit_val * weight;
                            total_weight += weight;
                        }
                    }
                    
                    if (total_weight > 0) {
                        if (weighted_sum / total_weight > 0.5) {
                            result->fused_data[byte] |= (1 << bit);
                        }
                    }
                }
            }
            break;
            
        case LIBFLUX_FUSION_CONFIDENCE:
            for (int byte = 0; byte < max_size; byte++) {
                uint8_t fused_byte = 0;
                
                for (int bit = 7; bit >= 0; bit--) {
                    int best_conf = 0;
                    int best_val = 0;
                    
                    for (int r = 0; r < rev_count; r++) {
                        if (byte < rev_sizes[r]) {
                            int conf = result->rev_quality[r].quality_score;
                            if (conf > best_conf) {
                                best_conf = conf;
                                best_val = (revolutions[r][byte] >> bit) & 1;
                            }
                        }
                    }
                    
                    if (best_val) {
                        fused_byte |= (1 << bit);
                    }
                }
                
                result->fused_data[byte] = fused_byte;
            }
            break;
    }
    
    int total_quality = 0;
    for (int i = 0; i < rev_count && i < LIBFLUX_MAX_REVOLUTIONS; i++) {
        total_quality += result->rev_quality[i].quality_score;
    }
    result->overall_confidence = (uint8_t)(total_quality / rev_count);
    
    if (result->bits_from_fusion > 0) {
        double agreement_ratio = (double)result->bits_from_single_rev / 
                                 (result->bits_from_single_rev + result->bits_from_fusion);
        result->overall_confidence = (uint8_t)(result->overall_confidence * (0.7 + 0.3 * agreement_ratio));
    }
    
    return 0;
}

void libflux_fusion_result_free(libflux_fusion_result_t *result) {
    if (!result) return;
    
    free(result->fused_data);
    free(result->bit_confidence);
    
    result->fused_data = NULL;
    result->bit_confidence = NULL;
}

int libflux_fusion_export_report(
    const libflux_fusion_result_t *result,
    const char *filename,
    int format
) {
    if (!result || !filename) return -1;
    
    FILE *f = fopen(filename, "w");
    if (!f) return -1;
    
    if (format == 1) {
        fprintf(f, "{\n");
        fprintf(f, "  \"fusion_report\": {\n");
        fprintf(f, "    \"overall_confidence\": %d,\n", result->overall_confidence);
        fprintf(f, "    \"revolutions_used\": %d,\n", result->revolutions_used);
        fprintf(f, "    \"fused_size\": %d,\n", result->fused_size);
        fprintf(f, "    \"bits_from_single_rev\": %d,\n", result->bits_from_single_rev);
        fprintf(f, "    \"bits_from_fusion\": %d,\n", result->bits_from_fusion);
        fprintf(f, "    \"weak_bits_detected\": %d,\n", result->weak_bits_detected);
        fprintf(f, "    \"revolution_quality\": [\n");
        
        for (int i = 0; i < result->rev_count; i++) {
            const libflux_revolution_quality_t *q = &result->rev_quality[i];
            fprintf(f, "      {\"index\": %d, \"score\": %d, \"jitter\": %.2f}%s\n",
                    q->revolution_index, q->quality_score, q->timing_jitter,
                    (i < result->rev_count - 1) ? "," : "");
        }
        
        fprintf(f, "    ]\n");
        fprintf(f, "  }\n");
        fprintf(f, "}\n");
    } else {
        fprintf(f, "=== Multi-Revolution Fusion Report ===\n\n");
        fprintf(f, "Overall Confidence: %d%%\n", result->overall_confidence);
        fprintf(f, "Revolutions Used: %d\n", result->revolutions_used);
        fprintf(f, "Fused Size: %d bytes\n", result->fused_size);
        fprintf(f, "\nBit Statistics:\n");
        fprintf(f, "  Single Rev Agreement: %d bits\n", result->bits_from_single_rev);
        fprintf(f, "  Required Fusion: %d bits\n", result->bits_from_fusion);
        fprintf(f, "  Weak Bits Detected: %d\n", result->weak_bits_detected);
        fprintf(f, "\nPer-Revolution Quality:\n");
        
        for (int i = 0; i < result->rev_count; i++) {
            const libflux_revolution_quality_t *q = &result->rev_quality[i];
            fprintf(f, "  Rev %d: Score=%d%%, Jitter=%.2fns\n",
                    q->revolution_index, q->quality_score, q->timing_jitter);
        }
    }
    
    fclose(f);
    return 0;
}
