#!/usr/bin/env python3
"""
extract_uft.py — pull XUM1541 / ZoomFloppy protocol constants from UFT.

Reads, never writes. Emits a JSON dict on stdout: every wire-protocol
constant UFT uses to talk to an XUM1541 / ZoomFloppy device, plus where
it was found (file:line). diff.py consumes this against extract_ref.py.

Sources:
  src/hardware_providers/xum1541_usb.h  — USB VID/PID, EP addresses,
      XUM1541 firmware command codes, IEC bus command bytes, CBM DOS
      status codes, drive geometry constants.
  src/hardwaretab.cpp                   — GUI-side VID/PID port hint.

The XUM1541 V2 provider is a native-USB backend (the real transport is
the OpenCBM dynamic library loaded by OpenCbmLibrary in xum1541_usb.h).
Unlike a CLI wrapper there IS a byte-exact constant table to diff.
"""

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
USB_H = REPO / "src" / "hardware_providers" / "xum1541_usb.h"
HWTAB = REPO / "src" / "hardwaretab.cpp"


def _scan_constexpr(text, type_hint, prefix):
    """constexpr <type> PREFIX_NAME = 0xVALUE;  ->  {NAME: [int, lineno]}"""
    out = {}
    pat = re.compile(
        rf"constexpr\s+{type_hint}\s+({re.escape(prefix)}\w+)\s*=\s*"
        rf"(0x[0-9A-Fa-f]+|\d+)")
    for n, line in enumerate(text.splitlines(), 1):
        m = pat.search(line)
        if m:
            out[m.group(1)] = [int(m.group(2), 0), n]
    return out


def _scan_enum_members(text, prefix):
    """NAME = 0xVALUE,  inside an enum  ->  {NAME: [int, lineno]}"""
    out = {}
    pat = re.compile(rf"\b({re.escape(prefix)}\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)")
    for n, line in enumerate(text.splitlines(), 1):
        # Skip comment-only lines.
        code = line.split("/*", 1)[0].split("//", 1)[0]
        m = pat.search(code)
        if m:
            out[m.group(1)] = [int(m.group(2), 0), n]
    return out


def main():
    if not USB_H.exists():
        print(f"extract_uft.py: missing {USB_H}", file=sys.stderr)
        return 2
    text = USB_H.read_text(encoding="utf-8", errors="replace")

    result = {
        "_source": {
            "usb_h": str(USB_H.relative_to(REPO)),
            "hwtab": str(HWTAB.relative_to(REPO)),
        },
        "usb_ids": {},
        "endpoints": {},
        "fw_commands": {},
        "iec_commands": {},
        "cbm_status": {},
        "geometry": {},
    }

    # USB VID/PID (constexpr uint16_t)
    ids = _scan_constexpr(text, "uint16_t", "USB_")
    for name in ("USB_VID_ZOOMFLOPPY", "USB_PID_ZOOMFLOPPY",
                 "USB_VID_XUM1541", "USB_PID_XUM1541", "USB_PID_XUM1541_ALT"):
        if name in ids:
            result["usb_ids"][name] = ids[name]

    # Endpoints (constexpr uint8_t)
    eps = _scan_constexpr(text, "uint8_t", "EP_")
    result["endpoints"] = eps

    # XUM1541 firmware command enum (enum Xum1541Cmd : uint8_t)
    result["fw_commands"] = _scan_enum_members(text, "XUM1541_")

    # IEC bus command bytes (constexpr uint8_t IEC_*)
    iec = _scan_constexpr(text, "uint8_t", "IEC_")
    result["iec_commands"] = iec

    # CBM DOS status codes (enum CbmDosStatus : uint8_t)
    result["cbm_status"] = _scan_enum_members(text, "CBM_STATUS_")

    # Drive geometry (constexpr int)
    geo = _scan_constexpr(text, "int", "CBM_15")
    result["geometry"] = geo

    # GUI-side VID/PID hint in hardwaretab.cpp detectSerialPorts().
    # The `vid == .. && pid == ..` test and the `controllerHint = "..."`
    # assignment are on adjacent lines, so scan a small window.
    if HWTAB.exists():
        lines = HWTAB.read_text(encoding="utf-8", errors="replace").splitlines()
        for n, line in enumerate(lines, 1):
            m = re.search(r"vid\s*==\s*(0x[0-9A-Fa-f]+)\s*&&\s*"
                          r"pid\s*==\s*(0x[0-9A-Fa-f]+)", line)
            if not m:
                continue
            window = "\n".join(lines[n - 1:n + 3])  # this line + next 3
            if "ZoomFloppy" in window or "XUM1541" in window:
                result["usb_ids"]["hwtab_hint_vid_pid"] = [
                    [int(m.group(1), 0), int(m.group(2), 0)], n]

    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
