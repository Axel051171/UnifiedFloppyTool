/**
 * @file uft_deepread_crosstrack.h
 * @brief DeepRead Cross-Track Correlation Analysis
 *
 * Computes normalized cross-correlation (NCC) between adjacent track
 * quality profiles to identify damage patterns that span multiple tracks.
 * Radial scratches, magnetic degradation, and circumferential wear each
 * produce distinctive correlation signatures across the disk surface.
 *
 * "Adjacent" heisst seit MF-1430: derselbe Kopf, Zylinder c und c+1 —
 * also Index t und t + num_heads in der Ablage von otdr_disk_create().
 * Vorher wurden t und t+1 verglichen, auf zweiseitigen Disketten also
 * stets die beiden OBERFLAECHEN (gemessen: Korrelation -1 statt +1 im
 * Fall tests/test_deepread_messung.c C1).
 *
 * Die Klassen (Radial/Magnetic/Circumferential) haengen an Schwellen
 * ohne Quelle (0,7 / 0,3 / 0,6) und sind an keiner echten Aufnahme
 * geeicht; das OTDR-Panel zeigt sie nur als "Heuristik". Die Spurbereiche
 * hinter `may_be_protection` (0-2, >= 36) sind unbelegt und zaehlen den
 * linearen Index, nicht den Zylinder — das Panel zeigt das Feld nicht
 * (P3-630).
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
    bool         may_be_protection;     /**< True if damage overlaps known protection track ranges */
    uint32_t     pair_count;            /**< Radially adjacent pairs actually measured
                                             (same head, cylinder c and c+1). 0 means
                                             mean_correlation is NOT a measurement. */
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
