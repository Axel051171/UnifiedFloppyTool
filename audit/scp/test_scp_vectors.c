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

/* ── USB command bytes ─────────────────────────────────────────────── */
/* MF-254 (v4.1.5-hardening): the pre-MF-254 pins (0x02..0x40) were
 * placeholder guesses that did NOT match the real SCP firmware
 * protocol. Verified against simonowen/samdisk include/SuperCardPro.h
 * — opcodes live in the 0x80-0xD2 range with packet framing
 * [CMD, LEN, params..., CHECKSUM] (checksum = CMD+LEN+sum+0x4A) and
 * response [CMD_ECHO, STATUS] with pr_Ok = 0x4F.
 *
 * These pins now track the VERIFIED values and catch silent drift. */
_Static_assert(UFT_SCP_CMD_SELA       == 0x80,
    "SCP CMD_SELA pinned at 0x80 (VERIFIED — samdisk SuperCardPro.h)");
_Static_assert(UFT_SCP_CMD_DSELA      == 0x82,
    "SCP CMD_DSELA pinned at 0x82 (VERIFIED — samdisk SuperCardPro.h)");
_Static_assert(UFT_SCP_CMD_MTRAON     == 0x84,
    "SCP CMD_MTRAON pinned at 0x84 (VERIFIED — samdisk SuperCardPro.h)");
_Static_assert(UFT_SCP_CMD_MTRAOFF    == 0x86,
    "SCP CMD_MTRAOFF pinned at 0x86 (VERIFIED — samdisk SuperCardPro.h)");
_Static_assert(UFT_SCP_CMD_SEEK0      == 0x88,
    "SCP CMD_SEEK0 pinned at 0x88 (VERIFIED — samdisk SuperCardPro.h)");
_Static_assert(UFT_SCP_CMD_STEPTO     == 0x89,
    "SCP CMD_STEPTO pinned at 0x89 (VERIFIED — samdisk SuperCardPro.h)");
_Static_assert(UFT_SCP_CMD_SIDE       == 0x8D,
    "SCP CMD_SIDE pinned at 0x8D (VERIFIED — samdisk SuperCardPro.h)");
_Static_assert(UFT_SCP_CMD_STATUS     == 0x8E,
    "SCP CMD_STATUS pinned at 0x8E (VERIFIED — samdisk SuperCardPro.h)");
_Static_assert(UFT_SCP_CMD_READ_FLUX  == 0xA0,
    "SCP CMD_READ_FLUX pinned at 0xA0 (VERIFIED — samdisk SuperCardPro.h)");
_Static_assert(UFT_SCP_CMD_WRITE_FLUX == 0xA2,
    "SCP CMD_WRITE_FLUX pinned at 0xA2 (VERIFIED — samdisk SuperCardPro.h)");
_Static_assert(UFT_SCP_CMD_SCPINFO    == 0xD0,
    "SCP CMD_SCPINFO pinned at 0xD0 (VERIFIED — samdisk SuperCardPro.h)");
_Static_assert(UFT_SCP_PR_OK          == 0x4F,
    "SCP pr_Ok response pinned at 0x4F (VERIFIED — samdisk SuperCardPro.h)");
_Static_assert(UFT_SCP_CHECKSUM_INIT  == 0x4A,
    "SCP CHECKSUM_INIT pinned at 0x4A (VERIFIED — samdisk SuperCardPro.h)");

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
