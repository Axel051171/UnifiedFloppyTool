#!/usr/bin/env python3
"""
extract_uft.py — pull Greaseweazle protocol constants from the UFT source.

Reads, never writes. Emits a JSON dict on stdout: every wire-protocol
constant UFT uses to talk to a Greaseweazle device, plus where it was
found (file:line). diff.py consumes this against extract_ref.py.

Sources:
  include/uft/hal/uft_greaseweazle_full.h  — opcodes, ACKs, VID/PID, freqs
  src/hardwaretab.cpp                      — GUI-side VID/PID for port hinting
"""

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
HEADER = REPO / "include" / "uft" / "hal" / "uft_greaseweazle_full.h"
HWTAB = REPO / "src" / "hardwaretab.cpp"


def _scan_defines(text, prefix):
    """#define PREFIX_NAME  0xValue   ->  {NAME: (int, lineno)}"""
    out = {}
    for n, line in enumerate(text.splitlines(), 1):
        m = re.match(rf"\s*#define\s+({re.escape(prefix)}\w+)\s+(-?0x[0-9A-Fa-f]+|-?\d+)",
                     line)
        if m:
            out[m.group(1)] = [int(m.group(2), 0), n]
    return out


def _scan_enum(text, prefix):
    """NAME = 0xValue,   inside an enum  ->  {NAME: (int, lineno)}"""
    out = {}
    for n, line in enumerate(text.splitlines(), 1):
        m = re.match(rf"\s*({re.escape(prefix)}\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)", line)
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
        "cmd_opcodes": {},
        "ack_codes": {},
        "timing": {},
    }

    defs = _scan_defines(htext, "UFT_GW_")
    for name in ("UFT_GW_USB_VID", "UFT_GW_USB_PID", "UFT_GW_USB_PID_F7"):
        if name in defs:
            result["usb_ids"][name] = defs[name]
    for name in ("UFT_GW_SAMPLE_FREQ_HZ", "UFT_GW_SAMPLE_FREQ_F7_PLUS",
                 "UFT_GW_MAX_CYLINDERS", "UFT_GW_MAX_REVOLUTIONS",
                 "UFT_GW_MOTOR_SPINUP_MS", "UFT_GW_SEEK_SETTLE_MS"):
        if name in defs:
            result["timing"][name] = defs[name]

    result["cmd_opcodes"] = _scan_enum(htext, "UFT_GW_CMD_")
    result["ack_codes"] = _scan_enum(htext, "UFT_GW_ACK_")

    # GUI-side VID/PID hint in detectSerialPorts()
    if HWTAB.exists():
        for n, line in enumerate(HWTAB.read_text(encoding="utf-8",
                                                 errors="replace").splitlines(), 1):
            m = re.search(r"vid\s*==\s*(0x[0-9A-Fa-f]+)\s*&&\s*pid\s*==\s*(0x[0-9A-Fa-f]+)",
                          line)
            if m and "Greaseweazle" in line:
                result["usb_ids"]["hwtab_vid_pid"] = [
                    [int(m.group(1), 0), int(m.group(2), 0)], n
                ]

    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
