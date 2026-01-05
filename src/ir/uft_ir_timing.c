/**
 * @file uft_ir_timing.c
 * @brief P0-PR-002: Timing Preservation in IR Implementation
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#include "uft/ir/uft_ir_timing.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define INITIAL_ENTRY_CAPACITY  4096
#define INITIAL_REGION_CAPACITY 64

/*===========================================================================
 * Initialization
 *===========================================================================*/

void uft_timing_config_default(uft_timing_config_t *config)
{
    if (!config) return;
    
    config->preserve_all = false;
    config->preserve_anomalies = true;
    config->preserve_regions = true;
    config->preserve_sectors = true;
    config->anomaly_threshold = 15;  /* 15% deviation */
    config->resolution_ns = UFT_TIMING_RESOLUTION_NS;
    config->detect_protection = true;
}

uft_track_timing_t *uft_timing_alloc(uint16_t track, uint8_t side,
                                      uint8_t max_sectors, size_t max_entries)
{
    uft_track_timing_t *timing = calloc(1, sizeof(uft_track_timing_t));
    if (!timing) return NULL;
    
    timing->track = track;
    timing->side = side;
    
    /* Allocate sector array */
    if (max_sectors > 0) {
        timing->sectors = calloc(max_sectors, sizeof(uft_sector_timing_t));
        if (!timing->sectors) {
            free(timing);
            return NULL;
        }
    }
    
    /* Allocate entry array */
    if (max_entries > 0) {
        timing->entries = calloc(max_entries, sizeof(uft_timing_entry_compact_t));
        if (!timing->entries) {
            free(timing->sectors);
            free(timing);
            return NULL;
        }
        timing->has_detailed_timing = true;
    }
    
    /* Allocate region array */
    timing->regions = calloc(INITIAL_REGION_CAPACITY, sizeof(uft_timing_region_t));
    
    return timing;
}

void uft_timing_free(uft_track_timing_t *timing)
{
    if (!timing) return;
    
    free(timing->sectors);
    free(timing->entries);
    free(timing->regions);
    free(timing);
}

/*===========================================================================
 * Timing Recording
 *===========================================================================*/

int uft_timing_record_bit(uft_track_timing_t *timing, uint32_t bit_index,
                          uint16_t actual_ns, uint16_t expected_ns, uint8_t flags)
{
    if (!timing || !timing->entries) return -1;
    
    /* Check if anomalous enough to record */
    int8_t delta = uft_timing_delta(actual_ns, expected_ns);
    
    /* Record entry */
    if (timing->entry_count < UFT_TIMING_MAX_ENTRIES) {
        uft_timing_entry_compact_t *entry = &timing->entries[timing->entry_count++];
        entry->bit_offset = (uint16_t)(bit_index & 0xFFFF);
        entry->delta_ns = delta;
        entry->flags = flags;
    }
    
    return 0;
}

int uft_timing_record_sector(uft_track_timing_t *timing, uint8_t sector_id,
                             const uft_sector_timing_t *sector_timing)
{
    if (!timing || !timing->sectors || !sector_timing) return -1;
    
    if (timing->sector_count >= 255) return -1;  /* Max sectors reached */
    
    /* Copy sector timing */
    timing->sectors[timing->sector_count] = *sector_timing;
    timing->sectors[timing->sector_count].sector_id = sector_id;
    timing->sector_count++;
    
    return 0;
}

int uft_timing_record_region(uft_track_timing_t *timing, uint32_t start_bit,
                             uint32_t end_bit, uint8_t region_type,
                             uint16_t expected_cell_ns)
{
    if (!timing || !timing->regions) return -1;
    
    /* Ensure capacity */
    if (timing->region_count >= INITIAL_REGION_CAPACITY) return -1;
    
    uft_timing_region_t *region = &timing->regions[timing->region_count++];
    region->start_bit = start_bit;
    region->end_bit = end_bit;
    region->region_type = region_type;
    region->expected_cell_ns = expected_cell_ns;
    
    return 0;
}

