/**
 * @file ufm_cbm_protection_methods.h
 * @brief CBM (Commodore Business Machines) Protection Method Utilities
 */
#ifndef UFM_CBM_PROTECTION_METHODS_H
#define UFM_CBM_PROTECTION_METHODS_H

#include "ufm_c64_protection_taxonomy.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * These predicates describe STRUCTURE, not products. None of them identifies a
 * named copy-protection scheme, and none may be used to emit one — see
 * KNOWN_ISSUES PROT-5 for what happened when that line was crossed.
 *
 * REMOVED in MF-402:
 *   ufm_cbm_check_vmax()     — tautological (see the note in
 *                              src/protection/ufm_c64_scheme_detect.c); a
 *                              V-MAX claim belongs to c64_detect_vmax_version()
 *   ufm_cbm_check_rapidlok() — renamed to ufm_cbm_has_half_track_beyond_35();
 *                              the condition cannot support a RapidLok
 *                              identification, only the structural fact
 */

/** Data on a half-track outside the standard 35-track area */
bool ufm_cbm_has_half_track_beyond_35(const ufm_c64_track_metrics_t *m);

/** Check for custom sync patterns */
bool ufm_cbm_check_custom_sync(const ufm_c64_track_metrics_t *m);

/** Check for half-track usage */
bool ufm_cbm_check_half_track(const ufm_c64_track_metrics_t *m);

/** Check for extended track length */
bool ufm_cbm_check_long_track(const ufm_c64_track_metrics_t *m,
                               float threshold);

#ifdef __cplusplus
}
#endif
#endif
