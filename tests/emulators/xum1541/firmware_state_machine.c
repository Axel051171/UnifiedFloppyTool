/**
 * @file tests/emulators/xum1541/firmware_state_machine.c
 * @brief XUM1541 firmware-realistic state machine implementation.
 *
 * Reference (MF-301 OpenCBM-verified): xum1541/xum1541_types.h,
 * opencbm/lib/plugin/xum1541/xum1541.c, archlib.c (BSD-2) and the UFT
 * header constants in include/uft/hal/uft_xum1541.h.
 *
 * Sequencing rules enforced:
 *   1. CTRL_ECHO / GITREV / GCCVER / LIBCVER — legal from CONNECTED
 *      onward, incl. ERROR (host diagnoses via them).
 *   2. CTRL_INIT — legal from CONNECTED onward; enters READY, clears
 *      any sticky error and the bus role (OpenCBM re-INITs to recover).
 *      Reports (and then clears) the power-on DOING_RESET flag;
 *      reports NO_DEVICE when no drive answers the bus.
 *   3. Any bulk command before CTRL_INIT -> STATUS_ERROR + sticky.
 *   4. The ONLY bulk data opcodes are READ(8) and WRITE(9); IEC
 *      addressing travels as WRITE with FLAG_WRITE_ATN and raw ATN
 *      bytes as payload. Anything else in the data range -> sticky
 *      BAD_OPCODE. Unknown protocol nibbles -> sticky BAD_PROTOCOL.
 *   5. ATN payload parsing drives the bus role:
 *      0x20|dev -> LISTENING, 0x40|dev -> TALKING (needs FLAG_WRITE_
 *      TALK for the bus turnaround), 0x3F -> unlisten, 0x5F -> untalk,
 *      0x60|sec -> secondary. Strict role rule: cross-role addressing
 *      without release -> sticky BUS_ROLE_CONFLICT (stricter than the
 *      real ATN protocol, DIVERGENCES.md X-D5). Absent device ->
 *      transient STATUS_ERROR with extended value = bytes clocked
 *      before the failing ATN byte.
 *   6. Plain (non-ATN) CBM data WRITE only while LISTENING; READ only
 *      while TALKING. WRITE defers its status to the payload phase
 *      (real wire order: cmd -> payload -> status). READ has NO status
 *      phase at all — short read = IEC EOI.
 *   7. Transfers larger than the firmware buffer -> sticky OVERRUN.
 *   8. CTRL_ENTER_BOOTLOADER is ALWAYS refused (bricking guard).
 *   9. CTRL_SHUTDOWN detaches: everything afterwards is refused.
 *  10. IOCTLs (23..31) return their result in the 16-bit status value,
 *      as OpenCBM's XUM_GET_STATUS_VAL does. PARBURST_* need a
 *      drive-side nibbler routine -> honest NOT_MODELLED refusal.
 *
 * Sticky violations park the firmware in XUM_FW_STATE_ERROR until
 * CTRL_INIT or CTRL_RESET; transient bus errors (device absent) do NOT
 * wedge the adapter — matching real IEC, where a timeout on ATN leaves
 * the bridge usable.
 */

#include "firmware_state_machine.h"

#include <string.h>

/* ─── Internal helpers ──────────────────────────────────────────────── */

static void enter_sticky_error(xum_fw_t *fw, xum_fw_err_reason_t reason)
{
    fw->state        = XUM_FW_STATE_ERROR;
    fw->sticky_error = reason;
    fw->last_error   = reason;
}

static void set_transient_error(xum_fw_t *fw, xum_fw_err_reason_t reason)
{
    fw->last_error = reason;   /* state unchanged — adapter stays usable */
}

static void copy_verstr(char *dst, const char *src)
{
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, XUM_FW_VERSTR_MAX - 1);
    dst[XUM_FW_VERSTR_MAX - 1] = '\0';
}

/* ─── Lifecycle ─────────────────────────────────────────────────────── */

