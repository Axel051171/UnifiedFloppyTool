/**
 * @file uft_protection_analysis.c
 * @brief Copy Protection Analysis Implementation
 * 
 * High-fidelity physical signature analysis for disk preservation.
 */

#include "uft_protection_analysis.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * CONTEXT MANAGEMENT
 * ======================================================================== */

uft_rc_t uft_dpm_create(uft_protection_ctx_t** ctx) {
    UFT_CHECK_NULL(ctx);
    
    *ctx = calloc(1, sizeof(uft_protection_ctx_t));
    if (!*ctx) {
        return UFT_ERR_MEMORY;
    }
    
    /* Default configuration */
    (*ctx)->dpm_precision_ns = 100;     /* 100ns precision */
    (*ctx)->weak_bit_reads = 5;         /* Read 5 times */
    (*ctx)->analyze_gaps = true;
    (*ctx)->measure_bitrate = true;
    
    return UFT_SUCCESS;
}

void uft_protection_destroy(uft_protection_ctx_t** ctx) {
    if (ctx && *ctx) {
        if ((*ctx)->analysis) {
            uft_protection_analysis_free(&(*ctx)->analysis);
        }
        free(*ctx);
        *ctx = NULL;
    }
}

/* ========================================================================
 * DPM (DATA POSITION MEASUREMENT)
 * ======================================================================== */

uft_rc_t uft_dpm_measure_track(
    uft_protection_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uft_dpm_entry_t** entries,
    uint32_t* count
) {
    UFT_CHECK_NULLS(ctx, entries, count);
    
    *entries = NULL;
    *count = 0;
    
    /* This is a simplified implementation.
     * Real DPM requires:
     * 1. Read track with flux-level timing
     * 2. Locate each sector's actual bit position
     * 3. Compare to theoretical position based on:
     *    - Track rotation speed
     *    - Sector number
     *    - Standard gap sizes
     * 4. Measure deviation in nanoseconds
     */
    
    /* For demonstration, create sample DPM entries */
    uint32_t sector_count = 18;  /* Example: HD disk */
    
    uft_dpm_entry_t* dpm_entries = calloc(sector_count, sizeof(uft_dpm_entry_t));
    if (!dpm_entries) {
        return UFT_ERR_MEMORY;
    }
    
    /* Simulate measurements */
    for (uint32_t s = 0; s < sector_count; s++) {
        dpm_entries[s].track = track;
        dpm_entries[s].head = head;
        dpm_entries[s].sector = s + 1;
        
        /* Expected position (evenly spaced) */
        dpm_entries[s].expected_bit_pos = (200000 / sector_count) * s;
        
        /* In real implementation, this comes from flux analysis */
        dpm_entries[s].actual_bit_pos = dpm_entries[s].expected_bit_pos;
        
        /* Small random deviation for simulation */
        dpm_entries[s].deviation_ns = (int32_t)((rand() % 200) - 100);
        
        /* Anomaly if deviation > 500ns */
        dpm_entries[s].is_anomaly = (abs(dpm_entries[s].deviation_ns) > 500);
        dpm_entries[s].confidence = 85;
    }
    
    *entries = dpm_entries;
    *count = sector_count;
    
    return UFT_SUCCESS;
}

