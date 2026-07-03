/**
 * @file tests/emulators/xum1541/firmware_state_machine.h
 * @brief Firmware-realistic state machine for the XUM1541/ZoomFloppy.
 *
 * The XUM1541 is an AVR-based USB <-> Commodore-IEC bridge, not a flux
 * sampler. Its firmware exposes (MF-301 OpenCBM-verified wire protocol):
 *   - USB control transfers: ECHO/INIT/RESET/SHUTDOWN/ENTER_BOOTLOADER/
 *     TAP_BREAK/GITREV/GCCVER/LIBCVER (bRequest values in
 *     include/uft/hal/uft_xum1541.h — the in-repo SSOT)
 *   - Bulk data commands: ONLY READ(8) and WRITE(9), 4-byte OUT header
 *     [opcode, proto|flags, size_lo, size_hi]. Upper nibble of byte 1 =
 *     protocol (CBM/S1/S2/PP/P2/NIB), lower nibble = flags
 *     (XUM_WRITE_TALK, XUM_WRITE_ATN). IEC addressing (talk/listen/
 *     untalk/unlisten) is NOT separate opcodes: it is a WRITE with the
 *     ATN flag and the raw IEC ATN bytes as payload (OpenCBM archlib.c).
 *   - IOCTLs (GET_EOI..PARBURST_WRITE = 23..31): 4-byte bulk commands
 *     [cmd, arg1, arg2, 0]; result in the status extended value.
 *   - Status phase: 3-byte bulk IN [status, val_lo, val_hi]. WRITE
 *     reports the IEC byte count in the extended value. READ has NO
 *     status phase — the data phase follows directly; a short read
 *     signals IEC EOI.
 *
 * SPEC_STATUS: SOURCE-VERIFIED (MF-301) — modelled on the OpenCBM
 * xum1541 firmware + host library (github.com/OpenCBM/OpenCBM, BSD-2:
 * xum1541/xum1541_types.h, opencbm/lib/plugin/xum1541/xum1541.c,
 * archlib.c). There is no official ZoomFloppy SDK. The pre-MF-301
 * emulator modelled a FICTIONAL opcode table (separate TALK/LISTEN/
 * OPEN/CLOSE bulk opcodes) — see DIVERGENCES.md X-DELTA-3.
 *
 * Forensic invariant: this emulator NEVER produces GCR/track bytes
 * itself. Drive payloads come from the synthetic GCR generator
 * (tests/flux_gen/xum1541/flux_gen.c), loaded via
 * xum_fw_load_talk_stream().
 *
 * Write path: bulk WRITE payloads are ACCEPTED and counted (the IEC
 * LISTEN direction also carries harmless drive commands, e.g. "I0"),
 * but nothing is ever persisted or echoed back as disk content — there
 * is no emulated medium to corrupt. ENTER_BOOTLOADER is ALWAYS refused
 * (a wrong bootloader entry can brick real hardware).
 */
#ifndef UFT_TESTS_XUM_FIRMWARE_STATE_MACHINE_H
#define UFT_TESTS_XUM_FIRMWARE_STATE_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/hal/uft_xum1541.h"   /* opcode/proto/status constants — SSOT */

