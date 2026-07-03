/**
 * @file uft_xum1541.c
 * @brief XUM1541/ZoomFloppy HAL backend (M3.2 partial scaffold).
 *
 * SPEC_STATUS: REVERSE-ENGINEERED — based on opencbm xum1541.c
 *   (https://github.com/OpenCBM/OpenCBM, BSD-2),
 *   IEC bus timing from Commodore 1541 service manual (Sams Photofact),
 *   sector-zone tables from VIC-1541 + 8050 ROM listings.
 *   NOT covered by an official ZoomFloppy SDK.
 *
 * Per docs/MASTER_PLAN.md §M3.2: this is the C HAL counterpart of the
 * Qt-based V2 provider src/hardware_providers/xum1541_provider_v2.cpp
 * (the V1 xum1541hardwareprovider.cpp was deleted in P1.18). Real
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

#ifdef UFT_HAS_LIBUSB
#  include <libusb-1.0/libusb.h>
#endif

/* ───────────────────────── Opaque context ────────────────────────────
 *
 * Holds libusb handle + USB lifecycle state when UFT_HAS_LIBUSB is
 * defined; otherwise only the pure-utility fields so the build still
 * succeeds without libusb installed.
 * ──────────────────────────────────────────────────────────────────── */

#define UFT_XUM_ERROR_BUF 256

struct uft_xum_config_s {
#ifdef UFT_HAS_LIBUSB
    libusb_context        *libusb_ctx;
    libusb_device_handle  *dev;
    bool                   interface_claimed;
#endif
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
    if (cfg->is_open) {
        uft_xum_close(cfg);
    }
    free(cfg);
}

/* ───────────────────────── libusb helpers ──────────────────────────── */

#ifdef UFT_HAS_LIBUSB
/** Internal: send a bulk-OUT payload. Returns number of bytes written
 *  or negative on error. */
static int xum_bulk_out(uft_xum_config_t *cfg,
                        const uint8_t *data, int len, int timeout_ms)
{
    int transferred = 0;
    int rc = libusb_bulk_transfer(cfg->dev,
                                  UFT_XUM1541_BULK_OUT_EP,
                                  (unsigned char *)data,
                                  len,
                                  &transferred,
                                  timeout_ms);
    if (rc != 0) return -rc;
    return transferred;
}

/** Internal: receive a bulk-IN payload. */
static int xum_bulk_in(uft_xum_config_t *cfg,
                       uint8_t *buf, int len, int timeout_ms)
{
    int transferred = 0;
    int rc = libusb_bulk_transfer(cfg->dev,
                                  UFT_XUM1541_BULK_IN_EP,
                                  buf,
                                  len,
                                  &transferred,
                                  timeout_ms);
    if (rc != 0) return -rc;
    return transferred;
}

/** Read the 3-byte XUM1541 status from bulk IN, looping while the
 *  firmware reports BUSY (async commands). Layout per OpenCBM
 *  xum1541_types.h: buf[0]=status, buf[1..2]=LE extended value.
 *  MF-301: the pre-audit code read only 1 byte — real firmware sends
 *  XUM_STATUSBUF_SIZE(3); a 1-byte request would fail with overflow. */
static uft_error_t xum_wait_status(uft_xum_config_t *cfg, uint16_t *val_out)
{
    for (int attempt = 0; attempt < 200; attempt++) {   /* ~bounded busy-wait */
        uint8_t buf[UFT_XUM1541_STATUSBUF_SIZE] = {0};
        int n = xum_bulk_in(cfg, buf, sizeof(buf), UFT_XUM1541_CTRL_TIMEOUT_MS);
        if (n != (int)sizeof(buf)) {
            xum_set_error(cfg, "xum_wait_status: bulk IN status failed");
            return UFT_ERR_IO;
        }
        switch (UFT_XUM1541_GET_STATUS(buf)) {
            case UFT_XUM1541_STATUS_BUSY:
                continue;                    /* async op still running */
            case UFT_XUM1541_STATUS_READY:
                if (val_out) *val_out = UFT_XUM1541_GET_STATUS_VAL(buf);
                return UFT_OK;
            case UFT_XUM1541_STATUS_ERROR:
            default:
                xum_set_error(cfg, "xum_wait_status: device returned STATUS_ERROR");
                return UFT_ERR_IO;
        }
    }
    xum_set_error(cfg, "xum_wait_status: device stuck BUSY");
    return UFT_ERR_TIMEOUT;
}

