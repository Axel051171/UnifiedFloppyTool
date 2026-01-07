// SPDX-License-Identifier: MIT
/*
 * weak_bits.c - Weak Bit Detection Implementation
 * 
 * THE COPY PROTECTION DETECTOR! ⭐⭐⭐
 * 
 * Analyzes multiple track reads to find bits that vary between revolutions.
 * These "weak bits" are intentionally unstable areas used for copy protection!
 * 
 * Algorithm inspired by ADF-Copy-App's "possible Weak Track" detection
 * and enhanced with statistical analysis.
 * 
 * @version 2.7.1
 * @date 2024-12-25
 */

#include "uft/uft_error.h"
#include "weak_bits.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/*============================================================================*
 * STATISTICS
 *============================================================================*/

static weak_bits_stats_t g_stats = {0};

/*============================================================================*
 * INITIALIZATION
 *============================================================================*/

int weak_bits_init(void) {
    memset(&g_stats, 0, sizeof(g_stats));
    return 0;
}

void weak_bits_shutdown(void) {
    // Nothing to clean up yet
}

/*============================================================================*
 * CORE DETECTION ALGORITHM
 *============================================================================*/

/**
 * @brief Extract a single bit from a byte array
 */
static inline uint8_t get_bit(const uint8_t *data, size_t byte_offset, uint8_t bit_pos) {
    return (data[byte_offset] >> (7 - bit_pos)) & 1;
}

/**
 * @brief Compare bit values across multiple revolutions
 * 
 * Returns: number of unique values seen (1 = stable, >1 = weak!)
 */
static uint8_t compare_bit_across_revolutions(
    const uint8_t **tracks,
    size_t track_count,
    size_t byte_offset,
    uint8_t bit_pos,
    uint8_t *samples_out
) {
    uint8_t unique_values[2] = {0xFF, 0xFF};  // Max 2 possible values (0 or 1)
    uint8_t unique_count = 0;
    
    for (size_t rev = 0; rev < track_count && rev < 10; rev++) {
        uint8_t bit_val = get_bit(tracks[rev], byte_offset, bit_pos);
        samples_out[rev] = bit_val;
        
        // Check if this is a new unique value
        bool found = false;
        for (uint8_t i = 0; i < unique_count; i++) {
            if (unique_values[i] == bit_val) {
                found = true;
                break;
            }
        }
        
        if (!found && unique_count < 2) {
            unique_values[unique_count++] = bit_val;
        }
    }
    
    return unique_count;
}

/**
 * @brief THE MAIN DETECTION ALGORITHM! ⭐
 * 
 * Compares multiple track reads bit-by-bit and identifies variations.
 * This is what finds Rob Northen, Speedlock, and other protections!
 */
