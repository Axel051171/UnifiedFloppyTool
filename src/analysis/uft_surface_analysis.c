/**
 * @file uft_surface_analysis.c
 * @brief Disk Surface Analysis Implementation
 * 
 * EXT4-007: Comprehensive disk surface analysis
 * 
 * Features:
 * - Track-by-track analysis
 * - Surface defect detection
 * - Flux density mapping
 * - Head alignment analysis
 * - Disk condition reporting
 */

#include "uft/analysis/uft_surface_analysis.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define MAX_TRACKS      84
#define MAX_SIDES       2
#define MAX_SECTORS     36
#define SAMPLE_WINDOW   100     /* Samples for running average */

/*===========================================================================
 * Track Analysis
 *===========================================================================*/

int uft_surface_analyze_track(const uint32_t *flux_times, size_t flux_count,
                              double sample_clock,
                              uft_track_surface_t *surface)
{
    if (!flux_times || !surface || flux_count < 100) return -1;
    
    memset(surface, 0, sizeof(*surface));
    surface->flux_count = flux_count;
    
    /* Calculate basic timing */
    uint64_t total_time = 0;
    double min_pulse = INFINITY;
    double max_pulse = 0;
    
    for (size_t i = 1; i < flux_count; i++) {
        uint32_t duration = flux_times[i] - flux_times[i - 1];
        double us = (double)duration * 1000000.0 / sample_clock;
        
        total_time += duration;
        
        if (us < min_pulse) min_pulse = us;
        if (us > max_pulse) max_pulse = us;
    }
    
    surface->track_time_us = (double)total_time * 1000000.0 / sample_clock;
    surface->min_pulse_us = min_pulse;
    surface->max_pulse_us = max_pulse;
    surface->mean_pulse_us = surface->track_time_us / (flux_count - 1);
    
    /* Calculate flux density (flux transitions per rotation) */
    surface->flux_density = flux_count;
    
    /* Estimate data rate */
    /* For MFM: 2 flux transitions per bit on average */
    double bits_per_rotation = flux_count / 2.0;
    double rotation_time_s = surface->track_time_us / 1000000.0;
    surface->estimated_data_rate = bits_per_rotation / rotation_time_s;
    
    /* Analyze flux distribution */
    int histogram[256] = {0};
    double bin_size = 0.1;  /* 100ns bins */
    
    for (size_t i = 1; i < flux_count; i++) {
        double us = (double)(flux_times[i] - flux_times[i - 1]) 
                   * 1000000.0 / sample_clock;
        int bin = (int)(us / bin_size);
        if (bin >= 0 && bin < 256) {
            histogram[bin]++;
        }
    }
    
    /* Find peaks (expected at 2µs, 3µs, 4µs for MFM DD) */
    int peak_bins[8];
    int peak_count = 0;
    
    for (int i = 5; i < 250 && peak_count < 8; i++) {
        if (histogram[i] > histogram[i-1] && histogram[i] > histogram[i+1] &&
            histogram[i] > histogram[i-2] && histogram[i] > histogram[i+2] &&
            histogram[i] > flux_count / 50) {
            peak_bins[peak_count++] = i;
        }
    }
    
    surface->timing_peak_count = peak_count;
    for (int i = 0; i < peak_count && i < 8; i++) {
        surface->timing_peaks[i] = peak_bins[i] * bin_size;
    }
    
    /* Detect anomalies */
    surface->anomaly_count = 0;
    
    /* Look for unusually long pulses (dropouts) */
    double dropout_threshold = max_pulse > 10.0 ? 10.0 : max_pulse * 0.8;
    
    for (size_t i = 1; i < flux_count; i++) {
        double us = (double)(flux_times[i] - flux_times[i - 1]) 
                   * 1000000.0 / sample_clock;
        
        if (us > dropout_threshold) {
            if (surface->anomaly_count < 64) {
                surface->anomaly_positions[surface->anomaly_count] = i;
                surface->anomaly_types[surface->anomaly_count] = UFT_ANOMALY_DROPOUT;
                surface->anomaly_count++;
            }
        }
    }
    
    /* Calculate quality score */
    surface->quality_score = 100.0;
    
    /* Penalize based on timing distribution */
    if (peak_count < 3) {
        surface->quality_score -= (3 - peak_count) * 10;
    }
    
    /* Penalize dropouts */
    surface->quality_score -= surface->anomaly_count * 2;
    
    /* Penalize high pulse variation */
    double pulse_range = max_pulse - min_pulse;
    if (pulse_range > 5.0) {
        surface->quality_score -= (pulse_range - 5.0) * 5;
    }
    
    if (surface->quality_score < 0) surface->quality_score = 0;
    
    return 0;
}

