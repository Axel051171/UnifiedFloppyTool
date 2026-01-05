// SPDX-License-Identifier: MIT
/*
 * adaptive_mfm.c - Adaptive MFM Processing Algorithm
 * 
 * Ported from FloppyControl (C# -> C)
 * Original: https://github.com/Skaizo/FloppyControl
 * 
 * This algorithm uses dynamic thresholds that adapt to the disk's
 * actual timing characteristics. Essential for recovering data from
 * degraded media with motor speed drift or weak signals.
 * 
 * Features:
 *   - Low-pass filtered threshold tracking
 *   - Automatic bit cell timing adjustment
 *   - Entropy tracking for analysis
 *   - Noise injection for testing
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/*============================================================================*
 * CONFIGURATION
 *============================================================================*/

/* Default MFM bit cell timings (in timer ticks, ~2µs resolution) */
#define DEFAULT_4US     20      /* Short pulse: 4µs */
#define DEFAULT_6US     30      /* Medium pulse: 6µs */
#define DEFAULT_8US     40      /* Long pulse: 8µs */

/* Processing parameters */
#define DEFAULT_RATE_OF_CHANGE      1.0f
#define DEFAULT_LOWPASS_RADIUS      32
#define MAX_LOOKAHEAD               10

/*============================================================================*
 * DATA STRUCTURES
 *============================================================================*/

typedef enum {
    DISK_FORMAT_UNKNOWN = 0,
    DISK_FORMAT_PC_DD,          /* PC DOS 720K */
    DISK_FORMAT_PC_HD,          /* PC DOS 1.44M */
    DISK_FORMAT_PC_2M,          /* Extended 2M format */
    DISK_FORMAT_AMIGA_DOS,      /* Amiga OFS/FFS */
    DISK_FORMAT_AMIGA_DISKSPARE,/* Amiga DiskSpare */
    DISK_FORMAT_ATARI_ST,       /* Atari ST */
    DISK_FORMAT_MSX             /* MSX-DOS */
} disk_format_t;

typedef struct {
    /* Input parameters */
    int fourus;                 /* 4µs timing threshold */
    int sixus;                  /* 6µs timing threshold */
    int eightus;                /* 8µs timing threshold */
    float rate_of_change;       /* Adaptation rate */
    int lowpass_radius;         /* Low-pass filter window */
    int start;                  /* Start offset in input */
    int end;                    /* End offset in input */
    bool is_hd;                 /* High-density flag (2x timing) */
    bool add_noise;             /* Enable noise injection */
    int noise_amount;           /* Noise amplitude */
    int noise_limit_start;      /* Noise window start */
    int noise_limit_end;        /* Noise window end */
} adaptive_settings_t;

typedef struct {
    uint8_t *mfm_data;          /* Output MFM bitstream */
    size_t mfm_length;          /* Output length */
    float *entropy;             /* Entropy/timing deviation per sample */
    size_t entropy_length;      /* Entropy array length */
    int stat_4us;               /* Count of 4µs pulses */
    int stat_6us;               /* Count of 6µs pulses */
    int stat_8us;               /* Count of 8µs pulses */
} adaptive_result_t;

/*============================================================================*
 * DEFAULT SETTINGS
 *============================================================================*/

void adaptive_default_settings(adaptive_settings_t *settings) {
    if (!settings) return;
    
    settings->fourus = DEFAULT_4US;
    settings->sixus = DEFAULT_6US;
    settings->eightus = DEFAULT_8US;
    settings->rate_of_change = DEFAULT_RATE_OF_CHANGE;
    settings->lowpass_radius = DEFAULT_LOWPASS_RADIUS;
    settings->start = 0;
    settings->end = 0;
    settings->is_hd = false;
    settings->add_noise = false;
    settings->noise_amount = 0;
    settings->noise_limit_start = 0;
    settings->noise_limit_end = 0;
}

/*============================================================================*
 * ADAPTIVE MFM PROCESSING
 *============================================================================*/

/**
 * @brief Process period data to MFM using adaptive thresholds
 * 
 * This is the core algorithm from FloppyControl. It converts raw
 * flux timing data to MFM bitstream while dynamically adjusting
 * the timing thresholds based on observed data.
 * 
 * @param rxbuf Input period data (flux timing)
 * @param rxbuf_len Input length
 * @param settings Processing settings
 * @param result Output result (caller must free)
 * @return 0 on success, -1 on error
 */
