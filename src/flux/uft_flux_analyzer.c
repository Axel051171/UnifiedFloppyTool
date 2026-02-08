/**
 * @file uft_flux_analyzer.c
 * @brief Flux Stream Analyzer Implementation
 * 
 * EXT4-009: Advanced flux stream analysis
 * 
 * Features:
 * - Multi-format flux parsing
 * - PLL-based decoding
 * - Encoding detection
 * - Revolution alignment
 * - Quality metrics
 */

#include "uft/flux/uft_flux_analyzer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* Standard bit cell timings (seconds) */
#define BITCELL_250KBPS     (1.0 / 250000.0)    /* DD MFM */
#define BITCELL_500KBPS     (1.0 / 500000.0)    /* HD MFM */
#define BITCELL_300KBPS     (1.0 / 300000.0)    /* DD FM */

/* PLL parameters */
#define PLL_PHASE_GAIN      0.05
#define PLL_FREQ_GAIN       0.005
#define PLL_MAX_DRIFT       0.15    /* 15% frequency drift */

/*===========================================================================
 * PLL Implementation
 *===========================================================================*/

typedef struct {
    double phase;
    double frequency;
    double nominal_freq;
    double phase_gain;
    double freq_gain;
    double min_freq;
    double max_freq;
} flux_pll_t;

static void pll_init(flux_pll_t *pll, double nominal_bitcell)
{
    pll->phase = 0;
    pll->frequency = 1.0 / nominal_bitcell;
    pll->nominal_freq = pll->frequency;
    pll->phase_gain = PLL_PHASE_GAIN;
    pll->freq_gain = PLL_FREQ_GAIN;
    pll->min_freq = pll->nominal_freq * (1.0 - PLL_MAX_DRIFT);
    pll->max_freq = pll->nominal_freq * (1.0 + PLL_MAX_DRIFT);
}

static int pll_process(flux_pll_t *pll, double flux_time, uint8_t *bits, int *bit_count)
{
    *bit_count = 0;
    
    double bitcell = 1.0 / pll->frequency;
    double phase_error = flux_time - pll->phase;
    
    /* Count bit cells since last flux */
    int cells = (int)round(phase_error / bitcell);
    if (cells < 1) cells = 1;
    if (cells > 4) cells = 4;
    
    /* Output bits: 0s followed by 1 */
    for (size_t i = 0; i < cells - 1 && *bit_count < 8; i++) {
        bits[(*bit_count)++] = 0;
    }
    if (*bit_count < 8) {
        bits[(*bit_count)++] = 1;
    }
    
    /* Update PLL */
    double expected = pll->phase + cells * bitcell;
    double error = flux_time - expected;
    
    pll->phase = flux_time + pll->phase_gain * error;
    pll->frequency += pll->freq_gain * error * pll->frequency;
    
    /* Clamp frequency */
    if (pll->frequency < pll->min_freq) pll->frequency = pll->min_freq;
    if (pll->frequency > pll->max_freq) pll->frequency = pll->max_freq;
    
    return cells;
}

/*===========================================================================
 * Flux Analysis Context
 *===========================================================================*/

int uft_flux_analyzer_init(uft_flux_analyzer_t *analyzer, 
                           uft_flux_format_t format)
{
    if (!analyzer) return -1;
    
    memset(analyzer, 0, sizeof(*analyzer));
    analyzer->format = format;
    analyzer->nominal_bitcell = BITCELL_250KBPS;  /* Default DD */
    
    return 0;
}

void uft_flux_analyzer_free(uft_flux_analyzer_t *analyzer)
{
    if (analyzer) {
        free(analyzer->flux_times);
        free(analyzer->decoded_bits);
        memset(analyzer, 0, sizeof(*analyzer));
    }
}

/*===========================================================================
 * Flux Loading
 *===========================================================================*/

int uft_flux_analyzer_load(uft_flux_analyzer_t *analyzer,
                           const double *flux_times, size_t count)
{
    if (!analyzer || !flux_times || count == 0) return -1;
    
    /* Copy flux data */
    analyzer->flux_times = malloc(count * sizeof(double));
    if (!analyzer->flux_times) return -1;
    
    memcpy(analyzer->flux_times, flux_times, count * sizeof(double));
    analyzer->flux_count = count;
    
    return 0;
}

/*===========================================================================
 * Encoding Detection
 *===========================================================================*/

