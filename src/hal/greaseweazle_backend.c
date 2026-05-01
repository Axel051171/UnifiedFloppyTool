/**
 * @file greaseweazle_backend.c
 * @brief Greaseweazle uft_hw_backend_t Implementation (Phase H3).
 *
 * ARCHITEKTUR:
 *   Sync read_track/write_track:  Ja (pflicht)
 *   Batch read_tracks_batch:      Ja — Batch-Schleife über uft_gw_read_track,
 *                                  nutzt aber bereits effiziente Low-Level-API.
 *                                  Nativer Bulk-Transfer kann später ergänzt werden.
 *   Flux-Support:                 Ja (read_flux + write_flux)
 *   Capabilities:                 READ|WRITE|FLUX|BATCH|DENSITY
 *
 * DESIGN:
 *   Backend wrappt die existierende uft_gw_* API aus uft_greaseweazle_full.c.
 *   Kein eigenes Device-Handling — delegiert an gw_device_t*.
 *
 *   read_track liefert den raw flux als track->raw_data. Die Anwender-API
 *   (uft_disk_* / Format-Plugins) dekodiert die Sektoren via eigener Pipeline.
 */
#include "uft/uft_hardware.h"
#include "uft/uft_hardware_internal.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_progress.h"
#include "uft/hal/uft_greaseweazle_full.h"
#include <stdlib.h>
#include <string.h>

/* Forward declaration: referenced by constructor */
extern const uft_hw_backend_t uft_hw_backend_greaseweazle;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

static uft_error_t gw_backend_init(void) {
    /* libusb / serial init — per-device, not global for now */
    return UFT_OK;
}

static void gw_backend_shutdown(void) {
    /* no-op */
}

static uft_error_t gw_backend_enumerate(uft_hw_info_t *devices,
                                          size_t max_devices, size_t *found)
{
    if (!devices || !found) return UFT_ERROR_NULL_POINTER;
    *found = 0;

    char *ports[16];
    for (int i = 0; i < 16; i++) ports[i] = NULL;
    int n = uft_gw_list_ports(ports, 16);
    if (n < 0) return UFT_ERROR_IO;

    for (int i = 0; i < n && i < (int)max_devices; i++) {
        memset(&devices[i], 0, sizeof(devices[i]));
        devices[i].type = UFT_HW_GREASEWEAZLE;
        if (ports[i]) {
            strncpy(devices[i].usb_path, ports[i], sizeof(devices[i].usb_path) - 1);
            strncpy(devices[i].name, "Greaseweazle", sizeof(devices[i].name) - 1);
            free(ports[i]);
        }
        (*found)++;
    }
    return UFT_OK;
}

static uft_error_t gw_backend_open(const uft_hw_info_t *info,
                                     uft_hw_device_t **device)
{
    if (!info || !device) return UFT_ERROR_NULL_POINTER;

    uft_gw_device_t *gw = NULL;
    int gw_err = uft_gw_open(info->usb_path, &gw);
    if (gw_err != 0) {
        /* Distinguish bootloader-mode firmware from generic I/O failures so
         * the upper-layer Qt provider can show the user a recovery hint
         * instead of a useless "I/O error" toast (Facebook-bug class). */
        if (gw_err == UFT_GW_ERR_BOOTLOADER) return UFT_ERROR_INVALID_STATE;
        return UFT_ERROR_IO;
    }

    uft_hw_device_t *dev = calloc(1, sizeof(uft_hw_device_t));
    if (!dev) {
        uft_gw_close(gw);
        return UFT_ERROR_NO_MEMORY;
    }
    dev->backend = &uft_hw_backend_greaseweazle;
    dev->info = *info;
    dev->handle = gw;
    dev->motor_running = false;

    *device = dev;
    return UFT_OK;
}

static void gw_backend_close(uft_hw_device_t *device) {
    if (!device) return;
    if (device->handle) uft_gw_close((uft_gw_device_t *)device->handle);
    free(device);
}

/* ============================================================================
 * Drive control
 * ============================================================================ */

