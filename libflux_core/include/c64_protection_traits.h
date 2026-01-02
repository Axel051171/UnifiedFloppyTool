// SPDX-License-Identifier: MIT
/*
 * c64_protection_traits.h - Unified C64 Protection Traits
 * 
 * Comprehensive C64 protection trait detection system.
 * Includes all 10 trait detection modules.
 * 
 * @version 2.8.6.1
 * @date 2024-12-26
 */

#ifndef C64_PROTECTION_TRAITS_H
#define C64_PROTECTION_TRAITS_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * CORE TRAIT MODULES
 *============================================================================*/

/* Weak Bits Detection */
#include "c64/uft_c64_trait_weakbits.h"

/* Long Track Detection */
#include "c64/uft_c64_trait_longtrack.h"

/* Half-Track Detection */
#include "c64/uft_c64_trait_halftracks.h"

/* Long Sync Run Detection */
#include "c64/uft_c64_trait_long_sync_run.h"

/* Illegal GCR Detection */
#include "c64/uft_c64_trait_illegal_gcr.h"

/* Marker Bytes Detection */
#include "c64/uft_c64_trait_marker_bytes.h"

/* Bit Length Jitter Detection */
#include "c64/uft_c64_trait_bitlen_jitter.h"

/* Directory Track Anomaly Detection */
#include "c64/uft_c64_trait_dirtrack_anomaly.h"

/* Multi-Revolution Recommendation */
#include "c64/uft_c64_trait_multirev_recommended.h"

/* Speed Anomaly Detection */
#include "c64/uft_c64_trait_speed_anomaly.h"

/*============================================================================*
 * TRAIT INFORMATION
 *============================================================================*/

/**
 * @brief Get number of available trait detectors
 * @return Number of trait modules (10)
 */
static inline int c64_traits_count(void) {
    return 10;
}

/**
 * @brief Get trait detector names
 * @return Array of trait names
 */
static inline const char** c64_traits_names(void) {
    static const char* names[] = {
        "Weak Bits",
        "Long Track",
        "Half-Tracks",
        "Long Sync Run",
        "Illegal GCR",
        "Marker Bytes",
        "Bit Length Jitter",
        "Directory Track Anomaly",
        "Multi-Revolution Recommended",
        "Speed Anomaly"
    };
    return names;
}

#ifdef __cplusplus
}
#endif

#endif /* C64_PROTECTION_TRAITS_H */
