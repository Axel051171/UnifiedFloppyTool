/**
 * @file uft_bitrate_analysis.c
 * @brief Bitrate Analysis Implementation
 * 
 * Software-level implementation of bitrate analysis concepts from nibtools IHS.
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/analysis/uft_bitrate_analysis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Static Data
 * ============================================================================ */

/** Standard bitrates for each density zone */
static const uint32_t standard_bitrates[4] = {
    BITRATE_DENSITY_0,  /* Density 0: 250 kbit/s */
    BITRATE_DENSITY_1,  /* Density 1: 266 kbit/s */
    BITRATE_DENSITY_2,  /* Density 2: 285 kbit/s */
    BITRATE_DENSITY_3   /* Density 3: 307 kbit/s */
};

/** Track to density mapping */
static const int track_density[43] = {
    -1,  /* Track 0 (invalid) */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  /*  1-10: Density 3 */
    3, 3, 3, 3, 3, 3, 3,           /* 11-17: Density 3 */
    2, 2, 2, 2, 2, 2, 2,           /* 18-24: Density 2 */
    1, 1, 1, 1, 1, 1,              /* 25-30: Density 1 */
    0, 0, 0, 0, 0,                 /* 31-35: Density 0 */
    0, 0, 0, 0, 0, 0, 0            /* 36-42: Density 0 (extended) */
};

/** Expected track capacity for each density */
static const size_t track_capacity[4] = {
    6250,   /* Density 0 */
    6666,   /* Density 1 */
    7142,   /* Density 2 */
    7692    /* Density 3 */
};

/** Flux source names */
static const char *source_names[] = {
    "Unknown", "SCP", "Kryoflux", "HFE", "Raw"
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Calculate standard deviation
 */
static float calc_std_dev(const uint32_t *values, size_t count, uint32_t mean)
{
    if (count < 2) return 0.0f;
    
    double sum_sq = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = (double)values[i] - mean;
        sum_sq += diff * diff;
    }
    
    return (float)sqrt(sum_sq / (count - 1));
}

/**
 * @brief Find nearest standard density
 */
static int find_nearest_density(uint32_t bitrate)
{
    int best_density = 0;
    uint32_t best_diff = UINT32_MAX;
    
    for (int d = 0; d < 4; d++) {
        uint32_t diff = (bitrate > standard_bitrates[d]) ?
                        (bitrate - standard_bitrates[d]) :
                        (standard_bitrates[d] - bitrate);
        if (diff < best_diff) {
            best_diff = diff;
            best_density = d;
        }
    }
    
    return best_density;
}

/* ============================================================================
 * Bitrate Calculation
 * ============================================================================ */

/**
 * @brief Calculate bitrate from flux timing data
 */
uint32_t bitrate_from_flux(const uint32_t *flux_data, size_t num_samples,
                           uint32_t sample_rate)
{
    if (!flux_data || num_samples < 2 || sample_rate == 0) return 0;
    
    /* Sum all timing values */
    uint64_t total_time = 0;
    for (size_t i = 0; i < num_samples; i++) {
        total_time += flux_data[i];
    }
    
    if (total_time == 0) return 0;
    
    /* Calculate total time in seconds */
    double time_seconds = (double)total_time / sample_rate;
    
    /* Each flux transition is approximately 1 bit */
    /* (Actually it's more complex with MFM/GCR, but this is an approximation) */
    return (uint32_t)(num_samples / time_seconds);
}

/**
 * @brief Calculate bitrate from GCR track data
 */
uint32_t bitrate_from_gcr(const uint8_t *track_data, size_t track_length,
                          uint32_t revolution_time_ns)
{
    if (!track_data || track_length == 0 || revolution_time_ns == 0) return 0;
    
    /* Total bits = bytes * 8 */
    uint64_t total_bits = track_length * 8;
    
    /* Time in seconds */
    double time_seconds = revolution_time_ns / 1e9;
    
    return (uint32_t)(total_bits / time_seconds);
}

/**
 * @brief Get expected bitrate for track
 */
