/**
 * @file uft_deepread_soft_decode.c
 * @brief DeepRead Probabilistic Bit Recovery (Soft-Decision LLR)
 *
 * Implements soft-decision decoding of raw flux timing data into
 * per-bit log-likelihood ratios and subsequent MFM sector recovery.
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#include "uft/analysis/uft_deepread_soft_decode.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -----------------------------------------------------------------------
 * Internal constants
 * ----------------------------------------------------------------------- */

/** Maximum absolute LLR value (prevents numeric overflow downstream). */
#define LLR_CLAMP       10.0f

/** Threshold below which a bit is considered low-confidence. */
#define LOW_CONF_THRESH  0.5f

/** Minimum sigma to avoid division by zero (nanoseconds). */
#define SIGMA_FLOOR      1.0f

/** MFM bitcells per data byte (8 data + 8 clock). */
#define MFM_BITCELLS_PER_BYTE  16

/* -----------------------------------------------------------------------
 * uft_deepread_compute_llr
 * ----------------------------------------------------------------------- */

int uft_deepread_compute_llr(
    const uint32_t *flux_ns,
    uint32_t flux_count,
    const float *quality_profile,
    uint32_t bitcell_count,
    uint32_t nominal_cell_ns,
    uft_soft_bits_t *result)
{
    /* ---- Validate inputs ---- */
    if (!flux_ns || flux_count == 0)
        return -1;
    if (nominal_cell_ns == 0)
        return -1;
    if (!quality_profile || bitcell_count == 0)
        return -1;
    if (!result)
        return -1;

    memset(result, 0, sizeof(*result));

    /* ---- Allocate LLR array ----
     * Use bitcell_count as the upper-bound estimate.  The actual number
     * of decoded bits may be slightly smaller due to rounding.          */
    size_t alloc_count = (size_t)bitcell_count + 64;   /* small padding */
    float *llr = (float *)malloc(alloc_count * sizeof(float));
    if (!llr)
        return -2;

    /* ---- Main flux-to-LLR loop ---- */
    size_t   bit_idx      = 0;
    double   sum_abs_llr  = 0.0;
    uint32_t low_count    = 0;

    for (uint32_t fi = 0; fi < flux_count; fi++) {
        /* (a) Estimate number of bitcells enclosed in this flux interval */
        double interval = (double)flux_ns[fi];
        double nominal  = (double)nominal_cell_ns;
        uint32_t cells  = (uint32_t)(interval / nominal + 0.5);
        if (cells == 0)
            cells = 1;

        /* (b) Timing error for the whole interval */
        double error = interval - (double)cells * nominal;

        /* (c) Emit LLR for each bitcell in this interval */
        for (uint32_t ci = 0; ci < cells; ci++) {
            if (bit_idx >= alloc_count) {
                /* Grow the buffer if estimate was too small */
                size_t new_alloc = alloc_count * 2;
                float *tmp = (float *)realloc(llr, new_alloc * sizeof(float));
                if (!tmp) {
                    free(llr);
                    memset(result, 0, sizeof(*result));
                    return -2;
                }
                llr = tmp;
                alloc_count = new_alloc;
            }

            /* Quality-profile index: clamp to available range */
            uint32_t qidx = (bit_idx < bitcell_count) ? (uint32_t)bit_idx
                                                       : bitcell_count - 1;

            /* Local jitter sigma from quality profile (dB -> linear).
             * quality_profile[qidx] is SNR in dB; convert to a sigma
             * expressed as a fraction of the nominal cell width:
             *   sigma = nominal_cell_ns * 10^(-quality_dB / 20)            */
            double quality_db = (double)quality_profile[qidx];
            double sigma = nominal * pow(10.0, -quality_db / 20.0);
            if (sigma < (double)SIGMA_FLOOR)
                sigma = (double)SIGMA_FLOOR;

            /* Quality normalisation factor for "no-transition" bits */
            double quality_norm = quality_db / 20.0;
            if (quality_norm < 0.1)
                quality_norm = 0.1;

            float bit_llr;
            bool is_last = (ci == cells - 1);

            if (is_last) {
                /* Transition bitcell ("1"): flux transition occurred here.
                 * Confidence grows as timing error shrinks relative to sigma. */
                double raw = fabs(error) / sigma;
                /* Invert: small error = high confidence of being a real 1 */
                bit_llr = (float)(quality_norm - raw);
                if (bit_llr < 0.1f)
                    bit_llr = 0.1f;   /* at minimum slightly positive */
            } else {
                /* Non-transition bitcell ("0"): no flux edge here.
                 * Negative LLR scaled by local quality. */
                bit_llr = (float)(-1.0 * quality_norm);
            }

            /* Clamp to [-LLR_CLAMP, +LLR_CLAMP] */
            if (bit_llr > LLR_CLAMP)
                bit_llr = LLR_CLAMP;
            else if (bit_llr < -LLR_CLAMP)
                bit_llr = -LLR_CLAMP;

            llr[bit_idx] = bit_llr;

            float abs_llr = fabsf(bit_llr);
            sum_abs_llr += (double)abs_llr;
            if (abs_llr < LOW_CONF_THRESH)
                low_count++;

            bit_idx++;
        }
    }

    /* ---- Fill result structure ---- */
    result->llr            = llr;
    result->bit_count      = bit_idx;
    result->mean_confidence = (bit_idx > 0) ? (float)(sum_abs_llr / (double)bit_idx)
                                            : 0.0f;
    result->low_conf_bits  = low_count;

    return 0;
}