static uft_error_t gw_backend_motor(uft_hw_device_t *device, bool on) {
    if (!device || !device->handle) return UFT_ERROR_INVALID_STATE;
    if (uft_gw_set_motor((uft_gw_device_t *)device->handle, on) != 0)
        return UFT_ERROR_IO;
    device->motor_running = on;
    return UFT_OK;
}

static uft_error_t gw_backend_seek(uft_hw_device_t *device, uint8_t track) {
    if (!device || !device->handle) return UFT_ERROR_INVALID_STATE;
    if (uft_gw_seek((uft_gw_device_t *)device->handle, track) != 0)
        return UFT_ERROR_IO;
    device->current_track = track;
    return UFT_OK;
}

static uft_error_t gw_backend_select_head(uft_hw_device_t *device, uint8_t head) {
    if (!device || !device->handle) return UFT_ERROR_INVALID_STATE;
    if (uft_gw_select_head((uft_gw_device_t *)device->handle, head) != 0)
        return UFT_ERROR_IO;
    device->current_head = head;
    return UFT_OK;
}

/* ============================================================================
 * Track I/O — sync
 * ============================================================================ */

static uft_error_t gw_backend_read_track(uft_hw_device_t *device,
                                           uft_track_t *track, uint8_t revs)
{
    if (!device || !device->handle || !track) return UFT_ERROR_NULL_POINTER;

    uft_gw_flux_data_t *flux = NULL;
    if (uft_gw_read_track((uft_gw_device_t *)device->handle,
                            device->current_track, device->current_head,
                            revs, &flux) != 0 || !flux) {
        return UFT_ERROR_IO;
    }

    /* Init track + store raw flux. Format-level decoding happens higher up. */
    uft_track_init(track, device->current_track, device->current_head);

    if (flux->samples && flux->sample_count > 0) {
        size_t bytes = flux->sample_count * sizeof(uint32_t);
        track->raw_data = malloc(bytes);
        if (track->raw_data) {
            memcpy(track->raw_data, flux->samples, bytes);
            track->raw_size = bytes;
            track->raw_bits = (size_t)flux->sample_count * 32;
            track->owns_data = true;
        }
    }
    uft_gw_flux_free(flux);
    free(flux);
    return UFT_OK;
}

static uft_error_t gw_backend_write_track(uft_hw_device_t *device,
                                            const uft_track_t *track)
{
    if (!device || !device->handle || !track) return UFT_ERROR_NULL_POINTER;
    /* Write requires flux samples. If track has raw_data, use it; else fail. */
    if (!track->raw_data || track->raw_size == 0) return UFT_ERROR_NOT_SUPPORTED;

    const uint32_t *samples = (const uint32_t *)track->raw_data;
    uint32_t count = (uint32_t)(track->raw_size / sizeof(uint32_t));

    if (uft_gw_write_flux_simple((uft_gw_device_t *)device->handle,
                                    samples, count) != 0)
        return UFT_ERROR_IO;
    return UFT_OK;
}

/* ============================================================================
 * Batch read — native impl via loop of uft_gw_read_track
 *
 * Note: a true native bulk-transfer batch would require new GW firmware ops.
 * The current implementation matches the semantic contract (1 begin + N reads
 * without tear-down between) by keeping the GW session open — still better
 * than per-call open/close since the device handle stays warm.
 * ============================================================================ */