/** Send a 4-byte ioctl-style bulk command [cmd, arg1, arg2, 0] and wait
 *  for the 3-byte status; the extended value lands in *val_out. OpenCBM
 *  sends ioctls (XUM1541_IOCTL+n) as BULK commands, not control
 *  transfers (MF-301 re-audit). */
static uft_error_t xum_iec_command(uft_xum_config_t *cfg,
                                    uint8_t opcode,
                                    uint8_t arg1,
                                    uint8_t arg2,
                                    uint16_t *val_out)
{
    uint8_t cmd[4] = { opcode, arg1, arg2, 0 };
    int n = xum_bulk_out(cfg, cmd, sizeof(cmd), UFT_XUM1541_CTRL_TIMEOUT_MS);
    if (n != (int)sizeof(cmd)) {
        xum_set_error(cfg, "xum_iec_command: bulk OUT failed");
        return UFT_ERR_IO;
    }
    return xum_wait_status(cfg, val_out);
}

/** CBM-protocol WRITE: header [XUM1541_BULK_WRITE, proto, len_lo,
 *  len_hi] + payload, then 3-byte status whose extended value is the
 *  byte count actually transferred on the IEC bus. This is ALSO how
 *  IEC addressing works — talk/listen/untalk/unlisten are a WRITE with
 *  the ATN flag and the raw ATN bytes as payload (OpenCBM archlib.c),
 *  NOT separate opcodes (the pre-MF-301 opcode table was fictional). */
static uft_error_t xum_cbm_write(uft_xum_config_t *cfg, uint8_t proto,
                                  const uint8_t *data, size_t len,
                                  uint16_t *written_out)
{
    uint8_t header[4] = {
        UFT_XUM1541_BULK_WRITE,
        proto,
        (uint8_t)(len & 0xFF),
        (uint8_t)((len >> 8) & 0xFF)
    };
    int n = xum_bulk_out(cfg, header, sizeof(header),
                         UFT_XUM1541_CTRL_TIMEOUT_MS);
    if (n != (int)sizeof(header)) {
        xum_set_error(cfg, "xum_cbm_write: header transfer failed");
        return UFT_ERR_IO;
    }
    n = xum_bulk_out(cfg, data, (int)len, UFT_XUM1541_CTRL_TIMEOUT_MS);
    if (n != (int)len) {
        xum_set_error(cfg, "xum_cbm_write: payload transfer short");
        return UFT_ERR_IO;
    }
    uint16_t written = 0;
    uft_error_t err = xum_wait_status(cfg, &written);
    if (err != UFT_OK) return err;
    if (written_out) *written_out = written;
    return UFT_OK;
}
#endif /* UFT_HAS_LIBUSB */

