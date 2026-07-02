/**
 * @file tests/emulators/greaseweazle/firmware_state_machine.h
 * @brief Firmware-realistic state machine for the Greaseweazle controller.
 *
 * Models the documented prerequisite-order of GW firmware commands so
 * tests can verify "did the host follow the sequencing rules a real
 * Greaseweazle would enforce?" semantics — complementary to (not a
 * replacement for) the byte-level USB mock under tests/usb_mock/.
 *
 * Reference: keirf/greaseweazle v1.23 usb.py + flux.py
 *   - Cmd.GetInfo                 → answers 32-byte info struct
 *   - Cmd.SetBusType              → must precede Select
 *   - Cmd.Select(unit)            → unit 0..3, returns NO_UNIT if out-of-range
 *   - Cmd.Motor(unit, on)         → motor only after Select
 *   - Cmd.Seek(int8)              → signed cylinder (flippy drives)
 *                                   → cyl 0 verifies TRK0 pin afterwards
 *   - Cmd.Head(side)              → head select after Seek
 *   - Cmd.ReadFlux(ticks, revs)   → drains 0x00-terminated stream
 *   - Cmd.WriteFlux(cue,terminate)→ host streams encoded flux
 *   - Cmd.GetFluxStatus           → sticky status byte after read/write
 *   - Cmd.EraseFlux(ticks)        → erase
 *   - Cmd.Reset                   → clear firmware state
 *   - Cmd.Deselect                → drop drive select
 *
 * SPEC_STATUS: SOURCE-AUTHORITATIVE (keirf/greaseweazle is the canonical
 * firmware impl; the UFT C-HAL at src/hal/uft_greaseweazle_full.c is
 * production-tested against real hardware).
 *
 * Forensic invariant: this emulator NEVER produces flux bytes itself.
 * Flux comes from the synthetic generator (tests/flux_gen/greaseweazle/
 * flux_gen.c) which the test loads via gw_fw_load_read_stream().
 *
 * Write path: like SCP, all CMD_WRITE_FLUX / CMD_ERASE_FLUX are
 * REFUSED with ACK_WRPROT regardless of media state. Forensic-safety
 * guard — never emulated.
 */
#ifndef UFT_TESTS_GW_FIRMWARE_STATE_MACHINE_H
#define UFT_TESTS_GW_FIRMWARE_STATE_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── State enum (explicit numeric values for ABI-stability) ─────────── */
typedef enum {
    GW_FW_STATE_DISCONNECTED      = 0, /* not opened — get_info will fail */
    GW_FW_STATE_CONNECTED         = 1, /* get_info answered, bus not set */
    GW_FW_STATE_BUS_CONFIGURED    = 2, /* set_bus_type accepted */
    GW_FW_STATE_DRIVE_SELECTED    = 3, /* Cmd.Select(unit) accepted */
    GW_FW_STATE_MOTOR_ON          = 4, /* Cmd.Motor(unit, 1) accepted */
    GW_FW_STATE_SEEKED            = 5, /* Cmd.Seek + Cmd.Head completed */
    GW_FW_STATE_STREAMING_READ    = 6, /* ReadFlux in progress — draining */
    GW_FW_STATE_STREAMING_WRITE   = 7, /* WriteFlux in progress (always refused) */
    GW_FW_STATE_FLUX_STATUS_READY = 8, /* read/write done, GetFluxStatus pending */
    GW_FW_STATE_ERROR             = 9, /* sticky error until reset */
} gw_fw_state_t;