int weak_bits_detect(
    const uint8_t **tracks,
    size_t track_count,
    size_t track_length,
    const weak_bit_params_t *params,
    weak_bit_result_t *result_out
) {
    if (!tracks || track_count < 2 || !track_length || !params || !result_out) {
        return -1;
    }
    
    clock_t start_time = clock();
    
    memset(result_out, 0, sizeof(*result_out));
    
    // Allocate initial weak bit array
    size_t weak_capacity = 256;
    weak_bit_t *weak_bits = malloc(weak_capacity * sizeof(weak_bit_t));
    size_t weak_count = 0;
    
    // Allocate weak byte tracking (optional)
    uint32_t *weak_bytes = NULL;
    size_t weak_byte_count = 0;
    size_t weak_byte_capacity = 0;
    
    if (params->enable_byte_level) {
        weak_byte_capacity = 128;
        weak_bytes = malloc(weak_byte_capacity * sizeof(uint32_t));
    }
    
    // Analyze each byte in the track
    for (size_t offset = 0; offset < track_length; offset++) {
        bool byte_has_variation = false;
        
        // Analyze each bit in the byte
        for (uint8_t bit = 0; bit < 8; bit++) {
            uint8_t samples[10] = {0};
            
            // Compare this bit across all revolutions
            uint8_t unique_count = compare_bit_across_revolutions(
                tracks,
                track_count,
                offset,
                bit,
                samples
            );
            
            // If we have variation (more than one unique value), it's weak!
            if (unique_count > 1) {
                // Calculate variation percentage
                uint8_t variation = (unique_count * 100) / track_count;
                
                // Check if exceeds threshold
                if (variation >= params->variation_threshold) {
                    // Expand array if needed
                    if (weak_count >= weak_capacity) {
                        weak_capacity *= 2;
                        weak_bits = realloc(weak_bits, weak_capacity * sizeof(weak_bit_t));
                    }
                    
                    // Record this weak bit! ⭐
                    weak_bit_t *wb = &weak_bits[weak_count++];
                    wb->offset = offset;
                    wb->bit_position = bit;
                    wb->variation_percent = variation;
                    wb->sample_count = unique_count;
                    memcpy(wb->samples, samples, 10);
                    
                    // Analyze pattern if requested
                    if (params->enable_pattern_analysis) {
                        wb->pattern_type = weak_bits_analyze_pattern(wb);
                        wb->has_pattern = (wb->pattern_type != 0);
                    } else {
                        wb->has_pattern = false;
                        wb->pattern_type = 0;
                    }
                    
                    byte_has_variation = true;
                }
            }
        }
        
        // Track weak bytes if enabled
        if (params->enable_byte_level && byte_has_variation) {
            if (weak_byte_count >= weak_byte_capacity) {
                weak_byte_capacity *= 2;
                weak_bytes = realloc(weak_bytes, weak_byte_capacity * sizeof(uint32_t));
            }
            weak_bytes[weak_byte_count++] = offset;
        }
    }
    
    // Fill results
    result_out->weak_bits = weak_bits;
    result_out->weak_bit_count = weak_count;
    result_out->bytes_analyzed = track_length;
    result_out->bits_analyzed = track_length * 8;
    result_out->weak_byte_count = weak_byte_count;
    result_out->weak_bytes = weak_bytes;
    
    // Calculate density (weak bits per 1000 bits)
    result_out->weak_bit_density = weak_bits_calculate_density(result_out);
    
    // Update statistics
    g_stats.tracks_analyzed++;
    g_stats.weak_bits_found += weak_count;
    if (weak_count > 0) {
        g_stats.protections_detected++;
    }
    
    clock_t end_time = clock();
    uint64_t time_ms = ((end_time - start_time) * 1000) / CLOCKS_PER_SEC;
    g_stats.total_time_ms += time_ms;
    g_stats.avg_density = (float)g_stats.weak_bits_found / (float)g_stats.tracks_analyzed;
    
    return 0;
}

/**
 * @brief Free detection results
 */
void weak_bits_free_result(weak_bit_result_t *result) {
    if (!result) return;
    
    if (result->weak_bits) {
        free(result->weak_bits);
        result->weak_bits = NULL;
    }
    
    if (result->weak_bytes) {
        free(result->weak_bytes);
        result->weak_bytes = NULL;
    }
    
    memset(result, 0, sizeof(*result));
}

/*============================================================================*
 * PATTERN ANALYSIS
 *============================================================================*/

uint8_t weak_bits_analyze_pattern(const weak_bit_t *weak_bit) {
    if (!weak_bit || weak_bit->sample_count < 2) return 0;
    
    // Check for alternating pattern (0,1,0,1,... or 1,0,1,0,...)
    bool alternating = true;
    for (uint8_t i = 1; i < weak_bit->sample_count && i < 10; i++) {
        if (weak_bit->samples[i] == weak_bit->samples[i-1]) {
            alternating = false;
            break;
        }
    }
    
    if (alternating) {
        return 1;  // Alternating pattern
    }
    
    // TODO: Check for other patterns (custom sequences, etc.)
    
    return 0;  // Random/no pattern
}