uft_error_t uft_xum_open(uft_xum_config_t *cfg, int device_num) {
    if (!cfg) return UFT_ERR_INVALID_ARG;
#ifdef UFT_HAS_LIBUSB
    if (cfg->is_open) return UFT_OK;  /* idempotent */

    int rc = libusb_init(&cfg->libusb_ctx);
    if (rc != 0) {
        xum_set_error(cfg, "libusb_init failed");
        return UFT_ERR_IO;
    }

    cfg->dev = libusb_open_device_with_vid_pid(cfg->libusb_ctx,
                                                UFT_XUM1541_USB_VID,
                                                UFT_XUM1541_USB_PID);
    if (!cfg->dev) {
        libusb_exit(cfg->libusb_ctx);
        cfg->libusb_ctx = NULL;
        xum_set_error(cfg,
            "no XUM1541/ZoomFloppy device found (VID 0x16D0 PID 0x0504)");
        return UFT_ERR_IO;
    }

    (void)libusb_set_auto_detach_kernel_driver(cfg->dev, 1);

    rc = libusb_claim_interface(cfg->dev, 0);
    if (rc != 0) {
        libusb_close(cfg->dev);
        libusb_exit(cfg->libusb_ctx);
        cfg->dev = NULL;
        cfg->libusb_ctx = NULL;
        xum_set_error(cfg, "libusb_claim_interface(0) failed");
        return UFT_ERR_IO;
    }
    cfg->interface_claimed = true;

    /* INIT control transfer — reads back 8 bytes of device info. */
    uint8_t info[8];
    rc = libusb_control_transfer(cfg->dev,
        /* bmRequestType: device-in, vendor, device */
        0xC0,
        /* bRequest */ UFT_XUM1541_CTRL_INIT,
        /* wValue */   0,
        /* wIndex */   0,
        info, sizeof(info),
        UFT_XUM1541_CTRL_TIMEOUT_MS);
    if (rc < 0) {
        (void)libusb_release_interface(cfg->dev, 0);
        libusb_close(cfg->dev);
        libusb_exit(cfg->libusb_ctx);
        cfg->dev = NULL;
        cfg->libusb_ctx = NULL;
        cfg->interface_claimed = false;
        xum_set_error(cfg, "XUM1541_INIT control transfer failed");
        return UFT_ERR_IO;
    }

    cfg->device_num = device_num;
    cfg->is_open = true;
    return UFT_OK;
#else
    cfg->device_num = device_num;
    xum_set_error(cfg, "uft_xum_open: built without libusb (UFT_HAS_LIBUSB)");
    return UFT_ERR_NOT_IMPLEMENTED;
#endif
}

void uft_xum_close(uft_xum_config_t *cfg) {
    if (!cfg) return;
#ifdef UFT_HAS_LIBUSB
    if (cfg->dev) {
        /* SHUTDOWN control transfer signals graceful close to firmware. */
        uint8_t dummy = 0;
        (void)libusb_control_transfer(cfg->dev, 0xC0,
            UFT_XUM1541_CTRL_SHUTDOWN, 0, 0,
            &dummy, sizeof(dummy), UFT_XUM1541_CTRL_TIMEOUT_MS);
        if (cfg->interface_claimed) {
            (void)libusb_release_interface(cfg->dev, 0);
        }
        libusb_close(cfg->dev);
        cfg->dev = NULL;
    }
    if (cfg->libusb_ctx) {
        libusb_exit(cfg->libusb_ctx);
        cfg->libusb_ctx = NULL;
    }
    cfg->interface_claimed = false;
#endif
    cfg->is_open = false;
}

bool uft_xum_is_connected(const uft_xum_config_t *cfg) {
    if (!cfg) return false;
    return cfg->is_open;
}

/* ───────────────────────── Device info ───────────────────────────── */

uft_error_t uft_xum_detect(int *device_count) {
    if (!device_count) return UFT_ERR_INVALID_ARG;
    *device_count = 0;
#ifdef UFT_HAS_LIBUSB
    libusb_context *ctx = NULL;
    if (libusb_init(&ctx) != 0) return UFT_ERR_IO;
    libusb_device **list = NULL;
    ssize_t n = libusb_get_device_list(ctx, &list);
    int count = 0;
    for (ssize_t i = 0; i < n; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(list[i], &desc) == 0) {
            if (desc.idVendor  == UFT_XUM1541_USB_VID &&
                desc.idProduct == UFT_XUM1541_USB_PID) {
                count++;
            }
        }
    }
    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    *device_count = count;
    return UFT_OK;
#else
    return UFT_ERR_NOT_IMPLEMENTED;
#endif
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
#ifdef UFT_HAS_LIBUSB
    if (!cfg->is_open || !cfg->dev) {
        xum_set_error(cfg, "uft_xum_iec_listen: device not open");
        return UFT_ERR_IO;
    }
    /* MF-301: LISTEN = CBM-WRITE with ATN flag; payload = raw IEC ATN
     * bytes (0x20|device, 0x60|secondary), per OpenCBM archlib.c. */
    {
        uint8_t atn[2] = { (uint8_t)(0x20 | device),
                           (uint8_t)(0x60 | secondary) };
        return xum_cbm_write(cfg,
                             UFT_XUM1541_PROTO_CBM | UFT_XUM1541_FLAG_WRITE_ATN,
                             atn, sizeof(atn), NULL);
    }
