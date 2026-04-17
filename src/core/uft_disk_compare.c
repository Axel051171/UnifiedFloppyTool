/**
 * @file uft_disk_compare.c
 * @brief Cross-format disk diff — built on uft_disk_stream_pair.
 */
#include "uft/uft_disk_compare.h"
#include "uft/uft_disk_stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ensure_diff_cap(uft_disk_compare_result_t *r) {
    if (r->diff_count < r->diff_capacity) return;
    size_t n = r->diff_capacity ? r->diff_capacity * 2 : 64;
    uft_diff_entry_t *p = realloc(r->diffs, n * sizeof(*p));
    if (!p) return;
    r->diffs = p;
    r->diff_capacity = n;
}

typedef struct {
    uft_disk_compare_result_t *result;
    const uft_compare_options_t *options;
    bool stop;
} compare_ctx_t;

static void add_diff(compare_ctx_t *ctx, const uft_diff_entry_t *d,
                       const uint8_t *data_a, const uint8_t *data_b, size_t len)
{
    uft_disk_compare_result_t *r = ctx->result;
    if (ctx->options->max_diffs > 0 && r->diff_count >= ctx->options->max_diffs)
        return;
    ensure_diff_cap(r);
    if (r->diff_count >= r->diff_capacity) return;

    uft_diff_entry_t *slot = &r->diffs[r->diff_count++];
    *slot = *d;
    slot->data_a = NULL;
    slot->data_b = NULL;
    slot->data_len = 0;

    if (ctx->options->include_data && len > 0) {
        slot->data_len = len;
        if (data_a) { slot->data_a = malloc(len); if (slot->data_a) memcpy(slot->data_a, data_a, len); }
        if (data_b) { slot->data_b = malloc(len); if (slot->data_b) memcpy(slot->data_b, data_b, len); }
    }
}

static uft_error_t compare_pair_visitor(uft_disk_t *a, uft_disk_t *b,
                                          int cyl, int head,
                                          uft_track_t *ta, uft_track_t *tb,
                                          void *user_data)
{
    (void)a; (void)b;
    compare_ctx_t *ctx = user_data;
    uft_disk_compare_result_t *r = ctx->result;
    r->total_tracks_compared++;

    size_t max_sec = ta->sector_count > tb->sector_count ?
                      ta->sector_count : tb->sector_count;

    for (size_t s = 0; s < max_sec; s++) {
        uft_diff_entry_t d;
        memset(&d, 0, sizeof(d));
        d.cylinder = cyl;
        d.head = head;
        d.sector = (uint8_t)s;

        r->total_sectors_compared++;

        if (s >= ta->sector_count) {
            d.type = UFT_DIFF_MISSING_IN_A;
            r->different_sectors++;
            add_diff(ctx, &d, NULL,
                     tb->sectors[s].data,
                     tb->sectors[s].data_len ? tb->sectors[s].data_len :
                                                 tb->sectors[s].data_size);
            continue;
        }
        if (s >= tb->sector_count) {
            d.type = UFT_DIFF_MISSING_IN_B;
            r->different_sectors++;
            add_diff(ctx, &d, ta->sectors[s].data, NULL,
                     ta->sectors[s].data_len ? ta->sectors[s].data_len :
                                                 ta->sectors[s].data_size);
            continue;
        }

        const uft_sector_t *sa = &ta->sectors[s];
        const uft_sector_t *sb = &tb->sectors[s];
        size_t la = sa->data_len ? sa->data_len : sa->data_size;
        size_t lb = sb->data_len ? sb->data_len : sb->data_size;

        if (la != lb) {
            d.type = UFT_DIFF_SIZE;
            d.byte_count = la > lb ? (la - lb) : (lb - la);
            r->different_sectors++;
            add_diff(ctx, &d, sa->data, sb->data, la < lb ? la : lb);
            continue;
        }

        if (!sa->data || !sb->data) {
            if (sa->data != sb->data) {
                d.type = UFT_DIFF_MISSING_IN_A;  /* one side NULL */
                r->different_sectors++;
                add_diff(ctx, &d, sa->data, sb->data, 0);
            } else {
                r->identical_sectors++;
            }
            continue;
        }

        if (memcmp(sa->data, sb->data, la) != 0) {
            /* Finde ersten unterschiedlichen Byte + Anzahl */
            size_t off = 0, cnt = 0;
            bool found = false;
            for (size_t b2 = 0; b2 < la; b2++) {
                if (sa->data[b2] != sb->data[b2]) {
                    if (!found) { off = b2; found = true; }
                    cnt++;
                }
            }
            d.type = UFT_DIFF_DATA;
            d.byte_offset = off;
            d.byte_count = cnt;
            r->different_sectors++;
            add_diff(ctx, &d, sa->data, sb->data, la);

            if (ctx->options->stop_after_first_diff) {
                ctx->stop = true;
                return UFT_ERROR_CANCELLED;
            }
        } else if (ctx->options->compare_status &&
                    (sa->crc_ok != sb->crc_ok || sa->weak != sb->weak ||
                     sa->deleted != sb->deleted)) {
            d.type = UFT_DIFF_STATUS;
            r->different_sectors++;
            add_diff(ctx, &d, NULL, NULL, 0);
        } else {
            r->identical_sectors++;
        }
    }

    return UFT_OK;
}

