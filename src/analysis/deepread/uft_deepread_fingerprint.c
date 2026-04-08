/**
 * @file uft_deepread_fingerprint.c
 * @brief DeepRead Revolution Fingerprint — Implementation
 *
 * Generates a media fingerprint from timing histogram data of evenly
 * spaced reference tracks. The fingerprint consists of:
 *   1. A normalized feature vector (8 tracks x 256 histogram bins)
 *   2. A 32-byte hash (8 x CRC32, one per reference track histogram)
 *   3. A hex string representation for display
 *
 * Comparison uses cosine similarity on the feature vectors.
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#include "uft/analysis/uft_deepread_fingerprint.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

/* ===================================================================
 * CRC32 implementation (IEEE 802.3 polynomial)
 * =================================================================== */

/**
 * Precomputed CRC32 lookup table.
 * Polynomial: 0xEDB88320 (reversed representation of 0x04C11DB7).
 */
static uint32_t crc32_table[256];
static bool     crc32_table_ready = false;

static void crc32_init_table(void)
{
    if (crc32_table_ready)
        return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1)
                c = 0xEDB88320u ^ (c >> 1);
            else
                c >>= 1;
        }
        crc32_table[i] = c;
    }
    crc32_table_ready = true;
}

/**
 * Compute CRC32 of a byte buffer.
 */
static uint32_t crc32_compute(const void *data, size_t len)
{
    crc32_init_table();

    const uint8_t *buf = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < len; i++)
        crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);

    return crc ^ 0xFFFFFFFFu;
}

/* ===================================================================
 * Internal helpers
 * =================================================================== */

/**
 * Normalize a histogram: divide each bin by the sum so total = 1.0.
 * If the sum is zero, the output is all zeros.
 */
static void normalize_histogram(const uint32_t *bins, float *out, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += (double)bins[i];

    if (sum < 1.0) {
        /* Empty histogram — no flux transitions recorded */
        memset(out, 0, (size_t)n * sizeof(float));
        return;
    }

    for (int i = 0; i < n; i++)
        out[i] = (float)((double)bins[i] / sum);
}

/**
 * Convert a 32-byte hash to a 64-character hex string (+ null terminator).
 */
static void hash_to_hex(const uint8_t *hash, char *hex, int hash_len)
{
    static const char hexchars[] = "0123456789abcdef";
    for (int i = 0; i < hash_len; i++) {
        hex[i * 2]     = hexchars[(hash[i] >> 4) & 0x0F];
        hex[i * 2 + 1] = hexchars[hash[i] & 0x0F];
    }
    hex[hash_len * 2] = '\0';
}

/* ===================================================================
 * Public API
 * =================================================================== */

int uft_deepread_fingerprint(const otdr_disk_t *disk,
                             uft_media_fingerprint_t *fp)
{
    if (!disk || !fp)
        return -1;

    memset(fp, 0, sizeof(*fp));
    fp->valid = false;

    uint16_t tc = disk->track_count;
    if (tc == 0 || !disk->tracks)
        return -1;

    /* --- Select reference tracks evenly spaced across the disk --- */
    for (int i = 0; i < UFT_FINGERPRINT_TRACKS; i++) {
        uint16_t idx = (uint16_t)((uint32_t)tc * (uint32_t)i /
                                  (uint32_t)UFT_FINGERPRINT_TRACKS);
        if (idx >= tc) idx = tc - 1;
        fp->ref_tracks[i] = (uint8_t)idx;
    }

    /* --- Extract and normalize histograms into feature vector --- */
    for (int i = 0; i < UFT_FINGERPRINT_TRACKS; i++) {
        uint8_t tidx = fp->ref_tracks[i];
        if (tidx >= tc)
            continue;

        const otdr_track_t *track = &disk->tracks[tidx];
        float *dest = &fp->feature_vector[i * UFT_FINGERPRINT_BINS];

        normalize_histogram(track->histogram.bins, dest, UFT_FINGERPRINT_BINS);
    }

    /* --- Compute hash: 8 x CRC32, one per histogram segment --- */
    for (int i = 0; i < UFT_FINGERPRINT_TRACKS; i++) {
        const float *segment = &fp->feature_vector[i * UFT_FINGERPRINT_BINS];
        size_t segment_bytes = (size_t)UFT_FINGERPRINT_BINS * sizeof(float);

        uint32_t crc = crc32_compute(segment, segment_bytes);

        /* Store as 4 big-endian bytes in the hash */
        int offset = i * 4;
        fp->hash[offset + 0] = (uint8_t)((crc >> 24) & 0xFF);
        fp->hash[offset + 1] = (uint8_t)((crc >> 16) & 0xFF);
        fp->hash[offset + 2] = (uint8_t)((crc >>  8) & 0xFF);
        fp->hash[offset + 3] = (uint8_t)((crc >>  0) & 0xFF);
    }

    /* --- Convert hash to hex string --- */
    hash_to_hex(fp->hash, fp->hash_hex, 32);

    fp->valid = true;
    return 0;
}

float uft_deepread_fingerprint_compare(const uft_media_fingerprint_t *a,
                                       const uft_media_fingerprint_t *b)
{
    if (!a || !b || !a->valid || !b->valid)
        return 0.0f;

    const int n = UFT_FINGERPRINT_TRACKS * UFT_FINGERPRINT_BINS;

    /*
     * Cosine similarity:
     *   sim = dot(a, b) / (|a| * |b|)
     *
     * Use double precision for accumulation to avoid catastrophic
     * cancellation on large vectors (2048 elements).
     */
    double dot_ab = 0.0;
    double mag_a  = 0.0;
    double mag_b  = 0.0;

    for (int i = 0; i < n; i++) {
        double va = (double)a->feature_vector[i];
        double vb = (double)b->feature_vector[i];
        dot_ab += va * vb;
        mag_a  += va * va;
        mag_b  += vb * vb;
    }

    double denom = sqrt(mag_a) * sqrt(mag_b);
    if (denom < 1e-15)
        return 0.0f;

    float sim = (float)(dot_ab / denom);

    /* Clamp to [0, 1] — negative similarity means anti-correlated,
     * which should not happen for normalized histograms, but guard
     * against floating point edge cases. */
    if (sim < 0.0f) sim = 0.0f;
    if (sim > 1.0f) sim = 1.0f;

    return sim;
}