uint32_t bitrate_expected(int track)
{
    if (track < 1 || track > 42) return 0;
    int density = track_density[track];
    if (density < 0 || density > 3) return 0;
    return standard_bitrates[density];
}

/**
 * @brief Get density from bitrate
 */
int bitrate_to_density(uint32_t bitrate)
{
    if (bitrate == 0) return -1;
    
    /* Find closest matching density */
    return find_nearest_density(bitrate);
}

/**
 * @brief Get bitrate for density
 */
uint32_t density_to_bitrate(int density)
{
    if (density < 0 || density > 3) return 0;
    return standard_bitrates[density];
}

/* ============================================================================
 * Index Hole Analysis
 * ============================================================================ */

/**
 * @brief Analyze index hole timing
 */
bool analyze_index_timing(const uint32_t *flux_data, size_t num_samples,
                          uint32_t sample_rate, index_info_t *result)
{
    if (!result) return false;
    
    memset(result, 0, sizeof(index_info_t));
    
    if (!flux_data || num_samples == 0 || sample_rate == 0) {
        return false;
    }
    
    /* Sum timing to get total revolution time */
    uint64_t total_ticks = 0;
    for (size_t i = 0; i < num_samples; i++) {
        total_ticks += flux_data[i];
    }
    
    /* Convert to nanoseconds */
    uint64_t total_ns = (total_ticks * 1000000000ULL) / sample_rate;
    
    result->revolution_time = (uint32_t)total_ns;
    result->rpm = calculate_rpm(result->revolution_time);
    result->index_detected = true;
    
    /* Calculate variation (simplified - would need multiple revolutions) */
    result->rpm_variation = 0.0f;
    
    return true;
}

/**
 * @brief Calculate RPM from revolution time
 */
float calculate_rpm(uint32_t revolution_time_ns)
{
    if (revolution_time_ns == 0) return 0.0f;
    
    /* RPM = 60 seconds / revolution_time_seconds */
    double rev_time_sec = revolution_time_ns / 1e9;
    return (float)(60.0 / rev_time_sec);
}

/**
 * @brief Check if RPM is within standard tolerance
 */
bool rpm_is_standard(float rpm)
{
    /* Standard is 300 RPM ±3% */
    return (rpm >= 291.0f && rpm <= 309.0f);
}

/* ============================================================================
 * Track Analysis
 * ============================================================================ */

/**
 * @brief Analyze bitrate statistics for a track
 */
int analyze_track_bitrate(const uint32_t *flux_data, size_t num_samples,
                          uint32_t sample_rate, int track,
                          bitrate_stats_t *stats)
{
    if (!stats) return -1;
    
    memset(stats, 0, sizeof(bitrate_stats_t));
    stats->track = track;
    stats->halftrack = track * 2;
    
    if (!flux_data || num_samples == 0 || sample_rate == 0) {
        return -2;
    }
    
    /* Calculate bitrate statistics */
    uint64_t total_time = 0;
    uint32_t min_interval = UINT32_MAX;
    uint32_t max_interval = 0;
    
    for (size_t i = 0; i < num_samples; i++) {
        total_time += flux_data[i];
        if (flux_data[i] < min_interval) min_interval = flux_data[i];
        if (flux_data[i] > max_interval) max_interval = flux_data[i];
    }
    
    /* Convert to nanoseconds */
    stats->total_time_ns = (uint32_t)((total_time * 1000000000ULL) / sample_rate);
    stats->total_bits = num_samples;
    
    /* Calculate RPM */
    stats->rpm = calculate_rpm(stats->total_time_ns);
    
    /* Calculate average bitrate */
    if (stats->total_time_ns > 0) {
        double time_sec = stats->total_time_ns / 1e9;
        stats->avg_bitrate = (uint32_t)(num_samples / time_sec);
    }
    
    /* Min/max bitrate (inverse of max/min intervals) */
    if (max_interval > 0) {
        double min_interval_sec = (double)max_interval / sample_rate;
        stats->min_bitrate = (uint32_t)(1.0 / min_interval_sec);
    }
    if (min_interval > 0 && min_interval != UINT32_MAX) {
        double max_interval_sec = (double)min_interval / sample_rate;
        stats->max_bitrate = (uint32_t)(1.0 / max_interval_sec);
    }
    
    /* Detect density */
    stats->detected_density = find_nearest_density(stats->avg_bitrate);
    
    /* Calculate confidence */
    uint32_t expected = standard_bitrates[stats->detected_density];
    float diff_pct = 100.0f * fabsf((float)stats->avg_bitrate - expected) / expected;
    stats->density_confidence = (diff_pct < 10.0f) ? (100.0f - diff_pct * 10) : 0.0f;
    
    /* Quality assessment */
    stats->stable = (stats->max_bitrate - stats->min_bitrate < 
                     stats->avg_bitrate * BITRATE_TOLERANCE_PCT / 100);
    stats->valid = (stats->avg_bitrate > 200000 && stats->avg_bitrate < 400000);
    stats->quality_score = stats->density_confidence * (stats->stable ? 1.0f : 0.5f);
    
    return 0;
}

