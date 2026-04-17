/**
 * @file uft_recovery_fusion.h
 * @brief Multi-source recovery fusion — best-of-N aggregation.
 *
 * Phase P3a of API consolidation. Provides the Multi-Source API that was
 * scattered across 4 different recovery-context types. Instead of breaking
 * the existing uft_recovery.h (retry config) and uft_disk_recovery.h
 * (safecopy-style passes), this header adds the missing piece: fusion
 * of multiple reads/flux-streams into a single best-result.
 *
 * Existing recovery headers kept as-is. New callers use this one.
 */
#ifndef UFT_RECOVERY_FUSION_H
#define UFT_RECOVERY_FUSION_H

#include "uft/uft_types.h"
#include "uft/uft_error.h"
#include "uft/uft_progress.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uft_recovery_fusion uft_recovery_fusion_t;

/* ============================================================================
 * Fusion strategies
 * ============================================================================ */

typedef enum {
    UFT_FUSE_MAJORITY,       /* Per-byte majority vote */
    UFT_FUSE_BEST_CRC,       /* Sector with valid CRC wins */
    UFT_FUSE_WEIGHTED,       /* Weighted average of sources */
    UFT_FUSE_UNION,          /* Anything readable, any source */
} uft_fusion_strategy_t;

/* ============================================================================
 * Session lifecycle
 * ============================================================================ */

/**
 * @brief Start a new recovery fusion session for a target disk.
 * @param target The disk to populate with the "best" data
 */
uft_recovery_fusion_t* uft_recovery_fusion_create(uft_disk_t *target);

/**
 * @brief Free the session. Does not free the target disk.
 */
void uft_recovery_fusion_free(uft_recovery_fusion_t *rec);

/* ============================================================================
 * Source enrollment
 * ============================================================================ */

/**
 * @brief Add an entire opened disk as a source.
 * @param weight 1-100, higher dominates in conflicts
 */
uft_error_t uft_recovery_fusion_add_disk(uft_recovery_fusion_t *rec,
                                           uft_disk_t *source, int weight);

/**
 * @brief Add flux data of a single revolution for (cyl, head).
 *        Accumulates across multiple revolutions per position.
 */
uft_error_t uft_recovery_fusion_add_revolution(uft_recovery_fusion_t *rec,
                                                 int cyl, int head,
                                                 const uint32_t *flux,
                                                 size_t flux_count, int weight);

/* ============================================================================
 * Fusion and results
 * ============================================================================ */

typedef struct {
    size_t    sources_total;
    size_t    tracks_fused;
    size_t    tracks_partial;
    size_t    tracks_failed;

    size_t    sectors_total;
    size_t    sectors_recovered;
    size_t    sectors_voted;
    size_t    sectors_uncertain;
    size_t    sectors_lost;

    double    confidence;
} uft_recovery_fusion_stats_t;

/**
 * @brief Run fusion using the chosen strategy.
 */
uft_error_t uft_recovery_fusion_run(uft_recovery_fusion_t *rec,
                                      uft_fusion_strategy_t strategy,
                                      uft_unified_progress_fn progress,
                                      void *user_data);

uft_error_t uft_recovery_fusion_get_stats(const uft_recovery_fusion_t *rec,
                                            uft_recovery_fusion_stats_t *stats);

uft_error_t uft_recovery_fusion_report_text(const uft_recovery_fusion_t *rec,
                                              char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif
#endif /* UFT_RECOVERY_FUSION_H */
