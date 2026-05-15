#!/usr/bin/env python3
"""
extract_uft.py — pull FC5025 protocol constants from the UFT source.

Reads, never writes. Emits a JSON dict on stdout: every wire-protocol
constant UFT uses to identify / talk to an FC5025 device, plus where it
was found (file:line). diff.py consumes this against extract_ref.py.

Sources:
  include/uft/hal/uft_fc5025.h               — USB VID/PID, format enum
  src/hardware_providers/fc5025_provider_v2.h — runner-side default format
  src/hardware_providers/fc5025_provider_v2.cpp — VID/PID cited in detect path

Note on scope: the FC5025 V2 provider does NOT contain a CBW opcode table
or a fcimage CLI-argv table — the actual hardware dialogue is delegated to
an injected `Fc5025Runner` lambda that the audit could not locate any
production construction of. The only byte-/value-exact surface UFT
exposes is the USB identity (VID/PID) and the disk-format enum default.
That is what this extractor pulls. See REPORT.md D1.
"""

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
HEADER = REPO / "include" / "uft" / "hal" / "uft_fc5025.h"
PROV_H = REPO / "src" / "hardware_providers" / "fc5025_provider_v2.h"
PROV_C = REPO / "src" / "hardware_providers" / "fc5025_provider_v2.cpp"


def _scan_defines(text, prefix):
    """#define PREFIX_NAME  0xValue  ->  {NAME: [int, lineno]}"""
    out = {}
    for n, line in enumerate(text.splitlines(), 1):
        m = re.match(rf"\s*#define\s+({re.escape(prefix)}\w+)\s+"
                     rf"(-?0x[0-9A-Fa-f]+|-?\d+)", line)
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
            "provider_h": str(PROV_H.relative_to(REPO)),
            "provider_c": str(PROV_C.relative_to(REPO)),
        },
        "usb_ids": {},
        "format_enum": {},
        "defaults": {},
    }

    defs = _scan_defines(htext, "UFT_FC5025_")
    for name in ("UFT_FC5025_VID", "UFT_FC5025_PID"):
        if name in defs:
            result["usb_ids"][name] = defs[name]

    # Format enum: UFT_FC_FMT_* — collect ordinal positions (auto-numbered).
    # The members are consecutive UFT_FC_FMT_* lines; ordinals auto-increment
    # from 0 unless an explicit "= N" is present (only AUTO = 0 here).
    ordinal = 0
    for n, line in enumerate(htext.splitlines(), 1):
        m = re.match(r"\s*(UFT_FC_FMT_\w+)\s*(?:=\s*(\d+))?\s*,?", line)
        if m:
            if m.group(2) is not None:
                ordinal = int(m.group(2))
            result["format_enum"][m.group(1)] = [ordinal, n]
            ordinal += 1

    # Provider-side default disk_format code.
    if PROV_H.exists():
        for n, line in enumerate(PROV_H.read_text(encoding="utf-8",
                                 errors="replace").splitlines(), 1):
            m = re.search(r"disk_format\s*=\s*(0x[0-9A-Fa-f]+|\d+)", line)
            if m:
                result["defaults"]["disk_format_default"] = [
                    int(m.group(1), 0), n]
                break

    # VID/PID as cited in the cpp detect path (string literal cross-check).
    if PROV_C.exists():
        for n, line in enumerate(PROV_C.read_text(encoding="utf-8",
                                 errors="replace").splitlines(), 1):
            m = re.search(r"VID:PID\s*(0x[0-9A-Fa-f]+):(0x[0-9A-Fa-f]+)", line)
            if m:
                result["usb_ids"]["cpp_cited_vid_pid"] = [
                    [int(m.group(1), 0), int(m.group(2), 0)], n]
                break

    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
