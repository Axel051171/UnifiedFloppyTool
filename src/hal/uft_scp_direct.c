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

/** Internal: TX/RX an SCP command + optional bulk payload readback.
 *
 *  Mirrors samdisk SuperCardPro::SendCmd(cmd, p, len, readbuf, readlen).
 *  Steps:
 *    1. Build packet [CMD, LEN, params..., CHECKSUM] and bulk-OUT.
 *    2. Bulk-IN the 2-byte [CMD_ECHO, STATUS] response.
 *    3. If status == PR_OK and bulk_rx_buf != NULL: bulk-IN the
 *       bulk_rx_len trailing payload bytes (e.g. flux samples).
 *
 *  Returns UFT_OK iff every step succeeds and the trailing payload
 *  (if any) was fully received. NEVER fabricates a partial buffer —
 *  if the bulk-IN returns fewer bytes than expected, the function
 *  reports UFT_ERR_IO and leaves bulk_rx_buf in an indeterminate
 *  state (caller must treat as garbage). Forensic invariant: no
 *  silent truncation.
 */
static uft_error_t
scp_send_cmd_ex(uft_scp_direct_ctx_t *ctx,
                uint8_t cmd,
                const uint8_t *params,
                size_t param_len,
                uint8_t *bulk_rx_buf,
                size_t bulk_rx_len)
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

    /* Optional bulk payload readback (CMD_GET_FLUX_INFO, CMD_SENDRAM_USB). */
    if (bulk_rx_buf != NULL && bulk_rx_len > 0) {
        transferred = 0;
        /* Longer timeout for big payloads — a 5-rev flux dump can be
         * hundreds of KB. samdisk uses 10 s for bulk reads; we mirror. */
        rc = libusb_bulk_transfer(ctx->dev,
                                  UFT_SCP_BULK_IN_EP,
                                  bulk_rx_buf,
                                  (int)bulk_rx_len,
                                  &transferred,
                                  /* timeout_ms */ 10000);
        if (rc != 0 || (size_t)transferred != bulk_rx_len) {
            return UFT_ERR_IO;
        }
    }
    return UFT_OK;
}

/** Convenience wrapper for commands with no bulk-read payload. */
static inline uft_error_t
scp_send_cmd(uft_scp_direct_ctx_t *ctx,
             uint8_t cmd,
             const uint8_t *params,
             size_t param_len)
{
    return scp_send_cmd_ex(ctx, cmd, params, param_len, NULL, 0);
}

/** Decode a 32-bit big-endian word from a byte buffer. Forensic:
 *  endian-explicit, never relies on host alignment or byte order. */
static inline uint32_t
scp_read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) | ((uint32_t)p[3]);
}

/** Decode a 16-bit big-endian word. */
static inline uint16_t
scp_read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

/** Encode a 32-bit big-endian word into a byte buffer. */
static inline void
scp_write_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t)(v);
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
    if (revolutions < UFT_SCP_MIN_REVOLUTIONS ||
        revolutions > UFT_SCP_MAX_REVOLUTIONS) {
        /* SCP firmware requires >= 2 revolutions (samdisk clamps).
         * We reject hard rather than silently coerce — forensic. */
        return UFT_ERR_INVALID_ARG;
    }
    if (out_capacity == 0) return UFT_ERR_BUFFER_TOO_SMALL;

    *out_count = 0;