void xum_fw_reset(xum_fw_t *fw)
{
    if (!fw) return;
    /* Talk-stream pointer is borrowed — zeroing is safe, caller's
     * buffer outlives this reset. */
    memset(fw, 0, sizeof(*fw));
    fw->state            = XUM_FW_STATE_DISCONNECTED;
    fw->fw_version       = XUM_FW_VERSION_DEFAULT;
    fw->capabilities     = XUM_FW_CAP_CBM | XUM_FW_CAP_NIB;
    fw->drive_device_num = 8;
    copy_verstr(fw->gitrev,  "xum1541-sim");
    copy_verstr(fw->gccver,  "gcc-sim");
    copy_verstr(fw->libcver, "avr-libc-sim");
}

void xum_fw_power_on_defaults(xum_fw_t *fw)
{
    xum_fw_reset(fw);
    fw->state            = XUM_FW_STATE_CONNECTED;
    fw->drive_present    = true;
    fw->doing_reset_flag = true;   /* AVR just booted */
    fw->iec_lines        = 0;      /* all lines released */
}

void xum_fw_set_firmware_version(xum_fw_t *fw, uint8_t version)
{ if (fw) fw->fw_version = version; }

void xum_fw_set_capabilities(xum_fw_t *fw, uint8_t caps)
{ if (fw) fw->capabilities = caps; }

void xum_fw_set_drive_present(xum_fw_t *fw, bool present)
{ if (fw) fw->drive_present = present; }

void xum_fw_set_drive_device_num(xum_fw_t *fw, uint8_t device)
{ if (fw) fw->drive_device_num = device; }

void xum_fw_set_iec_lines(xum_fw_t *fw, uint8_t line_mask)
{ if (fw) fw->iec_lines = line_mask; }

void xum_fw_set_version_strings(xum_fw_t *fw, const char *gitrev,
                                const char *gccver, const char *libcver)
{
    if (!fw) return;
    copy_verstr(fw->gitrev, gitrev);
    copy_verstr(fw->gccver, gccver);
    copy_verstr(fw->libcver, libcver);
}

void xum_fw_load_talk_stream(xum_fw_t *fw, const uint8_t *bytes, size_t len)
{
    if (!fw) return;
    fw->talk_bytes = bytes;
    fw->talk_len   = len;
    fw->talk_pos   = 0;
}

/* ─── Control transfers ─────────────────────────────────────────────── */

static int ctrl_string_reply(const char *s, uint8_t *out, size_t out_cap)
{
    size_t n = strlen(s);
    if (n > out_cap) n = out_cap;
    memcpy(out, s, n);
    return (int)n;
}

