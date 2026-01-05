/**
 * @file uft_fdc_analyzer.c
 * @brief FDC Bitstream Analyzer Implementation
 * 
 * EXT3-020: Floppy Disk Controller bitstream analysis
 * 
 * Features:
 * - Raw bitstream analysis
 * - Timing measurement
 * - Sector detection
 * - Error detection
 * - Statistics generation
 */

#include "uft/analysis/uft_fdc_analyzer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* Standard bit cell times (microseconds) */
#define BITCELL_FM_SD       4.0     /* FM Single Density */
#define BITCELL_MFM_DD      2.0     /* MFM Double Density */
#define BITCELL_MFM_HD      1.0     /* MFM High Density */
#define BITCELL_MFM_ED      0.5     /* MFM Extended Density */

/* Tolerance for bit cell timing */
#define TIMING_TOLERANCE    0.25    /* 25% tolerance */

/* Histogram bins */
#define HISTOGRAM_BINS      256
#define HISTOGRAM_BIN_NS    50      /* 50ns per bin */

/*===========================================================================
 * Timing Analysis
 *===========================================================================*/

int uft_fdc_analyze_timing(const double *flux_times, size_t count,
                           uft_fdc_timing_t *timing)
{
    if (!flux_times || !timing || count < 2) return -1;
    
    memset(timing, 0, sizeof(*timing));
    
    /* Calculate intervals */
    double *intervals = malloc((count - 1) * sizeof(double));
    if (!intervals) return -1;
    
    for (size_t i = 0; i < count - 1; i++) {
        intervals[i] = flux_times[i + 1] - flux_times[i];
    }
    
    /* Find min, max, mean */
    timing->min_interval = intervals[0];
    timing->max_interval = intervals[0];
    double sum = 0;
    
    for (size_t i = 0; i < count - 1; i++) {
        if (intervals[i] < timing->min_interval) {
            timing->min_interval = intervals[i];
        }
        if (intervals[i] > timing->max_interval) {
            timing->max_interval = intervals[i];
        }
        sum += intervals[i];
    }
    
    timing->mean_interval = sum / (count - 1);
    
    /* Calculate standard deviation */
    double variance = 0;
    for (size_t i = 0; i < count - 1; i++) {
        double diff = intervals[i] - timing->mean_interval;
        variance += diff * diff;
    }
    timing->std_dev = sqrt(variance / (count - 1));
    
    /* Build histogram */
    timing->histogram = calloc(HISTOGRAM_BINS, sizeof(uint32_t));
    if (timing->histogram) {
        for (size_t i = 0; i < count - 1; i++) {
            int bin = (int)(intervals[i] * 1000000.0 / HISTOGRAM_BIN_NS);
            if (bin >= 0 && bin < HISTOGRAM_BINS) {
                timing->histogram[bin]++;
            }
        }
        timing->histogram_bins = HISTOGRAM_BINS;
    }
    
    /* Detect encoding from timing clusters */
    /* Look for peaks at 1T, 1.5T, 2T intervals */
    double t1 = timing->mean_interval;
    int count_1t = 0, count_15t = 0, count_2t = 0;
    
    for (size_t i = 0; i < count - 1; i++) {
        double ratio = intervals[i] / t1;
        if (ratio > 0.8 && ratio < 1.2) count_1t++;
        else if (ratio > 1.3 && ratio < 1.7) count_15t++;
        else if (ratio > 1.8 && ratio < 2.2) count_2t++;
    }
    
    /* MFM has 1T and 2T, FM has mostly 1T */
    if (count_2t > count_1t / 4) {
        timing->detected_encoding = UFT_ENCODING_MFM;
    } else {
        timing->detected_encoding = UFT_ENCODING_FM;
    }
    
    /* Estimate bit rate */
    timing->estimated_bitrate = 1.0 / timing->mean_interval;
    
    free(intervals);
    return 0;
}

/*===========================================================================
 * Sector Detection
 *===========================================================================*/

/* MFM sync pattern in flux domain */
static const double MFM_SYNC_PATTERN[] = {
    1.5, 1.0, 1.0, 1.5, 1.0, 1.0, 1.5, 1.0  /* A1 pattern ratios */
};

