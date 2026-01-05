/**
 * @file flux_stat.c
 * @brief Statistical Flux Recovery (FluxStat) Implementation
 * 
 * Basierend auf FluxRipper's FluxStat-Algorithmen.
 * 
 * @version 1.0.0
 */

#include "uft/flux_stat.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/*============================================================================
 * Interner Zustand
 *============================================================================*/

static fluxstat_config_t g_config = {
    .pass_count = FLUXSTAT_DEFAULT_PASSES,
    .confidence_threshold = CONF_WEAK,
    .max_correction_bits = 8,
    .encoding = 0,  /* MFM */
    .data_rate = 250000,
    .use_crc_correction = true,
    .preserve_weak_bits = true
};

static bool g_initialized = false;
static fluxstat_capture_t g_last_capture;
static bool g_capture_valid = false;

/*============================================================================
 * Initialisierung
 *============================================================================*/

int fluxstat_init(void)
{
    if (g_initialized) {
        return FLUXSTAT_OK;
    }
    
    memset(&g_last_capture, 0, sizeof(g_last_capture));
    g_capture_valid = false;
    g_initialized = true;
    
    return FLUXSTAT_OK;
}

int fluxstat_configure(const fluxstat_config_t *config)
{
    if (!config) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    if (config->pass_count < FLUXSTAT_MIN_PASSES ||
        config->pass_count > FLUXSTAT_MAX_PASSES) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    if (config->confidence_threshold > 100) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    memcpy(&g_config, config, sizeof(fluxstat_config_t));
    return FLUXSTAT_OK;
}

int fluxstat_get_config(fluxstat_config_t *config)
{
    if (!config) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    memcpy(config, &g_config, sizeof(fluxstat_config_t));
    return FLUXSTAT_OK;
}

/*============================================================================
 * Confidence-Berechnung (Kern-Algorithmus aus FluxRipper)
 *============================================================================*/

uint8_t fluxstat_correlation_confidence(const flux_correlation_t *corr)
{
    if (!corr || corr->total_passes == 0 || corr->hit_count == 0) {
        return 0;
    }
    
    float hit_ratio = (float)corr->hit_count / (float)corr->total_passes;
    
    /* Timing-Varianz berechnen */
    float mean = (float)corr->time_sum / (float)corr->hit_count;
    float variance = ((float)corr->time_sum_sq / (float)corr->hit_count) - (mean * mean);
    float stddev = sqrtf(variance > 0 ? variance : 0);
    
    /* Confidence aus Hit-Ratio (0-50) + Timing-Konsistenz (0-50) */
    float ratio_score = hit_ratio * 50.0f;
    float timing_score = 50.0f * expf(-stddev / 50.0f);
    
    float confidence = ratio_score + timing_score;
    
    if (confidence > 100.0f) confidence = 100.0f;
    if (confidence < 0.0f) confidence = 0.0f;
    
    return (uint8_t)confidence;
}

/*============================================================================
 * Histogram Funktionen
 *============================================================================*/

int fluxstat_histogram_clear(void)
{
    /* In einer echten Implementation würde hier Hardware-Register
     * oder ein globales Histogram zurückgesetzt */
    return FLUXSTAT_OK;
}

void fluxstat_histogram_update(fluxstat_histogram_t *h, uint16_t interval)
{
    if (!h) return;
    
    h->total_count++;
    
    /* Min/Max aktualisieren */
    if (interval < h->interval_min) h->interval_min = interval;
    if (interval > h->interval_max) h->interval_max = interval;
    
    /* Bin-Index berechnen (mit Shift für Auflösung) */
    uint8_t bin = interval >> 2;  /* BIN_SHIFT = 2 */
    if (bin >= 255) {
        h->overflow_count++;
        bin = 255;
    }
    
    /* Bin inkrementieren (saturierend) */
    if (h->bins[bin] < 0xFFFF) {
        h->bins[bin]++;
        
        /* Peak-Tracking aktualisieren */
        if (h->bins[bin] > h->peak_count) {
            h->peak_bin = bin;
            h->peak_count = h->bins[bin];
        }
    }
    
    /* EMA aktualisieren: new_mean = mean + (interval - mean) / 16 */
    h->mean_interval = h->mean_interval - (h->mean_interval >> 4) + (interval >> 4);
}

int fluxstat_histogram_stats(fluxstat_histogram_t *hist)
{
    if (!hist) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    /* In einer echten Implementation würden hier Hardware-Register gelesen */
    return FLUXSTAT_OK;
}

