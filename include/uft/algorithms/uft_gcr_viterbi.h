/**
 * @file uft_gcr_viterbi.h
 * @brief Context-Aware GCR Decoder with Viterbi Error Correction
 */

#ifndef UFT_GCR_VITERBI_H
#define UFT_GCR_VITERBI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * GCR Format Enumeration
 * ============================================================================ */

typedef enum {
    UFT_GCR_FORMAT_UNKNOWN     = 0,
    UFT_GCR_FORMAT_C64         = 1,  /**< C64 5-bit GCR (4→5 encoding) */
    UFT_GCR_FORMAT_APPLE_DOS   = 2,  /**< Apple DOS 3.3 (6-and-2 encoding) */
    UFT_GCR_FORMAT_APPLE_PRODOS= 3,  /**< Apple ProDOS (6-and-2 encoding) */
} uft_gcr_format_t;

/* ============================================================================
 * Viterbi Configuration
 * ============================================================================ */

typedef struct {
    uft_gcr_format_t format_hint;       /**< Expected format (UNKNOWN=autodetect) */
    double           cell_ns_min;       /**< Minimum bit cell time (ns) */
    double           cell_ns_max;       /**< Maximum bit cell time (ns) */
    float            insertion_penalty;  /**< Viterbi insertion cost */
    float            deletion_penalty;   /**< Viterbi deletion cost */
    float            substitution_base;  /**< Viterbi substitution base cost */
    float            min_confidence;     /**< Min confidence threshold */
    bool             use_multi_rev;      /**< Use multiple revolutions */
    int              rev_count;          /**< Number of revolutions */
} uft_gcr_viterbi_config_t;

/* ============================================================================
 * Viterbi Output
 * ============================================================================ */

typedef struct {
    uint8_t         *data;               /**< Decoded data buffer (caller-allocated) */
    size_t           data_size;          /**< Buffer size / decoded bytes */
    float           *confidence;         /**< Per-byte confidence (optional, caller-allocated) */
    
    uft_gcr_format_t detected_format;   /**< Detected GCR format */
    int              sync_patterns_found; /**< Number of sync patterns found */
    size_t           total_bits_processed;/**< Total bits consumed */
    int              viterbi_corrections; /**< Number of Viterbi corrections */
    int              unrecoverable_errors;/**< Unrecoverable error count */
} uft_gcr_viterbi_output_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Get default Viterbi configuration
 */
uft_gcr_viterbi_config_t uft_gcr_viterbi_config_default(void);

/**
 * @brief Detect GCR format from raw bitstream
 */
uft_gcr_format_t uft_gcr_detect_format(const uint8_t *bits, size_t bit_count);

/**
 * @brief Decode a single GCR byte using Viterbi soft decision
 * @return Number of corrections made (0 = clean)
 */
int uft_gcr_viterbi_decode_byte(
    const uint8_t *bits,
    size_t bit_offset,
    const float *confidence,
    uint8_t *out_byte,
    float *out_conf);

/**
 * @brief Decode a full GCR track with Viterbi error correction
 * @return Number of successfully decoded sectors, or -1 on error
 */
int uft_gcr_viterbi_decode(
    const uint8_t *bits,
    size_t bit_count,
    const float *confidence,
    const uft_gcr_viterbi_config_t *cfg,
    uft_gcr_viterbi_output_t *output);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GCR_VITERBI_H */
