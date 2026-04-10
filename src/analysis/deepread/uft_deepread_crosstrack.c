/**
 * @file uft_deepread_crosstrack.c
 * @brief DeepRead Cross-Track Correlation — Implementation
 *
 * Computes Normalized Cross-Correlation (NCC) between adjacent track
 * quality profiles and classifies disk-wide damage patterns.
 *
 * NCC formula:
 *   NCC(a,b) = sum((a[i]-mean_a) * (b[i]-mean_b)) / (n * std_a * std_b)
 *
 * Classification rules:
 *   - Mean NCC > 0.6 across many adjacent pairs  => RADIAL damage
 *   - Low NCC with high individual track damage   => MAGNETIC (localized)
 *   - High NCC in circumferential band            => CIRCUMFERENTIAL
 *   - Multiple patterns                           => MIXED
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#include "uft/analysis/uft_deepread_crosstrack.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ===================================================================
 * Internal helpers
 * =================================================================== */

/**
 * Compute mean of a float array.
 */
static float compute_mean(const float *data, uint32_t n)
{
    if (n == 0) return 0.0f;

    double sum = 0.0;
    for (uint32_t i = 0; i < n; i++)
        sum += (double)data[i];

    return (float)(sum / (double)n);
}

/**
 * Compute standard deviation of a float array given its mean.
 */
static float compute_stddev(const float *data, uint32_t n, float mean)
{
    if (n < 2) return 0.0f;

    double sum_sq = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double d = (double)data[i] - (double)mean;
        sum_sq += d * d;
    }

    return (float)sqrt(sum_sq / (double)n);
}

/**
 * Compute Normalized Cross-Correlation between two quality profiles.
 *
 * Uses the shorter of the two arrays as the comparison length.
 * Returns 0.0 if either array has zero variance.
 */
static float compute_ncc(const float *a, uint32_t na,
                         const float *b, uint32_t nb)
{
    uint32_t n = (na < nb) ? na : nb;
    if (n == 0) return 0.0f;

    float mean_a = compute_mean(a, n);
    float mean_b = compute_mean(b, n);
    float std_a  = compute_stddev(a, n, mean_a);
    float std_b  = compute_stddev(b, n, mean_b);

    /* If either signal is flat (zero variance), correlation is undefined */
    if (std_a < 1e-9f || std_b < 1e-9f)
        return 0.0f;

    double cross = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        cross += ((double)a[i] - (double)mean_a) *
                 ((double)b[i] - (double)mean_b);
    }

    float ncc = (float)(cross / ((double)n * (double)std_a * (double)std_b));

    /* Clamp to [-1, 1] in case of floating point drift */
    if (ncc > 1.0f)  ncc = 1.0f;
    if (ncc < -1.0f) ncc = -1.0f;

    return ncc;
}

/**
 * Check whether a track's overall quality is below FAIR.
 */
static bool track_is_damaged(const otdr_track_t *track)
{
    return (track->stats.overall >= OTDR_QUAL_POOR);
}

/* ===================================================================
 * Public API
 * =================================================================== */