int uft_fdc_detect_sectors(const double *flux_times, size_t count,
                           uft_fdc_sector_t *sectors, size_t max_sectors,
                           size_t *sector_count)
{
    if (!flux_times || !sectors || !sector_count || count < 100) {
        return -1;
    }
    
    *sector_count = 0;
    
    /* Estimate bit cell from timing */
    uft_fdc_timing_t timing;
    if (uft_fdc_analyze_timing(flux_times, count, &timing) != 0) {
        return -1;
    }
    
    double bit_cell = timing.mean_interval;
    
    /* Scan for sync patterns */
    for (size_t i = 0; i < count - 50 && *sector_count < max_sectors; i++) {
        /* Look for A1 sync pattern (3 consecutive) */
        bool found_sync = true;
        int sync_count = 0;
        size_t pos = i;
        
        /* Check for 3x A1 sync */
        for (int s = 0; s < 3 && found_sync; s++) {
            for (int p = 0; p < 8 && pos < count - 1; p++) {
                double interval = flux_times[pos + 1] - flux_times[pos];
                double expected = bit_cell * MFM_SYNC_PATTERN[p];
                double tolerance = expected * TIMING_TOLERANCE;
                
                if (fabs(interval - expected) > tolerance) {
                    found_sync = false;
                    break;
                }
                pos++;
            }
            if (found_sync) sync_count++;
        }
        
        if (sync_count >= 3) {
            /* Found potential sector */
            uft_fdc_sector_t *sec = &sectors[*sector_count];
            memset(sec, 0, sizeof(*sec));
            
            sec->sync_offset = i;
            sec->data_offset = pos;
            
            /* Try to decode header */
            /* For now just mark position */
            sec->valid_header = true;
            
            (*sector_count)++;
            
            /* Skip past this sector */
            i = pos + 500;  /* Approximate sector size in transitions */
        }
    }
    
    free(timing.histogram);
    return 0;
}

/*===========================================================================
 * Error Detection
 *===========================================================================*/

int uft_fdc_detect_errors(const double *flux_times, size_t count,
                          uft_fdc_error_t *errors, size_t max_errors,
                          size_t *error_count)
{
    if (!flux_times || !errors || !error_count || count < 10) {
        return -1;
    }
    
    *error_count = 0;
    
    /* Analyze timing */
    uft_fdc_timing_t timing;
    if (uft_fdc_analyze_timing(flux_times, count, &timing) != 0) {
        return -1;
    }
    
    double bit_cell = timing.mean_interval;
    double tolerance = bit_cell * TIMING_TOLERANCE;
    
    /* Scan for anomalies */
    for (size_t i = 0; i < count - 1 && *error_count < max_errors; i++) {
        double interval = flux_times[i + 1] - flux_times[i];
        
        /* Check for timing errors */
        bool is_valid = false;
        for (double mult = 1.0; mult <= 3.0; mult += 0.5) {
            double expected = bit_cell * mult;
            if (fabs(interval - expected) <= tolerance) {
                is_valid = true;
                break;
            }
        }
        
        if (!is_valid) {
            uft_fdc_error_t *err = &errors[*error_count];
            err->offset = i;
            err->flux_time = flux_times[i];
            err->interval = interval;
            err->expected = bit_cell;
            
            if (interval < bit_cell * 0.5) {
                err->type = UFT_FDC_ERR_SHORT_PULSE;
            } else if (interval > bit_cell * 3.5) {
                err->type = UFT_FDC_ERR_LONG_PULSE;
            } else {
                err->type = UFT_FDC_ERR_TIMING;
            }
            
            (*error_count)++;
        }
        
        /* Check for missing flux (weak bit area) */
        if (interval > bit_cell * 4.0 && *error_count < max_errors) {
            uft_fdc_error_t *err = &errors[*error_count];
            err->offset = i;
            err->flux_time = flux_times[i];
            err->interval = interval;
            err->expected = bit_cell;
            err->type = UFT_FDC_ERR_MISSING_FLUX;
            (*error_count)++;
        }
    }
    
    free(timing.histogram);
    return 0;
}

/*===========================================================================
 * Weak Bit Detection
 *===========================================================================*/

