/**
 * @file uft_latency_tracking.c
 * @brief P0-HW-004: Per-Bit Latency Tracking Implementation
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#include "uft/hal/uft_latency_tracking.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define INITIAL_BIT_CAPACITY    1024
#define INITIAL_REGION_CAPACITY 32
#define HISTOGRAM_BUCKET_NS     10
#define HISTOGRAM_MAX_NS        10240

/*===========================================================================
 * Protection Detection Patterns
 *===========================================================================*/

typedef struct {
    uft_latency_protection_t type;
    const char *name;
    float min_density_ratio;
    float max_density_ratio;
    uint32_t min_region_bits;
    uint32_t max_region_bits;
    uint8_t min_confidence;
} protection_pattern_t;

static const protection_pattern_t protection_patterns[] = {
    { UFT_LAT_PROT_SPEEDLOCK,   "Speedlock",   0.85f, 1.25f, 500,  5000,  70 },
    { UFT_LAT_PROT_COPYLOCK,    "Copylock",    1.02f, 1.15f, 1000, 50000, 60 },
    { UFT_LAT_PROT_VMAX,        "V-MAX!",      0.90f, 1.10f, 100,  2000,  65 },
    { UFT_LAT_PROT_RAPIDLOK,    "RapidLok",    0.80f, 1.20f, 50,   500,   60 },
    { UFT_LAT_PROT_SPIRAL,      "Spiral",      0.95f, 1.05f, 5000, 50000, 55 },
    { UFT_LAT_PROT_MACRODOS,    "Macrodos",    1.00f, 1.10f, 2000, 10000, 50 },
    { UFT_LAT_PROT_FLASCHEL,    "Flaschel",    0.70f, 0.95f, 100,  1000,  60 },
};

#define NUM_PROTECTION_PATTERNS \
    (sizeof(protection_patterns) / sizeof(protection_patterns[0]))

/*===========================================================================
 * String Tables
 *===========================================================================*/

static const char *protection_names[] = {
    "None",
    "Speedlock",
    "Copylock",
    "V-MAX!",
    "RapidLok",
    "Spiral",
    "Macrodos",
    "Flaschel",
    "Generic"
};

static const char *latency_type_names[] = {
    "Normal",
    "Long",
    "Short",
    "Variable",
    "Weak",
    "Missing"
};

/*===========================================================================
 * Context Management
 *===========================================================================*/

uft_track_latency_t *uft_lat_track_create(void)
{
    uft_track_latency_t *lat = calloc(1, sizeof(uft_track_latency_t));
    if (!lat) return NULL;
    
    lat->bits = calloc(INITIAL_BIT_CAPACITY, sizeof(uft_bit_latency_t));
    if (!lat->bits) {
        free(lat);
        return NULL;
    }
    lat->bit_capacity = INITIAL_BIT_CAPACITY;
    
    lat->regions = calloc(INITIAL_REGION_CAPACITY, sizeof(uft_latency_region_t));
    if (!lat->regions) {
        free(lat->bits);
        free(lat);
        return NULL;
    }
    lat->region_capacity = INITIAL_REGION_CAPACITY;
    
    return lat;
}

void uft_lat_track_destroy(uft_track_latency_t *lat)
{
    if (!lat) return;
    free(lat->bits);
    free(lat->regions);
    free(lat);
}

void uft_lat_track_reset(uft_track_latency_t *lat)
{
    if (!lat) return;
    
    lat->cylinder = 0;
    lat->head = 0;
    lat->revolution = 0;
    lat->encoding = 0;
    lat->nominal_ns = 0;
    lat->sample_rate_mhz = 0;
    lat->total_bits = 0;
    lat->avg_latency_ns = 0;
    lat->min_latency_ns = 0xFFFF;
    lat->max_latency_ns = 0;
    lat->std_deviation_ns = 0;
    lat->anomaly_count = 0;
    lat->long_count = 0;
    lat->short_count = 0;
    lat->protection_type = UFT_LAT_PROT_NONE;
    lat->protection_confidence = 0;
    lat->protection_region_count = 0;
    lat->bit_count = 0;
    lat->region_count = 0;
    
    memset(lat->histogram, 0, sizeof(lat->histogram));
}

