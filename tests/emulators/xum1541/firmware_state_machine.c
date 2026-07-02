/**
 * @file tests/emulators/xum1541/firmware_state_machine.c
 * @brief XUM1541 firmware-realistic state machine implementation.
 *
 * Reference: OpenCBM xum1541 firmware + host lib (BSD-2) and the
 * UFT header constants in include/uft/hal/uft_xum1541.h (MF-255).
 *
 * Sequencing rules enforced:
 *   1. CTRL_ECHO / GITREV / GCCVER / LIBCVER — legal from CONNECTED
 *      onward, incl. ERROR (host diagnoses via them).
 *   2. CTRL_INIT — legal from CONNECTED onward; enters READY, clears
 *      any sticky error and the bus role (OpenCBM re-INITs to recover).
 *      Reports (and then clears) the power-on DOING_RESET flag;
 *      reports NO_DEVICE when no drive answers the bus.
 *   3. Any bulk command before CTRL_INIT -> STATUS_ERROR + sticky.
 *   4. LISTEN/TALK need an idle bus (strict: no role switch without
 *      UNLISTEN/UNTALK — stricter than the real ATN protocol, see
 *      DIVERGENCES.md X-D5); absent device -> transient STATUS_ERROR.
 *   5. WRITE_DATA only while LISTENING; READ_DATA only while TALKING.
 *      Both defer their status to the data phase (real wire order:
 *      cmd -> payload -> status).
 *   6. Transfers larger than the firmware buffer -> sticky OVERRUN.
 *   7. CTRL_ENTER_BOOTLOADER is ALWAYS refused (bricking guard).
 *   8. CTRL_SHUTDOWN detaches: everything afterwards is refused.
 *   9. IOCTLs (GET_EOI/CLEAR_EOI/IEC_POLL/IEC_WAIT/SETRELEASE) return
 *      their result in the 16-bit status value, as OpenCBM's
 *      XUM_GET_STATUS_VAL does.
 *
 * Sticky violations park the firmware in XUM_FW_STATE_ERROR until
 * CTRL_INIT or CTRL_RESET; transient bus errors (device absent, bad
 * device number) do NOT wedge the adapter — matching real IEC, where a
 * timeout on ATN leaves the bridge usable.
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

static uint8_t status_err(xum_fw_t *fw)
{
    fw->last_status_val = 0;
    return UFT_XUM1541_STATUS_ERROR;
}

/* LISTEN/TALK shared validation. Returns READY on success. */
static uint8_t iec_address(xum_fw_t *fw, uint8_t device, bool as_talker)
{
    /* Strict role rule: the current role must be released first. */
    if ((as_talker  && fw->state == XUM_FW_STATE_LISTENING) ||
        (!as_talker && fw->state == XUM_FW_STATE_TALKING)) {
        enter_sticky_error(fw, XUM_FW_ERR_BUS_ROLE_CONFLICT);
        return status_err(fw);
    }
    /* IEC primary addresses are 0..30 (31 encodes UNLISTEN/UNTALK). */
    if (device > 30) {
        set_transient_error(fw, XUM_FW_ERR_BAD_DEVICE);
        return status_err(fw);
    }
    /* No drive at that address: ATN sequence times out. Transient. */
    if (!fw->drive_present || device != fw->drive_device_num) {
        set_transient_error(fw, XUM_FW_ERR_DEVICE_NOT_PRESENT);
        return status_err(fw);
    }
    fw->state = as_talker ? XUM_FW_STATE_TALKING : XUM_FW_STATE_LISTENING;
    return status_ok(fw, 0);
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
        case UFT_XUM1541_BULK_LISTEN:
            fw->current_secondary = arg2;
            return iec_address(fw, arg1, /*as_talker=*/false);

        case UFT_XUM1541_BULK_TALK:
            fw->current_secondary = arg2;
            return iec_address(fw, arg1, /*as_talker=*/true);

        case UFT_XUM1541_BULK_UNLISTEN:
            if (fw->state == XUM_FW_STATE_LISTENING) {
                fw->state = XUM_FW_STATE_READY;
            }
            /* From READY/TALKING: harmless ATN command, no-op. */
            return status_ok(fw, 0);

        case UFT_XUM1541_BULK_UNTALK:
            if (fw->state == XUM_FW_STATE_TALKING) {
                fw->state = XUM_FW_STATE_READY;
            }
            return status_ok(fw, 0);

        case UFT_XUM1541_BULK_WRITE_DATA: {
            if (fw->state != XUM_FW_STATE_LISTENING) {
                enter_sticky_error(fw, XUM_FW_ERR_NO_LISTENER);
                return status_err(fw);
            }
            uint16_t len = (uint16_t)(arg1 | ((uint16_t)arg2 << 8));
            if (len > UFT_XUM1541_MAX_XFER_SIZE - 4) {
                enter_sticky_error(fw, XUM_FW_ERR_OVERRUN);
                return status_err(fw);
            }
            fw->pending_write_len = len;
            fw->write_pending     = true;
            /* Real wire order: cmd -> payload -> status. No status yet. */
            return XUM_FW_NO_STATUS;
        }

        case UFT_XUM1541_BULK_READ_DATA: {
            if (fw->state != XUM_FW_STATE_TALKING) {
                enter_sticky_error(fw, XUM_FW_ERR_NO_TALKER);
                return status_err(fw);
            }
            uint16_t len = (uint16_t)(arg1 | ((uint16_t)arg2 << 8));
            if (len > UFT_XUM1541_MAX_XFER_SIZE) {
                enter_sticky_error(fw, XUM_FW_ERR_OVERRUN);
                return status_err(fw);
            }
            fw->pending_read_len = len;
            fw->read_pending     = true;
            return XUM_FW_NO_STATUS;   /* data phase follows */
        }

        case UFT_XUM1541_BULK_OPEN_FILE:
        case UFT_XUM1541_BULK_CLOSE_FILE:
            /* Channel open/close not modelled (the UFT HAL never sends
             * them). Refuse honestly rather than fake success. */
            set_transient_error(fw, XUM_FW_ERR_NOT_MODELLED);
            return status_err(fw);

        /* ── IOCTL range: result in the 16-bit status value ───────── */
        case UFT_XUM1541_IOCTL_GET_EOI:
            return status_ok(fw, fw->eoi_flag ? 1 : 0);

        case UFT_XUM1541_IOCTL_CLEAR_EOI:
            fw->eoi_flag = false;
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

        default:
            enter_sticky_error(fw, XUM_FW_ERR_BAD_OPCODE);
            return status_err(fw);
    }
}

uint8_t xum_fw_bulk_write_payload(xum_fw_t *fw,
                                  const uint8_t *data, uint16_t len)
{
    if (!fw) return XUM_FW_NO_STATUS;
    if (!fw->write_pending || fw->state != XUM_FW_STATE_LISTENING) {
        enter_sticky_error(fw, XUM_FW_ERR_NO_LISTENER);
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
    /* Payload accepted + counted; never persisted (no emulated medium,
     * nothing to silently corrupt). */
    fw->listen_bytes_accepted += len;
    return status_ok(fw, len);
}

uint8_t xum_fw_bulk_read_payload(xum_fw_t *fw, uint8_t *out,
                                 uint16_t out_cap, uint16_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!fw || !out || !out_len) return XUM_FW_NO_STATUS;
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
     * whenever this transfer exhausts the stream. If no stream was
     * loaded at all we deliver 0 bytes with EOI; honest empty answer,
     * never invented bytes. */
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