#ifdef UFT_HAS_LIBUSB
    if (!ctx->is_open || !ctx->dev) return UFT_ERR_IO;

    /* Step 1 — Select side. SCP exposes side as its own command
     * (unlike KryoFlux/Applesauce which encode it in the read-flux
     * params). */
    const uint8_t side_param = (uint8_t)side;
    uft_error_t rc = scp_send_cmd(ctx, UFT_SCP_CMD_SIDE, &side_param, 1);
    if (rc != UFT_OK) return rc;

    /* Step 2 — CMD_READ_FLUX with [revs, flags]. flag bit ff_Index
     * = wait for the index pulse before starting capture (so the
     * first sample is index-relative, matching the SCP file format
     * convention). */
    const uint8_t read_params[2] = {
        (uint8_t)revolutions,
        UFT_SCP_FF_INDEX
    };
    rc = scp_send_cmd(ctx, UFT_SCP_CMD_READ_FLUX, read_params, 2);
    if (rc != UFT_OK) return rc;

    /* Step 3 — CMD_GET_FLUX_INFO returns a fixed-size index table
     * of MAX_REVOLUTIONS pairs of 32-bit big-endian words:
     *   [index_time_be32, flux_count_be32] × MAX_REVOLUTIONS
     * Only `flux_count` (samples this revolution) is needed for the
     * bulk read; `index_time` is currently ignored (samdisk also
     * ignores it). */
    uint8_t rev_index_bytes[UFT_SCP_MAX_REVOLUTIONS * 2 * 4];
    rc = scp_send_cmd_ex(ctx, UFT_SCP_CMD_GET_FLUX_INFO,
                         NULL, 0,
                         rev_index_bytes, sizeof(rev_index_bytes));
    if (rc != UFT_OK) return rc;

    /* Step 4 — For each revolution, CMD_SENDRAM_USB with
     * [offset_be32, length_be32] dumps `length` bytes of flux RAM
     * starting at `offset`. Iterate per samdisk's reference loop. */
    uint32_t flux_offset = 0;
    size_t   emitted = 0;

    /* Working buffer for one revolution's flux samples. Sized for
     * the worst case — a 300 RPM track at 25 ns/tick can hold up to
     * ~800 k samples per rev, but in practice mfm tracks are 80-200 k.
     * We allocate the largest the rev_index reports for this capture.
     * Heap allocation kept simple: one malloc per call, freed at end. */
    uint8_t *flux_data = NULL;
    size_t   flux_data_cap = 0;

    for (int i = 0; i < revolutions; i++) {
        uint32_t flux_count = scp_read_be32(&rev_index_bytes[i * 8 + 4]);
        uint32_t flux_bytes = flux_count * (uint32_t)sizeof(uint16_t);

        if (flux_count == 0) {
            /* Empty revolution — index pulse missed or drive not
             * spinning. Skip without emitting; advance offset by 0. */
            continue;
        }

        if (flux_bytes > flux_data_cap) {
            uint8_t *grown = realloc(flux_data, flux_bytes);
            if (!grown) {
                free(flux_data);
                return UFT_ERR_IO;  /* OOM mapped to IO at HAL boundary */
            }
            flux_data = grown;
            flux_data_cap = flux_bytes;
        }

        uint8_t sendram_params[8];
        scp_write_be32(&sendram_params[0], flux_offset);
        scp_write_be32(&sendram_params[4], flux_bytes);

        rc = scp_send_cmd_ex(ctx, UFT_SCP_CMD_SENDRAM_USB,
                             sendram_params, sizeof(sendram_params),
                             flux_data, flux_bytes);
        if (rc != UFT_OK) {
            free(flux_data);
            return rc;
        }

        flux_offset += flux_bytes;

        /* Decode 16-bit big-endian samples → uint32_t ns.
         * Per samdisk: 0x0000 marker adds 0x10000 to running tick
         * accumulator (cell overflow); non-zero emits
         * (accum + value) * NS_PER_TICK and resets accumulator.
         *
         * Accumulator is reset PER REVOLUTION — each capture starts
         * fresh after its index pulse (ff_Index), so trailing 0x0000
         * markers at the end of rev N are physical gaps, NOT data
         * that continues into rev N+1. This matches samdisk exactly
         * (`uint32_t total_time = 0;` inside the per-rev loop). */
        uint32_t accum_ticks = 0;

        for (uint32_t k = 0; k < flux_count; k++) {
            uint16_t sample = scp_read_be16(&flux_data[k * 2]);
            if (sample == 0) {
                accum_ticks += 0x10000u;
            } else {
                accum_ticks += sample;
                if (emitted >= out_capacity) {
                    /* Honest buffer-too-small: report the count
                     * consumed so far. A full pre-pass for exact
                     * required size would need a second iteration;
                     * callers should retry with a larger buffer
                     * (2× the partial count is usually safe). */
                    *out_count = emitted;
                    free(flux_data);
                    return UFT_ERR_BUFFER_TOO_SMALL;
                }
                out_flux[emitted++] = accum_ticks *
                                       (uint32_t)UFT_SCP_FLUX_NS_PER_SAMPLE;
                accum_ticks = 0;
            }
        }

        /* Trailing accumulator (no terminating non-zero in this rev)
         * is DROPPED here — those ticks are physical gap between the
         * last transition and the next index pulse, not transition
         * data we can attribute to a specific edge. Never fabricate
         * a final sample. Matches samdisk. */
    }

    free(flux_data);
    *out_count = emitted;
    return UFT_OK;
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