uft_disk_latency_t *uft_lat_disk_create(uint8_t cylinders, uint8_t heads)
{
    uft_disk_latency_t *lat = calloc(1, sizeof(uft_disk_latency_t));
    if (!lat) return NULL;
    
    lat->cylinders = cylinders;
    lat->heads = heads;
    
    size_t track_count = (size_t)cylinders * heads;
    lat->tracks = calloc(track_count, sizeof(uft_track_latency_t *));
    if (!lat->tracks) {
        free(lat);
        return NULL;
    }
    lat->track_count = track_count;
    
    return lat;
}

void uft_lat_disk_destroy(uft_disk_latency_t *lat)
{
    if (!lat) return;
    
    for (size_t i = 0; i < lat->track_count; i++) {
        uft_lat_track_destroy(lat->tracks[i]);
    }
    free(lat->tracks);
    free(lat);
}

/*===========================================================================
 * Data Recording
 *===========================================================================*/

static int ensure_bit_capacity(uft_track_latency_t *lat, size_t needed)
{
    if (needed <= lat->bit_capacity) return 0;
    
    size_t new_cap = lat->bit_capacity * 2;
    while (new_cap < needed) new_cap *= 2;
    if (new_cap > UFT_LAT_MAX_BITS) new_cap = UFT_LAT_MAX_BITS;
    
    uft_bit_latency_t *new_bits = realloc(lat->bits, 
                                          new_cap * sizeof(uft_bit_latency_t));
    if (!new_bits) return -1;
    
    lat->bits = new_bits;
    lat->bit_capacity = new_cap;
    return 0;
}

static int ensure_region_capacity(uft_track_latency_t *lat, size_t needed)
{
    if (needed <= lat->region_capacity) return 0;
    
    size_t new_cap = lat->region_capacity * 2;
    while (new_cap < needed) new_cap *= 2;
    if (new_cap > UFT_LAT_MAX_REGIONS) new_cap = UFT_LAT_MAX_REGIONS;
    
    uft_latency_region_t *new_regions = realloc(lat->regions,
                                                new_cap * sizeof(uft_latency_region_t));
    if (!new_regions) return -1;
    
    lat->regions = new_regions;
    lat->region_capacity = new_cap;
    return 0;
}

int uft_lat_record_bit(uft_track_latency_t *lat,
                       uint32_t bit_index,
                       uint16_t latency_ns,
                       uint16_t expected_ns)
{
    if (!lat) return -1;
    
    /* Calculate deviation */
    int32_t diff = (int32_t)latency_ns - (int32_t)expected_ns;
    int8_t deviation_pct = 0;
    if (expected_ns > 0) {
        int32_t pct = (diff * 100) / (int32_t)expected_ns;
        if (pct > 127) pct = 127;
        if (pct < -128) pct = -128;
        deviation_pct = (int8_t)pct;
    }
    
    /* Update histogram */
    if (latency_ns < HISTOGRAM_MAX_NS) {
        lat->histogram[latency_ns / HISTOGRAM_BUCKET_NS]++;
    }
    
    /* Update statistics */
    lat->total_bits++;
    if (latency_ns < lat->min_latency_ns) lat->min_latency_ns = latency_ns;
    if (latency_ns > lat->max_latency_ns) lat->max_latency_ns = latency_ns;
    
    /* Determine type */
    uft_latency_type_t type = UFT_LAT_TYPE_NORMAL;
    uint8_t abs_dev = (deviation_pct < 0) ? -deviation_pct : deviation_pct;
    
    if (abs_dev > UFT_LAT_ANOMALY_THRESHOLD) {
        lat->anomaly_count++;
        if (deviation_pct > 0) {
            type = UFT_LAT_TYPE_LONG;
            lat->long_count++;
        } else {
            type = UFT_LAT_TYPE_SHORT;
            lat->short_count++;
        }
        
        /* Store anomaly */
        if (ensure_bit_capacity(lat, lat->bit_count + 1) == 0) {
            uft_bit_latency_t *bit = &lat->bits[lat->bit_count++];
            bit->bit_index = bit_index;
            bit->latency_ns = latency_ns;
            bit->expected_ns = expected_ns;
            bit->deviation_pct = deviation_pct;
            bit->confidence = 255;
            bit->type = type;
            bit->flags = 0;
        }
    }
    
    return 0;
}

