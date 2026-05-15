/*
 * test_scp_vectors.c — D1 wire-protocol vector check (native backend).
 *
 * Compile-time assertion of UFT's SuperCard Pro constants. SCP is a
 * NATIVE libusb backend (src/hal/uft_scp_direct.c), so this is the C
 * twin of diff.py — it makes the header contract a BUILD GATE.
 *
 * IMPORTANT — reference provenance (see audit/scp/extract_ref.py):
 *   The SCP USB command bytes (CMD_*), VID/PID, and Bulk endpoints are
 *   graded "needs-source": they could NOT be confirmed against a
 *   vendored SuperCard Pro SDK v1.7 command header. They are therefore
 *   asserted here ONLY for internal-consistency / regression-pinning —
 *   i.e. "this value must not silently change" — NOT as a vendored
 *   protocol-correctness gate. A future edit that changes one of these
 *   still breaks the build (good: no silent drift), but a green build
 *   here does NOT prove protocol correctness.
 *
 *   The sample clock (25 ns) and geometry (167 max track index) ARE
 *   recalled-grade and cross-checked in-repo (samdisk/scp.cpp) — those
 *   assertions are real protocol-correctness gates.
 *
 * Reproduce:
 *   gcc -std=c11 -I../../include -fsyntax-only test_scp_vectors.c
 *
 * No runtime, no main() body work — every check is static_assert.
 */
#include "uft/hal/uft_scp_direct.h"

/* ── Sample clock + geometry (recalled — real correctness gate) ─────── */
_Static_assert(UFT_SCP_FLUX_NS_PER_SAMPLE == 25,
    "SCP samples at 40 MHz => 25 ns per tick "
    "(cross-checked vs samdisk/scp.cpp '25ns sampling time')");
_Static_assert(UFT_SCP_MAX_TRACK_INDEX == 167,
    "SCP geometry: 84 cylinders * 2 sides - 1 = 167");
_Static_assert(UFT_SCP_MAX_REVOLUTIONS == 5,
    "SCP captures up to 5 revolutions per command (SDK limit)");
_Static_assert(UFT_SCP_DEFAULT_REVOLUTIONS >= 1
            && UFT_SCP_DEFAULT_REVOLUTIONS <= UFT_SCP_MAX_REVOLUTIONS,
    "default revolution count must be within [1, MAX_REVOLUTIONS]");

/* ── USB command bytes (needs-source — regression-pin ONLY) ─────────── */
/* These values are NOT confirmed against a vendored SCP SDK header.
 * They are pinned so a silent change breaks the build (SCP-D1-1). */
_Static_assert(UFT_SCP_CMD_SET_CONTROL    == 0x02,
    "SCP CMD_SET_CONTROL pinned at 0x02 (UNVERIFIED — needs vendored SDK)");
_Static_assert(UFT_SCP_CMD_SELECT_DRIVE   == 0x03,
    "SCP CMD_SELECT_DRIVE pinned at 0x03 (UNVERIFIED — needs vendored SDK)");
_Static_assert(UFT_SCP_CMD_READ_FLUX      == 0x04,
    "SCP CMD_READ_FLUX pinned at 0x04 (UNVERIFIED — needs vendored SDK)");
_Static_assert(UFT_SCP_CMD_WRITE_FLUX     == 0x05,
    "SCP CMD_WRITE_FLUX pinned at 0x05 (UNVERIFIED — needs vendored SDK)");
_Static_assert(UFT_SCP_CMD_DESELECT_DRIVE == 0x09,
    "SCP CMD_DESELECT_DRIVE pinned at 0x09 (UNVERIFIED — needs vendored SDK)");
_Static_assert(UFT_SCP_CMD_GET_INFO       == 0x40,
    "SCP CMD_GET_INFO pinned at 0x40 (UNVERIFIED — needs vendored SDK)");

/* ── USB identity + endpoints ──────────────────────────────────────── */
/* SCP-D4-1 RESOLVED (audit ARCH-7-B / MF-212): the header VID/PID once
 * read 0x16C0/0x0753 and DISAGREED with the GUI port-hint. Verified
 * against the real device descriptor (USB\VID_16D0&PID_0F8C) — the GUI
 * hint was correct; uft_scp_direct.h was corrected and now single-
 * sources the value. These pins track the VERIFIED value and catch
 * silent drift. */
_Static_assert(UFT_SCP_USB_VID     == 0x16D0,
    "SCP USB VID pinned at 0x16D0 (VERIFIED — real device descriptor, MF-212)");
_Static_assert(UFT_SCP_USB_PID     == 0x0F8C,
    "SCP USB PID pinned at 0x0F8C (VERIFIED — real device descriptor, MF-212)");
_Static_assert(UFT_SCP_BULK_IN_EP  == 0x81,
    "SCP Bulk IN endpoint pinned at 0x81 (UNVERIFIED — needs vendored descriptor)");
_Static_assert(UFT_SCP_BULK_OUT_EP == 0x01,
    "SCP Bulk OUT endpoint pinned at 0x01 (UNVERIFIED — needs vendored descriptor)");

/* Translation unit needs one external symbol to be non-empty under -c. */
int uft_audit_scp_vectors_ok(void) { return 0; }