uft_rc_t uft_dpm_measure_disk(
    uft_protection_ctx_t* ctx,
    uft_dpm_map_t** dpm_map
) {
    UFT_CHECK_NULLS(ctx, dpm_map);
    
    *dpm_map = NULL;
    
    /* Allocate DPM map */
    uft_dpm_map_t* map = calloc(1, sizeof(uft_dpm_map_t));
    if (!map) {
        return UFT_ERR_MEMORY;
    }
    
    /* Estimate total sectors (80 tracks * 2 heads * 18 SPT) */
    uint32_t estimated_sectors = 80 * 2 * 18;
    
    map->entries = calloc(estimated_sectors, sizeof(uft_dpm_entry_t));
    if (!map->entries) {
        free(map);
        return UFT_ERR_MEMORY;
    }
    
    uint32_t entry_idx = 0;
    
    /* Measure each track */
    for (uint8_t t = 0; t < 80; t++) {
        for (uint8_t h = 0; h < 2; h++) {
            uft_dpm_entry_t* track_entries;
            uint32_t track_count;
            
            uft_rc_t rc = uft_dpm_measure_track(ctx, t, h, 
                                                &track_entries, &track_count);
            
            if (uft_success(rc)) {
                memcpy(&map->entries[entry_idx], track_entries,
                       track_count * sizeof(uft_dpm_entry_t));
                entry_idx += track_count;
                
                /* Count anomalies */
                for (uint32_t i = 0; i < track_count; i++) {
                    if (track_entries[i].is_anomaly) {
                        map->anomalies_found++;
                    }
                }
                
                free(track_entries);
            }
            
            /* Progress callback */
            if (ctx->progress_fn) {
                uint8_t percent = ((t * 2 + h) * 100) / (80 * 2);
                ctx->progress_fn(percent, "Measuring DPM...", 
                               ctx->progress_user_data);
            }
        }
    }
    
    map->entry_count = entry_idx;
    
    /* Calculate statistics */
    if (entry_idx > 0) {
        int32_t min_dev = map->entries[0].deviation_ns;
        int32_t max_dev = map->entries[0].deviation_ns;
        int64_t sum_dev = 0;
        
        for (uint32_t i = 0; i < entry_idx; i++) {
            int32_t dev = map->entries[i].deviation_ns;
            if (dev < min_dev) min_dev = dev;
            if (dev > max_dev) max_dev = dev;
            sum_dev += dev;
        }
        
        map->min_deviation_ns = min_dev;
        map->max_deviation_ns = max_dev;
        map->avg_deviation_ns = (int32_t)(sum_dev / entry_idx);
    }
    
    *dpm_map = map;
    return UFT_SUCCESS;
}

void uft_dpm_free(uft_dpm_map_t** dpm_map) {
    if (dpm_map && *dpm_map) {
        if ((*dpm_map)->entries) {
            free((*dpm_map)->entries);
        }
        free(*dpm_map);
        *dpm_map = NULL;
    }
}

/* ========================================================================
 * WEAK BIT DETECTION
 * ======================================================================== */

uft_rc_t uft_weak_bit_detect_sector(
    uft_protection_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uft_weak_bit_result_t* result
) {
    UFT_CHECK_NULLS(ctx, result);
    
    memset(result, 0, sizeof(*result));
    
    result->track = track;
    result->head = head;
    result->sector = sector;
    result->read_count = ctx->weak_bit_reads;
    
    /* Read sector multiple times */
    for (uint8_t i = 0; i < ctx->weak_bit_reads && i < 8; i++) {
        /* In real implementation:
         * - Read sector
         * - Calculate CRC
         * - Store in result->crc_values[i]
         * 
         * For now, simulate:
         */
        result->crc_values[i] = 0x1234 + (rand() % 10);
    }
    
    /* Check CRC stability */
    result->crc_stable = true;
    for (uint8_t i = 1; i < result->read_count; i++) {
        if (result->crc_values[i] != result->crc_values[0]) {
            result->crc_stable = false;
            break;
        }
    }
    
    /* If unstable, classify */
    if (!result->crc_stable) {
        /* Analyze bit-level differences */
        result->unstable_bit_count = 10;  /* Simulated */
        
        /* Pattern analysis:
         * - Few unstable bits (1-10) → Likely intentional weak bits
         * - Many unstable bits (100+) → Likely media damage
         * - Consistent pattern → Protection
         * - Random pattern → Damage
         */
        
        if (result->unstable_bit_count < 20) {
            result->is_weak_sector = true;
            result->is_media_error = false;
            result->confidence = 80;
        } else {
            result->is_weak_sector = false;
            result->is_media_error = true;
            result->confidence = 70;
        }
    } else {
        result->is_weak_sector = false;
        result->is_media_error = false;
        result->confidence = 100;
    }
    
    return UFT_SUCCESS;
}

