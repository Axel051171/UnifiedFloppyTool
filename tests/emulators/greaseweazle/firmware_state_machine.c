/**
 * @file tests/emulators/greaseweazle/firmware_state_machine.c
 * @brief GW firmware-realistic state machine implementation.
 *
 * Reference: keirf/greaseweazle v1.23 usb.py + src/hal/uft_greaseweazle_full.c
 * (the production-tested C HAL — treated as ground-truth behavioural spec).
 *
 * Sequencing rules enforced:
 *   1. Cmd.GetInfo  → always answered (even in ERROR / DISCONNECTED).
 *   2. Cmd.SetBusType must precede Cmd.Select (Select returns NO_BUS
 *      otherwise; HAL auto-sets IBM_PC if bus_type == NONE, but at the
 *      firmware layer we refuse without explicit configuration).
 *   3. Cmd.Select(unit) only accepts unit ≤ 3 (BAD_UNIT otherwise).
 *   4. Cmd.Motor requires a Select first (NO_UNIT otherwise).
 *   5. Cmd.Seek requires Motor on (BAD_COMMAND otherwise — firmware
 *      cannot physically step heads without spindle running).
 *   6. Cmd.Seek to cyl 0 verifies TRK0 pin afterwards (NO_TRK0).
 *   7. Cmd.ReadFlux requires SEEKED state + disk + (index pulses if
 *      index_sync) — fires NO_INDEX otherwise.
 *   8. Cmd.WriteFlux and Cmd.EraseFlux ALWAYS refused → ACK_WRPROT,
 *      regardless of write_protected flag. Forensic-safety guard.
 *   9. Cmd.GetFluxStatus may be issued any time; returns the sticky
 *      status from the last read/write attempt (defaulting to OK).
 *
 * Sequencing violations transition to GW_FW_STATE_ERROR with a sticky
 * error byte. Cmd.Reset clears it.
 */

#include "firmware_state_machine.h"

#include <string.h>

/* ─── Internal helpers ─────────────────────────────────────────────── */

static void gw_fw_enter_error(gw_fw_t *fw, gw_fw_ack_t reason)
{
    fw->state         = GW_FW_STATE_ERROR;
    fw->sticky_error  = reason;
}

static void gw_fw_count_cmd(gw_fw_t *fw)
{
    fw->cmd_count++;
}

/* Little-endian 32-bit write — GW protocol is LE throughout. */
static void le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >>  8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* ─── Lifecycle ─────────────────────────────────────────────────────── */

void gw_fw_reset(gw_fw_t *fw)
{
    if (!fw) return;
    /* Forensic invariant: the emulator never owns the read-stream buffer
     * (gw_fw_load_read_stream stores a borrowed pointer). Zeroing the
     * whole struct is safe — caller's buffer outlives this reset. */
    memset(fw, 0, sizeof(*fw));
    fw->state           = GW_FW_STATE_DISCONNECTED;
    fw->selected_unit   = 0xFF;
    fw->current_cyl     = -1;
    fw->sample_freq     = 72000000u; /* F7 default */
    fw->fw_major        = 1;
    fw->fw_minor        = 23;
    fw->is_main_fw      = 1;
    fw->hw_model        = 1; /* F7 */
}

void gw_fw_power_on_defaults(gw_fw_t *fw)
{
    gw_fw_reset(fw);
    fw->state              = GW_FW_STATE_CONNECTED;
    fw->disk_present       = true;
    fw->write_protected    = false;
    fw->trk0_present       = true;
    fw->index_pulses_fire  = true;
}

void gw_fw_set_firmware_version(gw_fw_t *fw, uint8_t major, uint8_t minor)
{
    if (!fw) return;
    fw->fw_major = major;
    fw->fw_minor = minor;
}

void gw_fw_set_bootloader_mode(gw_fw_t *fw, bool bootloader)
{
    if (!fw) return;
    fw->is_main_fw = bootloader ? 0 : 1;
}

void gw_fw_set_hardware_model(gw_fw_t *fw, uint8_t model, uint8_t submodel)
{
    if (!fw) return;
    fw->hw_model    = model;
    fw->hw_submodel = submodel;
}

void gw_fw_set_sample_freq(gw_fw_t *fw, uint32_t hz)
{
    if (!fw) return;
    fw->sample_freq = hz;
}

void gw_fw_set_disk_present(gw_fw_t *fw, bool present)
{ if (fw) fw->disk_present = present; }

void gw_fw_set_write_protected(gw_fw_t *fw, bool wp)
{ if (fw) fw->write_protected = wp; }

