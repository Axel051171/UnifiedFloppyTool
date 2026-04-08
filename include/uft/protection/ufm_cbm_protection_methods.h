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

/** Check if a track shows V-MAX! signatures */
bool ufm_cbm_check_vmax(const ufm_c64_track_metrics_t *m);

/** Check if a track shows RapidLok signatures */
bool ufm_cbm_check_rapidlok(const ufm_c64_track_metrics_t *m);

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