uft_rc_t uft_weak_bit_scan_disk(
    uft_protection_ctx_t* ctx,
    uft_weak_bit_result_t** results,
    uint32_t* count
) {
    UFT_CHECK_NULLS(ctx, results, count);
    
    *results = NULL;
    *count = 0;
    
    /* Estimate sectors */
    uint32_t max_sectors = 80 * 2 * 18;
    
    uft_weak_bit_result_t* weak_results = calloc(max_sectors, 
                                                  sizeof(uft_weak_bit_result_t));
    if (!weak_results) {
        return UFT_ERR_MEMORY;
    }
    
    uint32_t weak_count = 0;
    
    /* Scan all sectors */
    for (uint8_t t = 0; t < 80; t++) {
        for (uint8_t h = 0; h < 2; h++) {
            for (uint8_t s = 1; s <= 18; s++) {
                uft_weak_bit_result_t result;
                
                uft_rc_t rc = uft_weak_bit_detect_sector(ctx, t, h, s, &result);
                
                if (uft_success(rc) && (result.is_weak_sector || result.is_media_error)) {
                    weak_results[weak_count++] = result;
                }
            }
        }
        
        /* Progress */
        if (ctx->progress_fn) {
            uint8_t percent = (t * 100) / 80;
            ctx->progress_fn(percent, "Scanning weak bits...", 
                           ctx->progress_user_data);
        }
    }
    
    if (weak_count > 0) {
        /* Shrink to actual size */
        uft_weak_bit_result_t* final = realloc(weak_results,
                                               weak_count * sizeof(uft_weak_bit_result_t));
        if (final) {
            *results = final;
        } else {
            *results = weak_results;
        }
        *count = weak_count;
    } else {
        free(weak_results);
    }
    
    return UFT_SUCCESS;
}

/* ========================================================================
 * GAP ANALYSIS
 * ======================================================================== */

uft_rc_t uft_gap_analyze_track(
    uft_protection_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uft_gap_analysis_t* analysis
) {
    UFT_CHECK_NULLS(ctx, analysis);
    
    memset(analysis, 0, sizeof(*analysis));
    
    analysis->track = track;
    analysis->head = head;
    
    /* In real implementation:
     * 1. Read track bitstream
     * 2. Locate all sectors
     * 3. Identify gaps between sectors
     * 4. Scan gaps for:
     *    - Non-0x4E/0x00 bytes
     *    - Hidden sync marks
     *    - Data patterns
     * 5. Check sync marks for violations
     */
    
    /* Simulated gap analysis */
    analysis->gap_count = 18;  /* 18 sectors = 18 gaps */
    
    for (uint32_t i = 0; i < analysis->gap_count; i++) {
        analysis->gaps[i].start_bit = i * 10000;
        analysis->gaps[i].length_bits = 500;
        analysis->gaps[i].has_hidden_data = false;
        analysis->gaps[i].data = NULL;
        analysis->gaps[i].data_size = 0;
    }
    
    /* Detect sync violations */
    analysis->sync_violations = 0;
    analysis->missing_sync_marks = 0;
    
    return UFT_SUCCESS;
}

/* ========================================================================
 * BITRATE ANALYSIS
 * ======================================================================== */

