#!/usr/bin/env python3
"""
gen_kryoflux_fixture.py — Generate deterministic KryoFlux RAW stream fixtures.

KryoFlux RAW format (per track):
- Index blocks (OOB with type 2)
- Flux cell blocks with timing at 40ns per sample-clock tick
- OOB (Out-Of-Band) blocks: 0x0D + type + body
- Stream end: OOB type 0x0D

Reference: https://www.kryoflux.com/download/kryoflux_stream_protocol_rev1.2.pdf
"""

import random
import struct
from pathlib import Path

SEED = 0x4E554654
OUT_DIR = Path(__file__).resolve().parents[5] / "tests" / "vectors" / "kryoflux"
OUT_DIR.mkdir(parents=True, exist_ok=True)


def kf_flux_byte(delta_ticks: int) -> bytes:
    """Encode a flux delta as per KF stream protocol."""
    if delta_ticks < 0x08:
        # Value 0..7 are OOB; avoid overlap
        delta_ticks = 0x08
    if delta_ticks <= 0xFF:
        return bytes([delta_ticks])
    elif delta_ticks <= 0xFFFF:
        # Flux2 command: 0x0C + 2-byte BE
        return bytes([0x0C]) + struct.pack(">H", delta_ticks)
    else:
        # Overflow: Ovl16 (0x0B) + Flux2
        return bytes([0x0B]) + kf_flux_byte(delta_ticks - 0x10000)


def kf_oob_index(stream_pos: int, sample_counter: int,
                  index_counter: int) -> bytes:
    """OOB type 2: StreamInfo/Index."""
    body = struct.pack("<III", stream_pos, sample_counter, index_counter)
    return bytes([0x0D, 0x02, len(body) & 0xFF, (len(body) >> 8) & 0xFF]) + body


def kf_oob_end() -> bytes:
    """OOB type 0x0D: end of stream."""
    return bytes([0x0D, 0x0D, 0x00, 0x00])


def build_kf_track(track_num: int, corrupt: bool = False) -> bytes:
    rng = random.Random(SEED + track_num)
    out = bytearray()
    sample_counter = 0

    # Index at beginning
    out.extend(kf_oob_index(len(out), sample_counter, 0))

    # ~500 flux events per fixture track (deliberately small)
    for i in range(500):
        delta = rng.randint(80, 320)  # 2us–8us at 40ns/tick
        if corrupt and 100 <= i <= 110:
            delta = 0x03   # invalid (would be parsed as OOB marker)
        out.extend(kf_flux_byte(delta))
        sample_counter += delta

    # Closing index + end
    out.extend(kf_oob_index(len(out), sample_counter, 1))
    out.extend(kf_oob_end())

    return bytes(out)


def write(path: Path, data: bytes):
    path.write_bytes(data)
    print(f"  [gen]  {path.relative_to(path.parents[3])} ({len(data)} bytes)")


def main():
    random.seed(SEED)

    # KryoFlux stores one file per track; we emit 4 variants as 4 files
    write(OUT_DIR / "track00.0.raw", build_kf_track(0))
    write(OUT_DIR / "track18.0.raw", build_kf_track(18))
    write(OUT_DIR / "corrupted.raw", build_kf_track(5, corrupt=True))
    write(OUT_DIR / "truncated.raw", build_kf_track(10)[:200])

    print("Done. All 4 KryoFlux fixtures generated.")


if __name__ == "__main__":
    main()
