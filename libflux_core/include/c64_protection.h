// SPDX-License-Identifier: MIT
/*
 * c64_protection.h - Unified C64 Protection Detection API
 * 
 * High-level API that combines all C64 protection detection modules
 * into a single, easy-to-use interface.
 * 
 * @version 2.8.3
 * @date 2024-12-26
 */

#ifndef C64_PROTECTION_H
#define C64_PROTECTION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "c64/uft_cbm_protection_methods.h"
#include "c64/uft_c64_scheme_detect.h"
#include "c64/uft_c64_rapidlok.h"
#include "c64/uft_c64_rapidlok2_detect.h"
#include "c64/uft_c64_rapidlok6.h"
#include "c64/uft_c64_geos_gap_protection.h"
#include "c64/uft_c64_ealoader.h"
#include "c64/uft_c64_protection_taxonomy.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * UNIFIED PROTECTION DETECTION
 *============================================================================*/

/**
 * @brief Complete C64 protection analysis result
 */
typedef struct {
    /* Overall confidence (0-100) */
    int overall_confidence;
    
    /* Generic protection methods detected */
    uft_cbm_report_t cbm_methods;
    uft_cbm_method_hit_t cbm_hits[16];
    size_t cbm_hits_count;
    
    /* Named schemes detected */
    uft_c64_scheme_report_t schemes;
    
    /* Specific detectors */
    bool rapidlok_detected;
    int rapidlok_confidence;
    
    bool rapidlok2_detected;
    int rapidlok2_confidence;
    
    bool rapidlok6_detected;
    int rapidlok6_confidence;
    
    bool geos_gap_detected;
    int geos_confidence;
    
    bool ea_loader_detected;
    int ea_confidence;
    
    /* Human-readable summary */
    char summary[2048];
    
    /* Preservation recommendations */
    char recommendations[1024];
} c64_protection_result_t;

/**
 * @brief Analyze C64 disk for copy protection
 * 
 * This is the main entry point for C64 protection detection.
 * It runs all available detectors and combines the results.
 * 
 * @param track_metrics Per-track metrics (from your decoder)
 * @param track_count Number of tracks
 * @param result Output result structure
 * @return true on success
 * 
 * Example:
 *   c64_protection_result_t result;
 *   if (c64_analyze_protection(tracks, 42, &result)) {
 *       printf("Confidence: %d%%\n", result.overall_confidence);
 *       printf("Summary: %s\n", result.summary);
 *   }
 */
bool c64_analyze_protection(
    const uft_cbm_track_metrics_t* track_metrics,
    size_t track_count,
    c64_protection_result_t* result
);

/**
 * @brief Quick check: is any protection present?
 * 
 * @param track_metrics Per-track metrics
 * @param track_count Number of tracks
 * @return true if protection detected (confidence >= 50%)
 */
bool c64_has_protection(
    const uft_cbm_track_metrics_t* track_metrics,
    size_t track_count
);

/**
 * @brief Print protection analysis to stdout
 * 
 * @param result Protection analysis result
 * @param verbose Print detailed information
 */
void c64_print_protection(
    const c64_protection_result_t* result,
    bool verbose
);

#ifdef __cplusplus
}
#endif

#endif /* C64_PROTECTION_H */
