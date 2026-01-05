/**
 * @file uft_surface_analyzer.c
 * @brief Disk Surface Analysis Implementation
 * 
 * EXT4-007: Physical disk surface analysis
 * 
 * Features:
 * - Surface defect detection
 * - Track eccentricity analysis
 * - Head alignment checking
 * - Weak bit mapping
 * - Magnetic coating degradation
 */

#include "uft/analysis/uft_surface_analyzer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define MAX_TRACKS          84
#define MAX_REVOLUTIONS     5
#define DEFECT_THRESHOLD    3       /* Consecutive failures */
#define WEAK_BIT_THRESHOLD  0.7     /* Similarity threshold */
#define TIMING_VARIANCE_MAX 0.15    /* 15% timing variance */

/*===========================================================================
 * Surface Analyzer Context
 *===========================================================================*/

int uft_surface_init(uft_surface_ctx_t *ctx)
{
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    
    /* Allocate track analysis array */
    ctx->tracks = calloc(MAX_TRACKS * 2, sizeof(uft_surface_track_t));
    if (!ctx->tracks) return -1;
    
    ctx->max_tracks = MAX_TRACKS * 2;
    
    return 0;
}

void uft_surface_free(uft_surface_ctx_t *ctx)
{
    if (ctx) {
        if (ctx->tracks) {
            for (size_t i = 0; i < ctx->track_count; i++) {
                if (ctx->tracks[i].defects) {
                    free(ctx->tracks[i].defects);
                }
            }
            free(ctx->tracks);
        }
        memset(ctx, 0, sizeof(*ctx));
    }
}

/*===========================================================================
 * Track Analysis
 *===========================================================================*/

int uft_surface_analyze_track(uft_surface_ctx_t *ctx,
                              int track, int side,
                              const uint32_t *flux_times, size_t flux_count,
                              const uint32_t *rev_indices, int rev_count)
{
    if (!ctx || !flux_times || flux_count < 100) return -1;
    if (ctx->track_count >= ctx->max_tracks) return -1;
    
    uft_surface_track_t *t = &ctx->tracks[ctx->track_count];
    memset(t, 0, sizeof(*t));
    
    t->track = track;
    t->side = side;
    t->flux_count = flux_count;
    
    /* Allocate defect array */
    t->defects = calloc(256, sizeof(uft_surface_defect_t));
    if (!t->defects) return -1;
    t->max_defects = 256;
    
    /* Calculate track timing */
    if (rev_count >= 2 && rev_indices) {
        uint32_t rev_time = flux_times[rev_indices[1]] - flux_times[rev_indices[0]];
        t->track_time_us = rev_time / 1000;  /* Convert ns to us */
        
        /* Expected time for 300 RPM is 200ms = 200000us */
        /* For 360 RPM it's 166.67ms */
        if (t->track_time_us > 190000 && t->track_time_us < 210000) {
            t->rpm_estimate = 300.0;
        } else if (t->track_time_us > 160000 && t->track_time_us < 175000) {
            t->rpm_estimate = 360.0;
        } else {
            t->rpm_estimate = 60000000.0 / t->track_time_us;
        }
    }
    
    /* Analyze flux intervals */
    uint32_t min_interval = UINT32_MAX;
    uint32_t max_interval = 0;
    uint64_t sum = 0;
    
    for (size_t i = 1; i < flux_count; i++) {
        uint32_t interval = flux_times[i] - flux_times[i - 1];
        sum += interval;
        
        if (interval < min_interval) min_interval = interval;
        if (interval > max_interval) max_interval = interval;
    }
    
    t->avg_interval = sum / (flux_count - 1);
    t->min_interval = min_interval;
    t->max_interval = max_interval;
    
    /* Detect timing anomalies (potential defects) */
    uint32_t threshold_low = t->avg_interval * 0.5;
    uint32_t threshold_high = t->avg_interval * 4.0;
    
    int anomaly_run = 0;
    size_t anomaly_start = 0;
    
    for (size_t i = 1; i < flux_count; i++) {
        uint32_t interval = flux_times[i] - flux_times[i - 1];
        
        if (interval < threshold_low || interval > threshold_high) {
            if (anomaly_run == 0) {
                anomaly_start = i;
            }
            anomaly_run++;
        } else {
            if (anomaly_run >= DEFECT_THRESHOLD && t->defect_count < t->max_defects) {
                /* Record defect */
                uft_surface_defect_t *d = &t->defects[t->defect_count];
                d->type = UFT_DEFECT_TIMING;
                d->start_flux = anomaly_start;
                d->end_flux = i;
                d->severity = (anomaly_run > 10) ? UFT_SEV_HIGH : UFT_SEV_MEDIUM;
                
                /* Estimate angular position */
                d->angular_pos = (360.0 * anomaly_start) / flux_count;
                
                t->defect_count++;
            }
            anomaly_run = 0;
        }
    }
    
    /* Calculate quality score */
    t->quality_score = 100;
    
    /* Deduct for timing variance */
    float variance = (float)(max_interval - min_interval) / t->avg_interval;
    if (variance > TIMING_VARIANCE_MAX) {
        t->quality_score -= 20;
    }
    
    /* Deduct for defects */
    t->quality_score -= t->defect_count * 5;
    
    if (t->quality_score < 0) t->quality_score = 0;
    
    ctx->track_count++;
    return 0;
}

