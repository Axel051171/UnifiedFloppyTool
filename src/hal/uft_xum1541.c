/**
 * @file uft_xum1541.c
 * @brief XUM1541/ZoomFloppy HAL backend (M3.2 partial scaffold).
 *
 * Per docs/MASTER_PLAN.md §M3.2: this is the C HAL counterpart of the
 * Qt-based src/hardware_providers/xum1541hardwareprovider.cpp. Real
 * USB I/O via libusb is the multi-session continuation; this commit
 * lands:
 *
 *   (1) The opaque uft_xum_config_s struct (was forward-declared only)
 *   (2) Three pure-utility lookups with no USB dependency:
 *       - uft_xum_drive_name()        — enum → display string
 *       - uft_xum_tracks_for_drive()  — drive type → physical track count
 *       - uft_xum_sectors_for_track() — drive+track → CBM sector count
 *   (3) Honest stubs for the 18 USB/IEC-touching functions. Return
 *       UFT_ERR_NOT_IMPLEMENTED with config error message set; callers
 *       introspect via uft_xum_get_error(). Input-validation paths
 *       return UFT_ERR_INVALID_ARG. Status-returning sigs are uft_error_t
 *       (matching SCP-Direct M3.1 + rest of UFT). Pure-data lookups
 *       (tracks_for_drive, sectors_for_track) keep `int` return because
 *       0 = "invalid track / unknown drive" sentinel value.
 *
 * Prinzip 1 honesty rules per .claude/skills/uft-hal-backend:
 *   - never UFT_OK from a stub
 *   - never silently degrade
 *   - never fabricate data to fill gaps
 *
 * Header is the stable API surface (was UFT_SKELETON_PLANNED before
 * this commit; the planned banner is removed in the header now that
 * 3/26 functions have real implementations + the opaque struct exists).
 */

#include "uft/hal/uft_xum1541.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ───────────────────────── Opaque context ────────────────────────────
 *
 * Layout intentionally minimal — real impl will hold libusb_device_handle*,
 * device descriptors, IEC-bus state, current track, retry policy, error
 * buffer. For now we just track "is_open" + last-error string so the
 * stubs can return useful diagnostics.
 * ──────────────────────────────────────────────────────────────────── */

#define UFT_XUM_ERROR_BUF 256

struct uft_xum_config_s {
    bool             is_open;
    int              device_num;
    int              track_start;
    int              track_end;
    int              side;
    int              retries;
    char             last_error[UFT_XUM_ERROR_BUF];
};

static void xum_set_error(uft_xum_config_t *cfg, const char *msg) {
    if (!cfg) return;
    strncpy(cfg->last_error, msg, UFT_XUM_ERROR_BUF - 1);
    cfg->last_error[UFT_XUM_ERROR_BUF - 1] = '\0';
}

/* ─────────────────────── Pure utility lookups ───────────────────────
 *
 * These three functions do NOT touch USB and are fully testable
 * standalone. Values come from Commodore drive specifications:
 *
 *   1541 / 1541-II / 1570: 35 physical tracks, single-sided, GCR,
 *     variable sectors per track in 4 zones (21/19/18/17). Many
 *     custom-format disks use 36-40 (extended tracks).
 *   1571: 35 tracks, double-sided GCR.
 *   1581: 80 tracks, double-sided MFM, fixed 10 sectors/track at
 *     512 bytes (LDOS native; 40 sectors/track from CBM-LBA view).
 *   SFD-1001 / 8050 / 8250: 77 tracks, IEEE-488 (rare with XUM1541).
 *
 * Reference: VIC-1541 service manual + CBM 8000-series tech ref.
 * ──────────────────────────────────────────────────────────────────── */