void gw_fw_set_trk0_present(gw_fw_t *fw, bool present)
{ if (fw) fw->trk0_present = present; }

void gw_fw_set_index_pulses_fire(gw_fw_t *fw, bool fire)
{ if (fw) fw->index_pulses_fire = fire; }

void gw_fw_load_read_stream(gw_fw_t *fw,
                             const uint8_t *bytes, size_t len)
{
    if (!fw) return;
    fw->read_stream_bytes = bytes;
    fw->read_stream_len   = len;
    fw->read_stream_pos   = 0;
}

/* ─── Command implementations ──────────────────────────────────────── */

gw_fw_ack_t gw_fw_cmd_get_info(gw_fw_t *fw, uint8_t subindex,
                                uint8_t *out, size_t out_cap)
{
    gw_fw_count_cmd(fw);
    /* GetInfo is always legal — even from DISCONNECTED (host probes the
     * device this way) and ERROR (host diagnoses via GetInfo). */
    if (!out || out_cap < 32) return GW_FW_ACK_BAD_COMMAND;
    /* Only subindex 0 (Firmware) modelled; others would be BAD_COMMAND
     * on real firmware unless implemented. */
    if (subindex != 0) return GW_FW_ACK_BAD_COMMAND;

    /* 32-byte info response per usb.py:
     *   struct.unpack("<4BI4B3H14x", ser.read(32))
     *   = fw_major, fw_minor, is_main_fw, max_cmd,
     *     sample_freq:u32, hw_model, hw_submodel, usb_speed, _pad,
     *     3×u16 reserved, 14 bytes pad */
    memset(out, 0, 32);
    out[0] = fw->fw_major;
    out[1] = fw->fw_minor;
    out[2] = fw->is_main_fw;
    out[3] = 0x22;                /* max_cmd: arbitrary, matches v1.23 */
    le32(&out[4], fw->sample_freq);
    out[8]  = fw->hw_model;
    out[9]  = fw->hw_submodel;
    out[10] = fw->usb_speed;
    /* rest left zero */

    /* DISCONNECTED → CONNECTED is the documented transition that
     * uft_gw_open() relies on. We mirror that here. */
    if (fw->state == GW_FW_STATE_DISCONNECTED) {
        fw->state = GW_FW_STATE_CONNECTED;
    }
    fw->bytes_tx_to_host += 32;
    return GW_FW_ACK_OK;
}

gw_fw_ack_t gw_fw_cmd_set_bus_type(gw_fw_t *fw, gw_fw_bus_type_t bus_type)
{
    gw_fw_count_cmd(fw);
    if (fw->state == GW_FW_STATE_ERROR) return fw->sticky_error;
    if (fw->state == GW_FW_STATE_DISCONNECTED) {
        gw_fw_enter_error(fw, GW_FW_ACK_BAD_COMMAND);
        return GW_FW_ACK_BAD_COMMAND;
    }
    if (bus_type != GW_FW_BUS_IBM_PC && bus_type != GW_FW_BUS_SHUGART) {
        /* GW_FW_BUS_NONE or out-of-range — refuse (real firmware rejects
         * with BAD_COMMAND for unknown bus types). */
        gw_fw_enter_error(fw, GW_FW_ACK_BAD_COMMAND);
        return GW_FW_ACK_BAD_COMMAND;
    }
    fw->bus_type = bus_type;
    if (fw->state == GW_FW_STATE_CONNECTED) {
        fw->state = GW_FW_STATE_BUS_CONFIGURED;
    }
    return GW_FW_ACK_OK;
}

gw_fw_ack_t gw_fw_cmd_select(gw_fw_t *fw, uint8_t unit)
{
    gw_fw_count_cmd(fw);
    if (fw->state == GW_FW_STATE_ERROR) return fw->sticky_error;
    if (fw->state == GW_FW_STATE_DISCONNECTED) {
        gw_fw_enter_error(fw, GW_FW_ACK_BAD_COMMAND);
        return GW_FW_ACK_BAD_COMMAND;
    }
    if (fw->bus_type == GW_FW_BUS_NONE) {
        /* Reference firmware refuses with NO_BUS. The HAL auto-recovers
         * by setting bus_type to IBM_PC; the firmware layer does not. */
        gw_fw_enter_error(fw, GW_FW_ACK_NO_BUS);
        return GW_FW_ACK_NO_BUS;
    }
    if (unit > 3) {
        gw_fw_enter_error(fw, GW_FW_ACK_BAD_UNIT);
        return GW_FW_ACK_BAD_UNIT;
    }
    fw->selected_unit = unit;
    if (fw->state == GW_FW_STATE_BUS_CONFIGURED ||
        fw->state == GW_FW_STATE_CONNECTED) {
        fw->state = GW_FW_STATE_DRIVE_SELECTED;
    }
    return GW_FW_ACK_OK;
}

