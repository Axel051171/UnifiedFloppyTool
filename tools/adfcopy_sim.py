#!/usr/bin/env python3
"""
adfcopy_sim.py — minimal ADF-Copy protocol simulator (V415-PLAN MF-268).

Listens on a serial port and responds to the documented ADF-Copy binary
protocol. Useful for end-to-end Tier-2.5 bench-testing of UFT's ADFCopy
QSerialPort transport + the 5 runner-factories without a physical
device. Mirrors tools/applesauce_sim.py for the Applesauce serial line.

Setup:
  Windows: install com0com, pair COM30 + COM31, run this on COM31.
  Linux/macOS: socat -d -d pty,raw,echo=0 pty,raw,echo=0 → run on /dev/pts/N.

Dependencies:
  pip install pyserial

Usage:
  python tools/adfcopy_sim.py --port COM31
  python tools/adfcopy_sim.py --port /dev/pts/12

Protocol (per src/hardware_providers/adfcopy_provider_v2.h):
  ADFC_CMD_INIT       = 0x01   → reply 'O' (0x4F)
  ADFC_CMD_SEEK       = 0x02 + track(1)         → reply 'O'
  ADFC_CMD_READ_FLUX  = 0x06 + track + revs     → reply 3-byte header + flux payload
  ADFC_CMD_GET_STATUS = 0x0B   → reply 1-byte status
  ADFC_CMD_PING       = 0x00   → reply firmware-version string + '\n'

Status: SIMULATED (Tier 2.5) — proves the binary protocol round-trip
through QSerialPort works. Does NOT replace physical-disk testing.
"""
from __future__ import annotations
import argparse
import sys

CMD_INIT       = 0x01
CMD_SEEK       = 0x02
CMD_READ_FLUX  = 0x06
CMD_GET_STATUS = 0x0B
CMD_PING       = 0x00

RSP_OK = b"O"


def main():
    p = argparse.ArgumentParser(description="ADF-Copy protocol simulator")
    p.add_argument("--port", required=True,
                   help="Serial port to listen on (e.g. COM31, /dev/pts/12)")
    p.add_argument("--baud", type=int, default=460800,
                   help="Baud rate (default: 460800 per ADF-Copy spec)")
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
    print(f"adfcopy_sim listening on {args.port} @ {args.baud} baud",
          file=sys.stderr)

    def log(msg):
        if args.verbose:
            print(f"[sim] {msg}", file=sys.stderr)

    try:
        while True:
            head = ser.read(1)
            if not head:
                continue
            cmd = head[0]

            if cmd == CMD_PING:
                log("PING → sim-firmware-0.1")
                ser.write(b"ADFC sim-firmware-0.1\n")
            elif cmd == CMD_INIT:
                log("INIT → 'O'")
                ser.write(RSP_OK)
            elif cmd == CMD_SEEK:
                track_b = ser.read(1)
                track = track_b[0] if track_b else 0
                log(f"SEEK track={track} → 'O'")
                ser.write(RSP_OK)
            elif cmd == CMD_GET_STATUS:
                log("GET_STATUS → 0x00 (no error)")
                ser.write(b"\x00")
            elif cmd == CMD_READ_FLUX:
                hdr = ser.read(2)
                track = hdr[0] if len(hdr) >= 1 else 0
                revs = hdr[1] if len(hdr) >= 2 else 1
                # Reply contract: 3-byte header (size_lo, size_hi, status='O')
                # then flux payload of `size` bytes. Simulator emits a tiny
                # 256-byte zero-flux payload — provider sees a complete
                # frame and can parse it. Real hw payload is ~150 KB.
                payload_size = 256
                ser.write(bytes([payload_size & 0xff,
                                 (payload_size >> 8) & 0xff,
                                 ord('O')]))
                ser.write(b"\x00" * payload_size)
                log(f"READ_FLUX track={track} revs={revs} → "
                    f"3-byte header + {payload_size}-byte payload")
            else:
                log(f"UNKNOWN cmd 0x{cmd:02x} — sending 'E' (error)")
                ser.write(b"E")
    except KeyboardInterrupt:
        print("\nadfcopy_sim shutting down.", file=sys.stderr)
        return 0


if __name__ == "__main__":
    sys.exit(main())
