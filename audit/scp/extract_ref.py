#!/usr/bin/env python3
"""
extract_ref.py — the official SuperCard Pro protocol reference.

Emits the same JSON shape as extract_uft.py so diff.py can compare them
key-by-key.

PROVENANCE: grade = "needs-source" for the USB command bytes and
            "recalled" for the sample clock + geometry limits.

The SuperCard Pro is documented in the "SuperCard Pro SDK v1.7"
(cbmstuff.com, December 2015). That SDK defines:
  - the command protocol (a single-byte command set, e.g. CMD_READFLUX,
    CMD_WRITEFLUX, CMD_SELDRIVE, ... with a length+payload+checksum frame)
  - the 40 MHz / 25 ns sample clock
  - the .scp file-format container

The SDK is NOT vendored into this repo. Two distinct provenance grades
apply:

1. SAMPLE CLOCK + GEOMETRY ("recalled"):
   The 40 MHz / 25 ns sample clock and the 84-cylinder × 2-side geometry
   are well-known SCP facts, cross-checked in-repo against
   src/samdisk/scp.cpp ("25ns sampling time") and the
   uft_scp_direct.h header comment. recalled-grade.

2. USB COMMAND BYTES + VID/PID + ENDPOINTS ("needs-source"):
   uft_scp_direct.h hard-codes CMD_SET_CONTROL=0x02, CMD_SELECT_DRIVE=0x03,
   CMD_READ_FLUX=0x04, CMD_WRITE_FLUX=0x05, CMD_DESELECT_DRIVE=0x09,
   CMD_GET_INFO=0x40, VID=0x16C0, PID=0x0753, BULK_IN=0x81, BULK_OUT=0x01.

   These could NOT be confirmed against a vendored SCP SDK. The published
   SuperCard Pro SDK v1.7 command set uses a DIFFERENT, larger opcode
   space (the SDK's CMD_* values are not in the 0x02-0x05 range used
   here; the SDK frames are [cmd][len][payload...][checksum] over a
   serial/FTDI link, not raw USB-Bulk vendor commands). The header's own
   comment says "vendor command 0x02 set-control ... See a8rawconv/scp.cpp
   for the port source" — a8rawconv is itself a third-party reimplementation,
   not the vendor SDK.

   Therefore the USB-command-byte rows are graded "needs-source": the
   reference cannot be established without vendoring the SCP SDK v1.7
   command header (or a8rawconv/scp.cpp). diff.py emits these as
   UNVERIFIED, NOT as PASS — a PASS here would be a fabricated diff.

   Upgrading to "vendored" means checking in the SCP SDK command
   definitions (or a8rawconv/scp.cpp) and parsing them here.
"""

import json
import sys

PROVENANCE = {
    "grade": "mixed: recalled (clock/geometry) + needs-source (USB cmd bytes)",
    "project": "cbmstuff SuperCard Pro SDK v1.7 (Dec 2015)",
    "baseline": "SCP SDK v1.7 + samdisk/a8rawconv scp.cpp ports",
    "files_recalled_from": ["SCP SDK v1.7 command header (NOT vendored)",
                            "samdisk/scp.cpp (in-repo cross-check, clock only)"],
    "note": "USB command bytes are needs-source — see audit/README.md "
            "'Reference provenance honesty'. The header's 0x02-0x05 opcodes "
            "do not match the published SCP SDK v1.7 command set; a vendored "
            "SDK header is required to confirm or refute them.",
}

# USB command bytes: needs-source. The reference VALUE cannot be
# established without a vendored SCP SDK header. Marked with kind so
# diff.py emits UNVERIFIED (not PASS, not FAIL).
CMD_OPCODES = {
    "UFT_SCP_CMD_SET_CONTROL":    {"ref": "needs-source — SCP SDK v1.7 command header not vendored",
                                   "kind": "needs-source"},
    "UFT_SCP_CMD_SELECT_DRIVE":   {"ref": "needs-source — SCP SDK v1.7 command header not vendored",
                                   "kind": "needs-source"},
    "UFT_SCP_CMD_READ_FLUX":      {"ref": "needs-source — SCP SDK v1.7 command header not vendored",
                                   "kind": "needs-source"},
    "UFT_SCP_CMD_WRITE_FLUX":     {"ref": "needs-source — SCP SDK v1.7 command header not vendored",
                                   "kind": "needs-source"},
    "UFT_SCP_CMD_DESELECT_DRIVE": {"ref": "needs-source — SCP SDK v1.7 command header not vendored",
                                   "kind": "needs-source"},
    "UFT_SCP_CMD_GET_INFO":       {"ref": "needs-source — SCP SDK v1.7 command header not vendored",
                                   "kind": "needs-source"},
}

# USB identity: VERIFIED (SCP-D4-1 RESOLVED, audit ARCH-7-B / MF-212).
# The header once said 0x16C0/0x0753 and disagreed with the GUI port-
# hint; the real device descriptor (USB\VID_16D0&PID_0F8C) confirmed the
# GUI hint was correct. uft_scp_direct.h was corrected and now single-
# sources the value. Plain ints => diff.py compares them => PASS rows
# (no longer needs-source UNVERIFIED).
USB_IDS = {
    "UFT_SCP_USB_VID": 0x16D0,
    "UFT_SCP_USB_PID": 0x0F8C,
}

# USB Bulk endpoints: needs-source.
USB_ENDPOINTS = {
    "UFT_SCP_BULK_IN_EP":  {"ref": "needs-source — SCP interface descriptor not vendored",
                            "kind": "needs-source"},
    "UFT_SCP_BULK_OUT_EP": {"ref": "needs-source — SCP interface descriptor not vendored",
                            "kind": "needs-source"},
}

# Sample clock + geometry: recalled. These ARE well-known SCP facts and
# are cross-checked in-repo (samdisk/scp.cpp, header comment).
TIMING = {
    # 40 MHz sample clock => 25 ns per tick. Confirmed in-repo.
    "UFT_SCP_FLUX_NS_PER_SAMPLE": 25,
    # 84 cylinders * 2 sides - 1 = 167. Standard SCP geometry.
    "UFT_SCP_MAX_TRACK_INDEX": 167,
    # SCP captures up to 5 revolutions per command (SDK limit).
    "UFT_SCP_MAX_REVOLUTIONS": 5,
    # UFT-chosen default; not a protocol constant. Graded device/impl choice.
    "UFT_SCP_DEFAULT_REVOLUTIONS": {"ref": "UFT implementation choice (3) — not a "
                                           "fixed SCP protocol constant",
                                    "kind": "impl-choice"},
}


def main():
    json.dump({
        "_provenance": PROVENANCE,
        "usb_ids": USB_IDS,
        "usb_endpoints": USB_ENDPOINTS,
        "cmd_opcodes": CMD_OPCODES,
        "timing": TIMING,
    }, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
