/**
 * @file uft_recovery_fusion.c
 * @brief Multi-source recovery fusion implementation.
 */
#include "uft/uft_recovery_fusion.h"
#include "uft/uft_format_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct fusion_source {
    uft_disk_t             *disk;
    int                     weight;
    struct fusion_source   *next;
} fusion_source_t;

struct uft_recovery_fusion {
    uft_disk_t                  *target;
    fusion_source_t             *sources;
    size_t                       source_count;
    uft_recovery_fusion_stats_t  stats;
};

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

uft_recovery_fusion_t* uft_recovery_fusion_create(uft_disk_t *target) {
    if (!target) return NULL;
    uft_recovery_fusion_t *rec = calloc(1, sizeof(*rec));
    if (!rec) return NULL;
    rec->target = target;
    return rec;
}

void uft_recovery_fusion_free(uft_recovery_fusion_t *rec) {
    if (!rec) return;
    fusion_source_t *s = rec->sources;
    while (s) {
        fusion_source_t *next = s->next;
        free(s);
        s = next;
    }
    free(rec);
}

/* ============================================================================
 * Source enrollment
 * ============================================================================ */

uft_error_t uft_recovery_fusion_add_disk(uft_recovery_fusion_t *rec,
                                           uft_disk_t *source, int weight)
{
    if (!rec || !source) return UFT_ERROR_NULL_POINTER;
    if (weight < 1) weight = 1;
    if (weight > 100) weight = 100;

    fusion_source_t *fs = calloc(1, sizeof(*fs));
    if (!fs) return UFT_ERROR_NO_MEMORY;
    fs->disk = source;
    fs->weight = weight;
    fs->next = rec->sources;
    rec->sources = fs;
    rec->source_count++;
    rec->stats.sources_total = rec->source_count;
    return UFT_OK;
}

uft_error_t uft_recovery_fusion_add_revolution(uft_recovery_fusion_t *rec,
                                                 int cyl, int head,
                                                 const uint32_t *flux,
                                                 size_t flux_count, int weight)
{
    /* Flux-level fusion requires PLL pipeline integration — out of scope
     * for this phase. Accept the call but mark as not-implemented. */
    (void)rec; (void)cyl; (void)head; (void)flux; (void)flux_count; (void)weight;
    return UFT_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * Fusion strategies
 * ============================================================================ */

/* Per-sector: pick the source whose sector has crc_ok=true. If multiple,
 * take the one with highest weight. */
static uft_error_t fuse_best_crc(uft_recovery_fusion_t *rec,
                                   uft_unified_progress_fn progress,
                                   void *user_data)
{
    const uft_format_plugin_t *tplugin =
        uft_get_format_plugin(rec->target->format);
    if (!tplugin || !tplugin->write_track) return UFT_ERROR_NOT_SUPPORTED;

    int cyls = rec->target->geometry.cylinders;
    int heads = rec->target->geometry.heads;
    size_t total = (size_t)cyls * (size_t)heads;
    size_t done = 0;

    for (int cyl = 0; cyl < cyls; cyl++) {
        for (int head = 0; head < heads; head++) {
            uft_track_t best = {0};
            int best_weight = -1;
            size_t best_ok_count = 0;

            for (fusion_source_t *s = rec->sources; s; s = s->next) {
                const uft_format_plugin_t *sp =
                    uft_get_format_plugin(s->disk->format);
                if (!sp || !sp->read_track) continue;

                uft_track_t tr;
                memset(&tr, 0, sizeof(tr));
                if (sp->read_track(s->disk, cyl, head, &tr) != UFT_OK) {
                    uft_track_cleanup(&tr);
                    continue;
                }
                /* Count OK sectors */
                size_t ok = 0;
                for (size_t i = 0; i < tr.sector_count; i++) {
                    if (tr.sectors[i].crc_ok) ok++;
                }
                /* Better than current best? */
                bool pick_this = false;
                if (ok > best_ok_count) pick_this = true;
                else if (ok == best_ok_count && s->weight > best_weight)
                    pick_this = true;

                if (pick_this) {
                    uft_track_cleanup(&best);
                    best = tr;  /* takeover ownership */
                    best_weight = s->weight;
                    best_ok_count = ok;
                    rec->stats.sectors_voted++;
                } else {
                    uft_track_cleanup(&tr);
                }
            }

            if (best_weight >= 0) {
                if (tplugin->write_track(rec->target, cyl, head, &best) == UFT_OK) {
                    rec->stats.tracks_fused++;
                    rec->stats.sectors_recovered += best.sector_count;
                } else {
                    rec->stats.tracks_failed++;
                }
                rec->stats.sectors_total += best.sector_count;
            } else {
                rec->stats.tracks_failed++;
            }
            uft_track_cleanup(&best);

            done++;
            if (progress) {
                uft_progress_t p = { done, total, "fusing", NULL,
                                       0.0, -1.0, NULL };
                if (!progress(&p, user_data)) return UFT_ERROR_CANCELLED;
            }
        }
    }

    if (rec->stats.sectors_total > 0) {
        rec->stats.confidence = (double)rec->stats.sectors_recovered /
                                  (double)rec->stats.sectors_total;
    }
    return UFT_OK;
}

uft_error_t uft_recovery_fusion_run(uft_recovery_fusion_t *rec,
                                      uft_fusion_strategy_t strategy,
                                      uft_unified_progress_fn progress,
                                      void *user_data)
{
    if (!rec || !rec->target) return UFT_ERROR_NULL_POINTER;
    if (rec->source_count == 0) return UFT_ERROR_INVALID_ARG;

    switch (strategy) {
        case UFT_FUSE_BEST_CRC:
        case UFT_FUSE_MAJORITY:
        case UFT_FUSE_WEIGHTED:
        case UFT_FUSE_UNION:
            /* All strategies map to best-crc for initial implementation;
             * finer-grained fusion algorithms deferred. */
            return fuse_best_crc(rec, progress, user_data);
    }
    return UFT_ERROR_INVALID_ARG;
}

uft_error_t uft_recovery_fusion_get_stats(const uft_recovery_fusion_t *rec,
                                            uft_recovery_fusion_stats_t *stats)
{
    if (!rec || !stats) return UFT_ERROR_NULL_POINTER;
    *stats = rec->stats;
    return UFT_OK;
}

uft_error_t uft_recovery_fusion_report_text(const uft_recovery_fusion_t *rec,
                                              char *buf, size_t bsize)
{
    if (!rec || !buf) return UFT_ERROR_NULL_POINTER;
    int n = snprintf(buf, bsize,
        "Recovery Fusion:\n"
        "  Sources:  %zu\n"
        "  Tracks:   %zu fused, %zu failed\n"
        "  Sectors:  %zu recovered of %zu, %zu voted\n"
        "  Confidence: %.1f%%\n",
        rec->stats.sources_total,
        rec->stats.tracks_fused, rec->stats.tracks_failed,
        rec->stats.sectors_recovered, rec->stats.sectors_total,
        rec->stats.sectors_voted,
        rec->stats.confidence * 100.0);
    return (n > 0 && (size_t)n < bsize) ? UFT_OK : UFT_ERROR_IO;
}
