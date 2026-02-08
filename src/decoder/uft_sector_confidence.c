/**
 * @file uft_sector_confidence.c
 * @brief P0-DC-003: Sector Confidence Integration Implementation
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#include "uft/decoder/uft_sector_confidence.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Quality Descriptions
 *===========================================================================*/

static const char *quality_descriptions[] = {
    "Unknown",
    "Bad",
    "Poor",
    "Fair",
    "Good",
    "Excellent"
};

/*===========================================================================
 * Initialization
 *===========================================================================*/

void uft_conf_config_default(uft_confidence_config_t *config)
{
    if (!config) return;
    
    config->hw_weight = UFT_CONF_W_HARDWARE;
    config->dec_weight = UFT_CONF_W_DECODER;
    config->multirev_weight = UFT_CONF_W_MULTIREV;
    config->crc_weight = UFT_CONF_W_CRC;
    
    config->excellent_threshold = UFT_CONF_EXCELLENT;
    config->good_threshold = UFT_CONF_GOOD;
    config->fair_threshold = UFT_CONF_FAIR;
    config->poor_threshold = UFT_CONF_POOR;
    
    config->require_crc = true;
    config->boost_multirev = true;
    config->penalize_anomalies = true;
}

void uft_conf_init(uft_sector_confidence_t *conf)
{
    if (!conf) return;
    memset(conf, 0, sizeof(*conf));
    conf->quality_desc = quality_descriptions[0];
}

/*===========================================================================
 * Component Confidence Functions
 *===========================================================================*/

void uft_conf_calc_hardware(float flux_timing, float signal_strength,
                            float head_alignment, float rpm_stability,
                            uft_hw_confidence_t *hw)
{
    if (!hw) return;
    
    hw->flux_timing = uft_conf_clamp(flux_timing);
    hw->signal_strength = uft_conf_clamp(signal_strength);
    hw->head_alignment = uft_conf_clamp(head_alignment);
    hw->rpm_stability = uft_conf_clamp(rpm_stability);
    
    /* Weighted combination:
     * - Flux timing is most important (40%)
     * - Signal strength second (30%)
     * - Head alignment (20%)
     * - RPM stability (10%)
     */
    hw->overall = uft_conf_clamp(
        hw->flux_timing * 0.40f +
        hw->signal_strength * 0.30f +
        hw->head_alignment * 0.20f +
        hw->rpm_stability * 0.10f
    );
}

void uft_conf_calc_decoder(float sync_quality, float address_quality,
                           float data_quality, float encoding_confidence,
                           uft_dec_confidence_t *dec)
{
    if (!dec) return;
    
    dec->sync_quality = uft_conf_clamp(sync_quality);
    dec->address_quality = uft_conf_clamp(address_quality);
    dec->data_quality = uft_conf_clamp(data_quality);
    dec->encoding_confidence = uft_conf_clamp(encoding_confidence);
    
    /* Weighted combination:
     * - Data quality most important (40%)
     * - Address quality (25%)
     * - Sync quality (20%)
     * - Encoding detection (15%)
     */
    dec->overall = uft_conf_clamp(
        dec->data_quality * 0.40f +
        dec->address_quality * 0.25f +
        dec->sync_quality * 0.20f +
        dec->encoding_confidence * 0.15f
    );
}

void uft_conf_calc_multirev(uint8_t revolutions, uint8_t agreements,
                            float variance, uft_multirev_confidence_t *multirev)
{
    if (!multirev) return;
    
    multirev->revolutions = revolutions;
    multirev->agreements = agreements;
    multirev->variance = variance;
    
    if (revolutions == 0) {
        multirev->vote_confidence = 0.0f;
        multirev->overall = 0.5f;  /* No multi-rev data */
        return;
    }
    
    /* Vote confidence: percentage of agreeing revolutions */
    multirev->vote_confidence = (float)agreements / revolutions;
    
    /* Variance penalty: high variance reduces confidence */
    float variance_factor = 1.0f;
    if (variance > 0.1f) {
        variance_factor = 1.0f / (1.0f + variance * 5.0f);
    }
    
    /* Boost for more revolutions */
    float rev_boost = 1.0f;
    if (revolutions >= 5) rev_boost = 1.1f;
    if (revolutions >= 10) rev_boost = 1.2f;
    
    multirev->overall = uft_conf_clamp(
        multirev->vote_confidence * variance_factor * rev_boost
    );
}

