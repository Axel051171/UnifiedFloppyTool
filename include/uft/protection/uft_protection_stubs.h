/**
 * @file uft_protection_stubs.h
 * @brief Copy protection detection - previously stubbed implementations
 * 
 * P1-004: Complete the 8 stubbed protection detectors:
 * 1. Vorpal
 * 2. Rainbow Arts
 * 3. V-Max v3
 * 4. Rapidlok v2 full
 * 5. EA variant
 * 6. Epyx Fastload Plus
 * 7. Super Zaxxon
 * 8. Bounty Bob
 */

#ifndef UFT_PROTECTION_STUBS_H
#define UFT_PROTECTION_STUBS_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Protection detection result */
typedef struct {
    bool detected;
    uft_protection_t type;
    uint8_t confidence;
    uint8_t track_start;
    uint8_t track_end;
    const char *name;
    const char *description;
    
    /* Copy strategy hints */
    bool requires_flux;
    bool requires_timing;
    bool requires_weak_bits;
    bool requires_long_tracks;
    
    /* Additional data */
    uint8_t variant;
    uint32_t signature;
    
} protection_result_t;

/* ============================================================================
 * Protection Detectors
 * ============================================================================ */

/**
 * @brief Detect Vorpal protection (C64)
 * 
 * Vorpal uses non-standard GCR with extra sync marks and
 * half-track positioning.
 */
protection_result_t detect_vorpal(const uft_disk_image_t *disk);

/**
 * @brief Detect Rainbow Arts protection (C64)
 * 
 * Uses timing-based protection with precise gap lengths
 * and non-standard sector headers.
 */
protection_result_t detect_rainbow_arts(const uft_disk_image_t *disk);

/**
 * @brief Detect V-Max v3 protection (C64)
 * 
 * Enhanced version with:
 * - Long tracks on specific cylinders
 * - Custom sync patterns
 * - Density variations
 */
protection_result_t detect_vmax_v3(const uft_disk_image_t *disk);

/**
 * @brief Detect Rapidlok v2 full (C64)
 * 
 * Complete Rapidlok v2 detection including:
 * - Track 18 signature
 * - Density key
 * - Timing signature
 */
protection_result_t detect_rapidlok_v2_full(const uft_disk_image_t *disk);

/**
 * @brief Detect EA (Electronic Arts) variant (C64)
 * 
 * EA-specific protection variant with:
 * - Track 0 verification
 * - Custom boot loader
 * - Inter-sector timing
 */
protection_result_t detect_ea_variant(const uft_disk_image_t *disk);

/**
 * @brief Detect Epyx Fastload Plus (C64)
 * 
 * Uses:
 * - Non-standard directory track
 * - Custom fast loader
 * - Sector ID verification
 */
protection_result_t detect_epyx_fastload_plus(const uft_disk_image_t *disk);

/**
 * @brief Detect Super Zaxxon protection (C64)
 * 
 * Distinctive protection using:
 * - Half-track data
 * - Non-standard track lengths
 * - Timing verification
 */
protection_result_t detect_super_zaxxon(const uft_disk_image_t *disk);

/**
 * @brief Detect Bounty Bob Strikes Back protection (C64)
 * 
 * Features:
 * - Multi-track signature
 * - Custom sector numbering
 * - Timing-based verification
 */
protection_result_t detect_bounty_bob(const uft_disk_image_t *disk);

/* ============================================================================
 * Unified Detection API
 * ============================================================================ */

/**
 * @brief Run all protection detectors
 * @param disk Disk image to analyze
 * @param results Array to store results
 * @param max_results Maximum results to return
 * @return Number of protections detected
 */
int detect_all_protections(const uft_disk_image_t *disk,
                           protection_result_t *results,
                           size_t max_results);

/**
 * @brief Get protection name from type
 */
const char* protection_type_name(uft_protection_t type);

/**
 * @brief Get copy strategy for protection
 */
typedef struct {
    bool use_flux_copy;
    bool preserve_timing;
    bool preserve_weak_bits;
    bool use_multi_rev;
    int min_revisions;
    const char *notes;
} protection_copy_strategy_t;

protection_copy_strategy_t get_copy_strategy(uft_protection_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROTECTION_STUBS_H */