int uft_fdc_detect_weak_bits(const double *flux_rev1, size_t count1,
                             const double *flux_rev2, size_t count2,
                             uft_fdc_weak_t *weak_regions, size_t max_regions,
                             size_t *region_count)
{
    if (!flux_rev1 || !flux_rev2 || !weak_regions || !region_count) {
        return -1;
    }
    
    *region_count = 0;
    
    /* Compare two revolutions to find inconsistencies */
    size_t pos1 = 0, pos2 = 0;
    double time1 = 0, time2 = 0;
    
    /* Resync at start */
    /* Find first major flux transition */
    
    while (pos1 < count1 && pos2 < count2 && *region_count < max_regions) {
        double interval1 = (pos1 < count1 - 1) ? 
                          flux_rev1[pos1 + 1] - flux_rev1[pos1] : 0;
        double interval2 = (pos2 < count2 - 1) ? 
                          flux_rev2[pos2 + 1] - flux_rev2[pos2] : 0;
        
        /* Check for mismatch */
        double diff = fabs(interval1 - interval2);
        double threshold = (interval1 + interval2) / 2 * 0.3;  /* 30% difference */
        
        if (diff > threshold && interval1 > 0 && interval2 > 0) {
            /* Found weak bit region */
            uft_fdc_weak_t *weak = &weak_regions[*region_count];
            weak->start_offset = pos1;
            weak->rev1_interval = interval1;
            weak->rev2_interval = interval2;
            weak->confidence = 1.0 - (diff / (interval1 + interval2));
            
            /* Find extent of weak region */
            size_t extent = 0;
            while (pos1 + extent < count1 - 1 && 
                   pos2 + extent < count2 - 1 && extent < 100) {
                double int1 = flux_rev1[pos1 + extent + 1] - flux_rev1[pos1 + extent];
                double int2 = flux_rev2[pos2 + extent + 1] - flux_rev2[pos2 + extent];
                double d = fabs(int1 - int2);
                double t = (int1 + int2) / 2 * 0.3;
                
                if (d <= t) break;
                extent++;
            }
            
            weak->length = extent + 1;
            (*region_count)++;
            
            pos1 += extent + 1;
            pos2 += extent + 1;
        } else {
            pos1++;
            pos2++;
        }
        
        time1 += interval1;
        time2 += interval2;
        
        /* Resync if positions drift too far apart */
        if (fabs(time1 - time2) > 0.001) {  /* 1ms drift */
            if (time1 > time2) pos2++;
            else pos1++;
        }
    }
    
    return 0;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

int uft_fdc_generate_stats(const double *flux_times, size_t count,
                           uft_fdc_stats_t *stats)
{
    if (!flux_times || !stats || count < 2) return -1;
    
    memset(stats, 0, sizeof(*stats));
    
    /* Basic timing */
    uft_fdc_timing_t timing;
    if (uft_fdc_analyze_timing(flux_times, count, &timing) != 0) {
        return -1;
    }
    
    stats->total_flux = count;
    stats->track_time = flux_times[count - 1] - flux_times[0];
    stats->mean_interval = timing.mean_interval;
    stats->std_dev = timing.std_dev;
    stats->detected_encoding = timing.detected_encoding;
    stats->estimated_rpm = 60.0 / stats->track_time;
    
    /* Detect sectors */
    uft_fdc_sector_t sectors[50];
    size_t sector_count;
    if (uft_fdc_detect_sectors(flux_times, count, sectors, 50, &sector_count) == 0) {
        stats->sector_count = sector_count;
    }
    
    /* Detect errors */
    uft_fdc_error_t errors[1000];
    size_t error_count;
    if (uft_fdc_detect_errors(flux_times, count, errors, 1000, &error_count) == 0) {
        stats->error_count = error_count;
        
        /* Categorize errors */
        for (size_t i = 0; i < error_count; i++) {
            switch (errors[i].type) {
                case UFT_FDC_ERR_SHORT_PULSE:
                    stats->short_pulses++;
                    break;
                case UFT_FDC_ERR_LONG_PULSE:
                    stats->long_pulses++;
                    break;
                case UFT_FDC_ERR_MISSING_FLUX:
                    stats->missing_flux++;
                    break;
                default:
                    stats->timing_errors++;
                    break;
            }
        }
    }
    
    /* Estimate data rate */
    if (timing.detected_encoding == UFT_ENCODING_MFM) {
        stats->data_rate_kbps = stats->estimated_rpm * count / 60.0 / 1000.0 / 2;
    } else {
        stats->data_rate_kbps = stats->estimated_rpm * count / 60.0 / 1000.0;
    }
    
    free(timing.histogram);
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_fdc_report_json(const uft_fdc_stats_t *stats, char *buffer, size_t size)
{
    if (!stats || !buffer || size == 0) return -1;
    
    const char *encoding = (stats->detected_encoding == UFT_ENCODING_FM) ? 
                           "FM" : "MFM";
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"total_flux\": %zu,\n"
        "  \"track_time_ms\": %.3f,\n"
        "  \"estimated_rpm\": %.1f,\n"
        "  \"encoding\": \"%s\",\n"
        "  \"data_rate_kbps\": %.1f,\n"
        "  \"mean_interval_us\": %.3f,\n"
        "  \"std_dev_us\": %.3f,\n"
        "  \"sector_count\": %zu,\n"
        "  \"error_count\": %zu,\n"
        "  \"short_pulses\": %zu,\n"
        "  \"long_pulses\": %zu,\n"
        "  \"missing_flux\": %zu,\n"
        "  \"timing_errors\": %zu\n"
        "}",
        stats->total_flux,
        stats->track_time * 1000.0,
        stats->estimated_rpm,
        encoding,
        stats->data_rate_kbps,
        stats->mean_interval * 1000000.0,
        stats->std_dev * 1000000.0,
        stats->sector_count,
        stats->error_count,
        stats->short_pulses,
        stats->long_pulses,
        stats->missing_flux,
        stats->timing_errors
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
