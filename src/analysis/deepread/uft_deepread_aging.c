/**
 * @file uft_deepread_aging.c
 * @brief DeepRead Magnetic Aging Profile -- implementation
 *
 * Performs linear-regression based aging analysis on the per-bitcell
 * quality profiles produced by the OTDR engine.  Each track's quality
 * trace is fitted to y = a*x + b; the slope, R-squared, and maximum
 * residual together characterise the degradation gradient and the
 * presence of localised damage.  Disk-level metrics aggregate the
 * per-track results into a single aging classification.
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#include "uft/analysis/uft_deepread_aging.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* ===================================================================
 * Internal helpers
 * =================================================================== */

/**
 * Classify the disk aging from aggregated metrics.
 *
 * Priority order (checked first-to-last):
 *   DAMAGED   : damage_regions > track_count / 4
 *   SEVERE    : |mean_slope| >= 0.01  OR  damage_regions > 5
 *   MODERATE  : |mean_slope| <  0.01  OR  damage_regions <= 5
 *   MILD      : |mean_slope| <  0.005 AND damage_regions <= 2
 *   PRISTINE  : |mean_slope| <  0.001 AND damage_regions == 0
 */
static uft_aging_class_t classify(float mean_slope,
                                  uint32_t damage_regions,
                                  uint16_t track_count)
{
    float abs_slope = fabsf(mean_slope);

    if (track_count > 0 && damage_regions > (uint32_t)(track_count / 4))
        return UFT_AGING_DAMAGED;

    if (abs_slope >= 0.01f || damage_regions > 5)
        return UFT_AGING_SEVERE;

    if (abs_slope >= 0.005f || damage_regions > 2)
        return UFT_AGING_MODERATE;

    if (abs_slope >= 0.001f || damage_regions > 0)
        return UFT_AGING_MILD;

    return UFT_AGING_PRISTINE;
}

/* ===================================================================
 * Public API
 * =================================================================== */

int uft_deepread_aging_track(const otdr_track_t *track,
                             float *slope, float *r_squared,
                             float *residual_max)
{
    if (!track || !slope || !r_squared || !residual_max)
        return -1;
    if (!track->quality_profile || track->bitcell_count < 2)
        return -1;

    const float *y  = track->quality_profile;
    const uint32_t n = track->bitcell_count;

    /* Accumulate sums for linear regression: y = a*x + b */
    double sum_x  = 0.0;
    double sum_y  = 0.0;
    double sum_xy = 0.0;
    double sum_x2 = 0.0;

    for (uint32_t i = 0; i < n; i++) {
        double xi = (double)i;
        double yi = (double)y[i];
        sum_x  += xi;
        sum_y  += yi;
        sum_xy += xi * yi;
        sum_x2 += xi * xi;
    }

    double dn    = (double)n;
    double denom = dn * sum_x2 - sum_x * sum_x;
    if (fabs(denom) < 1e-30)
        return -1;   /* degenerate (all x identical -- impossible for n>=2) */

    double a = (dn * sum_xy - sum_x * sum_y) / denom;
    double b = (sum_y - a * sum_x) / dn;

    /* R-squared = 1 - SS_res / SS_tot */
    double mean_y = sum_y / dn;
    double ss_res = 0.0;
    double ss_tot = 0.0;
    double max_res = 0.0;

    for (uint32_t i = 0; i < n; i++) {
        double yi   = (double)y[i];
        double pred = a * (double)i + b;
        double res  = yi - pred;
        double diff = yi - mean_y;

        ss_res += res * res;
        ss_tot += diff * diff;

        double abs_res = fabs(res);
        if (abs_res > max_res)
            max_res = abs_res;
    }

    double r2 = (ss_tot > 1e-30) ? 1.0 - (ss_res / ss_tot) : 0.0;
    if (r2 < 0.0) r2 = 0.0;

    *slope        = (float)a;
    *r_squared    = (float)r2;
    *residual_max = (float)max_res;

    return 0;
}

int uft_deepread_aging_analyze(const otdr_disk_t *disk,
                               uft_aging_result_t *result)
{
    if (!disk || !result)
        return -1;
    if (!disk->tracks || disk->track_count == 0)
        return -1;

    memset(result, 0, sizeof(*result));

    const uint16_t tc = disk->track_count;

    /* Per-track accumulators */
    double slope_sum       = 0.0;
    double r2_sum          = 0.0;
    float  worst_residual  = 0.0f;
    double snr_sum         = 0.0;
    uint32_t damage_count  = 0;
    uint16_t valid_tracks  = 0;

    /* Arrays for SNR-gradient regression (stack-friendly for <=168 tracks) */
    float *track_snr = (float *)calloc(tc, sizeof(float));
    if (!track_snr)
        return -1;

    for (uint16_t t = 0; t < tc; t++) {
        const otdr_track_t *trk = &disk->tracks[t];

        float trk_slope, trk_r2, trk_resmax;
        if (uft_deepread_aging_track(trk, &trk_slope, &trk_r2, &trk_resmax) != 0) {
            track_snr[t] = 0.0f;
            continue;
        }

        slope_sum += (double)trk_slope;
        r2_sum    += (double)trk_r2;
        valid_tracks++;

        if (trk_resmax > worst_residual)
            worst_residual = trk_resmax;

        if (trk_resmax > 10.0f)
            damage_count++;

        /* Use the track-level SNR estimate from the OTDR stats */
        float snr = trk->stats.snr_estimate;
        track_snr[t] = snr;
        snr_sum += (double)snr;
    }

    if (valid_tracks == 0) {
        free(track_snr);
        return -1;
    }

    float mean_slope = (float)(slope_sum / (double)valid_tracks);
    float mean_r2    = (float)(r2_sum    / (double)valid_tracks);
    float mean_snr   = (float)(snr_sum   / (double)tc);

    /* SNR gradient: linear regression of per-track mean SNR over track number */
    double sx  = 0.0, sy  = 0.0;
    double sxy = 0.0, sx2 = 0.0;

    for (uint16_t t = 0; t < tc; t++) {
        double xi = (double)t;
        double yi = (double)track_snr[t];
        sx  += xi;
        sy  += yi;
        sxy += xi * yi;
        sx2 += xi * xi;
    }

    double dn    = (double)tc;
    double denom = dn * sx2 - sx * sx;
    float  snr_grad = 0.0f;
    if (fabs(denom) > 1e-30)
        snr_grad = (float)((dn * sxy - sx * sy) / denom);

    free(track_snr);

    /* Populate result */
    result->slope          = mean_slope;
    result->r_squared      = mean_r2;
    result->residual_max   = worst_residual;
    result->mean_snr_db    = mean_snr;
    result->snr_gradient   = snr_grad;
    result->damage_regions = damage_count;
    result->classification = classify(mean_slope, damage_count, tc);

    return 0;
}

const char *uft_aging_class_name(uft_aging_class_t cls)
{
    switch (cls) {
    case UFT_AGING_PRISTINE: return "Pristine";
    case UFT_AGING_MILD:     return "Mild";
    case UFT_AGING_MODERATE:  return "Moderate";
    case UFT_AGING_SEVERE:   return "Severe";
    case UFT_AGING_DAMAGED:  return "Damaged";
    default:                 return "Unknown";
    }
}
