/**
 * @file uft_deepread_splice.c
 * @brief DeepRead Write-Splice Detection -- Implementation
 *
 * Locates the write splice on each track by finding the largest first-
 * derivative discontinuity in the OTDR quality profile.  For whole-disk
 * analysis the consistency of splice positions across tracks is evaluated
 * against the median position.
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#include "uft/analysis/uft_deepread_splice.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* Minimum bitcell count to attempt detection. */
#define MIN_BITCELLS  100

/* Magnitude threshold (dB) above which a splice is considered detected. */
#define SPLICE_MAGNITUDE_THRESHOLD  3.0f

/* Tolerance (bitcells) around the median for the consistency metric. */
#define CONSISTENCY_TOLERANCE  50

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

/**
 * Compare function for qsort on uint32_t values.
 */
static int cmp_u32(const void *a, const void *b)
{
    uint32_t va = *(const uint32_t *)a;
    uint32_t vb = *(const uint32_t *)b;
    if (va < vb) return -1;
    if (va > vb) return  1;
    return 0;
}

/**
 * Estimate nanosecond position of a bitcell within the track.
 *
 * Uses a simple linear interpolation: each bitcell occupies
 * (revolution_ns / bitcell_count) nanoseconds.
 */
static uint32_t bitcell_to_ns(const otdr_track_t *track, uint32_t bitcell)
{
    if (track->bitcell_count == 0)
        return 0;
    return (uint32_t)((uint64_t)bitcell * track->revolution_ns
                      / track->bitcell_count);
}

/**
 * Largest first-derivative jump of a quality profile.
 * d[i] = profile[i+1] - profile[i]; returns the position i of max |d|.
 */
static uint32_t profile_peak(const float *profile, uint32_t n, float *mag)
{
    uint32_t max_pos = 0;
    float    max_abs = 0.0f;

    for (uint32_t i = 0; i + 1 < n; i++) {
        float a = fabsf(profile[i + 1] - profile[i]);
        if (a > max_abs) {
            max_abs = a;
            max_pos = i;
        }
    }
    *mag = max_abs;
    return max_pos;
}

/**
 * Splice position of ONE revolution, measured the same way as the track:
 * the revolution runs through the OTDR analysis as its own track (same
 * encoding as the main track, default configuration otherwise) and its
 * profile peak is taken. Returns 0 on success.
 */
static int revolution_peak(const otdr_track_t *track, uint8_t rev,
                           uint32_t *pos)
{
    const uint32_t *flux = track->flux_multi[rev];
    uint32_t        cnt  = track->flux_multi_count[rev];
    if (!flux || cnt < MIN_BITCELLS)
        return -1;

    otdr_track_t *tmp = (otdr_track_t *)calloc(1, sizeof(*tmp));
    if (!tmp)
        return -1;
    otdr_track_load_flux(tmp, flux, cnt, 0);

    otdr_config_t cfg;
    otdr_config_defaults(&cfg);
    cfg.encoding = track->encoding;
    int rc = -1;
    if (otdr_track_analyze(tmp, &cfg) == 0 && tmp->quality_profile &&
        tmp->bitcell_count >= MIN_BITCELLS) {
        float mag;
        *pos = profile_peak(tmp->quality_profile, tmp->bitcell_count, &mag);
        rc = 0;
    }
    otdr_track_free(tmp);   /* frees the struct as well */
    return rc;
}

/* -----------------------------------------------------------------------
 * Single-track splice detection
 * ----------------------------------------------------------------------- */