size_t uft_timing_record_flux(uft_track_timing_t *timing,
                              const uint32_t *flux_ns, size_t count,
                              uint16_t expected_cell_ns,
                              const uft_timing_config_t *config)
{
    if (!timing || !flux_ns || count == 0) return 0;
    
    uft_timing_config_t default_config;
    if (!config) {
        uft_timing_config_default(&default_config);
        config = &default_config;
    }
    
    timing->nominal_cell_ns = expected_cell_ns;
    
    size_t recorded = 0;
    uint32_t bit_index = 0;
    uint64_t total_ns = 0;
    int64_t delta_sum = 0;
    uint64_t delta_sq_sum = 0;
    
    /* Current region tracking */
    uint32_t region_start = 0;
    uint8_t region_type = UFT_TIMING_FLAG_NORMAL;
    int anomaly_run = 0;
    
    for (size_t i = 0; i < count; i++) {
        uint32_t interval_ns = flux_ns[i];
        total_ns += interval_ns;
        
        /* Calculate bits in this interval */
        uint32_t bits = (interval_ns + expected_cell_ns / 2) / expected_cell_ns;
        if (bits == 0) bits = 1;
        
        /* Per-bit timing */
        uint16_t per_bit_ns = (uint16_t)(interval_ns / bits);
        
        /* Check for anomaly */
        bool is_anomaly = uft_timing_is_anomaly(per_bit_ns, expected_cell_ns,
                                                 config->anomaly_threshold);
        
        /* Determine flags */
        uint8_t flags = UFT_TIMING_FLAG_NORMAL;
        if (is_anomaly) {
            flags |= UFT_TIMING_FLAG_ANOMALY;
            anomaly_run++;
        } else {
            anomaly_run = 0;
        }
        
        /* Record if needed */
        if (config->preserve_all || (config->preserve_anomalies && is_anomaly)) {
            for (uint32_t b = 0; b < bits; b++) {
                if (uft_timing_record_bit(timing, bit_index + b,
                                          per_bit_ns, expected_cell_ns, flags) == 0) {
                    recorded++;
                }
            }
        }
        
        /* Track statistics */
        int32_t delta = (int32_t)per_bit_ns - (int32_t)expected_cell_ns;
        delta_sum += delta * bits;
        delta_sq_sum += delta * delta * bits;
        
        /* Region detection */
        if (config->preserve_regions) {
            if (is_anomaly && anomaly_run == 1) {
                /* Start new anomaly region */
                if (region_type != UFT_TIMING_FLAG_NORMAL) {
                    uft_timing_record_region(timing, region_start, bit_index,
                                             region_type, expected_cell_ns);
                }
                region_start = bit_index;
                region_type = UFT_TIMING_FLAG_ANOMALY;
            } else if (!is_anomaly && region_type == UFT_TIMING_FLAG_ANOMALY) {
                /* End anomaly region */
                uft_timing_record_region(timing, region_start, bit_index,
                                         region_type, expected_cell_ns);
                region_start = bit_index;
                region_type = UFT_TIMING_FLAG_NORMAL;
            }
        }
        
        bit_index += bits;
    }
    
    /* Finalize last region */
    if (config->preserve_regions && region_type != UFT_TIMING_FLAG_NORMAL) {
        uft_timing_record_region(timing, region_start, bit_index,
                                 region_type, expected_cell_ns);
    }
    
    /* Calculate track statistics */
    timing->revolution_ns = (uint32_t)total_ns;
    if (total_ns > 0) {
        timing->rpm_measured = (uint16_t)((60000000000ULL / total_ns) * 10);
    }
    
    if (bit_index > 0) {
        timing->mean_cell_delta = (int8_t)(delta_sum / bit_index);
        double variance = (double)delta_sq_sum / bit_index - 
                          (double)(delta_sum * delta_sum) / (bit_index * bit_index);
        timing->cell_variance = (uint8_t)(sqrt(variance) / config->resolution_ns);
    }
    
    return recorded;
}

/*===========================================================================
 * Analysis Functions
 *===========================================================================*/

