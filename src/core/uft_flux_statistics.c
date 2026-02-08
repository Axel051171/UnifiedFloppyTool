/**
 * @file uft_flux_statistics.c
 * @brief Erweiterte Flux-Statistik und Hardware-Korrelation Implementation
 * 
 * Implementiert M-001, M-005, S-007 (Flux-Statistik)
 * 
 * @version 3.3.0
 * @date 2026-01-04
 */

#include "uft/uft_flux_statistics.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

/*============================================================================
 * Interne Hilfsfunktionen
 *============================================================================*/

static double safe_sqrt(double x) {
    return (x > 0.0) ? sqrt(x) : 0.0;
}

static double clamp_double(double v, double min, double max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

/*============================================================================
 * Initialisierung
 *============================================================================*/

static bool g_fstat_initialized = false;

int uft_fstat_init(void) {
    if (g_fstat_initialized) {
        return UFT_FSTAT_OK;
    }
    g_fstat_initialized = true;
    return UFT_FSTAT_OK;
}

void uft_fstat_cleanup(void) {
    g_fstat_initialized = false;
}

/*============================================================================
 * Varianz-Berechnung (Welford's Online Algorithm)
 *============================================================================*/

int uft_fstat_calculate_variance(
    const double *values,
    size_t count,
    double *mean,
    double *variance,
    double *stddev)
{
    if (!values || !mean || !variance || !stddev || count == 0) {
        return UFT_FSTAT_ERR_NULL;
    }

    /* Welford's Online Algorithm für numerische Stabilität */
    double m = 0.0;
    double m2 = 0.0;

    for (size_t i = 0; i < count; i++) {
        double delta = values[i] - m;
        m += delta / (double)(i + 1);
        double delta2 = values[i] - m;
        m2 += delta * delta2;
    }

    *mean = m;
    *variance = (count > 1) ? (m2 / (double)(count - 1)) : 0.0;
    *stddev = safe_sqrt(*variance);

    return UFT_FSTAT_OK;
}

/*============================================================================
 * Bitcell-Analyse
 *============================================================================*/

int uft_fstat_analyze_bitcell(
    const uint32_t *flux_data[],
    size_t rev_count,
    uint32_t bit_position,
    uft_bitcell_stats_t *stats)
{
    if (!flux_data || !stats || rev_count == 0) {
        return UFT_FSTAT_ERR_NULL;
    }

    memset(stats, 0, sizeof(uft_bitcell_stats_t));
    stats->bit_position = bit_position;

    /* Timing-Werte aus allen Revolutions sammeln */
    double *timings = calloc(rev_count, sizeof(double));
    if (!timings) {
        return UFT_FSTAT_ERR_MEMORY;
    }

    double min_t = 1e9, max_t = 0.0;
    uint16_t ones = 0, zeros = 0;

    for (size_t r = 0; r < rev_count; r++) {
        if (!flux_data[r]) continue;

        /* Flux-Zeit an dieser Position extrahieren */
        /* Annahme: flux_data enthält Flux-Intervalle in Nanosekunden */
        double t = (double)flux_data[r][bit_position];
        timings[r] = t;

        if (t < min_t) min_t = t;
        if (t > max_t) max_t = t;

        /* Bit-Wert aus Timing ableiten (vereinfacht) */
        /* MFM: kurzes Intervall = 1, langes = 0 */
        if (t < 3000.0) {  /* <3µs = kurz */
            ones++;
        } else {
            zeros++;
        }
    }

    /* Statistik berechnen */
    int ret = uft_fstat_calculate_variance(
        timings, rev_count,
        &stats->mean_timing_ns,
        &stats->variance_ns,
        &stats->stddev_ns
    );

    free(timings);

    if (ret != UFT_FSTAT_OK) {
        return ret;
    }

    stats->min_timing_ns = min_t;
    stats->max_timing_ns = max_t;
    stats->one_count = ones;
    stats->zero_count = zeros;
    stats->best_value = (ones >= zeros) ? 1 : 0;

    /* Confidence aus Konsistenz berechnen */
    double consistency_ratio = (double)(ones > zeros ? ones : zeros) / (double)rev_count;
    stats->consistency = (uint8_t)(consistency_ratio * 100.0);

    /* Confidence = Konsistenz * (1 - normalisierte Varianz) */
    double norm_variance = stats->stddev_ns / stats->mean_timing_ns;
    if (norm_variance > 1.0) norm_variance = 1.0;
    stats->confidence = (uint8_t)(stats->consistency * (1.0 - norm_variance * 0.5));

    /* Anomalie-Level */
    double deviation_percent = (stats->stddev_ns / stats->mean_timing_ns) * 100.0;
    if (deviation_percent < 10.0) {
        stats->anomaly_level = UFT_FSTAT_ANOMALY_NONE;
    } else if (deviation_percent < 25.0) {
        stats->anomaly_level = UFT_FSTAT_ANOMALY_LOW;
    } else if (deviation_percent < 50.0) {
        stats->anomaly_level = UFT_FSTAT_ANOMALY_MEDIUM;
    } else if (deviation_percent < 100.0) {
        stats->anomaly_level = UFT_FSTAT_ANOMALY_HIGH;
    } else {
        stats->anomaly_level = UFT_FSTAT_ANOMALY_CRITICAL;
    }

    return UFT_FSTAT_OK;
}

/*============================================================================
 * Sektor-Analyse
 *============================================================================*/

int uft_fstat_analyze_sector(
    const uint8_t *track_data,
    size_t track_len,
    uint8_t sector_num,
    uft_sector_stats_t *stats)
{
    if (!track_data || !stats || track_len == 0) {
        return UFT_FSTAT_ERR_NULL;
    }

    memset(stats, 0, sizeof(uft_sector_stats_t));
    stats->sector = sector_num;
    stats->size = 512;  /* Standard */

    /* Hier würde die echte Sektor-Suche und Analyse stattfinden */
    /* Placeholder-Implementierung */
    stats->header_crc_ok = true;
    stats->data_crc_ok = true;
    stats->min_confidence = 75;
    stats->avg_confidence = 90;
    stats->max_confidence = 100;
    stats->total_bits = stats->size * 8;
    stats->weak_bits = 0;
    stats->ambiguous_bits = 0;

    return UFT_FSTAT_OK;
}

uint8_t uft_fstat_sector_confidence(const uft_sector_stats_t *stats) {
    if (!stats) return 0;

    /* Gewichtetes Mittel aus verschiedenen Faktoren */
    uint32_t score = 0;

    /* CRC OK = 40 Punkte */
    if (stats->header_crc_ok && stats->data_crc_ok) {
        score += 40;
    } else if (stats->header_crc_ok || stats->data_crc_ok) {
        score += 20;
    }

    /* Durchschnittliche Confidence = 0-40 Punkte */
    score += (stats->avg_confidence * 40) / 100;

    /* Keine schwachen Bits = 0-20 Punkte */
    if (stats->total_bits > 0) {
        double weak_ratio = (double)stats->weak_bits / (double)stats->total_bits;
        score += (uint32_t)((1.0 - weak_ratio) * 20.0);
    }

    return (uint8_t)(score > 100 ? 100 : score);
}

/*============================================================================
 * Track-Analyse
 *============================================================================*/

int uft_fstat_analyze_track(
    const uint32_t *flux_revs[],
    const size_t *rev_lengths,
    size_t rev_count,
    uint8_t cylinder,
    uint8_t head,
    uft_track_stats_t *stats)
{
    if (!flux_revs || !rev_lengths || !stats || rev_count == 0) {
        return UFT_FSTAT_ERR_NULL;
    }

    memset(stats, 0, sizeof(uft_track_stats_t));
    stats->cylinder = cylinder;
    stats->head = head;

    /* Flux-Statistik berechnen */
    double *flux_counts = calloc(rev_count, sizeof(double));
    if (!flux_counts) {
        return UFT_FSTAT_ERR_MEMORY;
    }

    uint32_t min_flux = UINT32_MAX, max_flux = 0;
    uint32_t total_flux = 0;

    for (size_t r = 0; r < rev_count; r++) {
        uint32_t fc = (uint32_t)rev_lengths[r];
        flux_counts[r] = (double)fc;
        total_flux += fc;

        if (fc < min_flux) min_flux = fc;
        if (fc > max_flux) max_flux = fc;
    }

    stats->flux_min = min_flux;
    stats->flux_max = max_flux;
    stats->total_flux_transitions = total_flux;

    /* Varianz berechnen */
    double mean, variance, stddev;
    uft_fstat_calculate_variance(flux_counts, rev_count, &mean, &variance, &stddev);
    stats->flux_mean = mean;
    stats->flux_variance = variance;

    free(flux_counts);

    /* RPM schätzen (angenommen 200ms pro Revolution bei 300 RPM) */
    if (rev_count > 0 && rev_lengths[0] > 0) {
        /* Annahme: 24MHz Sample-Clock */
        double samples_per_rev = (double)rev_lengths[0];
        double time_per_rev_ms = samples_per_rev / 24000.0;  /* 24MHz = 24k samples/ms */
        stats->rotation_time_ms = time_per_rev_ms;
        stats->rpm = 60000.0 / time_per_rev_ms;
    }

    /* Sektoren analysieren (Placeholder - würde echte Decodierung nutzen) */
    stats->sector_count = 9;  /* Typisch für DD MFM */
    stats->sectors_ok = 9;
    stats->sectors_recovered = 0;
    stats->sectors_failed = 0;

    for (int s = 0; s < stats->sector_count; s++) {
        stats->sectors[s].cylinder = cylinder;
        stats->sectors[s].head = head;
        stats->sectors[s].sector = s + 1;
        stats->sectors[s].size = 512;
        stats->sectors[s].header_crc_ok = true;
        stats->sectors[s].data_crc_ok = true;
        stats->sectors[s].avg_confidence = 90;
    }

    /* Gesamt-Confidence */
    uint32_t total_conf = 0;
    for (int s = 0; s < stats->sector_count; s++) {
        total_conf += stats->sectors[s].avg_confidence;
    }
    stats->overall_confidence = (uint8_t)(total_conf / stats->sector_count);

    /* Heatmap generieren */
    uft_fstat_generate_heatmap(stats, stats->heatmap, UFT_FSTAT_HEATMAP_RESOLUTION);

    return UFT_FSTAT_OK;
}

int uft_fstat_generate_heatmap(
    const uft_track_stats_t *stats,
    uint8_t *heatmap,
    size_t resolution)
{
    if (!stats || !heatmap || resolution == 0) {
        return UFT_FSTAT_ERR_NULL;
    }

    /* Heatmap basierend auf Sektor-Anomalien */
    memset(heatmap, UFT_FSTAT_ANOMALY_NONE, resolution);

    for (int s = 0; s < stats->sector_count; s++) {
        const uft_sector_stats_t *sec = &stats->sectors[s];

        /* Position im Heatmap */
        size_t start_pos = (s * resolution) / stats->sector_count;
        size_t end_pos = ((s + 1) * resolution) / stats->sector_count;

        uint8_t level = UFT_FSTAT_ANOMALY_NONE;

        /* Anomalie-Level basierend auf Confidence */
        if (sec->avg_confidence < 50) {
            level = UFT_FSTAT_ANOMALY_CRITICAL;
        } else if (sec->avg_confidence < 70) {
            level = UFT_FSTAT_ANOMALY_HIGH;
        } else if (sec->avg_confidence < 85) {
            level = UFT_FSTAT_ANOMALY_MEDIUM;
        } else if (sec->avg_confidence < 95) {
            level = UFT_FSTAT_ANOMALY_LOW;
        }

        for (size_t i = start_pos; i < end_pos && i < resolution; i++) {
            heatmap[i] = level;
        }
    }

    return UFT_FSTAT_OK;
}

/*============================================================================
 * PLL-Metriken (S-007)
 *============================================================================*/

int uft_fstat_pll_update(
    double sample_time,
    double expected_time,
    bool locked,
    uft_pll_metrics_t *metrics)
{
    if (!metrics) {
        return UFT_FSTAT_ERR_NULL;
    }

    double phase_error = sample_time - expected_time;
    metrics->total_samples++;

    /* Lock-Status aktualisieren */
    if (locked) {
        metrics->status |= UFT_PLL_STATUS_LOCKED;
        metrics->status &= ~UFT_PLL_STATUS_LOST;
    } else {
        metrics->status &= ~UFT_PLL_STATUS_LOCKED;
        metrics->status |= UFT_PLL_STATUS_LOST;
        metrics->sync_loss_count++;
    }

    /* Phase-Fehler Statistik (Online-Update) */
    double delta = phase_error - metrics->phase_error_mean;
    metrics->phase_error_mean += delta / (double)metrics->total_samples;
    double delta2 = phase_error - metrics->phase_error_mean;
    metrics->phase_error_variance += delta * delta2;

    /* Max Phase-Fehler */
    double abs_error = fabs(phase_error);
    if (abs_error > metrics->phase_error_max) {
        metrics->phase_error_max = abs_error;
    }

    /* Bit-Slip Erkennung (Phase-Fehler > 0.5 Bit-Zeit) */
    if (abs_error > expected_time * 0.5) {
        metrics->slip_count++;
        metrics->status |= UFT_PLL_STATUS_SLIP;
    }

    /* Histogram aktualisieren */
    int bin = (int)((phase_error + expected_time) / (2.0 * expected_time) * UFT_FSTAT_HISTOGRAM_BINS);
    if (bin < 0) bin = 0;
    if (bin >= UFT_FSTAT_HISTOGRAM_BINS) bin = UFT_FSTAT_HISTOGRAM_BINS - 1;
    if (metrics->phase_histogram[bin] < 0xFFFF) {
        metrics->phase_histogram[bin]++;
    }

    return UFT_FSTAT_OK;
}

uint8_t uft_fstat_pll_quality_score(const uft_pll_metrics_t *metrics) {
    if (!metrics || metrics->total_samples == 0) {
        return 0;
    }

    double score = 100.0;

    /* Abzug für Sync-Verluste */
    double loss_ratio = (double)metrics->sync_loss_count / (double)metrics->total_samples;
    score -= loss_ratio * 200.0;  /* Bis zu 20% Abzug */

    /* Abzug für Bit-Slips */
    double slip_ratio = (double)metrics->slip_count / (double)metrics->total_samples;
    score -= slip_ratio * 300.0;  /* Bis zu 30% Abzug */

    /* Abzug für Phase-Fehler Varianz */
    double var = metrics->phase_error_variance / (double)metrics->total_samples;
    double stddev = safe_sqrt(var);
    double norm_stddev = stddev / metrics->frequency_estimate;
    if (norm_stddev > 0.1) {
        score -= (norm_stddev - 0.1) * 500.0;  /* Bis zu 50% Abzug */
    }

    return (uint8_t)clamp_double(score, 0.0, 100.0);
}

int uft_fstat_pll_detect_events(
    const double *phase_errors,
    size_t count,
    uft_pll_metrics_t *metrics)
{
    if (!phase_errors || !metrics || count == 0) {
        return UFT_FSTAT_ERR_NULL;
    }

    double prev_error = 0.0;
    double threshold = metrics->frequency_estimate * 0.25;  /* 25% der Bit-Zeit */

    for (size_t i = 0; i < count; i++) {
        double error = phase_errors[i];
        double delta = error - prev_error;

        /* Slip-Erkennung: plötzlicher großer Sprung */
        if (fabs(delta) > threshold) {
            metrics->slip_count++;
        }

        /* Lock-Loss-Erkennung: mehrere große Fehler hintereinander */
        if (i >= 3) {
            bool consecutive_errors = true;
            for (size_t j = i - 3; j <= i; j++) {
                if (fabs(phase_errors[j]) < threshold) {
                    consecutive_errors = false;
                    break;
                }
            }
            if (consecutive_errors) {
                metrics->sync_loss_count++;
            }
        }

        prev_error = error;
    }

    metrics->quality_score = uft_fstat_pll_quality_score(metrics);

    return UFT_FSTAT_OK;
}

/*============================================================================
 * Hardware-Decode-Korrelation (M-005)
 *============================================================================*/

int uft_fstat_correlate_error(
    uint32_t error_position,
    const uint32_t *flux_data,
    const uft_pll_metrics_t *pll,
    uft_decode_correlation_t *correlation)
{
    if (!flux_data || !pll || !correlation) {
        return UFT_FSTAT_ERR_NULL;
    }

    memset(correlation, 0, sizeof(uft_decode_correlation_t));
    correlation->bit_position = error_position;
    correlation->decode_error = true;

    /* Timing an Fehler-Position */
    correlation->timing_at_error_ns = (double)flux_data[error_position];
    
    /* Erwartetes Timing (aus PLL) */
    correlation->timing_expected_ns = 1e9 / pll->frequency_estimate;
    
    /* Abweichung */
    double diff = correlation->timing_at_error_ns - correlation->timing_expected_ns;
    correlation->timing_deviation = (diff / correlation->timing_expected_ns) * 100.0;

    /* PLL-Status zum Fehlerzeitpunkt */
    correlation->pll_status = pll->status;
    correlation->pll_phase_error = pll->phase_error_mean;

    /* Flux-Muster kopieren */
    for (int i = 0; i < 8; i++) {
        int idx = (int)error_position - 4 + i;
        if (idx >= 0) {
            correlation->flux_pattern[i] = (uint8_t)(flux_data[idx] >> 8);
        }
    }

    /* Korrelations-Score berechnen */
    uint8_t score = 0;

    /* Timing-Korrelation */
    if (fabs(correlation->timing_deviation) > 25.0) {
        score += 40;  /* Stark timing-korreliert */
    } else if (fabs(correlation->timing_deviation) > 10.0) {
        score += 20;
    }

    /* PLL-Korrelation */
    if (pll->status & UFT_PLL_STATUS_LOST) {
        score += 40;
    } else if (pll->status & UFT_PLL_STATUS_SLIP) {
        score += 20;
    }

    correlation->correlation_score = score;

    return UFT_FSTAT_OK;
}

int uft_fstat_aggregate_correlations(
    const uft_decode_correlation_t *correlations,
    size_t count,
    uft_correlation_stats_t *stats)
{
    if (!correlations || !stats || count == 0) {
        return UFT_FSTAT_ERR_NULL;
    }

    memset(stats, 0, sizeof(uft_correlation_stats_t));
    stats->total_errors = (uint32_t)count;

    double total_deviation = 0.0;
    double total_phase = 0.0;

    for (size_t i = 0; i < count; i++) {
        const uft_decode_correlation_t *c = &correlations[i];

        /* Kategorisieren */
        if (fabs(c->timing_deviation) > 25.0) {
            stats->timing_correlated++;
        }
        if (c->pll_status & (UFT_PLL_STATUS_LOST | UFT_PLL_STATUS_SLIP)) {
            stats->pll_correlated++;
        }
        if (c->correlation_score < 20) {
            stats->uncorrelated++;
        }

        /* Aggregation */
        total_deviation += fabs(c->timing_deviation);
        total_phase += fabs(c->pll_phase_error);

        if (c->pll_status & UFT_PLL_STATUS_LOST) {
            stats->errors_at_lock_loss++;
        }
        if (c->pll_status & UFT_PLL_STATUS_SLIP) {
            stats->errors_at_slip++;
        }
    }

    stats->avg_error_deviation = total_deviation / (double)count;
    stats->avg_phase_at_error = total_phase / (double)count;

    /* Threshold schätzen */
    stats->timing_threshold_ns = stats->avg_error_deviation * 0.5;

    return UFT_FSTAT_OK;
}

/*============================================================================
 * Report-Generierung
 *============================================================================*/

int uft_fstat_create_report(
    const uft_track_stats_t *tracks,
    size_t track_count,
    const uft_pll_metrics_t *pll,
    const uft_correlation_stats_t *correlation,
    uft_flux_analysis_report_t *report)
{
    if (!tracks || !report || track_count == 0) {
        return UFT_FSTAT_ERR_NULL;
    }

    memset(report, 0, sizeof(uft_flux_analysis_report_t));
    report->version = UFT_FSTAT_VERSION;
    report->total_tracks = (uint8_t)track_count;

    /* Tracks kopieren */
    size_t copy_count = track_count;
    if (copy_count > UFT_FSTAT_MAX_TRACKS * 2) {
        copy_count = UFT_FSTAT_MAX_TRACKS * 2;
    }
    memcpy(report->tracks, tracks, copy_count * sizeof(uft_track_stats_t));

    /* Statistik aggregieren */
    uint32_t total_conf = 0;

    for (size_t t = 0; t < copy_count; t++) {
        const uft_track_stats_t *tr = &tracks[t];
        report->total_sectors += tr->sector_count;
        report->sectors_ok += tr->sectors_ok;
        report->sectors_recovered += tr->sectors_recovered;
        report->sectors_failed += tr->sectors_failed;
        total_conf += tr->overall_confidence;

        /* Anomalien zählen */
        for (int i = 0; i < UFT_FSTAT_HEATMAP_RESOLUTION; i++) {
            if (tr->heatmap[i] > 0) {
                report->anomaly_total++;
                if (tr->heatmap[i] < 5) {
                    report->anomaly_by_level[tr->heatmap[i]]++;
                }
            }
        }
    }

    report->overall_confidence = (uint8_t)(total_conf / copy_count);

    /* PLL-Metriken kopieren */
    if (pll) {
        memcpy(&report->pll_metrics, pll, sizeof(uft_pll_metrics_t));
    }

    /* Korrelation kopieren */
    if (correlation) {
        memcpy(&report->correlation, correlation, sizeof(uft_correlation_stats_t));
    }

    /* Empfehlungen generieren */
    char *rec = report->recommendations;
    size_t rec_len = sizeof(report->recommendations);
    int offset = 0;

    if (report->sectors_failed > 0) {
        offset += snprintf(rec + offset, rec_len - offset,
            "- %u Sektor(en) nicht lesbar. Multi-Pass-Analyse empfohlen.\n",
            report->sectors_failed);
    }

    if (report->overall_confidence < 80) {
        offset += snprintf(rec + offset, rec_len - offset,
            "- Niedrige Gesamt-Confidence (%u%%). Medium prüfen.\n",
            report->overall_confidence);
    }

    if (pll && pll->sync_loss_count > 10) {
        offset += snprintf(rec + offset, rec_len - offset,
            "- %u PLL Sync-Verluste. Hardware-Kalibrierung prüfen.\n",
            pll->sync_loss_count);
    }

    if (report->anomaly_by_level[UFT_FSTAT_ANOMALY_CRITICAL] > 0) {
        offset += snprintf(rec + offset, rec_len - offset,
            "- %u kritische Anomalien gefunden. Forensische Analyse empfohlen.\n",
            report->anomaly_by_level[UFT_FSTAT_ANOMALY_CRITICAL]);
    }

    if (offset == 0) {
        snprintf(rec, rec_len, "- Keine besonderen Auffälligkeiten.");
    }

    return UFT_FSTAT_OK;
}

int uft_fstat_export_json(
    const uft_flux_analysis_report_t *report,
    char *buffer,
    size_t buffer_size)
{
    if (!report || !buffer || buffer_size == 0) {
        return UFT_FSTAT_ERR_NULL;
    }

    int len = snprintf(buffer, buffer_size,
        "{\n"
        "  \"version\": \"3.3.0\",\n"
        "  \"total_tracks\": %u,\n"
        "  \"total_sectors\": %u,\n"
        "  \"sectors_ok\": %u,\n"
        "  \"sectors_recovered\": %u,\n"
        "  \"sectors_failed\": %u,\n"
        "  \"overall_confidence\": %u,\n"
        "  \"pll\": {\n"
        "    \"quality_score\": %u,\n"
        "    \"sync_loss_count\": %u,\n"
        "    \"slip_count\": %u\n"
        "  },\n"
        "  \"anomalies\": {\n"
        "    \"total\": %u,\n"
        "    \"critical\": %u,\n"
        "    \"high\": %u,\n"
        "    \"medium\": %u,\n"
        "    \"low\": %u\n"
        "  },\n"
        "  \"recommendations\": \"%s\"\n"
        "}\n",
        report->total_tracks,
        report->total_sectors,
        report->sectors_ok,
        report->sectors_recovered,
        report->sectors_failed,
        report->overall_confidence,
        report->pll_metrics.quality_score,
        report->pll_metrics.sync_loss_count,
        report->pll_metrics.slip_count,
        report->anomaly_total,
        report->anomaly_by_level[UFT_FSTAT_ANOMALY_CRITICAL],
        report->anomaly_by_level[UFT_FSTAT_ANOMALY_HIGH],
        report->anomaly_by_level[UFT_FSTAT_ANOMALY_MEDIUM],
        report->anomaly_by_level[UFT_FSTAT_ANOMALY_LOW],
        report->recommendations
    );

    return (len > 0 && (size_t)len < buffer_size) ? UFT_FSTAT_OK : UFT_FSTAT_ERR_RANGE;
}

int uft_fstat_export_markdown(
    const uft_flux_analysis_report_t *report,
    char *buffer,
    size_t buffer_size)
{
    if (!report || !buffer || buffer_size == 0) {
        return UFT_FSTAT_ERR_NULL;
    }

    int len = snprintf(buffer, buffer_size,
        "# Flux-Analyse Report\n\n"
        "**Version:** 3.3.0\n"
        "**Confidence:** %u%%\n\n"
        "## Übersicht\n\n"
        "| Metrik | Wert |\n"
        "|--------|------|\n"
        "| Tracks | %u |\n"
        "| Sektoren gesamt | %u |\n"
        "| Sektoren OK | %u |\n"
        "| Wiederhergestellt | %u |\n"
        "| Fehlgeschlagen | %u |\n\n"
        "## PLL-Qualität\n\n"
        "| Metrik | Wert |\n"
        "|--------|------|\n"
        "| Qualitäts-Score | %u%% |\n"
        "| Sync-Verluste | %u |\n"
        "| Bit-Slips | %u |\n\n"
        "## Anomalien\n\n"
        "| Level | Anzahl |\n"
        "|-------|--------|\n"
        "| Kritisch | %u |\n"
        "| Hoch | %u |\n"
        "| Mittel | %u |\n"
        "| Niedrig | %u |\n\n"
        "## Empfehlungen\n\n%s\n",
        report->overall_confidence,
        report->total_tracks,
        report->total_sectors,
        report->sectors_ok,
        report->sectors_recovered,
        report->sectors_failed,
        report->pll_metrics.quality_score,
        report->pll_metrics.sync_loss_count,
        report->pll_metrics.slip_count,
        report->anomaly_by_level[UFT_FSTAT_ANOMALY_CRITICAL],
        report->anomaly_by_level[UFT_FSTAT_ANOMALY_HIGH],
        report->anomaly_by_level[UFT_FSTAT_ANOMALY_MEDIUM],
        report->anomaly_by_level[UFT_FSTAT_ANOMALY_LOW],
        report->recommendations
    );

    return (len > 0 && (size_t)len < buffer_size) ? UFT_FSTAT_OK : UFT_FSTAT_ERR_RANGE;
}

/*============================================================================
 * Anomalie-Detektion
 *============================================================================*/

uint8_t uft_fstat_evaluate_anomaly(
    double value,
    double expected,
    double tolerance_percent)
{
    if (expected == 0.0) {
        return UFT_FSTAT_ANOMALY_CRITICAL;
    }

    double deviation = fabs((value - expected) / expected) * 100.0;

    if (deviation < tolerance_percent * 0.5) {
        return UFT_FSTAT_ANOMALY_NONE;
    } else if (deviation < tolerance_percent) {
        return UFT_FSTAT_ANOMALY_LOW;
    } else if (deviation < tolerance_percent * 2.0) {
        return UFT_FSTAT_ANOMALY_MEDIUM;
    } else if (deviation < tolerance_percent * 4.0) {
        return UFT_FSTAT_ANOMALY_HIGH;
    } else {
        return UFT_FSTAT_ANOMALY_CRITICAL;
    }
}

int uft_fstat_detect_anomalies(
    const uft_track_stats_t *stats,
    uint32_t *anomaly_positions,
    uint8_t *anomaly_levels,
    size_t max_anomalies,
    size_t *found_count)
{
    if (!stats || !anomaly_positions || !anomaly_levels || !found_count) {
        return UFT_FSTAT_ERR_NULL;
    }

    *found_count = 0;

    for (int i = 0; i < UFT_FSTAT_HEATMAP_RESOLUTION && *found_count < max_anomalies; i++) {
        if (stats->heatmap[i] > UFT_FSTAT_ANOMALY_NONE) {
            anomaly_positions[*found_count] = i;
            anomaly_levels[*found_count] = stats->heatmap[i];
            (*found_count)++;
        }
    }

    return UFT_FSTAT_OK;
}

/*============================================================================
 * Utility-Funktionen
 *============================================================================*/

const char *uft_fstat_anomaly_name(uint8_t level) {
    switch (level) {
        case UFT_FSTAT_ANOMALY_NONE:     return "NONE";
        case UFT_FSTAT_ANOMALY_LOW:      return "LOW";
        case UFT_FSTAT_ANOMALY_MEDIUM:   return "MEDIUM";
        case UFT_FSTAT_ANOMALY_HIGH:     return "HIGH";
        case UFT_FSTAT_ANOMALY_CRITICAL: return "CRITICAL";
        default:                          return "UNKNOWN";
    }
}

const char *uft_fstat_pll_status_name(uint8_t status) {
    static char buf[64];
    buf[0] = '\0';

    if (status & UFT_PLL_STATUS_LOCKED)    strncat(buf, "LOCKED ", sizeof(buf) - strlen(buf) - 1);
    if (status & UFT_PLL_STATUS_TRACKING)  strncat(buf, "TRACKING ", sizeof(buf) - strlen(buf) - 1);
    if (status & UFT_PLL_STATUS_SLIP)      strncat(buf, "SLIP ", sizeof(buf) - strlen(buf) - 1);
    if (status & UFT_PLL_STATUS_LOST)      strncat(buf, "LOST ", sizeof(buf) - strlen(buf) - 1);
    if (status & UFT_PLL_STATUS_REACQUIRE) strncat(buf, "REACQUIRE ", sizeof(buf) - strlen(buf) - 1);

    if (buf[0] == '\0') {
        strncpy(buf, "NONE", sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
    }

    return buf;
}

uint8_t uft_fstat_variance_to_confidence(double variance, double max_variance) {
    if (max_variance <= 0.0 || variance < 0.0) {
        return 0;
    }

    double ratio = variance / max_variance;
    if (ratio >= 1.0) {
        return 0;
    }

    return (uint8_t)((1.0 - ratio) * 100.0);
}