/**
 * @brief Analyze bitrate from GCR track data
 */
int analyze_gcr_bitrate(const uint8_t *gcr_data, size_t gcr_length,
                        uint32_t revolution_time_ns, int track,
                        bitrate_stats_t *stats)
{
    if (!stats) return -1;
    
    memset(stats, 0, sizeof(bitrate_stats_t));
    stats->track = track;
    stats->halftrack = track * 2;
    
    if (!gcr_data || gcr_length == 0) {
        return -2;
    }
    
    /* Estimate revolution time if not provided */
    if (revolution_time_ns == 0) {
        revolution_time_ns = estimate_revolution_time(track, gcr_length);
    }
    
    stats->total_time_ns = revolution_time_ns;
    stats->total_bits = gcr_length * 8;
    stats->rpm = calculate_rpm(revolution_time_ns);
    
    /* Calculate bitrate */
    stats->avg_bitrate = bitrate_from_gcr(gcr_data, gcr_length, revolution_time_ns);
    stats->min_bitrate = stats->avg_bitrate;  /* Can't determine from GCR */
    stats->max_bitrate = stats->avg_bitrate;
    
    /* Detect density */
    stats->detected_density = find_nearest_density(stats->avg_bitrate);
    
    /* Calculate confidence */
    uint32_t expected = standard_bitrates[stats->detected_density];
    float diff_pct = 100.0f * fabsf((float)stats->avg_bitrate - expected) / expected;
    stats->density_confidence = (diff_pct < 10.0f) ? (100.0f - diff_pct * 10) : 0.0f;
    
    /* Quality assessment */
    stats->stable = true;  /* Can't determine from GCR alone */
    stats->valid = (stats->avg_bitrate > 200000 && stats->avg_bitrate < 400000);
    stats->quality_score = stats->density_confidence;
    
    return 0;
}

/**
 * @brief Detect bitrate zones in track
 */
int detect_bitrate_zones(const uint32_t *flux_data, size_t num_samples,
                         uint32_t sample_rate, bitrate_zone_t *zones,
                         int max_zones)
{
    if (!flux_data || !zones || num_samples == 0 || max_zones <= 0) return 0;
    
    int num_zones = 0;
    size_t window_size = num_samples / 100;  /* 1% of track */
    if (window_size < 10) window_size = 10;
    
    uint32_t current_bitrate = 0;
    size_t zone_start = 0;
    
    for (size_t i = 0; i < num_samples && num_zones < max_zones; i += window_size) {
        /* Calculate local bitrate */
        uint64_t window_time = 0;
        size_t count = 0;
        for (size_t j = i; j < i + window_size && j < num_samples; j++) {
            window_time += flux_data[j];
            count++;
        }
        
        uint32_t local_bitrate = 0;
        if (window_time > 0) {
            double time_sec = (double)window_time / sample_rate;
            local_bitrate = (uint32_t)(count / time_sec);
        }
        
        /* Detect zone change */
        int local_density = find_nearest_density(local_bitrate);
        int current_density = find_nearest_density(current_bitrate);
        
        if (current_bitrate == 0 || local_density != current_density) {
            /* Start new zone */
            if (current_bitrate != 0) {
                zones[num_zones].end_pos = (uint32_t)i;
                num_zones++;
            }
            
            if (num_zones < max_zones) {
                zones[num_zones].start_pos = (uint32_t)i;
                zones[num_zones].bitrate = local_bitrate;
                zones[num_zones].density = local_density;
                zones[num_zones].cell_time_ns = (local_bitrate > 0) ? 
                    (1e9f / local_bitrate) : 0;
            }
            
            current_bitrate = local_bitrate;
        }
    }
    
    /* Close last zone */
    if (num_zones < max_zones && current_bitrate != 0) {
        zones[num_zones].end_pos = (uint32_t)num_samples;
        num_zones++;
    }
    
    return num_zones;
}