int uft_flux_detect_encoding(uft_flux_analyzer_t *analyzer)
{
    if (!analyzer || !analyzer->flux_times || analyzer->flux_count < 100) {
        return -1;
    }
    
    /* Build histogram of intervals */
    #define HIST_BINS 100
    #define HIST_BIN_US 0.2  /* 0.2us per bin */
    
    int histogram[HIST_BINS] = {0};
    
    for (size_t i = 1; i < analyzer->flux_count; i++) {
        double interval = analyzer->flux_times[i] - analyzer->flux_times[i-1];
        int bin = (int)(interval * 1000000.0 / HIST_BIN_US);
        if (bin >= 0 && bin < HIST_BINS) {
            histogram[bin]++;
        }
    }
    
    /* Find peaks */
    int peaks[5] = {0};
    int peak_count = 0;
    
    for (int i = 2; i < HIST_BINS - 2 && peak_count < 5; i++) {
        if (histogram[i] > histogram[i-1] && histogram[i] > histogram[i-2] &&
            histogram[i] > histogram[i+1] && histogram[i] > histogram[i+2] &&
            histogram[i] > analyzer->flux_count / 100) {
            peaks[peak_count++] = i;
        }
    }
    
    if (peak_count < 2) {
        analyzer->detected_encoding = UFT_FLUX_ENCODING_UNKNOWN;
        return -1;
    }
    
    /* Analyze peak ratios */
    double t1 = peaks[0] * HIST_BIN_US;
    double t2 = peaks[1] * HIST_BIN_US;
    double ratio = t2 / t1;
    
    if (ratio > 1.4 && ratio < 1.6) {
        /* FM: T and 2T with 1.5 ratio suggests FM */
        analyzer->detected_encoding = UFT_FLUX_ENCODING_FM;
        analyzer->nominal_bitcell = t1 * 1e-6;  /* Convert to seconds */
    } else if (ratio > 1.9 && ratio < 2.1) {
        /* MFM: T and 2T */
        analyzer->detected_encoding = UFT_FLUX_ENCODING_MFM;
        analyzer->nominal_bitcell = t1 * 1e-6;
    } else if (peak_count >= 3) {
        double t3 = peaks[2] * HIST_BIN_US;
        double ratio2 = t3 / t1;
        
        if (ratio2 > 2.9 && ratio2 < 3.1) {
            /* GCR: T, 2T, 3T */
            analyzer->detected_encoding = UFT_FLUX_ENCODING_GCR;
            analyzer->nominal_bitcell = t1 * 1e-6;
        }
    }
    
    /* Estimate data rate */
    if (analyzer->nominal_bitcell > 0) {
        double freq = 1.0 / analyzer->nominal_bitcell;
        
        if (freq > 450000 && freq < 550000) {
            analyzer->data_rate = UFT_FLUX_RATE_HD;  /* 500 kbps */
        } else if (freq > 225000 && freq < 275000) {
            analyzer->data_rate = UFT_FLUX_RATE_DD;  /* 250 kbps */
        } else if (freq > 280000 && freq < 320000) {
            analyzer->data_rate = UFT_FLUX_RATE_DD_FM;  /* 300 kbps FM */
        }
    }
    
    return 0;
}

/*===========================================================================
 * PLL Decoding
 *===========================================================================*/

int uft_flux_decode_pll(uft_flux_analyzer_t *analyzer)
{
    if (!analyzer || !analyzer->flux_times || analyzer->flux_count < 10) {
        return -1;
    }
    
    /* Auto-detect if not set */
    if (analyzer->detected_encoding == UFT_FLUX_ENCODING_UNKNOWN) {
        uft_flux_detect_encoding(analyzer);
    }
    
    /* Allocate output buffer */
    size_t max_bits = analyzer->flux_count * 4;  /* Max 4 bits per flux */
    analyzer->decoded_bits = malloc(max_bits);
    if (!analyzer->decoded_bits) return -1;
    
    /* Initialize PLL */
    flux_pll_t pll;
    pll_init(&pll, analyzer->nominal_bitcell);
    
    /* Decode */
    analyzer->bit_count = 0;
    
    for (size_t i = 0; i < analyzer->flux_count; i++) {
        uint8_t bits[8];
        int bit_count;
        
        pll_process(&pll, analyzer->flux_times[i], bits, &bit_count);
        
        for (size_t j = 0; j < bit_count && analyzer->bit_count < max_bits; j++) {
            analyzer->decoded_bits[analyzer->bit_count++] = bits[j];
        }
    }
    
    return 0;
}

/*===========================================================================
 * Sync Pattern Detection
 *===========================================================================*/

int uft_flux_find_sync(const uft_flux_analyzer_t *analyzer,
                       size_t start_bit, size_t *sync_pos)
{
    if (!analyzer || !sync_pos || !analyzer->decoded_bits) {
        return -1;
    }
    
    /* MFM sync: A1 with missing clock = 0100010010001001 */
    static const uint8_t MFM_SYNC[] = {0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,1};
    #define SYNC_LEN 16
    
    for (size_t i = start_bit; i + SYNC_LEN <= analyzer->bit_count; i++) {
        bool match = true;
        
        for (int j = 0; j < SYNC_LEN && match; j++) {
            if (analyzer->decoded_bits[i + j] != MFM_SYNC[j]) {
                match = false;
            }
        }
        
        if (match) {
            *sync_pos = i;
            return 0;
        }
    }
    
    return -1;  /* Not found */
}

/*===========================================================================
 * Quality Metrics
 *===========================================================================*/