int fluxstat_histogram_read_bin(uint8_t bin, uint16_t *count)
{
    if (!count) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    *count = 0;
    return FLUXSTAT_OK;
}

/*============================================================================
 * Datenraten-Schätzung aus Histogram
 *============================================================================*/

int fluxstat_estimate_rate(uint32_t *rate_bps)
{
    if (!rate_bps) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    fluxstat_histogram_t hist;
    memset(&hist, 0, sizeof(hist));
    
    int ret = fluxstat_histogram_stats(&hist);
    if (ret != FLUXSTAT_OK) {
        return ret;
    }
    
    if (hist.total_count == 0) {
        return FLUXSTAT_ERR_NO_DATA;
    }
    
    /* Konvertiere Peak-Bin zu Datenrate
     * Peak-Bin repräsentiert häufigstes Flux-Intervall
     * Für MFM: bit_time = 2 * flux_time (im Durchschnitt)
     * 
     * Bei 200 MHz Clock: Bin 100 = 400 Clocks = 2µs
     * MFM bit_time = 4µs → rate = 250 Kbps
     */
    uint32_t interval_clocks = (uint32_t)hist.peak_bin << 2;  /* BIN_SHIFT = 2 */
    if (interval_clocks == 0) {
        interval_clocks = 1;
    }
    
    /* Annahme: 200 MHz Clock */
    uint32_t flux_interval_ns = interval_clocks * 5;
    *rate_bps = 1000000000UL / (2 * flux_interval_ns);
    
    return FLUXSTAT_OK;
}

/*============================================================================
 * Bit-Analyse
 *============================================================================*/

int fluxstat_get_bit_analysis(uint32_t bit_offset, uint32_t count,
                              fluxstat_bit_t *bits)
{
    if (!bits || count == 0) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    if (!g_capture_valid) {
        return FLUXSTAT_ERR_NO_DATA;
    }
    
    for (uint32_t i = 0; i < count; i++) {
        flux_correlation_t corr = {0};
        
        /* In einer echten Implementation würde hier die Korrelation
         * über alle Durchläufe berechnet */
        corr.total_passes = g_last_capture.pass_count;
        corr.hit_count = corr.total_passes;  /* Placeholder */
        corr.time_sum = (bit_offset + i) * corr.hit_count;
        corr.time_sum_sq = corr.time_sum * (bit_offset + i);
        
        /* Confidence berechnen */
        uint8_t confidence = fluxstat_correlation_confidence(&corr);
        
        bits[i].value = (corr.hit_count > corr.total_passes / 2) ? 1 : 0;
        bits[i].confidence = confidence;
        bits[i].transition_count = corr.hit_count;
        bits[i].timing_stddev = 5;  /* Placeholder */
        bits[i].corrected = 0;
        
        /* Klassifikation */
        if (bits[i].value == 1) {
            bits[i].classification = (confidence >= CONF_STRONG) ?
                                     BITCELL_STRONG_1 : BITCELL_WEAK_1;
        } else {
            bits[i].classification = (confidence >= CONF_STRONG) ?
                                     BITCELL_STRONG_0 : BITCELL_WEAK_0;
        }
        
        if (confidence < CONF_AMBIGUOUS) {
            bits[i].classification = BITCELL_AMBIGUOUS;
        }
    }
    
    return FLUXSTAT_OK;
}

/*============================================================================
 * Track-Analyse
 *============================================================================*/

int fluxstat_analyze_track(fluxstat_track_t *result)
{
    if (!result) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    if (!g_capture_valid) {
        return FLUXSTAT_ERR_NO_DATA;
    }
    
    memset(result, 0, sizeof(fluxstat_track_t));
    
    /* Durchschnittliche Index-Zeit berechnen */
    uint32_t avg_index_time = g_last_capture.total_time / g_last_capture.pass_count;
    
    /* Für MFM 250Kbps, 300 RPM (200ms/rev):
     * ~250000 bits/sec * 0.2 sec = 50000 bits/track
     * Mit 512 byte Sektoren + Overhead: ~9 Sektoren
     */
    result->sector_count = 9;  /* Würde aus Daten erkannt */
    result->track = 0;
    result->head = 0;
    
    /* Jeden Sektor analysieren */
    uint32_t total_conf = 0;
    
    for (int s = 0; s < result->sector_count; s++) {
        fluxstat_sector_t *sector = &result->sectors[s];
        
        sector->sector_num = s;
        sector->size = 512;
        sector->crc_ok = 1;  /* Placeholder */
        sector->confidence_min = 85;
        sector->confidence_avg = 95;
        sector->weak_bit_count = 0;
        sector->corrected_count = 0;
        
        total_conf += sector->confidence_avg;
        result->sectors_recovered++;
    }
    
    /* Gesamt-Confidence berechnen */
    result->overall_confidence = total_conf / result->sector_count;
    
    return FLUXSTAT_OK;
}

