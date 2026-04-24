#!/usr/bin/env python3
"""
crc_reference.py — Pure-Python reference CRC implementation for UFT families.

Use to cross-check C implementations without requiring reveng installation.

Usage:
    crc_reference.py --family ibm-3740 --input 313233343536373839
    crc_reference.py --family ibm-3740 --init 0xFFFF \\
                     --input 313233343536373839 --expected 0x29B1

Families supported:
    ibm-3740      — CRC-16/IBM-3740 (MFM, FM); init default 0xFFFF
    rx02          — DEC RX02 FM; init default 0x0000
    amiga-mfm     — Amiga AmigaDOS XOR-sum checksum (returns 32-bit)
    apple-dos33   — Apple DOS 3.3 byte XOR + rotate
"""

import argparse
import sys


def crc16_ibm_3740(data: bytes, init: int = 0xFFFF) -> int:
    """CRC-16/IBM-3740: poly=0x1021, refin=False, refout=False, xorout=0."""
    crc = init
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def crc16_rx02(data: bytes, init: int = 0x0000) -> int:
    """DEC RX02 FM: same poly as IBM-3740 but init=0."""
    return crc16_ibm_3740(data, init=init)


def amiga_mfm_checksum(data: bytes) -> int:
    """
    Amiga MFM XOR checksum. Input must be MFM-encoded (clock+data interleaved
    in 32-bit words). Returns the 32-bit XOR accumulator.
    For non-MFM bytes, the expected usage is pre-decoded long-words.
    """
    if len(data) % 4 != 0:
        raise ValueError("Amiga checksum requires input to be 4-byte aligned")
    checksum = 0
    for i in range(0, len(data), 4):
        longword = int.from_bytes(data[i:i+4], byteorder="big")
        checksum ^= longword
    return checksum & 0x55555555   # mask per AmigaDOS convention


def apple_dos33_checksum(data: bytes) -> int:
    """Apple DOS 3.3 sector data: XOR running-sum with left-rotate."""
    checksum = 0
    for b in data:
        # rotate left by 1
        checksum = ((checksum << 1) | (checksum >> 7)) & 0xFF
        checksum ^= b
    return checksum


# Presets for MFM sync-byte shortcuts
def preset_after_a1_a1_a1(init: int = 0xFFFF) -> int:
    """CRC state after processing A1 A1 A1 with init."""
    return crc16_ibm_3740(bytes([0xA1, 0xA1, 0xA1]), init=init)


def preset_after_c2_c2_c2(init: int = 0xFFFF) -> int:
    """CRC state after processing C2 C2 C2 with init."""
    return crc16_ibm_3740(bytes([0xC2, 0xC2, 0xC2]), init=init)


FAMILIES = {
    "ibm-3740": lambda d, init: crc16_ibm_3740(d, init),
    "rx02":     lambda d, init: crc16_rx02(d, init),
    "amiga-mfm": lambda d, init: amiga_mfm_checksum(d),
    "apple-dos33": lambda d, init: apple_dos33_checksum(d),
}

DEFAULT_INITS = {
    "ibm-3740": 0xFFFF,
    "rx02":     0x0000,
    "amiga-mfm": 0,
    "apple-dos33": 0,
}


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--family", required=True, choices=list(FAMILIES.keys()))
    p.add_argument("--init", type=lambda x: int(x, 0), default=None,
                   help="init value (hex or decimal); family default if omitted")
    p.add_argument("--input", required=False, default=None,
                   help="hex string of data bytes, no spaces (e.g. '313233')")
    p.add_argument("--expected", type=lambda x: int(x, 0), default=None,
                   help="expected result (hex or decimal); verifies if given")
    p.add_argument("--preset", choices=["a1a1a1", "c2c2c2"], default=None,
                   help="compute MFM preset instead of running on --input")
    args = p.parse_args()

    if args.preset:
        init = args.init if args.init is not None else 0xFFFF
        if args.preset == "a1a1a1":
            result = preset_after_a1_a1_a1(init)
            print(f"Preset after A1 A1 A1 with init 0x{init:04X}: 0x{result:04X}")
        else:
            result = preset_after_c2_c2_c2(init)
            print(f"Preset after C2 C2 C2 with init 0x{init:04X}: 0x{result:04X}")
        return 0

    if args.input is None:
        print("ERROR: --input is required unless --preset is given",
              file=sys.stderr)
        return 1

    try:
        data = bytes.fromhex(args.input)
    except ValueError as e:
        print(f"ERROR: invalid hex input: {e}", file=sys.stderr)
        return 1

    init = args.init if args.init is not None else DEFAULT_INITS[args.family]
    result = FAMILIES[args.family](data, init)

    width = 8 if args.family == "amiga-mfm" else 4
    print(f"CRC-16/{args.family} (init=0x{init:04X}, {len(data)} bytes) = "
          f"0x{result:0{width}X}")

    if args.expected is not None:
        if result == args.expected:
            print(f"MATCH: expected 0x{args.expected:0{width}X}")
            return 0
        else:
            print(f"MISMATCH: expected 0x{args.expected:0{width}X}, "
                  f"got 0x{result:0{width}X}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