gw_fw_ack_t gw_fw_cmd_deselect(gw_fw_t *fw)
{
    gw_fw_count_cmd(fw);
    if (fw->state == GW_FW_STATE_ERROR) return fw->sticky_error;
    fw->selected_unit = 0xFF;
    fw->motor_on      = false;
    /* Deselect collapses to BUS_CONFIGURED — bus type stays set. */
    if (fw->state >= GW_FW_STATE_DRIVE_SELECTED) {
        fw->state = GW_FW_STATE_BUS_CONFIGURED;
    }
    return GW_FW_ACK_OK;
}

gw_fw_ack_t gw_fw_cmd_motor(gw_fw_t *fw, uint8_t unit, bool on)
{
    gw_fw_count_cmd(fw);
    if (fw->state == GW_FW_STATE_ERROR) return fw->sticky_error;
    if (fw->state < GW_FW_STATE_DRIVE_SELECTED ||
        fw->selected_unit == 0xFF) {
        gw_fw_enter_error(fw, GW_FW_ACK_NO_UNIT);
        return GW_FW_ACK_NO_UNIT;
    }
    if (unit != fw->selected_unit) {
        /* Real firmware allows Motor on any-unit independently; UFT
         * passes the selected unit. We mirror the strict (paranoid)
         * stance — mismatch → BAD_UNIT. */
        gw_fw_enter_error(fw, GW_FW_ACK_BAD_UNIT);
        return GW_FW_ACK_BAD_UNIT;
    }
    fw->motor_on = on;
    if (on) {
        if (fw->state == GW_FW_STATE_DRIVE_SELECTED) {
            fw->state = GW_FW_STATE_MOTOR_ON;
        }
    } else {
        /* Motor off collapses to DRIVE_SELECTED (we keep current_cyl). */
        if (fw->state >= GW_FW_STATE_MOTOR_ON) {
            fw->state = GW_FW_STATE_DRIVE_SELECTED;
        }
    }
    return GW_FW_ACK_OK;
}

gw_fw_ack_t gw_fw_cmd_seek(gw_fw_t *fw, int8_t cylinder)
{
    gw_fw_count_cmd(fw);
    if (fw->state == GW_FW_STATE_ERROR) return fw->sticky_error;
    if (!fw->motor_on) {
        /* Firmware refuses Seek with motor off — heads cannot step
         * reliably without the spindle spinning. */
        gw_fw_enter_error(fw, GW_FW_ACK_BAD_COMMAND);
        return GW_FW_ACK_BAD_COMMAND;
    }
    /* Signed range: -128..+127 modelled. Real firmware refuses anything
     * outside the device's physical cylinder count, but we mirror the
     * generous protocol bound here (cylinder bound enforcement is the
     * HAL's job — see UFT_GW_MAX_CYLINDERS = 85). */
    if (cylinder > 84) {
        gw_fw_enter_error(fw, GW_FW_ACK_BAD_CYLINDER);
        return GW_FW_ACK_BAD_CYLINDER;
    }
    if (cylinder == 0 && !fw->trk0_present) {
        /* TRK0 verification: the production HAL calls get_pin(26) after
         * Seek(0). If pin is not active-low (=> trk0 absent) it tries
         * NoClickStep + recheck. We model the first-shot fail here; a
         * subsequent gw_fw_cmd_no_click_step() can recover. */
        gw_fw_enter_error(fw, GW_FW_ACK_NO_TRK0);
        return GW_FW_ACK_NO_TRK0;
    }
    fw->current_cyl = (int16_t)cylinder;
    fw->state       = GW_FW_STATE_SEEKED;
    return GW_FW_ACK_OK;
}

gw_fw_ack_t gw_fw_cmd_head(gw_fw_t *fw, uint8_t head)
{
    gw_fw_count_cmd(fw);
    if (fw->state == GW_FW_STATE_ERROR) return fw->sticky_error;
    if (head > 1) {
        gw_fw_enter_error(fw, GW_FW_ACK_BAD_PIN);
        return GW_FW_ACK_BAD_PIN;
    }
    /* Head select is documented as legal independent of seek state
     * (the HAL fires Head right after Seek; firmware accepts whenever). */
    if (fw->state < GW_FW_STATE_DRIVE_SELECTED) {
        gw_fw_enter_error(fw, GW_FW_ACK_NO_UNIT);
        return GW_FW_ACK_NO_UNIT;
    }
    fw->current_head = head;
    return GW_FW_ACK_OK;
}