/* ─── ACK bytes (mirrors uft_gw_ack_t verbatim) ──────────────────────── */
typedef enum {
    GW_FW_ACK_OK              = 0x00,
    GW_FW_ACK_BAD_COMMAND     = 0x01,
    GW_FW_ACK_NO_INDEX        = 0x02,
    GW_FW_ACK_NO_TRK0         = 0x03,
    GW_FW_ACK_FLUX_OVERFLOW   = 0x04,
    GW_FW_ACK_FLUX_UNDERFLOW  = 0x05,
    GW_FW_ACK_WRPROT          = 0x06,
    GW_FW_ACK_NO_UNIT         = 0x07,
    GW_FW_ACK_NO_BUS          = 0x08,
    GW_FW_ACK_BAD_UNIT        = 0x09,
    GW_FW_ACK_BAD_PIN         = 0x0A,
    GW_FW_ACK_BAD_CYLINDER    = 0x0B,
    GW_FW_ACK_OUT_OF_SRAM     = 0x0C,
    GW_FW_ACK_OUT_OF_FLASH    = 0x0D,
} gw_fw_ack_t;

/* ─── Bus type (mirrors uft_gw_bus_type_t) ───────────────────────────── */
typedef enum {
    GW_FW_BUS_NONE     = 0,
    GW_FW_BUS_IBM_PC   = 1,
    GW_FW_BUS_SHUGART  = 2,
} gw_fw_bus_type_t;

/* ─── Pin numbers we model ───────────────────────────────────────────── */
/* Reference: usb.py + uft_greaseweazle_full.c::uft_gw_is_write_protected
 * — pin 26 = TRK0 (active low), pin 28 = WRPROT (active low). */
#define GW_FW_PIN_TRK0    26
#define GW_FW_PIN_WRPROT  28

/* ─── Emulator context ──────────────────────────────────────────────
 *
 * Holds the full simulated firmware state. Reset to a defined power-on
 * state by gw_fw_reset(). NEVER aliased — caller owns one context per
 * simulated device.
 *
 * The read-flux stream buffer is caller-owned (loaded via
 * gw_fw_load_read_stream); the emulator does not copy. */
typedef struct {
    gw_fw_state_t   state;
    gw_fw_ack_t     sticky_error;   /* set when state == ERROR */

    /* Identity reported by GetInfo (configurable per test). */
    uint8_t   fw_major;
    uint8_t   fw_minor;
    uint8_t   is_main_fw;          /* 0 => bootloader; HAL rejects */
    uint8_t   hw_model;            /* 1 = F7, 7 = F7-Plus */
    uint8_t   hw_submodel;
    uint8_t   usb_speed;
    uint32_t  sample_freq;         /* 72_000_000 default; 84_000_000 for F7+ */

    /* Drive/bus state */
    gw_fw_bus_type_t  bus_type;
    uint8_t           selected_unit;     /* 0xFF = none */
    bool              motor_on;
    int16_t           current_cyl;       /* signed for flippy drives */
    uint8_t           current_head;

    /* Media / pin state (set per test) */
    bool      disk_present;
    bool      write_protected;
    bool      trk0_present;              /* /TRK0 active-low => firmware sees pin low */
    bool      index_pulses_fire;         /* false => ReadFlux returns NO_INDEX */

    /* Read-flux stream buffer (host-side) */
    const uint8_t *read_stream_bytes;
    size_t         read_stream_len;
    size_t         read_stream_pos;      /* next byte to deliver via gw_fw_pop_read_byte */
    gw_fw_ack_t    pending_flux_status;  /* what GetFluxStatus will return */

    /* Counters for assertions */
    uint64_t cmd_count;
    uint64_t bytes_tx_to_host;
} gw_fw_t;

/* ─── Lifecycle ─────────────────────────────────────────────────────── */

/** Reset to DISCONNECTED. Frees nothing the caller owns. */
void gw_fw_reset(gw_fw_t *fw);

/** Convenience: power-on, get_info will succeed.
 *  Defaults: F7 firmware v1.23, sample_freq 72 MHz, no bus type set,
 *  disk present, not write-protected, TRK0 sensed, index pulses firing. */
void gw_fw_power_on_defaults(gw_fw_t *fw);

