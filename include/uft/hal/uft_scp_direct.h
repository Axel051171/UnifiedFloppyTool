/**
 * @file uft_scp_direct.h
 * @brief SuperCard Pro direct-USB HAL backend (M3.1 scaffold).
 *
 * Direct libusb-based communication with SuperCard Pro hardware —
 * replaces the existing subprocess-based integration which shells out
 * to SCP.exe. Production target: full flux read/write via USB Bulk
 * transfer endpoint.
 *
 * Protocol reference: SuperCard Pro USB Command Reference (vendor
 * command 0x02 set-control, 0x03 select-drive, 0x04 read-flux,
 * 0x05 write-flux). See a8rawconv/scp.cpp for the port source.
 *
 * This header is the stable API. The implementation is currently a
 * scaffold — all I/O callbacks return UFT_ERR_NOT_IMPLEMENTED until
 * the USB protocol is wired in. Honest stubs per Prinzip 1.
 *
 * Part of MASTER_PLAN.md M3.1 (see docs/M3_HAL_PLAN.md).
 */
#ifndef UFT_HAL_SCP_DIRECT_H
#define UFT_HAL_SCP_DIRECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** SCP USB vendor IDs (from device descriptor). */
#define UFT_SCP_USB_VID            0x16C0
#define UFT_SCP_USB_PID            0x0753

/** SCP USB Bulk endpoints (from interface descriptor). */
#define UFT_SCP_BULK_IN_EP         0x81
#define UFT_SCP_BULK_OUT_EP        0x01

/** SCP USB command byte values. */
#define UFT_SCP_CMD_READ_FLUX      0x04
#define UFT_SCP_CMD_WRITE_FLUX     0x05
#define UFT_SCP_CMD_SELECT_DRIVE   0x03
#define UFT_SCP_CMD_SET_CONTROL    0x02
#define UFT_SCP_CMD_DESELECT_DRIVE 0x09
#define UFT_SCP_CMD_GET_INFO       0x40

/** Maximum tracks supported by SCP hardware (0..167 = 84 × 2 sides). */
#define UFT_SCP_MAX_TRACK_INDEX    167

/** Flux sample rate: SCP captures at 40 ns resolution. */
#define UFT_SCP_FLUX_NS_PER_SAMPLE 40

/** Per-capture limits. */
#define UFT_SCP_MAX_REVOLUTIONS    5
#define UFT_SCP_DEFAULT_REVOLUTIONS 3

/**
 * Opaque context for one open SCP-Direct session.
 * Internal layout is private to uft_scp_direct.c; callers treat as handle.
 */
typedef struct uft_scp_direct_ctx uft_scp_direct_ctx_t;

/**
 * Open a USB connection to the first SCP device found.
 *
 * @param out_ctx    out: context handle on success (caller frees via _close)
 * @return UFT_OK on success,
 *         UFT_ERR_NOT_IMPLEMENTED until M3.1 USB layer is wired,
 *         UFT_ERR_IO if USB enumeration fails / no SCP device present.
 */
uft_error_t uft_scp_direct_open(uft_scp_direct_ctx_t **out_ctx);

/**
 * Close and release the USB connection.
 */
void uft_scp_direct_close(uft_scp_direct_ctx_t *ctx);

/**
 * Select a physical track (0..167 with side encoded in bit 0).
 * @return UFT_OK, UFT_ERR_INVALID_ARG on out-of-range, or
 *         UFT_ERR_NOT_IMPLEMENTED until USB wiring.
 */
uft_error_t uft_scp_direct_seek(uft_scp_direct_ctx_t *ctx,
                                  int track_index);

/**
 * Capture flux transitions for the given track/side.
 *
 * @param ctx           open context
 * @param side          0 or 1 (upper head selection)
 * @param revolutions   how many revolutions to capture (1..5)
 * @param out_flux      caller-allocated array of transition intervals
 *                      in 40ns units (SCP native resolution)
 * @param out_capacity  capacity of out_flux
 * @param out_count     filled with number of transitions captured
 *
 * @return UFT_OK on success, UFT_ERR_NOT_IMPLEMENTED until M3.1 lands.
 */
uft_error_t uft_scp_direct_read_flux(uft_scp_direct_ctx_t *ctx,
                                      int side,
                                      int revolutions,
                                      uint32_t *out_flux,
                                      size_t out_capacity,
                                      size_t *out_count);

/**
 * Write flux transitions to the current track.
 * @return UFT_OK on success, UFT_ERR_NOT_IMPLEMENTED until M3.1 lands.
 */
uft_error_t uft_scp_direct_write_flux(uft_scp_direct_ctx_t *ctx,
                                       int side,
                                       const uint32_t *flux,
                                       size_t count);

/**
 * Capability flags reported by this backend. Stable across scaffold
 * and final implementation — the hardware capability doesn't change,
 * only whether the software has caught up.
 */
typedef struct {
    bool     can_read_flux;      /**< true — SCP captures flux natively */
    bool     can_write_flux;     /**< true — SCP can write back */
    bool     can_read_sector;    /**< false — no FDC mode */
    uint32_t max_revolutions;    /**< UFT_SCP_MAX_REVOLUTIONS = 5 */
    uint32_t flux_ns_per_sample; /**< 40 */
    uint32_t max_track_index;    /**< UFT_SCP_MAX_TRACK_INDEX = 167 */
} uft_scp_direct_capabilities_t;

uft_error_t uft_scp_direct_get_capabilities(
    uft_scp_direct_capabilities_t *out);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HAL_SCP_DIRECT_H */
