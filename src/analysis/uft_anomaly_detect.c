/**
 * @file uft_anomaly_detect.c
 * @brief ML-based Anomaly Detection — Implementation
 *
 * Lightweight autoencoder-inspired anomaly detector for floppy disk
 * track timing histograms.  Learns a per-bin mean/variance baseline
 * from "normal" tracks, then scores new tracks via Mahalanobis-like
 * distance mapped through a sigmoid.
 *
 * Self-training mode: when no external training data is provided, the
 * detector trains on the first N tracks of the disk itself, identifying
 * tracks that deviate from the disk's own statistical norm.
 *
 * Algorithm overview:
 *   1. Training: mean[i] and var[i] for each of 256 histogram bins
 *   2. Scoring:  d = SUM( (x[i]-mean[i])^2 / var[i] )  (Mahalanobis)
 *   3. Normalise: score = sigmoid(d - threshold)
 *   4. Classify: score > 0.5 => anomalous
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#include "uft/analysis/uft_anomaly_detect.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ===================================================================
 * Constants
 * =================================================================== */

/** Minimum variance to prevent division-by-zero in scoring */
#define MIN_VARIANCE        1e-8f

/** Number of tracks used for self-training when no external data given */
#define SELF_TRAIN_TRACKS   10

/** Sigmoid threshold — calibrated so "average distance" maps to ~0.25 */
#define SIGMOID_THRESHOLD   3.0f

/** Sigmoid steepness parameter */
#define SIGMOID_STEEPNESS   0.5f

/** Number of consecutive anomalous tracks that suggest copy protection */
#define PROTECTION_CLUSTER  3

/* ===================================================================
 * Opaque model structure
 * =================================================================== */

struct uft_anomaly_model {
    float mean[UFT_ANOMALY_HIST_BINS];      /**< Per-bin mean */
    float variance[UFT_ANOMALY_HIST_BINS];  /**< Per-bin variance */
    float threshold;                        /**< Sigmoid centre point */
    bool  trained;                          /**< True after training */
};

/* ===================================================================
 * Internal helpers
 * =================================================================== */

/**
 * Compute Mahalanobis-like distance between a histogram and the model.
 *
 * d = SUM_i( (x[i] - mean[i])^2 / variance[i] )
 *
 * Normalised by the number of non-trivial bins to make the score
 * independent of how many bins are populated.
 */
static float mahalanobis_distance(const uft_anomaly_model_t *model,
                                   const float *histogram)
{
    double dist = 0.0;
    int active_bins = 0;

    for (int i = 0; i < UFT_ANOMALY_HIST_BINS; i++) {
        float var = model->variance[i];
        if (var < MIN_VARIANCE)
            var = MIN_VARIANCE;

        double diff = (double)histogram[i] - (double)model->mean[i];
        dist += (diff * diff) / (double)var;

        /* Count bins that have meaningful data */
        if (model->mean[i] > MIN_VARIANCE || histogram[i] > MIN_VARIANCE)
            active_bins++;
    }

    /* Normalise by active bin count to get per-bin distance */
    if (active_bins > 0)
        dist /= (double)active_bins;

    return (float)dist;
}

/**
 * Map a raw distance to [0, 1] via sigmoid.
 *
 * score = 1 / (1 + exp(-(d - threshold) * steepness))
 */
static float sigmoid_score(float distance, float threshold)
{
    float x = (distance - threshold) * SIGMOID_STEEPNESS;

    /* Clamp to prevent overflow in exp() */
    if (x > 20.0f) return 1.0f;
    if (x < -20.0f) return 0.0f;

    return 1.0f / (1.0f + expf(-x));
}

/**
 * Check if anomalous tracks form clusters suggestive of copy protection.
 *
 * Copy protection typically affects a contiguous range of tracks (e.g.
 * tracks 20-24) rather than random scattered tracks.
 */
static bool check_protection_pattern(const uft_anomaly_result_t *result)
{
    if (result->anomalous_tracks < PROTECTION_CLUSTER)
        return false;

    /* Sort anomaly indices by track number — they are already in order
     * because we iterate tracks sequentially in uft_anomaly_detect_disk */

    int consecutive = 1;
    int max_consecutive = 1;

    for (int i = 1; i < result->count; i++) {
        if (result->anomalies[i].anomaly_score <= UFT_ANOMALY_THRESHOLD)
            continue;

        /* Check if this anomaly is on a track adjacent to the previous */
        int prev_idx = -1;
        for (int j = i - 1; j >= 0; j--) {
            if (result->anomalies[j].anomaly_score > UFT_ANOMALY_THRESHOLD) {
                prev_idx = j;
                break;
            }
        }
        if (prev_idx >= 0) {
            int delta = (int)result->anomalies[i].track
                      - (int)result->anomalies[prev_idx].track;
            if (delta <= 2) {
                consecutive++;
                if (consecutive > max_consecutive)
                    max_consecutive = consecutive;
            } else {
                consecutive = 1;
            }
        }
    }

    return max_consecutive >= PROTECTION_CLUSTER;
}

/* ===================================================================
 * Public API
 * =================================================================== */

uft_anomaly_model_t *uft_anomaly_model_create(void)
{
    uft_anomaly_model_t *model = (uft_anomaly_model_t *)calloc(
        1, sizeof(uft_anomaly_model_t));
    if (!model)
        return NULL;

    model->threshold = SIGMOID_THRESHOLD;
    model->trained = false;
    return model;
}

