#!/usr/bin/env python3
"""
mock_fc5025.py — a mock of the external `fcimage` CLI.

The FC5025 V2 provider documents a dual backend path: a direct-USB
libusb path and a `fcimage` subprocess fallback. The audit could not
run the real `fcimage` binary (not present on the audit host, and the
real CLI-arg grammar is "needs-source" — see extract_ref.py PROVENANCE).
This mock stands in for `fcimage` so the FC5025 datapath (D2) can be
exercised end-to-end without hardware.

It is DELIBERATELY minimal and HONEST about being a mock: it does not
pretend to know the real fcimage argv grammar byte-for-byte. It accepts
the argv shape the provider header documents
(fc5025_provider_v2.h:225-234):

    fcimage -f <format> -t <cyl> -T <cyl> [-s <side>]

and emits either:
  * a deterministic synthetic sector buffer (so do_read_sector can be
    traced to a SectorRead outcome), or
  * a non-zero exit + stderr (so the failure-translation path can be
    traced to ProviderError / SectorUnreadable).

This is NOT a conformance reference. When the real fcimage CLI grammar
is vendored (extract_ref.py "files_needed"), this mock should be
replaced by a recorded-transcript fixture.

Usage:
    python mock_fc5025.py -f apple33 -t 0 -T 0          # -> 4096 bytes stdout, exit 0
    python mock_fc5025.py -f apple33 -t 99 -T 99        # -> exit 3, "no index" stderr
    python mock_fc5025.py --version                     # -> "fcimage (mock) 0.0"
"""

import sys


SECTOR_SIZE = 256
SECTORS_PER_TRACK = 16  # Apple DOS 3.3 default


def main(argv):
    if "--version" in argv or "-V" in argv:
        sys.stdout.write("fcimage (mock_fc5025.py audit stand-in) 0.0\n")
        return 0

    fmt = None
    cyl = None
    side = 0
    it = iter(range(len(argv)))
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "-f" and i + 1 < len(argv):
            fmt = argv[i + 1]; i += 2; continue
        if a == "-t" and i + 1 < len(argv):
            cyl = int(argv[i + 1]); i += 2; continue
        if a == "-T" and i + 1 < len(argv):
            i += 2; continue  # end cylinder — mock reads one track
        if a == "-s" and i + 1 < len(argv):
            side = int(argv[i + 1]); i += 2; continue
        i += 1

    if cyl is None:
        sys.stderr.write("mock_fc5025: missing -t <cylinder>\n")
        return 2

    # Out-of-range cylinder -> "no index pulse" non-retryable failure.
    if cyl < 0 or cyl > 79:
        sys.stderr.write(f"mock_fc5025: no index pulse on cylinder {cyl}\n")
        return 3

    # Deterministic synthetic track: byte = (cyl + side + sector + offset) & 0xFF
    buf = bytearray()
    for s in range(SECTORS_PER_TRACK):
        for off in range(SECTOR_SIZE):
            buf.append((cyl + side + s + off) & 0xFF)
    sys.stdout.buffer.write(bytes(buf))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
