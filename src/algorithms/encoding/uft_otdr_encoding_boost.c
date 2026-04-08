/**
 * @file uft_otdr_encoding_boost.c
 * @brief OTDR-powered encoding detection boost for format detection pipeline
 *
 * Uses histogram peak analysis + sync correlation to detect MFM/FM/GCR
 * encoding type and feed confidence back to format detection.
 *
 * @version 1.0.0
 * @date 2026-04-08
 */

#include "uft/encoding/uft_otdr_encoding_boost.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define NS_PER_BIN          100     /* 100ns per histogram bin */
#define MIN_PEAK_FRACTION   0.02f   /* Minimum 2% of total for a peak */
#define PEAK_NEIGHBOR_RATIO 1.3f    /* Peak must be 1.3x both neighbors */

/* MFM sync: 0x4489 = 0100 0100 1000 1001 (A1 with missing clock) */
static const uint32_t MFM_SYNC_INTERVALS[] = {
    /* Approximate flux intervals for 0x4489 at MFM DD (4us cell) */
    4000, 4000, 8000, 6000, 4000, 4000, 6000, 4000
};
#define MFM_SYNC_LEN 8

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Histogram Builder
 * ═══════════════════════════════════════════════════════════════════════════ */

static void build_histogram(const uint32_t *flux_ns, uint32_t count,
                            uint32_t bins[UFT_ENC_BOOST_HIST_BINS],
                            uint32_t *total)
{
    memset(bins, 0, UFT_ENC_BOOST_HIST_BINS * sizeof(uint32_t));
    *total = 0;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t bin = flux_ns[i] / NS_PER_BIN;
        if (bin < UFT_ENC_BOOST_HIST_BINS) {
            bins[bin]++;
            (*total)++;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Peak Finder
 * ═══════════════════════════════════════════════════════════════════════════ */

static int find_peaks(const uint32_t bins[UFT_ENC_BOOST_HIST_BINS],
                      uint32_t total,
                      uft_enc_peak_t *peaks, int max_peaks)
{
    uint32_t min_count = (uint32_t)(total * MIN_PEAK_FRACTION);
    int n = 0;

    for (int b = 2; b < UFT_ENC_BOOST_HIST_BINS - 2 && n < max_peaks; b++) {
        if (bins[b] < min_count) continue;

        /* Must be local maximum (larger than both neighbors) */
        uint32_t left  = (bins[b - 1] > bins[b - 2]) ? bins[b - 1] : bins[b - 2];
        uint32_t right = (bins[b + 1] > bins[b + 2]) ? bins[b + 1] : bins[b + 2];

        if (bins[b] >= (uint32_t)(left * PEAK_NEIGHBOR_RATIO) &&
            bins[b] >= (uint32_t)(right * PEAK_NEIGHBOR_RATIO)) {
            peaks[n].bin = b;
            peaks[n].ns = b * NS_PER_BIN + NS_PER_BIN / 2;
            peaks[n].count = bins[b];
            peaks[n].prominence = (float)bins[b] / (float)total;
            n++;
        }
    }

    /* Sort by ns ascending */
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (peaks[j].ns < peaks[i].ns) {
                uft_enc_peak_t tmp = peaks[i];
                peaks[i] = peaks[j];
                peaks[j] = tmp;
            }
        }
    }

    return n;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Sync Pattern Correlation
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint32_t count_mfm_syncs(const uint32_t *flux_ns, uint32_t count,
                                uint32_t cell_ns, float *quality)
{
    uint32_t syncs = 0;
    float total_corr = 0.0f;
    float tolerance = 0.30f; /* ±30% timing match */

    if (count < MFM_SYNC_LEN + 3) {
        *quality = 0.0f;
        return 0;
    }

    for (uint32_t i = 0; i <= count - MFM_SYNC_LEN; i++) {
        int match = 0;
        float corr = 0.0f;

        for (int s = 0; s < MFM_SYNC_LEN; s++) {
            /* Scale expected interval to detected cell size */
            float expected = (float)MFM_SYNC_INTERVALS[s] * ((float)cell_ns / 4000.0f);
            float actual = (float)flux_ns[i + s];
            float ratio = actual / expected;

            if (ratio >= (1.0f - tolerance) && ratio <= (1.0f + tolerance)) {
                match++;
                corr += 1.0f - fabsf(ratio - 1.0f);
            }
        }

        if (match >= 6) { /* At least 6 of 8 intervals match */
            syncs++;
            total_corr += corr / MFM_SYNC_LEN;
        }
    }

    *quality = (syncs > 0) ? (total_corr / syncs) : 0.0f;
    return syncs;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Encoding Scoring
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    otdr_encoding_t encoding;
    float           score;
    uint32_t        cell_ns;
    uint32_t        data_rate;
    bool            hd;
} enc_candidate_t;

static float score_mfm(const uft_enc_peak_t *peaks, int npk,
                       float sync_quality, uint32_t sync_count, bool *is_hd)
{
    float score = 0.0f;
    *is_hd = false;

    if (npk < 2) return 0.0f;

    /* Peak ratio analysis */
    float ratio = (float)peaks[1].ns / (float)peaks[0].ns;
    if (ratio >= 1.3f && ratio <= 1.7f) {
        score += 0.35f; /* Clear MFM 2T:3T ratio ~1.5 */
    }

    /* Third peak check (4T) */
    if (npk >= 3) {
        float ratio3 = (float)peaks[2].ns / (float)peaks[0].ns;
        if (ratio3 >= 1.8f && ratio3 <= 2.2f) {
            score += 0.15f; /* 2T:4T ratio ~2.0 */
        }
    }

    /* Density detection */
    if (peaks[0].ns < 3000) {
        *is_hd = true; /* HD: 2T peak < 3µs */
    }

    /* Sync pattern contribution */
    if (sync_count >= 10) {
        score += 0.30f * sync_quality;
    } else if (sync_count >= 3) {
        score += 0.15f * sync_quality;
    }

    /* Peak prominence bonus */
    if (peaks[0].prominence > 0.10f) {
        score += 0.10f;
    }

    return (score > 1.0f) ? 1.0f : score;
}

static float score_fm(const uft_enc_peak_t *peaks, int npk)
{
    float score = 0.0f;

    if (npk < 2) return 0.0f;

    /* FM: two peaks at ~2.0 ratio (clock+0 vs clock+1) */
    float ratio = (float)peaks[1].ns / (float)peaks[0].ns;
    if (ratio >= 1.8f && ratio <= 2.2f) {
        score += 0.40f;
    }

    /* FM timing: first peak ~4µs (125kHz) or ~8µs (250kHz) */
    if (peaks[0].ns >= 3500 && peaks[0].ns <= 4500) {
        score += 0.20f;
    } else if (peaks[0].ns >= 7500 && peaks[0].ns <= 8500) {
        score += 0.20f;
    }

    /* Only 2 significant peaks expected */
    if (npk == 2) {
        score += 0.10f;
    }

    return (score > 1.0f) ? 1.0f : score;
}

static float score_gcr_c64(const uft_enc_peak_t *peaks, int npk)
{
    float score = 0.0f;

    if (npk < 2) return 0.0f;

    /* GCR C64: narrow peak separation, peaks ~3.2-4.0µs range */
    float ratio = (float)peaks[1].ns / (float)peaks[0].ns;
    if (ratio >= 1.0f && ratio <= 1.35f) {
        score += 0.30f; /* Narrow separation = GCR */
    }

    /* C64 timing range */
    if (peaks[0].ns >= 2800 && peaks[0].ns <= 4200) {
        score += 0.25f;
    }

    /* GCR has 4+ closely spaced peaks */
    if (npk >= 4) {
        score += 0.15f;
    }

    return (score > 1.0f) ? 1.0f : score;
}

static float score_gcr_apple(const uft_enc_peak_t *peaks, int npk)
{
    float score = 0.0f;

    if (npk < 2) return 0.0f;

    /* Apple GCR: similar to C64 but different timing */
    float ratio = (float)peaks[1].ns / (float)peaks[0].ns;
    if (ratio >= 1.0f && ratio <= 1.4f) {
        score += 0.25f;
    }

    /* Apple timing range (~3.5-4.0µs base) */
    if (peaks[0].ns >= 3200 && peaks[0].ns <= 4500) {
        score += 0.20f;
    }

    /* Apple GCR typically has 4-5 peaks */
    if (npk >= 4 && npk <= 6) {
        score += 0.15f;
    }

    return (score > 1.0f) ? 1.0f : score;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_otdr_detect_encoding(const uint32_t *flux_ns, uint32_t count,
                             uft_encoding_boost_result_t *result)
{
    if (!flux_ns || !result) return -1;
    if (count < UFT_ENC_BOOST_MIN_FLUX) return -2;

    memset(result, 0, sizeof(*result));

    /* Build histogram */
    uint32_t bins[UFT_ENC_BOOST_HIST_BINS];
    uint32_t total = 0;
    build_histogram(flux_ns, count, bins, &total);

    if (total < UFT_ENC_BOOST_MIN_FLUX) return -2;

    /* Find peaks */
    uft_enc_peak_t peaks[UFT_ENC_BOOST_MAX_PEAKS];
    int npk = find_peaks(bins, total, peaks, UFT_ENC_BOOST_MAX_PEAKS);

    if (npk < 2) {
        result->encoding = OTDR_ENC_AUTO;
        result->confidence = 0.0f;
        snprintf(result->description, sizeof(result->description),
                 "Insufficient peaks (%d)", npk);
        return 0;
    }

    /* Store peak separation */
    result->peak_separation = (float)peaks[1].ns / (float)peaks[0].ns;

    /* Score each encoding candidate */
    float sync_quality = 0.0f;
    uint32_t sync_count = count_mfm_syncs(flux_ns, count, peaks[0].ns, &sync_quality);

    bool mfm_hd = false;
    enc_candidate_t candidates[] = {
        { OTDR_ENC_MFM_DD,    score_mfm(peaks, npk, sync_quality, sync_count, &mfm_hd),
          peaks[0].ns, 500000, false },
        { OTDR_ENC_MFM_HD,    mfm_hd ? score_mfm(peaks, npk, sync_quality, sync_count, &mfm_hd) : 0.0f,
          peaks[0].ns, 1000000, true },
        { OTDR_ENC_FM_SD,     score_fm(peaks, npk),
          peaks[0].ns * 2, 125000, false },
        { OTDR_ENC_GCR_C64,   score_gcr_c64(peaks, npk),
          peaks[0].ns, 250000, false },
        { OTDR_ENC_GCR_APPLE, score_gcr_apple(peaks, npk),
          peaks[0].ns, 250000, false },
    };
    int ncand = sizeof(candidates) / sizeof(candidates[0]);

    /* Pick best candidate */
    int best = 0;
    for (int i = 1; i < ncand; i++) {
        if (candidates[i].score > candidates[best].score) {
            best = i;
        }
    }

    /* Handle HD/DD disambiguation */
    if (candidates[best].encoding == OTDR_ENC_MFM_DD && mfm_hd) {
        /* Re-check: if HD scored higher, use HD */
        if (candidates[1].score > candidates[0].score) {
            best = 1;
        }
    }

    /* Fill result */
    result->encoding = candidates[best].encoding;
    result->confidence = candidates[best].score;
    result->cell_ns = candidates[best].cell_ns;
    result->data_rate_bps = candidates[best].data_rate;
    result->is_high_density = candidates[best].hd;
    result->sync_quality = sync_quality;
    result->sync_count = sync_count;

    snprintf(result->description, sizeof(result->description),
             "%s (%.0f%% conf, %u syncs)",
             uft_otdr_encoding_name(result->encoding),
             result->confidence * 100.0f,
             result->sync_count);

    return 0;
}

float uft_encoding_boost_score(otdr_encoding_t format_encoding,
                               const uft_encoding_boost_result_t *otdr_result,
                               float base_score)
{
    if (!otdr_result || otdr_result->confidence < 0.1f) {
        return base_score; /* No meaningful OTDR data */
    }

    float adjusted = base_score;

    /* Check if OTDR encoding matches format profile */
    bool match = (format_encoding == otdr_result->encoding);

    /* Special case: MFM DD and Amiga DD are compatible */
    if ((format_encoding == OTDR_ENC_MFM_DD && otdr_result->encoding == OTDR_ENC_AMIGA_DD) ||
        (format_encoding == OTDR_ENC_AMIGA_DD && otdr_result->encoding == OTDR_ENC_MFM_DD)) {
        match = true;
    }

    if (match) {
        adjusted += otdr_result->confidence * 0.10f;
    } else if (otdr_result->confidence > 0.7f) {
        adjusted -= otdr_result->confidence * 0.20f;
    }

    /* Clamp */
    if (adjusted < 0.0f) adjusted = 0.0f;
    if (adjusted > 1.0f) adjusted = 1.0f;

    return adjusted;
}

const char *uft_otdr_encoding_name(otdr_encoding_t enc)
{
    switch (enc) {
        case OTDR_ENC_MFM_DD:    return "MFM DD";
        case OTDR_ENC_MFM_HD:    return "MFM HD";
        case OTDR_ENC_FM_SD:     return "FM SD";
        case OTDR_ENC_GCR_C64:   return "GCR C64";
        case OTDR_ENC_GCR_APPLE: return "GCR Apple";
        case OTDR_ENC_AMIGA_DD:  return "Amiga MFM DD";
        case OTDR_ENC_AUTO:      return "Auto";
        default:                 return "Unknown";
    }
}