int uft_lat_record_flux(uft_track_latency_t *lat,
                        uint32_t sample_index,
                        uint32_t sample_count,
                        uint32_t sample_rate_hz)
{
    if (!lat || sample_rate_hz == 0) return -1;
    
    /* Convert samples to nanoseconds */
    uint64_t ns = ((uint64_t)sample_count * 1000000000ULL) / sample_rate_hz;
    if (ns > 0xFFFF) ns = 0xFFFF;
    
    return uft_lat_record_bit(lat, sample_index, (uint16_t)ns, lat->nominal_ns);
}

int uft_lat_import_flux(uft_track_latency_t *lat,
                        const uint32_t *flux_times,
                        size_t count,
                        uint32_t sample_rate_hz,
                        uint16_t nominal_ns)
{
    if (!lat || !flux_times || count == 0) return -1;
    
    uft_lat_track_reset(lat);
    lat->nominal_ns = nominal_ns;
    lat->sample_rate_mhz = (uint16_t)(sample_rate_hz / 1000000);
    
    uint32_t prev_time = 0;
    for (size_t i = 0; i < count; i++) {
        uint32_t sample_count = flux_times[i] - prev_time;
        uft_lat_record_flux(lat, (uint32_t)i, sample_count, sample_rate_hz);
        prev_time = flux_times[i];
    }
    
    return 0;
}

/*===========================================================================
 * Analysis
 *===========================================================================*/

int uft_lat_analyze_track(uft_track_latency_t *lat,
                          const uft_latency_config_t *config)
{
    if (!lat) return -1;
    
    uft_latency_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        uft_lat_get_default_config(&cfg);
    }
    
    /* Calculate average latency from histogram */
    uint64_t sum = 0;
    uint32_t total = 0;
    for (int i = 0; i < 1024; i++) {
        uint32_t count = lat->histogram[i];
        uint16_t latency = (uint16_t)(i * HISTOGRAM_BUCKET_NS + HISTOGRAM_BUCKET_NS/2);
        sum += (uint64_t)count * latency;
        total += count;
    }
    
    if (total > 0) {
        lat->avg_latency_ns = (uint16_t)(sum / total);
    }
    
    /* Calculate standard deviation */
    if (total > 1) {
        uint64_t variance_sum = 0;
        for (int i = 0; i < 1024; i++) {
            uint32_t count = lat->histogram[i];
            if (count == 0) continue;
            
            int16_t latency = (int16_t)(i * HISTOGRAM_BUCKET_NS + HISTOGRAM_BUCKET_NS/2);
            int32_t diff = latency - lat->avg_latency_ns;
            variance_sum += (uint64_t)count * diff * diff;
        }
        lat->std_deviation_ns = (uint16_t)sqrt((double)variance_sum / total);
    }
    
    /* Detect regions */
    if (lat->bit_count > 0 && cfg.detect_protection) {
        /* Group consecutive anomalies into regions */
        size_t region_start = 0;
        uint32_t last_bit = 0;
        
        for (size_t i = 0; i < lat->bit_count; i++) {
            const uft_bit_latency_t *bit = &lat->bits[i];
            
            /* Check for gap (new region) */
            if (i > 0 && (bit->bit_index - last_bit) > cfg.min_region_bits) {
                /* Close previous region if large enough */
                if (i - region_start >= 2) {
                    if (ensure_region_capacity(lat, lat->region_count + 1) == 0) {
                        uft_latency_region_t *reg = &lat->regions[lat->region_count++];
                        reg->start_bit = lat->bits[region_start].bit_index;
                        reg->end_bit = last_bit;
                        
                        /* Calculate region statistics */
                        int64_t region_sum = 0;
                        int16_t region_dev_sum = 0;
                        for (size_t j = region_start; j < i; j++) {
                            region_sum += lat->bits[j].latency_ns;
                            region_dev_sum += lat->bits[j].deviation_pct;
                        }
                        size_t region_bits = i - region_start;
                        reg->avg_latency_ns = (uint16_t)(region_sum / region_bits);
                        reg->expected_ns = lat->nominal_ns;
                        reg->deviation_pct = (int16_t)(region_dev_sum / region_bits);
                        reg->density_ratio = (float)reg->avg_latency_ns / reg->expected_ns;
                        
                        /* Determine type */
                        if (reg->deviation_pct > UFT_LAT_ANOMALY_THRESHOLD) {
                            reg->type = UFT_LAT_TYPE_LONG;
                        } else if (reg->deviation_pct < -UFT_LAT_ANOMALY_THRESHOLD) {
                            reg->type = UFT_LAT_TYPE_SHORT;
                        } else {
                            reg->type = UFT_LAT_TYPE_VARIABLE;
                        }
                    }
                }
                region_start = i;
            }
            
            last_bit = bit->bit_index;
        }
        
        /* Detect protection from regions */
        if (cfg.detect_protection) {
            uft_latency_protection_t prot;
            uint8_t conf;
            uft_lat_detect_protection(lat, &prot, &conf);
            lat->protection_type = prot;
            lat->protection_confidence = conf;
        }
    }
    
    return 0;
}

