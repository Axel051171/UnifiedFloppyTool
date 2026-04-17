/**
 * @file uft_disk_verify.c
 * @brief Disk-level verify — built on uft_disk_stream_pair.
 */
#include "uft/uft_disk_verify.h"
#include "uft/uft_disk_stream.h"
#include "uft/uft_format_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Result helpers
 * ============================================================================ */

static void ensure_track_result_capacity(uft_verify_result_t *r) {
    if (r->track_results_count < r->track_results_capacity) return;
    size_t new_cap = r->track_results_capacity ? r->track_results_capacity * 2 : 32;
    uft_verify_track_result_t *p = realloc(r->track_results,
                                             new_cap * sizeof(*p));
    if (!p) return;
    r->track_results = p;
    r->track_results_capacity = new_cap;
}

static void append_track_result(uft_verify_result_t *r,
                                  const uft_verify_track_result_t *tr) {
    ensure_track_result_capacity(r);
    if (r->track_results_count >= r->track_results_capacity) return;
    r->track_results[r->track_results_count++] = *tr;
}

/* ============================================================================
 * Verify-Pair Visitor
 * ============================================================================ */

typedef struct {
    uft_verify_result_t       *result;
    const uft_verify_options_t *options;
} verify_ctx_t;

static uft_error_t verify_pair_visitor(uft_disk_t *a, uft_disk_t *b,
                                         int cyl, int head,
                                         uft_track_t *ta, uft_track_t *tb,
                                         void *user_data)
{
    (void)b;  /* b = reference, already read into tb */
    verify_ctx_t *ctx = user_data;
    const uft_format_plugin_t *plugin = uft_get_format_plugin(a->format);

    uft_verify_track_result_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.cylinder = cyl;
    tr.head = head;
    tr.sectors_expected = tb->sector_count;

    /* Plugin's eigener verify_track hat Priorität */
    uft_error_t v_err;
    if (plugin && plugin->verify_track) {
        v_err = plugin->verify_track(a, cyl, head, tb);
    } else {
        v_err = uft_generic_verify_track(a, cyl, head, tb);
    }

    /* Byte-level Sektor-Zählung via Sektoren-Vergleich */
    if (ta->sector_count == tb->sector_count) {
        for (size_t s = 0; s < tb->sector_count; s++) {
            const uft_sector_t *sa = &ta->sectors[s];
            const uft_sector_t *sb = &tb->sectors[s];
            size_t la = sa->data_len ? sa->data_len : sa->data_size;
            size_t lb = sb->data_len ? sb->data_len : sb->data_size;

            if (sa->weak && ctx->options && ctx->options->tolerate_weak) {
                tr.sectors_weak_skipped++;
                ctx->result->sectors_weak++;
            } else if (!sa->data || !sb->data) {
                tr.sectors_missing++;
            } else if (la != lb || memcmp(sa->data, sb->data, lb) != 0) {
                tr.sectors_failed++;
            } else {
                tr.sectors_matched++;
            }
        }
    } else {
        tr.sectors_missing = (tb->sector_count > ta->sector_count) ?
                              (tb->sector_count - ta->sector_count) : 0;
        tr.sectors_failed = (ta->sector_count > tb->sector_count) ?
                             (ta->sector_count - tb->sector_count) : 0;
    }

    tr.status = v_err;
    ctx->result->tracks_total++;
    ctx->result->sectors_total += tb->sector_count;
    ctx->result->sectors_ok += tr.sectors_matched;
    ctx->result->sectors_failed += tr.sectors_failed + tr.sectors_missing;

    if (v_err == UFT_OK) ctx->result->tracks_ok++;
    else ctx->result->tracks_failed++;

    if (ctx->options && ctx->options->detailed)
        append_track_result(ctx->result, &tr);

    /* Weiter iterieren auch bei Einzel-Fehler falls continue_on_error */
    if (v_err != UFT_OK &&
        (!ctx->options || !ctx->options->base.continue_on_error))
        return v_err;
    return UFT_OK;
}

/* ============================================================================
 * uft_disk_verify
 * ============================================================================ */

uft_error_t uft_disk_verify(uft_disk_t *disk, uft_disk_t *reference,
                             const uft_verify_options_t *options,
                             uft_verify_result_t *result)
{
    if (!disk || !reference || !result) return UFT_ERROR_NULL_POINTER;
    if (disk->geometry.cylinders != reference->geometry.cylinders ||
        disk->geometry.heads != reference->geometry.heads)
        return UFT_ERROR_GEOMETRY_MISMATCH;

    memset(result, 0, sizeof(*result));

    uft_verify_options_t dflt = UFT_VERIFY_OPTIONS_DEFAULT;
    const uft_verify_options_t *opts = options ? options : &dflt;

    verify_ctx_t ctx = { result, opts };

    uft_stream_options_t sopts = UFT_STREAM_OPTIONS_DEFAULT;
    sopts.base = opts->base;
    sopts.base.continue_on_error = true;  /* zähle alle Fehler */

    uft_error_t err = uft_disk_stream_pair(disk, reference,
                                             verify_pair_visitor, &sopts, &ctx);

    result->overall_status = (result->tracks_failed == 0) ?
                              UFT_OK : UFT_ERROR_VERIFY_FAILED;
    if (err != UFT_OK && err != UFT_ERROR_VERIFY_FAILED)
        return err;
    return result->overall_status;
}

/* ============================================================================
 * uft_disk_verify_self — read each track twice, compare
 * ============================================================================ */

typedef struct {
    uft_verify_result_t *result;
    const uft_verify_options_t *options;
} self_ctx_t;

