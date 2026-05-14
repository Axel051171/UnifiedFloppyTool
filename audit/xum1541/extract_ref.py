#!/usr/bin/env python3
"""
extract_ref.py — the official XUM1541 / ZoomFloppy protocol reference.

Emits the same JSON shape as extract_uft.py so diff.py can compare them
key-by-key.

PROVENANCE: grade = "recalled".

The XUM1541 firmware command set, USB endpoint layout, and USB VID/PID
are defined in the OpenCBM project:
  - firmware command enum + endpoints: OpenCBM `xum1541/xum1541.h`
  - USB VID/PID: OpenCBM `xum1541/xum1541.h` (ZoomFloppy 0x16D0/0x04B2,
    XUM1541 0x16D0/0x0504, plus the DIY alt PID 0x0503)
  - IEC bus command bytes (LISTEN/TALK/...): the IEEE-488 / Commodore
    serial bus standard, documented in the Commodore 1541 User's Guide
    and Service Manual (1982).
  - CBM DOS status codes: Commodore DOS manuals (the error-channel table).
  - 1541/1571/1581 geometry: Commodore drive ROM listings.

These are well-known protocol constants reproduced from that project
and from the Commodore documentation. They are NOT vendored into this
repo — see audit/README.md "Reference provenance honesty". Upgrading to
"vendored" means checking in a copy of OpenCBM's xum1541.h + opencbm.h
and parsing them here instead of hard-coding.

The audited UFT file (src/hardware_providers/xum1541_usb.h) explicitly
states it "mirrors the values found in the OpenCBM project (opencbm.h,
xum1541.h, archlib.h)" — this reference checks whether that mirror is
faithful.
"""

import json
import sys

PROVENANCE = {
    "grade": "recalled",
    "project": "OpenCBM (xum1541 firmware) + Commodore IEC bus standard",
    "files_recalled_from": ["xum1541/xum1541.h", "include/opencbm.h",
                            "Commodore 1541 User's Guide / Service Manual"],
    "note": "well-known protocol constants; not vendored — see "
            "audit/README.md. UFT's xum1541_usb.h claims to mirror these.",
}

# USB identity — OpenCBM xum1541.h.
USB_IDS = {
    "USB_VID_ZOOMFLOPPY": 0x16D0,
    "USB_PID_ZOOMFLOPPY": 0x04B2,
    "USB_VID_XUM1541":    0x16D0,
    "USB_PID_XUM1541":    0x0504,
    "USB_PID_XUM1541_ALT": 0x0503,
}

# Bulk endpoints — OpenCBM xum1541 firmware descriptor.
ENDPOINTS = {
    "EP_BULK_IN":  0x81,   # EP 1 IN
    "EP_BULK_OUT": 0x02,   # EP 2 OUT
}

# XUM1541 firmware command codes (OpenCBM xum1541.h enum).
FW_COMMANDS = {
    "XUM1541_ECHO":            0,
    "XUM1541_READ":            1,
    "XUM1541_WRITE":           2,
    "XUM1541_TALK":            3,
    "XUM1541_LISTEN":          4,
    "XUM1541_UNTALK":          5,
    "XUM1541_UNLISTEN":        6,
    "XUM1541_OPEN":            7,
    "XUM1541_CLOSE":           8,
    "XUM1541_RESET":           9,
    "XUM1541_GET_EOI":         10,
    "XUM1541_CLEAR_EOI":       11,
    "XUM1541_PP_READ":         12,
    "XUM1541_PP_WRITE":        13,
    "XUM1541_IEC_POLL":        14,
    "XUM1541_IEC_WAIT":        15,
    "XUM1541_IEC_SETRELEASE":  16,
    "XUM1541_PARBURST_READ":   17,
    "XUM1541_PARBURST_WRITE":  18,
    "XUM1541_SRQBURST_READ":   19,
    "XUM1541_SRQBURST_WRITE":  20,
    "XUM1541_TAP_MOTOR_ON":    21,
    "XUM1541_TAP_MOTOR_OFF":   22,
    "XUM1541_TAP_PREPARE_CAPTURE": 23,
    "XUM1541_TAP_PREPARE_WRITE":   24,
    "XUM1541_TAP_GET_SENSE":       25,
    "XUM1541_TAP_WAIT_FOR_STOP_SENSE": 26,
    "XUM1541_TAP_WAIT_FOR_PLAY_SENSE": 27,
    "XUM1541_NIBTOOLS_READ":   28,
    "XUM1541_NIBTOOLS_WRITE":  29,
    "XUM1541_INIT":            30,
    "XUM1541_SHUTDOWN":        31,
    "XUM1541_CMD_MAX":         32,
}