/** Test setters — keep this surface tight; new flags = new defects. */
void gw_fw_set_firmware_version(gw_fw_t *fw, uint8_t major, uint8_t minor);
void gw_fw_set_bootloader_mode(gw_fw_t *fw, bool bootloader);
void gw_fw_set_hardware_model(gw_fw_t *fw, uint8_t model, uint8_t submodel);
void gw_fw_set_sample_freq(gw_fw_t *fw, uint32_t hz);
void gw_fw_set_disk_present(gw_fw_t *fw, bool present);
void gw_fw_set_write_protected(gw_fw_t *fw, bool wp);
void gw_fw_set_trk0_present(gw_fw_t *fw, bool present);
void gw_fw_set_index_pulses_fire(gw_fw_t *fw, bool fire);

/** Load a pre-encoded flux stream (from the synthetic flux generator)
 *  that ReadFlux will drain. The stream MUST end with the 0x00 EOS
 *  byte the real firmware appends.
 *
 *  Buffer is NOT copied — caller must keep it alive for the duration
 *  of the read sequence. */
void gw_fw_load_read_stream(gw_fw_t *fw,
                             const uint8_t *bytes, size_t len);

/* ─── Command processing ────────────────────────────────────────────
 *
 * Each function takes the current command + params and returns the
 * ACK byte the firmware would emit. Side effects update fw->state.
 *
 * Convention: ACK == OK (0x00) means success. Anything non-zero is
 * the firmware error code. */

gw_fw_ack_t gw_fw_cmd_get_info(gw_fw_t *fw, uint8_t subindex,
                                uint8_t *out, size_t out_cap);
gw_fw_ack_t gw_fw_cmd_set_bus_type(gw_fw_t *fw, gw_fw_bus_type_t bus_type);
gw_fw_ack_t gw_fw_cmd_select(gw_fw_t *fw, uint8_t unit);
gw_fw_ack_t gw_fw_cmd_deselect(gw_fw_t *fw);
gw_fw_ack_t gw_fw_cmd_motor(gw_fw_t *fw, uint8_t unit, bool on);
gw_fw_ack_t gw_fw_cmd_seek(gw_fw_t *fw, int8_t cylinder);
gw_fw_ack_t gw_fw_cmd_head(gw_fw_t *fw, uint8_t head);
gw_fw_ack_t gw_fw_cmd_get_pin(gw_fw_t *fw, uint8_t pin, uint8_t *out_level);
gw_fw_ack_t gw_fw_cmd_set_pin(gw_fw_t *fw, uint8_t pin, uint8_t level);
gw_fw_ack_t gw_fw_cmd_read_flux(gw_fw_t *fw, uint32_t ticks, uint16_t revs);
gw_fw_ack_t gw_fw_cmd_write_flux(gw_fw_t *fw,
                                  uint8_t cue_at_index,
                                  uint8_t terminate_at_index);
gw_fw_ack_t gw_fw_cmd_erase_flux(gw_fw_t *fw, uint32_t ticks);
gw_fw_ack_t gw_fw_cmd_get_flux_status(gw_fw_t *fw);
gw_fw_ack_t gw_fw_cmd_reset(gw_fw_t *fw);
gw_fw_ack_t gw_fw_cmd_no_click_step(gw_fw_t *fw);

/** During STREAMING_READ, drain one byte from the loaded read stream.
 *  Returns ACK_OK and writes one byte to *out_byte until the stream
 *  reaches its 0x00 EOS, after which it transitions to FLUX_STATUS_READY
 *  and further calls return ACK_OK with *out_byte = 0x00. Returns
 *  ACK_BAD_COMMAND if state != STREAMING_READ. */
gw_fw_ack_t gw_fw_pop_read_byte(gw_fw_t *fw, uint8_t *out_byte);

/* ─── Wire-protocol packet builder ───────────────────────────────────
 *
 * Builds [CMD, LEN, params...] matching the C-HAL's serialization
 * (see uft_gw_command()). Includes no checksum (GW protocol does not
 * use one — the serial framing alone is authoritative).
 *
 * Returns total length (= 2 + param_len). out must be sized
 * >= 2 + param_len. */
size_t gw_fw_build_packet(uint8_t *out, uint8_t cmd,
                           const uint8_t *params, size_t param_len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_GW_FIRMWARE_STATE_MACHINE_H */