int uft_deepread_detect_splice(const otdr_track_t *track,
                               uft_splice_result_t *result)
{
    if (!track || !result)
        return -1;

    /* Clear the result. */
    memset(result, 0, sizeof(*result));

    /* Need a quality profile with enough data points. */
    if (!track->quality_profile || track->bitcell_count < MIN_BITCELLS)
        return -1;

    const uint32_t n = track->bitcell_count;

    /* --- Step 1: largest jump of the quality profile ----------------- */
    float    max_abs = 0.0f;
    uint32_t max_pos = profile_peak(track->quality_profile, n, &max_abs);

    /* --- Step 2: fill basic result fields ----------------------------- */
    result->splice_bitcell  = max_pos;
    result->splice_ns       = bitcell_to_ns(track, max_pos);
    result->splice_magnitude = max_abs;

    /* --- Step 3: multi-revolution stability --------------------------- */
    /*     Std-dev (bitcells) of the peak position over revolutions, each
     *     revolution measured exactly like the track. A write splice sits
     *     on the medium and recurs every revolution; a noise peak does not.
     *     Before MF-1431 this took the largest jump between consecutive RAW
     *     intervals (the first 2T->4T change, not the splice) and reported
     *     a spread of FLUX indices: the same splice on two revolutions gave
     *     4414.5 (tests/test_deepread_naht.c W2). */
    result->splice_stability = 0.0f;
    if (track->num_revolutions > 1) {
        uint32_t positions[OTDR_MAX_REVOLUTIONS];
        uint32_t valid_revs = 0;
        double   sum = 0.0;

        for (uint8_t r = 0; r < track->num_revolutions; r++) {
            uint32_t pos;
            if (revolution_peak(track, r, &pos) != 0)
                continue;
            positions[valid_revs++] = pos;
            sum += (double)pos;
        }

        if (valid_revs > 1) {
            double mean = sum / valid_revs;
            double var  = 0.0;
            for (uint32_t r = 0; r < valid_revs; r++) {
                double diff = (double)positions[r] - mean;
                var += diff * diff;
            }
            var /= (double)(valid_revs - 1);
            result->splice_stability = (float)sqrt(var);
        }
    }

    /* --- Step 4: index offset ----------------------------------------- */
    result->index_offset_ns = (int32_t)result->splice_ns
                            - (int32_t)track->revolution_ns;

    /* --- Step 5: detection flag --------------------------------------- */
    result->detected = (result->splice_magnitude > SPLICE_MAGNITUDE_THRESHOLD);

    return 0;
}

/* -----------------------------------------------------------------------
 * Disk-wide splice map
 * ----------------------------------------------------------------------- */

int uft_deepread_disk_splice_map(const otdr_disk_t *disk,
                                 uft_splice_result_t *results,
                                 float *consistency)
{
    if (!disk || !results || !consistency)
        return -1;
    if (disk->track_count == 0 || !disk->tracks)
        return -1;

    *consistency = 0.0f;

    /* --- Per-track detection ------------------------------------------ */
    uint32_t detected_count = 0;

    for (uint16_t t = 0; t < disk->track_count; t++) {
        uft_deepread_detect_splice(&disk->tracks[t], &results[t]);
        if (results[t].detected)
            detected_count++;
    }

    if (detected_count == 0)
        return 0;   /* Nothing detected -- consistency stays 0. */

    /* --- Compute median splice position ------------------------------- */
    uint32_t *positions = (uint32_t *)malloc(detected_count * sizeof(uint32_t));
    if (!positions)
        return -1;

    uint32_t idx = 0;
    for (uint16_t t = 0; t < disk->track_count; t++) {
        if (results[t].detected)
            positions[idx++] = results[t].splice_bitcell;
    }

    qsort(positions, detected_count, sizeof(uint32_t), cmp_u32);
    uint32_t median = positions[detected_count / 2];

    /* --- Consistency: ratio within +/- CONSISTENCY_TOLERANCE ---------- */
    uint32_t within = 0;
    for (uint32_t i = 0; i < detected_count; i++) {
        int32_t diff = (int32_t)positions[i] - (int32_t)median;
        if (abs(diff) <= CONSISTENCY_TOLERANCE)
            within++;
    }

    *consistency = (float)within / (float)detected_count;

    free(positions);
    return 0;
}
