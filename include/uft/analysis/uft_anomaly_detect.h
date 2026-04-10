/**
 * @file uft_anomaly_detect.h
 * @brief ML-based Anomaly Detection for Floppy Disk Tracks
 *
 * Implements a lightweight autoencoder-inspired anomaly detector that
 * learns a statistical baseline from "normal" tracks and flags tracks
 * whose flux timing histograms deviate significantly.  Uses Mahalanobis-
 * like distance with sigmoid normalization -- no external ML libraries.
 *
 * Typical workflow:
 *   1. Create model with uft_anomaly_model_create()
 *   2. Train on known-good histograms, or use self-training mode
 *      (uft_anomaly_detect_disk trains on the first 10 tracks)
 *   3. Score individual tracks or an entire disk
 *   4. Free the model when done
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#ifndef UFT_ANOMALY_DETECT_H
#define UFT_ANOMALY_DETECT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * Constants
 * =================================================================== */

/** Number of histogram bins expected per track (matches OTDR pipeline) */
#define UFT_ANOMALY_HIST_BINS   256

/** Maximum number of anomalies reported in a single result */
#define UFT_ANOMALY_MAX         256

/** Score threshold above which a track is considered anomalous */
#define UFT_ANOMALY_THRESHOLD   0.5f

/* ===================================================================
 * Types
 * =================================================================== */

/** A single detected anomaly on one track. */
typedef struct {
    float    anomaly_score;         /**< 0.0 = normal, 1.0 = highly anomalous */
    uint8_t  track;
    uint8_t  head;
    char     description[128];
} uft_anomaly_t;

/** Aggregated anomaly detection result for an entire disk. */
typedef struct {
    uft_anomaly_t anomalies[UFT_ANOMALY_MAX];
    int      count;                 /**< Number of anomalies in the array */
    float    mean_score;            /**< Average anomaly score across all tracks */
    float    max_score;             /**< Maximum anomaly score observed */
    int      anomalous_tracks;      /**< Tracks with score > UFT_ANOMALY_THRESHOLD */
    bool     likely_protection;     /**< Anomalies consistent with known patterns */
    char     summary[256];          /**< Human-readable summary */
} uft_anomaly_result_t;

/**
 * Opaque anomaly model.
 *
 * Stores the per-bin mean and variance learned during training,
 * plus the sigmoid threshold calibrated from the training set.
 */
typedef struct uft_anomaly_model uft_anomaly_model_t;

/* ===================================================================
 * API Functions
 * =================================================================== */

/**
 * Create an empty anomaly model.
 *
 * @return Newly allocated model, or NULL on allocation failure.
 */
uft_anomaly_model_t *uft_anomaly_model_create(void);

/**
 * Train the model from N "normal" track histograms.
 *
 * Computes per-bin mean and variance across all training histograms.
 * Bins with near-zero variance are clamped to a minimum to avoid
 * division-by-zero in the scoring step.
 *
 * @param model       Model to train (must not be NULL).
 * @param histograms  Array of N x 256 floats (row-major, N histograms).
 * @param n_tracks    Number of training histograms (must be >= 2).
 * @return 0 on success, -1 on invalid input.
 */
int uft_anomaly_model_train(uft_anomaly_model_t *model,
                             const float *histograms,
                             int n_tracks);

/**
 * Score a single track histogram against the trained model.
 *
 * Computes Mahalanobis-like distance and maps it through a sigmoid
 * to produce a score in [0.0, 1.0].
 *
 * @param model      Trained model.
 * @param histogram  256 floats representing the track's timing histogram.
 * @return Anomaly score in [0.0, 1.0], or -1.0 on error.
 */
float uft_anomaly_model_score(const uft_anomaly_model_t *model,
                               const float *histogram);

/**
 * Run anomaly detection on an entire disk.
 *
 * If the model is not yet trained, uses self-training mode: trains on
 * the first min(10, n_tracks) tracks, then scores all tracks.  Populates
 * the result structure with per-track anomalies and aggregate statistics.
 *
 * @param model             Model (may be untrained for self-training).
 * @param track_histograms  Array of n_tracks histograms, each 256 floats.
 * @param n_tracks          Total number of tracks on the disk.
 * @param result            Output result (caller allocates).
 * @return 0 on success, -1 on error.
 */
int uft_anomaly_detect_disk(uft_anomaly_model_t *model,
                             const float (*track_histograms)[256],
                             int n_tracks,
                             uft_anomaly_result_t *result);

/**
 * Free an anomaly model and all associated resources.
 *
 * @param model  Model to free (NULL is safe).
 */
void uft_anomaly_model_free(uft_anomaly_model_t *model);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ANOMALY_DETECT_H */
