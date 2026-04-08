/**
 * @file uft_deepread_fingerprint.h
 * @brief DeepRead Revolution Fingerprint — Media Identification
 *
 * Generates a unique fingerprint for a physical floppy disk based on
 * the timing histogram characteristics of reference tracks. Two reads
 * of the same physical media produce matching fingerprints even if the
 * read quality varies, because the histogram shape is determined by
 * the media's magnetic properties and drive mechanics.
 *
 * The fingerprint is NOT cryptographic — it uses CRC32 for compactness.
 * It is intended for media identification and duplicate detection.
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#ifndef UFT_DEEPREAD_FINGERPRINT_H
#define UFT_DEEPREAD_FINGERPRINT_H

#include <stdint.h>
#include <stdbool.h>
#include "floppy_otdr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * Constants
 * =================================================================== */

/** Number of reference tracks sampled for fingerprinting */
#define UFT_FINGERPRINT_TRACKS   8

/** Number of histogram bins per track (matches otdr_track_t.histogram.bins) */
#define UFT_FINGERPRINT_BINS   256

/* ===================================================================
 * Types
 * =================================================================== */

/**
 * Media fingerprint — a compact identifier derived from timing histograms.
 *
 * The hash is 32 bytes: 8 x CRC32 (one per reference track histogram).
 * The feature_vector contains the normalized histograms for similarity
 * comparison via cosine distance.
 */
typedef struct {
    uint8_t  hash[32];          /**< 8 x CRC32 values (4 bytes each) */
    char     hash_hex[65];      /**< Hex string representation of hash */
    float    feature_vector[UFT_FINGERPRINT_TRACKS * UFT_FINGERPRINT_BINS];
    uint8_t  ref_tracks[UFT_FINGERPRINT_TRACKS]; /**< Which tracks were sampled */
    bool     valid;             /**< True if fingerprint was computed successfully */
} uft_media_fingerprint_t;

/* ===================================================================
 * API Functions
 * =================================================================== */

/**
 * Compute a media fingerprint from an analyzed disk.
 *
 * Selects reference tracks evenly spaced across the disk, extracts
 * and normalizes their timing histograms, and computes a hash.
 * Requires that OTDR histogram analysis has been run on all tracks.
 *
 * @param disk  Analyzed disk with histogram data
 * @param fp    Output fingerprint (caller allocates)
 * @return 0 on success, -1 on error
 */
int uft_deepread_fingerprint(const otdr_disk_t *disk,
                             uft_media_fingerprint_t *fp);

/**
 * Compare two fingerprints using cosine similarity.
 *
 * @param a  First fingerprint
 * @param b  Second fingerprint
 * @return Similarity in range [0.0, 1.0], where 1.0 = identical
 */
float uft_deepread_fingerprint_compare(const uft_media_fingerprint_t *a,
                                       const uft_media_fingerprint_t *b);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DEEPREAD_FINGERPRINT_H */
