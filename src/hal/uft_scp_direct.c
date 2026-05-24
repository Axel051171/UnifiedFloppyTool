/**
 * @file uft_scp_direct.c
 * @brief SuperCard Pro direct-USB HAL backend (MF-254 — libusb wiring).
 *
 * SPEC_STATUS: VENDOR-DOCUMENTED — based on the SuperCard Pro SDK v1.7
 *   reference release (cbmstuff.com, December 2015) AND cross-checked
 *   against samdisk's SuperCardPro.h + SuperCardPro.cpp port
 *   (simonowen/samdisk on GitHub). Protocol details:
 *
 *   - Wire framing (TX): [CMD, LEN, params..., CHECKSUM]
 *     where CHECKSUM = CMD + LEN + sum(params) + 0x4A (UFT_SCP_CHECKSUM_INIT)
 *   - Wire framing (RX): [CMD_ECHO, STATUS]
 *     STATUS = UFT_SCP_PR_OK (0x4F) on success
 *   - Bulk endpoints: 0x01 OUT, 0x81 IN
 *   - Command opcodes in the 0x80-0xD2 range (NOT the 0x02-0x09 range
 *     the pre-MF-254 scaffold listed — that was placeholder guesses).
 *
 * Hardware-verification status: NOT YET DONE. The libusb wiring here
 * is internally consistent and matches the documented SCP protocol,
 * but has not been run against a physical SuperCard Pro device. See
 * docs/M3_SCP_TRANSPORT.md for the bench-verification plan. Until that
 * runs green, HardwareTab keeps the yellow "Disconnect (Beta)" styling
 * for SCP connections.
 *
 * Defensive contract (forensic invariants):
 *   - Every function validates inputs.
 *   - Every libusb error is mapped to a typed uft_error_t — no silent
 *     no-op, no fabricated flux data.
 *   - Read returns NOT_IMPLEMENTED when the full flux-decode path is
 *     pending (the bulk read of the flux stream is wired but the
 *     transition-time parsing follows the SDK v1.7 spec which has
 *     subtleties best validated against real hardware first).
 */

#include "uft/hal/uft_scp_direct.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* libusb-1.0 is the production transport. On hosts without libusb
 * installed, the build excludes this file at the CMake level
 * (UFT_HAS_LIBUSB compile-define gates the implementation; the
 * fall-back stubs continue to return UFT_ERR_NOT_IMPLEMENTED). */
#ifdef UFT_HAS_LIBUSB
#  include <libusb-1.0/libusb.h>
#endif

/* ─── Internal context ─────────────────────────────────────────────── */

struct uft_scp_direct_ctx {
#ifdef UFT_HAS_LIBUSB
    libusb_context        *libusb_ctx;
    libusb_device_handle  *dev;
    bool                   interface_claimed;
#endif
    bool      is_open;
    int       current_track;
    uint32_t  revolutions_setting;
};

/* ─── Helpers ──────────────────────────────────────────────────────── */

#ifdef UFT_HAS_LIBUSB

/** Send an SCP command packet over the BULK OUT endpoint and read
 *  the 2-byte [CMD_ECHO, STATUS] response. Returns UFT_OK iff status
 *  == UFT_SCP_PR_OK. */
static uft_error_t
scp_send_cmd(uft_scp_direct_ctx_t *ctx,
             uint8_t cmd,
             const uint8_t *params,
             size_t param_len)
{
    if (param_len > 253) return UFT_ERR_INVALID_ARG;

    /* Build packet: [CMD, LEN, params..., CHECKSUM] */
    uint8_t packet[256];
    packet[0] = cmd;
    packet[1] = (uint8_t)param_len;
    if (param_len && params) {
        memcpy(packet + 2, params, param_len);
    }
    uint8_t checksum = UFT_SCP_CHECKSUM_INIT + cmd + (uint8_t)param_len;
    for (size_t i = 0; i < param_len; i++) {
        checksum += params[i];
    }
    packet[2 + param_len] = checksum;
    size_t total = 3 + param_len;

    int transferred = 0;
    int rc = libusb_bulk_transfer(ctx->dev,
                                  UFT_SCP_BULK_OUT_EP,
                                  packet,
                                  (int)total,
                                  &transferred,
                                  /* timeout_ms */ 5000);
    if (rc != 0 || (size_t)transferred != total) {
        return UFT_ERR_IO;
    }

    /* Read 2-byte response [CMD_ECHO, STATUS]. */
    uint8_t response[2] = {0, 0};
    transferred = 0;
    rc = libusb_bulk_transfer(ctx->dev,
                              UFT_SCP_BULK_IN_EP,
                              response,
                              sizeof(response),
                              &transferred,
                              /* timeout_ms */ 5000);
    if (rc != 0 || transferred != 2) {
        return UFT_ERR_IO;
    }
    if (response[0] != cmd) {
        /* Echoed command must match. Any other byte = device is in an
         * unexpected state; refuse to fabricate "success". */
        return UFT_ERR_IO;
    }
    if (response[1] != UFT_SCP_PR_OK) {
        /* Device reported an error code. Map to generic I/O — caller
         * can read the raw status via a future status-query API. */
        return UFT_ERR_IO;
    }
    return UFT_OK;
}

