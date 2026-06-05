#!/usr/bin/env python3
"""
sim_fcimage.py — FC5025 fcimage simulator (V415-PLAN MF-267).

Pretends to be the `fcimage` binary. FC5025 is sector-only (read-only,
no flux). The provider runs:
    fcimage -f <format> -t <cyl_lo> -T <cyl_hi> [-s 1] <output_file>
and reads the resulting raw sector dump.

This simulator generates a sector pattern (deterministic per cylinder/
head/sector) so the FC5025 V2 provider can be exercised end-to-end
without an FC5025 board. The bytes are not from a real Apple-DOS or
IBM-PC disk — but they ARE deterministic and re-readable, sufficient
for the V2 outcome / SectorOutcome assertions.

See ../README.md for the limitations list.
"""
from __future__ import annotations
import sys
from pathlib import Path

# Per-format geometry table — sector size + tracks + sectors per track.
# Mirrors src/hardware_providers/fc5025_provider_v2.cpp format mapping.
FORMAT_GEOMETRY = {
    "apple33":   (35, 16, 256),   # Apple DOS 3.3
    "apple_po":  (35, 16, 256),   # Apple ProDOS
    "msdos360":  (40, 9,  512),   # IBM PC 360K
    "msdos720":  (80, 9,  512),   # IBM PC 720K (5.25 HD downformatted)
    "msdos720_3": (80, 9, 512),
    "msdos12":   (80, 15, 512),   # IBM PC 1.2M HD
    "ti994a":    (40, 9,  256),   # TI 99/4A
    "kpc1":      (40, 10, 512),   # Kaypro CP/M
    "trs80":     (40, 18, 256),   # TRS-80
    "_default":  (40, 9,  512),
}


def synth_sector(fmt: str, cyl: int, head: int, sector: int, size: int) -> bytes:
    """Deterministic, recognisable sector pattern."""
    header = f"SIM/FC5025/{fmt}/c{cyl:02d}h{head}s{sector:02d}".encode()
    header = header[:32].ljust(32, b"-")
    body = bytes(((i + cyl * 7 + head * 13 + sector * 17) & 0xff)
                 for i in range(size - 32))
    return header + body


def main():
    fmt = "_default"
    cyl_lo = 0
    cyl_hi = 0
    head = 0
    out_path = None

    argv = sys.argv[1:]
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "-f" and i + 1 < len(argv):
            fmt = argv[i + 1]
            i += 2
        elif a == "-t" and i + 1 < len(argv):
            cyl_lo = int(argv[i + 1])
            i += 2
        elif a == "-T" and i + 1 < len(argv):
            cyl_hi = int(argv[i + 1])
            i += 2
        elif a == "-s" and i + 1 < len(argv):
            head = int(argv[i + 1])
            i += 2
        elif not a.startswith("-"):
            out_path = Path(a)
            i += 1
        else:
            i += 1

    if out_path is None:
        sys.stderr.write("sim_fcimage: missing output-file argument\n")
        return 2

    tracks, spt, secsize = FORMAT_GEOMETRY.get(fmt, FORMAT_GEOMETRY["_default"])
    # Real fcimage uses default range = whole disk if -t/-T omitted.
    if cyl_hi == 0 and cyl_lo == 0:
        cyl_hi = tracks - 1

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("wb") as fp:
        for c in range(cyl_lo, cyl_hi + 1):
            for s in range(spt):
                fp.write(synth_sector(fmt, c, head, s, secsize))

    sys.stdout.write(
        f"sim_fcimage: format={fmt} cyls={cyl_lo}-{cyl_hi} head={head} "
        f"-> {out_path} "
        f"({(cyl_hi - cyl_lo + 1) * spt} sectors × {secsize} bytes)\n"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
