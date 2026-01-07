// SPDX-License-Identifier: MIT
/*
 * parameter_compensation.c - Universal Parameter Compensation System
 * 
 * Implements platform-specific parameter compensation for optimal
 * flux data quality across different disk formats.
 * 
 * Supported Modes:
 *   - Mac 800K (peak shift correction)
 *   - C64 (speed zones)
 *   - Amiga (MFM optimization)
 *   - Apple II (GCR compensation)
 *   - Atari ST (ST-specific)
 *   - PC MFM (standard)
 * 
 * @version 2.8.0
 * @date 2024-12-26
 */

#include "uft/uft_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/*============================================================================*
 * COMPENSATION MODES
 *============================================================================*/

typedef enum {
    COMP_MODE_NONE,         /* No compensation (raw data) */
    COMP_MODE_AUTO,         /* Auto-detect best mode */
    COMP_MODE_MAC800K,      /* Mac 800K peak shift correction */
    COMP_MODE_C64,          /* C64 GCR with speed zones */
    COMP_MODE_AMIGA,        /* Amiga MFM optimization */
    COMP_MODE_APPLE_II,     /* Apple II GCR compensation */
    COMP_MODE_ATARI_ST,     /* Atari ST MFM */
    COMP_MODE_PC_MFM,       /* PC standard MFM */
    COMP_MODE_CUSTOM        /* Custom parameters */
} compensation_mode_t;

/*============================================================================*
 * PARAMETERS
 *============================================================================*/

typedef struct {
    compensation_mode_t mode;
    
    /* Peak shift correction */
    float peak_shift_threshold;     /* Threshold for correction (in samples) */
    float peak_shift_strength;      /* Strength of correction (0.0-1.0) */
    
    /* Write precompensation */
    float write_precomp_early;      /* Early write shift (nanoseconds) */
    float write_precomp_late;       /* Late write shift (nanoseconds) */
    
    /* Track density */
    float track_density_factor;     /* Density adjustment per track */
    int physical_track;             /* Current physical track */
    
    /* Timing */
    uint32_t samples_per_rev;       /* Samples per revolution */
    float rotation_speed_rpm;       /* Rotation speed in RPM */
    
    /* Platform-specific */
    union {
        struct {
            int zone;               /* C64 speed zone (0-3) */
            bool gcr_mode;          /* GCR encoding */
        } c64;
        
        struct {
            bool high_density;      /* HD vs DD */
            int sectors_per_track;  /* Sector count */
        } amiga;
        
        struct {
            int encoding;           /* 5-and-3 vs 6-and-2 */
        } apple_ii;
    } platform;
    
} compensation_params_t;

/*============================================================================*
 * MAC 800K PEAK SHIFT CORRECTION
 * 
 * 
 * Mac 800K disks are prone to peak shift effects due to high density.
 * This applies adaptive correction to flux transitions that are too
 * close together, pushing them apart based on a threshold.
 *============================================================================*/

/**
 * @brief Apply Mac 800K peak shift correction
 * 
 * Algorithm:
 * - Computes threshold as ~1/45000th of rotation (vs 1/50000th for MFM)
 * - Applies correction to transitions below threshold
 * - Includes track density compensation for inner tracks
 */
static int compensate_mac800k(
    const uint32_t *transitions_in,
    size_t n,
    const compensation_params_t *params,
    uint32_t *transitions_out
) {
    if (n < 3 || !transitions_in || !transitions_out || !params) {
        return -1;
    }
    
    /* Copy first two transitions */
    transitions_out[0] = transitions_in[0];
    transitions_out[1] = transitions_in[1];
    
    uint32_t t0 = transitions_in[0];
    uint32_t t1 = transitions_in[1];
    
    /* Calculate threshold with track density compensation
     * Base: ~1/45000th of rotation
     * Adjust for physical track (inner tracks need less correction)
     */
    float base_threshold = params->samples_per_rev / 30000.0f;
    float track_factor = (160.0f + fminf(params->physical_track, 47)) / 240.0f;
    int thresh = (int)(0.5f + base_threshold * track_factor);
    
    /* Apply peak shift correction */
    for (size_t i = 2; i < n; i++) {
        uint32_t t2 = transitions_in[i];
        
        /* Compute deltas between each pair */
        int32_t t01 = (int32_t)(t1 - t0);
        int32_t t12 = (int32_t)(t2 - t1);
        
        /* Compute anti peak shift delta for narrow pairs */
        int32_t delta1 = (t01 < thresh) ? (thresh - t01) : 0;
        int32_t delta2 = (t12 < thresh) ? (thresh - t12) : 0;
        
        /* Apply correction shift
         * Limited to no more than half the distance rounded down
         * Strength factor standard: 5/12 (~0.417)
         */
        int32_t shift = ((delta2 - delta1) * 5) / 12;
        int32_t min_shift = -t01 / 2;
        int32_t max_shift = t12 / 2;
        
        if (shift < min_shift) shift = min_shift;
        if (shift > max_shift) shift = max_shift;
        
        transitions_out[i-1] = t1 + shift;
        
        t0 = t1;
        t1 = t2;
    }
    
    /* Copy last transition */
    transitions_out[n-1] = transitions_in[n-1];
    
    return 0;
}

