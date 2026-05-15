/*
 * test_fc5025_vectors.c — D1 wire-protocol vector check (USB-identity layer).
 *
 * FC5025 is documented as a dual-path backend (libusb USB CBW/CSW +
 * fcimage CLI). The CLI side is covered by mock_fc5025.py. THIS file
 * covers the only byte-/value-exact surface UFT actually exposes in
 * source: the USB identity (VID/PID) and the disk-format enum ordinals
 * in include/uft/hal/uft_fc5025.h.
 *
 * It is the C twin of diff.py: diff.py checks at audit time by parsing
 * the header; this file makes the same contract a BUILD GATE.
 *
 * SCOPE LIMIT — this file deliberately asserts NOTHING about CBW opcodes
 * or fcimage argv: per REPORT.md D1 findings FC-D1-1 / FC-D1-2 those
 * tables do not exist in the UFT source (the dialogue lives in an
 * un-constructed injected runner), so there is nothing to static_assert.
 *
 * Reproduce:
 *   gcc -std=c11 -I../../include -fsyntax-only test_fc5025_vectors.c
 *
 * Reference provenance: VID/PID "recalled" (open-source fc5025.h from
 * the Device Side Data Linux source release; not vendored). See
 * audit/fc5025/extract_ref.py PROVENANCE.
 */
#include "uft/hal/uft_fc5025.h"

/* -- USB identity ----------------------------------------------------- */
_Static_assert(UFT_FC5025_VID == 0x16C0,
    "FC5025 USB VID must be 0x16C0 (V-USB / objdev shared VID)");
_Static_assert(UFT_FC5025_PID == 0x06D6,
    "FC5025 USB PID must be 0x06D6 (Device Side Data FC5025 allocation)");

/* -- disk-format enum ordinal contract -------------------------------- *
 * uft_fc_format_t is a UFT-internal abstraction, not an FC5025 wire
 * value. Only two ordinals carry a hard contract:
 *   - AUTO must be 0 (it is the zero-initialised default).
 *   - APPLE_DOS33 must be 2, because FC5025ProviderV2's ctor defaults
 *     m_disk_format to 0x02 and documents that 0x02 == FMT_APPLE_DOS33
 *     (fc5025_provider_v2.h:124-125). If the enum is reordered without
 *     updating the ctor default, this assert breaks the build — exactly
 *     the silent-drift this gate exists to catch.
 */
_Static_assert(UFT_FC_FMT_AUTO == 0,
    "uft_fc_format_t: AUTO must be ordinal 0 (zero-init default)");
_Static_assert(UFT_FC_FMT_APPLE_DOS33 == 2,
    "uft_fc_format_t: APPLE_DOS33 must be ordinal 2 — "
    "FC5025ProviderV2 ctor defaults disk_format to 0x02 and relies on "
    "that equalling FMT_APPLE_DOS33");

/* Spot-check enum monotonicity around the contract anchors. */
_Static_assert(UFT_FC_FMT_APPLE_DOS32 == 1,
    "uft_fc_format_t: DOS32 sits between AUTO(0) and DOS33(2)");
_Static_assert(UFT_FC_FMT_APPLE_PRODOS == 3,
    "uft_fc_format_t: PRODOS follows DOS33");

/* Translation unit needs one external symbol to be non-empty under -c. */
int uft_audit_fc5025_vectors_ok(void) { return 0; }
