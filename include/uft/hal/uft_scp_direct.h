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

/* SCP USB vendor/product ID — single source of truth (audit ARCH-7-B,
 * MF-212). Verified from the real device's USB descriptor
 * (USB\VID_16D0&PID_0F8C). The pre-MF-212 value here was 0x16C0:0x0753,
 * which contradicted the GUI port-hint in hardwaretab.cpp; the device
 * readout confirmed the GUI hint was the correct one. The GUI hint now
 * #includes this header and references these macros so the value lives
 * in exactly one place. */
#define UFT_SCP_USB_VID            0x16D0
#define UFT_SCP_USB_PID            0x0F8C

/** SCP USB Bulk endpoints (from interface descriptor). */
#define UFT_SCP_BULK_IN_EP         0x81
#define UFT_SCP_BULK_OUT_EP        0x01

/* SCP USB command byte values.
 *
 * MF-254 (v4.1.5-hardening): the pre-MF-254 values here (0x04 / 0x05 /
 * 0x03 / 0x02 / 0x09 / 0x40) were placeholder guesses that did NOT match
 * the real SCP firmware protocol. Verified against simonowen/samdisk
 * `include/SuperCardPro.h` (the de-facto open-source reference for the
 * SCP SDK v1.7 protocol). The real opcodes are in the 0x80-0xD2 range
 * and follow the [CMD, LEN, params..., CHECKSUM] packet framing
 * documented in samdisk SuperCardPro.cpp. */
#define UFT_SCP_CMD_SELA           0x80  /* select drive A */
#define UFT_SCP_CMD_SELB           0x81  /* select drive B */
#define UFT_SCP_CMD_DSELA          0x82  /* deselect drive A */
#define UFT_SCP_CMD_DSELB          0x83  /* deselect drive B */
#define UFT_SCP_CMD_MTRAON         0x84  /* motor A on */
#define UFT_SCP_CMD_MTRBON         0x85  /* motor B on */
#define UFT_SCP_CMD_MTRAOFF        0x86  /* motor A off */
#define UFT_SCP_CMD_MTRBOFF        0x87  /* motor B off */
#define UFT_SCP_CMD_SEEK0          0x88  /* seek to track 0 (recalibrate) */
#define UFT_SCP_CMD_STEPTO         0x89  /* step to track N (param: 1 byte) */
#define UFT_SCP_CMD_STEPIN         0x8A
#define UFT_SCP_CMD_STEPOUT        0x8B
#define UFT_SCP_CMD_SELDENS        0x8C
#define UFT_SCP_CMD_SIDE           0x8D  /* select side (param: 1 byte: 0/1) */
#define UFT_SCP_CMD_STATUS         0x8E  /* get drive status */
#define UFT_SCP_CMD_GETPARAMS      0x90
#define UFT_SCP_CMD_SETPARAMS      0x91
#define UFT_SCP_CMD_READ_FLUX      0xA0  /* read flux (params: revs+flags) */
#define UFT_SCP_CMD_GET_FLUX_INFO  0xA1  /* fetch rev_index table (40 B) */
#define UFT_SCP_CMD_WRITE_FLUX     0xA2  /* write flux + bulk payload */
#define UFT_SCP_CMD_SENDRAM_USB    0xA9  /* dump firmware RAM via USB bulk-in */
#define UFT_SCP_CMD_SCPINFO        0xD0  /* firmware/board info */

/* CMD_READ_FLUX flag bits (samdisk SuperCardPro.h ff_Index et al). */
#define UFT_SCP_FF_INDEX           0x01  /* wait on index pulse before read */
#define UFT_SCP_FF_BITCELLSIZE     0x02  /* 8-bit vs 16-bit cell size */
#define UFT_SCP_FF_WIPE            0x04  /* wipe track before write */
#define UFT_SCP_FF_RPM360          0x08  /* 360 RPM vs 300 RPM drive */

/* Response status codes (pr_* in samdisk). */
#define UFT_SCP_PR_OK              0x4F  /* successful packet completion */