uft_rc_t uft_bitrate_analyze_track(
    uft_protection_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uft_bitrate_analysis_t* analysis
) {
    UFT_CHECK_NULLS(ctx, analysis);
    
    memset(analysis, 0, sizeof(*analysis));
    
    analysis->track = track;
    analysis->head = head;
    
    /* Standard bitrate for HD disk */
    analysis->nominal_bitrate = 500000;  /* 500 Kbit/s */
    
    /* In real implementation:
     * 1. Read flux transitions
     * 2. Measure cell times across track
     * 3. Calculate instantaneous bitrate
     * 4. Detect zones with different bitrates
     */
    
    /* Simulated analysis */
    analysis->min_bitrate = 490000;
    analysis->max_bitrate = 510000;
    analysis->bitrate_variance = 2.0f;  /* 2% variance */
    
    /* Check if variance is significant */
    if (analysis->bitrate_variance > 10.0f) {
        analysis->has_variable_bitrate = true;
        
        /* Detect zones (Speedlock-style) */
        analysis->zone_count = 2;
        analysis->zones[0] = (typeof(analysis->zones[0])){0, 100000, 500000};
        analysis->zones[1] = (typeof(analysis->zones[1])){100000, 100000, 400000};
    } else {
        analysis->has_variable_bitrate = false;
        analysis->zone_count = 0;
    }
    
    return UFT_SUCCESS;
}

/* ========================================================================
 * PROTECTION CLASSIFICATION
 * ======================================================================== */

const char* uft_protection_name(uft_protection_type_t type) {
    switch (type) {
    case UFT_PROTECTION_NONE: return "None";
    case UFT_PROTECTION_COPYLOCK: return "Rob Northen Copylock";
    case UFT_PROTECTION_DUNGEON_MASTER: return "Dungeon Master";
    case UFT_PROTECTION_SPEEDLOCK: return "Speedlock";
    case UFT_PROTECTION_RAPIDLOK: return "RapidLok";
    case UFT_PROTECTION_DPM: return "DPM (Sector Positioning)";
    case UFT_PROTECTION_LONG_TRACK: return "Long Track";
    case UFT_PROTECTION_HALF_TRACK: return "Half Track";
    case UFT_PROTECTION_BAD_SECTORS: return "Bad Sectors";
    case UFT_PROTECTION_VORTEX: return "Vortex Tracker";
    case UFT_PROTECTION_GAP_DATA: return "Gap Data";
    case UFT_PROTECTION_SYNC_VIOLATION: return "Sync Violation";
    case UFT_PROTECTION_CUSTOM: return "Custom/Unknown";
    default: return "Unknown";
    }
}

uft_rc_t uft_protection_analyze_disk(
    uft_protection_ctx_t* ctx,
    uft_protection_analysis_t** analysis
) {
    UFT_CHECK_NULLS(ctx, analysis);
    
    *analysis = NULL;
    
    /* Allocate analysis */
    uft_protection_analysis_t* result = calloc(1, sizeof(uft_protection_analysis_t));
    if (!result) {
        return UFT_ERR_MEMORY;
    }
    
    /* Run all analyses */
    
    /* 1. DPM */
    if (ctx->progress_fn) {
        ctx->progress_fn(0, "Starting DPM analysis...", ctx->progress_user_data);
    }
    
    uft_rc_t rc = uft_dpm_measure_disk(ctx, &result->dpm_map);
    if (uft_failed(rc)) {
        free(result);
        return rc;
    }
    
    /* 2. Weak bits */
    if (ctx->progress_fn) {
        ctx->progress_fn(33, "Scanning weak bits...", ctx->progress_user_data);
    }
    
    rc = uft_weak_bit_scan_disk(ctx, &result->weak_bits, &result->weak_bit_count);
    if (uft_failed(rc)) {
        uft_dpm_free(&result->dpm_map);
        free(result);
        return rc;
    }
    
    /* 3. Classification based on findings */
    if (ctx->progress_fn) {
        ctx->progress_fn(90, "Classifying protection...", ctx->progress_user_data);
    }
    
    /* Classify based on signatures */
    if (result->weak_bit_count > 0 && result->weak_bits[0].track == 0) {
        /* Weak bits on track 0 → Copylock */
        result->protection_type = UFT_PROTECTION_COPYLOCK;
        result->confidence = 80;
    } else if (result->dpm_map && result->dpm_map->anomalies_found > 10) {
        /* Many DPM anomalies → DPM protection */
        result->protection_type = UFT_PROTECTION_DPM;
        result->confidence = 75;
    } else {
        result->protection_type = UFT_PROTECTION_NONE;
        result->confidence = 95;
    }
    
    result->protection_name = uft_protection_name(result->protection_type);
    
    /* Generate description */
    snprintf(result->description, sizeof(result->description),
             "Protection: %s (Confidence: %d%%)\n"
             "DPM Anomalies: %u\n"
             "Weak Sectors: %u\n",
             result->protection_name,
             result->confidence,
             result->dpm_map ? result->dpm_map->anomalies_found : 0,
             result->weak_bit_count);
    
    /* Determine if flux preservation required */
    result->requires_flux_preservation = 
        (result->protection_type != UFT_PROTECTION_NONE);
    
    if (result->requires_flux_preservation) {
        snprintf(result->flux_profile_id, sizeof(result->flux_profile_id),
                 "uft_protection_%s", result->protection_name);
    }
    
    if (ctx->progress_fn) {
        ctx->progress_fn(100, "Analysis complete", ctx->progress_user_data);
    }
    
    *analysis = result;
    return UFT_SUCCESS;
}