int uft_flux_calc_quality(uft_flux_analyzer_t *analyzer,
                          uft_flux_quality_t *quality)
{
    if (!analyzer || !quality || !analyzer->flux_times) {
        return -1;
    }
    
    memset(quality, 0, sizeof(*quality));
    
    /* Calculate interval statistics */
    double sum = 0, sum_sq = 0;
    double min_int = 1.0, max_int = 0;
    int short_count = 0, long_count = 0;
    
    double bitcell = analyzer->nominal_bitcell;
    if (bitcell <= 0) bitcell = BITCELL_250KBPS;
    
    for (size_t i = 1; i < analyzer->flux_count; i++) {
        double interval = analyzer->flux_times[i] - analyzer->flux_times[i-1];
        
        sum += interval;
        sum_sq += interval * interval;
        
        if (interval < min_int) min_int = interval;
        if (interval > max_int) max_int = interval;
        
        /* Count anomalies */
        if (interval < bitcell * 0.5) short_count++;
        if (interval > bitcell * 4.0) long_count++;
    }
    
    size_t n = analyzer->flux_count - 1;
    double mean = sum / n;
    double variance = (sum_sq / n) - (mean * mean);
    double std_dev = sqrt(variance > 0 ? variance : 0);
    
    quality->mean_interval = mean;
    quality->std_dev = std_dev;
    quality->min_interval = min_int;
    quality->max_interval = max_int;
    quality->short_pulses = short_count;
    quality->long_pulses = long_count;
    
    /* Calculate quality score (0-100) */
    double jitter = std_dev / mean;
    double anomaly_rate = (double)(short_count + long_count) / n;
    
    quality->jitter_percent = jitter * 100.0;
    quality->anomaly_rate = anomaly_rate * 100.0;
    
    /* Score: lower jitter and anomaly rate = higher score */
    double score = 100.0;
    score -= jitter * 200.0;          /* -20 points per 10% jitter */
    score -= anomaly_rate * 1000.0;   /* -10 points per 1% anomaly */
    
    if (score < 0) score = 0;
    if (score > 100) score = 100;
    
    quality->quality_score = (int)score;
    
    return 0;
}

/*===========================================================================
 * Revolution Handling
 *===========================================================================*/

int uft_flux_split_revolutions(const uft_flux_analyzer_t *analyzer,
                               const double *index_times, size_t index_count,
                               uft_flux_revolution_t *revs, size_t max_revs,
                               size_t *rev_count)
{
    if (!analyzer || !index_times || !revs || !rev_count) {
        return -1;
    }
    
    *rev_count = 0;
    
    if (index_count < 2) return -1;
    
    for (size_t i = 0; i < index_count - 1 && *rev_count < max_revs; i++) {
        uft_flux_revolution_t *rev = &revs[*rev_count];
        
        rev->start_time = index_times[i];
        rev->end_time = index_times[i + 1];
        rev->duration = rev->end_time - rev->start_time;
        rev->rpm = 60.0 / rev->duration;
        
        /* Count flux in this revolution */
        rev->flux_count = 0;
        rev->start_flux = 0;
        
        for (size_t f = 0; f < analyzer->flux_count; f++) {
            double t = analyzer->flux_times[f];
            
            if (t >= rev->start_time && t < rev->end_time) {
                if (rev->flux_count == 0) {
                    rev->start_flux = f;
                }
                rev->flux_count++;
            }
        }
        
        (*rev_count)++;
    }
    
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

const char *uft_flux_encoding_name(uft_flux_encoding_t enc)
{
    switch (enc) {
        case UFT_FLUX_ENCODING_FM:  return "FM";
        case UFT_FLUX_ENCODING_MFM: return "MFM";
        case UFT_FLUX_ENCODING_GCR: return "GCR";
        default:                    return "Unknown";
    }
}

int uft_flux_analyzer_report_json(const uft_flux_analyzer_t *analyzer,
                                   char *buffer, size_t size)
{
    if (!analyzer || !buffer || size == 0) return -1;
    
    uft_flux_quality_t quality;
    uft_flux_calc_quality((uft_flux_analyzer_t*)analyzer, &quality);
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"flux_count\": %zu,\n"
        "  \"bit_count\": %zu,\n"
        "  \"encoding\": \"%s\",\n"
        "  \"nominal_bitcell_us\": %.3f,\n"
        "  \"quality_score\": %d,\n"
        "  \"jitter_percent\": %.2f,\n"
        "  \"anomaly_rate_percent\": %.2f,\n"
        "  \"short_pulses\": %d,\n"
        "  \"long_pulses\": %d\n"
        "}",
        analyzer->flux_count,
        analyzer->bit_count,
        uft_flux_encoding_name(analyzer->detected_encoding),
        analyzer->nominal_bitcell * 1000000.0,
        quality.quality_score,
        quality.jitter_percent,
        quality.anomaly_rate,
        quality.short_pulses,
        quality.long_pulses
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
