#!/usr/bin/env python3
"""
extract_ref.py — the official FC5025 reference values.

Emits the same JSON shape as extract_uft.py so diff.py can compare them
key-by-key.

PROVENANCE: mixed grade — see PROVENANCE dict below.

The FC5025 ("Device Side Data FC5025 USB 5.25\" Floppy Controller") speaks
a USB Mass-Storage-style CBW/CSW protocol. Two reference layers exist:

  1. USB identity (VID/PID) — grade "recalled".
     The FC5025 enumerates as VID 0x16C0 PID 0x06D6. 0x16C0 is the
     Van Ooijen Technische Informatica (V-USB / objdev) shared VID;
     0x06D6 is the FC5025's allocation. This is well-known from the
     open-source fc5025 driver (the `fc5025.h` in the Device Side Data
     Linux source release) and is reproduced here from memory — NOT
     vendored into this repo.

  2. CBW command opcodes + fcimage CLI argument grammar — grade
     "needs-source". The Device Side Data "FC5025 Command Set
     Specification v1309" documents the CBW opcode table (CMD_READ_FLEXIBLE
     etc.); the `fcimage` CLI grammar is documented in its man page / -h
     output. NEITHER is vendored, AND the UFT V2 provider does not contain
     a CBW opcode table or a CLI-argv table to diff against — both live
     inside an injected `Fc5025Runner` lambda that has no production
     construction site in the tree. The audit therefore cannot establish
     a byte-/arg-exact reference for D1's protocol layer. D1's
     protocol-opcode dimension is UNVERIFIED with a "needs-source" note.

  3. The disk-format enum (uft_fc_format_t) is a UFT-internal abstraction,
     not an FC5025 wire value — the FC5025 CBW protocol does not transmit
     a "format" byte the way UFT's header comment implies; format handling
     is a host-side decode concern. Graded "uft-internal": there is no
     external reference value, only an internal ordering contract.

Upgrading: vendor `fc5025.h` from the Device Side Data Linux source
release to lift the VID/PID row to "vendored"; vendor the Command Set
Specification v1309 + add a CBW opcode table to the provider (or to its
runner) to make the protocol layer auditable at all.
"""

import json
import sys

PROVENANCE = {
    "grade": "mixed",
    "usb_ids": "recalled",
    "cbw_opcodes": "needs-source",
    "cli_argv": "needs-source",
    "format_enum": "uft-internal",
    "project": "Device Side Data Inc. — FC5025",
    "files_recalled_from": ["fc5025.h (DSD Linux source release)"],
    "files_needed": [
        "FC5025 Command Set Specification v1309 (CBW opcode table)",
        "fcimage(1) man page or --help output (CLI argv grammar)",
    ],
    "note": "USB identity recalled; CBW/CLI layer not establishable — "
            "the UFT provider has no opcode/argv table, the dialogue is "
            "inside an un-constructed injected runner. See audit/README.md.",
}

# USB identity — recalled from the open-source fc5025 driver.
USB_IDS = {
    "UFT_FC5025_VID": 0x16C0,
    "UFT_FC5025_PID": 0x06D6,
}

# Disk-format enum: this is a UFT-internal abstraction. The reference
# here is only the *ordering contract* (AUTO must be 0 — it is the
# zero-initialised default). No external FC5025 wire value corresponds.
FORMAT_ENUM = {
    "UFT_FC_FMT_AUTO": {"ref": 0, "kind": "uft-internal-ordinal",
                        "note": "must be 0 (zero-init default)"},
    "UFT_FC_FMT_APPLE_DOS33": {"ref": 2, "kind": "uft-internal-ordinal",
                               "note": "provider default disk_format=0x02 "
                                       "must point here"},
}

# Provider-side default: the FC5025ProviderV2 ctor defaults disk_format
# to 0x02. The contract is that 0x02 == the ordinal of APPLE_DOS33 in
# uft_fc_format_t. This IS auditable (internal consistency).
DEFAULTS = {
    "disk_format_default": {"ref": 2, "kind": "internal-consistency",
                            "note": "ctor default 0x02 must equal the "
                                    "ordinal of UFT_FC_FMT_APPLE_DOS33"},
}


def main():
    json.dump({
        "_provenance": PROVENANCE,
        "usb_ids": USB_IDS,
        "format_enum": FORMAT_ENUM,
        "defaults": DEFAULTS,
    }, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