/* Checksum seed for the SendCmd packet framing
 *   packet = [CMD, LEN, params..., CHECKSUM]
 *   CHECKSUM = CMD + LEN + sum(params) + UFT_SCP_CHECKSUM_INIT
 * Verified from samdisk SuperCardPro.h (CHECKSUM_INIT = 0x4A). */
#define UFT_SCP_CHECKSUM_INIT      0x4A

/** Maximum tracks supported by SCP hardware (0..167 = 84 × 2 sides). */
#define UFT_SCP_MAX_TRACK_INDEX    167

/**
 * Flux sample rate: SCP samples at 40 MHz → 25 ns per sample.
 *
 * Canonical references (cross-checked):
 *   - src/samdisk/scp.cpp:132   `flux_times.push_back(total_time * 25);
 *                                 // 25ns sampling time`
 *   - src/hardware_providers/scphardwareprovider.cpp file-header comment:
 *     "Sample clock: 40 MHz (25 ns resolution)"
 *   - SuperCard Pro SDK v1.7 (cbmstuff.com, December 2015)
 *
 * Earlier scaffold revisions had this as 40 — that was a unit-confusion
 * bug (40 MHz misread as 40 ns) that would scale every flux interval
 * by 40/25 = 1.6× when libusb wiring lands. Corrected to 25 here so
 * the integration commit does not silently inherit the bug.
 */
#define UFT_SCP_FLUX_NS_PER_SAMPLE 25

/** Per-capture limits. SCP firmware requires >= 2 revolutions per read
 *  (samdisk SuperCardPro.cpp clamps `revs = max(2, min(revs, MAX_FLUX_REVS))`).
 *  UFT enforces this strictly at the HAL boundary — single-rev reads
 *  are rejected with UFT_ERR_INVALID_ARG rather than silently coerced. */
#define UFT_SCP_MIN_REVOLUTIONS    2
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
 * @param revolutions   how many revolutions to capture (2..5;
 *                      single-rev rejected with UFT_ERR_INVALID_ARG)
 * @param out_flux      caller-allocated array of transition intervals
 *                      in NANOSECONDS (each tick is 25 ns at the SCP
 *                      40 MHz sample clock)
 * @param out_capacity  capacity of out_flux (in samples)
 * @param out_count     filled with number of transitions captured
 *
 * @return UFT_OK on success,
 *         UFT_ERR_INVALID_ARG on bad parameters,
 *         UFT_ERR_BUFFER_TOO_SMALL if capture exceeds out_capacity
 *         (out_count is then set to required minimum),
 *         UFT_ERR_IO on USB/protocol error.
 *
 * Implementation note: follows the samdisk SuperCardPro.cpp
 * ReadFlux() reference (CMD_READ_FLUX → CMD_GET_FLUX_INFO →
 * per-revolution CMD_SENDRAM_USB). 16-bit big-endian samples; 0x0000
 * marker accumulates an additional 0x10000 ticks. Mock-validated
 * but not yet bench-verified against real hardware.
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
 *
 * GUI / dispatcher contract (rule H-1, MF-148):
 *   - can_*           describe what the HARDWARE can do
 *   - impl_complete   true once the runtime implementation is real
 *
 *   To enable an action in the UI, BOTH must be true. Until then the
 *   action button stays disabled — the user sees the capability exists
 *   but is greyed out, distinguishing "your device cannot do this" from
 *   "this build cannot do this yet".
 */
typedef struct {
    bool     can_read_flux;      /**< true — SCP captures flux natively */
    bool     can_write_flux;     /**< true — SCP can write back */
    bool     can_read_sector;    /**< false — no FDC mode */
    bool     impl_complete;      /**< MF-148: false while M3.1 USB stubs are pending */
    uint32_t max_revolutions;    /**< UFT_SCP_MAX_REVOLUTIONS = 5 */
    uint32_t flux_ns_per_sample; /**< 25 (40 MHz / 25 ns per sample) */
    uint32_t max_track_index;    /**< UFT_SCP_MAX_TRACK_INDEX = 167 */
} uft_scp_direct_capabilities_t;

uft_error_t uft_scp_direct_get_capabilities(
    uft_scp_direct_capabilities_t *out);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HAL_SCP_DIRECT_H */
