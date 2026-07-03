/**
 * @file tests/emulators/adfcopy/firmware_state_machine.h
 * @brief Teensy-firmware model for the ADFCopy controller (binary serial).
 *
 * ADFCopy is a Teensy-based (PJRC VID:PID 0x16C0:0x0483) USB-CDC serial
 * controller for Amiga 3.5" drives. Unlike Applesauce's ASCII protocol,
 * ADFCopy uses BINARY framing: a 1-byte command + variable payload, and
 * a 1-byte status response ('O'/'E'/'D') plus a binary payload where
 * applicable. This emulator models the firmware side: the host pushes a
 * command frame and the device produces the response bytes.
 *
 * SPEC_STATUS: VendorDocumented (community) — the protocol is the SSOT in
 * src/hardware_providers/adfcopy_serial_runners.h (MF-252):
 *   CMD_INIT       0x01  → 'O'            (spin motor + home head)
 *   CMD_SEEK       0x02 + track_byte → 'O' (track = cyl*2 + head, 0..159)
 *   CMD_READ_FLUX  0x06 + track + revs   → 3-byte header + flux payload
 *   CMD_GET_STATUS 0x0B  → 1-byte status bitmask
 * Responses: 'O'=0x4F ok, 'E'=0x45 error, 'D'=0x44 no-disk.
 * Status bits: DISK_PRESENT 0x01, WRITE_PROT 0x02, MOTOR_ON 0x04,
 *              FLUX_CAPABLE 0x08.
 * Sample clock 40 MHz / 25 ns (matches SCP).
 *
 * Forensic invariants:
 *   - No fabricated flux: a READ_FLUX success returns the synthetic
 *     generator's bytes verbatim; a no-disk read returns the 'D'-status
 *     header and no flux.
 *   - Write is not modelled: ADF-Copy hardware refuses raw flux writes
 *     (V1 writeRawFlux always false). There is no write command frame.
 *   - No silent state changes: an out-of-range seek or a command sent
 *     with no disk answers 'E'/'D' visibly.
 */
#ifndef UFT_TESTS_ADFC_STATE_MACHINE_H
#define UFT_TESTS_ADFC_STATE_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "flux_gen.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Protocol constants (SSOT: adfcopy_serial_runners.h) ───────────── */
#define ADFC_CMD_INIT        0x01u
#define ADFC_CMD_SEEK        0x02u
#define ADFC_CMD_READ_FLUX   0x06u
#define ADFC_CMD_GET_STATUS  0x0Bu
#define ADFC_RSP_OK          0x4Fu   /* 'O' */
#define ADFC_RSP_ERROR       0x45u   /* 'E' */
#define ADFC_RSP_NODISK      0x44u   /* 'D' */
#define ADFC_STATUS_DISK     0x01u
#define ADFC_STATUS_WPROT    0x02u
#define ADFC_STATUS_MOTOR    0x04u
#define ADFC_STATUS_FLUX     0x08u

typedef enum {
    ADFC_STATE_IDLE     = 0,   /* powered, motor off */
    ADFC_STATE_READY    = 1,   /* CMD_INIT done: motor on, head homed */
    ADFC_STATE_ERROR    = 2,
} adfc_state_t;

/* ─── device model ──────────────────────────────────────────────────── */
typedef struct {
    adfc_state_t state;

    bool   disk_present;
    bool   write_protected;
    bool   motor_on;
    bool   flux_capable;
    int    current_track;     /* 0..159, -1 until seek */

    uint64_t stream_seed;
    uint32_t inject_defects;  /* uft_adfc_defect_flags_t for READ_FLUX */

    uint8_t  last_response;   /* last single-byte response */
    uint64_t reads_ok;
} adfc_dev_t;

/* ─── lifecycle ─────────────────────────────────────────────────────── */
void adfc_reset(adfc_dev_t *dev);
void adfc_power_on_defaults(adfc_dev_t *dev);  /* disk in, not WP, flux-capable */
void adfc_set_disk_present(adfc_dev_t *dev, bool present);
void adfc_set_write_protected(adfc_dev_t *dev, bool wp);
void adfc_set_stream_seed(adfc_dev_t *dev, uint64_t seed);
void adfc_set_inject_defects(adfc_dev_t *dev, uint32_t defects);

/* ─── protocol ──────────────────────────────────────────────────────── */

/** Process CMD_INIT: spin motor + home head. Returns the response byte
 *  ('O' on success, 'D' if no disk). */
uint8_t adfc_cmd_init(adfc_dev_t *dev);

/** Process CMD_SEEK to `track` (cyl*2 + head, 0..159). Returns 'O' on
 *  success, 'E' if out of range or not initialised. */
uint8_t adfc_cmd_seek(adfc_dev_t *dev, int track);

/** Process CMD_GET_STATUS: returns the 1-byte status bitmask. */
uint8_t adfc_cmd_get_status(const adfc_dev_t *dev);

/** Process CMD_READ_FLUX for `track` over `revolutions`. On success
 *  allocates *out_reply / *out_len with the full wire reply (3-byte
 *  header + flux; caller frees via free()) and returns 'O'. On no-disk
 *  returns 'D' with a 3-byte no-disk header (no flux). On error returns
 *  'E' with *out_reply = NULL. */
uint8_t adfc_cmd_read_flux(adfc_dev_t *dev, int track, int revolutions,
                           uint8_t **out_reply, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_ADFC_STATE_MACHINE_H */