int uft_lat_detect_protection(const uft_track_latency_t *lat,
                              uft_latency_protection_t *protection,
                              uint8_t *confidence)
{
    if (!lat || !protection || !confidence) return -1;
    
    *protection = UFT_LAT_PROT_NONE;
    *confidence = 0;
    
    if (lat->region_count == 0) return 0;
    
    /* Score each protection pattern */
    uint8_t best_score = 0;
    uft_latency_protection_t best_prot = UFT_LAT_PROT_NONE;
    
    for (size_t p = 0; p < NUM_PROTECTION_PATTERNS; p++) {
        const protection_pattern_t *pat = &protection_patterns[p];
        uint32_t matches = 0;
        uint32_t total_bits = 0;
        
        for (size_t r = 0; r < lat->region_count; r++) {
            const uft_latency_region_t *reg = &lat->regions[r];
            uint32_t region_bits = reg->end_bit - reg->start_bit;
            
            /* Check if region matches pattern */
            bool density_match = (reg->density_ratio >= pat->min_density_ratio &&
                                  reg->density_ratio <= pat->max_density_ratio);
            bool size_match = (region_bits >= pat->min_region_bits &&
                               region_bits <= pat->max_region_bits);
            
            if (density_match && size_match) {
                matches++;
                total_bits += region_bits;
            }
        }
        
        /* Calculate confidence score */
        if (matches > 0) {
            uint8_t score = (uint8_t)(matches * 20);
            if (score > 100) score = 100;
            
            /* Boost for more coverage */
            if (total_bits > 1000) score = (score * 120) / 100;
            if (score > 100) score = 100;
            
            if (score > best_score && score >= pat->min_confidence) {
                best_score = score;
                best_prot = pat->type;
            }
        }
    }
    
    /* Check for generic timing anomalies if no specific match */
    if (best_prot == UFT_LAT_PROT_NONE && lat->anomaly_count > 100) {
        float anomaly_pct = (float)lat->anomaly_count / lat->total_bits * 100;
        if (anomaly_pct > 1.0f) {
            best_prot = UFT_LAT_PROT_GENERIC;
            best_score = (uint8_t)(anomaly_pct * 10);
            if (best_score > 80) best_score = 80;
        }
    }
    
    *protection = best_prot;
    *confidence = best_score;
    
    return 0;
}

int uft_lat_find_variable_regions(const uft_track_latency_t *lat,
                                  uft_latency_region_t *regions,
                                  size_t max_regions)
{
    if (!lat || !regions || max_regions == 0) return 0;
    
    size_t count = 0;
    for (size_t i = 0; i < lat->region_count && count < max_regions; i++) {
        const uft_latency_region_t *reg = &lat->regions[i];
        if (reg->type == UFT_LAT_TYPE_VARIABLE ||
            reg->type == UFT_LAT_TYPE_LONG ||
            reg->type == UFT_LAT_TYPE_SHORT) {
            regions[count++] = *reg;
        }
    }
    
    return (int)count;
}