static uft_error_t gw_backend_read_tracks_batch(uft_hw_device_t *device,
                                                  int cyl_start, int cyl_end,
                                                  int head_mask,
                                                  uft_track_t *tracks_out,
                                                  uint8_t revolutions,
                                                  uft_unified_progress_fn progress,
                                                  void *user_data)
{
    if (!device || !tracks_out) return UFT_ERROR_NULL_POINTER;
    if (cyl_end <= cyl_start || head_mask == 0) return UFT_ERROR_INVALID_ARG;

    int heads_per_cyl = 0;
    if (head_mask & 0x1) heads_per_cyl++;
    if (head_mask & 0x2) heads_per_cyl++;
    int total = (cyl_end - cyl_start) * heads_per_cyl;
    int idx = 0;

    /* Ensure motor is running for the whole batch */
    bool motor_was_off = !device->motor_running;
    if (motor_was_off) gw_backend_motor(device, true);

    for (int cyl = cyl_start; cyl < cyl_end; cyl++) {
        for (int head = 0; head < 2; head++) {
            if (!(head_mask & (1 << head))) continue;

            /* Seek + head are handled inside uft_gw_read_track */
            device->current_track = (uint8_t)cyl;
            device->current_head = (uint8_t)head;

            uft_error_t err = gw_backend_read_track(device,
                &tracks_out[idx], revolutions);
            if (err != UFT_OK) {
                if (motor_was_off) gw_backend_motor(device, false);
                return err;
            }
            idx++;

            if (progress) {
                uft_progress_t p;
                p.current = (size_t)idx; p.total = (size_t)total;
                p.stage = "gw-batch"; p.detail = NULL;
                p.throughput = 0.0; p.eta_seconds = -1.0; p.extra = NULL;
                if (!progress(&p, user_data)) {
                    if (motor_was_off) gw_backend_motor(device, false);
                    return UFT_ERROR_CANCELLED;
                }
            }
        }
    }

    if (motor_was_off) gw_backend_motor(device, false);
    return UFT_OK;
}

/* ============================================================================
 * Flux I/O
 * ============================================================================ */

static uft_error_t gw_backend_read_flux(uft_hw_device_t *device, uint32_t *flux,
                                          size_t max_flux, size_t *flux_count,
                                          uint8_t revolutions)
{
    if (!device || !device->handle || !flux || !flux_count)
        return UFT_ERROR_NULL_POINTER;

    /* uft_gw_read_flux_simple returns flux via uft_gw_flux_data_t** — wrap it */
    uft_gw_flux_data_t *fd = NULL;
    if (uft_gw_read_flux_simple((uft_gw_device_t *)device->handle,
                                  revolutions, &fd) != 0 || !fd)
        return UFT_ERROR_IO;

    size_t count = (fd->sample_count < max_flux) ? fd->sample_count : max_flux;
    if (fd->samples && count > 0)
        memcpy(flux, fd->samples, count * sizeof(uint32_t));
    *flux_count = count;
    uft_gw_flux_free(fd);
    free(fd);
    return UFT_OK;
}

static uft_error_t gw_backend_write_flux(uft_hw_device_t *device,
                                           const uint32_t *flux, size_t flux_count)
{
    if (!device || !device->handle || !flux) return UFT_ERROR_NULL_POINTER;
    if (uft_gw_write_flux_simple((uft_gw_device_t *)device->handle,
                                    flux, (uint32_t)flux_count) != 0)
        return UFT_ERROR_IO;
    return UFT_OK;
}

/* ============================================================================
 * Backend struct + auto-registration
 * ============================================================================ */

const uft_hw_backend_t uft_hw_backend_greaseweazle = {
    .name         = "Greaseweazle",
    .type         = UFT_HW_GREASEWEAZLE,

    .init         = gw_backend_init,
    .shutdown     = gw_backend_shutdown,
    .enumerate    = gw_backend_enumerate,
    .open         = gw_backend_open,
    .close        = gw_backend_close,

    .motor        = gw_backend_motor,
    .seek         = gw_backend_seek,
    .select_head  = gw_backend_select_head,

    .read_track   = gw_backend_read_track,
    .write_track  = gw_backend_write_track,

    .read_flux    = gw_backend_read_flux,
    .write_flux   = gw_backend_write_flux,

    .read_tracks_batch = gw_backend_read_tracks_batch,

    .capabilities = UFT_HW_CAP_READ_TRACK | UFT_HW_CAP_WRITE_TRACK |
                    UFT_HW_CAP_READ_FLUX  | UFT_HW_CAP_WRITE_FLUX  |
                    UFT_HW_CAP_BATCH_READ | UFT_HW_CAP_DENSITY_SELECT,
};

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void gw_backend_register(void) {
    uft_hw_register_backend(&uft_hw_backend_greaseweazle);
}
#endif
