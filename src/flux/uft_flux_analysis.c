/**
 * @file uft_flux_analysis.c
 * @brief Flux Timing Analysis Module Implementation
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/flux/uft_flux_analysis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Static Data
 * ============================================================================ */

/** Encoding names */
static const char *encoding_names[] = {
    "Unknown",
    "FM",
    "MFM",
    "GCR (C64)",
    "GCR (Apple)",
    "Amiga MFM",
    "Raw"
};

/** Source names */
static const char *source_names[] = {
    "Unknown",
    "Kryoflux",
    "SuperCard Pro",
    "Greaseweazle",
    "HxC",
    "Applesauce"
};

/** Expected cell times (ns) */
static const uint32_t cell_times[] = {
    0,      /* Unknown */
    4000,   /* FM */
    2000,   /* MFM */
    3250,   /* GCR C64 */
    4000,   /* GCR Apple */
    2000,   /* Amiga */
    0       /* Raw */
};

/* ============================================================================
 * Transition Management
 * ============================================================================ */

/**
 * @brief Create transitions structure
 */
flux_transitions_t *flux_create_transitions(uint32_t sample_rate, flux_source_t source)
{
    flux_transitions_t *trans = calloc(1, sizeof(flux_transitions_t));
    if (!trans) return NULL;
    
    trans->sample_rate = sample_rate;
    trans->source = source;
    trans->capacity = 1024;
    trans->times = malloc(trans->capacity * sizeof(uint32_t));
    
    if (!trans->times) {
        free(trans);
        return NULL;
    }
    
    return trans;
}

/**
 * @brief Free transitions
 */
void flux_free_transitions(flux_transitions_t *trans)
{
    if (!trans) return;
    free(trans->times);
    free(trans);
}

/**
 * @brief Add transition
 */
int flux_add_transition(flux_transitions_t *trans, uint32_t time)
{
    if (!trans) return -1;
    
    /* Grow if needed */
    if (trans->num_transitions >= trans->capacity) {
        size_t new_cap = trans->capacity * 2;
        uint32_t *new_times = realloc(trans->times, new_cap * sizeof(uint32_t));
        if (!new_times) return -2;
        
        trans->times = new_times;
        trans->capacity = new_cap;
    }
    
    trans->times[trans->num_transitions++] = time;
    return 0;
}

/**
 * @brief Load from raw data
 */
int flux_load_raw(const uint8_t *data, size_t size, flux_source_t source,
                  flux_transitions_t **trans)
{
    if (!data || !trans || size == 0) return -1;
    
    uint32_t sample_rate;
    switch (source) {
        case FLUX_SOURCE_KRYOFLUX:
            sample_rate = FLUX_SAMPLE_RATE_KRYOFLUX;
            break;
        case FLUX_SOURCE_SCP:
            sample_rate = FLUX_SAMPLE_RATE_SCP;
            break;
        case FLUX_SOURCE_GREASEWEAZLE:
            sample_rate = FLUX_SAMPLE_RATE_GW;
            break;
        default:
            sample_rate = FLUX_SAMPLE_RATE_SCP;
    }
    
    flux_transitions_t *t = flux_create_transitions(sample_rate, source);
    if (!t) return -2;
    
    /* Parse flux data based on source format */
    /* Simplified: treat each byte as timing value */
    uint32_t cumulative = 0;
    
    for (size_t i = 0; i < size; i++) {
        uint32_t delta = data[i];
        
        /* Handle overflow for SCP/KF formats */
        if (delta == 0xFF) {
            cumulative += 255;
            continue;
        }
        
        cumulative += delta;
        flux_add_transition(t, cumulative);
    }
    
    *trans = t;
    return 0;
}

/* ============================================================================
 * Basic Analysis
 * ============================================================================ */

/**
 * @brief Calculate cell statistics
 */
