/**
 * @file uft_deepread_soft_decode.h
 * @brief DeepRead Probabilistic Bit Recovery (Soft-Decision LLR)
 *
 * Converts raw flux timing data into per-bit log-likelihood ratios (LLR)
 * that quantify how confidently each bit can be identified as 0 or 1.
 * A positive LLR indicates the bit is more likely a 1; negative means
 * more likely a 0.  The magnitude reflects confidence.
 *
 * The soft-decision output feeds into MFM sector decoding where each
 * byte is recovered from the sign of its data-bitcell LLRs, with a
 * per-byte confidence metric for downstream error-correction or
 * multi-revolution fusion.
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#ifndef UFT_DEEPREAD_SOFT_DECODE_H
#define UFT_DEEPREAD_SOFT_DECODE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Types
 * ----------------------------------------------------------------------- */

/** Soft-decision bit array with per-bit log-likelihood ratios. */
typedef struct {
    float   *llr;               /**< Log-likelihood ratio per bit (positive=more likely 1) */
    size_t   bit_count;         /**< Number of decoded bitcells */
    float    mean_confidence;   /**< Average |LLR| across all bits */
    uint32_t low_conf_bits;     /**< Number of bits with |LLR| < 0.5 */
} uft_soft_bits_t;

/* -----------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------- */

/**
 * Compute per-bit log-likelihood ratios from raw flux timing data.
 *
 * For each flux interval the number of enclosed bitcells is estimated
 * from the nominal cell width, and the timing residual is converted
 * into an LLR using the local quality/jitter profile.  Transition
 * bitcells (last in each interval) receive a positive LLR scaled by
 * the timing error; non-transition bitcells receive a negative LLR
 * scaled by local quality.
 *
 * @param flux_ns          Array of flux transition intervals in nanoseconds.
 * @param flux_count       Number of entries in flux_ns.
 * @param quality_profile  Per-bitcell quality in dB (higher = better).
 *                         Array must contain at least bitcell_count entries.
 * @param bitcell_count    Expected total bitcells on the track (used for
 *                         quality_profile indexing and allocation estimate).
 * @param nominal_cell_ns  Nominal bitcell width in nanoseconds.
 * @param result           Output soft-bits structure (caller must later call
 *                         uft_soft_bits_free()).
 * @return 0 on success, -1 on invalid input, -2 on allocation failure.
 */
int uft_deepread_compute_llr(
    const uint32_t *flux_ns,
    uint32_t flux_count,
    const float *quality_profile,
    uint32_t bitcell_count,
    uint32_t nominal_cell_ns,
    uft_soft_bits_t *result);

/**
 * Decode one MFM sector from soft-decision bits.
 *
 * Extracts 16 bitcells per output byte (MFM encoding: clock bits at
 * odd positions, data bits at even positions).  Each data bit is
 * decided by the sign of its LLR, and the per-byte confidence is the
 * mean |LLR| over the 8 data bitcells in that byte.
 *
 * @param soft_bits           Soft-bit array from uft_deepread_compute_llr().
 * @param sector_start_bit    Bit index where the sector begins.
 * @param sector_size_bytes   Number of data bytes to decode.
 * @param output              Output buffer, must hold sector_size_bytes bytes.
 * @param per_byte_confidence Output per-byte confidence array, must hold
 *                            sector_size_bytes entries (may be NULL to skip).
 * @return 0 on success, -1 on invalid input.
 */
int uft_deepread_soft_decode_sector(
    const uft_soft_bits_t *soft_bits,
    uint32_t sector_start_bit,
    uint32_t sector_size_bytes,
    uint8_t *output,
    float *per_byte_confidence);

/**
 * Free resources owned by a soft-bits structure.
 *
 * Releases the LLR array and zeroes all fields.
 *
 * @param bits  Structure to free (NULL-safe).
 */
void uft_soft_bits_free(uft_soft_bits_t *bits);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DEEPREAD_SOFT_DECODE_H */
