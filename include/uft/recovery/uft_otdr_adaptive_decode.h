/**
 * @file uft_otdr_adaptive_decode.h
 * @brief OTDR-guided adaptive decoder — re-decodes failed sectors with
 *        aggressive PLL parameters on low-confidence bitcell regions.
 *
 * When a normal decode produces CRC failures, this module:
 * 1. Runs OTDR analysis on the track's flux data
 * 2. Identifies LOW-confidence bitcell regions (v10 confidence < threshold)
 * 3. Re-decodes with aggressive PLL (±33% tolerance, higher gains)
 * 4. Fuses normal + aggressive results weighted by OTDR quality profile
 * 5. Re-checks CRC on the fused data
 *
 * If otdr_ctx is NULL, falls back to normal decode only (backward-compatible).
 *
 * @version 1.0.0
 * @date 2026-04-08
 */

#ifndef UFT_OTDR_ADAPTIVE_DECODE_H
#define UFT_OTDR_ADAPTIVE_DECODE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* NOTE: This header is C-only due to flux_decoded_track_t by-value fields.
 * C++ code should NOT include this header directly; instead use the
 * updateDeepReadStats() method in UftOtdrPanel with scalar parameters. */
#include "uft/flux/uft_flux_decoder.h"
#include "uft/analysis/floppy_otdr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Maximum low-confidence regions per track */
#define UFT_ADAPTIVE_MAX_REGIONS    64

/** Default confidence threshold for LOW region detection */
#define UFT_ADAPTIVE_CONF_THRESHOLD 0.4f

/** Minimum consecutive low-conf bitcells to form a region */
#define UFT_ADAPTIVE_MIN_RUN        16

/** Aggressive PLL tolerance (±33%) */
#define UFT_ADAPTIVE_PLL_TOLERANCE  0.33

/** Aggressive PLL gain */
#define UFT_ADAPTIVE_PLL_GAIN       0.08

/** MFM: 16 raw bitcells per data byte (8 clock + 8 data) */
#define UFT_MFM_BITCELLS_PER_BYTE   16

/* ═══════════════════════════════════════════════════════════════════════════
 * Data Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Low-confidence region descriptor */
typedef struct {
    uint32_t start_bitcell;     /**< First low-confidence bitcell */
    uint32_t end_bitcell;       /**< Last low-confidence bitcell (inclusive) */
    float    avg_confidence;    /**< Average confidence in this region */
    int      sector_idx;        /**< Which sector this overlaps (-1 = gap) */
} uft_low_conf_region_t;

/** Adaptive decode result */
typedef struct {
    flux_decoded_track_t normal_track;      /**< Normal decode result */
    flux_decoded_track_t adaptive_track;    /**< Aggressive re-decode result */

    uint32_t sectors_total;         /**< Total sectors found */
    uint32_t sectors_crc_ok_normal; /**< CRC-OK sectors from normal decode */
    uint32_t sectors_improved;      /**< Sectors fixed by adaptive decode */
    uint32_t sectors_attempted;     /**< CRC-failed sectors that were re-tried */

    uint32_t low_conf_regions;      /**< Number of low-confidence regions */
    uft_low_conf_region_t regions[UFT_ADAPTIVE_MAX_REGIONS];

    bool     used_otdr;             /**< Whether OTDR analysis was invoked */
    float    avg_quality;           /**< Average OTDR quality across track */
} uft_adaptive_result_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Adaptive decode: normal + OTDR-guided re-decode on CRC failure.
 *
 * Flow:
 * 1. Normal decode via flux_decode_track()
 * 2. If all sectors CRC-OK → return immediately (used_otdr=false)
 * 3. Feed flux to OTDR, analyze, get quality profile
 * 4. Find low-confidence regions
 * 5. Re-decode with aggressive PLL
 * 6. Fuse sectors weighted by quality, re-check CRC
 *
 * @param flux      Raw flux data (required)
 * @param opts      Base decoder options (NULL = defaults)
 * @param otdr_ctx  Initialized OTDR context (NULL = skip OTDR, normal only)
 * @param cyl       Cylinder number
 * @param head      Head number
 * @param result    Output (caller allocates, call uft_adaptive_result_free)
 * @return FLUX_OK on success, error code otherwise
 */
flux_status_t uft_otdr_adaptive_decode(
    const flux_raw_data_t       *flux,
    const flux_decoder_options_t *opts,
    void                        *otdr_ctx,      /* uft_otdr_context_t* */
    uint8_t                      cyl,
    uint8_t                      head,
    uft_adaptive_result_t       *result);

/**
 * @brief Fuse two decoded sector data arrays weighted by OTDR quality profile.
 *
 * For each byte position, selects the byte from the decode (normal vs
 * aggressive) whose bitcell region has higher OTDR quality.
 *
 * @param normal_data       Normal decode sector data
 * @param aggressive_data   Aggressive decode sector data
 * @param data_len          Sector size in bytes
 * @param quality_profile   Per-bitcell OTDR quality (float array)
 * @param profile_len       Length of quality_profile array
 * @param bitcell_offset    Bitcell offset where this sector's data starts
 * @param bitcells_per_byte Bitcells per data byte (16 for MFM)
 * @param output            Fused output buffer (caller allocates, data_len)
 * @return Average confidence of fused result (0.0-1.0)
 */
float uft_otdr_fuse_sector(
    const uint8_t *normal_data,
    const uint8_t *aggressive_data,
    size_t data_len,
    const float *quality_profile,
    size_t profile_len,
    uint32_t bitcell_offset,
    uint32_t bitcells_per_byte,
    uint8_t *output);

/**
 * @brief Free resources in adaptive result.
 *
 * Calls flux_decoded_track_free() on both normal_track and adaptive_track.
 */
void uft_adaptive_result_free(uft_adaptive_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_OTDR_ADAPTIVE_DECODE_H */