int flux_calc_cell_stats(const flux_transitions_t *trans, flux_encoding_t encoding,
                         flux_cell_stats_t *stats)
{
    if (!trans || !stats || trans->num_transitions < 2) return -1;
    
    memset(stats, 0, sizeof(flux_cell_stats_t));
    
    uint32_t expected_cell = cell_times[encoding];
    if (expected_cell == 0) expected_cell = 2000;  /* Default to MFM */
    
    /* Calculate deltas */
    double sum = 0;
    double sum_sq = 0;
    uint32_t min_delta = 0xFFFFFFFF;
    uint32_t max_delta = 0;
    int count = 0;
    int outliers = 0;
    
    for (size_t i = 1; i < trans->num_transitions; i++) {
        uint32_t delta = trans->times[i] - trans->times[i - 1];
        uint32_t delta_ns = flux_samples_to_ns(delta, trans->sample_rate);
        
        /* Skip outliers (> 5x expected) */
        if (delta_ns > expected_cell * 5) {
            outliers++;
            continue;
        }
        
        sum += delta_ns;
        sum_sq += (double)delta_ns * delta_ns;
        count++;
        
        if (delta_ns < min_delta) min_delta = delta_ns;
        if (delta_ns > max_delta) max_delta = delta_ns;
    }
    
    if (count == 0) return -2;
    
    stats->mean_ns = sum / count;
    stats->stddev_ns = sqrt((sum_sq / count) - (stats->mean_ns * stats->mean_ns));
    stats->jitter_percent = (stats->stddev_ns / stats->mean_ns) * 100.0f;
    stats->sample_count = count;
    stats->min_ns = min_delta;
    stats->max_ns = max_delta;
    stats->outliers = outliers;
    
    return 0;
}

/**
 * @brief Generate histogram
 */
int flux_generate_histogram(const flux_transitions_t *trans, flux_histogram_t *histogram)
{
    if (!trans || !histogram || trans->num_transitions < 2) return -1;
    
    memset(histogram, 0, sizeof(flux_histogram_t));
    
    uint32_t ns_per_bin = FLUX_HISTOGRAM_MAX_NS / FLUX_HISTOGRAM_BINS;
    histogram->min_time_ns = 0xFFFFFFFF;
    histogram->max_time_ns = 0;
    
    for (size_t i = 1; i < trans->num_transitions; i++) {
        uint32_t delta = trans->times[i] - trans->times[i - 1];
        uint32_t delta_ns = flux_samples_to_ns(delta, trans->sample_rate);
        
        if (delta_ns < histogram->min_time_ns) histogram->min_time_ns = delta_ns;
        if (delta_ns > histogram->max_time_ns) histogram->max_time_ns = delta_ns;
        
        int bin = delta_ns / ns_per_bin;
        if (bin >= FLUX_HISTOGRAM_BINS) bin = FLUX_HISTOGRAM_BINS - 1;
        
        histogram->bins[bin]++;
        histogram->total_samples++;
    }
    
    return 0;
}

/**
 * @brief Find histogram peaks
 */
int flux_find_histogram_peaks(flux_histogram_t *histogram, int max_peaks)
{
    if (!histogram || max_peaks <= 0) return 0;
    
    histogram->num_peaks = 0;
    
    /* Simple peak detection: look for local maxima */
    for (int i = 1; i < FLUX_HISTOGRAM_BINS - 1 && histogram->num_peaks < max_peaks; i++) {
        if (histogram->bins[i] > histogram->bins[i - 1] &&
            histogram->bins[i] > histogram->bins[i + 1] &&
            histogram->bins[i] > histogram->total_samples / 50) {  /* 2% threshold */
            
            histogram->peak_bins[histogram->num_peaks++] = i;
        }
    }
    
    return histogram->num_peaks;
}

/**
 * @brief Detect encoding
 */
