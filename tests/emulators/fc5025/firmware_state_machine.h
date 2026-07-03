/**
 * @file tests/emulators/fc5025/firmware_state_machine.h
 * @brief FC5025 USB CBW/CSW device model.
 *
 * FC5025 is a sector-only, read-only USB device (Device Side Data,
 * VID:PID 0x16C0:0x06D6). Its firmware decodes the disk and delivers
 * decoded sector payloads via USB Bulk-Only Transport: a CMD_READ_FLEXIBLE
 * Command Block Wrapper (CBW) on EP 0x01, sector data + a Command Status
 * Wrapper (CSW) on EP 0x81. This emulator models that device.
 *
 * SPEC_STATUS: VendorDocumented — the FC5025 Command Set Specification
 * v1309 (Device Side Data, Inc., deviceside.com/fc5025.html) documents
 * the CBW/CSW protocol and opcodes; the USB BOT framing is the standard
 * Mass Storage Class. Divergences from the real device are in
 * DIVERGENCES.md.
 *
 * Forensic invariant (provider Rule F-3, divergent-read preservation):
 *   A CRC-error sector is re-read up to the retry limit, and the DISTINCT
 *   reads are PRESERVED (>= 2 divergent copies) — never collapsed to one
 *   "resolved" value. fc_read_sector() returns the divergent set so the
 *   test can assert the marginal data survived.
 *
 * Read-only invariant: there is NO write opcode in the FC5025 command
 * table. A write request is refused with a distinct status, never a
 * silent no-op.
 */
#ifndef UFT_TESTS_FC_STATE_MACHINE_H
#define UFT_TESTS_FC_STATE_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "flux_gen.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── USB Bulk-Only Transport framing (standard MSC + FC5025 v1309) ── */
#define FC_CBW_SIGNATURE   0x43425355u   /* "USBC" little-endian */
#define FC_CSW_SIGNATURE   0x53425355u   /* "USBS" little-endian */
#define FC_CBW_LEN         31u
#define FC_CSW_LEN         13u
#define FC_BULK_OUT_EP     0x01u
#define FC_BULK_IN_EP      0x81u
/* FC5025 vendor opcodes (v1309). Only READ is used by UFT (read-only). */
#define FC_CMD_READ_FLEXIBLE  0xE5u
#define FC_CMD_SEEK           0xC0u
#define FC_CMD_RECALIBRATE    0xC1u

/* ─── CSW status byte ───────────────────────────────────────────────── */
typedef enum {
    FC_CSW_OK          = 0x00,
    FC_CSW_FAILED      = 0x01,   /* generic command failure          */
    FC_CSW_CRC_ERROR   = 0x02,   /* sector read but CRC bad          */
    FC_CSW_NO_SECTOR   = 0x03,   /* address mark not found           */
    FC_CSW_NO_DISK     = 0x04,   /* drive empty                      */
    FC_CSW_WRITE_DENY  = 0x05,   /* write attempted (no such command) */
} fc_csw_status_t;

typedef enum {
    FC_STATE_DISCONNECTED = 0,
    FC_STATE_READY        = 1,
    FC_STATE_ERROR        = 2,
} fc_state_t;

/* ─── device model ──────────────────────────────────────────────────── */
typedef struct {
    fc_state_t state;

    bool   device_present;   /* board on the USB bus            */
    bool   disk_present;

    /* The mounted synthetic track (one at a time, per test). */
    const uft_fc_gen_track_t *track;

    /* Read policy. */
    int    max_retries;      /* CRC-error re-read attempts (>= 2 to preserve) */

    /* Bookkeeping. */
    fc_csw_status_t last_csw;
    uint64_t sectors_ok;
    uint64_t sectors_crc;
} fc_dev_t;

/* Result of a sector read: status + the preserved divergent copies. */
typedef struct {
    fc_csw_status_t status;
    int             attempts;        /* how many reads were performed */
    int             divergent_count; /* DISTINCT byte copies preserved */
    /* Up to 8 divergent copies kept (marginal preservation). */
    uint8_t        *copies[8];
    uint16_t        copy_len;
} fc_read_result_t;

/* ─── lifecycle ─────────────────────────────────────────────────────── */
void fc_reset(fc_dev_t *dev);
void fc_power_on_defaults(fc_dev_t *dev);   /* device+disk present, retries=3 */
void fc_set_device_present(fc_dev_t *dev, bool present);
void fc_set_disk_present(fc_dev_t *dev, bool present);
void fc_mount_track(fc_dev_t *dev, const uft_fc_gen_track_t *track);
void fc_set_max_retries(fc_dev_t *dev, int retries);

/* ─── operations ────────────────────────────────────────────────────── */

/** Model `dtc -i`-equivalent detect: FC5025 reports presence only (it
 *  cannot auto-detect geometry — the format is user-selected). Returns
 *  1 if a device is present, else 0. */
int fc_detect(fc_dev_t *dev);

/** Build a CBW for CMD_READ_FLEXIBLE into `cbw` (>= FC_CBW_LEN). Returns
 *  bytes written (FC_CBW_LEN) or 0. Exposed for wire-framing tests. */
size_t fc_build_read_cbw(uint8_t *cbw, size_t cap, uint32_t tag,
                         int track, int side, int sector,
                         uint16_t sector_size);

/** Parse a CSW from `csw` (>= FC_CSW_LEN). Returns false on bad
 *  signature / short buffer; else fills *tag, *residue, *status. */
bool fc_parse_csw(const uint8_t *csw, size_t len,
                  uint32_t *tag, uint32_t *residue, fc_csw_status_t *status);

/** Read one sector. For a CRC-error sector, re-reads up to max_retries,
 *  collecting the DISTINCT byte copies (Rule F-3: >= 2 preserved). The
 *  caller must fc_free_read_result(). Returns the CSW status. */
fc_csw_status_t fc_read_sector(fc_dev_t *dev, int track, int side,
                               int sector, fc_read_result_t *out);

/** Attempt a write — always refused (no write opcode). Returns
 *  FC_CSW_WRITE_DENY. */
fc_csw_status_t fc_write_sector(fc_dev_t *dev, int track, int side, int sector);

void fc_free_read_result(fc_read_result_t *r);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_FC_STATE_MACHINE_H */