#else
    xum_set_error(cfg, "uft_xum_iec_listen: built without libusb");
    return UFT_ERR_NOT_IMPLEMENTED;
#endif
}

uft_error_t uft_xum_iec_talk(uft_xum_config_t *cfg, int device, int secondary) {
    if (!cfg) return UFT_ERR_INVALID_ARG;
    if (device < 0 || device > 31 || secondary < 0 || secondary > 31) {
        xum_set_error(cfg, "IEC device/secondary out of range");
        return UFT_ERR_INVALID_ARG;
    }
#ifdef UFT_HAS_LIBUSB
    if (!cfg->is_open || !cfg->dev) {
        xum_set_error(cfg, "uft_xum_iec_talk: device not open");
        return UFT_ERR_IO;
    }
    /* MF-301: TALK = CBM-WRITE with ATN+TALK flags; payload = raw IEC
     * ATN bytes (0x40|device, 0x60|secondary). */
    {
        uint8_t atn[2] = { (uint8_t)(0x40 | device),
                           (uint8_t)(0x60 | secondary) };
        return xum_cbm_write(cfg,
                             UFT_XUM1541_PROTO_CBM
                                 | UFT_XUM1541_FLAG_WRITE_ATN
                                 | UFT_XUM1541_FLAG_WRITE_TALK,
                             atn, sizeof(atn), NULL);
    }
#else
    xum_set_error(cfg, "uft_xum_iec_talk: built without libusb");
    return UFT_ERR_NOT_IMPLEMENTED;
#endif
}

uft_error_t uft_xum_iec_unlisten(uft_xum_config_t *cfg) {
    if (!cfg) return UFT_ERR_INVALID_ARG;
#ifdef UFT_HAS_LIBUSB
    if (!cfg->is_open || !cfg->dev) {
        xum_set_error(cfg, "uft_xum_iec_unlisten: device not open");
        return UFT_ERR_IO;
    }
    /* MF-301: UNLISTEN = ATN byte 0x3F via CBM-WRITE+ATN. */
    {
        uint8_t atn = 0x3F;
        return xum_cbm_write(cfg,
                             UFT_XUM1541_PROTO_CBM | UFT_XUM1541_FLAG_WRITE_ATN,
                             &atn, 1, NULL);
    }
#else
    xum_set_error(cfg, "uft_xum_iec_unlisten: built without libusb");
    return UFT_ERR_NOT_IMPLEMENTED;
#endif
}

uft_error_t uft_xum_iec_untalk(uft_xum_config_t *cfg) {
    if (!cfg) return UFT_ERR_INVALID_ARG;
#ifdef UFT_HAS_LIBUSB
    if (!cfg->is_open || !cfg->dev) {
        xum_set_error(cfg, "uft_xum_iec_untalk: device not open");
        return UFT_ERR_IO;
    }
    /* MF-301: UNTALK = ATN byte 0x5F via CBM-WRITE+ATN. */
    {
        uint8_t atn = 0x5F;
        return xum_cbm_write(cfg,
                             UFT_XUM1541_PROTO_CBM | UFT_XUM1541_FLAG_WRITE_ATN,
                             &atn, 1, NULL);
    }
#else
    xum_set_error(cfg, "uft_xum_iec_untalk: built without libusb");
    return UFT_ERR_NOT_IMPLEMENTED;
#endif
}