void uft_conf_calc_crc(bool crc_valid, uint16_t calculated, uint16_t stored,
                       uint8_t bits_corrected, uft_crc_confidence_t *crc)
{
    if (!crc) return;
    
    crc->crc_valid = crc_valid;
    crc->calculated_crc = calculated;
    crc->stored_crc = stored;
    crc->bits_corrected = bits_corrected;
    
    if (crc_valid) {
        if (bits_corrected == 0) {
            /* Perfect CRC match */
            crc->correction_confidence = 1.0f;
            crc->overall = 1.0f;
        } else {
            /* CRC valid after correction */
            /* Confidence decreases with more corrections */
            crc->correction_confidence = 1.0f - (float)bits_corrected * 0.1f;
            if (crc->correction_confidence < 0.5f) {
                crc->correction_confidence = 0.5f;
            }
            crc->overall = crc->correction_confidence;
        }
    } else {
        /* CRC failed */
        crc->correction_confidence = 0.0f;
        crc->overall = 0.0f;
    }
}

/*===========================================================================
 * Combined Confidence Functions
 *===========================================================================*/

void uft_conf_calculate_combined(uft_sector_confidence_t *conf,
                                 const uft_confidence_config_t *config)
{
    if (!conf) return;
    
    uft_confidence_config_t default_config;
    if (!config) {
        uft_conf_config_default(&default_config);
        config = &default_config;
    }
    
    /* Base weighted combination */
    conf->combined = uft_conf_clamp(
        conf->hardware.overall * config->hw_weight +
        conf->decoder.overall * config->dec_weight +
        conf->multirev.overall * config->multirev_weight +
        conf->crc.overall * config->crc_weight
    );
    
    /* Apply modifiers */
    
    /* CRC requirement: if CRC failed and required, cap at 0.5 */
    if (config->require_crc && !conf->crc.crc_valid) {
        if (conf->combined > 0.5f) {
            conf->combined = 0.5f;
        }
    }
    
    /* Multi-rev boost: if high agreement, boost confidence */
    if (config->boost_multirev && conf->multirev.revolutions >= 3) {
        if (conf->multirev.vote_confidence >= 0.9f) {
            conf->combined = uft_conf_clamp(conf->combined * 1.1f);
        }
    }
    
    /* Anomaly penalty */
    if (config->penalize_anomalies) {
        if (conf->weak_bits_detected) {
            conf->combined *= 0.9f;
        }
        if (conf->timing_anomaly) {
            conf->combined *= 0.95f;
        }
    }
    
    /* Classify quality */
    conf->quality_level = uft_conf_classify(conf->combined, config);
    conf->quality_desc = uft_conf_quality_desc(conf->quality_level);
    
    /* Set recommendations */
    conf->needs_reread = (conf->combined < config->fair_threshold);
    conf->needs_manual_review = (conf->protection_suspected || 
                                 conf->multiple_candidates ||
                                 conf->combined < config->poor_threshold);
}

void uft_conf_calculate_full(uft_sector_confidence_t *conf,
                             const uft_confidence_config_t *config,
                             float flux_timing, float signal_strength,
                             float sync_quality, bool crc_valid,
                             uint8_t revolutions, uint8_t agreements)
{
    if (!conf) return;
    
    uft_conf_init(conf);
    
    /* Calculate hardware confidence */
    uft_conf_calc_hardware(flux_timing, signal_strength, 1.0f, 1.0f,
                           &conf->hardware);
    
    /* Calculate decoder confidence */
    uft_conf_calc_decoder(sync_quality, sync_quality, 
                          crc_valid ? 1.0f : 0.5f, 1.0f,
                          &conf->decoder);
    
    /* Calculate multi-rev confidence */
    uft_conf_calc_multirev(revolutions, agreements, 0.0f, &conf->multirev);
    
    /* Calculate CRC confidence */
    uft_conf_calc_crc(crc_valid, 0, 0, 0, &conf->crc);
    
    /* Calculate combined */
    uft_conf_calculate_combined(conf, config);
}