void uft_timing_calculate_stats(uft_track_timing_t *timing)
{
    if (!timing) return;
    
    /* Calculate region statistics */
    for (size_t r = 0; r < timing->region_count; r++) {
        uft_timing_region_t *region = &timing->regions[r];
        
        int64_t delta_sum = 0;
        uint64_t delta_sq_sum = 0;
        uint32_t count = 0;
        uint16_t max_dev = 0;
        
        /* Scan entries in region */
        for (size_t e = 0; e < timing->entry_count; e++) {
            uint32_t bit_idx = timing->entries[e].bit_offset;
            if (bit_idx >= region->start_bit && bit_idx < region->end_bit) {
                int16_t delta = timing->entries[e].delta_ns * UFT_TIMING_RESOLUTION_NS;
                delta_sum += delta;
                delta_sq_sum += delta * delta;
                count++;
                
                uint16_t abs_delta = (delta < 0) ? -delta : delta;
                if (abs_delta > max_dev) max_dev = abs_delta;
            }
        }
        
        if (count > 0) {
            region->mean_delta_ns = (int16_t)(delta_sum / count);
            double variance = (double)delta_sq_sum / count - 
                              (double)(delta_sum * delta_sum) / (count * count);
            region->variance_ns = (uint16_t)sqrt(variance);
            region->max_deviation_ns = max_dev;
        }
    }
}

bool uft_timing_detect_protection(uft_track_timing_t *timing)
{
    if (!timing) return false;
    
    timing->timing_protection = false;
    
    /* Check for Speedlock-like patterns */
    for (size_t r = 0; r < timing->region_count; r++) {
        uft_timing_region_t *region = &timing->regions[r];
        
        if (region->region_type & UFT_TIMING_FLAG_ANOMALY) {
            uint32_t region_bits = region->end_bit - region->start_bit;
            
            /* Speedlock: significant timing deviation over 500+ bits */
            if (region_bits >= 500 && region->max_deviation_ns > 200) {
                timing->timing_protection = true;
                timing->protection_start = region->start_bit;
                timing->protection_length = region_bits;
                timing->protection_type = 1;  /* Speedlock-like */
                return true;
            }
            
            /* Long track: entire track has timing deviation */
            if (region_bits > 50000) {
                timing->timing_protection = true;
                timing->protection_start = region->start_bit;
                timing->protection_length = region_bits;
                timing->protection_type = 2;  /* Long track */
                return true;
            }
        }
    }
    
    /* Check for weak bit patterns */
    int weak_regions = 0;
    for (size_t r = 0; r < timing->region_count; r++) {
        if (timing->regions[r].region_type & UFT_TIMING_FLAG_WEAK) {
            weak_regions++;
        }
    }
    if (weak_regions >= 3) {
        timing->timing_protection = true;
        timing->protection_type = 3;  /* Weak bit */
        return true;
    }
    
    return false;
}

size_t uft_timing_find_anomalies(const uft_track_timing_t *timing,
                                 uint8_t threshold,
                                 uint32_t *positions, size_t max_count)
{
    if (!timing || !positions || max_count == 0) return 0;
    
    size_t found = 0;
    
    for (size_t i = 0; i < timing->entry_count && found < max_count; i++) {
        const uft_timing_entry_compact_t *entry = &timing->entries[i];
        
        /* Check if above threshold */
        int8_t abs_delta = (entry->delta_ns < 0) ? -entry->delta_ns : entry->delta_ns;
        int32_t deviation_ns = abs_delta * UFT_TIMING_RESOLUTION_NS;
        int32_t threshold_ns = (timing->nominal_cell_ns * threshold) / 100;
        
        if (deviation_ns > threshold_ns) {
            positions[found++] = entry->bit_offset;
        }
    }
    
    return found;
}

