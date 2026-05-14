#!/usr/bin/env python3
"""
extract_uft.py — pull SuperCard Pro protocol constants from the UFT source.

Reads, never writes. Emits a JSON dict on stdout: every wire-protocol
constant UFT uses to talk to a SuperCard Pro device, plus where it was
found (file:line). diff.py consumes this against extract_ref.py.

SCP is a NATIVE backend: src/hal/uft_scp_direct.c talks to the device
over libusb (USB Bulk endpoints), not a CLI subprocess. The constants
below are the USB command bytes + endpoints + timing.

Sources:
  include/uft/hal/uft_scp_direct.h  — USB VID/PID, endpoints, command
                                       bytes, sample clock, limits
  src/hardwaretab.cpp               — GUI-side VID/PID for port hinting
"""

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
HEADER = REPO / "include" / "uft" / "hal" / "uft_scp_direct.h"
HWTAB = REPO / "src" / "hardwaretab.cpp"


def _scan_defines(text, prefix):
    """#define PREFIX_NAME  0xValue   ->  {NAME: [int, lineno]}"""
    out = {}
    for n, line in enumerate(text.splitlines(), 1):
        m = re.match(rf"\s*#define\s+({re.escape(prefix)}\w+)\s+(-?0x[0-9A-Fa-f]+|-?\d+)",
                     line)
        if m:
            out[m.group(1)] = [int(m.group(2), 0), n]
    return out


def main():
    if not HEADER.exists():
        print(f"extract_uft.py: missing {HEADER}", file=sys.stderr)
        return 2
    htext = HEADER.read_text(encoding="utf-8", errors="replace")

    result = {
        "_source": {
            "header": str(HEADER.relative_to(REPO)),
            "hwtab": str(HWTAB.relative_to(REPO)),
        },
        "usb_ids": {},
        "usb_endpoints": {},
        "cmd_opcodes": {},
        "timing": {},
    }

    defs = _scan_defines(htext, "UFT_SCP_")
    for name in ("UFT_SCP_USB_VID", "UFT_SCP_USB_PID"):
        if name in defs:
            result["usb_ids"][name] = defs[name]
    for name in ("UFT_SCP_BULK_IN_EP", "UFT_SCP_BULK_OUT_EP"):
        if name in defs:
            result["usb_endpoints"][name] = defs[name]
    for name in ("UFT_SCP_CMD_READ_FLUX", "UFT_SCP_CMD_WRITE_FLUX",
                 "UFT_SCP_CMD_SELECT_DRIVE", "UFT_SCP_CMD_SET_CONTROL",
                 "UFT_SCP_CMD_DESELECT_DRIVE", "UFT_SCP_CMD_GET_INFO"):
        if name in defs:
            result["cmd_opcodes"][name] = defs[name]
    for name in ("UFT_SCP_FLUX_NS_PER_SAMPLE", "UFT_SCP_MAX_TRACK_INDEX",
                 "UFT_SCP_MAX_REVOLUTIONS", "UFT_SCP_DEFAULT_REVOLUTIONS"):
        if name in defs:
            result["timing"][name] = defs[name]

    # GUI-side VID/PID hint in detectSerialPorts()
    if HWTAB.exists():
        for n, line in enumerate(HWTAB.read_text(encoding="utf-8",
                                                 errors="replace").splitlines(), 1):
            m = re.search(r"vid\s*==\s*(0x[0-9A-Fa-f]+)\s*&&\s*pid\s*==\s*(0x[0-9A-Fa-f]+)",
                          line)
            if m and ("SuperCard" in line or "SCP" in line):
                result["usb_ids"]["hwtab_vid_pid"] = [
                    [int(m.group(1), 0), int(m.group(2), 0)], n
                ]

    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