uint8_t uft_conf_classify(float confidence, const uft_confidence_config_t *config)
{
    uft_confidence_config_t default_config;
    if (!config) {
        uft_conf_config_default(&default_config);
        config = &default_config;
    }
    
    if (confidence >= config->excellent_threshold) return 5;
    if (confidence >= config->good_threshold) return 4;
    if (confidence >= config->fair_threshold) return 3;
    if (confidence >= config->poor_threshold) return 2;
    if (confidence > 0.0f) return 1;
    return 0;
}

const char *uft_conf_quality_desc(uint8_t quality_level)
{
    if (quality_level > 5) quality_level = 0;
    return quality_descriptions[quality_level];
}

/*===========================================================================
 * Track-Level Functions
 *===========================================================================*/

void uft_conf_track_summary(const uft_sector_confidence_t *sectors,
                            size_t sector_count, uint16_t track, uint8_t side,
                            uft_track_confidence_t *summary)
{
    if (!summary) return;
    memset(summary, 0, sizeof(*summary));
    
    summary->track = track;
    summary->side = side;
    summary->total_sectors = (uint16_t)sector_count;
    
    if (!sectors || sector_count == 0) {
        return;
    }
    
    float sum = 0.0f;
    float sum_sq = 0.0f;
    summary->min_confidence = 1.0f;
    summary->max_confidence = 0.0f;
    
    for (size_t i = 0; i < sector_count; i++) {
        const uft_sector_confidence_t *s = &sectors[i];
        float conf = s->combined;
        
        /* Statistics */
        sum += conf;
        sum_sq += conf * conf;
        
        if (conf < summary->min_confidence) summary->min_confidence = conf;
        if (conf > summary->max_confidence) summary->max_confidence = conf;
        
        /* Quality counts */
        switch (s->quality_level) {
            case 5: summary->excellent_count++; break;
            case 4: summary->good_count++; break;
            case 3: summary->fair_count++; break;
            case 2: summary->poor_count++; break;
            default: summary->bad_count++; break;
        }
        
        /* Flags */
        if (s->weak_bits_detected) summary->has_weak_bits = true;
        if (!s->crc.crc_valid) summary->has_crc_errors = true;
        if (s->timing_anomaly) summary->has_anomalies = true;
    }
    
    /* Calculate average */
    summary->avg_confidence = sum / sector_count;
    
    /* Calculate standard deviation */
    float variance = (sum_sq / sector_count) - 
                     (summary->avg_confidence * summary->avg_confidence);
    summary->std_confidence = (variance > 0) ? sqrtf(variance) : 0.0f;
    
    /* Fully readable if no errors */
    summary->fully_readable = !summary->has_crc_errors;
}

size_t uft_conf_find_weakest(const uft_sector_confidence_t *sectors,
                             size_t sector_count)
{
    if (!sectors || sector_count == 0) return 0;
    
    size_t weakest = 0;
    float min_conf = sectors[0].combined;
    
    for (size_t i = 1; i < sector_count; i++) {
        if (sectors[i].combined < min_conf) {
            min_conf = sectors[i].combined;
            weakest = i;
        }
    }
    
    return weakest;
}

size_t uft_conf_needs_reread(const uft_sector_confidence_t *sectors,
                             size_t sector_count, float threshold,
                             size_t *indices, size_t max_indices)
{
    if (!sectors || sector_count == 0 || !indices || max_indices == 0) {
        return 0;
    }
    
    size_t count = 0;
    for (size_t i = 0; i < sector_count && count < max_indices; i++) {
        if (sectors[i].combined < threshold || sectors[i].needs_reread) {
            indices[count++] = i;
        }
    }
    
    return count;
}

