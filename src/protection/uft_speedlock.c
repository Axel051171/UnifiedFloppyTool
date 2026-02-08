/**
 * @file uft_speedlock.c
 * @brief Speedlock Variable-Density Protection Handler Implementation
 * 
 * Clean-room reimplementation based on algorithm analysis.
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/protection/uft_speedlock.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

static float calc_stddev(const uint16_t *data, uint32_t count, float mean) {
    if (count < 2) return 0.0f;
    
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < count; i++) {
        double diff = (double)data[i] - mean;
        sum_sq += diff * diff;
    }
    
    return (float)sqrt(sum_sq / (count - 1));
}

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

void uft_speedlock_calc_baseline(const uint16_t *timing_data, uint32_t count,
                                  float *avg, float *stddev) {
    if (!timing_data || count == 0 || !avg) return;
    
    /* Use first N samples for baseline (should be normal region) */
    uint32_t sample_count = (count < UFT_SPEEDLOCK_SAMPLE_COUNT) ? 
                            count : UFT_SPEEDLOCK_SAMPLE_COUNT;
    
    /* Calculate average */
    uint64_t sum = 0;
    for (uint32_t i = 0; i < sample_count; i++) {
        sum += timing_data[i];
    }
    *avg = (float)sum / sample_count;
    
    /* Calculate standard deviation if requested */
    if (stddev) {
        *stddev = calc_stddev(timing_data, sample_count, *avg);
    }
}

bool uft_speedlock_quick_check(const uint16_t *timing_data, uint32_t count) {
    if (!timing_data || count < UFT_SPEEDLOCK_MIN_TRACK_BITS) return false;
    
    float baseline, stddev;
    uft_speedlock_calc_baseline(timing_data, count, &baseline, &stddev);
    
    if (baseline < 100.0f) return false; /* Invalid timing data */
    
    /* Calculate thresholds */
    float long_thresh = baseline * UFT_SPEEDLOCK_LONG_THRESHOLD / 100.0f;
    float short_thresh = baseline * UFT_SPEEDLOCK_SHORT_THRESHOLD / 100.0f;
    
    /* Count samples outside normal range */
    uint32_t long_count = 0, short_count = 0;
    
    for (uint32_t i = UFT_SPEEDLOCK_LONG_START_MIN; 
         i < count && i < UFT_SPEEDLOCK_LONG_START_MAX + 10000; i++) {
        if (timing_data[i] > long_thresh) long_count++;
        if (timing_data[i] < short_thresh) short_count++;
    }
    
    /* Speedlock has significant regions of both long and short */
    return (long_count > 1000 && short_count > 500);
}

int32_t uft_speedlock_find_region(const uint16_t *timing_data, uint32_t count,
                                   float baseline,
                                   uft_speedlock_region_type_t type,
                                   uint32_t start_pos) {
    if (!timing_data || count == 0 || baseline < 100.0f) return -1;
    
    float threshold;
    switch (type) {
        case UFT_SPEEDLOCK_REGION_LONG:
            threshold = baseline * UFT_SPEEDLOCK_LONG_THRESHOLD / 100.0f;
            break;
        case UFT_SPEEDLOCK_REGION_SHORT:
            threshold = baseline * UFT_SPEEDLOCK_SHORT_THRESHOLD / 100.0f;
            break;
        case UFT_SPEEDLOCK_REGION_NORMAL:
            threshold = baseline * UFT_SPEEDLOCK_NORMAL_THRESHOLD / 100.0f;
            break;
        default:
            return -1;
    }
    
    /* Scan with sliding window */
    uint32_t window_size = UFT_SPEEDLOCK_WINDOW_SIZE;
    uint32_t consecutive = 0;
    
    for (uint32_t i = start_pos; i < count; i++) {
        bool matches = false;
        
        switch (type) {
            case UFT_SPEEDLOCK_REGION_LONG:
                matches = (timing_data[i] > threshold);
                break;
            case UFT_SPEEDLOCK_REGION_SHORT:
                matches = (timing_data[i] < threshold);
                break;
            case UFT_SPEEDLOCK_REGION_NORMAL:
                matches = (timing_data[i] > threshold);
                break;
        }
        
        if (matches) {
            consecutive++;
            if (consecutive >= window_size) {
                return (int32_t)(i - window_size + 1);
            }
        } else {
            consecutive = 0;
        }
    }
    
    return -1;
}