uft_error_t uft_xum_iec_write(uft_xum_config_t *cfg, const uint8_t *data, size_t len) {
    if (!cfg || !data) return UFT_ERR_INVALID_ARG;
    if (len == 0) return UFT_OK;
    if (len > UFT_XUM1541_MAX_XFER_SIZE - 4) return UFT_ERR_BUFFER_TOO_SMALL;
#ifdef UFT_HAS_LIBUSB
    if (!cfg->is_open || !cfg->dev) {
        xum_set_error(cfg, "uft_xum_iec_write: device not open");
        return UFT_ERR_IO;
    }
    /* MF-301: plain CBM data write — header [WRITE, PROTO_CBM, lo, hi]
     * + payload + 3-byte status. The status extended value is the byte
     * count actually delivered on the IEC bus; a short count is an
     * honest partial-write error, never silently accepted. */
    {
        uint16_t written = 0;
        uft_error_t err = xum_cbm_write(cfg, UFT_XUM1541_PROTO_CBM,
                                        data, len, &written);
        if (err != UFT_OK) return err;
        if (written != len) {
            xum_set_error(cfg, "uft_xum_iec_write: short IEC write");
            return UFT_ERR_IO;
        }
        return UFT_OK;
    }
#else
    xum_set_error(cfg, "uft_xum_iec_write: built without libusb");
    return UFT_ERR_NOT_IMPLEMENTED;
#endif
}

uft_error_t uft_xum_iec_read(uft_xum_config_t *cfg, uint8_t *data,
                              size_t max_len, size_t *bytes_read) {
    if (!cfg || !data || max_len == 0 || !bytes_read) return UFT_ERR_INVALID_ARG;
    *bytes_read = 0;
    if (max_len > UFT_XUM1541_MAX_XFER_SIZE) max_len = UFT_XUM1541_MAX_XFER_SIZE;
#ifdef UFT_HAS_LIBUSB
    if (!cfg->is_open || !cfg->dev) {
        xum_set_error(cfg, "uft_xum_iec_read: device not open");
        return UFT_ERR_IO;
    }
    /* MF-301: header [READ, PROTO_CBM, len_lo, len_hi], then the data
     * phase directly (no status phase on reads, per OpenCBM host lib). */
    uint8_t header[4] = {
        UFT_XUM1541_BULK_READ,
        UFT_XUM1541_PROTO_CBM,
        (uint8_t)(max_len & 0xFF),
        (uint8_t)((max_len >> 8) & 0xFF)
    };
    int n = xum_bulk_out(cfg, header, sizeof(header),
                         UFT_XUM1541_CTRL_TIMEOUT_MS);
    if (n != (int)sizeof(header)) {
        xum_set_error(cfg, "uft_xum_iec_read: header transfer failed");
        return UFT_ERR_IO;
    }
    n = xum_bulk_in(cfg, data, (int)max_len, UFT_XUM1541_CTRL_TIMEOUT_MS);
    if (n < 0) {
        xum_set_error(cfg, "uft_xum_iec_read: bulk IN failed");
        return UFT_ERR_IO;
    }
    /* Short reads are legitimate (IEC EOI ends the transfer early) —
     * the caller gets the exact count instead of guessing (MF-301,
     * forensic length preservation). */
    *bytes_read = (size_t)n;
    return UFT_OK;
#else
    xum_set_error(cfg, "uft_xum_iec_read: built without libusb");
    return UFT_ERR_NOT_IMPLEMENTED;
#endif
}

uft_error_t uft_xum_iec_poll(uft_xum_config_t *cfg, uint8_t *lines_out) {
    if (!cfg || !lines_out) return UFT_ERR_INVALID_ARG;
    *lines_out = 0;
#ifdef UFT_HAS_LIBUSB
    if (!cfg->is_open || !cfg->dev) {
        xum_set_error(cfg, "uft_xum_iec_poll: device not open");
        return UFT_ERR_IO;
    }
    /* MF-301: IEC_POLL ioctl (27) as bulk command; the line states come
     * back in the 3-byte status extended value (low byte). */
    uint16_t val = 0;
    uft_error_t err = xum_iec_command(cfg, UFT_XUM1541_IOCTL_IEC_POLL,
                                      0, 0, &val);
    if (err != UFT_OK) return err;
    *lines_out = (uint8_t)(val & 0xFF);
    return UFT_OK;
#else
    xum_set_error(cfg, "uft_xum_iec_poll: built without libusb");
    return UFT_ERR_NOT_IMPLEMENTED;
#endif
}

/* ───────────────────────── Utility (real) ────────────────────────── */

const char *uft_xum_get_error(const uft_xum_config_t *cfg) {
    if (!cfg) return "NULL config";
    return cfg->last_error[0] ? cfg->last_error : "no error";
}