int xum_fw_ctrl(xum_fw_t *fw, uint8_t bRequest, uint16_t wValue,
                uint8_t *out, size_t out_cap)
{
    if (!fw) return XUM_FW_CTRL_REFUSED;
    fw->cmd_count++;

    if (fw->state == XUM_FW_STATE_DISCONNECTED) return XUM_FW_CTRL_REFUSED;
    if (fw->state == XUM_FW_STATE_SHUTDOWN)     return XUM_FW_CTRL_REFUSED;

    switch (bRequest) {
        case UFT_XUM1541_CTRL_ECHO:
            /* Echo test — returns wValue LE16. Legal even in ERROR. */
            if (!out || out_cap < 2) return XUM_FW_CTRL_REFUSED;
            out[0] = (uint8_t)(wValue & 0xFF);
            out[1] = (uint8_t)(wValue >> 8);
            return 2;

        case UFT_XUM1541_CTRL_INIT: {
            /* 8-byte device info: [version, capabilities, status, 0...].
             * Layout per OpenCBM host lib (devInfo[0..2]); bytes 3..7
             * reserved-zero. INIT also clears sticky errors and the bus
             * role — it is OpenCBM's documented recovery path. */
            if (!out || out_cap < 8) return XUM_FW_CTRL_REFUSED;
            memset(out, 0, 8);
            out[0] = fw->fw_version;
            out[1] = fw->capabilities;
            uint8_t flags = 0;
            if (fw->doing_reset_flag) flags |= XUM_FW_INIT_DOING_RESET;
            if (!fw->drive_present)   flags |= XUM_FW_INIT_NO_DEVICE;
            out[2] = flags;
            fw->doing_reset_flag = false;   /* reported exactly once */
            fw->state         = XUM_FW_STATE_READY;
            fw->sticky_error  = XUM_FW_ERR_NONE;
            fw->eoi_flag      = false;
            fw->write_pending = false;
            fw->read_pending  = false;
            fw->bytes_tx_to_host += 8;
            return 8;
        }

        case UFT_XUM1541_CTRL_RESET:
            /* IEC bus reset (pulls the RESET line; drives reboot).
             * Requires a prior INIT — inferred from OpenCBM call order,
             * see DIVERGENCES.md X-D6. Clears sticky error + bus role. */
            if (fw->state == XUM_FW_STATE_CONNECTED) return XUM_FW_CTRL_REFUSED;
            fw->state         = XUM_FW_STATE_READY;
            fw->sticky_error  = XUM_FW_ERR_NONE;
            fw->eoi_flag      = false;
            fw->write_pending = false;
            fw->read_pending  = false;
            fw->iec_lines     = 0;
            return 0;

        case UFT_XUM1541_CTRL_SHUTDOWN:
            fw->state = XUM_FW_STATE_SHUTDOWN;
            fw->last_error = XUM_FW_ERR_SHUTDOWN;
            if (out && out_cap >= 1) { out[0] = 0; return 1; }
            return 0;

        case UFT_XUM1541_CTRL_ENTER_BOOTLOADER:
            /* NEVER emulated: a wrong bootloader entry on real hardware
             * can leave the AVR unbootable. Forensic-safety guard. */
            fw->bootloader_refusals++;
            return XUM_FW_CTRL_REFUSED;

        case UFT_XUM1541_CTRL_TAP_BREAK:
            /* Tape mode not modelled — refuse honestly. */
            set_transient_error(fw, XUM_FW_ERR_NOT_MODELLED);
            return XUM_FW_CTRL_REFUSED;

        case UFT_XUM1541_CTRL_GITREV:
            if (!out || out_cap == 0) return XUM_FW_CTRL_REFUSED;
            return ctrl_string_reply(fw->gitrev, out, out_cap);
        case UFT_XUM1541_CTRL_GCCVER:
            if (!out || out_cap == 0) return XUM_FW_CTRL_REFUSED;
            return ctrl_string_reply(fw->gccver, out, out_cap);
        case UFT_XUM1541_CTRL_LIBCVER:
            if (!out || out_cap == 0) return XUM_FW_CTRL_REFUSED;
            return ctrl_string_reply(fw->libcver, out, out_cap);

        default:
            set_transient_error(fw, XUM_FW_ERR_BAD_OPCODE);
            return XUM_FW_CTRL_REFUSED;
    }
}

/* ─── Bulk commands ─────────────────────────────────────────────────── */

static uint8_t status_ok(xum_fw_t *fw, uint16_t value)
{
    fw->last_status_val = value;
    return UFT_XUM1541_STATUS_READY;
}

static uint8_t status_err_val(xum_fw_t *fw, uint16_t value)
{
    fw->last_status_val = value;
    return UFT_XUM1541_STATUS_ERROR;
}

static uint8_t status_err(xum_fw_t *fw)
{
    return status_err_val(fw, 0);
}

/** Validate the protocol nibble of a READ/WRITE header byte 1.
 *  Returns XUM_FW_NO_STATUS if the command may proceed, else the error
 *  status to return. Only CBM is modelled; the real S1/S2/PP/P2/NIB
 *  protocols are refused honestly (DIVERGENCES.md X-D13). */
static uint8_t check_proto(xum_fw_t *fw, uint8_t proto)
{
    switch (proto) {
        case UFT_XUM1541_PROTO_CBM:
            return XUM_FW_NO_STATUS;
        case UFT_XUM1541_PROTO_S1:
        case UFT_XUM1541_PROTO_S2:
        case UFT_XUM1541_PROTO_PP:
        case UFT_XUM1541_PROTO_P2:
        case UFT_XUM1541_PROTO_NIB:
            /* Real protocols this sim does not model — transient honest
             * refusal, never fake success. */
            set_transient_error(fw, XUM_FW_ERR_NOT_MODELLED);
            return status_err(fw);
        default:
            /* Nibble the real firmware does not know either. */
            enter_sticky_error(fw, XUM_FW_ERR_BAD_PROTOCOL);
            return status_err(fw);
    }
}

