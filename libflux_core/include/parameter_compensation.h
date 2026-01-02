// SPDX-License-Identifier: MIT
/*
 * parameter_compensation.h - Universal Parameter Compensation System Header
 * 
 * @version 2.8.0
 * @date 2024-12-26
 */

#ifndef PARAMETER_COMPENSATION_H
#define PARAMETER_COMPENSATION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

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
    float peak_shift_threshold;
    float peak_shift_strength;
    
    /* Write precompensation */
    float write_precomp_early;
    float write_precomp_late;
    
    /* Track density */
    float track_density_factor;
    int physical_track;
    
    /* Timing */
    uint32_t samples_per_rev;
    float rotation_speed_rpm;
    
    /* Platform-specific */
    union {
        struct {
            int zone;
            bool gcr_mode;
        } c64;
        
        struct {
            bool high_density;
            int sectors_per_track;
        } amiga;
        
        struct {
            int encoding;
        } apple_ii;
    } platform;
    
} compensation_params_t;

/*============================================================================*
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Initialize compensation parameters
 * 
 * @param mode Compensation mode
 * @param params Parameters structure (output)
 * @return 0 on success
 */
int compensation_init_params(
    compensation_mode_t mode,
    compensation_params_t *params
);

/**
 * @brief Apply compensation to flux transitions
 * 
 * @param transitions_in Input transitions
 * @param n Number of transitions
 * @param params Compensation parameters
 * @param transitions_out Output transitions (allocated by function)
 * @param n_out Number of output transitions
 * @return 0 on success
 */
int compensation_apply(
    const uint32_t *transitions_in,
    size_t n,
    const compensation_params_t *params,
    uint32_t **transitions_out,
    size_t *n_out
);

/**
 * @brief Get mode name
 * 
 * @param mode Compensation mode
 * @return Mode name string
 */
const char* compensation_get_mode_name(compensation_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* PARAMETER_COMPENSATION_H */