float uft_speedlock_measure_ratio(const uint16_t *timing_data,
                                   uint32_t start, uint32_t end,
                                   float baseline) {
    if (!timing_data || end <= start || baseline < 100.0f) return 100.0f;
    
    uint64_t sum = 0;
    uint32_t count = end - start;
    
    for (uint32_t i = start; i < end; i++) {
        sum += timing_data[i];
    }
    
    float avg = (float)sum / count;
    return (avg / baseline) * 100.0f;
}

bool uft_speedlock_verify_sequence(const uft_speedlock_result_t *result) {
    if (!result || result->region_count < 3) return false;
    
    /* Expected sequence: NORMAL -> LONG -> SHORT -> NORMAL */
    /* Or just: LONG comes before SHORT */
    
    int32_t long_pos = -1, short_pos = -1;
    
    for (uint8_t i = 0; i < result->region_count; i++) {
        if (result->regions[i].type == UFT_SPEEDLOCK_REGION_LONG && long_pos < 0) {
            long_pos = result->regions[i].start_bit;
        }
        if (result->regions[i].type == UFT_SPEEDLOCK_REGION_SHORT && short_pos < 0) {
            short_pos = result->regions[i].start_bit;
        }
    }
    
    /* Long region should start before short region */
    if (long_pos >= 0 && short_pos >= 0) {
        return (long_pos < short_pos);
    }
    
    return false;
}