uint8_t xum_fw_bulk_command(xum_fw_t *fw, const uint8_t cmd[4])
{
    if (!fw || !cmd) return XUM_FW_NO_STATUS;
    fw->cmd_count++;

    if (fw->state == XUM_FW_STATE_DISCONNECTED ||
        fw->state == XUM_FW_STATE_SHUTDOWN) {
        return XUM_FW_NO_STATUS;   /* device cannot answer */
    }
    if (fw->state == XUM_FW_STATE_CONNECTED) {
        enter_sticky_error(fw, XUM_FW_ERR_NOT_INITIALIZED);
        return status_err(fw);
    }
    if (fw->state == XUM_FW_STATE_ERROR) {
        return status_err(fw);     /* sticky until INIT / RESET */
    }

    const uint8_t opcode = cmd[0];
    const uint8_t arg1 = cmd[1], arg2 = cmd[2];

    switch (opcode) {
        case UFT_XUM1541_BULK_WRITE: {
            /* [WRITE, proto|flags, size_lo, size_hi] */
            const uint8_t proto = (uint8_t)(cmd[1] & 0xF0);
            const uint8_t flags = (uint8_t)(cmd[1] & 0x0F);
            uint8_t rc = check_proto(fw, proto);
            if (rc != XUM_FW_NO_STATUS) return rc;
            uint16_t len = (uint16_t)(cmd[2] | ((uint16_t)cmd[3] << 8));
            if (len > UFT_XUM1541_MAX_XFER_SIZE) {
                enter_sticky_error(fw, XUM_FW_ERR_OVERRUN);
                return status_err(fw);
            }
            /* A plain data write needs an addressed listener. ATN writes
             * are legal from any bus role (ATN overrides). Sim reports
             * this at the command phase; real firmware would swallow the
             * payload first (X-D4). */
            if (!(flags & UFT_XUM1541_FLAG_WRITE_ATN) &&
                fw->state != XUM_FW_STATE_LISTENING) {
                enter_sticky_error(fw, XUM_FW_ERR_NO_LISTENER);
                return status_err(fw);
            }
            fw->pending_write_len   = len;
            fw->pending_write_flags = flags;
            fw->write_pending       = true;
            /* Real wire order: cmd -> payload -> status. No status yet. */
            return XUM_FW_NO_STATUS;
        }

        case UFT_XUM1541_BULK_READ: {
            /* [READ, proto, size_lo, size_hi] — flags unused on reads. */
            const uint8_t proto = (uint8_t)(cmd[1] & 0xF0);
            uint8_t rc = check_proto(fw, proto);
            if (rc != XUM_FW_NO_STATUS) return rc;
            uint16_t len = (uint16_t)(cmd[2] | ((uint16_t)cmd[3] << 8));
            if (len > UFT_XUM1541_MAX_XFER_SIZE) {
                enter_sticky_error(fw, XUM_FW_ERR_OVERRUN);
                return status_err(fw);
            }
            if (fw->state != XUM_FW_STATE_TALKING) {
                /* Real firmware would clock a bus with no talker and
                 * hang/time out; the sim fails loudly instead (X-D4). */
                enter_sticky_error(fw, XUM_FW_ERR_NO_TALKER);
                return status_err(fw);
            }
            fw->pending_read_len = len;
            fw->read_pending     = true;
            return XUM_FW_NO_STATUS;   /* data phase follows, NO status */
        }

        /* ── IOCTL range: result in the 16-bit status value ───────── */
        case UFT_XUM1541_IOCTL_GET_EOI:
            return status_ok(fw, fw->eoi_flag ? 1 : 0);

        case UFT_XUM1541_IOCTL_CLEAR_EOI:
            fw->eoi_flag = false;
            return status_ok(fw, 0);

        case UFT_XUM1541_IOCTL_PP_READ:
            return status_ok(fw, fw->pp_port);

        case UFT_XUM1541_IOCTL_PP_WRITE:
            fw->pp_port = arg1;
            return status_ok(fw, 0);

        case UFT_XUM1541_IOCTL_IEC_POLL:
            return status_ok(fw, fw->iec_lines);

        case UFT_XUM1541_IOCTL_IEC_WAIT: {
            /* arg1 = line mask, arg2 = wanted state (nonzero = set).
             * No time model in this sim: if the condition already
             * holds -> READY; else the firmware would block -> BUSY
             * (see DIVERGENCES.md X-D7). */
            bool is_set = (fw->iec_lines & arg1) != 0;
            bool want   = (arg2 != 0);
            if (is_set == want) return status_ok(fw, fw->iec_lines);
            fw->last_status_val = 0;
            return UFT_XUM1541_STATUS_BUSY;
        }

        case UFT_XUM1541_IOCTL_IEC_SETRELEASE:
            /* arg1 = lines to assert, arg2 = lines to release. */
            fw->iec_lines = (uint8_t)((fw->iec_lines | arg1) & (uint8_t)~arg2);
            return status_ok(fw, fw->iec_lines);

        case UFT_XUM1541_IOCTL_PARBURST_READ:
        case UFT_XUM1541_IOCTL_PARBURST_WRITE:
            /* Parallel-burst nibbler transfers need a drive-side routine
             * this sim does not model (X-D13). Honest refusal. */
            set_transient_error(fw, XUM_FW_ERR_NOT_MODELLED);
            return status_err(fw);

        default:
            /* Includes every pre-MF-301 FICTIONAL opcode (TALK=1,
             * LISTEN=2, ... OPEN=8-as-file, CLOSE=9-as-file): the real
             * firmware knows only READ(8)/WRITE(9)/ioctls. */
            enter_sticky_error(fw, XUM_FW_ERR_BAD_OPCODE);
            return status_err(fw);
    }
}