/* ============================================================================
 * Deep Analysis
 * ============================================================================ */

/**
 * @brief Perform deep bitrate analysis on disk image
 */
int deep_bitrate_analysis(const uint8_t **track_data, const size_t *track_lengths,
                          const uint32_t *track_times, int num_tracks,
                          flux_source_t source, deep_analysis_t *result)
{
    if (!track_data || !track_lengths || !result || num_tracks <= 0) return -1;
    
    memset(result, 0, sizeof(deep_analysis_t));
    result->num_tracks = num_tracks;
    
    float rpm_sum = 0.0f;
    uint64_t bitrate_sum = 0;
    int valid_tracks = 0;
    
    for (int t = 0; t < num_tracks && t < 84; t++) {
        if (!track_data[t] || track_lengths[t] == 0) continue;
        
        uint32_t rev_time = track_times ? track_times[t] : 
                            estimate_revolution_time(t/2 + 1, track_lengths[t]);
        
        analyze_gcr_bitrate(track_data[t], track_lengths[t], rev_time,
                            t/2 + 1, &result->tracks[t]);
        
        if (result->tracks[t].valid) {
            rpm_sum += result->tracks[t].rpm;
            bitrate_sum += result->tracks[t].avg_bitrate;
            valid_tracks++;
            
            /* Check for variable density */
            if (result->tracks[t].num_zones > 1) {
                result->variable_density = true;
            }
        }
    }
    
    if (valid_tracks > 0) {
        result->avg_rpm = rpm_sum / valid_tracks;
        result->avg_bitrate = (uint32_t)(bitrate_sum / valid_tracks);
        result->non_standard_rpm = !rpm_is_standard(result->avg_rpm);
        
        /* Calculate RPM stability */
        float rpm_variance = 0.0f;
        for (int t = 0; t < num_tracks && t < 84; t++) {
            if (result->tracks[t].valid) {
                float diff = result->tracks[t].rpm - result->avg_rpm;
                rpm_variance += diff * diff;
            }
        }
        result->rpm_stability = 100.0f - sqrtf(rpm_variance / valid_tracks);
        if (result->rpm_stability < 0) result->rpm_stability = 0;
    }
    
    /* Generate summary */
    snprintf(result->summary, sizeof(result->summary),
             "%d tracks analyzed, avg %.1f RPM, avg %u bps, %s",
             valid_tracks, result->avg_rpm, result->avg_bitrate,
             result->variable_density ? "variable density detected" : "standard density");
    
    return 0;
}

/**
 * @brief Generate bitrate analysis report
 */
size_t generate_bitrate_report(const deep_analysis_t *analysis,
                               char *buffer, size_t buffer_size)
{
    if (!analysis || !buffer || buffer_size == 0) return 0;
    
    size_t written = snprintf(buffer, buffer_size,
        "=== Deep Bitrate Analysis Report ===\n\n"
        "Tracks analyzed: %d\n"
        "Average RPM: %.2f\n"
        "RPM stability: %.1f%%\n"
        "Average bitrate: %u bps\n"
        "Variable density: %s\n"
        "Non-standard RPM: %s\n"
        "Weak bit tracks: %d\n\n"
        "Summary: %s\n",
        analysis->num_tracks,
        analysis->avg_rpm,
        analysis->rpm_stability,
        analysis->avg_bitrate,
        analysis->variable_density ? "Yes" : "No",
        analysis->non_standard_rpm ? "Yes" : "No",
        analysis->weak_bit_tracks,
        analysis->summary);
    
    return written;
}