float uft_lat_get_density_ratio(const uft_track_latency_t *lat,
                                uint32_t bit_index)
{
    if (!lat) return 1.0f;
    
    /* Find region containing bit */
    for (size_t i = 0; i < lat->region_count; i++) {
        const uft_latency_region_t *reg = &lat->regions[i];
        if (bit_index >= reg->start_bit && bit_index < reg->end_bit) {
            return reg->density_ratio;
        }
    }
    
    /* Find nearest bit latency */
    for (size_t i = 0; i < lat->bit_count; i++) {
        if (lat->bits[i].bit_index == bit_index) {
            if (lat->bits[i].expected_ns > 0) {
                return (float)lat->bits[i].latency_ns / lat->bits[i].expected_ns;
            }
        }
    }
    
    return 1.0f;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

int uft_lat_get_stats(const uft_track_latency_t *lat,
                      uint32_t start_bit, uint32_t end_bit,
                      uint16_t *avg_ns, uint16_t *std_ns,
                      uint16_t *min_ns, uint16_t *max_ns)
{
    if (!lat) return -1;
    
    uint64_t sum = 0;
    uint32_t count = 0;
    uint16_t local_min = 0xFFFF;
    uint16_t local_max = 0;
    
    for (size_t i = 0; i < lat->bit_count; i++) {
        const uft_bit_latency_t *bit = &lat->bits[i];
        if (bit->bit_index >= start_bit && bit->bit_index < end_bit) {
            sum += bit->latency_ns;
            count++;
            if (bit->latency_ns < local_min) local_min = bit->latency_ns;
            if (bit->latency_ns > local_max) local_max = bit->latency_ns;
        }
    }
    
    if (count == 0) return -1;
    
    if (avg_ns) *avg_ns = (uint16_t)(sum / count);
    if (min_ns) *min_ns = local_min;
    if (max_ns) *max_ns = local_max;
    
    /* Calculate std deviation */
    if (std_ns && count > 1) {
        uint16_t avg = (uint16_t)(sum / count);
        uint64_t var_sum = 0;
        for (size_t i = 0; i < lat->bit_count; i++) {
            const uft_bit_latency_t *bit = &lat->bits[i];
            if (bit->bit_index >= start_bit && bit->bit_index < end_bit) {
                int32_t diff = (int32_t)bit->latency_ns - avg;
                var_sum += diff * diff;
            }
        }
        *std_ns = (uint16_t)sqrt((double)var_sum / count);
    }
    
    return 0;
}

uint32_t uft_lat_histogram_get(const uft_track_latency_t *lat,
                               uint16_t latency_ns)
{
    if (!lat || latency_ns >= HISTOGRAM_MAX_NS) return 0;
    return lat->histogram[latency_ns / HISTOGRAM_BUCKET_NS];
}

uint16_t uft_lat_histogram_peak(const uft_track_latency_t *lat)
{
    if (!lat) return 0;
    
    uint32_t max_count = 0;
    int peak_bucket = 0;
    
    for (int i = 0; i < 1024; i++) {
        if (lat->histogram[i] > max_count) {
            max_count = lat->histogram[i];
            peak_bucket = i;
        }
    }
    
    return (uint16_t)(peak_bucket * HISTOGRAM_BUCKET_NS + HISTOGRAM_BUCKET_NS/2);
}

/*===========================================================================
 * Multi-Revolution
 *===========================================================================*/

int uft_lat_merge_revolutions(uft_track_latency_t *dst,
                              const uft_track_latency_t **src,
                              size_t count)
{
    if (!dst || !src || count == 0) return -1;
    
    uft_lat_track_reset(dst);
    
    /* Copy metadata from first source */
    dst->cylinder = src[0]->cylinder;
    dst->head = src[0]->head;
    dst->encoding = src[0]->encoding;
    dst->nominal_ns = src[0]->nominal_ns;
    dst->sample_rate_mhz = src[0]->sample_rate_mhz;
    
    /* Merge histograms */
    for (size_t r = 0; r < count; r++) {
        for (int i = 0; i < 1024; i++) {
            dst->histogram[i] += src[r]->histogram[i];
        }
        dst->total_bits += src[r]->total_bits;
    }
    
    /* Average statistics */
    uint64_t avg_sum = 0;
    uint16_t global_min = 0xFFFF;
    uint16_t global_max = 0;
    
    for (size_t r = 0; r < count; r++) {
        avg_sum += src[r]->avg_latency_ns;
        if (src[r]->min_latency_ns < global_min) global_min = src[r]->min_latency_ns;
        if (src[r]->max_latency_ns > global_max) global_max = src[r]->max_latency_ns;
    }
    
    dst->avg_latency_ns = (uint16_t)(avg_sum / count);
    dst->min_latency_ns = global_min;
    dst->max_latency_ns = global_max;
    
    return 0;
}

float uft_lat_revolution_variance(const uft_track_latency_t **profiles,
                                  size_t count,
                                  uint32_t bit_index)
{
    if (!profiles || count < 2) return 0.0f;
    
    /* Collect latencies for this bit across revolutions */
    float sum = 0;
    int found = 0;
    float values[16];
    
    for (size_t r = 0; r < count && r < 16; r++) {
        for (size_t i = 0; i < profiles[r]->bit_count; i++) {
            if (profiles[r]->bits[i].bit_index == bit_index) {
                values[found++] = profiles[r]->bits[i].latency_ns;
                sum += values[found-1];
                break;
            }
        }
    }
    
    if (found < 2) return 0.0f;
    
    float mean = sum / found;
    float var_sum = 0;
    for (int i = 0; i < found; i++) {
        float diff = values[i] - mean;
        var_sum += diff * diff;
    }
    
    return var_sum / found;
}

/*===========================================================================
 * Export/Report
 *===========================================================================*/

int uft_lat_export_json(const uft_track_latency_t *lat,
                        char *buffer, size_t size)
{
    if (!lat || !buffer || size == 0) return -1;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"cylinder\": %d,\n"
        "  \"head\": %d,\n"
        "  \"revolution\": %d,\n"
        "  \"total_bits\": %u,\n"
        "  \"nominal_ns\": %u,\n"
        "  \"avg_latency_ns\": %u,\n"
        "  \"min_latency_ns\": %u,\n"
        "  \"max_latency_ns\": %u,\n"
        "  \"std_deviation_ns\": %u,\n"
        "  \"anomaly_count\": %u,\n"
        "  \"protection_type\": \"%s\",\n"
        "  \"protection_confidence\": %u,\n"
        "  \"region_count\": %zu,\n"
        "  \"bit_count\": %zu\n"
        "}",
        lat->cylinder, lat->head, lat->revolution,
        lat->total_bits, lat->nominal_ns,
        lat->avg_latency_ns, lat->min_latency_ns, lat->max_latency_ns,
        lat->std_deviation_ns, lat->anomaly_count,
        uft_lat_protection_name(lat->protection_type),
        lat->protection_confidence,
        lat->region_count, lat->bit_count);
    
    return written;
}