flux_encoding_t flux_detect_encoding(const flux_transitions_t *trans)
{
    if (!trans || trans->num_transitions < 100) return FLUX_ENCODING_UNKNOWN;
    
    /* Generate histogram */
    flux_histogram_t hist;
    flux_generate_histogram(trans, &hist);
    flux_find_histogram_peaks(&hist, 4);
    
    if (hist.num_peaks == 0) return FLUX_ENCODING_UNKNOWN;
    
    /* Calculate dominant timing */
    uint32_t ns_per_bin = FLUX_HISTOGRAM_MAX_NS / FLUX_HISTOGRAM_BINS;
    uint32_t dominant_ns = hist.peak_bins[0] * ns_per_bin + ns_per_bin / 2;
    
    /* Match to encoding */
    if (dominant_ns >= 1800 && dominant_ns <= 2200) {
        return FLUX_ENCODING_MFM;
    } else if (dominant_ns >= 3000 && dominant_ns <= 3500) {
        return FLUX_ENCODING_GCR_C64;
    } else if (dominant_ns >= 3800 && dominant_ns <= 4200) {
        /* Could be FM or Apple GCR */
        return (hist.num_peaks >= 3) ? FLUX_ENCODING_FM : FLUX_ENCODING_GCR_APPLE;
    }
    
    return FLUX_ENCODING_UNKNOWN;
}

/* ============================================================================
 * Revolution Analysis
 * ============================================================================ */

/**
 * @brief Find revolutions
 */
int flux_find_revolutions(const flux_transitions_t *trans,
                          flux_revolution_t *revolutions, int max_revolutions)
{
    if (!trans || !revolutions || max_revolutions <= 0) return 0;
    if (trans->num_transitions < 1000) return 0;
    
    /* Expected revolution duration: ~200ms at 300 RPM */
    uint32_t expected_rev_ns = 200000000;  /* 200ms */
    uint32_t expected_rev_samples = flux_ns_to_samples(expected_rev_ns, trans->sample_rate);
    
    int num_revs = 0;
    size_t rev_start = 0;
    uint32_t start_time = 0;
    
    for (size_t i = 1; i < trans->num_transitions && num_revs < max_revolutions; i++) {
        uint32_t duration = trans->times[i] - start_time;
        
        /* Check if we've accumulated roughly one revolution */
        if (duration >= expected_rev_samples * 0.95 && duration <= expected_rev_samples * 1.05) {
            revolutions[num_revs].start_index = rev_start;
            revolutions[num_revs].num_transitions = i - rev_start;
            revolutions[num_revs].duration_ns = flux_samples_to_ns(duration, trans->sample_rate);
            revolutions[num_revs].rpm = 60000000000.0f / revolutions[num_revs].duration_ns;
            
            num_revs++;
            rev_start = i;
            start_time = trans->times[i];
        }
    }
    
    return num_revs;
}

/**
 * @brief Calculate RPM
 */
float flux_calc_rpm(const flux_revolution_t *rev, uint32_t sample_rate)
{
    (void)sample_rate;
    
    if (!rev || rev->duration_ns == 0) return 0;
    return 60000000000.0f / rev->duration_ns;
}

/**
 * @brief Analyze speed
 */
int flux_analyze_speed(const flux_revolution_t *revolutions, int num_revolutions,
                       float *mean_rpm, float *variation)
{
    if (!revolutions || num_revolutions == 0 || !mean_rpm || !variation) return -1;
    
    float sum = 0;
    float min_rpm = 999.0f;
    float max_rpm = 0;
    
    for (int i = 0; i < num_revolutions; i++) {
        float rpm = revolutions[i].rpm;
        sum += rpm;
        if (rpm < min_rpm) min_rpm = rpm;
        if (rpm > max_rpm) max_rpm = rpm;
    }
    
    *mean_rpm = sum / num_revolutions;
    *variation = ((max_rpm - min_rpm) / *mean_rpm) * 100.0f;
    
    return 0;
}

/* ============================================================================
 * Track Analysis
 * ============================================================================ */

/**
 * @brief Analyze track
 */
