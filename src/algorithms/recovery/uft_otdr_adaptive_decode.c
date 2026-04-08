/**
 * @file uft_otdr_adaptive_decode.c
 * @brief OTDR-guided adaptive decoder implementation
 *
 * When normal decode fails CRC on a sector, this module uses OTDR analysis
 * to identify low-confidence bitcell regions, re-decodes with aggressive
 * PLL parameters, and fuses the results weighted by OTDR quality.
 *
 * @version 1.0.0
 * @date 2026-04-08
 */

#include "uft/recovery/uft_otdr_adaptive_decode.h"
#include "uft/analysis/uft_otdr_bridge.h"
#include "uft/analysis/uft_confidence_bridge.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Convert flux transitions to nanosecond intervals
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint32_t *flux_to_ns_intervals(const flux_raw_data_t *flux,
                                      uint32_t *out_count)
{
    if (!flux || flux->transition_count < 2 || flux->sample_rate == 0) {
        *out_count = 0;
        return NULL;
    }

    uint32_t count = (uint32_t)(flux->transition_count - 1);
    uint32_t *ns = malloc(count * sizeof(uint32_t));
    if (!ns) {
        *out_count = 0;
        return NULL;
    }

    double ns_per_tick = 1e9 / (double)flux->sample_rate;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t delta = flux->transitions[i + 1] - flux->transitions[i];
        ns[i] = (uint32_t)(delta * ns_per_tick + 0.5);
    }

    *out_count = count;
    return ns;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: CRC-CCITT (0xFFFF initial, 0x1021 polynomial)
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint16_t crc_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;

    /* Pre-load with 3x A1 sync bytes */
    static const uint8_t sync[] = { 0xA1, 0xA1, 0xA1 };
    for (int s = 0; s < 3; s++) {
        crc ^= (uint16_t)sync[s] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }
    }

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }
    }

    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Find low-confidence regions from OTDR quality profile
 * ═══════════════════════════════════════════════════════════════════════════ */

