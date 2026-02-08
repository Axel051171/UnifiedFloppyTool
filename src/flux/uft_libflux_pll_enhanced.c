/**
 * @file uft_libflux_pll_enhanced.c
 * @version 1.0.0
 * @date 2026-01-06
 *
 * License: GPL v2
 *
 * This module contains improved PLL algorithms for flux stream analysis:
 * - detectpeaks(): Automatic bitrate detection via histogram analysis
 * - getCellTiming(): Core PLL cell timing with inter-band rejection
 * - Victor 9K variable speed band definitions
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "uft_libflux_pll_enhanced.h"

/*============================================================================
 * Constants
 *============================================================================*/

/* Default tick frequency (24 MHz) */
#define DEFAULT_TICK_FREQ   24000000

/* Bitrate detection thresholds (tick_freq / threshold) */
#define THRESH_250K_LOW     276243  /* Lower bound for 250 kbit/s */
#define THRESH_250K_HIGH    224618  /* Upper bound for 250 kbit/s */
#define THRESH_300K_LOW     353606  /* Lower bound for 300 kbit/s */
#define THRESH_300K_HIGH    276243  /* Upper bound for 300 kbit/s */
#define THRESH_500K_LOW     572082  /* Lower bound for 500 kbit/s */
#define THRESH_500K_HIGH    437062  /* Upper bound for 500 kbit/s */

/* Minimum percentage for bitrate detection */
#define MIN_DETECTION_PERCENT   2.0f

/*============================================================================
 * Victor 9K Band Definitions (Variable Speed GCR)
 *============================================================================*/

/**
 * Victor 9000/Sirius band definitions for variable speed zones
 * Format: { start_track, 1, cell1, 3, cell2, 5, cell3, 0 }
 */
static const int victor_9k_bands_def[] = {
    0,  1, 2142, 3, 3600, 5, 5200, 0,
    4,  1, 2492, 3, 3800, 5, 5312, 0,
    16, 1, 2550, 3, 3966, 5, 5552, 0,
    27, 1, 2723, 3, 4225, 5, 5852, 0,
    38, 1, 2950, 3, 4500, 5, 6450, 0,
    48, 1, 3150, 3, 4836, 5, 6800, 0,
    60, 1, 3400, 3, 5250, 5, 7500, 0,
    71, 1, 3800, 3, 5600, 5, 8000, 0,
    -1, 0,    0, 0,    0, 0,    0, 0  /* Terminator */
};

/*============================================================================
 * Internal Structures
 *============================================================================*/

/**
 * @brief Histogram statistics entry for peak detection
 */
typedef struct {
    uint32_t val;           /**< Timing value */
    uint32_t occurence;     /**< Number of occurrences */
    float    pourcent;      /**< Percentage of total */
} libflux_stathisto_t;

/*============================================================================
 * PLL State Management
 *============================================================================*/

uft_libflux_pll_t* uft_libflux_pll_create(void)
{
    uft_libflux_pll_t* pll = calloc(1, sizeof(uft_libflux_pll_t));
    if (!pll) return NULL;

    /* Set defaults */
    pll->tick_freq = DEFAULT_TICK_FREQ;
    pll->pll_min_max_percent = 18;  /* ±18% window */
    
    /* Fast correction: 1/2 */
    pll->fast_correction_ratio_n = 1;
    pll->fast_correction_ratio_d = 2;
    
    /* Slow correction: 3/4 */
    pll->slow_correction_ratio_n = 3;
    pll->slow_correction_ratio_d = 4;
    
    pll->inter_band_rejection = 0;
    pll->band_mode = 0;
    
    return pll;
}

void uft_libflux_pll_destroy(uft_libflux_pll_t* pll)
{
    free(pll);
}

void uft_libflux_pll_reset(uft_libflux_pll_t* pll)
{
    if (!pll) return;
    
    pll->pump_charge = 0;
    pll->phase = 0;
    pll->lastpulsephase = 0;
    pll->last_error = 0;
    pll->pll_min = 0;
    pll->pivot = 0;
    pll->pll_max = 0;
}

/*============================================================================
 * Histogram Analysis
 *============================================================================*/

void uft_libflux_compute_histogram(const uint32_t* indata, size_t size, 
                                uint32_t* outdata)
{
    if (!indata || !outdata) return;
    
    memset(outdata, 0, sizeof(uint32_t) * UFT_LIBFLUX_HISTOGRAM_SIZE);
    
    for (size_t i = 0; i < size; i++) {
        if (indata[i] < UFT_LIBFLUX_HISTOGRAM_SIZE) {
            outdata[indata[i]]++;
        }
    }
}

/*============================================================================
 * Automatic Bitrate Detection
 *============================================================================*/