int fluxstat_recover_sector(uint8_t sector_num, fluxstat_sector_t *result)
{
    if (!result) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    if (!g_capture_valid) {
        return FLUXSTAT_ERR_NO_DATA;
    }
    
    memset(result, 0, sizeof(fluxstat_sector_t));
    
    result->sector_num = sector_num;
    result->size = 512;
    result->crc_ok = 1;
    result->confidence_min = 80;
    result->confidence_avg = 95;
    
    return FLUXSTAT_OK;
}

/*============================================================================
 * Utilities
 *============================================================================*/

const char *fluxstat_classification_name(uint8_t classification)
{
    switch (classification) {
        case BITCELL_STRONG_1:  return "STRONG_1";
        case BITCELL_WEAK_1:    return "WEAK_1";
        case BITCELL_STRONG_0:  return "STRONG_0";
        case BITCELL_WEAK_0:    return "WEAK_0";
        case BITCELL_AMBIGUOUS: return "AMBIGUOUS";
        default:                return "UNKNOWN";
    }
}

uint32_t fluxstat_calculate_rpm(uint32_t index_clocks, uint32_t clk_mhz)
{
    if (index_clocks == 0 || clk_mhz == 0) {
        return 0;
    }
    
    /* RPM = 60 / (index_clocks / (clk_mhz * 1e6))
     *     = 60 * clk_mhz * 1e6 / index_clocks
     */
    uint64_t numerator = 60ULL * clk_mhz * 1000000ULL;
    return (uint32_t)(numerator / index_clocks);
}

int fluxstat_calculate_confidence(const uint8_t *data, uint32_t length,
                                  uint8_t *min_conf, uint8_t *avg_conf)
{
    if (!data || !min_conf || !avg_conf || length == 0) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    /* Placeholder - würde Bit-Level-Confidence analysieren */
    *min_conf = 80;
    *avg_conf = 95;
    
    return FLUXSTAT_OK;
}

/*============================================================================
 * Capture Stubs (würden Hardware-Interface nutzen)
 *============================================================================*/

int fluxstat_capture_start(uint8_t drive, uint8_t track, uint8_t head)
{
    (void)drive;
    (void)track;
    (void)head;
    
    if (!g_initialized) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    g_capture_valid = false;
    return FLUXSTAT_OK;
}

int fluxstat_capture_abort(void)
{
    g_capture_valid = false;
    return FLUXSTAT_OK;
}

bool fluxstat_capture_busy(void)
{
    return false;
}

int fluxstat_capture_wait(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return FLUXSTAT_OK;
}

int fluxstat_capture_result(fluxstat_capture_t *result)
{
    if (!result) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    /* Simuliere ein Ergebnis für Tests */
    result->pass_count = g_config.pass_count;
    result->total_flux = 12500 * result->pass_count;
    result->min_flux = 12400;
    result->max_flux = 12600;
    result->total_time = 200000 * result->pass_count;  /* 200ms pro Pass */
    result->base_addr = 0x100000;
    
    for (int i = 0; i < result->pass_count && i < FLUXSTAT_MAX_PASSES; i++) {
        result->passes[i].flux_count = 12500;
        result->passes[i].index_time = 200000;
        result->passes[i].base_addr = result->base_addr + (i * 0x10000);
        result->passes[i].data_size = result->passes[i].flux_count * 4;
    }
    
    memcpy(&g_last_capture, result, sizeof(fluxstat_capture_t));
    g_capture_valid = true;
    
    return FLUXSTAT_OK;
}

int fluxstat_capture_progress(uint8_t *current_pass, uint8_t *total_passes)
{
    if (current_pass) {
        *current_pass = 0;
    }
    if (total_passes) {
        *total_passes = g_config.pass_count;
    }
    return FLUXSTAT_OK;
}

int fluxstat_get_pass_data(uint8_t pass, uint32_t *addr, uint32_t *size)
{
    if (!addr || !size) {
        return FLUXSTAT_ERR_INVALID;
    }
    
    if (!g_capture_valid || pass >= g_last_capture.pass_count) {
        return FLUXSTAT_ERR_NO_DATA;
    }
    
    *addr = g_last_capture.passes[pass].base_addr;
    *size = g_last_capture.passes[pass].data_size;
    
    return FLUXSTAT_OK;
}