/*============================================================================*
 * DEFAULT PARAMETERS
 *============================================================================*/

void weak_bits_get_default_params(
    uint8_t format,
    weak_bit_params_t *params_out
) {
    if (!params_out) return;
    
    // Default parameters (work well for most formats)
    params_out->revolution_count = 5;
    params_out->variation_threshold = 30;  // 30% variation = weak bit
    params_out->enable_byte_level = true;
    params_out->enable_pattern_analysis = true;
    
    // Format-specific adjustments
    switch (format) {
    case 0:  // Amiga
        params_out->revolution_count = 5;
        params_out->variation_threshold = 25;
        break;
        
    case 1:  // PC
        params_out->revolution_count = 3;
        params_out->variation_threshold = 30;
        break;
        
    case 2:  // C64
        params_out->revolution_count = 5;
        params_out->variation_threshold = 25;
        break;
        
    default:
        // Use defaults
        break;
    }
}

/*============================================================================*
 * DENSITY CALCULATION
 *============================================================================*/

float weak_bits_calculate_density(const weak_bit_result_t *result) {
    if (!result || result->bits_analyzed == 0) return 0.0f;
    
    // Weak bits per 1000 bits
    return ((float)result->weak_bit_count * 1000.0f) / (float)result->bits_analyzed;
}

/*============================================================================*
 * X-COPY INTEGRATION
 *============================================================================*/

bool weak_bits_triggers_xcopy_error(
    const weak_bit_result_t *result,
    uint32_t threshold
) {
    if (!result) return false;
    
    // X-Copy Error Code 8 (Verify error) triggered if:
    // - Weak bits found above threshold
    // - OR high density (>5 per 1000 bits)
    
    return (result->weak_bit_count >= threshold) || 
           (result->weak_bit_density > 5.0f);
}

void weak_bits_format_xcopy_message(
    const weak_bit_result_t *result,
    char *message_out,
    size_t message_size
) {
    if (!result || !message_out || message_size == 0) return;
    
    if (result->weak_bit_count == 0) {
        snprintf(message_out, message_size, "No weak bits detected");
        return;
    }
    
    snprintf(message_out, message_size,
             "WEAK BITS DETECTED! %zu weak bits (%.2f per 1000 bits) - "
             "Possible copy protection (Rob Northen, Speedlock, etc.)",
             result->weak_bit_count,
             result->weak_bit_density);
}

/*============================================================================*
 * UFM INTEGRATION (PLACEHOLDER)
 *============================================================================*/

int weak_bits_add_to_ufm_track(
    void *track,
    const weak_bit_t *weak_bits,
    size_t count
) {
    // TODO: Implement UFM integration
    // Will add when UFM structures are finalized
    (void)track;
    (void)weak_bits;
    (void)count;
    return 0;
}

int weak_bits_get_from_ufm_track(
    const void *track,
    weak_bit_t **weak_bits_out,
    size_t *count_out
) {
    // TODO: Implement UFM integration
    (void)track;
    (void)weak_bits_out;
    (void)count_out;
    return -1;
}

/*============================================================================*
 * STATISTICS
 *============================================================================*/

void weak_bits_get_stats(weak_bits_stats_t *stats) {
    if (stats) {
        *stats = g_stats;
    }
}

void weak_bits_reset_stats(void) {
    memset(&g_stats, 0, sizeof(g_stats));
}

/*============================================================================*
 * UTILITIES
 *============================================================================*/

