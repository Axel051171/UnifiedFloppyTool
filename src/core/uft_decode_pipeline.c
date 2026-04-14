/**
 * @file uft_decode_pipeline.c
 * @brief UFT Decode Pipeline Implementation
 *
 * Connects PLL, CRC, format parser and OTDR into a unified
 * decode session with retry logic.
 */

#include "uft_decode_pipeline.h"
#include "uft/uft_decode_session.h"
#include "uft/uft_error.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Session lifecycle
 * ============================================================================ */

void uft_session_init(uft_decode_session_t *s,
                      int cylinder, int head,
                      uft_pll_preset_t preset,
                      const char *platform)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->cylinder   = cylinder;
    s->head       = head;
    s->pll_preset = preset;
    s->platform   = platform;
    s->last_error = UFT_OK;
}

void uft_session_reset_for_retry(uft_decode_session_t *s)
{
    if (!s) return;
    /* Flux stays — reused for retry */
    free(s->bitstream);
    s->bitstream = NULL;
    s->bit_count = 0;
    memset(&s->pll, 0, sizeof(s->pll));
    memset(&s->crc, 0, sizeof(s->crc));
    s->encoding_errors  = 0;
    s->sync_marks_found = 0;
    s->sectors_found    = 0;
    s->sectors_ok       = 0;
    s->track            = NULL;
    s->retry_requested  = false;
    s->retry_reason     = UFT_RETRY_NONE;
}

void uft_session_destroy(uft_decode_session_t *s)
{
    if (!s) return;
    free(s->bitstream);
    s->bitstream = NULL;
}

/* ============================================================================
 * PLL stage: Flux → Bits
 * ============================================================================ */

uft_error_t uft_pipeline_run_pll(uft_decode_session_t *s)
{
    if (!s || !s->flux_ns || s->flux_count == 0)
        return UFT_ERR_INVALID_ARG;

    /* Allocate bitstream buffer */
    size_t max_bits = s->flux_count * 4;
    if (max_bits > UFT_SESSION_MAX_BITS) max_bits = UFT_SESSION_MAX_BITS;

    s->bitstream = calloc(1, (max_bits + 7) / 8);
    if (!s->bitstream) return UFT_ERR_MEMORY;
    s->bit_count = 0;

    /* Use the simple PLL from uft_pll.h (uft_flux_to_bits_pll) */
    uft_pll_cfg_t cfg;
    switch (s->pll_preset) {
        case UFT_PLL_PRESET_IBM_HD:
            cfg = uft_pll_cfg_default_mfm_hd();
            break;
        default:
            cfg = uft_pll_cfg_default_mfm_dd();
            break;
    }

    /* Convert uint32_t flux intervals to uint64_t timestamps */
    uint64_t *timestamps = malloc(s->flux_count * sizeof(uint64_t));
    if (!timestamps) {
        free(s->bitstream);
        s->bitstream = NULL;
        return UFT_ERR_MEMORY;
    }

    uint64_t cumulative = 0;
    for (size_t i = 0; i < s->flux_count; i++) {
        cumulative += s->flux_ns[i];
        timestamps[i] = cumulative;
    }

    uint32_t final_cell = 0;
    size_t dropped = 0;
    s->bit_count = uft_flux_to_bits_pll(
        timestamps, s->flux_count, &cfg,
        s->bitstream, max_bits,
        &final_cell, &dropped
    );

    free(timestamps);

    /* Fill PLL stats */
    s->pll.final_cell_size_ns = (double)final_cell;
    s->pll.locked = (dropped < s->flux_count / 10);
    s->pll.quality = (s->flux_count > 0)
        ? 1.0f - (float)dropped / (float)s->flux_count
        : 0.0f;
    if (s->pll.quality < 0.0f) s->pll.quality = 0.0f;

    return UFT_OK;
}

/* ============================================================================
 * CRC check with session tracking
 * ============================================================================ */

bool uft_pipeline_check_crc(uft_decode_session_t *s,
                             const uint8_t *data, size_t len,
                             uint16_t expected, uint32_t bit_pos)
{
    (void)bit_pos;
    if (!data) return false;

    /* Simple CRC-CCITT computation (MFM standard) */
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }

    if (s) {
        s->crc.checked++;
        if (crc == expected) {
            s->crc.passed++;
            return true;
        }
        s->crc.failed++;
        s->crc.last_expected = expected;
        s->crc.last_actual   = crc;
        return false;
    }

    return (crc == expected);
}

/* ============================================================================
 * Retry evaluation
 * ============================================================================ */

static void evaluate_retry(uft_decode_session_t *s)
{
    if (!s) return;
    if (s->attempt_count >= UFT_SESSION_MAX_RETRIES) {
        s->retry_requested = false;
        return;
    }

    float coverage = (s->sectors_expected > 0)
        ? (float)s->sectors_found / (float)s->sectors_expected
        : 1.0f;

    if (coverage >= 0.95f && s->crc.failed == 0) {
        s->retry_requested = false;
        return;
    }

    if (s->pll.quality < UFT_SESSION_PLL_MARGINAL) {
        s->retry_requested = true;
        s->retry_reason    = UFT_RETRY_PLL_QUALITY;
        s->retry_preset    = (s->pll_preset == UFT_PLL_PRESET_AUTO)
                             ? UFT_PLL_PRESET_IBM_DD
                             : UFT_PLL_PRESET_AUTO;
        return;
    }

    if (s->crc.failed > 2 && s->pll.quality >= UFT_SESSION_PLL_OK) {
        s->retry_requested   = true;
        s->retry_reason      = UFT_RETRY_CRC_ERRORS;
        s->use_recovery_mode = true;
        return;
    }

    if (coverage < 0.80f) {
        s->retry_requested = true;
        s->retry_reason    = UFT_RETRY_MISSING_SECTORS;
        s->retry_preset    = UFT_PLL_PRESET_AUTO;
    }
}

/* ============================================================================
 * Main pipeline
 * ============================================================================ */

uft_error_t uft_pipeline_run(uft_decode_session_t *s)
{
    if (!s || !s->flux_ns || s->flux_count == 0)
        return UFT_ERR_INVALID_ARG;

    do {
        s->attempt_count++;

        /* Stage 1: PLL — Flux → Bits */
        uft_error_t err = uft_pipeline_run_pll(s);
        if (err != UFT_OK) {
            s->last_error = err;
            break;
        }

        /* Stage 2: Format decode — deferred to format plugin system
         * The format parser would call uft_pipeline_check_crc()
         * for each sector. For now, the pipeline produces the
         * bitstream which the caller can feed to format plugins. */

        /* Stage 3: Retry evaluation */
        evaluate_retry(s);

        if (s->retry_requested) {
            s->pll_preset = s->retry_preset;
            uft_session_reset_for_retry(s);
        }

    } while (s->retry_requested);

    return s->last_error;
}
