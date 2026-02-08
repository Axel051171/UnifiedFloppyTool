/**
 * @file uft_encoding_detect.h
 * @brief Automatic encoding detection (MFM/FM/GCR)
 * 
 * Fixes algorithmic weakness #6: GCR/MFM encoding confusion
 * 
 * Features:
 * - Score-based encoding detection
 * - Pulse interval analysis
 * - Sync pattern recognition
 * - Multi-format support
 */

#ifndef UFT_ENCODING_DETECT_H
#define UFT_ENCODING_DETECT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Supported encodings
 */
typedef enum {
    UFT_ENCODING_UNKNOWN = 0,
    UFT_ENCODING_FM,            /**< Frequency Modulation (single density) */
    UFT_ENCODING_MFM,           /**< Modified FM (IBM PC, Atari ST, etc.) */
    UFT_ENCODING_GCR_APPLE,     /**< Apple II/III GCR (6-and-2, 5-and-3) */
    UFT_ENCODING_GCR_C64,       /**< Commodore 64/1541 GCR */
    UFT_ENCODING_GCR_MAC,       /**< Macintosh GCR */
    UFT_ENCODING_AMIGA_MFM,     /**< Amiga MFM (different sync) */
    UFT_ENCODING_M2FM,          /**< M2FM (Intel) */
    UFT_ENCODING_MAX
} uft_encoding_type_t;

/**
 * @brief Encoding detection result
 */
typedef struct {
    uft_encoding_type_t encoding;
    int score;                  /**< Detection score (higher = more confident) */
    double bit_rate;            /**< Detected/estimated bit rate */
    double cell_size;           /**< Data cell size in samples */
    const char *name;           /**< Human-readable name */
    const char *description;    /**< Additional info */
    
    /* Diagnostic info */
    int pulse_score;            /**< Score from pulse analysis */
    int pattern_score;          /**< Score from pattern matching */
    int structure_score;        /**< Score from structure analysis */
    
} uft_encoding_result_t;

/**
 * @brief Encoding detection candidates (all evaluated)
 */
typedef struct {
    uft_encoding_result_t results[UFT_ENCODING_MAX];
    size_t count;
    uft_encoding_result_t *best;  /**< Pointer to best match */
    
    /* Analysis data */
    size_t total_pulses;
    double avg_pulse_interval;
    double pulse_interval_variance;
    
} uft_encoding_candidates_t;

/**
 * @brief Pulse interval histogram
 */
typedef struct {
    size_t buckets[64];         /**< Histogram buckets */
    size_t bucket_width;        /**< Samples per bucket */
    size_t total_pulses;
    
    /* Detected peaks (pulse intervals) */
    size_t peak_positions[8];
    size_t peak_count;
    
} uft_pulse_histogram_t;

/**
 * @brief Detect encoding from raw flux/bitstream data
 * @param data Raw data
 * @param len Data length in bytes
 * @param sample_rate Sample rate in Hz
 * @return Best encoding match
 */
uft_encoding_result_t uft_encoding_detect(const uint8_t *data,
                                          size_t len,
                                          double sample_rate);

/**
 * @brief Get all encoding candidates with scores
 * @param data Raw data
 * @param len Data length in bytes
 * @param sample_rate Sample rate in Hz
 * @param candidates Output candidates
 */
void uft_encoding_detect_all(const uint8_t *data,
                             size_t len,
                             double sample_rate,
                             uft_encoding_candidates_t *candidates);

/**
 * @brief Build pulse interval histogram
 * @param data Raw bit data
 * @param len_bits Length in bits
 * @param histogram Output histogram
 */
void uft_encoding_build_histogram(const uint8_t *data,
                                  size_t len_bits,
                                  uft_pulse_histogram_t *histogram);

/**
 * @brief Analyze histogram to find peaks
 * @param histogram Histogram to analyze
 */
void uft_encoding_find_peaks(uft_pulse_histogram_t *histogram);

/**
 * @brief Check for specific sync patterns
 * @param data Raw data
 * @param len_bits Length in bits
 * @param encoding Encoding to check for
 * @return Number of sync patterns found
 */
size_t uft_encoding_find_syncs(const uint8_t *data,
                               size_t len_bits,
                               uft_encoding_type_t encoding);

/**
 * @brief Get encoding name
 */
const char* uft_encoding_name(uft_encoding_type_t encoding);

/**
 * @brief Get typical bit rate for encoding
 */
double uft_encoding_typical_bitrate(uft_encoding_type_t encoding);

/**
 * @brief Print detection results
 */
void uft_encoding_dump_results(const uft_encoding_candidates_t *candidates);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ENCODING_DETECT_H */