static int find_low_conf_regions(const float *quality_profile,
                                 uint32_t profile_len,
                                 float threshold,
                                 uint32_t min_run,
                                 uft_low_conf_region_t *regions,
                                 int max_regions)
{
    int count = 0;
    uint32_t run_start = 0;
    bool in_run = false;
    float run_sum = 0.0f;
    uint32_t run_len = 0;

    for (uint32_t i = 0; i < profile_len && count < max_regions; i++) {
        /* Quality profile is in dB; convert to normalized 0..1.
         * Excellent (>10dB) → ~1.0, Critical (<-5dB) → ~0.0 */
        float norm = (quality_profile[i] + 10.0f) / 50.0f;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;

        if (norm < threshold) {
            if (!in_run) {
                run_start = i;
                run_sum = 0.0f;
                run_len = 0;
                in_run = true;
            }
            run_sum += norm;
            run_len++;
        } else {
            if (in_run && run_len >= min_run) {
                regions[count].start_bitcell = run_start;
                regions[count].end_bitcell = i - 1;
                regions[count].avg_confidence = run_sum / run_len;
                regions[count].sector_idx = -1;
                count++;
            }
            in_run = false;
        }
    }

    /* Handle run extending to end of track */
    if (in_run && run_len >= min_run && count < max_regions) {
        regions[count].start_bitcell = run_start;
        regions[count].end_bitcell = profile_len - 1;
        regions[count].avg_confidence = run_sum / run_len;
        regions[count].sector_idx = -1;
        count++;
    }

    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Map low-conf regions to sectors
 * ═══════════════════════════════════════════════════════════════════════════ */

static void map_regions_to_sectors(uft_low_conf_region_t *regions,
                                   int region_count,
                                   const flux_decoded_track_t *track)
{
    for (int r = 0; r < region_count; r++) {
        regions[r].sector_idx = -1;

        for (size_t s = 0; s < track->sector_count; s++) {
            uint32_t sec_start = track->sectors[s].data_position;
            uint32_t sec_end = sec_start +
                (uint32_t)(track->sectors[s].data_size * UFT_MFM_BITCELLS_PER_BYTE);

            /* Check overlap */
            if (regions[r].start_bitcell < sec_end &&
                regions[r].end_bitcell > sec_start) {
                regions[r].sector_idx = (int)s;
                break;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public: Fuse sector data weighted by OTDR quality
 * ═══════════════════════════════════════════════════════════════════════════ */

float uft_otdr_fuse_sector(const uint8_t *normal_data,
                           const uint8_t *aggressive_data,
                           size_t data_len,
                           const float *quality_profile,
                           size_t profile_len,
                           uint32_t bitcell_offset,
                           uint32_t bitcells_per_byte,
                           uint8_t *output)
{
    if (!normal_data || !aggressive_data || !output || data_len == 0) {
        return 0.0f;
    }

    float total_quality = 0.0f;

    for (size_t i = 0; i < data_len; i++) {
        /* Average quality over the bitcells belonging to this byte */
        float byte_quality = 0.5f; /* default if no profile */

        if (quality_profile && profile_len > 0 && bitcells_per_byte > 0) {
            uint32_t bc_start = bitcell_offset + (uint32_t)(i * bitcells_per_byte);
            float sum = 0.0f;
            uint32_t cnt = 0;

            for (uint32_t b = 0; b < bitcells_per_byte; b++) {
                uint32_t idx = bc_start + b;
                if (idx < profile_len) {
                    /* Normalize dB to 0..1 */
                    float norm = (quality_profile[idx] + 10.0f) / 50.0f;
                    if (norm < 0.0f) norm = 0.0f;
                    if (norm > 1.0f) norm = 1.0f;
                    sum += norm;
                    cnt++;
                }
            }
            if (cnt > 0) byte_quality = sum / cnt;
        }

        /* High quality → trust normal decode; low quality → trust aggressive */
        if (byte_quality >= UFT_ADAPTIVE_CONF_THRESHOLD) {
            output[i] = normal_data[i];
        } else {
            output[i] = aggressive_data[i];
        }

        total_quality += byte_quality;
    }

    return total_quality / (float)data_len;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public: Adaptive decode
 * ═══════════════════════════════════════════════════════════════════════════ */

flux_status_t uft_otdr_adaptive_decode(const flux_raw_data_t *flux,
                                       const flux_decoder_options_t *opts,
                                       void *otdr_ctx_ptr,
                                       uint8_t cyl, uint8_t head,
                                       uft_adaptive_result_t *result)
{
    if (!flux || !result) return FLUX_ERR_INVALID;

    memset(result, 0, sizeof(*result));

    /* ── Step 1: Normal decode ─────────────────────────────────────────── */

    flux_decoder_options_t base_opts;
    if (opts) {
        base_opts = *opts;
    } else {
        flux_decoder_options_init(&base_opts);
    }

    flux_decoded_track_init(&result->normal_track);
    flux_status_t status = flux_decode_track(flux, &result->normal_track, &base_opts);
    if (status != FLUX_OK && status != FLUX_ERR_BAD_CRC) {
        return status;
    }

    /* Count CRC results */
    result->sectors_total = (uint32_t)result->normal_track.sector_count;
    for (size_t s = 0; s < result->normal_track.sector_count; s++) {
        if (result->normal_track.sectors[s].data_crc_ok) {
            result->sectors_crc_ok_normal++;
        }
    }

    /* ── Step 2: If all CRC-OK, we're done ─────────────────────────── */

    if (result->sectors_crc_ok_normal == result->sectors_total) {
        result->used_otdr = false;
        return FLUX_OK;
    }

    /* ── Step 3: If no OTDR context, return normal result ────────── */

    uft_otdr_context_t *otdr_ctx = (uft_otdr_context_t *)otdr_ctx_ptr;
    if (!otdr_ctx) {
        result->used_otdr = false;
        return (result->sectors_crc_ok_normal > 0) ? FLUX_OK : FLUX_ERR_BAD_CRC;
    }

    result->used_otdr = true;

    /* ── Step 4: OTDR analysis ──────────────────────────────────────── */

    /* Convert flux to nanosecond intervals */
    uint32_t ns_count = 0;
    uint32_t *flux_ns = flux_to_ns_intervals(flux, &ns_count);
    if (!flux_ns || ns_count < 100) {
        free(flux_ns);
        return (result->sectors_crc_ok_normal > 0) ? FLUX_OK : FLUX_ERR_BAD_CRC;
    }

    /* Feed to OTDR */
    int rc = uft_otdr_feed_flux_ns(otdr_ctx, flux_ns, ns_count, cyl, head, 0);
    free(flux_ns);
    if (rc != 0) {
        return (result->sectors_crc_ok_normal > 0) ? FLUX_OK : FLUX_ERR_BAD_CRC;
    }

    /* Run analysis */
    rc = uft_otdr_analyze(otdr_ctx);
    if (rc != 0) {
        return (result->sectors_crc_ok_normal > 0) ? FLUX_OK : FLUX_ERR_BAD_CRC;
    }

    /* Get quality profile */
    const otdr_track_t *otdr_track = uft_otdr_get_track(otdr_ctx, cyl, head);
    if (!otdr_track || !otdr_track->quality_profile ||
        otdr_track->bitcell_count == 0) {
        return (result->sectors_crc_ok_normal > 0) ? FLUX_OK : FLUX_ERR_BAD_CRC;
    }

    result->avg_quality = otdr_track->stats.quality_mean_db;

    /* ── Step 5: Find low-confidence regions ────────────────────────── */

    result->low_conf_regions = (uint32_t)find_low_conf_regions(
        otdr_track->quality_profile,
        otdr_track->bitcell_count,
        UFT_ADAPTIVE_CONF_THRESHOLD,
        UFT_ADAPTIVE_MIN_RUN,
        result->regions,
        UFT_ADAPTIVE_MAX_REGIONS);

    /* Map regions to sectors */
    map_regions_to_sectors(result->regions, (int)result->low_conf_regions,
                           &result->normal_track);

    /* If no low-confidence regions found, OTDR says quality is fine —
     * CRC errors are likely due to media defects, not PLL issues */
    if (result->low_conf_regions == 0) {
        return (result->sectors_crc_ok_normal > 0) ? FLUX_OK : FLUX_ERR_BAD_CRC;
    }

    /* ── Step 6: Aggressive re-decode ───────────────────────────────── */

    flux_decoder_options_t agg_opts = base_opts;
    agg_opts.tolerance = UFT_ADAPTIVE_PLL_TOLERANCE;
    agg_opts.pll_gain = UFT_ADAPTIVE_PLL_GAIN;

    flux_decoded_track_init(&result->adaptive_track);
    flux_decode_track(flux, &result->adaptive_track, &agg_opts);

    /* ── Step 7: Fuse per-sector ────────────────────────────────────── */

    for (size_t s = 0; s < result->normal_track.sector_count; s++) {
        flux_decoded_sector_t *normal_sec = &result->normal_track.sectors[s];

        /* Skip sectors that already have good CRC */
        if (normal_sec->data_crc_ok) continue;
        if (!normal_sec->data || normal_sec->data_size == 0) continue;

        result->sectors_attempted++;

        /* Find matching sector in aggressive decode */
        flux_decoded_sector_t *agg_sec = NULL;
        for (size_t a = 0; a < result->adaptive_track.sector_count; a++) {
            flux_decoded_sector_t *candidate = &result->adaptive_track.sectors[a];
            if (candidate->cylinder == normal_sec->cylinder &&
                candidate->head == normal_sec->head &&
                candidate->sector == normal_sec->sector &&
                candidate->data && candidate->data_size == normal_sec->data_size) {
                agg_sec = candidate;
                break;
            }
        }

        if (!agg_sec) continue;

        /* If aggressive decode already fixed CRC, just use it */
        if (agg_sec->data_crc_ok) {
            memcpy(normal_sec->data, agg_sec->data, normal_sec->data_size);
            normal_sec->data_crc_ok = true;
            result->sectors_improved++;
            continue;
        }

        /* Fuse weighted by OTDR quality */
        uint8_t *fused = malloc(normal_sec->data_size);
        if (!fused) continue;

        uft_otdr_fuse_sector(
            normal_sec->data,
            agg_sec->data,
            normal_sec->data_size,
            otdr_track->quality_profile,
            otdr_track->bitcell_count,
            normal_sec->data_position,
            UFT_MFM_BITCELLS_PER_BYTE,
            fused);

        /* Re-check CRC on fused data */
        /* Build CRC input: DAM byte + data */
        size_t crc_input_len = 1 + normal_sec->data_size;
        uint8_t *crc_buf = malloc(crc_input_len);
        if (crc_buf) {
            crc_buf[0] = normal_sec->deleted ? 0xF8 : 0xFB; /* DAM */
            memcpy(crc_buf + 1, fused, normal_sec->data_size);

            uint16_t calc_crc = crc_ccitt(crc_buf, crc_input_len);
            free(crc_buf);

            if (calc_crc == normal_sec->data_crc) {
                /* CRC matches! Replace sector data with fused version */
                memcpy(normal_sec->data, fused, normal_sec->data_size);
                normal_sec->data_crc_ok = true;
                result->sectors_improved++;
            }
        }

        free(fused);
    }

    /* Update normal_track stats */
    result->normal_track.good_sectors = result->sectors_crc_ok_normal +
                                         result->sectors_improved;
    result->normal_track.bad_data_crc = result->sectors_total -
                                         result->normal_track.good_sectors;

    return FLUX_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public: Free resources
 * ═══════════════════════════════════════════════════════════════════════════ */

void uft_adaptive_result_free(uft_adaptive_result_t *result)
{
    if (!result) return;
    flux_decoded_track_free(&result->normal_track);
    flux_decoded_track_free(&result->adaptive_track);
    memset(result, 0, sizeof(*result));
}
