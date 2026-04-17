/**
 * @file uft_disk_stream.c
 * @brief Generic track/sector iteration — foundation of Disk-API.
 *
 * Part of Master-API Phase 2. All other modules (verify, compare,
 * convert, stats) build on top of these iterators.
 */
#include "uft/uft_disk_stream.h"
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline const uft_format_plugin_t *disk_plugin(uft_disk_t *disk) {
    if (!disk) return NULL;
    return uft_get_format_plugin(disk->format);
}

/* Report progress; return false if user cancelled */
static bool progress_report(const uft_disk_op_options_t *opts,
                             size_t cur, size_t total, const char *msg) {
    if (!opts || !opts->on_progress) return true;
    return opts->on_progress(cur, total, msg, opts->user_data);
}

/* Report non-fatal error to callback (if any) */
static void error_report(const uft_disk_op_options_t *opts,
                          uft_error_t err, int cyl, int head,
                          const char *detail) {
    if (!opts || !opts->on_error) return;
    opts->on_error(err, cyl, head, detail, opts->user_data);
}

/* Iteration order: turn (step_idx) into (cyl, head) pair.
 * Returns false when step_idx is beyond the total. */
static bool step_to_ch(uft_iter_order_t order,
                        int cyl_start, int cyl_end, int heads,
                        size_t step, int *out_cyl, int *out_head)
{
    int range = cyl_end - cyl_start;
    if (range <= 0 || heads <= 0) return false;
    size_t total = (size_t)range * (size_t)heads;
    if (step >= total) return false;

    switch (order) {
        case UFT_ITER_BY_HEAD: {
            int head = (int)(step / range);
            int cyl  = cyl_start + (int)(step % range);
            *out_cyl = cyl; *out_head = head;
            return true;
        }
        case UFT_ITER_INTERLEAVED: {
            /* odd cyls first, then even. head cycles inside. */
            size_t odds  = (size_t)((range + 1) / 2);
            size_t evens = (size_t)(range / 2);
            size_t half_total = (odds * heads);
            if (step < half_total) {
                int idx = (int)(step / heads);
                int head = (int)(step % heads);
                /* Map idx to odd-index cylinders: 1, 3, 5... */
                *out_cyl = cyl_start + 1 + idx * 2;
                if (*out_cyl >= cyl_end) {
                    /* shouldn't happen if odds computed right */
                    return false;
                }
                *out_head = head;
                return true;
            } else {
                size_t s2 = step - half_total;
                int idx = (int)(s2 / heads);
                int head = (int)(s2 % heads);
                *out_cyl = cyl_start + idx * 2;
                (void)evens;
                if (*out_cyl >= cyl_end) return false;
                *out_head = head;
                return true;
            }
        }
        case UFT_ITER_LINEAR:
        default: {
            int cyl = cyl_start + (int)(step / heads);
            int head = (int)(step % heads);
            *out_cyl = cyl; *out_head = head;
            return true;
        }
    }
}

/* ============================================================================
 * uft_disk_stream_tracks
 * ============================================================================ */

uft_error_t uft_disk_stream_tracks(uft_disk_t *disk,
                                     uft_track_visitor_t visitor,
                                     const uft_stream_options_t *options,
                                     void *user_data)
{
    if (!disk || !visitor) return UFT_ERROR_NULL_POINTER;
    const uft_format_plugin_t *plugin = disk_plugin(disk);
    if (!plugin || !plugin->read_track) return UFT_ERROR_NOT_SUPPORTED;

    uft_stream_options_t dflt = UFT_STREAM_OPTIONS_DEFAULT;
    const uft_stream_options_t *opts = options ? options : &dflt;
    const uft_disk_op_options_t *base = &opts->base;

    int cyl_start, cyl_end;
    uft_disk_op_options_resolve_range(disk->geometry.cylinders, base,
                                        &cyl_start, &cyl_end);
    int heads = disk->geometry.heads > 0 ? disk->geometry.heads : 1;
    size_t total = (size_t)(cyl_end - cyl_start) * (size_t)heads;

    size_t visited = 0;
    uft_error_t last_err = UFT_OK;

    for (size_t step = 0; step < total; step++) {
        int cyl, head;
        if (!step_to_ch(opts->order, cyl_start, cyl_end, heads,
                         step, &cyl, &head)) continue;

        /* Filter */
        if (base->head_filter >= 0 && base->head_filter != head) continue;

        /* Read track */
        uft_track_t track;
        memset(&track, 0, sizeof(track));
        int retries = base->max_retries + 1;
        uft_error_t read_err = UFT_OK;
        while (retries-- > 0) {
            read_err = plugin->read_track(disk, cyl, head, &track);
            if (read_err == UFT_OK) break;
            uft_track_cleanup(&track);
            memset(&track, 0, sizeof(track));
        }

        if (read_err != UFT_OK) {
            error_report(base, read_err, cyl, head, "read_track failed");
            if (!base->continue_on_error) {
                uft_track_cleanup(&track);
                return read_err;
            }
            last_err = read_err;
            uft_track_cleanup(&track);
            continue;
        }

        /* Visit */
        uft_error_t v_err = visitor(disk, cyl, head, &track, user_data);

        /* Write back if modify_allowed and visitor returned OK */
        if (v_err == UFT_OK && opts->modify_allowed && !disk->read_only &&
            plugin->write_track) {
            uft_error_t w_err = plugin->write_track(disk, cyl, head, &track);
            if (w_err != UFT_OK) {
                error_report(base, w_err, cyl, head, "write_track failed");
                if (!base->continue_on_error) {
                    uft_track_cleanup(&track);
                    return w_err;
                }
                last_err = w_err;
            }
        }

        uft_track_cleanup(&track);

        /* Visitor wants to stop? */
        if (v_err == UFT_ERROR_CANCELLED) return UFT_OK;
        if (v_err != UFT_OK) {
            error_report(base, v_err, cyl, head, "visitor returned error");
            if (!base->continue_on_error) return v_err;
            last_err = v_err;
        }

        visited++;
        if (!progress_report(base, visited, total, NULL))
            return UFT_ERROR_CANCELLED;
    }

    return last_err;
}