int uft_speedlock_detect(const uint8_t *track_data,
                         uint32_t track_bits,
                         const uint16_t *timing_data,
                         uint8_t track,
                         uint8_t head,
                         uft_speedlock_result_t *result) {
    if (!result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->track = track;
    result->head = head;
    result->track_bits = track_bits;
    
    /* Timing data is required for Speedlock detection */
    if (!timing_data || track_bits < UFT_SPEEDLOCK_MIN_TRACK_BITS) {
        snprintf(result->info, sizeof(result->info),
                 "Speedlock detection requires timing data and full track");
        return 0;
    }
    
    (void)track_data; /* Not used for initial detection */
    
    /* Step 1: Calculate baseline */
    uft_speedlock_calc_baseline(timing_data, track_bits,
                                 &result->baseline_avg,
                                 &result->baseline_stddev);
    result->samples_analyzed = track_bits;
    result->params.baseline_timing_ns = (uint16_t)result->baseline_avg;
    
    if (result->baseline_avg < 1000.0f || result->baseline_avg > 4000.0f) {
        snprintf(result->info, sizeof(result->info),
                 "Invalid baseline timing: %.1f ns", result->baseline_avg);
        return 0;
    }
    
    /* Step 2: Find long region */
    int32_t long_start = uft_speedlock_find_region(
        timing_data, track_bits, result->baseline_avg,
        UFT_SPEEDLOCK_REGION_LONG, UFT_SPEEDLOCK_LONG_START_MIN - 5000);
    
    if (long_start >= 0) {
        /* Find end of long region (transition to short or normal) */
        int32_t long_end = uft_speedlock_find_region(
            timing_data, track_bits, result->baseline_avg,
            UFT_SPEEDLOCK_REGION_SHORT, long_start + 1000);
        
        if (long_end < 0) {
            long_end = uft_speedlock_find_region(
                timing_data, track_bits, result->baseline_avg,
                UFT_SPEEDLOCK_REGION_NORMAL, long_start + 1000);
        }
        
        if (long_end < 0) long_end = long_start + 5000;
        
        result->regions[result->region_count].type = UFT_SPEEDLOCK_REGION_LONG;
        result->regions[result->region_count].start_bit = (uint32_t)long_start;
        result->regions[result->region_count].end_bit = (uint32_t)long_end;
        result->regions[result->region_count].length_bits = long_end - long_start;
        result->regions[result->region_count].expected_ratio = UFT_SPEEDLOCK_LONG_RATIO;
        result->regions[result->region_count].measured_ratio = 
            uft_speedlock_measure_ratio(timing_data, long_start, long_end,
                                        result->baseline_avg);
        
        float diff = fabsf(result->regions[result->region_count].measured_ratio - 
                          UFT_SPEEDLOCK_LONG_RATIO);
        result->regions[result->region_count].timing_valid = (diff < 5.0f);
        
        result->params.long_region_start = (uint32_t)long_start;
        result->params.long_region_end = (uint32_t)long_end;
        result->params.long_ratio = result->regions[result->region_count].measured_ratio;
        
        result->region_count++;
    }
    
    /* Step 3: Find short region (should be after long) */
    uint32_t search_start = (long_start >= 0) ? 
                            result->params.long_region_end : 
                            UFT_SPEEDLOCK_LONG_START_MIN;
    
    int32_t short_start = uft_speedlock_find_region(
        timing_data, track_bits, result->baseline_avg,
        UFT_SPEEDLOCK_REGION_SHORT, search_start);
    
    if (short_start >= 0) {
        /* Find end of short region */
        int32_t short_end = uft_speedlock_find_region(
            timing_data, track_bits, result->baseline_avg,
            UFT_SPEEDLOCK_REGION_NORMAL, short_start + 500);
        
        if (short_end < 0) short_end = short_start + 3000;
        
        result->regions[result->region_count].type = UFT_SPEEDLOCK_REGION_SHORT;
        result->regions[result->region_count].start_bit = (uint32_t)short_start;
        result->regions[result->region_count].end_bit = (uint32_t)short_end;
        result->regions[result->region_count].length_bits = short_end - short_start;
        result->regions[result->region_count].expected_ratio = UFT_SPEEDLOCK_SHORT_RATIO;
        result->regions[result->region_count].measured_ratio =
            uft_speedlock_measure_ratio(timing_data, short_start, short_end,
                                        result->baseline_avg);
        
        float diff = fabsf(result->regions[result->region_count].measured_ratio -
                          UFT_SPEEDLOCK_SHORT_RATIO);
        result->regions[result->region_count].timing_valid = (diff < 5.0f);
        
        result->params.short_region_start = (uint32_t)short_start;
        result->params.short_region_end = (uint32_t)short_end;
        result->params.short_ratio = result->regions[result->region_count].measured_ratio;
        
        result->region_count++;
    }
    
    /* Step 4: Find return to normal region */
    if (short_start >= 0) {
        int32_t normal_start = uft_speedlock_find_region(
            timing_data, track_bits, result->baseline_avg,
            UFT_SPEEDLOCK_REGION_NORMAL, result->params.short_region_end);
        
        if (normal_start >= 0) {
            result->regions[result->region_count].type = UFT_SPEEDLOCK_REGION_NORMAL;
            result->regions[result->region_count].start_bit = (uint32_t)normal_start;
            result->regions[result->region_count].end_bit = track_bits;
            result->regions[result->region_count].length_bits = track_bits - normal_start;
            result->regions[result->region_count].expected_ratio = UFT_SPEEDLOCK_NORMAL_RATIO;
            result->regions[result->region_count].measured_ratio = 100.0f;
            result->regions[result->region_count].timing_valid = true;
            
            result->params.normal_region_start = (uint32_t)normal_start;
            result->region_count++;
        }
    }
    
    /* Step 5: Determine detection result */
    if (result->region_count >= 2) {
        result->detected = true;
        
        /* Check for valid sequence */
        bool valid_sequence = uft_speedlock_verify_sequence(result);
        
        /* Check long region position */
        bool valid_position = (result->params.long_region_start >= 
                              UFT_SPEEDLOCK_LONG_START_MIN - 5000 &&
                              result->params.long_region_start <=
                              UFT_SPEEDLOCK_LONG_START_MAX + 5000);
        
        /* Count timing matches */
        int timing_matches = 0;
        for (uint8_t i = 0; i < result->region_count; i++) {
            if (result->regions[i].timing_valid) timing_matches++;
        }
        
        /* Determine confidence */
        if (valid_sequence && valid_position && timing_matches >= 2) {
            result->confidence = UFT_SPEEDLOCK_CONF_CERTAIN;
            result->variant = UFT_SPEEDLOCK_V1;
        } else if (valid_sequence || timing_matches >= 1) {
            result->confidence = UFT_SPEEDLOCK_CONF_LIKELY;
            result->variant = UFT_SPEEDLOCK_V1;
        } else {
            result->confidence = UFT_SPEEDLOCK_CONF_POSSIBLE;
        }
    }
    
    /* Generate info string */
    snprintf(result->info, sizeof(result->info),
             "Speedlock %s: %d regions (long=%.1f%%, short=%.1f%%), "
             "baseline=%.0fns",
             uft_speedlock_confidence_name(result->confidence),
             result->region_count,
             result->params.long_ratio,
             result->params.short_ratio,
             result->baseline_avg);
    
    return 0;
}

/*===========================================================================
 * Writing Functions
 *===========================================================================*/

int uft_speedlock_generate_timing(const uft_speedlock_params_t *params,
                                   uint32_t track_bits,
                                   uint16_t *timing_out,
                                   uint32_t timing_count) {
    if (!params || !timing_out) return -1;
    
    uint16_t base = params->baseline_timing_ns;
    if (base == 0) base = 2000; /* Default 2µs */
    
    uint16_t long_timing = (uint16_t)(base * params->long_ratio / 100.0f);
    uint16_t short_timing = (uint16_t)(base * params->short_ratio / 100.0f);
    
    for (uint32_t i = 0; i < timing_count && i < track_bits; i++) {
        if (i >= params->long_region_start && i < params->long_region_end) {
            timing_out[i] = long_timing;
        } else if (i >= params->short_region_start && i < params->short_region_end) {
            timing_out[i] = short_timing;
        } else {
            timing_out[i] = base;
        }
    }
    
    return 0;
}

int uft_speedlock_write(const uft_speedlock_recon_params_t *params,
                         uint8_t *output,
                         uint32_t *output_bits,
                         uint16_t *timing_out) {
    if (!params || !output || !output_bits) return -1;
    
    /* This would require full Amiga track encoding */
    /* For now, generate basic structure */
    
    uint32_t bit_pos = 0;
    uint32_t byte_pos = 0;
    
    memset(output, 0x4E, 16384); /* Fill with gap pattern */
    
    /* Generate 11 sectors with standard Amiga format */
    for (int sector = 0; sector < 11; sector++) {
        /* Sector header */
        output[byte_pos++] = 0x00;
        output[byte_pos++] = 0x00;
        output[byte_pos++] = 0xA1;
        output[byte_pos++] = 0xA1;
        bit_pos += 32;
        
        /* Sector data placeholder */
        memcpy(&output[byte_pos], params->sector_data[sector], 512);
        byte_pos += 512;
        bit_pos += 4096;
        
        /* Gap */
        for (int i = 0; i < 40; i++) {
            output[byte_pos++] = 0x4E;
            bit_pos += 8;
        }
    }
    
    *output_bits = bit_pos;
    
    /* Generate timing if requested */
    if (timing_out) {
        uft_speedlock_generate_timing(&params->params, bit_pos,
                                       timing_out, bit_pos);
    }
    
    return 0;
}

/*===========================================================================
 * Reporting Functions
 *===========================================================================*/

const char* uft_speedlock_variant_name(uft_speedlock_variant_t variant) {
    switch (variant) {
        case UFT_SPEEDLOCK_V1: return "Speedlock v1";
        case UFT_SPEEDLOCK_V2: return "Speedlock v2";
        case UFT_SPEEDLOCK_V3: return "Speedlock v3";
        default: return "Unknown";
    }
}

const char* uft_speedlock_confidence_name(uft_speedlock_confidence_t conf) {
    switch (conf) {
        case UFT_SPEEDLOCK_CONF_NONE: return "Not Detected";
        case UFT_SPEEDLOCK_CONF_POSSIBLE: return "Possible";
        case UFT_SPEEDLOCK_CONF_LIKELY: return "Likely";
        case UFT_SPEEDLOCK_CONF_CERTAIN: return "Certain";
        default: return "Unknown";
    }
}

const char* uft_speedlock_region_name(uft_speedlock_region_type_t type) {
    switch (type) {
        case UFT_SPEEDLOCK_REGION_NORMAL: return "Normal";
        case UFT_SPEEDLOCK_REGION_LONG: return "Long (Slow)";
        case UFT_SPEEDLOCK_REGION_SHORT: return "Short (Fast)";
        default: return "Unknown";
    }
}

size_t uft_speedlock_report(const uft_speedlock_result_t *result,
                             char *buffer, size_t buffer_size) {
    if (!result || !buffer || buffer_size == 0) return 0;
    
    size_t w = 0;
    
    w += snprintf(buffer + w, buffer_size - w,
        "=== Speedlock Analysis Report ===\n\n"
        "Detection: %s\n"
        "Variant: %s\n"
        "Confidence: %s\n\n"
        "Track: %d, Head: %d\n"
        "Track bits: %u\n\n"
        "Baseline Timing:\n"
        "  Average: %.1f ns\n"
        "  Std Dev: %.1f ns\n"
        "  Samples: %u\n\n",
        result->detected ? "YES" : "NO",
        uft_speedlock_variant_name(result->variant),
        uft_speedlock_confidence_name(result->confidence),
        result->track, result->head,
        result->track_bits,
        result->baseline_avg,
        result->baseline_stddev,
        result->samples_analyzed);
    
    if (w >= buffer_size) return buffer_size - 1;
    
    w += snprintf(buffer + w, buffer_size - w,
        "Region Analysis (%d regions):\n", result->region_count);
    
    for (uint8_t i = 0; i < result->region_count && w < buffer_size; i++) {
        const uft_speedlock_region_t *r = &result->regions[i];
        w += snprintf(buffer + w, buffer_size - w,
            "  [%d] %s: bits %u-%u (%u bits)\n"
            "      Timing: %.1f%% (expected %.1f%%) %s\n",
            i, uft_speedlock_region_name(r->type),
            r->start_bit, r->end_bit, r->length_bits,
            r->measured_ratio, r->expected_ratio,
            r->timing_valid ? "✓" : "✗");
    }
    
    if (w < buffer_size) {
        w += snprintf(buffer + w, buffer_size - w,
            "\nParameters:\n"
            "  Long region:  %u - %u (ratio: %.1f%%)\n"
            "  Short region: %u - %u (ratio: %.1f%%)\n"
            "  Normal start: %u\n",
            result->params.long_region_start,
            result->params.long_region_end,
            result->params.long_ratio,
            result->params.short_region_start,
            result->params.short_region_end,
            result->params.short_ratio,
            result->params.normal_region_start);
    }
    
    return w;
}

size_t uft_speedlock_export_json(const uft_speedlock_result_t *result,
                                  char *buffer, size_t buffer_size) {
    if (!result || !buffer || buffer_size == 0) return 0;
    
    size_t w = 0;
    
    w += snprintf(buffer + w, buffer_size - w,
        "{\n"
        "  \"protection_type\": \"Speedlock\",\n"
        "  \"detected\": %s,\n"
        "  \"variant\": \"%s\",\n"
        "  \"confidence\": \"%s\",\n"
        "  \"track\": %d,\n"
        "  \"head\": %d,\n"
        "  \"track_bits\": %u,\n"
        "  \"baseline\": {\n"
        "    \"average_ns\": %.1f,\n"
        "    \"stddev_ns\": %.1f,\n"
        "    \"samples\": %u\n"
        "  },\n"
        "  \"parameters\": {\n"
        "    \"long_region_start\": %u,\n"
        "    \"long_region_end\": %u,\n"
        "    \"long_ratio\": %.2f,\n"
        "    \"short_region_start\": %u,\n"
        "    \"short_region_end\": %u,\n"
        "    \"short_ratio\": %.2f,\n"
        "    \"normal_region_start\": %u\n"
        "  },\n",
        result->detected ? "true" : "false",
        uft_speedlock_variant_name(result->variant),
        uft_speedlock_confidence_name(result->confidence),
        result->track, result->head,
        result->track_bits,
        result->baseline_avg,
        result->baseline_stddev,
        result->samples_analyzed,
        result->params.long_region_start,
        result->params.long_region_end,
        result->params.long_ratio,
        result->params.short_region_start,
        result->params.short_region_end,
        result->params.short_ratio,
        result->params.normal_region_start);
    
    if (w >= buffer_size) return buffer_size - 1;
    
    w += snprintf(buffer + w, buffer_size - w,
        "  \"regions\": [\n");
    
    for (uint8_t i = 0; i < result->region_count && w < buffer_size; i++) {
        const uft_speedlock_region_t *r = &result->regions[i];
        w += snprintf(buffer + w, buffer_size - w,
            "    {\n"
            "      \"type\": \"%s\",\n"
            "      \"start_bit\": %u,\n"
            "      \"end_bit\": %u,\n"
            "      \"length_bits\": %u,\n"
            "      \"measured_ratio\": %.2f,\n"
            "      \"expected_ratio\": %.2f,\n"
            "      \"timing_valid\": %s\n"
            "    }%s\n",
            uft_speedlock_region_name(r->type),
            r->start_bit, r->end_bit, r->length_bits,
            r->measured_ratio, r->expected_ratio,
            r->timing_valid ? "true" : "false",
            (i < result->region_count - 1) ? "," : "");
    }
    
    if (w < buffer_size) {
        w += snprintf(buffer + w, buffer_size - w,
            "  ]\n}\n");
    }
    
    return w;
}
