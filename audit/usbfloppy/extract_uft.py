#!/usr/bin/env python3
"""
extract_uft.py — pull USB-Floppy (UFI) protocol constants from the UFT source.

Reads, never writes. Emits a JSON dict on stdout: every wire-protocol
constant UFT uses to talk to a UFI (USB Floppy Interface) device, plus
where it was found (file:line). diff.py consumes this against extract_ref.py.

Unlike ADF-Copy, USB-Floppy DOES have a C-HAL: include/uft/hal/ufi.h
defines a real `uft_ufi_opcode_t` enum (SCSI-like CDB opcodes) and
src/hal/ufi.c builds the CDBs. The opcodes are byte-exact verifiable
against the USB-IF "UFI Command Specification Rev. 1.0".

What is scanned:
  - uft_ufi_opcode_t enum values        — real enum in ufi.h, byte-exact
  - INQUIRY CDB allocation-length (36)  — real literal in ufi.c
  - REQUEST SENSE CDB alloc-length (18) — real literal in ufi.c
  - READ(10) / WRITE(10) CDB length 10  — real literal in ufi.c
  - geometry inference table            — real literals in ufi.c / provider .cpp

Sources:
  include/uft/hal/ufi.h                          — opcode enum
  src/hal/ufi.c                                  — CDB builders
  src/hardware_providers/usbfloppy_provider_v2.cpp — geometry inference
"""

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
UFI_H = REPO / "include" / "uft" / "hal" / "ufi.h"
UFI_C = REPO / "src" / "hal" / "ufi.c"
PROV_CPP = REPO / "src" / "hardware_providers" / "usbfloppy_provider_v2.cpp"


def _scan_enum(text, prefix):
    """NAME = 0xValue,  inside an enum  ->  {NAME: [int, lineno]}"""
    out = {}
    for n, line in enumerate(text.splitlines(), 1):
        m = re.match(rf"\s*({re.escape(prefix)}\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)", line)
        if m:
            out[m.group(1)] = [int(m.group(2), 0), n]
    return out


def main():
    if not UFI_H.exists() or not UFI_C.exists():
        print(f"extract_uft.py: missing {UFI_H} or {UFI_C}", file=sys.stderr)
        return 2
    htext = UFI_H.read_text(encoding="utf-8", errors="replace")
    ctext = UFI_C.read_text(encoding="utf-8", errors="replace")

    result = {
        "_source": {
            "ufi_h": str(UFI_H.relative_to(REPO)),
            "ufi_c": str(UFI_C.relative_to(REPO)),
            "provider_cpp": str(PROV_CPP.relative_to(REPO)),
        },
        "_note": "USB-Floppy HAS a C-HAL (ufi.h opcode enum, ufi.c CDB builders). "
                 "Opcodes are byte-exact verifiable. The platform backend "
                 "(SG_IO / SCSI_PASS_THROUGH) is ABSENT — uft_ufi_backend_init() "
                 "is a stub returning -1; g_ops stays NULL on every OS.",
        "opcodes": {},
        "cdb_lengths": {},
        "geometry": {},
    }

    # Real enum — byte-exact verifiable against the UFI spec.
    result["opcodes"] = _scan_enum(htext, "UFT_UFI_")

    # CDB allocation lengths — real literals in ufi.c.
    # INQUIRY: cdb[6] = { 0x12, 0,0,0, 36, 0 }
    for n, line in enumerate(ctext.splitlines(), 1):
        m = re.search(r"UFT_UFI_INQUIRY,\s*0,\s*0,\s*0,\s*(\d+)", line)
        if m:
            result["cdb_lengths"]["INQUIRY_ALLOC_LEN"] = [int(m.group(1)), n]
        m = re.search(r"UFT_UFI_REQUEST_SENSE,\s*0,\s*0,\s*0,\s*(\d+)", line)
        if m:
            result["cdb_lengths"]["REQUEST_SENSE_ALLOC_LEN"] = [int(m.group(1)), n]
        # READ(10) / WRITE(10) CDB is uint8_t cdb[10]
        m = re.search(r"uint8_t\s+cdb\[(\d+)\]\s*=\s*\{0\}", line)
        if m and "READ10_WRITE10_CDB_LEN" not in result["cdb_lengths"]:
            result["cdb_lengths"]["READ10_WRITE10_CDB_LEN"] = [int(m.group(1)), n]

    # Geometry inference: 1.44M => 2880 LBA / 18 spt, etc. Pull from provider .cpp.
    # The .cpp writes `if (result.total_lba == 2880) {` then `spt = 18;` on the
    # next line, so scan across the line boundary.
    if PROV_CPP.exists():
        ptext = PROV_CPP.read_text(encoding="utf-8", errors="replace")
        lines = ptext.splitlines()
        for n, line in enumerate(lines, 1):
            m = re.search(r"total_lba\s*==\s*(\d+)", line)
            if not m:
                continue
            lba = int(m.group(1))
            # Look on this line and the next two for `spt = N;`
            window = " ".join(lines[n - 1:n + 2])
            sm = re.search(r"spt\s*=\s*(\d+)\s*;", window)
            if sm:
                result["geometry"][f"SPT_FOR_{lba}_LBA"] = [int(sm.group(1)), n]

    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