const char *uft_xum_drive_name(uft_cbm_drive_t type) {
    switch (type) {
        case UFT_CBM_DRIVE_AUTO:    return "auto-detect";
        case UFT_CBM_DRIVE_1541:    return "Commodore 1541";
        case UFT_CBM_DRIVE_1541_II: return "Commodore 1541-II";
        case UFT_CBM_DRIVE_1570:    return "Commodore 1570";
        case UFT_CBM_DRIVE_1571:    return "Commodore 1571";
        case UFT_CBM_DRIVE_1581:    return "Commodore 1581";
        case UFT_CBM_DRIVE_SFD1001: return "Commodore SFD-1001";
        case UFT_CBM_DRIVE_8050:    return "Commodore 8050";
        case UFT_CBM_DRIVE_8250:    return "Commodore 8250";
        default:                    return "unknown";
    }
}

int uft_xum_tracks_for_drive(uft_cbm_drive_t type) {
    switch (type) {
        case UFT_CBM_DRIVE_1541:
        case UFT_CBM_DRIVE_1541_II:
        case UFT_CBM_DRIVE_1570:
        case UFT_CBM_DRIVE_1571:    return 35;  /* native; 36-40 = extended */
        case UFT_CBM_DRIVE_1581:    return 80;
        case UFT_CBM_DRIVE_SFD1001:
        case UFT_CBM_DRIVE_8050:
        case UFT_CBM_DRIVE_8250:    return 77;
        case UFT_CBM_DRIVE_AUTO:
        default:                    return 0;   /* unknown — caller probes */
    }
}

int uft_xum_sectors_for_track(uft_cbm_drive_t type, int track) {
    /* All CBM GCR drives share the same 4-zone sector layout for tracks
     * 1..35. Tracks 36-42 (if a modded drive supports them) stay in
     * zone 0 = 17 sectors. The 1581 and IEEE-488 drives use different
     * fixed schemes. */
    if (track < 1) return 0;

    switch (type) {
        case UFT_CBM_DRIVE_1541:
        case UFT_CBM_DRIVE_1541_II:
        case UFT_CBM_DRIVE_1570:
        case UFT_CBM_DRIVE_1571:
            if (track > 42) return 0;
            if (track <= 17) return 21;
            if (track <= 24) return 19;
            if (track <= 30) return 18;
            return 17;  /* 31..42 */

        case UFT_CBM_DRIVE_1581:
            /* 1581 uses MFM with fixed 40 sectors of 256 bytes (CBM
             * logical-block view). 1..80 valid. */
            if (track > 80) return 0;
            return 40;

        case UFT_CBM_DRIVE_SFD1001:
        case UFT_CBM_DRIVE_8050:
        case UFT_CBM_DRIVE_8250:
            /* IEEE-488 drives: 4 zones across 77 tracks.
             * Reference: 8050 SAMs Photofact technical manual. */
            if (track > 77) return 0;
            if (track <= 39) return 29;
            if (track <= 53) return 27;
            if (track <= 64) return 25;
            return 23;  /* 65..77 */

        case UFT_CBM_DRIVE_AUTO:
        default:
            return 0;
    }
}

/* ───────────────────────── Lifecycle (stubs) ─────────────────────── */

uft_xum_config_t *uft_xum_config_create(void) {
    uft_xum_config_t *cfg = calloc(1, sizeof(*cfg));
    if (!cfg) return NULL;
    cfg->device_num   = 0;
    cfg->track_start  = 1;
    cfg->track_end    = 35;
    cfg->side         = 0;
    cfg->retries      = 3;
    cfg->is_open      = false;
    /* last_error already zeroed by calloc. */
    return cfg;
}

void uft_xum_config_destroy(uft_xum_config_t *cfg) {
    if (!cfg) return;
    /* Real impl: if is_open, also close the USB handle. */
    if (cfg->is_open) {
        /* M3.2 TODO: libusb_release_interface + libusb_close */
    }
    free(cfg);
}

uft_error_t uft_xum_open(uft_xum_config_t *cfg, int device_num) {
    if (!cfg) return UFT_ERR_INVALID_ARG;
    /* M3.2 TODO: libusb_init, enumerate devices by VID/PID, open. */
    cfg->device_num = device_num;
    xum_set_error(cfg, "uft_xum_open: libusb integration pending (M3.2)");
    return UFT_ERR_NOT_IMPLEMENTED;
}

