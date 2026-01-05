#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gw_stats.py — Greaseweazle Firmware Stats Reader (CDC-ACM protocol)
Reads GET_INFO indices without needing hosttool patches.

Usage:
  python3 gw_stats.py /dev/ttyACM0
  python3 gw_stats.py /dev/ttyACM0 --watch 0.5
  python3 gw_stats.py /dev/ttyACM0 --reset

Run your normal 'gw write ... --cue-at-index' in another terminal, then watch stats.
"""

import argparse
import struct
import sys
import time

try:
    import serial
except ImportError:
    print("Missing pyserial. Install: pip install pyserial", file=sys.stderr)
    sys.exit(1)

CMD_GET_INFO = 0
CMD_RESET    = 16

# Our added indices (protocol-compatible additions):
GETINFO_OP_STATS         = 2   # existing gw_op_stats (we extended data source)
GETINFO_ERR_STATS        = 5
GETINFO_INDEXWRITE_STATS = 11
GETINFO_INDEX_FILTER     = 12

def tx_cmd(ser, cmd: int, payload: bytes) -> None:
    # Packet = [cmd][len][payload...]
    pkt = bytes([cmd, 2 + len(payload)]) + payload
    ser.write(pkt)
    ser.flush()

def rx_exact(ser, n: int) -> bytes:
    data = ser.read(n)
    if len(data) != n:
        raise RuntimeError(f"Short read: got {len(data)} wanted {n}")
    return data

def get_info(ser, idx: int) -> tuple[int, bytes]:
    tx_cmd(ser, CMD_GET_INFO, bytes([idx]))
    resp = rx_exact(ser, 2 + 32)
    rcmd, ack = resp[0], resp[1]
    if rcmd != CMD_GET_INFO:
        raise RuntimeError(f"Bad response cmd: {rcmd}")
    return ack, resp[2:]

def do_reset(ser) -> None:
    tx_cmd(ser, CMD_RESET, b"")
    resp = rx_exact(ser, 2)
    if resp[0] != CMD_RESET:
        raise RuntimeError(f"Bad reset resp cmd: {resp[0]}")
    if resp[1] != 0:
        raise RuntimeError(f"Reset failed ack={resp[1]}")

def u32s(buf: bytes) -> list[int]:
    return list(struct.unpack("<8I", buf))

def print_err_stats(buf: bytes) -> None:
    bad_command,no_index,no_trk0,wrprot,ovf,udf,other,last = u32s(buf)
    last_cmd = (last >> 16) & 0xffff
    last_ack = (last >> 8) & 0xff
    print(f"[ERR] bad_cmd={bad_command} no_idx={no_index} no_trk0={no_trk0} "
          f"wrprot={wrprot} ovf={ovf} udf={udf} other={other} "
          f"last(cmd={last_cmd},ack={last_ack})")

def print_indexwrite(buf: bytes) -> None:
    starts, mn, mx, total, last, flags, _, _ = u32s(buf)
    avg = (total / starts) if starts else 0.0
    in_irq  = bool(flags & 0x1)
    in_main = bool(flags & 0x2)
    print(f"[IDXWRITE] starts={starts} last={last}us min={mn}us max={mx}us avg={avg:.1f}us "
          f"last_path={'IRQ' if in_irq else ''}{'MAIN' if in_main else ''}")

def print_indexfilter(buf: bytes) -> None:
    accepted, masked, glitch, last_db, comp_starts, comp_total, comp_last, _ = u32s(buf)
    comp_avg = (comp_total / comp_starts) if comp_starts else 0.0
    print(f"[IDXFLT] accepted={accepted} masked={masked} glitch={glitch} "
          f"debounce_last={last_db}us comp(starts={comp_starts} last={comp_last}us avg={comp_avg:.1f}us)")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", help="e.g. /dev/ttyACM0 or COM5")
    ap.add_argument("--baud", type=int, default=9600, help="Line coding; firmware ignores for bulk data.")
    ap.add_argument("--watch", type=float, default=0.0, help="Poll interval seconds (0 = single shot)")
    ap.add_argument("--reset", action="store_true", help="Send CMD_RESET first (also clears stats on our build)")
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=1)
    time.sleep(0.2)

    if args.reset:
        do_reset(ser)
        print("Device reset OK")

    def one():
        ack, b = get_info(ser, GETINFO_ERR_STATS)
        if ack == 0: print_err_stats(b)
        else: print(f"[ERR] ACK={ack}")

        ack, b = get_info(ser, GETINFO_INDEX_FILTER)
        if ack == 0: print_indexfilter(b)
        else: print(f"[IDXFLT] ACK={ack}")

        ack, b = get_info(ser, GETINFO_INDEXWRITE_STATS)
        if ack == 0: print_indexwrite(b)
        else: print(f"[IDXWRITE] ACK={ack}")

    if args.watch > 0:
        while True:
            one()
            time.sleep(args.watch)
    else:
        one()

if __name__ == "__main__":
    main()