/*===========================================================================
 * Multi-Revolution Weak Bit Analysis
 *===========================================================================*/

int uft_surface_weak_bits(uft_surface_ctx_t *ctx,
                          const uint32_t **revolutions,
                          const size_t *rev_counts,
                          int rev_count)
{
    if (!ctx || !revolutions || !rev_counts || rev_count < 2) return -1;
    
    /* Find minimum revolution length */
    size_t min_len = rev_counts[0];
    for (int r = 1; r < rev_count; r++) {
        if (rev_counts[r] < min_len) {
            min_len = rev_counts[r];
        }
    }
    
    if (min_len < 100) return -1;
    
    /* Compare intervals between revolutions */
    uint32_t *ref_intervals = malloc((min_len - 1) * sizeof(uint32_t));
    if (!ref_intervals) return -1;
    
    /* Use first revolution as reference */
    for (size_t i = 1; i < min_len; i++) {
        ref_intervals[i - 1] = revolutions[0][i] - revolutions[0][i - 1];
    }
    
    /* Compare each subsequent revolution */
    for (int r = 1; r < rev_count; r++) {
        for (size_t i = 1; i < min_len; i++) {
            uint32_t interval = revolutions[r][i] - revolutions[r][i - 1];
            uint32_t ref = ref_intervals[i - 1];
            
            /* Calculate similarity */
            float ratio;
            if (ref > 0) {
                ratio = (float)interval / ref;
            } else {
                ratio = 0;
            }
            
            /* Detect weak bit */
            if (ratio < WEAK_BIT_THRESHOLD || ratio > (1.0 / WEAK_BIT_THRESHOLD)) {
                ctx->weak_bit_count++;
                
                /* Record if we have space */
                if (ctx->track_count > 0) {
                    uft_surface_track_t *t = &ctx->tracks[ctx->track_count - 1];
                    
                    if (t->defect_count < t->max_defects) {
                        uft_surface_defect_t *d = &t->defects[t->defect_count];
                        d->type = UFT_DEFECT_WEAK_BIT;
                        d->start_flux = i;
                        d->end_flux = i;
                        d->severity = UFT_SEV_LOW;
                        d->angular_pos = (360.0 * i) / min_len;
                        t->defect_count++;
                    }
                }
            }
        }
    }
    
    free(ref_intervals);
    return 0;
}

/*===========================================================================
 * Head Alignment Analysis
 *===========================================================================*/

int uft_surface_head_alignment(const uft_surface_ctx_t *ctx,
                               uft_head_alignment_t *alignment)
{
    if (!ctx || !alignment || ctx->track_count < 4) return -1;
    
    memset(alignment, 0, sizeof(*alignment));
    
    /* Compare side 0 vs side 1 track timings */
    int side0_count = 0, side1_count = 0;
    uint64_t side0_time = 0, side1_time = 0;
    
    for (size_t i = 0; i < ctx->track_count; i++) {
        const uft_surface_track_t *t = &ctx->tracks[i];
        
        if (t->side == 0) {
            side0_time += t->track_time_us;
            side0_count++;
        } else {
            side1_time += t->track_time_us;
            side1_count++;
        }
    }
    
    if (side0_count > 0 && side1_count > 0) {
        alignment->side0_avg_time = side0_time / side0_count;
        alignment->side1_avg_time = side1_time / side1_count;
        
        /* Time difference indicates alignment issues */
        int64_t diff = (int64_t)alignment->side0_avg_time - alignment->side1_avg_time;
        if (diff < 0) diff = -diff;
        
        alignment->timing_diff = (uint32_t)diff;
        
        /* Calculate alignment quality */
        float diff_percent = (float)diff / alignment->side0_avg_time * 100.0;
        
        if (diff_percent < 0.5) {
            alignment->quality = UFT_ALIGN_EXCELLENT;
        } else if (diff_percent < 1.0) {
            alignment->quality = UFT_ALIGN_GOOD;
        } else if (diff_percent < 2.0) {
            alignment->quality = UFT_ALIGN_FAIR;
        } else {
            alignment->quality = UFT_ALIGN_POOR;
        }
    }
    
    /* Check track-to-track consistency */
    float rpm_min = 1000, rpm_max = 0;
    
    for (size_t i = 0; i < ctx->track_count; i++) {
        const uft_surface_track_t *t = &ctx->tracks[i];
        
        if (t->rpm_estimate > 0) {
            if (t->rpm_estimate < rpm_min) rpm_min = t->rpm_estimate;
            if (t->rpm_estimate > rpm_max) rpm_max = t->rpm_estimate;
        }
    }
    
    alignment->rpm_variance = rpm_max - rpm_min;
    
    return 0;
}

