/**
 * @file uft_disk_convert.c
 * @brief Cross-format conversion via stream + plugin-write_track.
 */
#include "uft/uft_disk_convert.h"
#include "uft/uft_disk_stream.h"
#include "uft/uft_format_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uft_disk_t            *target;
    const uft_format_plugin_t *target_plugin;
    uft_convert_result_t  *result;
    const uft_disk_convert_options_t *options;
} convert_ctx_t;

static uft_error_t convert_visitor(uft_disk_t *source, int cyl, int head,
                                     uft_track_t *track, void *user_data)
{
    (void)source;
    convert_ctx_t *ctx = user_data;
    if (!ctx->target_plugin || !ctx->target_plugin->write_track) {
        ctx->result->tracks_failed++;
        return UFT_ERROR_NOT_SUPPORTED;
    }

    uft_error_t err = ctx->target_plugin->write_track(ctx->target, cyl, head,
                                                        track);
    if (err == UFT_OK) {
        ctx->result->tracks_converted++;
        ctx->result->sectors_copied += track->sector_count;
    } else {
        ctx->result->tracks_failed++;
        ctx->result->sectors_lost += track->sector_count;
    }

    /* Check for data loss markers in source track */
    for (size_t i = 0; i < track->sector_count; i++) {
        if (track->sectors[i].weak) ctx->result->weak_bits_lost = true;
        if (track->sectors[i].deleted) ctx->result->protection_lost = true;
    }

    return err;
}

uft_error_t uft_disk_convert_to_disk(uft_disk_t *source, uft_disk_t *target,
                                       const uft_disk_convert_options_t *options,
                                       uft_convert_result_t *result)
{
    if (!source || !target || !result) return UFT_ERROR_NULL_POINTER;
    if (target->read_only) return UFT_ERROR_NOT_SUPPORTED;

    memset(result, 0, sizeof(*result));
    result->mode_used = UFT_CONVERT_SECTOR;

    const uft_format_plugin_t *tplugin = uft_get_format_plugin(target->format);
    if (!tplugin || !tplugin->write_track) return UFT_ERROR_NOT_SUPPORTED;

    convert_ctx_t ctx = { target, tplugin, result, options };

    uft_disk_convert_options_t dflt = UFT_CONVERT_OPTIONS_DEFAULT;
    const uft_disk_convert_options_t *opts = options ? options : &dflt;

    uft_stream_options_t sopts = UFT_STREAM_OPTIONS_DEFAULT;
    sopts.base = opts->base;
    sopts.base.continue_on_error = true;

    uft_error_t err = uft_disk_stream_tracks(source, convert_visitor,
                                               &sopts, &ctx);

    /* Final error summary */
    if (result->tracks_failed > 0 && result->tracks_converted == 0)
        return UFT_ERROR_NOT_SUPPORTED;
    return err;
}

uft_error_t uft_disk_convert(uft_disk_t *source, const char *target_path,
                              uft_format_t target_format,
                              const uft_disk_convert_options_t *options,
                              uft_convert_result_t *result)
{
    if (!source || !target_path || !result) return UFT_ERROR_NULL_POINTER;

    /* Find plugin by format enum */
    const uft_format_plugin_t *tplugin = uft_get_format_plugin(target_format);
    if (!tplugin || !tplugin->open) return UFT_ERROR_NOT_SUPPORTED;

    /* Create empty target file (plugin-specific) */
    uft_disk_t *target = calloc(1, sizeof(uft_disk_t));
    if (!target) return UFT_ERROR_NO_MEMORY;

    target->format = target_format;
    target->read_only = false;
    target->geometry = source->geometry;

    /* Plugin open() — must accept r+b for write */
    uft_error_t err = tplugin->open(target, target_path, false);
    if (err != UFT_OK) {
        free(target);
        return err;
    }

    err = uft_disk_convert_to_disk(source, target, options, result);

    if (tplugin->close) tplugin->close(target);
    free(target);
    return err;
}

uft_error_t uft_disk_convert_check(uft_format_t source_format,
                                    uft_format_t target_format,
                                    uft_convert_mode_t *recommended_mode,
                                    bool *lossy_out)
{
    const uft_format_plugin_t *sp = uft_get_format_plugin(source_format);
    const uft_format_plugin_t *tp = uft_get_format_plugin(target_format);
    if (!sp || !tp) return UFT_ERROR_NOT_SUPPORTED;
    if (!sp->read_track || !tp->write_track) return UFT_ERROR_NOT_SUPPORTED;

    if (recommended_mode) *recommended_mode = UFT_CONVERT_SECTOR;

    /* Lossy check: source has flux/weak/timing, target doesn't */
    bool src_flux = (sp->capabilities & UFT_FORMAT_CAP_FLUX);
    bool tgt_flux = (tp->capabilities & UFT_FORMAT_CAP_FLUX);
    bool src_weak = (sp->capabilities & UFT_FORMAT_CAP_WEAK_BITS);
    bool tgt_weak = (tp->capabilities & UFT_FORMAT_CAP_WEAK_BITS);

    if (lossy_out)
        *lossy_out = (src_flux && !tgt_flux) || (src_weak && !tgt_weak);

    return UFT_OK;
}

uft_error_t uft_convert_result_to_text(const uft_convert_result_t *r,
                                         char *buf, size_t bsize)
{
    if (!r || !buf) return UFT_ERROR_NULL_POINTER;
    const char *mode_str = "AUTO";
    switch (r->mode_used) {
        case UFT_CONVERT_SECTOR:    mode_str = "SECTOR"; break;
        case UFT_CONVERT_BITSTREAM: mode_str = "BITSTREAM"; break;
        case UFT_CONVERT_FLUX:      mode_str = "FLUX"; break;
        default: break;
    }
    int n = snprintf(buf, bsize,
        "UFT Convert Result:\n"
        "  Mode used: %s\n"
        "  Tracks: %zu converted, %zu failed\n"
        "  Sectors: %zu copied, %zu lost\n"
        "  Data loss: weak=%s, timing=%s, protection=%s\n",
        mode_str,
        r->tracks_converted, r->tracks_failed,
        r->sectors_copied, r->sectors_lost,
        r->weak_bits_lost ? "yes" : "no",
        r->timing_lost ? "yes" : "no",
        r->protection_lost ? "yes" : "no");
    return (n > 0 && (size_t)n < bsize) ? UFT_OK : UFT_ERROR_IO;
}
