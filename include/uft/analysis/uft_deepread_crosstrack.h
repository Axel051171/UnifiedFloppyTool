/**
 * @file uft_deepread_crosstrack.h
 * @brief DeepRead Cross-Track Correlation Analysis
 *
 * Computes normalized cross-correlation (NCC) between adjacent track
 * quality profiles to identify damage patterns that span multiple tracks.
 * Radial scratches, magnetic degradation, and circumferential wear each
 * produce distinctive correlation signatures across the disk surface.
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#ifndef UFT_DEEPREAD_CROSSTRACK_H
#define UFT_DEEPREAD_CROSSTRACK_H

#include <stdint.h>
#include <stdbool.h>
#include "floppy_otdr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * Types
 * =================================================================== */

/** Classification of damage pattern detected across tracks */
typedef enum {
    UFT_DAMAGE_NONE,        /**< No significant damage pattern */
    UFT_DAMAGE_MAGNETIC,    /**< Localized magnetic degradation (single track) */
    UFT_DAMAGE_RADIAL,      /**< Radial scratch/damage spanning multiple tracks */
    UFT_DAMAGE_CIRCUMFER,   /**< Circumferential wear (ring-shaped) */
    UFT_DAMAGE_MIXED        /**< Multiple damage types present */
} uft_damage_type_t;

/** Cross-track correlation analysis result */
typedef struct {
    float       *correlation_matrix;    /**< track_count x track_count NCC values */
    uint16_t     matrix_size;           /**< = track_count */
    float        mean_correlation;      /**< Mean NCC across adjacent pairs */
    uint32_t     radial_damage_count;   /**< Number of radial damage regions */
    uft_damage_type_t overall;          /**< Overall damage classification */
} uft_crosstrack_result_t;

/* ===================================================================
 * API Functions
 * =================================================================== */

/**
 * Analyze cross-track correlation on a full disk.
 *
 * Computes NCC between adjacent track quality profiles to classify
 * damage patterns. Requires that OTDR analysis has already been run
 * on all tracks (quality_profile must be populated).
 *
 * @param disk   Analyzed disk with quality profiles
 * @param result Output structure (caller allocates, internal arrays allocated here)
 * @return 0 on success, -1 on error
 */
int uft_deepread_crosstrack_analyze(const otdr_disk_t *disk,
                                    uft_crosstrack_result_t *result);

/**
 * Free internal allocations in a crosstrack result.
 * Does not free the result struct itself.
 */
void uft_crosstrack_result_free(uft_crosstrack_result_t *result);

/**
 * Get human-readable name for a damage type.
 */
const char *uft_damage_type_name(uft_damage_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DEEPREAD_CROSSTRACK_H */