#endif /* UFT_HAS_LIBUSB */

/* ─── Public API ───────────────────────────────────────────────────── */

uft_error_t uft_scp_direct_open(uft_scp_direct_ctx_t **out_ctx)
{
    if (out_ctx == NULL) return UFT_ERR_INVALID_ARG;
    *out_ctx = NULL;

#ifdef UFT_HAS_LIBUSB
    uft_scp_direct_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return UFT_ERR_IO;

    int rc = libusb_init(&ctx->libusb_ctx);
    if (rc != 0) {
        free(ctx);
        return UFT_ERR_IO;
    }

    ctx->dev = libusb_open_device_with_vid_pid(ctx->libusb_ctx,
                                                UFT_SCP_USB_VID,
                                                UFT_SCP_USB_PID);
    if (!ctx->dev) {
        libusb_exit(ctx->libusb_ctx);
        free(ctx);
        /* No SCP device present (or driver wrong — on Windows the
         * default FTDI driver does not expose libusb endpoints;
         * Zadig must replace it with WinUSB). */
        return UFT_ERR_IO;
    }

    /* Detach kernel driver if present (Linux). On Windows + Zadig this
     * is a no-op; on macOS libusb handles it internally. */
    (void)libusb_set_auto_detach_kernel_driver(ctx->dev, 1);

    rc = libusb_claim_interface(ctx->dev, 0);
    if (rc != 0) {
        libusb_close(ctx->dev);
        libusb_exit(ctx->libusb_ctx);
        free(ctx);
        return UFT_ERR_IO;
    }
    ctx->interface_claimed = true;
    ctx->is_open = true;
    ctx->current_track = -1;
    ctx->revolutions_setting = UFT_SCP_DEFAULT_REVOLUTIONS;

    *out_ctx = ctx;
    return UFT_OK;
#else
    /* Build without libusb — keep the honest-stub behaviour. */
    return UFT_ERR_NOT_IMPLEMENTED;
#endif
}

void uft_scp_direct_close(uft_scp_direct_ctx_t *ctx)
{
    if (ctx == NULL) return;
#ifdef UFT_HAS_LIBUSB
    if (ctx->dev) {
        if (ctx->interface_claimed) {
            (void)libusb_release_interface(ctx->dev, 0);
        }
        libusb_close(ctx->dev);
    }
    if (ctx->libusb_ctx) {
        libusb_exit(ctx->libusb_ctx);
    }
#endif
    ctx->is_open = false;
    free(ctx);
}

uft_error_t uft_scp_direct_seek(uft_scp_direct_ctx_t *ctx,
                                  int track_index)
{
    if (ctx == NULL) return UFT_ERR_INVALID_ARG;
    if (track_index < 0 || track_index > UFT_SCP_MAX_TRACK_INDEX) {
        return UFT_ERR_INVALID_ARG;
    }
#ifdef UFT_HAS_LIBUSB
    if (!ctx->is_open || !ctx->dev) return UFT_ERR_IO;
    const uint8_t param = (uint8_t)track_index;
    uft_error_t rc = scp_send_cmd(ctx, UFT_SCP_CMD_STEPTO, &param, 1);
    if (rc == UFT_OK) ctx->current_track = track_index;
    return rc;
#else
    ctx->current_track = track_index;  /* keep state visible even in stub */
    return UFT_ERR_NOT_IMPLEMENTED;
#endif
}