uft_error_t uft_disk_compare(uft_disk_t *a, uft_disk_t *b,
                              const uft_compare_options_t *options,
                              uft_disk_compare_result_t *result)
{
    if (!a || !b || !result) return UFT_ERROR_NULL_POINTER;
    if (a->geometry.cylinders != b->geometry.cylinders ||
        a->geometry.heads != b->geometry.heads)
        return UFT_ERROR_GEOMETRY_MISMATCH;

    memset(result, 0, sizeof(*result));

    uft_compare_options_t dflt = UFT_COMPARE_OPTIONS_DEFAULT;
    const uft_compare_options_t *opts = options ? options : &dflt;

    compare_ctx_t ctx = { result, opts, false };

    uft_stream_options_t sopts = UFT_STREAM_OPTIONS_DEFAULT;
    sopts.base = opts->base;
    sopts.base.continue_on_error = true;

    uft_error_t err = uft_disk_stream_pair(a, b, compare_pair_visitor,
                                             &sopts, &ctx);

    if (result->total_sectors_compared > 0) {
        result->similarity = (double)result->identical_sectors /
                              (double)result->total_sectors_compared;
    }

    if (err == UFT_ERROR_CANCELLED && ctx.stop) return UFT_OK;
    return err;
}

void uft_compare_result_free(uft_disk_compare_result_t *result) {
    if (!result) return;
    for (size_t i = 0; i < result->diff_count; i++) {
        free(result->diffs[i].data_a);
        free(result->diffs[i].data_b);
    }
    free(result->diffs);
    result->diffs = NULL;
    result->diff_count = 0;
    result->diff_capacity = 0;
}

uft_error_t uft_disk_compare_result_to_text(const uft_disk_compare_result_t *r,
                                         char *buf, size_t bsize)
{
    if (!r || !buf) return UFT_ERROR_NULL_POINTER;
    int n = snprintf(buf, bsize,
        "UFT Compare Result:\n"
        "  Tracks compared: %zu\n"
        "  Sectors:        %zu total, %zu identical, %zu different\n"
        "  Similarity:     %.1f%%\n"
        "  Diff entries:   %zu\n",
        r->total_tracks_compared,
        r->total_sectors_compared, r->identical_sectors, r->different_sectors,
        r->similarity * 100.0, r->diff_count);
    return (n > 0 && (size_t)n < bsize) ? UFT_OK : UFT_ERROR_IO;
}

uft_error_t uft_disk_compare_result_to_json(const uft_disk_compare_result_t *r,
                                         char *buf, size_t bsize)
{
    if (!r || !buf) return UFT_ERROR_NULL_POINTER;
    int n = snprintf(buf, bsize,
        "{\"tracks_compared\":%zu,"
        "\"sectors\":{\"total\":%zu,\"identical\":%zu,\"different\":%zu},"
        "\"similarity\":%.3f,\"diff_count\":%zu}",
        r->total_tracks_compared,
        r->total_sectors_compared, r->identical_sectors, r->different_sectors,
        r->similarity, r->diff_count);
    return (n > 0 && (size_t)n < bsize) ? UFT_OK : UFT_ERROR_IO;
}