void weak_bits_print_results(const weak_bit_result_t *result) {
    if (!result) return;
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  WEAK BIT DETECTION RESULTS\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("Track Analysis:\n");
    printf("  Bytes analyzed:     %u\n", result->bytes_analyzed);
    printf("  Bits analyzed:      %u\n", result->bits_analyzed);
    printf("\n");
    printf("Weak Bits Found:      %zu 🔒\n", result->weak_bit_count);
    printf("Weak Bit Density:     %.2f per 1000 bits\n", result->weak_bit_density);
    
    if (result->weak_byte_count > 0) {
        printf("Weak Bytes:           %zu\n", result->weak_byte_count);
    }
    
    if (result->weak_bit_count > 0) {
        printf("\n");
        printf("First 10 Weak Bits:\n");
        
        size_t show_count = result->weak_bit_count < 10 ? result->weak_bit_count : 10;
        for (size_t i = 0; i < show_count; i++) {
            const weak_bit_t *wb = &result->weak_bits[i];
            printf("  [%zu] Offset: %u, Bit: %u, Variation: %u%%, Samples: ",
                   i + 1, wb->offset, wb->bit_position, wb->variation_percent);
            
            for (uint8_t j = 0; j < wb->sample_count && j < 10; j++) {
                printf("%u", wb->samples[j]);
                if (j < wb->sample_count - 1) printf(",");
            }
            
            if (wb->has_pattern) {
                printf(" (Pattern: %s)", wb->pattern_type == 1 ? "Alternating" : "Custom");
            }
            
            printf("\n");
        }
        
        if (result->weak_bit_count > 10) {
            printf("  ... and %zu more\n", result->weak_bit_count - 10);
        }
    }
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    
    if (result->weak_bit_density > 5.0f) {
        printf("\n");
        printf("🔒 COPY PROTECTION DETECTED!\n");
        printf("   High weak bit density suggests intentional protection.\n");
        printf("   Possible systems: Rob Northen, Speedlock, or similar.\n");
        printf("\n");
    }
}

int weak_bits_export_json(
    const weak_bit_result_t *result,
    char *json_out,
    size_t json_size
) {
    if (!result || !json_out || json_size == 0) return -1;
    
    int written = snprintf(json_out, json_size,
        "{\n"
        "  \"weak_bits_found\": %zu,\n"
        "  \"bytes_analyzed\": %u,\n"
        "  \"bits_analyzed\": %u,\n"
        "  \"density_per_1000\": %.2f,\n"
        "  \"weak_bytes\": %zu,\n"
        "  \"weak_bits\": [\n",
        result->weak_bit_count,
        result->bytes_analyzed,
        result->bits_analyzed,
        result->weak_bit_density,
        result->weak_byte_count
    );
    
    if (written < 0 || (size_t)written >= json_size) return -1;
    
    // Add first few weak bits as examples
    size_t show_count = result->weak_bit_count < 5 ? result->weak_bit_count : 5;
    for (size_t i = 0; i < show_count; i++) {
        const weak_bit_t *wb = &result->weak_bits[i];
        
        char samples_str[128] = "[";
        size_t samples_pos = 1;
        for (uint8_t j = 0; j < wb->sample_count && j < 10; j++) {
            char buf[16];
            int n = snprintf(buf, sizeof(buf), "%u%s", wb->samples[j],
                    j < wb->sample_count - 1 ? "," : "");
            if (samples_pos + n < sizeof(samples_str) - 1) {
                memcpy(samples_str + samples_pos, buf, n);
                samples_pos += n;
            }
        }
        samples_str[samples_pos++] = ']';
        samples_str[samples_pos] = '\0';
        
        char entry[256];
        snprintf(entry, sizeof(entry),
            "    {\"offset\": %u, \"bit\": %u, \"variation\": %u, \"samples\": %s}%s\n",
            wb->offset, wb->bit_position, wb->variation_percent,
            samples_str,
            i < show_count - 1 ? "," : ""
        );
        
        size_t cur_len = strlen(json_out);
        size_t entry_len = strlen(entry);
        if (cur_len + entry_len >= json_size) break;
        memcpy(json_out + cur_len, entry, entry_len + 1);
    }
    
    /* Safe append of closing */
    size_t cur_len = strlen(json_out);
    if (cur_len + 8 < json_size) {
        memcpy(json_out + cur_len, "  ]\n}\n", 7);
    }
    
    return 0;
}