int uft_deepread_crosstrack_analyze(const otdr_disk_t *disk,
                                    uft_crosstrack_result_t *result)
{
    if (!disk || !result)
        return -1;

    uint16_t tc = disk->track_count;
    if (tc < 2) {
        /* Need at least 2 tracks for cross-correlation */
        memset(result, 0, sizeof(*result));
        result->overall = UFT_DAMAGE_NONE;
        return 0;
    }

    /* --- Allocate correlation matrix --- */
    float *matrix = (float *)calloc((size_t)tc * (size_t)tc, sizeof(float));
    if (!matrix)
        return -1;

    /* Diagonal is always 1.0 (self-correlation) */
    for (uint16_t i = 0; i < tc; i++)
        matrix[i * tc + i] = 1.0f;

    /* --- Compute NCC for every adjacent pair --- */
    uint32_t adjacent_count = 0;
    double   adjacent_sum   = 0.0;
    uint32_t radial_count   = 0;
    uint32_t high_corr_damaged = 0;
    uint32_t low_corr_damaged  = 0;

    for (uint16_t t = 0; t + 1 < tc; t++) {
        const otdr_track_t *ta = &disk->tracks[t];
        const otdr_track_t *tb = &disk->tracks[t + 1];

        /* Skip tracks without quality profiles */
        if (!ta->quality_profile || ta->bitcell_count == 0 ||
            !tb->quality_profile || tb->bitcell_count == 0) {
            matrix[t * tc + (t + 1)] = 0.0f;
            matrix[(t + 1) * tc + t] = 0.0f;
            continue;
        }

        float ncc = compute_ncc(ta->quality_profile, ta->bitcell_count,
                                tb->quality_profile, tb->bitcell_count);

        /* Store symmetrically */
        matrix[t * tc + (t + 1)] = ncc;
        matrix[(t + 1) * tc + t] = ncc;

        adjacent_sum += (double)ncc;
        adjacent_count++;

        /* Classify this pair */
        bool a_damaged = track_is_damaged(ta);
        bool b_damaged = track_is_damaged(tb);

        if (ncc > 0.7f && a_damaged && b_damaged) {
            /*
             * High correlation + both tracks damaged = radial damage.
             * The same defect pattern repeats across adjacent tracks,
             * characteristic of a radial scratch or head crash.
             */
            radial_count++;
            high_corr_damaged++;
        } else if (ncc < 0.3f && (a_damaged || b_damaged)) {
            /*
             * Low correlation + damage on one/both tracks = localized
             * magnetic degradation. The damage is confined to individual
             * tracks and does not correlate across the disk.
             */
            low_corr_damaged++;
        }
    }

    /* --- Check if high-NCC damage overlaps known copy protection ranges ---
     * CopyLock typically uses tracks 0-2; RapidLok uses tracks 36+.
     * If "radial damage" falls in those ranges, it may actually be
     * intentional copy protection rather than physical damage. */
    result->may_be_protection = false;
    for (uint16_t t = 0; t + 1 < tc; t++) {
        float ncc = matrix[t * tc + (t + 1)];
        if (ncc > 0.7f && track_is_damaged(&disk->tracks[t])) {
            uint16_t track_num = disk->tracks[t].track_num;
            if (track_num <= 2 || track_num >= 36) {
                result->may_be_protection = true;
                break;
            }
        }
    }

    /* --- Fill result structure --- */
    result->correlation_matrix = matrix;
    result->matrix_size        = tc;
    result->radial_damage_count = radial_count;

    if (adjacent_count > 0)
        result->mean_correlation = (float)(adjacent_sum / (double)adjacent_count);
    else
        result->mean_correlation = 0.0f;

    /* --- Classify overall damage pattern --- */
    bool has_radial   = (radial_count > 0);
    bool has_magnetic = (low_corr_damaged > 0);

    /*
     * Circumferential damage: high mean correlation across many tracks
     * with moderate damage indicates a ring-shaped wear pattern.
     * This happens from repeated head contact at a consistent position.
     */
    bool has_circumferential = false;
    if (adjacent_count >= 4 && result->mean_correlation > 0.6f) {
        /* Count how many tracks show damage at similar positions */
        uint32_t consistent_damage = 0;
        for (uint16_t t = 0; t + 1 < tc; t++) {
            float ncc = matrix[t * tc + (t + 1)];
            if (ncc > 0.6f && track_is_damaged(&disk->tracks[t]))
                consistent_damage++;
        }
        /* If more than half the adjacent pairs show correlated damage,
         * it is circumferential wear */
        if (consistent_damage > adjacent_count / 2)
            has_circumferential = true;
    }

    /* Determine overall classification */
    int pattern_count = (has_radial ? 1 : 0) +
                        (has_magnetic ? 1 : 0) +
                        (has_circumferential ? 1 : 0);

    if (pattern_count == 0) {
        result->overall = UFT_DAMAGE_NONE;
    } else if (pattern_count > 1) {
        result->overall = UFT_DAMAGE_MIXED;
    } else if (has_radial) {
        result->overall = UFT_DAMAGE_RADIAL;
    } else if (has_circumferential) {
        result->overall = UFT_DAMAGE_CIRCUMFER;
    } else {
        result->overall = UFT_DAMAGE_MAGNETIC;
    }

    return 0;
}

void uft_crosstrack_result_free(uft_crosstrack_result_t *result)
{
    if (!result) return;

    free(result->correlation_matrix);
    result->correlation_matrix = NULL;
    result->matrix_size = 0;
}

const char *uft_damage_type_name(uft_damage_type_t type)
{
    switch (type) {
    case UFT_DAMAGE_NONE:       return "None";
    case UFT_DAMAGE_MAGNETIC:   return "Magnetic (localized)";
    case UFT_DAMAGE_RADIAL:     return "Radial scratch";
    case UFT_DAMAGE_CIRCUMFER:  return "Circumferential wear";
    case UFT_DAMAGE_MIXED:      return "Mixed damage";
    default:                    return "Unknown";
    }
}