int uft_timing_compare_revolutions(const uft_track_timing_t **revolutions,
                                   size_t rev_count,
                                   uft_multirev_timing_t *result)
{
    if (!revolutions || rev_count == 0 || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->revolution_count = (uint8_t)rev_count;
    
    /* Allocate revolution time array */
    result->revolution_ns = calloc(rev_count, sizeof(uint32_t));
    if (!result->revolution_ns) return -1;
    
    /* Collect revolution times */
    double sum_ns = 0;
    for (size_t r = 0; r < rev_count; r++) {
        result->revolution_ns[r] = revolutions[r]->revolution_ns;
        sum_ns += result->revolution_ns[r];
    }
    
    /* Calculate mean */
    result->mean_rev_ns = sum_ns / rev_count;
    
    /* Calculate standard deviation */
    double var_sum = 0;
    for (size_t r = 0; r < rev_count; r++) {
        double diff = result->revolution_ns[r] - result->mean_rev_ns;
        var_sum += diff * diff;
    }
    result->std_rev_ns = sqrt(var_sum / rev_count);
    
    /* Calculate jitter percentage */
    if (result->mean_rev_ns > 0) {
        result->jitter_pct = (result->std_rev_ns / result->mean_rev_ns) * 100.0;
    }
    
    return 0;
}

/*===========================================================================
 * Serialization
 *===========================================================================*/

/* Binary format:
 * [4] magic "UTIM"
 * [2] version
 * [2] track
 * [1] side
 * [1] flags
 * [2] sector_count
 * [4] entry_count
 * [4] region_count
 * [4] revolution_ns
 * [2] nominal_cell_ns
 * [N] sector data
 * [N] entry data
 * [N] region data
 */

#define TIMING_MAGIC 0x4D495455  /* "UTIM" */

size_t uft_timing_serialize(const uft_track_timing_t *timing,
                            uint8_t *buffer, size_t buffer_size)
{
    if (!timing) return 0;
    
    /* Calculate required size */
    size_t header_size = 24;
    size_t sector_size = timing->sector_count * sizeof(uft_sector_timing_t);
    size_t entry_size = timing->entry_count * sizeof(uft_timing_entry_compact_t);
    size_t region_size = timing->region_count * sizeof(uft_timing_region_t);
    size_t total_size = header_size + sector_size + entry_size + region_size;
    
    if (!buffer || buffer_size < total_size) {
        return total_size;  /* Return required size */
    }
    
    /* Write header */
    uint8_t *p = buffer;
    
    *(uint32_t *)p = TIMING_MAGIC; p += 4;
    *(uint16_t *)p = 0x0100; p += 2;  /* Version 1.0 */
    *(uint16_t *)p = timing->track; p += 2;
    *p++ = timing->side;
    *p++ = timing->flags;
    *(uint16_t *)p = timing->sector_count; p += 2;
    *(uint32_t *)p = (uint32_t)timing->entry_count; p += 4;
    *(uint32_t *)p = (uint32_t)timing->region_count; p += 4;
    *(uint32_t *)p = timing->revolution_ns; p += 4;
    *(uint16_t *)p = timing->nominal_cell_ns; p += 2;
    
    /* Write sector data */
    if (timing->sector_count > 0 && timing->sectors) {
        memcpy(p, timing->sectors, sector_size);
        p += sector_size;
    }
    
    /* Write entry data */
    if (timing->entry_count > 0 && timing->entries) {
        memcpy(p, timing->entries, entry_size);
        p += entry_size;
    }
    
    /* Write region data */
    if (timing->region_count > 0 && timing->regions) {
        memcpy(p, timing->regions, region_size);
        p += region_size;
    }
    
    return total_size;
}

int uft_timing_deserialize(const uint8_t *buffer, size_t buffer_size,
                           uft_track_timing_t **timing_out)
{
    if (!buffer || buffer_size < 24 || !timing_out) return -1;
    
    const uint8_t *p = buffer;
    
    /* Check magic */
    if (*(uint32_t *)p != TIMING_MAGIC) return -1;
    p += 4;
    
    /* Check version */
    uint16_t version = *(uint16_t *)p; p += 2;
    if ((version >> 8) > 1) return -1;  /* Unsupported major version */
    
    /* Read header */
    uint16_t track = *(uint16_t *)p; p += 2;
    uint8_t side = *p++;
    uint8_t flags = *p++;
    uint16_t sector_count = *(uint16_t *)p; p += 2;
    uint32_t entry_count = *(uint32_t *)p; p += 4;
    uint32_t region_count = *(uint32_t *)p; p += 4;
    uint32_t revolution_ns = *(uint32_t *)p; p += 4;
    uint16_t nominal_cell_ns = *(uint16_t *)p; p += 2;
    
    /* Validate sizes */
    size_t data_size = sector_count * sizeof(uft_sector_timing_t) +
                       entry_count * sizeof(uft_timing_entry_compact_t) +
                       region_count * sizeof(uft_timing_region_t);
    if (buffer_size < 24 + data_size) return -1;
    
    /* Allocate timing structure */
    uft_track_timing_t *timing = uft_timing_alloc(track, side, 
                                                   (uint8_t)sector_count, entry_count);
    if (!timing) return -1;
    
    timing->flags = flags;
    timing->revolution_ns = revolution_ns;
    timing->nominal_cell_ns = nominal_cell_ns;
    
    /* Read sector data */
    if (sector_count > 0) {
        memcpy(timing->sectors, p, sector_count * sizeof(uft_sector_timing_t));
        p += sector_count * sizeof(uft_sector_timing_t);
        timing->sector_count = (uint8_t)sector_count;
    }
    
    /* Read entry data */
    if (entry_count > 0) {
        memcpy(timing->entries, p, entry_count * sizeof(uft_timing_entry_compact_t));
        p += entry_count * sizeof(uft_timing_entry_compact_t);
        timing->entry_count = entry_count;
    }
    
    /* Read region data */
    if (region_count > 0 && timing->regions) {
        memcpy(timing->regions, p, region_count * sizeof(uft_timing_region_t));
        timing->region_count = region_count;
    }
    
    *timing_out = timing;
    return 0;
}

int uft_timing_export_json(const uft_track_timing_t *timing,
                           const char *path, bool include_entries)
{
    if (!timing || !path) return -1;
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"track\": %u,\n", timing->track);
    fprintf(f, "  \"side\": %u,\n", timing->side);
    fprintf(f, "  \"revolution_ns\": %u,\n", timing->revolution_ns);
    fprintf(f, "  \"rpm_measured\": %.1f,\n", timing->rpm_measured / 10.0);
    fprintf(f, "  \"nominal_cell_ns\": %u,\n", timing->nominal_cell_ns);
    fprintf(f, "  \"mean_cell_delta\": %d,\n", timing->mean_cell_delta);
    fprintf(f, "  \"timing_protection\": %s,\n", 
            timing->timing_protection ? "true" : "false");
    
    /* Sectors */
    fprintf(f, "  \"sectors\": [\n");
    for (size_t i = 0; i < timing->sector_count; i++) {
        const uft_sector_timing_t *s = &timing->sectors[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"id\": %u,\n", s->sector_id);
        fprintf(f, "      \"pre_gap_bits\": %u,\n", s->pre_gap_bits);
        fprintf(f, "      \"sync_bits\": %u,\n", s->sync_bits);
        fprintf(f, "      \"data_bits\": %u,\n", s->data_bits);
        fprintf(f, "      \"anomaly_count\": %u\n", s->anomaly_count);
        fprintf(f, "    }%s\n", i < timing->sector_count - 1 ? "," : "");
    }
    fprintf(f, "  ],\n");
    
    /* Regions */
    fprintf(f, "  \"regions\": [\n");
    for (size_t i = 0; i < timing->region_count; i++) {
        const uft_timing_region_t *r = &timing->regions[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"start_bit\": %u,\n", r->start_bit);
        fprintf(f, "      \"end_bit\": %u,\n", r->end_bit);
        fprintf(f, "      \"type\": %u,\n", r->region_type);
        fprintf(f, "      \"mean_delta_ns\": %d,\n", r->mean_delta_ns);
        fprintf(f, "      \"variance_ns\": %u\n", r->variance_ns);
        fprintf(f, "    }%s\n", i < timing->region_count - 1 ? "," : "");
    }
    fprintf(f, "  ]");
    
    /* Detailed entries (optional) */
    if (include_entries && timing->entry_count > 0) {
        fprintf(f, ",\n  \"entries\": [\n");
        for (size_t i = 0; i < timing->entry_count && i < 1000; i++) {
            const uft_timing_entry_compact_t *e = &timing->entries[i];
            fprintf(f, "    [%u, %d, %u]%s\n", 
                    e->bit_offset, e->delta_ns, e->flags,
                    i < timing->entry_count - 1 && i < 999 ? "," : "");
        }
        if (timing->entry_count > 1000) {
            fprintf(f, "    /* ... %zu more entries truncated ... */\n",
                    timing->entry_count - 1000);
        }
        fprintf(f, "  ]");
    }
    
    fprintf(f, "\n}\n");
    fclose(f);
    
    return 0;
}

