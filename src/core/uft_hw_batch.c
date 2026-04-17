/**
 * @file uft_hw_batch.c
 * @brief Generic batch-read dispatcher + fallback loop.
 *
 * Part of Phase H1 of the HAL unification. Anchors the decision:
 * sync-primary API, batch-optional for streaming backends.
 */
#include "uft/uft_hardware.h"
#include "uft/uft_hardware_internal.h"  /* for struct uft_hw_device */
#include "uft/uft_progress.h"
#include "uft/uft_error.h"
#include "uft/uft_format_plugin.h"      /* for uft_track_t */

/* Fallback for compilers without __builtin_popcount */
#ifndef __GNUC__
static inline int popcount8(uint8_t x) {
    int c = 0;
    while (x) { c += x & 1; x >>= 1; }
    return c;
}
#define BUILTIN_POPCOUNT(x) popcount8((uint8_t)(x))
#else
#define BUILTIN_POPCOUNT(x) __builtin_popcount(x)
#endif

uft_error_t uft_hw_read_tracks_batch_default(uft_hw_device_t *device,
                                                int cyl_start, int cyl_end,
                                                int head_mask,
                                                uft_track_t *tracks_out,
                                                uint8_t revolutions,
                                                uft_unified_progress_fn progress,
                                                void *user_data)
{
    if (!device || !device->backend || !device->backend->read_track)
        return UFT_ERROR_NOT_SUPPORTED;
    if (cyl_end <= cyl_start) return UFT_ERROR_INVALID_ARG;
    if (head_mask == 0) return UFT_ERROR_INVALID_ARG;
    if (!tracks_out) return UFT_ERROR_NULL_POINTER;

    int heads_per_cyl = BUILTIN_POPCOUNT(head_mask);
    int total = (cyl_end - cyl_start) * heads_per_cyl;
    int idx = 0;

    for (int cyl = cyl_start; cyl < cyl_end; cyl++) {
        for (int head = 0; head < 2; head++) {
            if (!(head_mask & (1 << head))) continue;

            /* Seek + select_head if backend supports them */
            if (device->backend->seek) {
                uft_error_t serr = device->backend->seek(device, (uint8_t)cyl);
                if (serr != UFT_OK) return serr;
            }
            if (device->backend->select_head) {
                device->backend->select_head(device, (uint8_t)head);
            }

            /* Per-track read */
            uft_error_t err = device->backend->read_track(device,
                &tracks_out[idx], revolutions);
            if (err != UFT_OK) return err;

            idx++;

            /* Progress + cancellation */
            if (progress) {
                uft_progress_t p;
                p.current      = (size_t)idx;
                p.total        = (size_t)total;
                p.stage        = "hw-batch";
                p.detail       = NULL;
                p.throughput   = 0.0;
                p.eta_seconds  = -1.0;
                p.extra        = NULL;
                if (!progress(&p, user_data))
                    return UFT_ERROR_CANCELLED;
            }
        }
    }
    return UFT_OK;
}

/* ============================================================================
 * Backend registry (minimal in-process list)
 * ============================================================================ */
#define UFT_HW_MAX_BACKENDS 16
static const uft_hw_backend_t *g_backends[UFT_HW_MAX_BACKENDS];
static size_t g_backend_count = 0;

uft_error_t uft_hw_register_backend(const uft_hw_backend_t *backend) {
    if (!backend) return UFT_ERROR_NULL_POINTER;
    if (g_backend_count >= UFT_HW_MAX_BACKENDS) return UFT_ERROR_NO_MEMORY;
    for (size_t i = 0; i < g_backend_count; i++)
        if (g_backends[i] == backend) return UFT_OK;  /* dup skip */
    g_backends[g_backend_count++] = backend;
    return UFT_OK;
}

const uft_hw_backend_t *uft_hw_get_backend(size_t index) {
    return index < g_backend_count ? g_backends[index] : NULL;
}

size_t uft_hw_backend_count(void) {
    return g_backend_count;
}

uft_error_t uft_hw_read_tracks(uft_hw_device_t *device,
                                 int cyl_start, int cyl_end,
                                 int head_mask,
                                 uft_track_t *tracks_out,
                                 uint8_t revolutions,
                                 uft_unified_progress_fn progress,
                                 void *user_data)
{
    if (!device || !device->backend) return UFT_ERROR_NULL_POINTER;

    /* Fast path: backend's native batch */
    if (device->backend->read_tracks_batch &&
        (device->backend->capabilities & UFT_HW_CAP_BATCH_READ)) {
        return device->backend->read_tracks_batch(device,
            cyl_start, cyl_end, head_mask, tracks_out,
            revolutions, progress, user_data);
    }

    /* Fallback: loop over read_track */
    return uft_hw_read_tracks_batch_default(device,
        cyl_start, cyl_end, head_mask, tracks_out,
        revolutions, progress, user_data);
}