int flux_analyze_track(const flux_transitions_t *trans, int track, int side,
                       flux_track_analysis_t *analysis)
{
    if (!trans || !analysis) return -1;
    
    memset(analysis, 0, sizeof(flux_track_analysis_t));
    analysis->track = track;
    analysis->side = side;
    
    /* Detect encoding */
    analysis->encoding = flux_detect_encoding(trans);
    
    /* Calculate cell statistics */
    flux_calc_cell_stats(trans, analysis->encoding, &analysis->cell_stats);
    
    /* Generate histogram */
    flux_generate_histogram(trans, &analysis->histogram);
    flux_find_histogram_peaks(&analysis->histogram, 4);
    
    /* Find revolutions */
    analysis->num_revolutions = flux_find_revolutions(trans, analysis->revolutions, 16);
    
    /* Analyze speed */
    if (analysis->num_revolutions > 0) {
        flux_analyze_speed(analysis->revolutions, analysis->num_revolutions,
                           &analysis->rpm_mean, &analysis->speed_variation);
        
        /* Calculate RPM stddev */
        float sum_sq = 0;
        for (int i = 0; i < analysis->num_revolutions; i++) {
            float diff = analysis->revolutions[i].rpm - analysis->rpm_mean;
            sum_sq += diff * diff;
        }
        analysis->rpm_stddev = sqrt(sum_sq / analysis->num_revolutions);
    }
    
    /* Calculate signal quality */
    analysis->signal_quality = 100.0f - analysis->cell_stats.jitter_percent * 2;
    if (analysis->signal_quality < 0) analysis->signal_quality = 0;
    
    /* Find weak bits */
    analysis->weak_bits = flux_find_weak_bits(trans, 20, NULL);
    
    /* Find no-flux regions */
    int no_flux_regions = flux_find_no_flux(trans, 50000, NULL, 0);  /* 50µs */
    analysis->has_no_flux = (no_flux_regions > 0);
    
    /* Check for anomalies */
    int anomalies;
    analysis->has_timing_anomaly = flux_detect_anomalies(trans, analysis->encoding, &anomalies);
    
    /* Check track length */
    if (trans->num_transitions > 0) {
        uint32_t track_time = trans->times[trans->num_transitions - 1] - trans->times[0];
        uint32_t track_time_ns = flux_samples_to_ns(track_time, trans->sample_rate);
        
        /* Expected: ~200ms per revolution */
        analysis->has_long_track = (track_time_ns > 210000000);
        analysis->has_short_track = (track_time_ns < 190000000);
    }
    
    /* Generate description */
    snprintf(analysis->description, sizeof(analysis->description),
             "Track %d.%d: %s, %.1f RPM, %.1f%% jitter, quality %.0f%%",
             track, side, flux_encoding_name(analysis->encoding),
             analysis->rpm_mean, analysis->cell_stats.jitter_percent,
             analysis->signal_quality);
    
    return 0;
}

/**
 * @brief Find weak bits
 */
int flux_find_weak_bits(const flux_transitions_t *trans, int threshold, int *regions)
{
    if (!trans || trans->num_transitions < 10) return 0;
    
    int weak_count = 0;
    int region_count = 0;
    bool in_weak = false;
    
    /* Calculate mean interval */
    double sum = 0;
    for (size_t i = 1; i < trans->num_transitions; i++) {
        sum += (trans->times[i] - trans->times[i - 1]);
    }
    double mean = sum / (trans->num_transitions - 1);
    
    /* Find intervals significantly different from mean */
    for (size_t i = 1; i < trans->num_transitions; i++) {
        uint32_t delta = trans->times[i] - trans->times[i - 1];
        double deviation = fabs(delta - mean) / mean * 100.0;
        
        if (deviation > threshold) {
            weak_count++;
            if (!in_weak) {
                region_count++;
                in_weak = true;
            }
        } else {
            in_weak = false;
        }
    }
    
    if (regions) *regions = region_count;
    return weak_count;
}

/**
 * @brief Find no-flux regions
 */
