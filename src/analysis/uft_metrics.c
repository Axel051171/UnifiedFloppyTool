/**
 * @file uft_metrics.c
 * @brief Disk Analysis Metrics Implementation
 * 
 * EXT4-006: Quality metrics for disk analysis
 * 
 * Features:
 * - Read quality scoring
 * - Sector reliability metrics
 * - Protection detection confidence
 * - Flux quality analysis
 * - Multi-revolution comparison
 */

#include "uft/analysis/uft_metrics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define QUALITY_EXCELLENT   95
#define QUALITY_GOOD        80
#define QUALITY_FAIR        60
#define QUALITY_POOR        40

/*===========================================================================
 * Flux Quality Metrics
 *===========================================================================*/

int uft_metrics_flux_quality(const uint32_t *flux_times, size_t count,
                             uint32_t expected_cell_ns,
                             uft_flux_quality_t *quality)
{
    if (!flux_times || !quality || count < 10) return -1;
    
    memset(quality, 0, sizeof(*quality));
    
    /* Calculate intervals */
    uint32_t *intervals = malloc((count - 1) * sizeof(uint32_t));
    if (!intervals) return -1;
    
    uint64_t sum = 0;
    quality->min_interval = UINT32_MAX;
    quality->max_interval = 0;
    
    for (size_t i = 1; i < count; i++) {
        intervals[i - 1] = flux_times[i] - flux_times[i - 1];
        sum += intervals[i - 1];
        
        if (intervals[i - 1] < quality->min_interval) {
            quality->min_interval = intervals[i - 1];
        }
        if (intervals[i - 1] > quality->max_interval) {
            quality->max_interval = intervals[i - 1];
        }
    }
    
    size_t interval_count = count - 1;
    quality->avg_interval = sum / interval_count;
    
    /* Standard deviation */
    double variance = 0;
    for (size_t i = 0; i < interval_count; i++) {
        double diff = (double)intervals[i] - quality->avg_interval;
        variance += diff * diff;
    }
    quality->std_deviation = sqrt(variance / interval_count);
    
    /* Classify intervals */
    int valid_cells = 0;
    int outliers = 0;
    
    for (size_t i = 0; i < interval_count; i++) {
        float cells = (float)intervals[i] / expected_cell_ns;
        
        /* Valid if close to 1T, 2T, or 3T */
        if ((cells >= 0.8 && cells <= 1.2) ||
            (cells >= 1.8 && cells <= 2.2) ||
            (cells >= 2.8 && cells <= 3.2)) {
            valid_cells++;
        } else {
            outliers++;
        }
    }
    
    quality->valid_percent = (valid_cells * 100) / interval_count;
    quality->outlier_percent = (outliers * 100) / interval_count;
    
    /* Calculate jitter as percentage of cell time */
    quality->jitter_percent = (quality->std_deviation * 100.0) / expected_cell_ns;
    
    /* Overall score */
    int score = 100;
    
    /* Deduct for jitter */
    if (quality->jitter_percent > 20) score -= 40;
    else if (quality->jitter_percent > 10) score -= 20;
    else if (quality->jitter_percent > 5) score -= 10;
    
    /* Deduct for outliers */
    score -= quality->outlier_percent;
    
    if (score < 0) score = 0;
    quality->overall_score = score;
    
    /* Quality grade */
    if (score >= QUALITY_EXCELLENT) quality->grade = UFT_GRADE_EXCELLENT;
    else if (score >= QUALITY_GOOD) quality->grade = UFT_GRADE_GOOD;
    else if (score >= QUALITY_FAIR) quality->grade = UFT_GRADE_FAIR;
    else if (score >= QUALITY_POOR) quality->grade = UFT_GRADE_POOR;
    else quality->grade = UFT_GRADE_BAD;
    
    free(intervals);
    return 0;
}

/*===========================================================================
 * Sector Quality Metrics
 *===========================================================================*/