#ifdef __cplusplus
extern "C" {
#endif

/* ─── State enum (explicit numeric values for ABI-stability) ────────── */
typedef enum {
    XUM_FW_STATE_DISCONNECTED = 0, /* not enumerated on USB */
    XUM_FW_STATE_CONNECTED    = 1, /* enumerated; CTRL_INIT not yet done */
    XUM_FW_STATE_READY        = 2, /* INIT done; IEC bus idle */
    XUM_FW_STATE_LISTENING    = 3, /* drive addressed as listener (ATN) */
    XUM_FW_STATE_TALKING      = 4, /* drive addressed as talker (ATN) */
    XUM_FW_STATE_SHUTDOWN     = 5, /* CTRL_SHUTDOWN received */
    XUM_FW_STATE_ERROR        = 6, /* sticky protocol violation */
} xum_fw_state_t;

/* ─── Error reasons (sim-side diagnostics, sticky in ERROR state) ───── */
typedef enum {
    XUM_FW_ERR_NONE                = 0,
    XUM_FW_ERR_NOT_INITIALIZED     = 1,  /* bulk cmd before CTRL_INIT */
    XUM_FW_ERR_BAD_OPCODE          = 2,  /* opcode not READ/WRITE/ioctl */
    XUM_FW_ERR_BAD_PROTOCOL        = 3,  /* unknown proto nibble / missing
                                            TALK flag on a talk-ATN */
    XUM_FW_ERR_DEVICE_NOT_PRESENT  = 4,  /* IEC ATN timeout (transient) */
    XUM_FW_ERR_BUS_ROLE_CONFLICT   = 5,  /* listen-ATN while TALKING etc. */
    XUM_FW_ERR_NO_LISTENER         = 6,  /* data WRITE without listener */
    XUM_FW_ERR_NO_TALKER           = 7,  /* READ without talker */
    XUM_FW_ERR_SHUTDOWN            = 8,
    XUM_FW_ERR_OVERRUN             = 9,  /* transfer > firmware buffer */
    XUM_FW_ERR_NOT_MODELLED        = 10, /* honest refusal (S1..NIB, tape,
                                            parburst) */
} xum_fw_err_reason_t;

/* ─── Wire constants not in the UFT HAL header ──────────────────────
 * Values from OpenCBM xum1541_types.h (BSD-2). Best-effort — flagged
 * in DIVERGENCES.md X-D9 until verified on real hardware. */
#define XUM_FW_VERSION_DEFAULT   8      /* XUM1541_VERSION */
#define XUM_FW_CAP_CBM           0x01
#define XUM_FW_CAP_NIB           0x02
#define XUM_FW_CAP_NIB_SRQ       0x04
#define XUM_FW_CAP_IEEE488       0x08
#define XUM_FW_CAP_TAP           0x10
#define XUM_FW_INIT_DOING_RESET  0x01   /* INIT status flag byte 2 */
#define XUM_FW_INIT_NO_DEVICE    0x02

/* IEC line bits (OpenCBM iec_poll semantics). */
#define XUM_FW_IEC_DATA   0x01
#define XUM_FW_IEC_CLOCK  0x02
#define XUM_FW_IEC_ATN    0x04
#define XUM_FW_IEC_RESET  0x08
#define XUM_FW_IEC_SRQ    0x10

/* Sim-side sentinel: no status phase occurred (device disconnected /
 * shut down, or command has a data phase before its status phase).
 * Real status codes are UFT_XUM1541_STATUS_{BUSY,READY,ERROR} = 1,2,3. */
#define XUM_FW_NO_STATUS  0

/* Control-transfer refusal (mirrors a libusb pipe error at the shim). */
#define XUM_FW_CTRL_REFUSED  (-1)

#define XUM_FW_VERSTR_MAX 32

/* ─── Emulator context ──────────────────────────────────────────────
 * One context per simulated device; caller-owned, reset via
 * xum_fw_reset(). The talk-stream buffer is borrowed (never copied,
 * never freed by the emulator). */
typedef struct {
    xum_fw_state_t      state;
    xum_fw_err_reason_t sticky_error;   /* set while state == ERROR */
    xum_fw_err_reason_t last_error;     /* incl. transient refusals */

    /* Identity reported by CTRL_INIT (configurable per test). */
    uint8_t fw_version;
    uint8_t capabilities;
    bool    doing_reset_flag;   /* power-on; reported once by INIT */
    char    gitrev[XUM_FW_VERSTR_MAX];
    char    gccver[XUM_FW_VERSTR_MAX];
    char    libcver[XUM_FW_VERSTR_MAX];

    /* Simulated bus/drive environment (set per test). */
    bool    drive_present;
    uint8_t drive_device_num;   /* IEC primary address, usually 8 */
    uint8_t iec_lines;          /* XUM_FW_IEC_* mask */
    uint8_t pp_port;            /* parallel-port latch (PP_READ/PP_WRITE) */

    /* IEC session state. */
    uint8_t current_secondary;  /* last 0x60|sec ATN byte seen */
    bool    eoi_flag;

    /* Pending bulk data phase (set by a WRITE / READ command). */
    uint16_t pending_write_len;
    uint8_t  pending_write_flags;  /* lower-nibble flags of the WRITE */
    bool     write_pending;
    uint16_t pending_read_len;
    bool     read_pending;

    /* Talk stream (drive -> host payload), borrowed pointer. */
    const uint8_t *talk_bytes;
    size_t         talk_len;
    size_t         talk_pos;

    /* Counters for assertions. */
    uint64_t listen_bytes_accepted;
    uint64_t bytes_tx_to_host;
    uint64_t bootloader_refusals;
    uint64_t cmd_count;

    uint16_t last_status_val;   /* extended value of the last status */
} xum_fw_t;

/* ─── Lifecycle ─────────────────────────────────────────────────────── */

/** Reset to DISCONNECTED with default identity (fw v8, CAP_CBM|CAP_NIB). */
void xum_fw_reset(xum_fw_t *fw);

/** Power-on: CONNECTED, drive 8 present, doing_reset flag armed,
 *  IEC lines idle (all released = 0). */
void xum_fw_power_on_defaults(xum_fw_t *fw);

/* Test setters — keep this surface tight; new flags = new defects. */
void xum_fw_set_firmware_version(xum_fw_t *fw, uint8_t version);
void xum_fw_set_capabilities(xum_fw_t *fw, uint8_t caps);
void xum_fw_set_drive_present(xum_fw_t *fw, bool present);
void xum_fw_set_drive_device_num(xum_fw_t *fw, uint8_t device);
void xum_fw_set_iec_lines(xum_fw_t *fw, uint8_t line_mask);
void xum_fw_set_version_strings(xum_fw_t *fw, const char *gitrev,
                                const char *gccver, const char *libcver);

/** Load the drive->host payload (from the synthetic GCR generator)
 *  that a talk-ATN + READ sequence will drain. Borrowed pointer —
 *  caller keeps it alive for the duration of the read sequence. */
void xum_fw_load_talk_stream(xum_fw_t *fw, const uint8_t *bytes, size_t len);

/* ─── Control transfers (wire layer 1) ──────────────────────────────
 * Mirrors libusb_control_transfer(0xC0, bRequest, wValue, 0, out, cap).
 * Returns bytes written to out (>= 0) or XUM_FW_CTRL_REFUSED. */
int xum_fw_ctrl(xum_fw_t *fw, uint8_t bRequest, uint16_t wValue,
                uint8_t *out, size_t out_cap);

/* ─── Bulk commands (wire layer 2) ──────────────────────────────────
 * cmd is the exact 4-byte bulk-OUT header the HAL sends:
 *   WRITE(9): [9, proto|flags, size_lo, size_hi] — payload follows,
 *             then a 3-byte status whose extended value = IEC byte
 *             count. Returns XUM_FW_NO_STATUS (payload phase next) or
 *             an immediate error status.
 *   READ(8):  [8, proto, size_lo, size_hi] — data phase follows, NO
 *             status phase on the real wire. Returns XUM_FW_NO_STATUS
 *             or an immediate sim-side error status (a real device
 *             would hang the bus instead — see DIVERGENCES.md X-D4).
 *   IOCTL(23..31): [cmd, arg1, arg2, 0] — returns the status code
 *             directly; result value in fw->last_status_val.
 * Unknown opcodes and unknown protocol nibbles are refused with a
 * sticky error — never fake success. */
uint8_t xum_fw_bulk_command(xum_fw_t *fw, const uint8_t cmd[4]);

/** Data phase after a WRITE command: host streams `len` payload bytes.
 *  Returns the deferred status. For an ATN write (FLAG_WRITE_ATN) the
 *  payload bytes are parsed as raw IEC ATN commands and drive the
 *  LISTENING/TALKING state transitions:
 *    0x20|dev -> listen, 0x40|dev -> talk (requires FLAG_WRITE_TALK),
 *    0x3F -> unlisten, 0x5F -> untalk, 0x60|sec -> secondary address.
 *  On success: READY with extended value = bytes transferred on the
 *  IEC bus; on a mid-payload failure: ERROR with extended value =
 *  bytes transferred before the failing ATN byte. */
uint8_t xum_fw_bulk_write_payload(xum_fw_t *fw,
                                  const uint8_t *data, uint16_t len);

/** Data phase after a READ command: firmware streams up to the
 *  requested length from the loaded talk stream. Writes delivered
 *  count to *out_len; sets the EOI flag when the drive ends
 *  transmission (short read = EOI, exactly as on the real wire; there
 *  is NO status phase after a read). The uint8_t return is a SIM-side
 *  result code (READY/ERROR), not wire bytes. */
uint8_t xum_fw_bulk_read_payload(xum_fw_t *fw, uint8_t *out,
                                 uint16_t out_cap, uint16_t *out_len);

/** Serialize a status phase exactly as the firmware sends it over
 *  bulk IN: [code, val_lo, val_hi] (3 bytes, little-endian value). */
void xum_fw_status_serialize(uint8_t code, uint16_t value, uint8_t out[3]);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_XUM_FIRMWARE_STATE_MACHINE_H */
