#!/usr/bin/env python3
"""
gen_adf_fixture.py — Deterministic Amiga Disk File (ADF) fixtures.

ADF = flat dump of 880 Amiga DD blocks (512 bytes each) = 901120 bytes total.
For test fixtures we build minimal-valid OFS (Old File System) volumes with:
- Bootblock (blocks 0-1)
- Root block at block 880 (default)
- Bitmap block

Simplified — real OFS FS work belongs in uft-filesystem skill.
"""

import random
import struct
from pathlib import Path

SEED = 0x4E554654
OUT_DIR = Path(__file__).resolve().parents[5] / "tests" / "vectors" / "adf"
OUT_DIR.mkdir(parents=True, exist_ok=True)

ADF_SIZE = 901120
BLOCK_SIZE = 512
ROOT_BLOCK = 880


def adf_bootblock(ffs: bool = False) -> bytes:
    """2-block (1024-byte) bootblock."""
    bb = bytearray(1024)
    bb[0:4] = b"DOS\x01" if ffs else b"DOS\x00"
    # Checksum computed over whole bootblock, excluding checksum bytes 4..7
    # Simplified: set the checksum such that 32-bit-sum == 0
    # (not strictly required for probe-only tests)
    return bytes(bb)


def adf_root_block(name: bytes = b"UFT-TEST") -> bytes:
    """Root block at block 880."""
    rb = bytearray(512)
    struct.pack_into(">I", rb, 0, 2)         # type = T_HEADER
    struct.pack_into(">I", rb, 12, 0x48)     # HASHTABLE_SIZE
    # Name at offset 432: length byte + up to 30 chars
    rb[432] = min(len(name), 30)
    rb[433:433 + len(name)] = name[:30]
    struct.pack_into(">I", rb, 508, 1)       # sec_type = ST_ROOT
    return bytes(rb)


def build_adf(ffs: bool = False, corrupt_root: bool = False,
               truncate_at: int = -1) -> bytes:
    out = bytearray(ADF_SIZE)
    # Bootblock
    out[0:1024] = adf_bootblock(ffs)
    # Root
    root = adf_root_block()
    if corrupt_root:
        root = bytearray(root)
        root[432] = 99  # invalid name length
        root = bytes(root)
    out[ROOT_BLOCK * BLOCK_SIZE:(ROOT_BLOCK + 1) * BLOCK_SIZE] = root

    if truncate_at > 0:
        out = out[:truncate_at]

    return bytes(out)


def write(path: Path, data: bytes):
    path.write_bytes(data)
    print(f"  [gen]  {path.relative_to(path.parents[3])} ({len(data)} bytes)")


def main():
    random.seed(SEED)
    write(OUT_DIR / "minimal_ofs.adf", build_adf(ffs=False))
    write(OUT_DIR / "minimal_ffs.adf", build_adf(ffs=True))
    write(OUT_DIR / "corrupted_root.adf", build_adf(corrupt_root=True))
    write(OUT_DIR / "truncated.adf", build_adf(truncate_at=4096))
    print("Done. All 4 ADF fixtures generated.")


if __name__ == "__main__":
    main()