/*===========================================================================
 * Export Functions
 *===========================================================================*/

size_t uft_conf_export_json(const uft_sector_confidence_t *conf,
                            uint16_t track, uint8_t sector,
                            char *buffer, size_t buffer_size)
{
    if (!conf || !buffer || buffer_size == 0) return 0;
    
    return (size_t)snprintf(buffer, buffer_size,
        "{\n"
        "  \"track\": %u,\n"
        "  \"sector\": %u,\n"
        "  \"combined\": %.3f,\n"
        "  \"quality_level\": %u,\n"
        "  \"quality\": \"%s\",\n"
        "  \"hardware\": {\n"
        "    \"flux_timing\": %.3f,\n"
        "    \"signal_strength\": %.3f,\n"
        "    \"overall\": %.3f\n"
        "  },\n"
        "  \"decoder\": {\n"
        "    \"sync_quality\": %.3f,\n"
        "    \"data_quality\": %.3f,\n"
        "    \"overall\": %.3f\n"
        "  },\n"
        "  \"multirev\": {\n"
        "    \"revolutions\": %u,\n"
        "    \"agreements\": %u,\n"
        "    \"overall\": %.3f\n"
        "  },\n"
        "  \"crc\": {\n"
        "    \"valid\": %s,\n"
        "    \"overall\": %.3f\n"
        "  },\n"
        "  \"flags\": {\n"
        "    \"weak_bits\": %s,\n"
        "    \"timing_anomaly\": %s,\n"
        "    \"protection_suspected\": %s,\n"
        "    \"needs_reread\": %s\n"
        "  }\n"
        "}",
        track, sector,
        conf->combined,
        conf->quality_level,
        conf->quality_desc ? conf->quality_desc : "Unknown",
        conf->hardware.flux_timing,
        conf->hardware.signal_strength,
        conf->hardware.overall,
        conf->decoder.sync_quality,
        conf->decoder.data_quality,
        conf->decoder.overall,
        conf->multirev.revolutions,
        conf->multirev.agreements,
        conf->multirev.overall,
        conf->crc.crc_valid ? "true" : "false",
        conf->crc.overall,
        conf->weak_bits_detected ? "true" : "false",
        conf->timing_anomaly ? "true" : "false",
        conf->protection_suspected ? "true" : "false",
        conf->needs_reread ? "true" : "false"
    );
}

size_t uft_conf_track_export_json(const uft_track_confidence_t *summary,
                                  char *buffer, size_t buffer_size)
{
    if (!summary || !buffer || buffer_size == 0) return 0;
    
    return (size_t)snprintf(buffer, buffer_size,
        "{\n"
        "  \"track\": %u,\n"
        "  \"side\": %u,\n"
        "  \"total_sectors\": %u,\n"
        "  \"quality_counts\": {\n"
        "    \"excellent\": %u,\n"
        "    \"good\": %u,\n"
        "    \"fair\": %u,\n"
        "    \"poor\": %u,\n"
        "    \"bad\": %u\n"
        "  },\n"
        "  \"confidence\": {\n"
        "    \"min\": %.3f,\n"
        "    \"max\": %.3f,\n"
        "    \"avg\": %.3f,\n"
        "    \"std\": %.3f\n"
        "  },\n"
        "  \"flags\": {\n"
        "    \"has_weak_bits\": %s,\n"
        "    \"has_crc_errors\": %s,\n"
        "    \"has_anomalies\": %s,\n"
        "    \"fully_readable\": %s\n"
        "  }\n"
        "}",
        summary->track,
        summary->side,
        summary->total_sectors,
        summary->excellent_count,
        summary->good_count,
        summary->fair_count,
        summary->poor_count,
        summary->bad_count,
        summary->min_confidence,
        summary->max_confidence,
        summary->avg_confidence,
        summary->std_confidence,
        summary->has_weak_bits ? "true" : "false",
        summary->has_crc_errors ? "true" : "false",
        summary->has_anomalies ? "true" : "false",
        summary->fully_readable ? "true" : "false"
    );
}