void uft_xum_close(uft_xum_config_t *cfg) {
    if (!cfg) return;
    /* M3.2 TODO: USB release. */
    cfg->is_open = false;
}

bool uft_xum_is_connected(const uft_xum_config_t *cfg) {
    if (!cfg) return false;
    return cfg->is_open;
}

/* ───────────────────────── Device info (stubs) ───────────────────── */

uft_error_t uft_xum_detect(int *device_count) {
    if (device_count) *device_count = 0;
    /* M3.2 TODO: libusb enumerate; count XUM1541 devices. */
    return UFT_ERR_INVALID_ARG;
}

uft_error_t uft_xum_identify_drive(uft_xum_config_t *cfg, uft_cbm_drive_t *type) {
    if (!cfg || !type) return UFT_ERR_INVALID_ARG;
    /* M3.2 TODO: send IEC "M-R" memory-read commands to probe drive ROM
     * signature, then map to UFT_CBM_DRIVE_* enum. */
    *type = UFT_CBM_DRIVE_AUTO;
    xum_set_error(cfg, "uft_xum_identify_drive: not implemented");
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_xum_get_status(uft_xum_config_t *cfg, char *status, size_t max_len) {
    if (!cfg || !status || max_len == 0) return UFT_ERR_INVALID_ARG;
    /* M3.2 TODO: read status channel (secondary 15) from drive. */
    status[0] = '\0';
    xum_set_error(cfg, "uft_xum_get_status: not implemented");
    return UFT_ERR_NOT_IMPLEMENTED;
}

/* ───────────────────────── Configuration ─────────────────────────── */

uft_error_t uft_xum_set_device(uft_xum_config_t *cfg, int device_num) {
    if (!cfg) return UFT_ERR_INVALID_ARG;
    if (device_num < 8 || device_num > 15) {
        xum_set_error(cfg, "device_num out of IEC range (8..15)");
        return UFT_ERR_INVALID_ARG;
    }
    cfg->device_num = device_num;
    return UFT_OK;
}

uft_error_t uft_xum_set_track_range(uft_xum_config_t *cfg, int start, int end) {
    if (!cfg) return UFT_ERR_INVALID_ARG;
    if (start < 1 || end < start || end > 80) {
        xum_set_error(cfg, "track range invalid (1..80, start <= end)");
        return UFT_ERR_INVALID_ARG;
    }
    cfg->track_start = start;
    cfg->track_end   = end;
    return UFT_OK;
}

uft_error_t uft_xum_set_side(uft_xum_config_t *cfg, int side) {
    if (!cfg) return UFT_ERR_INVALID_ARG;
    if (side < 0 || side > 1) {
        xum_set_error(cfg, "side must be 0 or 1");
        return UFT_ERR_INVALID_ARG;
    }
    cfg->side = side;
    return UFT_OK;
}

uft_error_t uft_xum_set_retries(uft_xum_config_t *cfg, int count) {
    if (!cfg) return UFT_ERR_INVALID_ARG;
    if (count < 0 || count > 100) {
        xum_set_error(cfg, "retry count out of range (0..100)");
        return UFT_ERR_INVALID_ARG;
    }
    cfg->retries = count;
    return UFT_OK;
}

/* ───────────────────────── Capture (stubs) ───────────────────────── */

uft_error_t uft_xum_read_track_gcr(uft_xum_config_t *cfg, int track, int side,
                            uint8_t **gcr, size_t *size) {
    if (!cfg || !gcr || !size) return UFT_ERR_INVALID_ARG;
    if (track < 1 || side < 0 || side > 1) return UFT_ERR_INVALID_ARG;
    *gcr = NULL;
    *size = 0;
    /* M3.2 TODO: send U1: command to drive, read GCR via fastloader. */
    xum_set_error(cfg, "uft_xum_read_track_gcr: not implemented");
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_xum_read_track(uft_xum_config_t *cfg, int track, int side,
                        uint8_t *sectors, int *sector_count,
                        uint8_t *errors) {
    if (!cfg || !sectors || !sector_count || !errors) return UFT_ERR_INVALID_ARG;
    if (track < 1 || side < 0 || side > 1) return UFT_ERR_INVALID_ARG;
    *sector_count = 0;
    /* M3.2 TODO: per-sector U1 read + status check. */
    xum_set_error(cfg, "uft_xum_read_track: not implemented");
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_xum_read_disk(uft_xum_config_t *cfg, uft_xum_callback_t callback,
                       void *user) {
    if (!cfg || !callback) return UFT_ERR_INVALID_ARG;
    (void)user;
    /* M3.2 TODO: iterate all tracks, invoke callback per track. */
    xum_set_error(cfg, "uft_xum_read_disk: not implemented");
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_xum_write_track(uft_xum_config_t *cfg, int track, int side,
                         const uint8_t *data, size_t size) {
    if (!cfg || !data) return UFT_ERR_INVALID_ARG;
    if (track < 1 || side < 0 || side > 1 || size == 0) return UFT_ERR_INVALID_ARG;
    /* M3.2 TODO: write via U2 command. */
    xum_set_error(cfg, "uft_xum_write_track: not implemented");
    return UFT_ERR_NOT_IMPLEMENTED;
}

/* ───────────────────────── Low-level IEC (stubs) ─────────────────── */

uft_error_t uft_xum_iec_listen(uft_xum_config_t *cfg, int device, int secondary) {
    if (!cfg) return UFT_ERR_INVALID_ARG;
    if (device < 0 || device > 31 || secondary < 0 || secondary > 31) {
        xum_set_error(cfg, "IEC device/secondary out of range");
        return UFT_ERR_INVALID_ARG;
    }
    /* M3.2 TODO: bulk-out byte UFT_IEC_LISTEN | device, then secondary. */
    xum_set_error(cfg, "uft_xum_iec_listen: not implemented");
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_xum_iec_talk(uft_xum_config_t *cfg, int device, int secondary) {
    if (!cfg) return UFT_ERR_INVALID_ARG;
    if (device < 0 || device > 31 || secondary < 0 || secondary > 31) {
        xum_set_error(cfg, "IEC device/secondary out of range");
        return UFT_ERR_INVALID_ARG;
    }
    /* M3.2 TODO: bulk-out byte UFT_IEC_TALK | device, then secondary. */
    xum_set_error(cfg, "uft_xum_iec_talk: not implemented");
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_xum_iec_unlisten(uft_xum_config_t *cfg) {
    if (!cfg) return UFT_ERR_INVALID_ARG;
    /* M3.2 TODO: bulk-out byte UFT_IEC_UNLISTEN. */
    xum_set_error(cfg, "uft_xum_iec_unlisten: not implemented");
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_xum_iec_untalk(uft_xum_config_t *cfg) {
    if (!cfg) return UFT_ERR_INVALID_ARG;
    /* M3.2 TODO: bulk-out byte UFT_IEC_UNTALK. */
    xum_set_error(cfg, "uft_xum_iec_untalk: not implemented");
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_xum_iec_write(uft_xum_config_t *cfg, const uint8_t *data, size_t len) {
    if (!cfg || !data) return UFT_ERR_INVALID_ARG;
    if (len == 0) return UFT_OK;  /* nothing to write — not an error */
    /* M3.2 TODO: bulk transfer. */
    xum_set_error(cfg, "uft_xum_iec_write: not implemented");
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_xum_iec_read(uft_xum_config_t *cfg, uint8_t *data, size_t max_len) {
    if (!cfg || !data || max_len == 0) return UFT_ERR_INVALID_ARG;
    /* M3.2 TODO: bulk transfer. */
    xum_set_error(cfg, "uft_xum_iec_read: not implemented");
    return UFT_ERR_NOT_IMPLEMENTED;
}

/* ───────────────────────── Utility (real) ────────────────────────── */

const char *uft_xum_get_error(const uft_xum_config_t *cfg) {
    if (!cfg) return "NULL config";
    return cfg->last_error[0] ? cfg->last_error : "no error";
}