int uft_lat_report(const uft_track_latency_t *lat,
                   char *buffer, size_t size)
{
    if (!lat || !buffer || size == 0) return -1;
    
    int written = snprintf(buffer, size,
        "Track Latency Report\n"
        "====================\n"
        "Location: Cylinder %d, Head %d, Rev %d\n"
        "Total bits: %u\n"
        "Nominal timing: %u ns\n"
        "\n"
        "Timing Statistics:\n"
        "  Average: %u ns\n"
        "  Minimum: %u ns\n"
        "  Maximum: %u ns\n"
        "  Std Dev: %u ns\n"
        "\n"
        "Anomalies:\n"
        "  Total: %u (%.2f%%)\n"
        "  Long:  %u\n"
        "  Short: %u\n"
        "\n"
        "Protection Detection:\n"
        "  Type: %s\n"
        "  Confidence: %u%%\n"
        "  Regions: %zu\n",
        lat->cylinder, lat->head, lat->revolution,
        lat->total_bits, lat->nominal_ns,
        lat->avg_latency_ns, lat->min_latency_ns, lat->max_latency_ns,
        lat->std_deviation_ns,
        lat->anomaly_count, 
        lat->total_bits > 0 ? (float)lat->anomaly_count / lat->total_bits * 100 : 0,
        lat->long_count, lat->short_count,
        uft_lat_protection_name(lat->protection_type),
        lat->protection_confidence,
        lat->region_count);
    
    return written;
}

int uft_lat_export_density_map(const uft_track_latency_t *lat,
                               float *density_map,
                               size_t count)
{
    if (!lat || !density_map || count == 0) return 0;
    
    /* Initialize to 1.0 (normal density) */
    for (size_t i = 0; i < count; i++) {
        density_map[i] = 1.0f;
    }
    
    /* Fill in from regions */
    for (size_t r = 0; r < lat->region_count; r++) {
        const uft_latency_region_t *reg = &lat->regions[r];
        for (uint32_t bit = reg->start_bit; bit < reg->end_bit && bit < count; bit++) {
            density_map[bit] = reg->density_ratio;
        }
    }
    
    return (int)(count < lat->total_bits ? count : lat->total_bits);
}