int adaptive_period_to_mfm(
    const uint8_t *rxbuf,
    size_t rxbuf_len,
    const adaptive_settings_t *settings,
    adaptive_result_t *result)
{
    if (!rxbuf || !settings || !result || rxbuf_len == 0) {
        return -1;
    }
    
    /* Extract settings */
    int fourus = settings->fourus;
    int sixus = settings->sixus;
    int eightus = settings->eightus;
    float rate_of_change = settings->rate_of_change;
    int lowpass_radius = settings->lowpass_radius;
    int start = settings->start;
    int end = settings->end;
    
    /* Validate bounds */
    if (end == 0) end = (int)rxbuf_len;
    if (start >= end || start < 0 || end > (int)rxbuf_len) {
        return -1;
    }
    
    /* Allocate output buffers */
    size_t max_mfm_len = (end - start) * 5;  /* Max 4 bits per period + margin */
    result->mfm_data = calloc(max_mfm_len, 1);
    result->entropy = calloc(rxbuf_len, sizeof(float));
    
    if (!result->mfm_data || !result->entropy) {
        free(result->mfm_data);
        free(result->entropy);
        return -1;
    }
    
    /* Allocate low-pass filter buffers */
    float *lowpass4 = malloc(lowpass_radius * sizeof(float));
    float *lowpass6 = malloc(lowpass_radius * sizeof(float));
    float *lowpass8 = malloc(lowpass_radius * sizeof(float));
    
    if (!lowpass4 || !lowpass6 || !lowpass8) {
        free(result->mfm_data);
        free(result->entropy);
        free(lowpass4);
        free(lowpass6);
        free(lowpass8);
        return -1;
    }
    
    /* Initialize low-pass buffers */
    for (int i = 0; i < lowpass_radius; i++) {
        lowpass4[i] = (float)fourus;
        lowpass6[i] = (float)sixus;
        lowpass8[i] = (float)eightus;
    }
    
    float _4usavg = lowpass_radius * (float)fourus;
    float _6usavg = lowpass_radius * (float)sixus;
    float _8usavg = lowpass_radius * (float)eightus;
    
    /* Calculate base thresholds */
    int base_threshold4 = (sixus - fourus) / 2;
    int base_threshold6 = (eightus - sixus) / 2;
    
    float threshold4us = fourus + base_threshold4;
    float threshold6us = sixus + base_threshold6;
    
    float fourus_f = (float)fourus;
    float sixus_f = (float)sixus;
    float eightus_f = (float)eightus;
    
    float average_time = 0;
    size_t mfm_len = 0;
    
    /* Statistics */
    result->stat_4us = 0;
    result->stat_6us = 0;
    result->stat_8us = 0;
    
    /* Main processing loop */
    for (int i = start; i < end - MAX_LOOKAHEAD; i++) {
        int raw_value = rxbuf[i];
        
        /* Skip index signals (very short pulses) */
        if (raw_value < 4) continue;
        
        /* Apply HD scaling if needed */
        int value = settings->is_hd ? (raw_value << 1) : raw_value;
        
        /* Apply rate-of-change correction */
        value -= (int)(average_time / rate_of_change);
        
        /* Classify pulse and output MFM bits */
        if (value <= threshold4us) {
            /* 4µs pulse -> "10" */
            result->mfm_data[mfm_len++] = 1;
            result->mfm_data[mfm_len++] = 0;
            result->stat_4us++;
            
            /* Update adaptive threshold */
            float _4us = fourus_f - raw_value;
            if (lowpass_radius > 0) {
                int idx = (i + 1) % lowpass_radius;
                _4usavg -= lowpass4[idx];
                lowpass4[idx] = (float)raw_value;
                _4usavg += raw_value;
                fourus_f = _4usavg / lowpass_radius;
            }
            average_time = _4us;
        }
        else if (value > threshold4us && value < threshold6us) {
            /* 6µs pulse -> "100" */
            result->mfm_data[mfm_len++] = 1;
            result->mfm_data[mfm_len++] = 0;
            result->mfm_data[mfm_len++] = 0;
            result->stat_6us++;
            
            float _6us = sixus_f - raw_value;
            if (lowpass_radius > 0) {
                int idx = (i + 1) % lowpass_radius;
                _6usavg -= lowpass6[idx];
                lowpass6[idx] = (float)raw_value;
                _6usavg += raw_value;
                sixus_f = _6usavg / lowpass_radius;
            }
            average_time = _6us;
        }
        else {
            /* 8µs pulse -> "1000" */
            result->mfm_data[mfm_len++] = 1;
            result->mfm_data[mfm_len++] = 0;
            result->mfm_data[mfm_len++] = 0;
            result->mfm_data[mfm_len++] = 0;
            result->stat_8us++;
            
            float _8us = eightus_f - raw_value;
            if (lowpass_radius > 0) {
                int idx = (i + 1) % lowpass_radius;
                _8usavg -= lowpass8[idx];
                lowpass8[idx] = (float)raw_value;
                _8usavg += raw_value;
                eightus_f = _8usavg / lowpass_radius;
            }
            average_time = _8us;
        }
        
        /* Store entropy for analysis */
        result->entropy[i] = average_time;
        
        /* Update thresholds based on adapted values */
        threshold4us = fourus_f + (sixus_f - fourus_f) / 2;
        threshold6us = sixus_f + (eightus_f - sixus_f) / 2;
    }
    
    result->mfm_length = mfm_len;
    result->entropy_length = rxbuf_len;
    
    /* Cleanup */
    free(lowpass4);
    free(lowpass6);
    free(lowpass8);
    
    return 0;
}