/* ============================================================================
 * uft_disk_stream_sectors
 * ============================================================================ */

typedef struct {
    uft_sector_visitor_t  visitor;
    void                 *user_data;
    uft_error_t           last_err;
} sector_stream_ctx_t;

static uft_error_t sector_stream_adapter(uft_disk_t *disk, int cyl, int head,
                                           uft_track_t *track, void *ud)
{
    sector_stream_ctx_t *ctx = ud;
    if (!ctx || !ctx->visitor) return UFT_ERROR_NULL_POINTER;

    for (size_t i = 0; i < track->sector_count; i++) {
        uft_error_t err = ctx->visitor(disk, cyl, head,
                                        &track->sectors[i], ctx->user_data);
        if (err == UFT_ERROR_CANCELLED) return UFT_ERROR_CANCELLED;
        if (err != UFT_OK) {
            ctx->last_err = err;
            return err;
        }
    }
    return UFT_OK;
}

uft_error_t uft_disk_stream_sectors(uft_disk_t *disk,
                                      uft_sector_visitor_t visitor,
                                      const uft_stream_options_t *options,
                                      void *user_data)
{
    if (!disk || !visitor) return UFT_ERROR_NULL_POINTER;
    sector_stream_ctx_t ctx = { visitor, user_data, UFT_OK };
    uft_error_t err = uft_disk_stream_tracks(disk, sector_stream_adapter,
                                               options, &ctx);
    return (err != UFT_OK) ? err : ctx.last_err;
}

/* ============================================================================
 * uft_disk_stream_pair
 * ============================================================================ */

uft_error_t uft_disk_stream_pair(uft_disk_t *disk_a, uft_disk_t *disk_b,
                                   uft_pair_visitor_t visitor,
                                   const uft_stream_options_t *options,
                                   void *user_data)
{
    if (!disk_a || !disk_b || !visitor) return UFT_ERROR_NULL_POINTER;

    /* Geometrie-Kompat-Check */
    if (disk_a->geometry.cylinders != disk_b->geometry.cylinders ||
        disk_a->geometry.heads != disk_b->geometry.heads)
        return UFT_ERROR_GEOMETRY_MISMATCH;

    const uft_format_plugin_t *plugin_a = disk_plugin(disk_a);
    const uft_format_plugin_t *plugin_b = disk_plugin(disk_b);
    if (!plugin_a || !plugin_b) return UFT_ERROR_NOT_SUPPORTED;
    if (!plugin_a->read_track || !plugin_b->read_track)
        return UFT_ERROR_NOT_SUPPORTED;

    uft_stream_options_t dflt = UFT_STREAM_OPTIONS_DEFAULT;
    const uft_stream_options_t *opts = options ? options : &dflt;
    const uft_disk_op_options_t *base = &opts->base;

    int cyl_start, cyl_end;
    uft_disk_op_options_resolve_range(disk_a->geometry.cylinders, base,
                                        &cyl_start, &cyl_end);
    int heads = disk_a->geometry.heads > 0 ? disk_a->geometry.heads : 1;
    size_t total = (size_t)(cyl_end - cyl_start) * (size_t)heads;

    size_t visited = 0;
    uft_error_t last_err = UFT_OK;

    for (int cyl = cyl_start; cyl < cyl_end; cyl++) {
        for (int head = 0; head < heads; head++) {
            if (base->head_filter >= 0 && base->head_filter != head) continue;

            uft_track_t track_a, track_b;
            memset(&track_a, 0, sizeof(track_a));
            memset(&track_b, 0, sizeof(track_b));

            uft_error_t ea = plugin_a->read_track(disk_a, cyl, head, &track_a);
            uft_error_t eb = plugin_b->read_track(disk_b, cyl, head, &track_b);

            if (ea != UFT_OK || eb != UFT_OK) {
                error_report(base, ea != UFT_OK ? ea : eb, cyl, head,
                              "pair read failed");
                if (!base->continue_on_error) {
                    uft_track_cleanup(&track_a);
                    uft_track_cleanup(&track_b);
                    return ea != UFT_OK ? ea : eb;
                }
                last_err = ea != UFT_OK ? ea : eb;
                uft_track_cleanup(&track_a);
                uft_track_cleanup(&track_b);
                continue;
            }

            uft_error_t v_err = visitor(disk_a, disk_b, cyl, head,
                                          &track_a, &track_b, user_data);

            uft_track_cleanup(&track_a);
            uft_track_cleanup(&track_b);

            if (v_err == UFT_ERROR_CANCELLED) return UFT_OK;
            if (v_err != UFT_OK) {
                error_report(base, v_err, cyl, head, "pair visitor error");
                if (!base->continue_on_error) return v_err;
                last_err = v_err;
            }

            visited++;
            if (!progress_report(base, visited, total, NULL))
                return UFT_ERROR_CANCELLED;
        }
    }

    return last_err;
}