int flux_find_no_flux(const flux_transitions_t *trans, uint32_t min_gap_ns,
                      size_t *positions, int max_positions)
{
    if (!trans || trans->num_transitions < 2) return 0;
    
    int count = 0;
    
    for (size_t i = 1; i < trans->num_transitions; i++) {
        uint32_t delta = trans->times[i] - trans->times[i - 1];
        uint32_t delta_ns = flux_samples_to_ns(delta, trans->sample_rate);
        
        if (delta_ns >= min_gap_ns) {
            if (positions && count < max_positions) {
                positions[count] = i - 1;
            }
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Detect anomalies
 */
bool flux_detect_anomalies(const flux_transitions_t *trans, flux_encoding_t encoding,
                           int *anomalies)
{
    if (!trans || trans->num_transitions < 100) return false;
    
    uint32_t expected = cell_times[encoding];
    if (expected == 0) expected = 2000;
    
    int count = 0;
    
    for (size_t i = 1; i < trans->num_transitions; i++) {
        uint32_t delta = trans->times[i] - trans->times[i - 1];
        uint32_t delta_ns = flux_samples_to_ns(delta, trans->sample_rate);
        
        /* Check for timing outside 50-150% of expected */
        if (delta_ns < expected / 2 || delta_ns > expected * 3) {
            count++;
        }
    }
    
    if (anomalies) *anomalies = count;
    
    /* Significant if more than 1% are anomalies */
    return (count > (int)trans->num_transitions / 100);
}

/* ============================================================================
 * Disk Analysis
 * ============================================================================ */

/**
 * @brief Create disk analysis
 */
flux_disk_analysis_t *flux_create_disk_analysis(int num_tracks, int num_sides)
{
    flux_disk_analysis_t *analysis = calloc(1, sizeof(flux_disk_analysis_t));
    if (!analysis) return NULL;
    
    analysis->num_tracks = num_tracks;
    analysis->num_sides = num_sides;
    
    analysis->tracks = calloc(num_tracks * num_sides, sizeof(flux_track_analysis_t));
    if (!analysis->tracks) {
        free(analysis);
        return NULL;
    }
    
    return analysis;
}

/**
 * @brief Free disk analysis
 */
void flux_free_disk_analysis(flux_disk_analysis_t *analysis)
{
    if (!analysis) return;
    free(analysis->tracks);
    free(analysis);
}

/**
 * @brief Analyze disk
 */
int flux_analyze_disk(flux_transitions_t **track_trans, int num_tracks, int num_sides,
                      flux_disk_analysis_t *analysis)
{
    if (!track_trans || !analysis) return -1;
    
    analysis->num_tracks = num_tracks;
    analysis->num_sides = num_sides;
    
    float rpm_sum = 0;
    float jitter_sum = 0;
    float quality_sum = 0;
    int analyzed = 0;
    
    for (int t = 0; t < num_tracks; t++) {
        for (int s = 0; s < num_sides; s++) {
            int idx = t * num_sides + s;
            
            if (track_trans[idx]) {
                flux_analyze_track(track_trans[idx], t, s, &analysis->tracks[idx]);
                
                rpm_sum += analysis->tracks[idx].rpm_mean;
                jitter_sum += analysis->tracks[idx].cell_stats.jitter_percent;
                quality_sum += analysis->tracks[idx].signal_quality;
                analyzed++;
                
                /* Check for protection indicators */
                if (analysis->tracks[idx].has_long_track ||
                    analysis->tracks[idx].has_weak_region ||
                    analysis->tracks[idx].has_timing_anomaly) {
                    analysis->protection_tracks++;
                }
            }
        }
    }
    
    if (analyzed > 0) {
        analysis->avg_rpm = rpm_sum / analyzed;
        analysis->avg_jitter = jitter_sum / analyzed;
        analysis->signal_quality = quality_sum / analyzed;
    }
    
    analysis->has_protections = (analysis->protection_tracks > 0);
    
    /* Determine primary encoding */
    int encoding_counts[7] = {0};
    for (int i = 0; i < num_tracks * num_sides; i++) {
        if (analysis->tracks[i].encoding < 7) {
            encoding_counts[analysis->tracks[i].encoding]++;
        }
    }
    
    int max_count = 0;
    for (int e = 0; e < 7; e++) {
        if (encoding_counts[e] > max_count) {
            max_count = encoding_counts[e];
            analysis->encoding = e;
        }
    }
    
    /* Generate summary */
    snprintf(analysis->summary, sizeof(analysis->summary),
             "Disk: %d tracks, %d sides, %s encoding\n"
             "Average RPM: %.1f, Jitter: %.1f%%, Quality: %.0f%%\n"
             "Protection tracks: %d",
             num_tracks, num_sides, flux_encoding_name(analysis->encoding),
             analysis->avg_rpm, analysis->avg_jitter, analysis->signal_quality,
             analysis->protection_tracks);
    
    return 0;
}

/**
 * @brief Generate report
 */
int flux_generate_report(const flux_disk_analysis_t *analysis,
                         char *buffer, size_t buffer_size)
{
    if (!analysis || !buffer || buffer_size == 0) return 0;
    
    int written = 0;
    
    written += snprintf(buffer + written, buffer_size - written,
                        "=== Flux Analysis Report ===\n\n");
    
    written += snprintf(buffer + written, buffer_size - written,
                        "Disk Summary:\n%s\n\n", analysis->summary);
    
    written += snprintf(buffer + written, buffer_size - written,
                        "Track Details:\n");
    
    for (int t = 0; t < analysis->num_tracks && written < (int)buffer_size - 100; t++) {
        for (int s = 0; s < analysis->num_sides; s++) {
            int idx = t * analysis->num_sides + s;
            const flux_track_analysis_t *trk = &analysis->tracks[idx];
            
            written += snprintf(buffer + written, buffer_size - written,
                                "  %s\n", trk->description);
        }
    }
    
    return written;
}

/* ============================================================================
 * Protection Detection
 * ============================================================================ */

/**
 * @brief Detect protection via flux
 */
bool flux_detect_protection(const flux_track_analysis_t *analysis,
                            char *description, size_t desc_size)
{
    if (!analysis) return false;
    
    bool detected = false;
    const char *reason = NULL;
    
    if (analysis->has_long_track) {
        detected = true;
        reason = "Long track";
    } else if (analysis->has_weak_region) {
        detected = true;
        reason = "Weak bits";
    } else if (analysis->has_no_flux) {
        detected = true;
        reason = "No-flux region";
    } else if (analysis->has_timing_anomaly) {
        detected = true;
        reason = "Timing anomaly";
    } else if (analysis->has_density_change) {
        detected = true;
        reason = "Density change";
    }
    
    if (detected && description && desc_size > 0) {
        snprintf(description, desc_size, "Protection detected: %s on track %d",
                 reason, analysis->track);
    }
    
    return detected;
}

/**
 * @brief Check track length
 */
int flux_check_track_length(const flux_transitions_t *trans,
                            uint32_t expected_length_ns, float tolerance_percent)
{
    if (!trans || trans->num_transitions < 2) return 0;
    
    uint32_t actual_ns = flux_samples_to_ns(
        trans->times[trans->num_transitions - 1] - trans->times[0],
        trans->sample_rate);
    
    int diff_ns = (int)actual_ns - (int)expected_length_ns;
    int tolerance_ns = (int)(expected_length_ns * tolerance_percent / 100);
    
    if (abs(diff_ns) > tolerance_ns) {
        return diff_ns;
    }
    
    return 0;
}

/**
 * @brief Check density protection
 */
bool flux_check_density_protection(const flux_transitions_t *trans,
                                   flux_encoding_t encoding, int *num_changes)
{
    if (!trans || trans->num_transitions < 100) return false;
    
    uint32_t expected = cell_times[encoding];
    if (expected == 0) return false;
    
    int changes = 0;
    uint32_t last_density = expected;
    
    /* Check density in windows */
    size_t window_size = trans->num_transitions / 20;
    
    for (size_t start = 0; start < trans->num_transitions - window_size; start += window_size) {
        uint32_t sum = 0;
        for (size_t i = start + 1; i < start + window_size; i++) {
            sum += trans->times[i] - trans->times[i - 1];
        }
        
        uint32_t avg_ns = flux_samples_to_ns(sum / window_size, trans->sample_rate);
        
        /* Check for significant density change */
        if (abs((int)avg_ns - (int)last_density) > (int)expected / 4) {
            changes++;
            last_density = avg_ns;
        }
    }
    
    if (num_changes) *num_changes = changes;
    return (changes > 2);
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Samples to nanoseconds
 */
uint32_t flux_samples_to_ns(uint32_t samples, uint32_t sample_rate)
{
    if (sample_rate == 0) return 0;
    return (uint32_t)((uint64_t)samples * 1000000000ULL / sample_rate);
}

/**
 * @brief Nanoseconds to samples
 */
uint32_t flux_ns_to_samples(uint32_t ns, uint32_t sample_rate)
{
    return (uint32_t)((uint64_t)ns * sample_rate / 1000000000ULL);
}

/**
 * @brief Get encoding name
 */
const char *flux_encoding_name(flux_encoding_t encoding)
{
    if (encoding < sizeof(encoding_names) / sizeof(encoding_names[0])) {
        return encoding_names[encoding];
    }
    return encoding_names[0];
}

/**
 * @brief Get source name
 */
const char *flux_source_name(flux_source_t source)
{
    if (source < sizeof(source_names) / sizeof(source_names[0])) {
        return source_names[source];
    }
    return source_names[0];
}

/**
 * @brief Get expected cell time
 */
uint32_t flux_expected_cell_time(flux_encoding_t encoding)
{
    if (encoding < sizeof(cell_times) / sizeof(cell_times[0])) {
        return cell_times[encoding];
    }
    return 0;
}

/**
 * @brief Print histogram
 */
void flux_print_histogram(const flux_histogram_t *histogram, FILE *fp)
{
    if (!histogram || !fp) return;
    
    fprintf(fp, "\nFlux Timing Histogram:\n");
    fprintf(fp, "Min: %u ns, Max: %u ns, Total: %u samples\n\n",
            histogram->min_time_ns, histogram->max_time_ns, histogram->total_samples);
    
    /* Find max bin for scaling */
    uint32_t max_bin = 0;
    for (int i = 0; i < FLUX_HISTOGRAM_BINS; i++) {
        if (histogram->bins[i] > max_bin) max_bin = histogram->bins[i];
    }
    
    uint32_t ns_per_bin = FLUX_HISTOGRAM_MAX_NS / FLUX_HISTOGRAM_BINS;
    
    /* Print only non-empty bins */
    for (int i = 0; i < FLUX_HISTOGRAM_BINS; i++) {
        if (histogram->bins[i] > max_bin / 100) {  /* 1% threshold */
            int bar_len = (int)(histogram->bins[i] * 50 / max_bin);
            fprintf(fp, "%5u ns: %6u |", i * ns_per_bin, histogram->bins[i]);
            for (int j = 0; j < bar_len; j++) fprintf(fp, "#");
            fprintf(fp, "\n");
        }
    }
}

/**
 * @brief Print track analysis
 */
void flux_print_track_analysis(const flux_track_analysis_t *analysis, FILE *fp)
{
    if (!analysis || !fp) return;
    
    fprintf(fp, "\n=== Track %d.%d Analysis ===\n", analysis->track, analysis->side);
    fprintf(fp, "Encoding: %s\n", flux_encoding_name(analysis->encoding));
    fprintf(fp, "Revolutions: %d\n", analysis->num_revolutions);
    fprintf(fp, "RPM: %.2f (stddev: %.2f, variation: %.2f%%)\n",
            analysis->rpm_mean, analysis->rpm_stddev, analysis->speed_variation);
    fprintf(fp, "Cell Stats: mean=%.1f ns, stddev=%.1f ns, jitter=%.1f%%\n",
            analysis->cell_stats.mean_ns, analysis->cell_stats.stddev_ns,
            analysis->cell_stats.jitter_percent);
    fprintf(fp, "Signal Quality: %.0f%%\n", analysis->signal_quality);
    fprintf(fp, "Weak bits: %d, Missing clocks: %d, Extra clocks: %d\n",
            analysis->weak_bits, analysis->missing_clocks, analysis->extra_clocks);
    
    if (analysis->has_long_track) fprintf(fp, "  [!] Long track\n");
    if (analysis->has_short_track) fprintf(fp, "  [!] Short track\n");
    if (analysis->has_density_change) fprintf(fp, "  [!] Density change\n");
    if (analysis->has_weak_region) fprintf(fp, "  [!] Weak region\n");
    if (analysis->has_no_flux) fprintf(fp, "  [!] No-flux region\n");
    if (analysis->has_timing_anomaly) fprintf(fp, "  [!] Timing anomaly\n");
}