uint32_t uft_libflux_detectpeaks(uft_libflux_pll_t* pll, const uint32_t* histogram)
{
    if (!pll || !histogram) return 0;
    
    int total = 0;
    int nbval = 0;
    
    /* Count total samples and unique values */
    for (int i = 0; i < UFT_LIBFLUX_HISTOGRAM_SIZE; i++) {
        total += histogram[i];
        if (histogram[i]) nbval++;
    }
    
    if (total == 0 || nbval == 0) return 0;
    
    /* Build statistics table */
    libflux_stathisto_t* stattab = calloc(nbval + 1, sizeof(libflux_stathisto_t));
    if (!stattab) return 0;
    
    int k = 0;
    for (int i = 0; i < UFT_LIBFLUX_HISTOGRAM_SIZE; i++) {
        if (histogram[i]) {
            stattab[k].occurence = histogram[i];
            stattab[k].val = i;
            stattab[k].pourcent = ((float)stattab[k].occurence / (float)total) * 100.0f;
            k++;
        }
    }
    
    /* Calculate percentages for each bitrate band */
    float pourcent250k = 0.0f;
    float pourcent300k = 0.0f;
    float pourcent500k = 0.0f;
    
    for (int i = 0; i < nbval; i++) {
        uint32_t val = stattab[i].val;
        
        /* 250 kbit/s band */
        if (val < (pll->tick_freq / THRESH_250K_HIGH) && 
            val >= (pll->tick_freq / THRESH_250K_LOW)) {
            pourcent250k += stattab[i].pourcent;
        }
        
        /* 300 kbit/s band */
        if (val < (pll->tick_freq / THRESH_300K_HIGH) && 
            val >= (pll->tick_freq / THRESH_300K_LOW)) {
            pourcent300k += stattab[i].pourcent;
        }
        
        /* 500 kbit/s band */
        if (val < (pll->tick_freq / THRESH_500K_HIGH) && 
            val > (pll->tick_freq / THRESH_500K_LOW)) {
            pourcent500k += stattab[i].pourcent;
        }
    }
    
    free(stattab);
    
    /* Determine most likely bitrate */
    if (pourcent500k > MIN_DETECTION_PERCENT) {
        return pll->tick_freq / 500000;  /* 500 kbit/s (HD) */
    }
    
    if (pourcent300k > MIN_DETECTION_PERCENT && pourcent300k > pourcent250k) {
        return pll->tick_freq / 300000;  /* 300 kbit/s (Amiga/Atari) */
    }
    
    if (pourcent250k > MIN_DETECTION_PERCENT && pourcent250k > pourcent300k) {
        return pll->tick_freq / 250000;  /* 250 kbit/s (PC DD) */
    }
    
    /* Default based on which is higher */
    if (pourcent300k > pourcent250k) {
        return pll->tick_freq / 300000;
    }
    return pll->tick_freq / 250000;
}

/*============================================================================
 * Band Mode (Variable Speed like Victor 9K)
 *============================================================================*/

static int get_band_cells(uft_libflux_pll_t* pll, int current_pulsevalue)
{
    int i = 0;
    
    while (i < 16 && (pll->bands_separators[i] == -1 || 
           pll->bands_separators[i] < current_pulsevalue)) {
        i++;
    }
    
    if (i > 0) i--;
    return i + 1;
}

/*============================================================================
 * Core PLL Cell Timing
 *============================================================================*/

int uft_libflux_get_cell_timing(uft_libflux_pll_t* pll, int current_pulsevalue,
                            bool* badpulse, int overlapval, int phasecorrection)
{
    if (!pll) return 0;
    
    int blankcell = 0;
    
    /* Band mode for variable speed drives (Victor 9K) */
    if (pll->band_mode) {
        return get_band_cells(pll, current_pulsevalue);
    }
    
    /* Scale pulse value for precision */
    current_pulsevalue = current_pulsevalue * 16;
    
    /* Overflow protection for very long tracks */
    if (pll->phase > (512 * 1024 * 1014)) {
        pll->phase -= (256 * 1024 * 1014);
        pll->lastpulsephase -= (256 * 1024 * 1014);
    }
    
    /* Calculate window boundaries */
    int left_boundary = pll->phase;
    int right_boundary = pll->phase + pll->pump_charge;
    int center = pll->phase + (pll->pump_charge / 2);
    int current_pulse_position = pll->lastpulsephase + current_pulsevalue;
    
    pll->last_error = 0xFFFF;
    
    /* Pulse is before current window? */
    if (current_pulse_position < left_boundary) {
        pll->lastpulsephase += current_pulsevalue;
        if (badpulse) *badpulse = true;
        return blankcell;
    }
    
    blankcell = 1;
    
    /* Count cells until pulse is within window */
    while (current_pulse_position > right_boundary) {
        pll->phase += pll->pump_charge;
        left_boundary = pll->phase;
        right_boundary = pll->phase + pll->pump_charge;
        center = pll->phase + (pll->pump_charge / 2);
        blankcell++;
    }
    
    /* Inter-band rejection for specific encodings */
    if (pll->inter_band_rejection) {
        switch (pll->inter_band_rejection) {
            case UFT_LIBFLUX_BAND_GCR:
                /* GCR: Reject cells 3 and 5 */
                if (blankcell == 3) {
                    blankcell = (right_boundary - current_pulse_position > pll->pump_charge / 2) ? 2 : 4;
                }
                if (blankcell == 5) {
                    blankcell = (right_boundary - current_pulse_position > pll->pump_charge / 2) ? 4 : 6;
                }
                break;
                
            case UFT_LIBFLUX_BAND_FM:
                /* FM: Force to 2 or 4 cells */
                if (blankcell == 1) {
                    blankcell = 2;
                }
                if (blankcell == 3) {
                    blankcell = (right_boundary - current_pulse_position > pll->pump_charge / 2) ? 2 : 4;
                }
                if (blankcell > 4) {
                    blankcell = 4;
                }
                break;
        }
    }
    
    /* Phase correction */
    if (phasecorrection) {
        int cur_pll_error = current_pulse_position - center;
        
        /* Fast or slow correction based on error sign */
        if (cur_pll_error >= 0) {
            pll->pump_charge += (cur_pll_error * pll->fast_correction_ratio_n) / 
                                pll->fast_correction_ratio_d;
        } else {
            pll->pump_charge += (cur_pll_error * pll->slow_correction_ratio_n) / 
                                pll->slow_correction_ratio_d;
        }
        
        /* Clamp pump_charge to min/max */
        if (pll->pump_charge < pll->pll_min) {
            pll->pump_charge = pll->pll_min;
        }
        if (pll->pump_charge > pll->pll_max) {
            pll->pump_charge = pll->pll_max;
        }
        
        pll->last_error = abs(cur_pll_error);
    }
    
    /* Update phase for next pulse */
    pll->lastpulsephase = current_pulse_position;
    pll->phase = right_boundary;
    
    if (badpulse) *badpulse = false;
    
    return blankcell;
}