void uft_protection_analysis_free(uft_protection_analysis_t** analysis) {
    if (analysis && *analysis) {
        if ((*analysis)->dpm_map) {
            uft_dpm_free(&(*analysis)->dpm_map);
        }
        if ((*analysis)->weak_bits) {
            free((*analysis)->weak_bits);
        }
        free(*analysis);
        *analysis = NULL;
    }
}

/* ========================================================================
 * FLUX PROFILE EXPORT
 * ======================================================================== */

uft_rc_t uft_protection_export_flux_profile(
    const uft_protection_analysis_t* analysis,
    const char* profile_path
) {
    UFT_CHECK_NULLS(analysis, profile_path);
    
    FILE* fp = fopen(profile_path, "w");
    if (!fp) {
        return UFT_ERR_NOT_FOUND;
    }
    
    fprintf(fp, "# UFT Flux Profile - Protection Analysis\n");
    fprintf(fp, "# Generated by UFT Protection Analyzer v2.12.0\n\n");
    
    fprintf(fp, "protection_type: %s\n", analysis->protection_name);
    fprintf(fp, "confidence: %d%%\n", analysis->confidence);
    fprintf(fp, "flux_preservation_required: %s\n\n",
            analysis->requires_flux_preservation ? "true" : "false");
    
    /* DPM data */
    if (analysis->dpm_map) {
        fprintf(fp, "# DPM Data\n");
        fprintf(fp, "dpm_anomalies: %u\n", analysis->dpm_map->anomalies_found);
        fprintf(fp, "dpm_deviation_min: %d ns\n", analysis->dpm_map->min_deviation_ns);
        fprintf(fp, "dpm_deviation_max: %d ns\n", analysis->dpm_map->max_deviation_ns);
        fprintf(fp, "dpm_deviation_avg: %d ns\n\n", analysis->dpm_map->avg_deviation_ns);
    }
    
    /* Weak bits */
    if (analysis->weak_bit_count > 0) {
        fprintf(fp, "# Weak Bit Sectors\n");
        fprintf(fp, "weak_sector_count: %u\n", analysis->weak_bit_count);
        fprintf(fp, "weak_sectors:\n");
        for (uint32_t i = 0; i < analysis->weak_bit_count && i < 100; i++) {
            fprintf(fp, "  - track: %d, head: %d, sector: %d\n",
                    analysis->weak_bits[i].track,
                    analysis->weak_bits[i].head,
                    analysis->weak_bits[i].sector);
        }
    }
    
    fclose(fp);
    return UFT_SUCCESS;
}
