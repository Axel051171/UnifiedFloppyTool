#!/usr/bin/env python3
"""
applesauce_sim.py — minimal Applesauce protocol simulator (MF-253).

Listens on a serial port and responds to documented Applesauce text-protocol
commands. Useful for end-to-end Tier-2 bench-testing of UFT's Applesauce
QSerialPort transport + the 7 runner-factories without a physical device.

Setup:
  Windows: install com0com, pair COM20 + COM21, run this on COM21.
  Linux/macOS: socat -d -d pty,raw,echo=0 pty,raw,echo=0 → run on /dev/pts/N.

Dependencies:
  pip install pyserial

Usage:
  python tools/applesauce_sim.py --port COM21
  python tools/applesauce_sim.py --port /dev/pts/11

The responses are protocol-correct but NOT physically meaningful — the
flux bytes are zeros, the firmware version is "sim-firmware-0.1", the RPM
is exactly 300.00 etc. This validates the UFT I/O stack, not floppy
electrical behaviour. For that, real hardware is needed (Tier 3 — see
docs/M3_VIRTUAL_SERIAL_BENCH.md §Status).

See docs/M3_VIRTUAL_SERIAL_BENCH.md for the full bench-test plan.
"""

import argparse
import sys


def main():
    p = argparse.ArgumentParser(description="Applesauce protocol simulator")
    p.add_argument("--port", required=True,
                   help="Serial port to listen on (e.g. COM21, /dev/pts/11)")
    p.add_argument("--baud", type=int, default=115200,
                   help="Baud rate (default: 115200)")
    p.add_argument("--verbose", action="store_true",
                   help="Echo every command + response to stderr")
    args = p.parse_args()

    try:
        import serial  # type: ignore[import-not-found]
    except ImportError:
        print("ERROR: pyserial not installed. Run: pip install pyserial",
              file=sys.stderr)
        sys.exit(2)

    s = serial.Serial(args.port, args.baud, timeout=0.05)
    print(f"applesauce_sim: listening on {args.port} @ {args.baud}",
          file=sys.stderr)

    def writeln(line: str) -> None:
        if args.verbose:
            print(f">>> {line}", file=sys.stderr)
        s.write(line.encode() + b"\n")
        s.flush()

    buf = b""
    pending_buffer_size = 0  # bytes pending in our fake device buffer

    while True:
        chunk = s.read(256)
        if chunk:
            buf += chunk
        while b"\n" in buf:
            line, _, buf = buf.partition(b"\n")
            cmd = line.decode(errors="replace").strip()
            if args.verbose:
                print(f"<<< {cmd}", file=sys.stderr)

            # --- Identity / status queries ---
            if cmd == "?kind":          writeln("+5.25")
            elif cmd == "?vers":        writeln("+sim-firmware-0.1")
            elif cmd == "?pcb":         writeln("+sim-pcb")
            elif cmd == "data:?max":    writeln("+163840")
            elif cmd == "sync:?speed":  writeln("+300.00")
            elif cmd == "psu:?":        writeln("+1")
            elif cmd == "psu:on":       writeln(".")
            elif cmd in ("sync:on", "sync:off"): writeln(".")

            # --- Mechanical control ---
            elif cmd.startswith("head:track"):   writeln(".")
            elif cmd.startswith("head:side"):    writeln(".")
            elif cmd == "head:zero":             writeln(".")
            elif cmd in ("motor:on", "motor:off"): writeln(".")

            # --- Capture ---
            elif cmd.startswith("disk:readx"):
                # Pretend the device has a small buffer ready.
                pending_buffer_size = 512
                writeln(".")
            elif cmd == "data:?size":
                writeln(f"+{pending_buffer_size}")

            # --- Binary transfer (download from device) ---
            elif cmd.startswith("data:< ") or cmd.startswith("data:<"):
                # Send pending_buffer_size bytes — zeros are fine for the
                # I/O test (the UFT-side runner validates structure, not
                # electrical correctness).
                payload = bytes(pending_buffer_size)
                if args.verbose:
                    print(f">>> [binary {len(payload)} bytes]",
                          file=sys.stderr)
                s.write(payload)
                s.flush()

            # --- Write path ---
            elif cmd == "disk:?write": writeln("+0")  # not write-protected
            elif cmd == "data:clear":  writeln(".")
            elif cmd.startswith("data:> ") or cmd.startswith("data:>"):
                # Parse N, swallow N binary bytes.
                try:
                    n = int(cmd.split(":>")[1].strip())
                except (IndexError, ValueError):
                    writeln("-bad data:> length")
                    continue
                received = 0
                while received < n:
                    bin_chunk = s.read(n - received)
                    if not bin_chunk:
                        break
                    received += len(bin_chunk)
                if args.verbose:
                    print(f"<<< [binary {received} of {n} bytes]",
                          file=sys.stderr)
                writeln(".")
            elif cmd == "disk:write": writeln(".")

            else:
                writeln("-unknown command")


if __name__ == "__main__":
    main()
