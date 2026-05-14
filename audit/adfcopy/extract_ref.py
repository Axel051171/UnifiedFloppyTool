#!/usr/bin/env python3
"""
extract_ref.py — the official ADF-Copy / ADF-Drive protocol reference.

Emits the same JSON shape as extract_uft.py so diff.py can compare them
key-by-key.

PROVENANCE: grade = "needs-source" (mixed).

ADF-Copy ("ADFCopy" / "ADF-Drive") is a DIY open-source Teensy-based Amiga
3.5" floppy controller. There is NO official vendor specification — the
SpecStatus in the V2 provider is `CommunityConsensus`. The protocol is
documented only in the project's own open-source firmware and the (now
DELETED) UFT V1 adfcopyhardwareprovider.{h,cpp}.

Three reference tiers here:

  1. RESP_* status bytes ('O','E','D')   — grade "recalled". These are
     ASCII status bytes; their values are tautological (the byte IS the
     letter). Verifiable byte-exact: 'O'==0x4F etc.
  2. sample clock / geometry             — grade "recalled". 40 MHz / 25 ns
     and Amiga 80cyl×2head 300 RPM are well-known Amiga-floppy facts.
  3. ADFC_CMD_* opcodes                  — grade "needs-source". The numeric
     opcode values (0x00..0x0B) appear ONLY in UFT's own comments. The
     authoritative adfcopyhardwareprovider.h was deleted (MF-169); no
     vendored firmware copy exists in the repo. These rows are reported
     UNVERIFIED — they cannot be cross-checked against anything but UFT's
     own recollection of a file it deleted.
  4. VID:PID 0x16C0:0x0483               — grade "recalled". This is the
     generic PJRC Teensy USB ID (shared with Applesauce — see MF-146).

Upgrading to "vendored" means checking in a copy of the ADF-Copy firmware
protocol header and parsing it here instead of hard-coding.
"""

import json
import sys

PROVENANCE = {
    "grade": "needs-source",
    "project": "ADF-Copy / ADF-Drive (DIY open-source Teensy Amiga controller)",
    "baseline": "UFT V1 adfcopyhardwareprovider.h — DELETED by MF-169/P1.17",
    "spec_status": "CommunityConsensus (no official vendor spec exists)",
    "note": "RESP_* + clock + geometry are recalled-grade; ADFC_CMD_* opcodes "
            "are needs-source (only survive as UFT comments, authoritative "
            "header deleted) — those rows are UNVERIFIED.",
    "tiers": {
        "responses": "recalled (ASCII status bytes — tautological)",
        "timing": "recalled (Amiga 40 MHz / 25 ns standard)",
        "geometry": "recalled (Amiga 80 cyl x 2 head, 300 RPM)",
        "cmd_opcodes": "needs-source (header deleted; UNVERIFIED)",
        "usb_ids": "recalled (generic PJRC Teensy ID, shared w/ Applesauce)",
    },
}

# Response status bytes — ASCII, byte-exact tautology.
RESPONSES = {
    "RESP_OK":     ord('O'),   # 0x4F
    "RESP_ERROR":  ord('E'),   # 0x45
    "RESP_NODISK": ord('D'),   # 0x44
}

# Sample clock + derived ns/tick — Amiga-floppy standard, SCP-compatible.
TIMING = {
    "ADFC_SAMPLE_CLOCK_HZ": 40000000.0,   # 40 MHz
    "ADFC_SAMPLE_NS":       25.0,         # 1e9 / 40e6
}

# Amiga 3.5" DD geometry.
GEOMETRY = {
    "ADFC_STD_CYLINDERS": 80,
    "ADFC_HEADS":         2,
}

# ADFC_CMD_* opcodes — needs-source. The numbers below are reproduced from
# UFT's own comments; they are NOT independently verifiable. diff.py marks
# these UNVERIFIED rather than PASS even on numeric match.
CMD_OPCODES = {
    "ADFC_CMD_PING":       {"ref": 0x00, "kind": "needs-source"},
    "ADFC_CMD_INIT":       {"ref": 0x01, "kind": "needs-source"},
    "ADFC_CMD_SEEK":       {"ref": 0x02, "kind": "needs-source"},
    "ADFC_CMD_READ_FLUX":  {"ref": 0x06, "kind": "needs-source"},
    "ADFC_CMD_GET_STATUS": {"ref": 0x0B, "kind": "needs-source"},
}

# Generic PJRC Teensy USB ID — shared with Applesauce (MF-146 disambiguation).
USB_IDS = {
    "ADFC_VID_PID": [0x16C0, 0x0483],
}


def main():
    json.dump({
        "_provenance": PROVENANCE,
        "responses": RESPONSES,
        "timing": TIMING,
        "geometry": GEOMETRY,
        "cmd_opcodes": CMD_OPCODES,
        "usb_ids": USB_IDS,
    }, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