gw_fw_ack_t gw_fw_cmd_get_pin(gw_fw_t *fw, uint8_t pin, uint8_t *out_level)
{
    gw_fw_count_cmd(fw);
    if (!out_level) return GW_FW_ACK_BAD_COMMAND;
    if (fw->state == GW_FW_STATE_ERROR) return fw->sticky_error;
    /* Pins we model. Pin layout is active-low per GW firmware:
     *   TRK0:   pin low (0)  => head over track 0
     *   WRPROT: pin low (0)  => disk is write-protected
     * The HAL reads get_pin(26) and inverts ("!uft_gw_get_pin"). We
     * deliver the raw level here. */
    switch (pin) {
        case GW_FW_PIN_TRK0:
            *out_level = fw->trk0_present ? 0 : 1;
            return GW_FW_ACK_OK;
        case GW_FW_PIN_WRPROT:
            *out_level = fw->write_protected ? 0 : 1;
            return GW_FW_ACK_OK;
        default:
            gw_fw_enter_error(fw, GW_FW_ACK_BAD_PIN);
            return GW_FW_ACK_BAD_PIN;
    }
}

gw_fw_ack_t gw_fw_cmd_set_pin(gw_fw_t *fw, uint8_t pin, uint8_t level)
{
    gw_fw_count_cmd(fw);
    (void)level;
    if (fw->state == GW_FW_STATE_ERROR) return fw->sticky_error;
    /* SetPin only legal on output pins. We do not model output pins
     * (HAL never writes them in current usage) — refuse them all
     * honestly rather than fake success. */
    gw_fw_enter_error(fw, GW_FW_ACK_BAD_PIN);
    (void)pin;
    return GW_FW_ACK_BAD_PIN;
}

gw_fw_ack_t gw_fw_cmd_read_flux(gw_fw_t *fw, uint32_t ticks, uint16_t revs)
{
    gw_fw_count_cmd(fw);
    (void)ticks;
    if (fw->state == GW_FW_STATE_ERROR) return fw->sticky_error;
    if (fw->state < GW_FW_STATE_MOTOR_ON || !fw->motor_on) {
        gw_fw_enter_error(fw, GW_FW_ACK_BAD_COMMAND);
        return GW_FW_ACK_BAD_COMMAND;
    }
    if (!fw->disk_present) {
        gw_fw_enter_error(fw, GW_FW_ACK_NO_INDEX);
        return GW_FW_ACK_NO_INDEX;
    }
    if (!fw->index_pulses_fire) {
        gw_fw_enter_error(fw, GW_FW_ACK_NO_INDEX);
        return GW_FW_ACK_NO_INDEX;
    }
    if (revs == 0) {
        /* HAL passes revs = revolutions + 1; revs == 0 is illegal at
         * the protocol layer. */
        gw_fw_enter_error(fw, GW_FW_ACK_BAD_COMMAND);
        return GW_FW_ACK_BAD_COMMAND;
    }
    fw->state              = GW_FW_STATE_STREAMING_READ;
    fw->read_stream_pos    = 0;
    fw->pending_flux_status = GW_FW_ACK_OK;
    return GW_FW_ACK_OK;
}

gw_fw_ack_t gw_fw_cmd_write_flux(gw_fw_t *fw,
                                  uint8_t cue_at_index,
                                  uint8_t terminate_at_index)
{
    gw_fw_count_cmd(fw);
    (void)cue_at_index; (void)terminate_at_index;
    /* Forensic-safety: this emulator NEVER emits writes. We always
     * refuse with ACK_WRPROT regardless of media state. The UFT HAL
     * itself does perform writes, but this sim is for testing the
     * READ pipeline and ERROR-handling — writes are bench-only. */
    return GW_FW_ACK_WRPROT;
}

gw_fw_ack_t gw_fw_cmd_erase_flux(gw_fw_t *fw, uint32_t ticks)
{
    gw_fw_count_cmd(fw);
    (void)ticks;
    /* Same forensic-safety stance as write_flux. */
    return GW_FW_ACK_WRPROT;
}

