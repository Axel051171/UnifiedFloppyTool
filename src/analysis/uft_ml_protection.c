/**
 * @file uft_ml_protection.c
 * @brief ML-based Copy Protection Classifier — Implementation
 *
 * Feature-based classifier that identifies floppy disk copy protection
 * schemes using cosine similarity against known reference signatures.
 *
 * Feature vector (8 dimensions per track):
 *   [0] histogram_entropy    — Shannon entropy of flux timing histogram
 *   [1] track_length_ratio   — actual/nominal (>1.02 = long track)
 *   [2] sync_pattern_score   — custom sync frequency, normalised 0-1
 *   [3] bad_gcr_ratio        — bad GCR bytes / total, normalised 0-1
 *   [4] duplicate_id_count   — normalised 0-1
 *   [5] jitter_rms           — normalised 0-1
 *   [6] half_track_flag      — 0.0 or 1.0
 *   [7] custom_sync_flag     — 0.0 or 1.0
 *
 * Classification algorithm:
 *   1. Average feature vectors across all tracks
 *   2. Compute cosine similarity against each known protection signature
 *   3. Rank by similarity, return top 5 candidates
 *   4. If best match > 0.7: confident classification
 *   5. If best match < 0.5 but features suggest anomalies: "unknown"
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#include "uft/analysis/uft_ml_protection.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ===================================================================
 * Known protection reference signatures
 *
 * Each row: [entropy, length_ratio, sync, bad_gcr, dup_id,
 *            jitter, half_track, custom_sync]
 *
 * Values are normalised to the same 0-1 (or ratio) space as
 * uft_ml_extract_features() produces.
 * =================================================================== */

typedef struct {
    const char *name;
    float       signature[UFT_ML_PROT_FEATURES];
} protection_ref_t;

static const protection_ref_t KNOWN_SCHEMES[] = {
    /* Commodore 64 protection schemes */
    { "V-MAX!",
      { 0.70f, 1.00f, 0.90f, 0.10f, 0.00f, 0.30f, 0.00f, 1.00f } },
    { "RapidLok",
      { 0.50f, 1.00f, 0.80f, 0.00f, 0.00f, 0.20f, 1.00f, 1.00f } },
    { "Vorpal",
      { 0.65f, 0.95f, 0.70f, 0.05f, 0.00f, 0.25f, 0.00f, 1.00f } },
    { "FatBits",
      { 0.80f, 1.10f, 0.20f, 0.30f, 0.00f, 0.60f, 0.00f, 0.00f } },
    { "Pirate Slayer",
      { 0.55f, 1.05f, 0.60f, 0.15f, 0.00f, 0.40f, 0.00f, 1.00f } },
    { "ProLok",
      { 0.60f, 1.00f, 0.50f, 0.20f, 0.50f, 0.35f, 0.00f, 0.00f } },

    /* Amiga protection schemes */
    { "CopyLock (Amiga)",
      { 0.80f, 1.00f, 0.30f, 0.00f, 0.00f, 0.80f, 0.00f, 0.00f } },
    { "Rob Northen",
      { 0.75f, 1.05f, 0.40f, 0.00f, 0.00f, 0.70f, 0.00f, 0.00f } },

    /* Atari ST protection schemes */
    { "CopyLock (ST)",
      { 0.75f, 1.00f, 0.35f, 0.00f, 0.00f, 0.75f, 0.00f, 0.00f } },
    { "Macrodos",
      { 0.60f, 1.15f, 0.20f, 0.00f, 0.80f, 0.30f, 0.00f, 0.00f } },

    /* PC/Generic protection schemes */
    { "Speedlock",
      { 0.60f, 1.05f, 0.40f, 0.00f, 0.00f, 0.50f, 0.00f, 0.00f } },
    { "Dungeon Master Fuzzy",
      { 0.90f, 1.00f, 0.10f, 0.00f, 0.00f, 0.90f, 0.00f, 0.00f } },

    /* Amstrad/Spectrum */
    { "Speedlock (CPC)",
      { 0.55f, 1.05f, 0.45f, 0.00f, 0.00f, 0.45f, 0.00f, 0.00f } },

    /* Long-track / density-mismatch based */
    { "Long Track Generic",
      { 0.50f, 1.20f, 0.10f, 0.00f, 0.00f, 0.20f, 0.00f, 0.00f } },
    { "Density Mismatch",
      { 0.85f, 0.80f, 0.10f, 0.40f, 0.00f, 0.60f, 0.00f, 0.00f } },
};

#define N_KNOWN_SCHEMES ((int)(sizeof(KNOWN_SCHEMES) / sizeof(KNOWN_SCHEMES[0])))

/** Fixed upper bound for score arrays (must be >= N_KNOWN_SCHEMES) */
#define MAX_KNOWN_SCHEMES  32

/* ===================================================================
 * Internal helpers
 * =================================================================== */

/**
 * Compute Shannon entropy of a histogram.
 */