/*===========================================================================
 * Track Eccentricity Analysis
 *===========================================================================*/

int uft_surface_eccentricity(const uft_surface_ctx_t *ctx,
                             uft_eccentricity_t *ecc)
{
    if (!ctx || !ecc || ctx->track_count == 0) return -1;
    
    memset(ecc, 0, sizeof(*ecc));
    
    /* Eccentricity shows as periodic timing variation within a track */
    /* A perfectly centered disk has constant angular velocity */
    /* An eccentric disk has faster/slower regions */
    
    /* Use track 40 (middle of disk) as reference */
    const uft_surface_track_t *ref_track = NULL;
    
    for (size_t i = 0; i < ctx->track_count; i++) {
        if (ctx->tracks[i].track == 40 && ctx->tracks[i].side == 0) {
            ref_track = &ctx->tracks[i];
            break;
        }
    }
    
    if (!ref_track) {
        /* Use first track */
        ref_track = &ctx->tracks[0];
    }
    
    /* Calculate timing deviation from average */
    float deviation = (float)(ref_track->max_interval - ref_track->min_interval) / 
                      ref_track->avg_interval;
    
    ecc->max_deviation_percent = deviation * 100.0;
    
    /* Estimate eccentricity in terms of track displacement */
    /* Higher deviation = higher eccentricity */
    if (deviation < 0.05) {
        ecc->severity = UFT_ECC_NONE;
    } else if (deviation < 0.10) {
        ecc->severity = UFT_ECC_LOW;
    } else if (deviation < 0.20) {
        ecc->severity = UFT_ECC_MEDIUM;
    } else {
        ecc->severity = UFT_ECC_HIGH;
    }
    
    return 0;
}

/*===========================================================================
 * Surface Map Generation
 *===========================================================================*/

int uft_surface_generate_map(const uft_surface_ctx_t *ctx,
                             uft_surface_map_t *map)
{
    if (!ctx || !map) return -1;
    
    memset(map, 0, sizeof(*map));
    
    /* Find track range */
    int min_track = 255, max_track = 0;
    int max_side = 0;
    
    for (size_t i = 0; i < ctx->track_count; i++) {
        if (ctx->tracks[i].track < min_track) min_track = ctx->tracks[i].track;
        if (ctx->tracks[i].track > max_track) max_track = ctx->tracks[i].track;
        if (ctx->tracks[i].side > max_side) max_side = ctx->tracks[i].side;
    }
    
    map->min_track = min_track;
    map->max_track = max_track;
    map->sides = max_side + 1;
    
    /* Build quality map */
    for (size_t i = 0; i < ctx->track_count; i++) {
        const uft_surface_track_t *t = &ctx->tracks[i];
        
        if (t->track < MAX_TRACKS && t->side < 2) {
            map->quality[t->track][t->side] = t->quality_score;
            map->defects[t->track][t->side] = t->defect_count;
        }
    }
    
    /* Count overall statistics */
    for (int t = 0; t <= max_track; t++) {
        for (int s = 0; s <= max_side; s++) {
            if (map->quality[t][s] >= 80) {
                map->good_tracks++;
            } else if (map->quality[t][s] >= 50) {
                map->fair_tracks++;
            } else if (map->quality[t][s] > 0) {
                map->bad_tracks++;
            }
            
            map->total_defects += map->defects[t][s];
        }
    }
    
    return 0;
}

/*===========================================================================
 * Report Generation
 *===========================================================================*/

int uft_surface_report_json(const uft_surface_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    uft_surface_map_t map;
    uft_surface_generate_map(ctx, &map);
    
    uft_head_alignment_t align;
    uft_surface_head_alignment(ctx, &align);
    
    uft_eccentricity_t ecc;
    uft_surface_eccentricity(ctx, &ecc);
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"track_count\": %zu,\n"
        "  \"track_range\": [%d, %d],\n"
        "  \"sides\": %d,\n"
        "  \"good_tracks\": %d,\n"
        "  \"fair_tracks\": %d,\n"
        "  \"bad_tracks\": %d,\n"
        "  \"total_defects\": %d,\n"
        "  \"weak_bits\": %zu,\n"
        "  \"head_alignment\": \"%s\",\n"
        "  \"eccentricity\": \"%s\",\n"
        "  \"rpm_variance\": %.2f\n"
        "}",
        ctx->track_count,
        map.min_track, map.max_track,
        map.sides,
        map.good_tracks,
        map.fair_tracks,
        map.bad_tracks,
        map.total_defects,
        ctx->weak_bit_count,
        (align.quality == UFT_ALIGN_EXCELLENT) ? "Excellent" :
        (align.quality == UFT_ALIGN_GOOD) ? "Good" :
        (align.quality == UFT_ALIGN_FAIR) ? "Fair" : "Poor",
        (ecc.severity == UFT_ECC_NONE) ? "None" :
        (ecc.severity == UFT_ECC_LOW) ? "Low" :
        (ecc.severity == UFT_ECC_MEDIUM) ? "Medium" : "High",
        align.rpm_variance
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