int uft_anomaly_model_train(uft_anomaly_model_t *model,
                             const float *histograms,
                             int n_tracks)
{
    if (!model || !histograms || n_tracks < 2)
        return -1;

    /* Pass 1: compute per-bin mean */
    memset(model->mean, 0, sizeof(model->mean));

    for (int t = 0; t < n_tracks; t++) {
        const float *h = histograms + t * UFT_ANOMALY_HIST_BINS;
        for (int i = 0; i < UFT_ANOMALY_HIST_BINS; i++)
            model->mean[i] += h[i];
    }

    float inv_n = 1.0f / (float)n_tracks;
    for (int i = 0; i < UFT_ANOMALY_HIST_BINS; i++)
        model->mean[i] *= inv_n;

    /* Pass 2: compute per-bin variance */
    memset(model->variance, 0, sizeof(model->variance));

    for (int t = 0; t < n_tracks; t++) {
        const float *h = histograms + t * UFT_ANOMALY_HIST_BINS;
        for (int i = 0; i < UFT_ANOMALY_HIST_BINS; i++) {
            float diff = h[i] - model->mean[i];
            model->variance[i] += diff * diff;
        }
    }

    float inv_n1 = 1.0f / (float)(n_tracks - 1);  /* Bessel's correction */
    for (int i = 0; i < UFT_ANOMALY_HIST_BINS; i++) {
        model->variance[i] *= inv_n1;
        /* Clamp minimum variance */
        if (model->variance[i] < MIN_VARIANCE)
            model->variance[i] = MIN_VARIANCE;
    }

    /* Calibrate threshold: compute mean distance of training set, then
     * set threshold at 2x the mean distance so normal tracks score ~0.1 */
    double total_dist = 0.0;
    for (int t = 0; t < n_tracks; t++) {
        const float *h = histograms + t * UFT_ANOMALY_HIST_BINS;
        total_dist += (double)mahalanobis_distance(model, h);
    }
    float mean_dist = (float)(total_dist / (double)n_tracks);

    /* Threshold = 2x mean distance — normal tracks cluster below this */
    model->threshold = mean_dist * 2.0f;
    if (model->threshold < SIGMOID_THRESHOLD)
        model->threshold = SIGMOID_THRESHOLD;

    model->trained = true;
    return 0;
}

float uft_anomaly_model_score(const uft_anomaly_model_t *model,
                               const float *histogram)
{
    if (!model || !model->trained || !histogram)
        return -1.0f;

    float dist = mahalanobis_distance(model, histogram);
    return sigmoid_score(dist, model->threshold);
}

int uft_anomaly_detect_disk(uft_anomaly_model_t *model,
                             const float (*track_histograms)[256],
                             int n_tracks,
                             uft_anomaly_result_t *result)
{
    if (!model || !track_histograms || n_tracks < 1 || !result)
        return -1;

    memset(result, 0, sizeof(*result));

    /* Self-training: if model not yet trained, use first N tracks */
    if (!model->trained) {
        int train_count = n_tracks < SELF_TRAIN_TRACKS
                        ? n_tracks : SELF_TRAIN_TRACKS;
        if (train_count < 2)
            train_count = n_tracks;  /* Need at least 2 for variance */
        if (train_count < 2)
            return -1;  /* Cannot train on <2 tracks */

        int rc = uft_anomaly_model_train(model,
                                          (const float *)track_histograms,
                                          train_count);
        if (rc != 0)
            return -1;
    }

    /* Score every track */
    double score_sum = 0.0;
    float  max_score = 0.0f;
    int    anom_count = 0;

    for (int t = 0; t < n_tracks && result->count < UFT_ANOMALY_MAX; t++) {
        float score = uft_anomaly_model_score(model, track_histograms[t]);
        if (score < 0.0f)
            score = 0.0f;

        score_sum += (double)score;
        if (score > max_score)
            max_score = score;

        if (score > UFT_ANOMALY_THRESHOLD) {
            anom_count++;
        }

        /* Record all tracks in the anomaly array (for later analysis) */
        uft_anomaly_t *a = &result->anomalies[result->count];
        a->anomaly_score = score;
        a->track = (uint8_t)(t & 0xFF);
        a->head = 0;

        if (score > 0.8f) {
            snprintf(a->description, sizeof(a->description),
                     "Track %d: HIGHLY anomalous (score %.3f) — "
                     "significant deviation from baseline",
                     t, (double)score);
        } else if (score > UFT_ANOMALY_THRESHOLD) {
            snprintf(a->description, sizeof(a->description),
                     "Track %d: anomalous (score %.3f) — "
                     "moderate deviation from baseline",
                     t, (double)score);
        } else {
            snprintf(a->description, sizeof(a->description),
                     "Track %d: normal (score %.3f)",
                     t, (double)score);
        }

        result->count++;
    }

    /* Aggregate statistics */
    result->mean_score = (n_tracks > 0)
                       ? (float)(score_sum / (double)n_tracks)
                       : 0.0f;
    result->max_score = max_score;
    result->anomalous_tracks = anom_count;

    /* Check for protection-like clustering */
    result->likely_protection = check_protection_pattern(result);

    /* Build summary */
    if (anom_count == 0) {
        snprintf(result->summary, sizeof(result->summary),
                 "No anomalies detected across %d tracks "
                 "(mean score: %.3f)",
                 n_tracks, (double)result->mean_score);
    } else if (result->likely_protection) {
        snprintf(result->summary, sizeof(result->summary),
                 "%d anomalous tracks detected (max score: %.3f) — "
                 "pattern consistent with copy protection",
                 anom_count, (double)max_score);
    } else {
        snprintf(result->summary, sizeof(result->summary),
                 "%d anomalous tracks detected (max score: %.3f) — "
                 "possible media damage or unusual formatting",
                 anom_count, (double)max_score);
    }

    return 0;
}

void uft_anomaly_model_free(uft_anomaly_model_t *model)
{
    free(model);
}
