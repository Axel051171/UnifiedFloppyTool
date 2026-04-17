/**
 * @file uft_capture.c
 * @brief Capture / restore implementations — Phase P2b.
 */
#include "uft/uft_capture.h"
#include "uft/uft_hardware_internal.h"  /* struct uft_hw_device */

#include <stdlib.h>
#include <string.h>
#include <time.h>

static int popcount_nibble(int m) {
    int c = 0; while (m) { c += m & 1; m >>= 1; } return c;
}

static double now_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

uft_error_t uft_capture_disk(uft_hw_device_t *device,
                              const uft_capture_options_t *opts,
                              uft_capture_result_t *out)
{
    if (!device || !opts || !out) return UFT_ERROR_NULL_POINTER;
    if (opts->cyl_end <= opts->cyl_start) return UFT_ERROR_INVALID_ARG;
    if (opts->head_mask == 0) return UFT_ERROR_INVALID_ARG;

    memset(out, 0, sizeof(*out));

    const int heads = popcount_nibble(opts->head_mask);
    const size_t total = (size_t)(opts->cyl_end - opts->cyl_start) * (size_t)heads;
    if (total == 0) return UFT_ERROR_INVALID_ARG;

    uft_disk_t *disk = (uft_disk_t *)calloc(1, sizeof(uft_disk_t));
    uft_track_t *track_buf = (uft_track_t *)calloc(total, sizeof(uft_track_t));
    uft_track_t **track_ptrs = (uft_track_t **)calloc(total, sizeof(uft_track_t *));
    if (!disk || !track_buf || !track_ptrs) {
        free(disk); free(track_buf); free(track_ptrs);
        return UFT_ERROR_NO_MEMORY;
    }

    const double t0 = now_seconds();
    uft_error_t err = uft_hw_read_tracks(device,
                                         opts->cyl_start, opts->cyl_end,
                                         opts->head_mask,
                                         track_buf,
                                         opts->revolutions,
                                         opts->progress,
                                         opts->user_data);
    const double t1 = now_seconds();

    /* Populate disk. Note: failures inside uft_hw_read_tracks may leave
     * some entries zero-initialised — the caller sees tracks_failed > 0. */
    for (size_t i = 0; i < total; i++) {
        track_ptrs[i] = &track_buf[i];
        if (track_buf[i].error == 0 && track_buf[i].cylinder != -1)
            out->tracks_succeeded++;
        else
            out->tracks_failed++;
        out->total_bytes += (uint64_t)track_buf[i].raw_size;
    }

    disk->tracks       = track_ptrs;
    disk->track_count  = (int)total;
    disk->is_open      = true;
    disk->read_only    = true;
    disk->plugin_data  = track_buf;   /* store base ptr so free() can undo */

    out->disk              = disk;
    out->tracks_attempted  = total;
    out->elapsed_seconds   = t1 - t0;
    return err;
}

uft_error_t uft_restore_disk(uft_hw_device_t *device,
                              const uft_disk_t *disk,
                              const uft_capture_options_t *opts,
                              uft_capture_result_t *out)
{
    if (!device || !device->backend || !device->backend->write_track)
        return UFT_ERROR_NOT_SUPPORTED;
    if (!disk || !disk->tracks || !opts) return UFT_ERROR_NULL_POINTER;

    if (out) memset(out, 0, sizeof(*out));
    const double t0 = now_seconds();
    uft_error_t first_err = UFT_OK;
    size_t attempted = 0, failed = 0;

    for (int i = 0; i < disk->track_count; i++) {
        const uft_track_t *t = disk->tracks[i];
        if (!t) continue;
        if (t->cylinder < opts->cyl_start || t->cylinder >= opts->cyl_end) continue;
        if (!(opts->head_mask & (1 << t->head))) continue;

        if (device->backend->seek) {
            uft_error_t se = device->backend->seek(device, (uint8_t)t->cylinder);
            if (se != UFT_OK) { if (first_err == UFT_OK) first_err = se; failed++; continue; }
        }
        if (device->backend->select_head)
            device->backend->select_head(device, (uint8_t)t->head);

        attempted++;
        uft_error_t we = device->backend->write_track(device, t);
        if (we != UFT_OK) {
            if (first_err == UFT_OK) first_err = we;
            failed++;
        }

        if (opts->progress) {
            uft_progress_t p = {0};
            p.current = (size_t)(i + 1);
            p.total   = (size_t)disk->track_count;
            p.stage   = "restore";
            p.eta_seconds = -1.0;
            if (!opts->progress(&p, opts->user_data)) {
                if (first_err == UFT_OK) first_err = UFT_ERROR_CANCELLED;
                break;
            }
        }
    }

    if (out) {
        out->tracks_attempted = attempted;
        out->tracks_failed    = failed;
        out->tracks_succeeded = attempted - failed;
        out->elapsed_seconds  = now_seconds() - t0;
    }
    return first_err;
}

void uft_capture_free(uft_capture_result_t *result)
{
    if (!result || !result->disk) return;
    uft_disk_t *d = result->disk;
    /* We stored the contiguous uft_track_t buffer in plugin_data. */
    free(d->plugin_data);
    free(d->tracks);
    free(d);
    result->disk = NULL;
}
