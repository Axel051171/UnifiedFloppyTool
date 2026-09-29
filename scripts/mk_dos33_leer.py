#!/usr/bin/env python3
"""Empty Apple DOS 3.3 skeleton for the a2tools fixture (P3-384, MF-1497).

This is the OWN-HAND half of tests/corpus_free/a2tools_dos33_filled.do:
a2tools (catseye/a2tools) cannot format — its commands are dir, out, in,
del (measured, MF-1207) — so the skeleton it fills has to come from
somewhere, and it comes from here. Everything a2tools then writes into it
(catalog entries, T/S lists, file data, the VTOC free map) is the
foreign-hand half; the manifest names both.

What this skeleton decides on its own, and which is therefore NOT checked
by a foreign hand: the volume number (254), where the catalog chain starts
(T17 S15) and how it runs (S15 -> S1 on track 17), and that tracks 0 and 17
start out allocated.

The fields are the six a2tools checks before it accepts an image
(a2tools.c:199-203: 0x03=3, 0x27=0x7A, 0x34=35, 0x35=16, 0x36/0x37=00 01)
plus the free map a2tools allocates from (a2tools.c:336-373: sectors 15..8
in byte 0x38+4*t, 7..0 in byte 0x39+4*t, bit set = free).

Usage:  python scripts/mk_dos33_leer.py <out.do>
Then, with a2tools_dos.exe built as `gcc -O2 -DDOS -o a2tools_dos.exe
a2tools.c` from catseye/a2tools @ 52ad81cc (the manifest has the full
command sequence).
"""
import sys

IMAGE = 35 * 16 * 256        # 143 360, DOS order
VTOC = (17 * 16 + 0) * 256


def skeleton() -> bytes:
    img = bytearray(IMAGE)
    v = bytearray(256)
    v[0x01], v[0x02] = 17, 15            # first catalog sector T17 S15
    v[0x03] = 3                          # DOS release
    v[0x06] = 254                        # volume
    v[0x27] = 0x7A                       # T/S pairs per T/S list
    v[0x30], v[0x31] = 17, 1             # last track allocated, direction +1
    v[0x34], v[0x35] = 35, 16            # tracks, sectors per track
    v[0x36], v[0x37] = 0x00, 0x01        # 256 bytes per sector
    for t in range(35):
        if t in (0, 17):
            continue                     # DOS image and VTOC/catalog track
        v[0x38 + 4 * t] = 0xFF
        v[0x38 + 4 * t + 1] = 0xFF
    img[VTOC:VTOC + 256] = v
    for s in range(15, 0, -1):           # catalog chain S15 -> S1
        o = (17 * 16 + s) * 256
        if s > 1:
            img[o + 1], img[o + 2] = 17, s - 1
    return bytes(img)


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    with open(sys.argv[1], "wb") as f:
        f.write(skeleton())
    print("wrote %s: %d bytes, %d sectors free" % (sys.argv[1], IMAGE, 33 * 16))
    return 0


if __name__ == "__main__":
    sys.exit(main())
