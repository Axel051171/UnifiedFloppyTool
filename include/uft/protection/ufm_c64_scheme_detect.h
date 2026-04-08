/**
 * @file ufm_c64_scheme_detect.h
 * @brief C64 Copy Protection Scheme Detection API
 */
#ifndef UFM_C64_SCHEME_DETECT_H
#define UFM_C64_SCHEME_DETECT_H

#include "ufm_c64_protection_taxonomy.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Analyze track metrics for copy protection signatures.
 *
 * @param metrics   Array of per-track metrics
 * @param n_tracks  Number of tracks
 * @param hits_out  Output array for detection hits
 * @param max_hits  Capacity of hits_out
 * @param report    Output summary report
 * @return true on success
 */
bool ufm_c64_prot_analyze(
    const ufm_c64_track_metrics_t *metrics,
    int n_tracks,
    ufm_c64_prot_hit_t *hits_out,
    int max_hits,
    ufm_c64_prot_report_t *report);

#ifdef __cplusplus
}
#endif
#endif