/*===========================================================================
 * Self-Test
 *===========================================================================*/

#ifdef UFT_TIMING_TEST

#include <stdio.h>

int main(void)
{
    printf("UFT IR Timing Self-Test\n");
    printf("=======================\n\n");
    
    /* Create timing structure */
    uft_track_timing_t *timing = uft_timing_alloc(35, 0, 9, 10000);
    if (!timing) {
        printf("FAIL: Could not allocate timing\n");
        return 1;
    }
    printf("Created timing structure\n");
    
    /* Simulate flux data with some anomalies */
    uint32_t flux_ns[5000];
    for (int i = 0; i < 5000; i++) {
        flux_ns[i] = 2000;  /* Nominal MFM DD */
        
        /* Add anomaly region (500-600) */
        if (i >= 500 && i < 600) {
            flux_ns[i] = 2400;  /* +20% */
        }
        /* Add random jitter */
        if (i % 100 == 0) {
            flux_ns[i] = 2100;
        }
    }
    
    /* Record flux */
    uft_timing_config_t config;
    uft_timing_config_default(&config);
    
    size_t recorded = uft_timing_record_flux(timing, flux_ns, 5000, 2000, &config);
    printf("Recorded %zu timing entries\n", recorded);
    printf("Region count: %zu\n", timing->region_count);
    
    /* Calculate statistics */
    uft_timing_calculate_stats(timing);
    printf("Mean cell delta: %d ns\n", timing->mean_cell_delta);
    
    /* Detect protection */
    bool has_prot = uft_timing_detect_protection(timing);
    printf("Protection detected: %s\n", has_prot ? "yes" : "no");
    
    /* Find anomalies */
    uint32_t anomalies[100];
    size_t anom_count = uft_timing_find_anomalies(timing, 15, anomalies, 100);
    printf("Anomalies found: %zu\n", anom_count);
    
    /* Test serialization */
    printf("\nSerialization test:\n");
    size_t ser_size = uft_timing_serialize(timing, NULL, 0);
    printf("  Required size: %zu bytes\n", ser_size);
    
    uint8_t *buffer = malloc(ser_size);
    if (buffer) {
        size_t written = uft_timing_serialize(timing, buffer, ser_size);
        printf("  Written: %zu bytes\n", written);
        
        /* Deserialize */
        uft_track_timing_t *timing2 = NULL;
        int err = uft_timing_deserialize(buffer, ser_size, &timing2);
        printf("  Deserialize: %s\n", err == 0 ? "PASS" : "FAIL");
        
        if (timing2) {
            printf("  Track: %d, Side: %d\n", timing2->track, timing2->side);
            printf("  Entries: %zu\n", timing2->entry_count);
            uft_timing_free(timing2);
        }
        
        free(buffer);
    }
    
    /* Test JSON export */
    printf("\nJSON export test:\n");
    int json_err = uft_timing_export_json(timing, "/tmp/timing_test.json", false);
    printf("  Export: %s\n", json_err == 0 ? "PASS" : "FAIL");
    
    uft_timing_free(timing);
    
    printf("\nSelf-test complete.\n");
    return 0;
}

#endif /* UFT_TIMING_TEST */