/** Parse an ATN payload byte stream and drive the bus-role state.
 *  Returns the status code; extended value = ATN bytes clocked onto
 *  the bus (all of them on success, the count before the failing byte
 *  on error — mirroring the WRITE byte-count semantics). */
static uint8_t process_atn_payload(xum_fw_t *fw, const uint8_t *data,
                                   uint16_t len, bool talk_flag)
{
    uint16_t done = 0;
    for (uint16_t i = 0; i < len; i++) {
        const uint8_t b = data[i];
        if (b == UFT_IEC_UNLISTEN) {          /* 0x3F — NOT device 31 */
            if (fw->state == XUM_FW_STATE_LISTENING)
                fw->state = XUM_FW_STATE_READY;
        } else if (b == UFT_IEC_UNTALK) {     /* 0x5F — NOT device 31 */
            if (fw->state == XUM_FW_STATE_TALKING)
                fw->state = XUM_FW_STATE_READY;
        } else if ((b & 0xE0) == UFT_IEC_LISTEN) {   /* 0x20|dev, dev 0..30 */
            const uint8_t dev = (uint8_t)(b & 0x1F);
            if (fw->state == XUM_FW_STATE_TALKING) {
                /* Strict role rule (X-D5): release the talker first. */
                enter_sticky_error(fw, XUM_FW_ERR_BUS_ROLE_CONFLICT);
                return status_err_val(fw, done);
            }
            if (!fw->drive_present || dev != fw->drive_device_num) {
                set_transient_error(fw, XUM_FW_ERR_DEVICE_NOT_PRESENT);
                return status_err_val(fw, done);
            }
            fw->state = XUM_FW_STATE_LISTENING;
        } else if ((b & 0xE0) == UFT_IEC_TALK) {     /* 0x40|dev, dev 0..30 */
            const uint8_t dev = (uint8_t)(b & 0x1F);
            if (!talk_flag) {
                /* Without FLAG_WRITE_TALK the firmware skips the bus
                 * turnaround and the bus wedges on real HW — the sim
                 * fails loudly instead (X-D12). */
                enter_sticky_error(fw, XUM_FW_ERR_BAD_PROTOCOL);
                return status_err_val(fw, done);
            }
            if (fw->state == XUM_FW_STATE_LISTENING) {
                enter_sticky_error(fw, XUM_FW_ERR_BUS_ROLE_CONFLICT);
                return status_err_val(fw, done);
            }
            if (!fw->drive_present || dev != fw->drive_device_num) {
                set_transient_error(fw, XUM_FW_ERR_DEVICE_NOT_PRESENT);
                return status_err_val(fw, done);
            }
            fw->state = XUM_FW_STATE_TALKING;
        } else if ((b & 0xE0) == UFT_IEC_DATA) {     /* 0x60|sec */
            fw->current_secondary = (uint8_t)(b & 0x1F);
        } else {
            /* OPEN (0xF0|sec) / CLOSE (0xE0|sec) / other ATN bytes:
             * the firmware clocks them to the drive verbatim; the
             * drive-side file semantics are not modelled (X-D2). The
             * byte still counts as transferred. */
        }
        done++;
    }
    return status_ok(fw, done);
}

