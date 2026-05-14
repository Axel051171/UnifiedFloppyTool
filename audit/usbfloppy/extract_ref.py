#!/usr/bin/env python3
"""
extract_ref.py — the official USB-Floppy Interface (UFI) protocol reference.

Emits the same JSON shape as extract_uft.py so diff.py can compare them
key-by-key.

PROVENANCE: grade = "recalled".

The UFI command set is defined in:
  "Universal Serial Bus Mass Storage Class UFI Command Specification
   Rev. 1.0", USB Implementers Forum (USB-IF), 1998.

UFI is a SCSI-like subset: the opcodes are the standard SCSI/MMC opcodes
(TEST UNIT READY 0x00, INQUIRY 0x12, READ(10) 0x28, WRITE(10) 0x2A, ...).
These are well-known, publicly documented values reproduced here from
the spec — NOT vendored. The spec PDF is publicly available; checking it
in and parsing it would upgrade this to grade "vendored".

Three reference tiers:
  1. UFI opcodes        — grade "recalled". Standard SCSI opcodes; the
     spec is public. Byte-exact verifiable.
  2. CDB lengths        — grade "recalled". The 6-byte CDB for INQUIRY /
     REQUEST SENSE and the 10-byte CDB for READ(10) / WRITE(10) are
     fixed by the SCSI command-descriptor-block format. INQUIRY
     allocation length 36 = standard inquiry-data length; REQUEST SENSE
     allocation length 18 = standard sense-data length.
  3. Geometry inference — grade "recalled". 3.5" floppy geometries
     (1.44M = 2880 LBA / 18 spt, 720K = 1440 LBA / 9 spt, 2.88M =
     5760 LBA / 36 spt) are universal PC-floppy facts, not UFI-specific.
"""

import json
import sys

PROVENANCE = {
    "grade": "recalled",
    "project": "USB-IF UFI Command Specification Rev. 1.0 (1998)",
    "baseline": "Standard SCSI/MMC opcode subset used by UFI",
    "spec_status": "VendorDocumented (publicly available USB-IF spec)",
    "note": "UFI opcodes are standard SCSI opcodes; the spec is public but "
            "not vendored into the repo — recalled-grade. CDB lengths are "
            "fixed by SCSI CDB format. Geometry table is universal PC-floppy "
            "knowledge.",
    "tiers": {
        "opcodes": "recalled (standard SCSI opcodes, USB-IF UFI spec)",
        "cdb_lengths": "recalled (fixed by SCSI CDB format)",
        "geometry": "recalled (universal 3.5\" PC-floppy geometries)",
    },
}

# UFI opcodes — standard SCSI/MMC opcode values (USB-IF UFI spec Rev. 1.0).
OPCODES = {
    "UFT_UFI_TEST_UNIT_READY": 0x00,
    "UFT_UFI_REQUEST_SENSE":   0x03,
    "UFT_UFI_FORMAT_UNIT":     0x04,
    "UFT_UFI_INQUIRY":         0x12,
    "UFT_UFI_MODE_SELECT_6":   0x15,
    "UFT_UFI_MODE_SENSE_6":    0x1A,
    "UFT_UFI_START_STOP":      0x1B,
    "UFT_UFI_READ_FORMAT_CAP": 0x23,
    "UFT_UFI_READ_CAPACITY":   0x25,
    "UFT_UFI_READ_10":         0x28,
    "UFT_UFI_WRITE_10":        0x2A,
    "UFT_UFI_VERIFY_10":       0x2F,
}

# CDB allocation lengths / sizes — fixed by the SCSI CDB format.
CDB_LENGTHS = {
    "INQUIRY_ALLOC_LEN":        36,  # standard inquiry data length
    "REQUEST_SENSE_ALLOC_LEN":  18,  # standard sense data length
    "READ10_WRITE10_CDB_LEN":   10,  # READ(10)/WRITE(10) CDB is 10 bytes
}

# Sectors-per-track for the standard 3.5" floppy geometries.
GEOMETRY = {
    "SPT_FOR_2880_LBA": 18,   # 1.44M HD : 80 cyl x 2 head x 18 spt x 512 B
    "SPT_FOR_1440_LBA": 9,    # 720K  DD : 80 cyl x 2 head x  9 spt x 512 B
    "SPT_FOR_5760_LBA": 36,   # 2.88M ED : 80 cyl x 2 head x 36 spt x 512 B
}


def main():
    json.dump({
        "_provenance": PROVENANCE,
        "opcodes": OPCODES,
        "cdb_lengths": CDB_LENGTHS,
        "geometry": GEOMETRY,
    }, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