static uft_error_t self_visitor(uft_disk_t *disk, int cyl, int head,
                                  uft_track_t *track1, void *user_data)
{
    self_ctx_t *ctx = user_data;
    const uft_format_plugin_t *plugin = uft_get_format_plugin(disk->format);
    if (!plugin || !plugin->read_track) return UFT_ERROR_NOT_SUPPORTED;

    uft_track_t track2;
    memset(&track2, 0, sizeof(track2));
    uft_error_t err = plugin->read_track(disk, cyl, head, &track2);

    uft_verify_track_result_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.cylinder = cyl;
    tr.head = head;
    tr.sectors_expected = track1->sector_count;

    if (err != UFT_OK) {
        tr.status = err;
        ctx->result->tracks_failed++;
        ctx->result->tracks_total++;
        if (ctx->options && ctx->options->detailed)
            append_track_result(ctx->result, &tr);
        uft_track_cleanup(&track2);
        return UFT_OK;
    }

    if (track1->sector_count != track2.sector_count) {
        tr.status = UFT_ERROR_VERIFY_FAILED;
        tr.sectors_missing = track1->sector_count > track2.sector_count ?
                              (track1->sector_count - track2.sector_count) : 0;
    } else {
        for (size_t s = 0; s < track1->sector_count; s++) {
            const uft_sector_t *s1 = &track1->sectors[s];
            const uft_sector_t *s2 = &track2.sectors[s];
            size_t l1 = s1->data_len ? s1->data_len : s1->data_size;
            size_t l2 = s2->data_len ? s2->data_len : s2->data_size;
            if (l1 != l2 || !s1->data || !s2->data ||
                memcmp(s1->data, s2->data, l1) != 0) {
                tr.sectors_failed++;
            } else {
                tr.sectors_matched++;
            }
        }
        tr.status = (tr.sectors_failed == 0) ? UFT_OK : UFT_ERROR_VERIFY_FAILED;
    }

    ctx->result->tracks_total++;
    ctx->result->sectors_total += track1->sector_count;
    ctx->result->sectors_ok += tr.sectors_matched;
    ctx->result->sectors_failed += tr.sectors_failed;
    if (tr.status == UFT_OK) ctx->result->tracks_ok++;
    else ctx->result->tracks_failed++;

    if (ctx->options && ctx->options->detailed)
        append_track_result(ctx->result, &tr);

    uft_track_cleanup(&track2);
    return UFT_OK;
}

uft_error_t uft_disk_verify_self(uft_disk_t *disk,
                                  const uft_verify_options_t *options,
                                  uft_verify_result_t *result)
{
    if (!disk || !result) return UFT_ERROR_NULL_POINTER;
    memset(result, 0, sizeof(*result));

    uft_verify_options_t dflt = UFT_VERIFY_OPTIONS_DEFAULT;
    const uft_verify_options_t *opts = options ? options : &dflt;
    self_ctx_t ctx = { result, opts };

    uft_stream_options_t sopts = UFT_STREAM_OPTIONS_DEFAULT;
    sopts.base = opts->base;
    sopts.base.continue_on_error = true;

    uft_error_t err = uft_disk_stream_tracks(disk, self_visitor, &sopts, &ctx);

    result->overall_status = (result->tracks_failed == 0) ?
                              UFT_OK : UFT_ERROR_VERIFY_FAILED;
    if (err != UFT_OK && err != UFT_ERROR_VERIFY_FAILED)
        return err;
    return result->overall_status;
}

/* ============================================================================
 * Result helpers
 * ============================================================================ */

void uft_verify_result_free(uft_verify_result_t *result) {
    if (!result) return;
    free(result->track_results);
    result->track_results = NULL;
    result->track_results_count = 0;
    result->track_results_capacity = 0;
}

uft_error_t uft_verify_result_to_text(const uft_verify_result_t *result,
                                        char *buffer, size_t buffer_size)
{
    if (!result || !buffer || buffer_size < 64) return UFT_ERROR_NULL_POINTER;
    int n = snprintf(buffer, buffer_size,
        "UFT Verify Result: %s\n"
        "  Tracks:  %zu total, %zu ok, %zu failed\n"
        "  Sectors: %zu total, %zu ok, %zu failed, %zu weak\n",
        result->overall_status == UFT_OK ? "PASS" : "FAIL",
        result->tracks_total, result->tracks_ok, result->tracks_failed,
        result->sectors_total, result->sectors_ok, result->sectors_failed,
        result->sectors_weak);
    return (n > 0 && (size_t)n < buffer_size) ? UFT_OK : UFT_ERROR_IO;
}

uft_error_t uft_verify_result_to_json(const uft_verify_result_t *result,
                                        char *buffer, size_t buffer_size)
{
    if (!result || !buffer || buffer_size < 64) return UFT_ERROR_NULL_POINTER;
    int n = snprintf(buffer, buffer_size,
        "{\"status\":\"%s\","
        "\"tracks\":{\"total\":%zu,\"ok\":%zu,\"failed\":%zu},"
        "\"sectors\":{\"total\":%zu,\"ok\":%zu,\"failed\":%zu,\"weak\":%zu}}",
        result->overall_status == UFT_OK ? "pass" : "fail",
        result->tracks_total, result->tracks_ok, result->tracks_failed,
        result->sectors_total, result->sectors_ok, result->sectors_failed,
        result->sectors_weak);
    return (n > 0 && (size_t)n < buffer_size) ? UFT_OK : UFT_ERROR_IO;
}