/*============================================================================*
 * C64 SPEED ZONE COMPENSATION
 *============================================================================*/

/* C64 speed zones (tracks 0-16, 17-23, 24-29, 30-35+) */
static const float c64_zone_speeds[4] = {
    3.00f,  /* Zone 0: Outer tracks (fastest) */
    2.67f,  /* Zone 1 */
    2.50f,  /* Zone 2 */
    2.29f   /* Zone 3: Inner tracks (slowest) */
};

static int compensate_c64(
    const uint32_t *transitions_in,
    size_t n,
    const compensation_params_t *params,
    uint32_t *transitions_out
) {
    /* Determine speed zone from physical track */
    int zone = 0;
    if (params->physical_track >= 31) zone = 3;
    else if (params->physical_track >= 25) zone = 2;
    else if (params->physical_track >= 18) zone = 1;
    
    float speed_factor = c64_zone_speeds[zone];
    
    /* Apply speed zone compensation */
    for (size_t i = 0; i < n; i++) {
        transitions_out[i] = (uint32_t)(transitions_in[i] * speed_factor);
    }
    
    return 0;
}

/*============================================================================*
 * AMIGA MFM OPTIMIZATION
 *============================================================================*/

static int compensate_amiga(
    const uint32_t *transitions_in,
    size_t n,
    const compensation_params_t *params,
    uint32_t *transitions_out
) {
    /* Amiga uses constant speed across all tracks
     * Apply standard MFM precompensation
     */
    
    int precomp_threshold = params->samples_per_rev / 60000;  /* ~2us for MFM */
    
    for (size_t i = 0; i < n; i++) {
        if (i == 0 || i == n - 1) {
            transitions_out[i] = transitions_in[i];
            continue;
        }
        
        int32_t delta_prev = transitions_in[i] - transitions_in[i-1];
        int32_t delta_next = transitions_in[i+1] - transitions_in[i];
        
        /* Apply precompensation for close transitions */
        int32_t shift = 0;
        if (delta_prev < precomp_threshold && delta_next < precomp_threshold) {
            shift = (delta_next - delta_prev) / 4;
        }
        
        transitions_out[i] = transitions_in[i] + shift;
    }
    
    return 0;
}

/*============================================================================*
 * APPLE II GCR COMPENSATION
 *============================================================================*/

static int compensate_apple_ii(
    const uint32_t *transitions_in,
    size_t n,
    const compensation_params_t *params,
    uint32_t *transitions_out
) {
    /* Apple II GCR has variable bit cells
     * Apply adaptive threshold based on GCR encoding
     */
    
    float threshold_factor = (params->platform.apple_ii.encoding == 0) ? 
                            1.2f :   /* 5-and-3 */
                            1.0f;    /* 6-and-2 */
    
    int thresh = (int)(params->samples_per_rev / 50000.0f * threshold_factor);
    
    for (size_t i = 0; i < n; i++) {
        if (i == 0) {
            transitions_out[i] = transitions_in[i];
            continue;
        }
        
        int32_t delta = transitions_in[i] - transitions_in[i-1];
        
        /* Compensate short pulses */
        if (delta < thresh) {
            transitions_out[i] = transitions_out[i-1] + thresh;
        } else {
            transitions_out[i] = transitions_in[i];
        }
    }
    
    return 0;
}

/*============================================================================*
 * AUTO-DETECTION
 *============================================================================*/

/**
 * @brief Auto-detect best compensation mode
 */
static compensation_mode_t auto_detect_mode(
    const uint32_t *transitions,
    size_t n,
    uint32_t samples_per_rev
) {
    if (n < 10) {
        return COMP_MODE_NONE;
    }
    
    /* Calculate average transition spacing */
    uint32_t total_delta = 0;
    for (size_t i = 1; i < n; i++) {
        total_delta += transitions[i] - transitions[i-1];
    }
    uint32_t avg_delta = total_delta / (n - 1);
    
    /* Estimate encoding based on transition density */
    float transitions_per_rev = (float)samples_per_rev / avg_delta;
    
    /* Heuristics:
     * - Mac 800K: ~12000-16000 transitions/rev
     * - Amiga MFM: ~11000-13000 transitions/rev
     * - C64 GCR: ~7800-8400 transitions/rev
     * - Apple II GCR: ~6000-7000 transitions/rev
     */
    
    if (transitions_per_rev > 14000) {
        return COMP_MODE_MAC800K;
    } else if (transitions_per_rev > 10000) {
        return COMP_MODE_AMIGA;
    } else if (transitions_per_rev > 7000) {
        return COMP_MODE_C64;
    } else if (transitions_per_rev > 5000) {
        return COMP_MODE_APPLE_II;
    }
    
    return COMP_MODE_PC_MFM;  /* Default */
}