/*===========================================================================
 * Utilities
 *===========================================================================*/

const char *uft_lat_protection_name(uft_latency_protection_t prot)
{
    if (prot >= sizeof(protection_names) / sizeof(protection_names[0])) {
        return "Unknown";
    }
    return protection_names[prot];
}

const char *uft_lat_type_name(uft_latency_type_t type)
{
    if (type >= sizeof(latency_type_names) / sizeof(latency_type_names[0])) {
        return "Unknown";
    }
    return latency_type_names[type];
}

void uft_lat_get_default_config(uft_latency_config_t *config)
{
    if (!config) return;
    
    config->nominal_ns = 0;  /* Auto-detect */
    config->anomaly_threshold_pct = UFT_LAT_ANOMALY_THRESHOLD;
    config->min_region_bits = UFT_LAT_MIN_REGION_BITS;
    config->store_all_bits = false;
    config->build_histogram = true;
    config->detect_protection = true;
    config->multi_rev_average = true;
}

uint16_t uft_lat_nominal_timing(uint8_t encoding, uint8_t density)
{
    /* Density: 0=SD, 1=DD, 2=HD, 3=ED */
    switch (encoding) {
        case 1:  /* MFM */
            switch (density) {
                case 0:  return UFT_LAT_FM_DD_NS;     /* SD as FM */
                case 1:  return UFT_LAT_MFM_DD_NS;
                case 2:  return UFT_LAT_MFM_HD_NS;
                case 3:  return UFT_LAT_MFM_ED_NS;
                default: return UFT_LAT_MFM_DD_NS;
            }
        case 2:  /* FM */
            return UFT_LAT_FM_DD_NS;
        case 3:  /* GCR (C64) */
            return UFT_LAT_GCR_C64_NS;
        case 4:  /* Apple GCR */
            return UFT_LAT_GCR_APPLE_NS;
        default:
            return UFT_LAT_MFM_DD_NS;
    }
}

/*===========================================================================
 * Self-Test
 *===========================================================================*/

#ifdef UFT_LATENCY_TEST

#include <stdio.h>

int main(void)
{
    printf("UFT Latency Tracking Self-Test\n");
    printf("==============================\n\n");
    
    /* Create track profile */
    uft_track_latency_t *lat = uft_lat_track_create();
    if (!lat) {
        printf("FAIL: Could not create track latency\n");
        return 1;
    }
    
    lat->nominal_ns = 2000;  /* MFM DD */
    lat->cylinder = 35;
    lat->head = 0;
    
    /* Simulate normal bits with some anomalies */
    printf("Recording test data...\n");
    for (uint32_t i = 0; i < 50000; i++) {
        uint16_t latency = 2000;
        
        /* Add some anomalies */
        if (i >= 10000 && i < 10500) {
            latency = 2400;  /* Long region (Speedlock-like) */
        } else if (i >= 25000 && i < 25100) {
            latency = 1600;  /* Short region */
        } else if (i % 1000 == 0) {
            latency = 2200;  /* Random anomaly */
        }
        
        uft_lat_record_bit(lat, i, latency, 2000);
    }
    
    /* Analyze */
    printf("Analyzing...\n");
    uft_lat_analyze_track(lat, NULL);
    
    /* Report */
    char report[2048];
    uft_lat_report(lat, report, sizeof(report));
    printf("\n%s\n", report);
    
    /* Check results */
    printf("Tests:\n");
    printf("  Total bits: %u - %s\n", lat->total_bits, 
           lat->total_bits == 50000 ? "PASS" : "FAIL");
    printf("  Anomaly count: %u - %s\n", lat->anomaly_count,
           lat->anomaly_count > 0 ? "PASS" : "FAIL");
    printf("  Regions found: %zu - %s\n", lat->region_count,
           lat->region_count > 0 ? "PASS" : "FAIL");
    printf("  Histogram peak: %u ns - %s\n", uft_lat_histogram_peak(lat),
           uft_lat_histogram_peak(lat) >= 1990 && uft_lat_histogram_peak(lat) <= 2010 ? "PASS" : "FAIL");
    
    uft_lat_track_destroy(lat);
    
    printf("\nSelf-test complete.\n");
    return 0;
}

#endif /* UFT_LATENCY_TEST */
