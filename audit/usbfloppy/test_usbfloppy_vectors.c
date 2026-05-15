/*
 * test_usbfloppy_vectors.c — D1 wire-protocol vector check (native backend).
 *
 * Compile-time assertion that UFT's USB-Floppy (UFI) opcode constants
 * equal the official USB-IF "UFI Command Specification Rev. 1.0" values
 * (reference provenance: "recalled", see audit/usbfloppy/extract_ref.py).
 *
 * UFI is a SCSI-like subset: every opcode below is a standard SCSI/MMC
 * opcode. This file is the C twin of diff.py: diff.py checks at audit
 * time by parsing the header; this file makes the same contract a BUILD
 * GATE — if a future edit changes an opcode, the build breaks here, not
 * silently on the wire.
 *
 * Reproduce:
 *   gcc -std=c11 -I../../include -fsyntax-only test_usbfloppy_vectors.c
 *
 * No runtime, no main() body work — every check is _Static_assert.
 *
 * NOTE: this file proves the D1 wire-protocol layer only. The D4/D5
 * finding — that no platform backend (SG_IO / SCSI_PASS_THROUGH) exists,
 * so uft_ufi_backend_init() is a stub returning -1 and every UFI call
 * returns UFT_ERR_NOT_IMPLEMENTED — is a runtime/integration fact that a
 * static_assert cannot capture. See REPORT.md.
 */
#include "uft/hal/ufi.h"

/* ── UFI SCSI-like opcodes (USB-IF UFI Command Specification Rev. 1.0) ── */
_Static_assert(UFT_UFI_TEST_UNIT_READY == 0x00, "UFI TEST UNIT READY");
_Static_assert(UFT_UFI_REQUEST_SENSE   == 0x03, "UFI REQUEST SENSE");
_Static_assert(UFT_UFI_FORMAT_UNIT     == 0x04, "UFI FORMAT UNIT");
_Static_assert(UFT_UFI_INQUIRY         == 0x12, "UFI INQUIRY");
_Static_assert(UFT_UFI_MODE_SELECT_6   == 0x15, "UFI MODE SELECT(6)");
_Static_assert(UFT_UFI_MODE_SENSE_6    == 0x1A, "UFI MODE SENSE(6)");
_Static_assert(UFT_UFI_START_STOP      == 0x1B, "UFI START STOP UNIT");
_Static_assert(UFT_UFI_READ_FORMAT_CAP == 0x23, "UFI READ FORMAT CAPACITIES");
_Static_assert(UFT_UFI_READ_CAPACITY   == 0x25, "UFI READ CAPACITY");
_Static_assert(UFT_UFI_READ_10         == 0x28, "UFI READ(10)");
_Static_assert(UFT_UFI_WRITE_10        == 0x2A, "UFI WRITE(10)");
_Static_assert(UFT_UFI_VERIFY_10       == 0x2F, "UFI VERIFY(10)");

/* ── Opcode ordering sanity — the enum must stay monotonic where the ──
 * SCSI opcode space is monotonic, so an accidental reordering is caught. */
_Static_assert(UFT_UFI_TEST_UNIT_READY < UFT_UFI_INQUIRY,
    "TEST UNIT READY (0x00) precedes INQUIRY (0x12)");
_Static_assert(UFT_UFI_INQUIRY < UFT_UFI_READ_10,
    "INQUIRY (0x12) precedes READ(10) (0x28)");
_Static_assert(UFT_UFI_READ_10 < UFT_UFI_WRITE_10,
    "READ(10) (0x28) precedes WRITE(10) (0x2A)");

/* This translation unit intentionally has no symbols — it is a pure
 * compile-time conformance gate. */