/* -----------------------------------------------------------------------
 * uft_deepread_soft_decode_sector
 * ----------------------------------------------------------------------- */

int uft_deepread_soft_decode_sector(
    const uft_soft_bits_t *soft_bits,
    uint32_t sector_start_bit,
    uint32_t sector_size_bytes,
    uint8_t *output,
    float *per_byte_confidence)
{
    /* ---- Validate inputs ---- */
    if (!soft_bits || !soft_bits->llr)
        return -1;
    if (!output || sector_size_bytes == 0)
        return -1;

    /* Total bitcells required for this sector (MFM: 16 per byte) */
    size_t bits_needed = (size_t)sector_start_bit
                       + (size_t)sector_size_bytes * MFM_BITCELLS_PER_BYTE;
    if (bits_needed > soft_bits->bit_count)
        return -1;

    const float *llr = soft_bits->llr;

    /* ---- Decode each byte ---- */
    for (uint32_t byte_idx = 0; byte_idx < sector_size_bytes; byte_idx++) {
        uint32_t base = sector_start_bit
                      + byte_idx * MFM_BITCELLS_PER_BYTE;

        uint8_t  decoded  = 0;
        double   conf_sum = 0.0;

        /* MFM layout within 16 bitcells:
         *   positions 0,2,4,6,8,10,12,14 = clock bitcells
         *   positions 1,3,5,7,9,11,13,15 = data bitcells
         * We extract 8 data bits from the even-indexed positions
         * in "data" terms, which are the odd-indexed raw bitcells
         * (bit 0 of data = raw position 1, bit 1 = raw position 3, ...). */
        for (int di = 0; di < 8; di++) {
            uint32_t raw_pos = base + (uint32_t)(di * 2 + 1);
            float    bit_llr = llr[raw_pos];

            /* Positive LLR -> data bit is 1; negative -> 0 */
            if (bit_llr > 0.0f)
                decoded |= (uint8_t)(0x80u >> di);

            conf_sum += (double)fabsf(bit_llr);
        }

        output[byte_idx] = decoded;

        if (per_byte_confidence)
            per_byte_confidence[byte_idx] = (float)(conf_sum / 8.0);
    }

    return 0;
}

/* -----------------------------------------------------------------------
 * uft_soft_bits_free
 * ----------------------------------------------------------------------- */

void uft_soft_bits_free(uft_soft_bits_t *bits)
{
    if (!bits)
        return;

    free(bits->llr);
    memset(bits, 0, sizeof(*bits));
}