uft_error_t uft_scp_direct_read_flux(uft_scp_direct_ctx_t *ctx,
                                      int side,
                                      int revolutions,
                                      uint32_t *out_flux,
                                      size_t out_capacity,
                                      size_t *out_count)
{
    if (ctx == NULL || out_flux == NULL || out_count == NULL) {
        return UFT_ERR_INVALID_ARG;
    }
    if (side < 0 || side > 1) return UFT_ERR_INVALID_ARG;
    if (revolutions < 1 || revolutions > UFT_SCP_MAX_REVOLUTIONS) {
        return UFT_ERR_INVALID_ARG;
    }
    if (out_capacity == 0) return UFT_ERR_BUFFER_TOO_SMALL;

    *out_count = 0;

#ifdef UFT_HAS_LIBUSB
    if (!ctx->is_open || !ctx->dev) return UFT_ERR_IO;

    /* Select side first. The Applesauce and KryoFlux protocols put
     * the side selection into the read-flux command; SCP exposes it
     * as a separate command. */
    const uint8_t side_param = (uint8_t)side;
    uft_error_t rc = scp_send_cmd(ctx, UFT_SCP_CMD_SIDE, &side_param, 1);
    if (rc != UFT_OK) return rc;

    /* The SDK v1.7 READ_FLUX parameter layout (per samdisk reference)
     * encodes revolutions in the low nibble + flags in the high
     * nibble. We send revolutions only — flags=0 for "plain capture
     * with index detection". The full flag bitmap is documented in
     * docs/M3_SCP_TRANSPORT.md.
     *
     * IMPORTANT: the bulk read of the flux-payload after this command
     * follows a SDK-specific framing (length header + 16-bit big-
     * endian samples or 32-bit little-endian depending on firmware
     * version). Verifying that against a real device is part of the
     * bench-test plan — until then this path returns NOT_IMPLEMENTED
     * BUT honestly states the cause. Sending the command itself is
     * safe (the device just enters capture mode); we abort before
     * fabricating any flux data. */
    const uint8_t read_params[1] = {(uint8_t)revolutions};
    rc = scp_send_cmd(ctx, UFT_SCP_CMD_READ_FLUX, read_params, 1);
    if (rc != UFT_OK) return rc;

    /* MF-254: full flux-payload bulk-read + sample parsing is the
     * remaining piece. The command above has put the device into
     * capture mode but reading the payload requires getting the
     * length-header format right (16-bit big-endian count + 16-bit
     * samples per SCP file spec? or per-revolution headers?). Honest
     * stop here — see docs/M3_SCP_TRANSPORT.md §3. */
    return UFT_ERR_NOT_IMPLEMENTED;
#else
    return UFT_ERR_NOT_IMPLEMENTED;
#endif
}

uft_error_t uft_scp_direct_write_flux(uft_scp_direct_ctx_t *ctx,
                                       int side,
                                       const uint32_t *flux,
                                       size_t count)
{
    if (ctx == NULL || flux == NULL) return UFT_ERR_INVALID_ARG;
    if (side < 0 || side > 1) return UFT_ERR_INVALID_ARG;
    if (count == 0) return UFT_ERR_INVALID_ARG;
    /* Writing to a real disk without bench verification is exactly the
     * irreversible-action class the project forbids (forensic media
     * can be physically damaged by malformed flux). Until the read
     * path is verified on real hardware, the write path stays
     * NOT_IMPLEMENTED even if libusb is available. */
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_scp_direct_get_capabilities(
    uft_scp_direct_capabilities_t *out)
{
    if (out == NULL) return UFT_ERR_INVALID_ARG;
    out->can_read_flux      = true;
    out->can_write_flux     = true;
    out->can_read_sector    = false;
#ifdef UFT_HAS_LIBUSB
    /* MF-254: libusb wiring complete for open/close/seek (verified
     * compiles). Full flux read/write payload parsing pending hardware
     * bench verification — impl_complete stays false until then. */
    out->impl_complete      = false;
#else
    out->impl_complete      = false;
#endif
    out->max_revolutions    = UFT_SCP_MAX_REVOLUTIONS;
    out->flux_ns_per_sample = UFT_SCP_FLUX_NS_PER_SAMPLE;
    out->max_track_index    = UFT_SCP_MAX_TRACK_INDEX;
    return UFT_OK;
}
