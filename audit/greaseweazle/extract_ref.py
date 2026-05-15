#!/usr/bin/env python3
"""
extract_ref.py — the official Greaseweazle protocol reference.

Emits the same JSON shape as extract_uft.py so diff.py can compare them
key-by-key.

PROVENANCE: grade = "recalled".
The Greaseweazle USB command protocol is defined in the official
firmware/host project keirf/greaseweazle:
  - command opcodes + ACK codes: firmware `inc/cmd.h`, host
    `src/greaseweazle/usb.py` (class `Cmd`, class `Ack`)
  - USB VID/PID: pid.codes-allocated 0x1209 / 0x4D69
The values below are well-known protocol constants reproduced from that
project. They are NOT vendored into this repo — see audit/README.md
"Reference provenance honesty". Upgrading to grade "vendored" means
checking in a copy of cmd.h and parsing it here instead of hard-coding.

Reference baseline: greaseweazle v1.23 era protocol (the gw_corpus
frozen version, tests/gw_corpus/).
"""

import json
import sys

PROVENANCE = {
    "grade": "recalled",
    "project": "keirf/greaseweazle",
    "baseline": "v1.23 era protocol",
    "files_recalled_from": ["inc/cmd.h", "src/greaseweazle/usb.py"],
    "note": "well-known protocol constants; not vendored — see audit/README.md",
}

# Official Cmd opcodes (firmware inc/cmd.h / host usb.py class Cmd).
CMD_OPCODES = {
    "UFT_GW_CMD_GET_INFO": 0x00,
    "UFT_GW_CMD_UPDATE": 0x01,
    "UFT_GW_CMD_SEEK": 0x02,
    "UFT_GW_CMD_HEAD": 0x03,
    "UFT_GW_CMD_SET_PARAMS": 0x04,
    "UFT_GW_CMD_GET_PARAMS": 0x05,
    "UFT_GW_CMD_MOTOR": 0x06,
    "UFT_GW_CMD_READ_FLUX": 0x07,
    "UFT_GW_CMD_WRITE_FLUX": 0x08,
    "UFT_GW_CMD_GET_FLUX_STATUS": 0x09,
    "UFT_GW_CMD_GET_INDEX_TIMES": 0x0A,
    "UFT_GW_CMD_SWITCH_FW_MODE": 0x0B,
    "UFT_GW_CMD_SELECT": 0x0C,
    "UFT_GW_CMD_DESELECT": 0x0D,
    "UFT_GW_CMD_SET_BUS_TYPE": 0x0E,
    "UFT_GW_CMD_SET_PIN": 0x0F,
    "UFT_GW_CMD_RESET": 0x10,
    "UFT_GW_CMD_ERASE_FLUX": 0x11,
    "UFT_GW_CMD_SOURCE_BYTES": 0x12,
    "UFT_GW_CMD_SINK_BYTES": 0x13,
    "UFT_GW_CMD_GET_PIN": 0x14,
    "UFT_GW_CMD_TEST_MODE": 0x15,
    "UFT_GW_CMD_NO_CLICK_STEP": 0x16,
}

# Official Ack codes (firmware inc/cmd.h / host usb.py class Ack).
ACK_CODES = {
    "UFT_GW_ACK_OK": 0x00,           # official name: Okay
    "UFT_GW_ACK_BAD_COMMAND": 0x01,
    "UFT_GW_ACK_NO_INDEX": 0x02,
    "UFT_GW_ACK_NO_TRK0": 0x03,
    "UFT_GW_ACK_FLUX_OVERFLOW": 0x04,
    "UFT_GW_ACK_FLUX_UNDERFLOW": 0x05,
    "UFT_GW_ACK_WRPROT": 0x06,
    "UFT_GW_ACK_NO_UNIT": 0x07,
    "UFT_GW_ACK_NO_BUS": 0x08,
    "UFT_GW_ACK_BAD_UNIT": 0x09,
    "UFT_GW_ACK_BAD_PIN": 0x0A,
    "UFT_GW_ACK_BAD_CYLINDER": 0x0B,
    "UFT_GW_ACK_OUT_OF_SRAM": 0x0C,
    "UFT_GW_ACK_OUT_OF_FLASH": 0x0D,
}

USB_IDS = {
    "UFT_GW_USB_VID": 0x1209,
    "UFT_GW_USB_PID": 0x4D69,
    # F7 enumerates with the same VID/PID as the base device.
    "UFT_GW_USB_PID_F7": 0x4D69,
}

# Sample frequency is per-device and reported by GET_INFO at runtime —
# there is no single canonical constant. UFT's #defines are *fallbacks*.
# 72 MHz is the typical F7 figure; F7 Plus variants report higher.
# Graded "device-reported" — a fixed reference value would be wrong.
TIMING = {
    "UFT_GW_SAMPLE_FREQ_HZ": {"ref": "device-reported (~72 MHz on F7)",
                              "kind": "device-reported"},
    "UFT_GW_SAMPLE_FREQ_F7_PLUS": {"ref": "device-reported (higher on F7 Plus)",
                                   "kind": "device-reported"},
}


def main():
    json.dump({
        "_provenance": PROVENANCE,
        "usb_ids": USB_IDS,
        "cmd_opcodes": CMD_OPCODES,
        "ack_codes": ACK_CODES,
        "timing": TIMING,
    }, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