/**
 * @brief Generate track alignment report
 */
size_t generate_track_report(const bitrate_stats_t *stats,
                             char *buffer, size_t buffer_size)
{
    if (!stats || !buffer || buffer_size == 0) return 0;
    
    return snprintf(buffer, buffer_size,
        "Track %u (halftrack %u):\n"
        "  Bitrate: %u bps (min %u, max %u)\n"
        "  RPM: %.2f\n"
        "  Detected density: %d\n"
        "  Confidence: %.1f%%\n"
        "  Quality: %.1f%%\n"
        "  Status: %s, %s\n",
        stats->track, stats->halftrack,
        stats->avg_bitrate, stats->min_bitrate, stats->max_bitrate,
        stats->rpm,
        stats->detected_density,
        stats->density_confidence,
        stats->quality_score,
        stats->valid ? "valid" : "invalid",
        stats->stable ? "stable" : "unstable");
}

/* ============================================================================
 * Sync Analysis
 * ============================================================================ */

/**
 * @brief Analyze sync bitrate in track
 */
int analyze_sync_bitrate(const uint8_t *gcr_data, size_t gcr_length,
                         uint32_t revolution_time_ns,
                         uint32_t *sync_bitrate, int *sync_count)
{
    if (!gcr_data || gcr_length == 0) return -1;
    
    int syncs = 0;
    size_t sync_bytes = 0;
    
    /* Count sync bytes */
    for (size_t i = 0; i < gcr_length; i++) {
        if (gcr_data[i] == 0xFF) {
            sync_bytes++;
            if (i == 0 || gcr_data[i-1] != 0xFF) {
                syncs++;
            }
        }
    }
    
    if (sync_count) *sync_count = syncs;
    
    /* Calculate sync bitrate (sync region only) */
    if (sync_bitrate && sync_bytes > 0 && revolution_time_ns > 0) {
        double sync_fraction = (double)sync_bytes / gcr_length;
        double sync_time_ns = revolution_time_ns * sync_fraction;
        if (sync_time_ns > 0) {
            *sync_bitrate = (uint32_t)((sync_bytes * 8) / (sync_time_ns / 1e9));
        }
    }
    
    return 0;
}

/**
 * @brief Check for variable density protection
 */
bool detect_variable_density(const bitrate_stats_t *stats)
{
    if (!stats) return false;
    
    return (stats->num_zones > 1 ||
            (stats->max_bitrate - stats->min_bitrate) > 
             stats->avg_bitrate * 15 / 100);  /* >15% variation */
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Get flux source name
 */
const char *flux_source_name(flux_source_t source)
{
    if (source <= FLUX_SOURCE_RAW) {
        return source_names[source];
    }
    return "Unknown";
}

/**
 * @brief Convert SCP timing to nanoseconds
 */
uint32_t scp_ticks_to_ns(uint32_t scp_ticks)
{
    return scp_ticks * BITRATE_SCP_NS_PER_TICK;
}

/**
 * @brief Convert Kryoflux timing to nanoseconds
 */
uint32_t kryoflux_to_ns(uint32_t kf_sample)
{
    /* Kryoflux sample clock is ~41.6 MHz */
    return (uint32_t)((uint64_t)kf_sample * 1000000000ULL / BITRATE_KRYOFLUX_SCK);
}

/**
 * @brief Estimate revolution time from track data
 */
uint32_t estimate_revolution_time(int track, size_t track_length)
{
    if (track < 1 || track > 42 || track_length == 0) {
        return BITRATE_US_PER_REV * 1000;  /* Default 200ms */
    }
    
    /* Get expected capacity for this track's density */
    int density = track_density[track];
    if (density < 0 || density > 3) density = 3;
    
    size_t expected = track_capacity[density];
    
    /* Scale revolution time based on actual vs expected length */
    float ratio = (float)track_length / expected;
    
    /* Standard revolution is 200ms = 200,000,000 ns */
    return (uint32_t)(BITRATE_US_PER_REV * 1000 * ratio);
}