/*============================================================================*
 * HISTOGRAM PEAK DETECTION
 *============================================================================*/

typedef struct {
    int peak1;      /* 4µs peak position */
    int peak2;      /* 6µs peak position */
    int peak3;      /* 8µs peak position */
    int count1;     /* Count at peak1 */
    int count2;     /* Count at peak2 */
    int count3;     /* Count at peak3 */
} histogram_peaks_t;

/**
 * @brief Build histogram from period data
 * 
 * @param rxbuf Input period data
 * @param len Input length
 * @param histogram Output histogram (must be 256 elements)
 * @param max_val Maximum value in histogram
 */
void build_histogram(const uint8_t *rxbuf, size_t len, int *histogram, int *max_val) {
    memset(histogram, 0, 256 * sizeof(int));
    *max_val = 0;
    
    for (size_t i = 0; i < len; i++) {
        uint8_t v = rxbuf[i];
        if (v >= 4 && v < 256) {  /* Skip index pulses */
            histogram[v]++;
            if (histogram[v] > *max_val) {
                *max_val = histogram[v];
            }
        }
    }
}

/**
 * @brief Find peaks in histogram for automatic threshold detection
 * 
 * @param histogram Input histogram (256 elements)
 * @param peaks Output peak positions
 */
void find_histogram_peaks(const int *histogram, histogram_peaks_t *peaks) {
    if (!histogram || !peaks) return;
    
    memset(peaks, 0, sizeof(*peaks));
    
    /* Simple peak detection: find 3 highest local maxima */
    int prev = 0;
    int local_peaks[20];
    int local_counts[20];
    int num_peaks = 0;
    
    for (int i = 5; i < 100 && num_peaks < 20; i++) {
        int curr = histogram[i];
        int next = histogram[i + 1];
        
        /* Local maximum: higher than neighbors */
        if (curr > prev && curr > next && curr > 10) {
            local_peaks[num_peaks] = i;
            local_counts[num_peaks] = curr;
            num_peaks++;
        }
        prev = curr;
    }
    
    /* Sort by count (descending) and take top 3 */
    for (int i = 0; i < num_peaks - 1; i++) {
        for (int j = i + 1; j < num_peaks; j++) {
            if (local_counts[j] > local_counts[i]) {
                int tmp = local_peaks[i];
                local_peaks[i] = local_peaks[j];
                local_peaks[j] = tmp;
                tmp = local_counts[i];
                local_counts[i] = local_counts[j];
                local_counts[j] = tmp;
            }
        }
    }
    
    /* Assign peaks in order (4µs < 6µs < 8µs) */
    int p[3] = {0, 0, 0};
    int c[3] = {0, 0, 0};
    int n = (num_peaks < 3) ? num_peaks : 3;
    
    for (int i = 0; i < n; i++) {
        p[i] = local_peaks[i];
        c[i] = local_counts[i];
    }
    
    /* Sort by position (ascending) */
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (p[j] < p[i]) {
                int tmp = p[i]; p[i] = p[j]; p[j] = tmp;
                tmp = c[i]; c[i] = c[j]; c[j] = tmp;
            }
        }
    }
    
    peaks->peak1 = p[0];
    peaks->peak2 = p[1];
    peaks->peak3 = p[2];
    peaks->count1 = c[0];
    peaks->count2 = c[1];
    peaks->count3 = c[2];
}

/**
 * @brief Auto-configure settings from period data
 * 
 * Analyzes the histogram to automatically determine optimal
 * threshold settings for the specific disk.
 * 
 * @param rxbuf Input period data
 * @param len Input length
 * @param settings Settings to configure
 * @return 0 on success
 */
int adaptive_auto_configure(const uint8_t *rxbuf, size_t len, adaptive_settings_t *settings) {
    if (!rxbuf || !settings || len == 0) return -1;
    
    int histogram[256];
    int max_val;
    
    build_histogram(rxbuf, len, histogram, &max_val);
    
    histogram_peaks_t peaks;
    find_histogram_peaks(histogram, &peaks);
    
    if (peaks.peak1 > 0 && peaks.peak2 > 0 && peaks.peak3 > 0) {
        settings->fourus = peaks.peak1;
        settings->sixus = peaks.peak2;
        settings->eightus = peaks.peak3;
        return 0;
    }
    
    /* Fallback to defaults if peaks not found */
    return -1;
}

/*============================================================================*
 * CLEANUP
 *============================================================================*/

void adaptive_free_result(adaptive_result_t *result) {
    if (!result) return;
    free(result->mfm_data);
    free(result->entropy);
    result->mfm_data = NULL;
    result->entropy = NULL;
    result->mfm_length = 0;
    result->entropy_length = 0;
}
