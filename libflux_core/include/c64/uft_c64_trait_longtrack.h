/*
 * ufm_c64_trait_longtrack.h
 *
 * Long-track / overlength track
 *
 * Scope (preservation):
 * - Detect likely *traits* in a C64/1541 disk capture (flux/nibble/bitstream metrics).
 * - Helps decide capture strategy (multi-rev, half-tracks, don't "repair" weak bits).
 *
 * Safety:
 * - No cracking, no bypass logic, no "how to defeat". Detection only.
 */
#ifndef UFM_C64_TRAIT_LONGTRACK_H
#define UFM_C64_TRAIT_LONGTRACK_H

#include <stddef.h>

#include "ufm_c64_scheme_detect.h" /* ufm_c64_track_sig_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Returns confidence 0..100. evidence is optional. */
int ufm_c64_detect_trait_longtrack(const ufm_c64_track_sig_t* tracks, size_t track_count,
                               char* evidence, size_t evidence_cap);

#ifdef __cplusplus
}
#endif

#endif /* UFM_C64_TRAIT_LONGTRACK_H */
