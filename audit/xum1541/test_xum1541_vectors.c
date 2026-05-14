/*
 * test_xum1541_vectors.c — D1 wire-protocol vector check (native backend).
 *
 * Compile-time assertion that UFT's XUM1541 IEC-bus command constants
 * equal the official IEEE-488 / Commodore serial-bus values, and that
 * the CBM drive-type enum has the ordinals the provider relies on.
 *
 * SCOPE NOTE — the bulk of the XUM1541 wire protocol (firmware command
 * enum Xum1541Cmd, USB VID/PID, bulk endpoint addresses, CBM DOS status
 * codes) lives in src/hardware_providers/xum1541_usb.h, which is a
 * C++ header (namespace + constexpr + <windows.h>/<dlfcn.h>). It cannot
 * be included from a `-std=c11` translation unit. Those constants are
 * therefore covered by diff.py only (which parses the header textually),
 * not by this build gate. THIS file covers the pure-C surface:
 * include/uft/hal/uft_xum1541.h — the uft_iec_cmd_t enum + the
 * uft_cbm_drive_t enum.
 *
 * This is the C twin of diff.py for the C-HAL header: if a future edit
 * changes an IEC command byte, the build breaks here.
 *
 * Reproduce:
 *   gcc -std=c11 -I../../include -fsyntax-only test_xum1541_vectors.c
 *
 * Reference provenance: "recalled" — IEEE-488 / Commodore IEC bus
 * standard (Commodore 1541 User's Guide & Service Manual, 1982).
 * Not vendored. See audit/xum1541/extract_ref.py PROVENANCE.
 */
#include "uft/hal/uft_xum1541.h"

/* -- IEC bus ATN command bytes (uft_iec_cmd_t) ------------------------ *
 * These are the IEEE-488 / Commodore-serial primary/secondary address
 * command bytes. The device number (0-30) is OR'd into LISTEN/TALK; the
 * channel (0-15) is OR'd into OPEN/CLOSE/DATA. The base bytes are fixed
 * by the bus standard.
 */
_Static_assert(UFT_IEC_LISTEN   == 0x20, "IEC LISTEN base byte = 0x20");
_Static_assert(UFT_IEC_UNLISTEN == 0x3F, "IEC UNLISTEN (global) = 0x3F");
_Static_assert(UFT_IEC_TALK     == 0x40, "IEC TALK base byte = 0x40");
_Static_assert(UFT_IEC_UNTALK   == 0x5F, "IEC UNTALK (global) = 0x5F");
_Static_assert(UFT_IEC_OPEN     == 0xF0, "IEC OPEN  secondary = 0xF0");
_Static_assert(UFT_IEC_CLOSE    == 0xE0, "IEC CLOSE secondary = 0xE0");
_Static_assert(UFT_IEC_DATA     == 0x60, "IEC DATA  secondary = 0x60");

/* Cross-check: the C-HAL uft_iec_cmd_t values MUST agree with the
 * C++ xum1541_usb.h IEC_* constexprs (which diff.py validates against
 * the same reference). Two headers, one bus standard — they must not
 * drift apart. */
_Static_assert(UFT_IEC_LISTEN   == 0x20 && UFT_IEC_TALK == 0x40,
    "uft_xum1541.h IEC bytes must match xum1541_usb.h IEC_LISTEN/IEC_TALK");

/* -- CBM drive-type enum ordinal contract (uft_cbm_drive_t) ----------- *
 * UFT_CBM_DRIVE_AUTO must be 0 (zero-init / "auto-detect" default that
 * Xum1541ReadRequest::drive_type relies on — see xum1541_provider_v2.h:140
 * "drive_type = 0 (0 = auto)"). The rest are consecutive.
 */
_Static_assert(UFT_CBM_DRIVE_AUTO    == 0, "drive enum: AUTO must be 0");
_Static_assert(UFT_CBM_DRIVE_1541    == 1, "drive enum: 1541 follows AUTO");
_Static_assert(UFT_CBM_DRIVE_1541_II == 2, "drive enum: 1541-II");
_Static_assert(UFT_CBM_DRIVE_1571    == 4, "drive enum: 1571");
_Static_assert(UFT_CBM_DRIVE_1581    == 5, "drive enum: 1581");

/* Translation unit needs one external symbol to be non-empty under -c. */
int uft_audit_xum1541_vectors_ok(void) { return 0; }
