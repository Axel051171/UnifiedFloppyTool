/**
 * @file sync_backends.c
 * @brief Sync-only HAL backends (Phase H5) — minimal registration stubs.
 *
 * These four backends share:
 *   - No native batch support (would just be a loop — use default fallback)
 *   - Sync read_track/write_track delegating to existing low-level modules
 *
 * ARCHITECTURE DECISION per backend:
 *
 *   FC5025:     USB-Controller is track-by-track by design. Batch adds nothing.
 *   SuperCard Pro: SCP firmware wants Start/Stop per track, no streaming mode.
 *   XUM1541:    CBM protocol is sector-based, not track-based — "batch" doesn't
 *               semantically apply.
 *   Linux FDC:  ioctl FDRAWCMD is per-track; no kernel-level batching exists.
 *
 * Current status: skeleton structs registered, but actual read_track/
 * write_track hooks are NULL because the project's low-level modules
 * for these backends are incomplete. The structures exist so enumerate()
 * lists them and capability queries return sensible answers.
 *
 * Full implementations will be added as the corresponding low-level
 * modules stabilize (separate work items — see HAL phase doc).
 */
#include "uft/uft_hardware.h"
#include "uft/uft_hardware_internal.h"
#include "uft/uft_error.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Generic lifecycle (all backends use no-op init/shutdown for now)
 * ============================================================================ */

static uft_error_t noop_init(void) { return UFT_OK; }
static void        noop_shutdown(void) {}

static uft_error_t empty_enumerate(uft_hw_info_t *devices, size_t max,
                                     size_t *found) {
    (void)devices; (void)max;
    if (found) *found = 0;
    return UFT_OK;  /* zero devices until low-level modules wired in */
}

static uft_error_t empty_open(const uft_hw_info_t *info,
                                uft_hw_device_t **device) {
    (void)info; (void)device;
    return UFT_ERROR_NOT_SUPPORTED;
}

static void empty_close(uft_hw_device_t *device) { (void)device; }

/* ============================================================================
 * FC5025 — Device Side Industries USB 5.25" Controller
 *
 * H5+ upgrade: the device itself needs libusb (USB_VID=0x16C0 / PID=0x06D6,
 * CBW/CSW protocol) or the vendor CLI tool `fcimage`. The Qt provider uses
 * both; the pure-C HAL backend can't pull in libusb unconditionally, so we
 * check for `fcimage` on PATH as a pragmatic presence probe:
 *
 *   enumerate()  — returns one pseudo-device iff `fcimage` is in PATH.
 *   open()       — still UFT_ERROR_NOT_SUPPORTED until the CLI-wrapper
 *                  read_track is implemented (needs tempfile + format arg +
 *                  return-code handling that's riskier than the rest of
 *                  this file; gated to keep this commit tight).
 *
 * This is a visible improvement over the old stub: `readDiskByType(
 * UFT_HW_FC5025, ...)` now reports "device found but driver not wired"
 * on systems where `fcimage` is installed, instead of "no device found"
 * regardless of system state.
 * ============================================================================ */

static bool fcimage_in_path(void) {
#if defined(_WIN32)
    /* `where` exits 0 when the tool is found, 1 otherwise. */
    int rc = system("where fcimage >NUL 2>&1");
#else
    int rc = system("command -v fcimage >/dev/null 2>&1");
#endif
    return rc == 0;
}

static uft_error_t fc5025_enumerate(uft_hw_info_t *devices, size_t max,
                                    size_t *found)
{
    if (found) *found = 0;
    if (!fcimage_in_path()) return UFT_OK;      /* zero devices */
    if (!devices || max == 0) { if (found) *found = 1; return UFT_OK; }

    memset(&devices[0], 0, sizeof(devices[0]));
    devices[0].type = UFT_HW_FC5025;
    strncpy(devices[0].name,     "FC5025 (via fcimage)", sizeof(devices[0].name) - 1);
    strncpy(devices[0].firmware, "unknown",              sizeof(devices[0].firmware) - 1);
    devices[0].usb_vid = 0x16C0;
    devices[0].usb_pid = 0x06D6;
    devices[0].capabilities = UFT_HW_CAP_READ;   /* read-only device */
    if (found) *found = 1;
    return UFT_OK;
}

const uft_hw_backend_t uft_hw_backend_fc5025 = {
    .name          = "FC5025",
    .type          = UFT_HW_FC5025,
    .init          = noop_init,
    .shutdown      = noop_shutdown,
    .enumerate     = fc5025_enumerate,
    .open          = empty_open,        /* CLI-wrapper open: todo */
    .close         = empty_close,
    .read_track    = NULL,
    .write_track   = NULL,
    .read_tracks_batch = NULL,
    .capabilities  = UFT_HW_CAP_READ_TRACK,  /* write protection — read only */
};

/* ============================================================================
 * SuperCard Pro
 * ============================================================================ */

const uft_hw_backend_t uft_hw_backend_supercard_pro = {
    .name          = "SuperCard Pro",
    .type          = UFT_HW_SUPERCARD_PRO,
    .init          = noop_init,
    .shutdown      = noop_shutdown,
    .enumerate     = empty_enumerate,
    .open          = empty_open,
    .close         = empty_close,
    .read_track    = NULL,
    .write_track   = NULL,
    .read_tracks_batch = NULL,
    .capabilities  = UFT_HW_CAP_READ_TRACK | UFT_HW_CAP_WRITE_TRACK |
                     UFT_HW_CAP_READ_FLUX,
};

/* ============================================================================
 * XUM1541 / ZoomFloppy (CBM IEC bus)
 * ============================================================================ */

const uft_hw_backend_t uft_hw_backend_xum1541 = {
    .name          = "XUM1541",
    .type          = UFT_HW_XUM1541,
    .init          = noop_init,
    .shutdown      = noop_shutdown,
    .enumerate     = empty_enumerate,
    .open          = empty_open,
    .close         = empty_close,
    .read_track    = NULL,
    .write_track   = NULL,
    .read_tracks_batch = NULL,
    .capabilities  = UFT_HW_CAP_READ_TRACK | UFT_HW_CAP_WRITE_TRACK |
                     UFT_HW_CAP_IEC_BUS,
};

/* ============================================================================
 * KryoFlux — batch-capable but low-level module still in development
 *
 * Registered here as sync-only until kryoflux_dtc.c exposes a stable API.
 * Batch hook will move to a separate kryoflux_backend.c once ready.
 * ============================================================================ */

const uft_hw_backend_t uft_hw_backend_kryoflux = {
    .name          = "KryoFlux",
    .type          = UFT_HW_UNKNOWN,  /* KryoFlux has no dedicated enum value yet */
    .init          = noop_init,
    .shutdown      = noop_shutdown,
    .enumerate     = empty_enumerate,
    .open          = empty_open,
    .close         = empty_close,
    .read_track    = NULL,
    .write_track   = NULL,
    .read_tracks_batch = NULL,   /* Will become kf_read_tracks_batch when wired */
    .capabilities  = UFT_HW_CAP_READ_TRACK | UFT_HW_CAP_READ_FLUX,
};

/* ============================================================================
 * Auto-registration
 * ============================================================================ */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void sync_backends_register(void) {
    uft_hw_register_backend(&uft_hw_backend_fc5025);
    uft_hw_register_backend(&uft_hw_backend_supercard_pro);
    uft_hw_register_backend(&uft_hw_backend_xum1541);
    uft_hw_register_backend(&uft_hw_backend_kryoflux);
}
#endif