/*===========================================================================
 * Multi-Track Surface Map
 *===========================================================================*/

int uft_surface_map_init(uft_surface_map_t *map, int tracks, int sides)
{
    if (!map || tracks > MAX_TRACKS || sides > MAX_SIDES) return -1;
    
    memset(map, 0, sizeof(*map));
    map->tracks = tracks;
    map->sides = sides;
    
    /* Allocate track data */
    map->track_data = calloc(tracks * sides, sizeof(uft_track_surface_t));
    if (!map->track_data) return -1;
    
    return 0;
}

void uft_surface_map_free(uft_surface_map_t *map)
{
    if (!map) return;
    
    free(map->track_data);
    memset(map, 0, sizeof(*map));
}

int uft_surface_map_set_track(uft_surface_map_t *map, int track, int side,
                              const uft_track_surface_t *surface)
{
    if (!map || !surface || !map->track_data) return -1;
    if (track < 0 || track >= map->tracks) return -1;
    if (side < 0 || side >= map->sides) return -1;
    
    int idx = track * map->sides + side;
    map->track_data[idx] = *surface;
    map->track_data[idx].valid = true;
    
    return 0;
}

const uft_track_surface_t *uft_surface_map_get_track(const uft_surface_map_t *map,
                                                     int track, int side)
{
    if (!map || !map->track_data) return NULL;
    if (track < 0 || track >= map->tracks) return NULL;
    if (side < 0 || side >= map->sides) return NULL;
    
    int idx = track * map->sides + side;
    return &map->track_data[idx];
}

/*===========================================================================
 * Surface Statistics
 *===========================================================================*/

int uft_surface_get_stats(const uft_surface_map_t *map, 
                          uft_surface_stats_t *stats)
{
    if (!map || !stats || !map->track_data) return -1;
    
    memset(stats, 0, sizeof(*stats));
    stats->total_tracks = map->tracks * map->sides;
    
    double quality_sum = 0;
    double flux_sum = 0;
    int valid_count = 0;
    
    for (int t = 0; t < map->tracks; t++) {
        for (int s = 0; s < map->sides; s++) {
            const uft_track_surface_t *track = uft_surface_map_get_track(map, t, s);
            
            if (track && track->valid) {
                valid_count++;
                quality_sum += track->quality_score;
                flux_sum += track->flux_count;
                
                stats->total_anomalies += track->anomaly_count;
                
                if (track->quality_score >= 90) {
                    stats->good_tracks++;
                } else if (track->quality_score >= 70) {
                    stats->fair_tracks++;
                } else if (track->quality_score >= 50) {
                    stats->poor_tracks++;
                } else {
                    stats->bad_tracks++;
                }
                
                /* Track min/max quality */
                if (stats->min_quality == 0 || track->quality_score < stats->min_quality) {
                    stats->min_quality = track->quality_score;
                    stats->worst_track = t;
                    stats->worst_side = s;
                }
                if (track->quality_score > stats->max_quality) {
                    stats->max_quality = track->quality_score;
                    stats->best_track = t;
                    stats->best_side = s;
                }
            }
        }
    }
    
    if (valid_count > 0) {
        stats->avg_quality = quality_sum / valid_count;
        stats->avg_flux = flux_sum / valid_count;
    }
    
    stats->analyzed_tracks = valid_count;
    
    /* Overall disk grade */
    if (stats->avg_quality >= 90 && stats->bad_tracks == 0) {
        stats->disk_grade = 'A';
    } else if (stats->avg_quality >= 80 && stats->bad_tracks <= 2) {
        stats->disk_grade = 'B';
    } else if (stats->avg_quality >= 70) {
        stats->disk_grade = 'C';
    } else if (stats->avg_quality >= 50) {
        stats->disk_grade = 'D';
    } else {
        stats->disk_grade = 'F';
    }
    
    return 0;
}

/*===========================================================================
 * Head Alignment Analysis
 *===========================================================================*/