int uft_metrics_sector_quality(const uft_sector_read_t *reads, size_t count,
                               uft_sector_quality_t *quality)
{
    if (!reads || !quality || count == 0) return -1;
    
    memset(quality, 0, sizeof(*quality));
    
    quality->total_sectors = count;
    
    for (size_t i = 0; i < count; i++) {
        const uft_sector_read_t *r = &reads[i];
        
        if (r->crc_valid) {
            quality->good_sectors++;
        } else {
            quality->bad_sectors++;
        }
        
        if (r->deleted) quality->deleted_sectors++;
        if (r->weak_bits) quality->weak_sectors++;
        
        /* Sum up retry counts */
        quality->total_retries += r->retry_count;
    }
    
    /* Calculate percentages */
    quality->good_percent = (quality->good_sectors * 100) / count;
    quality->bad_percent = (quality->bad_sectors * 100) / count;
    
    /* Overall score */
    int score = quality->good_percent;
    
    /* Bonus for no retries */
    if (quality->total_retries == 0 && score < 100) {
        score += 5;
    }
    
    /* Penalty for weak bits */
    score -= (quality->weak_sectors * 5);
    
    if (score < 0) score = 0;
    if (score > 100) score = 100;
    
    quality->overall_score = score;
    
    /* Grade */
    if (score >= QUALITY_EXCELLENT) quality->grade = UFT_GRADE_EXCELLENT;
    else if (score >= QUALITY_GOOD) quality->grade = UFT_GRADE_GOOD;
    else if (score >= QUALITY_FAIR) quality->grade = UFT_GRADE_FAIR;
    else if (score >= QUALITY_POOR) quality->grade = UFT_GRADE_POOR;
    else quality->grade = UFT_GRADE_BAD;
    
    return 0;
}

/*===========================================================================
 * Track Quality Metrics
 *===========================================================================*/

int uft_metrics_track_quality(const uft_track_read_t *track,
                              uft_track_quality_t *quality)
{
    if (!track || !quality) return -1;
    
    memset(quality, 0, sizeof(*quality));
    
    quality->track = track->track;
    quality->side = track->side;
    quality->sector_count = track->sector_count;
    
    /* Calculate sector quality */
    uft_sector_quality_t sec_quality;
    if (track->sectors && track->sector_count > 0) {
        uft_metrics_sector_quality(track->sectors, track->sector_count, &sec_quality);
        quality->sector_score = sec_quality.overall_score;
    }
    
    /* Calculate flux quality if available */
    if (track->flux_times && track->flux_count > 0) {
        uft_flux_quality_t flux_quality;
        uint32_t cell_ns = (track->encoding == UFT_METRICS_HD) ? 1000 : 2000;
        
        uft_metrics_flux_quality(track->flux_times, track->flux_count,
                                 cell_ns, &flux_quality);
        quality->flux_score = flux_quality.overall_score;
    }
    
    /* Overall track score */
    if (quality->flux_score > 0 && quality->sector_score > 0) {
        quality->overall_score = (quality->flux_score + quality->sector_score) / 2;
    } else if (quality->sector_score > 0) {
        quality->overall_score = quality->sector_score;
    } else if (quality->flux_score > 0) {
        quality->overall_score = quality->flux_score;
    }
    
    return 0;
}

/*===========================================================================
 * Multi-Revolution Comparison
 *===========================================================================*/