gw_fw_ack_t gw_fw_cmd_get_flux_status(gw_fw_t *fw)
{
    gw_fw_count_cmd(fw);
    /* GetFluxStatus is always legal (host polls after read/write).
     * Returns the sticky pending status set when the stream completed. */
    gw_fw_ack_t s = fw->pending_flux_status;
    /* After delivering the status, transition back to SEEKED so the
     * next command (Head/Seek/ReadFlux) lands cleanly. */
    if (fw->state == GW_FW_STATE_FLUX_STATUS_READY) {
        fw->state = GW_FW_STATE_SEEKED;
    }
    return s;
}

gw_fw_ack_t gw_fw_cmd_reset(gw_fw_t *fw)
{
    gw_fw_count_cmd(fw);
    /* Reset is the only command that ALWAYS works and clears sticky
     * errors. We collapse to CONNECTED (post-info), preserving the
     * configured firmware version etc. */
    gw_fw_state_t restore_to = GW_FW_STATE_CONNECTED;
    fw->state         = restore_to;
    fw->sticky_error  = GW_FW_ACK_OK;
    fw->bus_type      = GW_FW_BUS_NONE;
    fw->selected_unit = 0xFF;
    fw->motor_on      = false;
    fw->current_cyl   = -1;
    fw->current_head  = 0;
    fw->read_stream_pos     = 0;
    fw->pending_flux_status = GW_FW_ACK_OK;
    return GW_FW_ACK_OK;
}

gw_fw_ack_t gw_fw_cmd_no_click_step(gw_fw_t *fw)
{
    gw_fw_count_cmd(fw);
    /* NoClickStep is documented as the HAL's TRK0-recovery fallback.
     * From ERROR(NO_TRK0) state, accept it; flip trk0_present so the
     * next Seek(0) succeeds. From any non-ERROR state, no-op. */
    if (fw->state == GW_FW_STATE_ERROR &&
        fw->sticky_error == GW_FW_ACK_NO_TRK0) {
        /* Simulate the recovery: TRK0 sensor "found" via blind step. */
        fw->trk0_present  = true;
        fw->sticky_error  = GW_FW_ACK_OK;
        fw->state         = GW_FW_STATE_MOTOR_ON;
        return GW_FW_ACK_OK;
    }
    if (fw->state == GW_FW_STATE_ERROR) return fw->sticky_error;
    return GW_FW_ACK_OK;
}

gw_fw_ack_t gw_fw_pop_read_byte(gw_fw_t *fw, uint8_t *out_byte)
{
    if (!out_byte) return GW_FW_ACK_BAD_COMMAND;
    if (fw->state != GW_FW_STATE_STREAMING_READ) {
        /* Wrong state — host should not be draining bytes here. */
        return GW_FW_ACK_BAD_COMMAND;
    }
    if (!fw->read_stream_bytes || fw->read_stream_len == 0) {
        /* No stream loaded — deliver an immediate 0x00 EOS and
         * transition. This is honest: a real firmware would emit some
         * stream, but if the test author forgot to load one, we should
         * not invent bytes. */
        *out_byte = 0x00;
        fw->state = GW_FW_STATE_FLUX_STATUS_READY;
        return GW_FW_ACK_OK;
    }
    if (fw->read_stream_pos >= fw->read_stream_len) {
        /* Underrun — caller drained past the loaded buffer. Real
         * firmware would have ended with 0x00; we make this an honest
         * 0x00 + transition. */
        *out_byte = 0x00;
        fw->state = GW_FW_STATE_FLUX_STATUS_READY;
        return GW_FW_ACK_OK;
    }
    uint8_t b = fw->read_stream_bytes[fw->read_stream_pos++];
    *out_byte = b;
    fw->bytes_tx_to_host++;
    if (b == 0x00) {
        /* EOS marker — transition to FLUX_STATUS_READY so the host's
         * follow-up GetFluxStatus lands. */
        fw->state = GW_FW_STATE_FLUX_STATUS_READY;
    }
    return GW_FW_ACK_OK;
}

/* ─── Wire-protocol packet builder ───────────────────────────────────
 *
 * GW protocol: [CMD, LEN, params...]. LEN = total length (cmd + len
 * + params), matching uft_gw_command()'s `cmd_buf[1] = (uint8_t)total_len`.
 * No checksum. */

size_t gw_fw_build_packet(uint8_t *out, uint8_t cmd,
                           const uint8_t *params, size_t param_len)
{
    out[0] = cmd;
    out[1] = (uint8_t)(2 + param_len);
    if (params && param_len > 0) {
        memcpy(out + 2, params, param_len);
    }
    return 2 + param_len;
}