static float shannon_entropy(const float *hist, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        if (hist[i] > 0.0f)
            sum += (double)hist[i];
    }
    if (sum <= 0.0)
        return 0.0f;

    double entropy = 0.0;
    for (int i = 0; i < n; i++) {
        double p = (double)hist[i] / sum;
        if (p > 1e-12)
            entropy -= p * log(p);
    }

    /* Normalise to [0, 1] using max possible entropy for 256 bins */
    double max_entropy = log(256.0);
    if (max_entropy > 0.0)
        entropy /= max_entropy;

    return (float)entropy;
}

/**
 * Compute cosine similarity between two feature vectors.
 *
 * cos(a,b) = (a . b) / (|a| * |b|)
 *
 * Returns 0.0 if either vector has zero magnitude.
 */
static float cosine_similarity(const float *a, const float *b, int n)
{
    double dot = 0.0, mag_a = 0.0, mag_b = 0.0;

    for (int i = 0; i < n; i++) {
        dot   += (double)a[i] * (double)b[i];
        mag_a += (double)a[i] * (double)a[i];
        mag_b += (double)b[i] * (double)b[i];
    }

    double denom = sqrt(mag_a) * sqrt(mag_b);
    if (denom < 1e-12)
        return 0.0f;

    float sim = (float)(dot / denom);

    /* Clamp to [0, 1] — negative similarity means anti-correlated,
     * which we treat as zero similarity for protection matching */
    if (sim < 0.0f) sim = 0.0f;
    if (sim > 1.0f) sim = 1.0f;

    return sim;
}

/**
 * Clamp a float value to [min, max].
 */