/*===========================================================================
 * Self-Test
 *===========================================================================*/

#ifdef UFT_CONFIDENCE_TEST

#include <stdio.h>

int main(void)
{
    printf("UFT Sector Confidence Self-Test\n");
    printf("================================\n\n");
    
    /* Test configuration */
    uft_confidence_config_t config;
    uft_conf_config_default(&config);
    printf("Default config:\n");
    printf("  HW weight: %.2f\n", config.hw_weight);
    printf("  DEC weight: %.2f\n", config.dec_weight);
    printf("  Excellent: %.2f\n", config.excellent_threshold);
    
    /* Test sector confidence */
    uft_sector_confidence_t conf;
    
    printf("\nTest 1: Perfect sector\n");
    uft_conf_calculate_full(&conf, &config,
                            1.0f, 1.0f,     /* flux, signal */
                            1.0f, true,      /* sync, crc */
                            5, 5);           /* revs, agreements */
    printf("  Combined: %.3f (%s)\n", conf.combined, conf.quality_desc);
    printf("  Quality level: %d\n", conf.quality_level);
    printf("  Test: %s\n", conf.quality_level >= 4 ? "PASS" : "FAIL");
    
    printf("\nTest 2: CRC error\n");
    uft_conf_calculate_full(&conf, &config,
                            0.9f, 0.9f,
                            0.8f, false,     /* CRC failed */
                            3, 3);
    printf("  Combined: %.3f (%s)\n", conf.combined, conf.quality_desc);
    printf("  Needs reread: %s\n", conf.needs_reread ? "yes" : "no");
    printf("  Test: %s\n", conf.combined <= 0.5f ? "PASS" : "FAIL");
    
    printf("\nTest 3: Weak signal\n");
    uft_conf_calculate_full(&conf, &config,
                            0.5f, 0.3f,      /* Low signal */
                            0.6f, true,
                            3, 2);           /* Partial agreement */
    printf("  Combined: %.3f (%s)\n", conf.combined, conf.quality_desc);
    printf("  Test: %s\n", conf.quality_level <= 3 ? "PASS" : "FAIL");
    
    /* Test track summary */
    printf("\nTest 4: Track summary\n");
    uft_sector_confidence_t sectors[9];
    for (int i = 0; i < 9; i++) {
        uft_conf_calculate_full(&sectors[i], &config,
                                0.7f + i * 0.03f, 0.8f,
                                0.9f, (i < 7),  /* 2 CRC errors */
                                3, 3);
    }
    
    uft_track_confidence_t summary;
    uft_conf_track_summary(sectors, 9, 35, 0, &summary);
    printf("  Sectors: %d\n", summary.total_sectors);
    printf("  Avg confidence: %.3f\n", summary.avg_confidence);
    printf("  CRC errors: %s\n", summary.has_crc_errors ? "yes" : "no");
    printf("  Fully readable: %s\n", summary.fully_readable ? "yes" : "no");
    
    /* Test JSON export */
    printf("\nTest 5: JSON export\n");
    char buffer[2048];
    size_t len = uft_conf_export_json(&sectors[0], 35, 1, buffer, sizeof(buffer));
    printf("  Exported %zu bytes\n", len);
    printf("  Test: %s\n", len > 100 ? "PASS" : "FAIL");
    
    /* Test find weakest */
    printf("\nTest 6: Find weakest\n");
    size_t weakest = uft_conf_find_weakest(sectors, 9);
    printf("  Weakest sector index: %zu\n", weakest);
    printf("  Test: %s\n", weakest >= 7 ? "PASS" : "FAIL");  /* CRC errors at 7,8 */
    
    printf("\nSelf-test complete.\n");
    return 0;
}

#endif /* UFT_CONFIDENCE_TEST */