# IEC bus command bytes (IEEE-488 / Commodore serial bus standard).
IEC_COMMANDS = {
    "IEC_LISTEN":   0x20,
    "IEC_UNLISTEN": 0x3F,
    "IEC_TALK":     0x40,
    "IEC_UNTALK":   0x5F,
    "IEC_OPEN":     0xF0,
    "IEC_CLOSE":    0xE0,
    "IEC_DATA":     0x60,
    "IEC_LINE_DATA":  0x01,
    "IEC_LINE_CLOCK": 0x02,
    "IEC_LINE_ATN":   0x04,
    "IEC_LINE_RESET": 0x08,
    "IEC_LINE_SRQ":   0x10,
}

# CBM DOS error-channel status codes (Commodore DOS manuals).
CBM_STATUS = {
    "CBM_STATUS_OK":              0,
    "CBM_STATUS_FILES_SCRATCHED": 1,
    "CBM_STATUS_READ_ERROR_20":   20,
    "CBM_STATUS_READ_ERROR_21":   21,
    "CBM_STATUS_READ_ERROR_22":   22,
    "CBM_STATUS_READ_ERROR_23":   23,
    "CBM_STATUS_READ_ERROR_24":   24,
    "CBM_STATUS_READ_ERROR_25":   25,
    "CBM_STATUS_WRITE_PROTECT":   26,
    "CBM_STATUS_READ_ERROR_27":   27,
    "CBM_STATUS_DISK_ID_MISMATCH": 29,
    "CBM_STATUS_SYNTAX_ERROR":    30,
    "CBM_STATUS_UNKNOWN_CMD":     31,
    "CBM_STATUS_RECORD_NOT_PRESENT": 50,
    "CBM_STATUS_OVERFLOW":        51,
    "CBM_STATUS_FILE_NOT_FOUND":  62,
    "CBM_STATUS_FILE_EXISTS":     63,
    "CBM_STATUS_DISK_FULL":       72,
    "CBM_STATUS_DRIVE_NOT_READY": 74,
}

# 1541/1571/1581 geometry (Commodore drive ROM listings).
GEOMETRY = {
    "CBM_1541_TRACKS":        35,
    "CBM_1541_TRACKS_EXT":    40,
    "CBM_1541_SECTOR_SIZE":   256,
    "CBM_1541_DISK_SIZE":     174848,
    "CBM_1541_DISK_SIZE_EXT": 196608,
    "CBM_1571_TRACKS":        35,
    "CBM_1571_DISK_SIZE":     349696,
    "CBM_1581_TRACKS":        80,
    "CBM_1581_SECTORS_PER_TRACK": 40,
    "CBM_1581_SECTOR_SIZE":   256,
    "CBM_1581_DISK_SIZE":     819200,
}


def main():
    json.dump({
        "_provenance": PROVENANCE,
        "usb_ids": USB_IDS,
        "endpoints": ENDPOINTS,
        "fw_commands": FW_COMMANDS,
        "iec_commands": IEC_COMMANDS,
        "cbm_status": CBM_STATUS,
        "geometry": GEOMETRY,
    }, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