int uft_surface_check_alignment(const uft_surface_map_t *map,
                                uft_alignment_result_t *result)
{
    if (!map || !result || !map->track_data) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* Compare flux density across tracks */
    double flux_densities[MAX_TRACKS] = {0};
    int valid_tracks = 0;
    
    for (int t = 0; t < map->tracks; t++) {
        double side_sum = 0;
        int side_count = 0;
        
        for (int s = 0; s < map->sides; s++) {
            const uft_track_surface_t *track = uft_surface_map_get_track(map, t, s);
            if (track && track->valid) {
                side_sum += track->flux_density;
                side_count++;
            }
        }
        
        if (side_count > 0) {
            flux_densities[t] = side_sum / side_count;
            valid_tracks++;
        }
    }
    
    if (valid_tracks < 5) {
        result->alignment_status = UFT_ALIGN_UNKNOWN;
        return 0;
    }
    
    /* Check for consistent flux density across tracks */
    double mean = 0;
    for (int t = 0; t < map->tracks; t++) {
        if (flux_densities[t] > 0) {
            mean += flux_densities[t];
        }
    }
    mean /= valid_tracks;
    
    double variance = 0;
    for (int t = 0; t < map->tracks; t++) {
        if (flux_densities[t] > 0) {
            double diff = flux_densities[t] - mean;
            variance += diff * diff;
        }
    }
    variance /= valid_tracks;
    
    result->flux_variance = sqrt(variance);
    result->mean_flux = mean;
    
    /* Check for radial gradient (indicates off-center or worn disk) */
    double inner_mean = 0, outer_mean = 0;
    int inner_count = 0, outer_count = 0;
    
    for (int t = 0; t < map->tracks / 2; t++) {
        if (flux_densities[t] > 0) {
            outer_mean += flux_densities[t];
            outer_count++;
        }
    }
    for (int t = map->tracks / 2; t < map->tracks; t++) {
        if (flux_densities[t] > 0) {
            inner_mean += flux_densities[t];
            inner_count++;
        }
    }
    
    if (outer_count > 0 && inner_count > 0) {
        outer_mean /= outer_count;
        inner_mean /= inner_count;
        
        result->radial_gradient = (inner_mean - outer_mean) / mean * 100;
    }
    
    /* Determine alignment status */
    double cv = result->flux_variance / mean;
    
    if (cv < 0.05 && fabs(result->radial_gradient) < 5) {
        result->alignment_status = UFT_ALIGN_GOOD;
    } else if (cv < 0.1 && fabs(result->radial_gradient) < 10) {
        result->alignment_status = UFT_ALIGN_FAIR;
    } else if (cv < 0.15) {
        result->alignment_status = UFT_ALIGN_POOR;
    } else {
        result->alignment_status = UFT_ALIGN_BAD;
    }
    
    return 0;
}

/*===========================================================================
 * Defect Detection
 *===========================================================================*/

int uft_surface_find_defects(const uft_surface_map_t *map,
                             uft_defect_t *defects, size_t max_defects,
                             size_t *defect_count)
{
    if (!map || !defects || !defect_count || !map->track_data) return -1;
    
    *defect_count = 0;
    
    for (int t = 0; t < map->tracks && *defect_count < max_defects; t++) {
        for (int s = 0; s < map->sides && *defect_count < max_defects; s++) {
            const uft_track_surface_t *track = uft_surface_map_get_track(map, t, s);
            
            if (!track || !track->valid) continue;
            
            /* Check for severe quality issues */
            if (track->quality_score < 50) {
                defects[*defect_count].track = t;
                defects[*defect_count].side = s;
                defects[*defect_count].type = UFT_DEFECT_QUALITY;
                defects[*defect_count].severity = 100 - track->quality_score;
                defects[*defect_count].position = 0;
                (*defect_count)++;
            }
            
            /* Check for anomalies */
            for (size_t a = 0; a < track->anomaly_count && *defect_count < max_defects; a++) {
                defects[*defect_count].track = t;
                defects[*defect_count].side = s;
                defects[*defect_count].type = UFT_DEFECT_DROPOUT;
                defects[*defect_count].severity = 50;
                defects[*defect_count].position = track->anomaly_positions[a];
                (*defect_count)++;
            }
        }
    }
    
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_surface_report_json(const uft_surface_stats_t *stats,
                            char *buffer, size_t size)
{
    if (!stats || !buffer || size == 0) return -1;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"surface_analysis\": {\n"
        "    \"total_tracks\": %d,\n"
        "    \"analyzed_tracks\": %d,\n"
        "    \"good_tracks\": %d,\n"
        "    \"fair_tracks\": %d,\n"
        "    \"poor_tracks\": %d,\n"
        "    \"bad_tracks\": %d,\n"
        "    \"avg_quality\": %.2f,\n"
        "    \"min_quality\": %.2f,\n"
        "    \"max_quality\": %.2f,\n"
        "    \"worst_track\": [%d, %d],\n"
        "    \"best_track\": [%d, %d],\n"
        "    \"total_anomalies\": %d,\n"
        "    \"disk_grade\": \"%c\"\n"
        "  }\n"
        "}",
        stats->total_tracks,
        stats->analyzed_tracks,
        stats->good_tracks,
        stats->fair_tracks,
        stats->poor_tracks,
        stats->bad_tracks,
        stats->avg_quality,
        stats->min_quality,
        stats->max_quality,
        stats->worst_track, stats->worst_side,
        stats->best_track, stats->best_side,
        stats->total_anomalies,
        stats->disk_grade
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