int uft_metrics_revolution_compare(const uint32_t *rev1, size_t count1,
                                   const uint32_t *rev2, size_t count2,
                                   uft_rev_compare_t *result)
{
    if (!rev1 || !rev2 || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    result->rev1_flux = count1;
    result->rev2_flux = count2;
    
    /* Use shorter revolution as reference */
    size_t min_count = (count1 < count2) ? count1 : count2;
    
    if (min_count < 2) return -1;
    
    /* Convert to intervals */
    uint32_t *int1 = malloc((min_count - 1) * sizeof(uint32_t));
    uint32_t *int2 = malloc((min_count - 1) * sizeof(uint32_t));
    
    if (!int1 || !int2) {
        free(int1);
        free(int2);
        return -1;
    }
    
    for (size_t i = 1; i < min_count; i++) {
        int1[i - 1] = rev1[i] - rev1[i - 1];
        int2[i - 1] = rev2[i] - rev2[i - 1];
    }
    
    /* Compare intervals */
    int matching = 0;
    int different = 0;
    uint64_t diff_sum = 0;
    
    for (size_t i = 0; i < min_count - 1; i++) {
        int32_t diff = (int32_t)int1[i] - (int32_t)int2[i];
        if (diff < 0) diff = -diff;
        
        diff_sum += diff;
        
        /* Consider matching if within 10% */
        float ratio = (float)int1[i] / int2[i];
        if (ratio >= 0.9 && ratio <= 1.1) {
            matching++;
        } else {
            different++;
        }
    }
    
    result->matching_cells = matching;
    result->different_cells = different;
    result->avg_difference = diff_sum / (min_count - 1);
    result->similarity_percent = (matching * 100) / (min_count - 1);
    
    /* Detect potential weak bits */
    result->weak_bit_count = different;
    
    free(int1);
    free(int2);
    return 0;
}

/*===========================================================================
 * Protection Confidence
 *===========================================================================*/

int uft_metrics_protection_confidence(const uft_protection_detect_t *detections,
                                      size_t count,
                                      uft_protection_conf_t *confidence)
{
    if (!detections || !confidence || count == 0) return -1;
    
    memset(confidence, 0, sizeof(*confidence));
    
    confidence->detection_count = count;
    
    float max_conf = 0;
    
    for (size_t i = 0; i < count; i++) {
        const uft_protection_detect_t *d = &detections[i];
        
        if (d->confidence > max_conf) {
            max_conf = d->confidence;
            confidence->primary_type = d->type;
        }
        
        /* Track by protection category */
        switch (d->type) {
            case UFT_PROT_COPYLOCK:
            case UFT_PROT_SPEEDLOCK:
            case UFT_PROT_LONGTRACK:
                confidence->amiga_count++;
                break;
            case UFT_PROT_VMAX:
            case UFT_PROT_RAPIDLOK:
            case UFT_PROT_VORPAL:
                confidence->c64_count++;
                break;
            default:
                confidence->other_count++;
        }
    }
    
    confidence->max_confidence = max_conf;
    
    /* Calculate overall confidence */
    if (max_conf >= 0.9) {
        confidence->overall = UFT_CONF_HIGH;
    } else if (max_conf >= 0.7) {
        confidence->overall = UFT_CONF_MEDIUM;
    } else if (max_conf >= 0.5) {
        confidence->overall = UFT_CONF_LOW;
    } else {
        confidence->overall = UFT_CONF_UNKNOWN;
    }
    
    return 0;
}

/*===========================================================================
 * Disk Summary Metrics
 *===========================================================================*/

int uft_metrics_disk_summary(const uft_track_quality_t *tracks, size_t count,
                             uft_disk_summary_t *summary)
{
    if (!tracks || !summary || count == 0) return -1;
    
    memset(summary, 0, sizeof(*summary));
    
    summary->total_tracks = count;
    
    uint64_t sector_sum = 0;
    uint64_t flux_sum = 0;
    int sector_count = 0;
    int flux_count = 0;
    
    for (size_t i = 0; i < count; i++) {
        const uft_track_quality_t *t = &tracks[i];
        
        if (t->sector_score > 0) {
            sector_sum += t->sector_score;
            sector_count++;
        }
        
        if (t->flux_score > 0) {
            flux_sum += t->flux_score;
            flux_count++;
        }
        
        if (t->overall_score >= QUALITY_GOOD) {
            summary->good_tracks++;
        } else if (t->overall_score >= QUALITY_FAIR) {
            summary->fair_tracks++;
        } else {
            summary->bad_tracks++;
        }
    }
    
    if (sector_count > 0) {
        summary->avg_sector_score = sector_sum / sector_count;
    }
    
    if (flux_count > 0) {
        summary->avg_flux_score = flux_sum / flux_count;
    }
    
    /* Overall disk score */
    summary->overall_score = (summary->avg_sector_score + summary->avg_flux_score) / 2;
    
    /* Grade */
    if (summary->overall_score >= QUALITY_EXCELLENT) {
        summary->grade = UFT_GRADE_EXCELLENT;
    } else if (summary->overall_score >= QUALITY_GOOD) {
        summary->grade = UFT_GRADE_GOOD;
    } else if (summary->overall_score >= QUALITY_FAIR) {
        summary->grade = UFT_GRADE_FAIR;
    } else if (summary->overall_score >= QUALITY_POOR) {
        summary->grade = UFT_GRADE_POOR;
    } else {
        summary->grade = UFT_GRADE_BAD;
    }
    
    return 0;
}

/*===========================================================================
 * Report Generation
 *===========================================================================*/

const char *uft_metrics_grade_name(uft_quality_grade_t grade)
{
    switch (grade) {
        case UFT_GRADE_EXCELLENT: return "Excellent";
        case UFT_GRADE_GOOD:      return "Good";
        case UFT_GRADE_FAIR:      return "Fair";
        case UFT_GRADE_POOR:      return "Poor";
        case UFT_GRADE_BAD:       return "Bad";
        default:                  return "Unknown";
    }
}

int uft_metrics_report_json(const uft_disk_summary_t *summary,
                            char *buffer, size_t size)
{
    if (!summary || !buffer || size == 0) return -1;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"total_tracks\": %zu,\n"
        "  \"good_tracks\": %zu,\n"
        "  \"fair_tracks\": %zu,\n"
        "  \"bad_tracks\": %zu,\n"
        "  \"avg_sector_score\": %u,\n"
        "  \"avg_flux_score\": %u,\n"
        "  \"overall_score\": %u,\n"
        "  \"grade\": \"%s\"\n"
        "}",
        summary->total_tracks,
        summary->good_tracks,
        summary->fair_tracks,
        summary->bad_tracks,
        summary->avg_sector_score,
        summary->avg_flux_score,
        summary->overall_score,
        uft_metrics_grade_name(summary->grade)
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
