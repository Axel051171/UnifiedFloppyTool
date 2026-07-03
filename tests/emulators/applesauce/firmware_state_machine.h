/**
 * @file tests/emulators/applesauce/firmware_state_machine.h
 * @brief Firmware-realistic state machine for the Applesauce controller.
 *
 * Applesauce speaks an ASCII line protocol over USB-CDC serial
 * (115200 8N1). This emulator models the device side: the host feeds
 * one command line at a time into as_fw_process_line() and receives
 * the response line the firmware would emit, plus a classification of
 * that response. Binary flux download (`data:< N`) is drained through
 * as_fw_pop_binary().
 *
 * SPEC_STATUS: REVERSE-ENGINEERED. There is NO official Applesauce
 * protocol SDK. The in-tree protocol-truth hierarchy this emulator
 * follows, strongest first:
 *   1. src/hardware_providers/applesauce_serial_runners.cpp (MF-250) —
 *      the executable host-side implementation UFT will drive real
 *      hardware with. Command vocabulary: sync:on/off, head:track N,
 *      head:side N, head:zero, disk:readx R, data:?size, data:< N,
 *      disk:?write, data:clear, data:> N, disk:write, motor:on/off,
 *      psu:?, psu:on, sync:?speed, ?kind, data:?max, ?vers, ?pcb.
 *   2. src/hardware_providers/applesauce_provider_v2.h — V1 audit of
 *      the deleted 1330-LOC applesaucehardwareprovider.cpp, written
 *      against wiki.applesaucefdc.com. Status chars: '.'=OK, '!'=error,
 *      '?'=unknown, '+'=on/payload, '-'=off/error, 'v'=no power.
 *   3. docs/M3_APPLESAUCE_TRANSPORT.md — CONFLICTS with 1+2 on both
 *      command vocabulary ("info", "seek NN", "track NN hN capture R
 *      revolutions") and status-prefix semantics. This emulator follows
 *      1+2; the conflict is DIVERGENCES.md D-1.
 *
 * Forensic invariants:
 *   - This state machine NEVER produces flux bytes itself. Flux comes
 *     from the synthetic generator (tests/flux_gen/applesauce/) which
 *     the test wires in via as_fw_load_capture().
 *   - The write path (`data:> N`, `disk:write`) is ALWAYS refused with
 *     '!' regardless of media state — forensic-safety guard, matching
 *     the UFT HAL which also refuses writes until bench verification.
 *   - No silent state changes: sequencing violations answer '-'/'!'
 *     visibly; a host line sent while a binary download is pending
 *     wedges the emulator into AS_FW_STATE_ERROR (protocol desync is
 *     unrecoverable without reset — see DIVERGENCES.md D-5).
 */
#ifndef UFT_TESTS_AS_FIRMWARE_STATE_MACHINE_H
#define UFT_TESTS_AS_FIRMWARE_STATE_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants (from applesauce_provider_v2.cpp) ───────────────────── */

/** Device capture-buffer capacities. */
#define AS_FW_BUFFER_STANDARD  163840u   /* 160K — original Applesauce */
#define AS_FW_BUFFER_PLUS      430080u   /* 420K — Applesauce+        */

/** Highest quarter-track index `head:track` accepts (per the V2
 *  provider's default max_cylinder = 83). */
#define AS_FW_MAX_TRACK        83

/** `disk:readx R` revolution bounds (runner clamps host-side to 1..15;
 *  the firmware itself refusing outside this range is inferred —
 *  DIVERGENCES.md D-6). */
#define AS_FW_MIN_REVS         1
#define AS_FW_MAX_REVS         15

/** Longest command line the emulator accepts (incl. NUL). Real
 *  firmware line-buffer size is unknown — DIVERGENCES.md D-7. */
#define AS_FW_LINE_MAX         64

/* ─── State enum (explicit numeric values for ABI-stability) ────────── */
typedef enum {
    AS_FW_STATE_POWER_ON   = 0, /* USB enumerated; drive PSU off        */
    AS_FW_STATE_PSU_ON     = 1, /* psu:on accepted; motor still off     */
    AS_FW_STATE_MOTOR_ON   = 2, /* motor:on accepted; drive spinning    */
    AS_FW_STATE_SYNC_ON    = 3, /* sync:on accepted; capture-ready      */
    AS_FW_STATE_CAPTURED   = 4, /* disk:readx done; buffer holds flux   */
    AS_FW_STATE_BINARY_TX  = 5, /* data:< accepted; streaming binary    */
    AS_FW_STATE_ERROR      = 6, /* sticky desync — only reset recovers  */
} as_fw_state_t;

/* ─── Response classification ────────────────────────────────────────
 *
 * The literal response line is written to the caller's buffer; this
 * enum classifies it so tests don't have to re-parse. Mapping to the
 * documented status characters:
 *   ACK        → "."           (bare success)
 *   OK_PAYLOAD → "+<payload>"  (success with data)
 *   ERROR      → "-<message>"  (device error)
 *   PROTOCOL   → "!"           (protocol error / write-protect refusal)
 *   UNKNOWN    → "?"           (unrecognized command)
 *   NO_POWER   → "v"           (drive PSU is off)
 *   BINARY     → ""            (no line; binary bytes follow via
 *                               as_fw_pop_binary — see DIVERGENCES D-4)
 *   SILENT     → ""            (no response at all: simulated timeout /
 *                               wedged device / desync ERROR state)
 */
