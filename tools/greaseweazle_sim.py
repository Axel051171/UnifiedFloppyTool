#!/usr/bin/env python3
"""
greaseweazle_sim.py — minimal Greaseweazle protocol simulator (MF-270).

Listens on a virtual serial port and responds to the documented
Greaseweazle CDC USB-serial protocol. Useful for Tier-2.5 bench-testing
of UFT's Greaseweazle C-HAL transport (src/hal/uft_greaseweazle_full.c)
without a physical Greaseweazle F1/F7/V4 board.

WHY THIS SIM IS SEPARATE FROM tools/hw_simulators/sim_*.py
=========================================================

The QProcess simulators (sim_dtc, sim_fluxengine, sim_fcimage) fake
external CLI binaries. The libusb-mock framework
(tests/usb_mock/libusb_mock.h) fakes libusb_* calls for SCP-Direct and
XUM1541. Greaseweazle uses NEITHER: it talks USB-CDC virtual-serial via
the OS-native serial APIs (HANDLE/ReadFile on Windows, tcgetattr/read
on POSIX), going through the OS's USB-CDC class driver — not libusb at
all. So the mock layer for GW lives one level up: a Python serial
simulator paired with com0com (Windows) or socat (Linux/macOS).

This is the same pattern as tools/applesauce_sim.py and
tools/adfcopy_sim.py — three Teensy-based or USB-CDC serial-protocol
controllers, one virtual-COM bridge each.

Setup
=====

Windows: install com0com, pair COM40 + COM41, run this on COM41.
Linux/macOS: socat -d -d pty,raw,echo=0 pty,raw,echo=0 → run on /dev/pts/N.

Dependencies:
  pip install pyserial

Usage:
  python tools/greaseweazle_sim.py --port COM41
  python tools/greaseweazle_sim.py --port /dev/pts/14

Protocol scope (per keirf/greaseweazle host-side reference docs):
  CMD_GET_INFO          = 0x00   → 32-byte info block (firmware version,
                                    HW model, sample period, etc.)
  CMD_UPDATE            = 0x01   → not implemented (firmware-update path)
  CMD_SEEK              = 0x02 + cyl(1)  → ACK
  CMD_HEAD              = 0x03 + side(1) → ACK
  CMD_SET_PARAMS        = 0x04 + params  → ACK
  CMD_GET_PARAMS        = 0x05 → params block
  CMD_MOTOR             = 0x06 + on/off(1) → ACK
  CMD_READ_FLUX         = 0x07 + nrev(1) → ACK + flux stream
  CMD_WRITE_FLUX        = 0x08 + … → ACK
  CMD_GET_FLUX_STATUS   = 0x09 → 1-byte status
  CMD_GET_INDEX_TIMES   = 0x0A → revolutions × 4 bytes (LE)
  CMD_SELECT            = 0x0B + unit(1) → ACK
  CMD_DESELECT          = 0x0C → ACK
  CMD_SET_BUS_TYPE      = 0x0D + type(1) → ACK

The simulator implements the SUBSET that the UFT C-HAL actually invokes
during a detect + minimal read cycle (GET_INFO + SELECT + SEEK + MOTOR
+ READ_FLUX). Other commands return a generic ACK with no payload. The
RESPONSE format is [ACK_STATUS(1)] for simple cases and
[ACK_STATUS(1), LEN_LO, LEN_HI, payload...] for commands that return data.

Status: SIMULATED (Tier 2.5) — proves the binary protocol round-trip
through OS-serial APIs works. Does NOT cover physical-flux behaviour.
"""
from __future__ import annotations
import argparse
import struct
import sys

ACK   = 0    # success
NOK   = 1    # generic error

CMD_GET_INFO        = 0x00
CMD_SEEK            = 0x02
CMD_HEAD            = 0x03
CMD_SET_PARAMS      = 0x04
CMD_GET_PARAMS      = 0x05
CMD_MOTOR           = 0x06
CMD_READ_FLUX       = 0x07
CMD_WRITE_FLUX      = 0x08
CMD_GET_FLUX_STATUS = 0x09
CMD_GET_INDEX_TIMES = 0x0A
CMD_SELECT          = 0x0B
CMD_DESELECT        = 0x0C
CMD_SET_BUS_TYPE    = 0x0D