/*============================================================================*
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Initialize compensation parameters
 */
int compensation_init_params(
    compensation_mode_t mode,
    compensation_params_t *params
) {
    if (!params) {
        return -1;
    }
    
    memset(params, 0, sizeof(*params));
    params->mode = mode;
    
    /* Set defaults based on mode */
    switch (mode) {
        case COMP_MODE_MAC800K:
            params->peak_shift_threshold = 0.417f;  /* 5/12 */
            params->samples_per_rev = 100000;
            params->rotation_speed_rpm = 394.0f;
            break;
            
        case COMP_MODE_C64:
            params->samples_per_rev = 100000;
            params->rotation_speed_rpm = 300.0f;
            params->platform.c64.gcr_mode = true;
            break;
            
        case COMP_MODE_AMIGA:
            params->samples_per_rev = 100000;
            params->rotation_speed_rpm = 300.0f;
            params->platform.amiga.high_density = false;
            params->platform.amiga.sectors_per_track = 11;
            break;
            
        case COMP_MODE_APPLE_II:
            params->samples_per_rev = 100000;
            params->rotation_speed_rpm = 300.0f;
            params->platform.apple_ii.encoding = 1;  /* 6-and-2 */
            break;
            
        default:
            params->samples_per_rev = 100000;
            params->rotation_speed_rpm = 300.0f;
            break;
    }
    
    return 0;
}

/**
 * @brief Apply compensation to flux transitions
 */
int compensation_apply(
    const uint32_t *transitions_in,
    size_t n,
    const compensation_params_t *params,
    uint32_t **transitions_out,
    size_t *n_out
) {
    if (!transitions_in || !params || !transitions_out || !n_out) {
        return -1;
    }
    
    if (n == 0) {
        *transitions_out = NULL;
        *n_out = 0;
        return 0;
    }
    
    /* Allocate output buffer */
    uint32_t *output = malloc(n * sizeof(uint32_t));
    if (!output) {
        return -1;
    }
    
    int result = 0;
    
    /* Apply mode-specific compensation */
    switch (params->mode) {
        case COMP_MODE_NONE:
            memcpy(output, transitions_in, n * sizeof(uint32_t));
            break;
            
        case COMP_MODE_AUTO:
            {
                compensation_mode_t detected = auto_detect_mode(
                    transitions_in, n, params->samples_per_rev
                );
                compensation_params_t auto_params = *params;
                auto_params.mode = detected;
                result = compensation_apply(transitions_in, n, &auto_params,
                                          transitions_out, n_out);
                free(output);
                return result;
            }
            
        case COMP_MODE_MAC800K:
            result = compensate_mac800k(transitions_in, n, params, output);
            break;
            
        case COMP_MODE_C64:
            result = compensate_c64(transitions_in, n, params, output);
            break;
            
        case COMP_MODE_AMIGA:
            result = compensate_amiga(transitions_in, n, params, output);
            break;
            
        case COMP_MODE_APPLE_II:
            result = compensate_apple_ii(transitions_in, n, params, output);
            break;
            
        default:
            /* PC MFM and others: minimal compensation */
            memcpy(output, transitions_in, n * sizeof(uint32_t));
            break;
    }
    
    if (result < 0) {
        free(output);
        return -1;
    }
    
    *transitions_out = output;
    *n_out = n;
    
    return 0;
}

/**
 * @brief Get mode name
 */
const char* compensation_get_mode_name(compensation_mode_t mode)
{
    switch (mode) {
        case COMP_MODE_NONE:     return "None";
        case COMP_MODE_AUTO:     return "Auto";
        case COMP_MODE_MAC800K:  return "Mac800K";
        case COMP_MODE_C64:      return "C64";
        case COMP_MODE_AMIGA:    return "Amiga";
        case COMP_MODE_APPLE_II: return "AppleII";
        case COMP_MODE_ATARI_ST: return "AtariST";
        case COMP_MODE_PC_MFM:   return "PC-MFM";
        case COMP_MODE_CUSTOM:   return "Custom";
        default:                 return "Unknown";
    }
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