typedef enum {
    AS_FW_RESP_ACK        = 0,
    AS_FW_RESP_OK_PAYLOAD = 1,
    AS_FW_RESP_ERROR      = 2,
    AS_FW_RESP_PROTOCOL   = 3,
    AS_FW_RESP_UNKNOWN    = 4,
    AS_FW_RESP_NO_POWER   = 5,
    AS_FW_RESP_BINARY     = 6,
    AS_FW_RESP_SILENT     = 7,
} as_fw_resp_class_t;

/* ─── Drive kinds `?kind` can report ────────────────────────────────── */
typedef enum {
    AS_FW_KIND_NONE = 0,   /* no drive attached  → "NONE" */
    AS_FW_KIND_525  = 1,   /* Apple II 5.25"     → "5.25" */
    AS_FW_KIND_35   = 2,   /* Mac / IIgs 3.5"    → "3.5"  */
    AS_FW_KIND_PC   = 3,   /* PC 3.5"            → "PC"   */
} as_fw_drive_kind_t;

/* ─── Emulator context ──────────────────────────────────────────────
 *
 * One context per simulated device. Every field's value is traceable
 * to a configure call (as_fw_set_*) or the loaded synthetic capture —
 * no silent fabrication. */
typedef struct {
    as_fw_state_t state;

    /* Device identity (configurable per test). */
    char   version_str[32];      /* "?vers" payload */
    char   pcb_str[32];          /* "?pcb"  payload */
    as_fw_drive_kind_t drive_kind;
    uint32_t buffer_max;         /* data:?max payload */

    /* Electro-mechanical state. */
    bool   psu_on;
    bool   motor_on;
    bool   sync_on;
    int    current_track;        /* quarter-track index, -1 until seek */
    int    current_side;

    /* Media truth (configurable per test). */
    bool   disk_present;
    bool   write_protected;      /* disk:?write reports this truthfully */
    bool   index_present;        /* false => sync:?speed / readx fail   */
    double rpm;                  /* sync:?speed payload */

    /* Capture buffer — fed by the synthetic flux generator. NOT
     * copied; caller owns the lifetime. */
    const uint8_t *capture;
    size_t         capture_len;
    size_t         captured_len;   /* what disk:readx "captured" */
    int            captured_revs;

    /* Binary download bookkeeping. */
    size_t binary_offset;
    size_t binary_remaining;

    /* Fault injection: next N commands get no response (simulated
     * cable pull / wedged firmware). */
    int silent_next;

    /* Counters for assertions. */
    uint64_t cmd_count;
    uint64_t bytes_tx_to_host;
} as_fw_t;

/* ─── Lifecycle ─────────────────────────────────────────────────────── */

/** Hard reset to AS_FW_STATE_POWER_ON. Clears everything including the
 *  capture pointer (caller re-loads). Safe on an untouched struct. */
void as_fw_reset(as_fw_t *fw);

/** Power-on with a sensible bench setup: 5.25" drive attached, disk
 *  present, not write-protected, index pulse OK, 300.0 RPM,
 *  Applesauce+ buffer (420K), version "Applesauce 2.05", PCB "2.1". */
void as_fw_power_on_defaults(as_fw_t *fw);

/* Test setters — keep this surface tight; new flags = new defects. */
void as_fw_set_drive_kind(as_fw_t *fw, as_fw_drive_kind_t kind);
void as_fw_set_disk_present(as_fw_t *fw, bool present);
void as_fw_set_write_protected(as_fw_t *fw, bool wp);
void as_fw_set_index_present(as_fw_t *fw, bool present);
void as_fw_set_rpm(as_fw_t *fw, double rpm);
void as_fw_set_buffer_max(as_fw_t *fw, uint32_t bytes);
void as_fw_set_version(as_fw_t *fw, const char *vers, const char *pcb);
/** Next `n` commands produce AS_FW_RESP_SILENT (host-timeout drill). */
void as_fw_set_silent_next(as_fw_t *fw, int n);

/** Load the flux payload `disk:readx` will "capture" (from the
 *  synthetic generator). NOT copied — buffer must outlive the
 *  sequence. */
void as_fw_load_capture(as_fw_t *fw, const uint8_t *bytes, size_t len);

/* ─── Protocol entry points ─────────────────────────────────────────── */

/** Process one host command line (WITHOUT trailing '\n'). Writes the
 *  response line (WITHOUT '\n', NUL-terminated) into `response`
 *  (capacity `response_cap`, recommend >= AS_FW_LINE_MAX) and returns
 *  its classification. NULL args or tiny buffers return
 *  AS_FW_RESP_SILENT without touching state. */
as_fw_resp_class_t as_fw_process_line(as_fw_t *fw, const char *line,
                                      char *response, size_t response_cap);

/** During AS_FW_STATE_BINARY_TX, copy up to `want` bytes of the
 *  announced download into `out`. Returns bytes copied (0 when the
 *  transfer is complete or state is wrong). When the last byte is
 *  popped the state returns to AS_FW_STATE_CAPTURED. */
size_t as_fw_pop_binary(as_fw_t *fw, uint8_t *out, size_t want);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_AS_FIRMWARE_STATE_MACHINE_H */