def build_info_block() -> bytes:
    """32-byte GW info block.
       Layout (per host-side parser, gw/Greaseweazle.py):
         u8  hw_model      = 7   (F7)
         u8  hw_submodel   = 0
         u8  fw_major      = 1
         u8  fw_minor      = 4
         u8  is_main       = 1
         u8  unused        = 0
         u8  tbuf_pages    = 4
         u8  unused        = 0
         u32 sample_freq   = 72000000  (72 MHz)
       Tail is zero-padded to 32 bytes. """
    pkt = struct.pack(
        "<BBBBBBBBI",
        7, 0,            # hw_model, hw_submodel
        1, 4,            # fw major, minor
        1, 0,            # is_main, unused
        4, 0,            # tbuf_pages, unused
        72_000_000,      # sample freq
    )
    return pkt.ljust(32, b"\x00")


def main():
    p = argparse.ArgumentParser(description="Greaseweazle protocol simulator")
    p.add_argument("--port", required=True,
                   help="Serial port to listen on (e.g. COM41, /dev/pts/14)")
    p.add_argument("--baud", type=int, default=3_000_000,
                   help="Baud rate (GW CDC ignores this, but pyserial wants it)")
    p.add_argument("--verbose", action="store_true",
                   help="Echo every command + response to stderr")
    args = p.parse_args()

    try:
        import serial
    except ImportError:
        print("ERROR: pyserial not installed. Run: pip install pyserial",
              file=sys.stderr)
        return 1

    ser = serial.Serial(args.port, args.baud, timeout=1)
    print(f"greaseweazle_sim listening on {args.port}", file=sys.stderr)

    def log(msg):
        if args.verbose:
            print(f"[gw_sim] {msg}", file=sys.stderr)

    try:
        while True:
            head = ser.read(1)
            if not head:
                continue
            cmd = head[0]
            # GW commands are [CMD, LEN, payload...]
            length_b = ser.read(1)
            length = length_b[0] if length_b else 0
            payload = ser.read(length - 2) if length > 2 else b""

            if cmd == CMD_GET_INFO:
                log("GET_INFO → 32-byte info block")
                ser.write(bytes([cmd, ACK]) + build_info_block())
            elif cmd == CMD_SEEK:
                cyl = payload[0] if payload else 0
                log(f"SEEK cyl={cyl} → ACK")
                ser.write(bytes([cmd, ACK]))
            elif cmd == CMD_HEAD:
                head_n = payload[0] if payload else 0
                log(f"HEAD side={head_n} → ACK")
                ser.write(bytes([cmd, ACK]))
            elif cmd == CMD_MOTOR:
                on = payload[1] if len(payload) >= 2 else 0
                log(f"MOTOR on={on} → ACK")
                ser.write(bytes([cmd, ACK]))
            elif cmd == CMD_SELECT:
                unit = payload[0] if payload else 0
                log(f"SELECT unit={unit} → ACK")
                ser.write(bytes([cmd, ACK]))
            elif cmd == CMD_DESELECT:
                log("DESELECT → ACK")
                ser.write(bytes([cmd, ACK]))
            elif cmd == CMD_READ_FLUX:
                nrev = payload[0] if payload else 1
                # Minimal flux payload: 256 zero-bytes then a stop-marker (0).
                # Real GW emits a flux stream encoded as variable-length
                # transitions. Zero-flux is valid (no transitions captured).
                log(f"READ_FLUX nrev={nrev} → ACK + 257-byte flux")
                ser.write(bytes([cmd, ACK]))
                ser.write(b"\x00" * 256 + b"\x00")
            elif cmd == CMD_GET_FLUX_STATUS:
                log("GET_FLUX_STATUS → ACK")
                ser.write(bytes([cmd, ACK]))
            elif cmd == CMD_GET_INDEX_TIMES:
                # Return 5 revolutions × 4 bytes LE = 20 bytes
                idx_times = struct.pack("<5I", 200_000, 400_000, 600_000,
                                         800_000, 1_000_000)
                log(f"GET_INDEX_TIMES → ACK + {len(idx_times)}B")
                ser.write(bytes([cmd, ACK]) + idx_times)
            else:
                log(f"UNKNOWN cmd 0x{cmd:02x} → ACK (generic)")
                ser.write(bytes([cmd, ACK]))
    except KeyboardInterrupt:
        print("\ngreaseweazle_sim shutting down.", file=sys.stderr)
        return 0


if __name__ == "__main__":
    sys.exit(main())