static float clampf(float value, float lo, float hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

/**
 * Insertion sort candidates by confidence (descending).
 * Used for the small (max 5) candidate array.
 */
static void sort_candidates(uft_ml_prot_candidate_t *cands, int count)
{
    for (int i = 1; i < count; i++) {
        uft_ml_prot_candidate_t tmp = cands[i];
        int j = i - 1;
        while (j >= 0 && cands[j].confidence < tmp.confidence) {
            cands[j + 1] = cands[j];
            j--;
        }
        cands[j + 1] = tmp;
    }
}

/* ===================================================================
 * Public API
 * =================================================================== */

int uft_ml_extract_features(const float *histogram,
                             float track_length_ratio,
                             int sync_count,
                             int bad_gcr_count,
                             int duplicate_ids,
                             float jitter_rms,
                             bool has_half_track,
                             bool has_custom_sync,
                             float features_out[UFT_ML_PROT_FEATURES])
{
    if (!features_out)
        return -1;

    memset(features_out, 0, sizeof(float) * UFT_ML_PROT_FEATURES);

    /* [0] Histogram entropy (normalised 0-1) */
    if (histogram)
        features_out[0] = shannon_entropy(histogram, 256);
    else
        features_out[0] = 0.0f;

    /* [1] Track length ratio (keep as-is, typical range 0.8 - 1.3) */
    features_out[1] = track_length_ratio;

    /* [2] Sync pattern score: normalise sync_count to 0-1 range.
     *     Typical custom-sync tracks have 10-50 patterns; cap at 100. */
    features_out[2] = clampf((float)sync_count / 100.0f, 0.0f, 1.0f);

    /* [3] Bad GCR ratio: normalise against a typical track of ~7000 bytes.
     *     More than 500 bad GCR bytes = 1.0. */
    features_out[3] = clampf((float)bad_gcr_count / 500.0f, 0.0f, 1.0f);

    /* [4] Duplicate sector IDs: normalise against max ~20 sectors/track */
    features_out[4] = clampf((float)duplicate_ids / 20.0f, 0.0f, 1.0f);

    /* [5] Jitter RMS: normalise against 10 us (very high jitter) */
    features_out[5] = clampf(jitter_rms / 10.0f, 0.0f, 1.0f);

    /* [6] Half-track flag */
    features_out[6] = has_half_track ? 1.0f : 0.0f;

    /* [7] Custom sync flag */
    features_out[7] = has_custom_sync ? 1.0f : 0.0f;

    return 0;
}

int uft_ml_detect_protection(const float (*track_features)[UFT_ML_PROT_FEATURES],
                              int n_tracks,
                              uft_ml_prot_result_t *result)
{
    if (!track_features || n_tracks < 1 || !result)
        return -1;

    memset(result, 0, sizeof(*result));

    /* Step 1: Compute per-feature average across all tracks */
    float avg_features[UFT_ML_PROT_FEATURES];
    memset(avg_features, 0, sizeof(avg_features));

    for (int t = 0; t < n_tracks; t++) {
        for (int f = 0; f < UFT_ML_PROT_FEATURES; f++)
            avg_features[f] += track_features[t][f];
    }
    {
        float inv_n = 1.0f / (float)n_tracks;
        for (int f = 0; f < UFT_ML_PROT_FEATURES; f++)
            avg_features[f] *= inv_n;
    }

    /* Step 2: Also compute max features (some protections only affect
     * a few tracks — the max captures the peak anomaly). */
    float max_features[UFT_ML_PROT_FEATURES];
    memset(max_features, 0, sizeof(max_features));

    for (int t = 0; t < n_tracks; t++) {
        for (int f = 0; f < UFT_ML_PROT_FEATURES; f++) {
            if (track_features[t][f] > max_features[f])
                max_features[f] = track_features[t][f];
        }
    }

    /* Step 3: Blend average and max features (60/40) to capture both
     * disk-wide and localized protection signatures. */
    float blended[UFT_ML_PROT_FEATURES];
    for (int f = 0; f < UFT_ML_PROT_FEATURES; f++)
        blended[f] = 0.6f * avg_features[f] + 0.4f * max_features[f];

    /* Step 4: Score against all known signatures using cosine similarity */
    typedef struct {
        int   index;
        float similarity;
    } score_entry_t;

    score_entry_t scores[MAX_KNOWN_SCHEMES];

    for (int s = 0; s < N_KNOWN_SCHEMES; s++) {
        scores[s].index = s;
        scores[s].similarity = cosine_similarity(
            blended, KNOWN_SCHEMES[s].signature, UFT_ML_PROT_FEATURES);
    }

    /* Sort by similarity descending (simple selection sort, N is small) */
    for (int i = 0; i < N_KNOWN_SCHEMES - 1; i++) {
        int best = i;
        for (int j = i + 1; j < N_KNOWN_SCHEMES; j++) {
            if (scores[j].similarity > scores[best].similarity)
                best = j;
        }
        if (best != i) {
            score_entry_t tmp = scores[i];
            scores[i] = scores[best];
            scores[best] = tmp;
        }
    }

    /* Step 5: Populate candidates (top 5) */
    float best_similarity = 0.0f;
    int n_cands = 0;

    for (int i = 0; i < N_KNOWN_SCHEMES && n_cands < UFT_ML_PROT_MAX_CANDS; i++) {
        /* Only include candidates with non-trivial similarity */
        if (scores[i].similarity < 0.30f)
            break;

        const protection_ref_t *ref = &KNOWN_SCHEMES[scores[i].index];

        uft_ml_prot_candidate_t *c = &result->candidates[n_cands];
        snprintf(c->scheme_name, sizeof(c->scheme_name), "%s", ref->name);
        c->confidence = scores[i].similarity;

        if (i == 0)
            best_similarity = scores[i].similarity;

        n_cands++;
    }
    result->count = n_cands;

    /* Step 6: Determine overall classification */

    /* Compute a "weirdness" metric: how far from a normal disk?
     * A normal unprotected disk has: entropy ~0.3-0.5, length ~1.0,
     * no custom sync, no bad GCR, no half-tracks, low jitter. */
    float weirdness = 0.0f;
    weirdness += fabsf(blended[1] - 1.0f) * 5.0f;   /* length deviation */
    weirdness += blended[2] * 2.0f;                   /* custom sync */
    weirdness += blended[3] * 3.0f;                   /* bad GCR */
    weirdness += blended[4] * 2.0f;                   /* duplicate IDs */
    weirdness += blended[5] * 2.0f;                   /* jitter */
    weirdness += blended[6] * 3.0f;                   /* half tracks */
    weirdness += blended[7] * 2.0f;                   /* custom sync flag */
    weirdness = clampf(weirdness / 10.0f, 0.0f, 1.0f);

    if (best_similarity >= UFT_ML_PROT_MATCH_MIN) {
        /* High-confidence match */
        result->is_protected = true;
        result->unknown_probability = 0.0f;
        snprintf(result->summary, sizeof(result->summary),
                 "Copy protection detected: %s (confidence: %.1f%%)",
                 result->candidates[0].scheme_name,
                 (double)(best_similarity * 100.0f));
    } else if (best_similarity >= UFT_ML_PROT_UNKNOWN_TH) {
        /* Moderate match — possible but uncertain */
        result->is_protected = true;
        result->unknown_probability = 1.0f - best_similarity;
        snprintf(result->summary, sizeof(result->summary),
                 "Possible copy protection: %s (confidence: %.1f%%, "
                 "%.0f%% chance of unknown scheme)",
                 result->candidates[0].scheme_name,
                 (double)(best_similarity * 100.0f),
                 (double)(result->unknown_probability * 100.0f));
    } else if (weirdness > 0.4f) {
        /* Features are abnormal but no known scheme matches */
        result->is_protected = true;
        result->unknown_probability = weirdness;
        snprintf(result->summary, sizeof(result->summary),
                 "Unknown protection scheme suspected "
                 "(weirdness: %.1f%%, no known signature match)",
                 (double)(weirdness * 100.0f));
    } else {
        /* Clean disk — no protection detected */
        result->is_protected = false;
        result->unknown_probability = 0.0f;
        snprintf(result->summary, sizeof(result->summary),
                 "No copy protection detected (best match: %.1f%%)",
                 (double)(best_similarity * 100.0f));
    }

    /* Final sort of candidates by confidence */
    sort_candidates(result->candidates, result->count);

    return 0;
}
