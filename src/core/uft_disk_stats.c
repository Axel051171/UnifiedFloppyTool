/**
 * @file uft_disk_stats.c
 * @brief Disk health/quality metrics via uft_disk_stream.
 */
#include "uft/uft_disk_stats.h"
#include "uft/uft_disk_stream.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ensure_cap(uft_disk_stats_t *s) {
    if (s->tracks_count < s->tracks_capacity) return;
    size_t n = s->tracks_capacity ? s->tracks_capacity * 2 : 32;
    uft_track_stats_entry_t *p = realloc(s->tracks, n * sizeof(*p));
    if (!p) return;
    s->tracks = p;
    s->tracks_capacity = n;
}

typedef struct {
    uft_disk_stats_t *stats;
    const uft_stats_options_t *options;
} stats_ctx_t;

static uft_error_t stats_visitor(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track, void *user_data)
{
    (void)disk;
    stats_ctx_t *ctx = user_data;
    uft_disk_stats_t *s = ctx->stats;

    uft_track_stats_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.cylinder = cyl;
    entry.head = head;
    entry.sectors_total = track->sector_count;

    double conf_sum = 0.0;
    size_t conf_n = 0;

    for (size_t i = 0; i < track->sector_count; i++) {
        const uft_sector_t *sec = &track->sectors[i];
        size_t dlen = sec->data_len ? sec->data_len : sec->data_size;

        if (!sec->data || dlen == 0) {
            entry.sectors_missing++;
        } else if (!sec->crc_ok) {
            entry.sectors_crc_error++;
        } else {
            entry.sectors_ok++;
        }
        if (sec->weak) {
            entry.sectors_weak++;
            entry.has_weak_bits = true;
            s->has_weak_sectors = true;
        }
        if (sec->deleted) {
            entry.sectors_deleted++;
            s->has_deleted_sectors = true;
        }
        if (sec->confidence > 0) {
            conf_sum += sec->confidence;
            conf_n++;
        }
    }

    entry.average_confidence = conf_n > 0 ? (conf_sum / conf_n) : 0.0;
    entry.read_quality = track->sector_count > 0 ?
        ((double)entry.sectors_ok / (double)track->sector_count) : 0.0;

    s->total_tracks++;
    if (entry.sectors_ok > 0 || entry.sectors_total == 0) s->readable_tracks++;
    else s->unreadable_tracks++;

    s->total_sectors += entry.sectors_total;
    s->ok_sectors += entry.sectors_ok;
    s->failed_sectors += entry.sectors_crc_error + entry.sectors_missing;
    s->weak_sectors += entry.sectors_weak;
    s->deleted_sectors += entry.sectors_deleted;

    if (ctx->options && ctx->options->detailed) {
        ensure_cap(s);
        if (s->tracks_count < s->tracks_capacity)
            s->tracks[s->tracks_count++] = entry;
    }

    return UFT_OK;
}

uft_error_t uft_disk_collect_stats(uft_disk_t *disk,
                                     const uft_stats_options_t *options,
                                     uft_disk_stats_t *stats)
{
    if (!disk || !stats) return UFT_ERROR_NULL_POINTER;
    memset(stats, 0, sizeof(*stats));

    uft_stats_options_t dflt = UFT_STATS_OPTIONS_DEFAULT;
    const uft_stats_options_t *opts = options ? options : &dflt;

    stats_ctx_t ctx = { stats, opts };
    uft_stream_options_t sopts = UFT_STREAM_OPTIONS_DEFAULT;
    sopts.base = opts->base;
    sopts.base.continue_on_error = true;

    uft_error_t err = uft_disk_stream_tracks(disk, stats_visitor, &sopts, &ctx);

    /* Aggregate ratings */
    if (stats->total_sectors > 0) {
        stats->read_success_rate =
            (double)stats->ok_sectors / (double)stats->total_sectors;
    }
    if (stats->total_tracks > 0) {
        stats->disk_health =
            (double)stats->readable_tracks / (double)stats->total_tracks;
    }
    if (stats->ok_sectors > 0) {
        /* Grobe Näherung: vollständig-lesbare Sektoren geben 1.0 */
        stats->average_confidence = stats->read_success_rate;
    }

    /* Copy-protection heuristic */
    if (stats->has_weak_sectors || stats->has_deleted_sectors)
        stats->has_copy_protection = true;

    return err;
}

void uft_disk_stats_free(uft_disk_stats_t *stats) {
    if (!stats) return;
    free(stats->tracks);
    stats->tracks = NULL;
    stats->tracks_count = 0;
    stats->tracks_capacity = 0;
}

uft_error_t uft_disk_stats_to_text(const uft_disk_stats_t *stats,
                                     char *buffer, size_t buffer_size)
{
    if (!stats || !buffer) return UFT_ERROR_NULL_POINTER;
    int n = snprintf(buffer, buffer_size,
        "UFT Disk Stats:\n"
        "  Tracks: %zu total, %zu readable, %zu unreadable\n"
        "  Sectors: %zu total, %zu OK, %zu failed\n"
        "  Health: %.1f%%, Read Success: %.1f%%\n"
        "  Weak bits: %s, Deleted: %s, Copy Protection: %s\n",
        stats->total_tracks, stats->readable_tracks, stats->unreadable_tracks,
        stats->total_sectors, stats->ok_sectors, stats->failed_sectors,
        stats->disk_health * 100.0, stats->read_success_rate * 100.0,
        stats->has_weak_sectors ? "yes" : "no",
        stats->has_deleted_sectors ? "yes" : "no",
        stats->has_copy_protection ? "yes" : "no");
    return (n > 0 && (size_t)n < buffer_size) ? UFT_OK : UFT_ERROR_IO;
}

uft_error_t uft_disk_stats_to_json(const uft_disk_stats_t *stats,
                                     char *buffer, size_t buffer_size)
{
    if (!stats || !buffer) return UFT_ERROR_NULL_POINTER;
    int n = snprintf(buffer, buffer_size,
        "{\"tracks\":{\"total\":%zu,\"readable\":%zu,\"unreadable\":%zu},"
        "\"sectors\":{\"total\":%zu,\"ok\":%zu,\"failed\":%zu,"
        "\"weak\":%zu,\"deleted\":%zu},"
        "\"health\":%.3f,\"read_success_rate\":%.3f,"
        "\"has_weak_sectors\":%s,\"has_deleted_sectors\":%s,"
        "\"has_copy_protection\":%s}",
        stats->total_tracks, stats->readable_tracks, stats->unreadable_tracks,
        stats->total_sectors, stats->ok_sectors, stats->failed_sectors,
        stats->weak_sectors, stats->deleted_sectors,
        stats->disk_health, stats->read_success_rate,
        stats->has_weak_sectors ? "true" : "false",
        stats->has_deleted_sectors ? "true" : "false",
        stats->has_copy_protection ? "true" : "false");
    return (n > 0 && (size_t)n < buffer_size) ? UFT_OK : UFT_ERROR_IO;
}

int uft_disk_health_score(uft_disk_t *disk) {
    if (!disk) return 0;
    uft_disk_stats_t s;
    uft_stats_options_t opts = UFT_STATS_OPTIONS_DEFAULT;
    if (uft_disk_collect_stats(disk, &opts, &s) != UFT_OK) return 0;
    int score = (int)(s.disk_health * 100.0 + 0.5);
    uft_disk_stats_free(&s);
    return score;
}