uint8_t xum_fw_bulk_write_payload(xum_fw_t *fw,
                                  const uint8_t *data, uint16_t len)
{
    if (!fw) return XUM_FW_NO_STATUS;
    if (fw->state == XUM_FW_STATE_DISCONNECTED ||
        fw->state == XUM_FW_STATE_SHUTDOWN) {
        return XUM_FW_NO_STATUS;
    }
    if (fw->state == XUM_FW_STATE_ERROR) {
        return status_err(fw);     /* sticky — keep the original reason */
    }
    if (!fw->write_pending) {
        /* Payload without a preceding WRITE command — the firmware
         * would misparse it as a command header. Sticky. */
        enter_sticky_error(fw, XUM_FW_ERR_BAD_PROTOCOL);
        return status_err(fw);
    }
    fw->write_pending = false;
    if (!data && len > 0) return status_err(fw);
    if (len != fw->pending_write_len) {
        /* Host announced one length, sent another — firmware FIFO
         * accounting breaks. Sticky. */
        enter_sticky_error(fw, XUM_FW_ERR_OVERRUN);
        return status_err(fw);
    }

    if (fw->pending_write_flags & UFT_XUM1541_FLAG_WRITE_ATN) {
        return process_atn_payload(fw, data, len,
            (fw->pending_write_flags & UFT_XUM1541_FLAG_WRITE_TALK) != 0);
    }

    /* Plain CBM data write: payload accepted + counted; never persisted
     * (no emulated medium, nothing to silently corrupt). The status
     * extended value is the IEC byte count — all bytes here, since the
     * sim listener never stalls mid-transfer. FLAG_WRITE_TALK without
     * ATN only affects the bus turnaround timing on real HW; ignored. */
    fw->listen_bytes_accepted += len;
    return status_ok(fw, len);
}

uint8_t xum_fw_bulk_read_payload(xum_fw_t *fw, uint8_t *out,
                                 uint16_t out_cap, uint16_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!fw || !out || !out_len) return XUM_FW_NO_STATUS;
    if (fw->state == XUM_FW_STATE_ERROR) {
        return status_err(fw);
    }
    if (!fw->read_pending || fw->state != XUM_FW_STATE_TALKING) {
        enter_sticky_error(fw, XUM_FW_ERR_NO_TALKER);
        return status_err(fw);
    }
    fw->read_pending = false;

    uint16_t want = fw->pending_read_len;
    if (want > out_cap) want = out_cap;

    size_t remaining = (fw->talk_bytes && fw->talk_pos < fw->talk_len)
                           ? fw->talk_len - fw->talk_pos : 0;
    uint16_t deliver = (remaining < want) ? (uint16_t)remaining : want;
    if (deliver > 0) {
        memcpy(out, fw->talk_bytes + fw->talk_pos, deliver);
        fw->talk_pos += deliver;
        fw->bytes_tx_to_host += deliver;
    }
    /* Drive signals EOI on the final byte of its transmission — i.e.
     * whenever this transfer exhausts the stream. On the real wire this
     * is exactly the short read (no status phase follows a READ). If no
     * stream was loaded at all we deliver 0 bytes with EOI; honest
     * empty answer, never invented bytes. */
    if (fw->talk_pos >= fw->talk_len) fw->eoi_flag = true;
    *out_len = deliver;
    return status_ok(fw, deliver);
}

void xum_fw_status_serialize(uint8_t code, uint16_t value, uint8_t out[3])
{
    /* OpenCBM XUM_STATUSBUF_SIZE = 3: [status, val_lo, val_hi]. */
    out[0] = code;
    out[1] = (uint8_t)(value & 0xFF);
    out[2] = (uint8_t)(value >> 8);
}