/*============================================================================
 * PLL Configuration
 *============================================================================*/

void uft_libflux_pll_set_bitrate(uft_libflux_pll_t* pll, uint32_t bitrate_hz)
{
    if (!pll || bitrate_hz == 0) return;
    
    /* Calculate cell size in ticks */
    pll->pivot = pll->tick_freq / bitrate_hz;
    
    /* Calculate min/max with percentage window */
    int window = (pll->pivot * pll->pll_min_max_percent) / 100;
    pll->pll_min = (pll->pivot - window) * 16;
    pll->pll_max = (pll->pivot + window) * 16;
    pll->pump_charge = pll->pivot * 16;
}

void uft_libflux_pll_set_tick_freq(uft_libflux_pll_t* pll, uint32_t tick_freq)
{
    if (!pll || tick_freq == 0) return;
    pll->tick_freq = tick_freq;
}

void uft_libflux_pll_set_band_mode(uft_libflux_pll_t* pll, int track)
{
    if (!pll) return;
    
    /* Find appropriate Victor 9K band for track */
    const int* band = victor_9k_bands_def;
    
    while (band[0] >= 0) {
        if (track >= band[0]) {
            /* Copy band separators */
            for (int i = 0; i < 8 && i < 16; i++) {
                pll->bands_separators[i] = band[i];
            }
        }
        band += 8;  /* Next band definition */
    }
    
    pll->band_mode = 1;
    pll->track = track;
}

void uft_libflux_pll_set_inter_band_rejection(uft_libflux_pll_t* pll, int mode)
{
    if (!pll) return;
    pll->inter_band_rejection = mode;
}

/*============================================================================
 * Bitrate Detection Helper
 *============================================================================*/

uft_libflux_bitrate_t uft_libflux_detect_bitrate(const uint32_t* flux_data, 
                                          size_t num_samples,
                                          uint32_t tick_freq)
{
    uft_libflux_bitrate_t result = {0};
    
    if (!flux_data || num_samples == 0) return result;
    
    /* Allocate histogram */
    uint32_t* histogram = calloc(UFT_LIBFLUX_HISTOGRAM_SIZE, sizeof(uint32_t));
    if (!histogram) return result;
    
    /* Compute histogram */
    uft_libflux_compute_histogram(flux_data, num_samples, histogram);
    
    /* Create temporary PLL for detection */
    uft_libflux_pll_t pll = {0};
    pll.tick_freq = tick_freq ? tick_freq : DEFAULT_TICK_FREQ;
    
    /* Detect bitrate */
    uint32_t cell_ticks = uft_libflux_detectpeaks(&pll, histogram);
    
    if (cell_ticks > 0) {
        result.detected = true;
        result.cell_ticks = cell_ticks;
        result.bitrate_hz = pll.tick_freq / cell_ticks;
        
        /* Classify */
        if (result.bitrate_hz >= 450000) {
            result.encoding = UFT_LIBFLUX_ENC_HD;
        } else if (result.bitrate_hz >= 280000) {
            result.encoding = UFT_LIBFLUX_ENC_DD_300K;
        } else {
            result.encoding = UFT_LIBFLUX_ENC_DD_250K;
        }
    }
    
    free(histogram);
    return result;
}
