#!/usr/bin/env python3
"""
gen_corpus.py — build the P3.2 differential-conformance flux corpus
(task #109, Hybrid Layer 1).

What it does
------------
1. Generates 6 deterministic synthetic SECTOR images — one per disk
   class the differential covers (IBM-DD, IBM-HD, Amiga-DD, C64-GCR,
   Apple2-5.25, AtariST-DD). Each is a fixed, reproducible byte pattern
   of the exact size that disk class holds. These are the COMMITTED,
   deterministic corpus inputs — `corpus/sources/` + `MANIFEST.sha256`.

2. Runs `gw convert <source> <flux>.scp --format=<fmt>` to derive a
   synthetic FLUX stream from each source image. The .scp files land in
   `corpus/flux/` and are a BUILD ARTIFACT — gitignored, regenerated on
   demand. They are NOT committed because gw's .scp output is ~24 MB
   per disk (not repo-suitable) and carries a non-deterministic header
   timestamp (no stable hash). The flux *data* is deterministic; the
   differential compares decoded sectors, never .scp bytes.

`gw` resolution: $GW env var → `gw` on PATH. Greaseweazle host tools
are not on PyPI — install from github.com/keirf/greaseweazle (Windows:
gw.exe from the release zip) and either put it on PATH or point $GW at
it. If `gw` is absent this script exits 0 with a clear message: the
source images + manifest are still (re)generated, only the .scp
derivation is skipped.

Determinism: the source images are pure functions of (size, pattern
seed) — identical bytes on every platform, every run. The manifest is
therefore stable and committable.
"""
from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CORPUS = HERE / "corpus"
SOURCES = CORPUS / "sources"
FLUX = CORPUS / "flux"
MANIFEST = CORPUS / "MANIFEST.sha256"

# Each corpus entry: (basename, source-suffix, byte size, gw --format).
# Sizes are the exact raw sector-image size of the disk class.
CORPUS_SPEC = [
    # name           src     bytes      gw format
    ("ibm_dd",       ".img",  737280,   "ibm.720"),            # 80*2*9*512
    ("ibm_hd",       ".img",  1474560,  "ibm.1440"),           # 80*2*18*512
    ("amiga_dd",     ".adf",  901120,   "amiga.amigados"),     # 80*2*11*512
    ("c64_gcr",      ".d64",  196608,   "commodore.1541"),     # 40-track .d64: gw's commodore.1541 diskdef is 768 sectors (17*21+7*19+6*18+10*17)
    ("apple2_525",   ".do",   143360,   "apple2.appledos.140"),# 35*16*256
    ("atarist_dd",   ".st",   737280,   "atarist.720"),        # 80*2*9*512
]


def synth_pattern(size: int) -> bytes:
    """Deterministic, platform-independent synthetic disk content.

    Pure function of the byte index, identical on every run and host.
    Crucially it is NOT periodic: the earlier `(i*31+17) & 0xFF` had
    period 256, so EVERY 256-byte block was byte-identical — the
    differential then only proved per-sector decode correctness and
    sector COUNT, never sector PLACEMENT (a transposition bug would
    pass). Here each byte is an avalanche hash of its FULL index, so
    every 256- and 512-byte block on every corpus disk is distinct: a
    wrong sector mapping fails the byte-exact differential.
    """
    out = bytearray(size)
    for i in range(size):
        h = (i * 2654435761) & 0xFFFFFFFF      # Knuth multiplicative
        h ^= h >> 13
        h = (h * 0x85EBCA6B) & 0xFFFFFFFF      # murmur3 finalizer mix
        h ^= h >> 16
        out[i] = h & 0xFF
    return bytes(out)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def resolve_gw() -> str | None:
    env = os.environ.get("GW")
    if env:
        return env if Path(env).exists() else None
    return shutil.which("gw")


def main() -> int:
    SOURCES.mkdir(parents=True, exist_ok=True)
    FLUX.mkdir(parents=True, exist_ok=True)

    # 1. (re)generate the deterministic source images + manifest.
    manifest_lines: list[str] = []
    for name, suffix, size, _fmt in CORPUS_SPEC:
        src = SOURCES / f"{name}{suffix}"
        src.write_bytes(synth_pattern(size))
        manifest_lines.append(f"{sha256_file(src)}  sources/{src.name}")
        print(f"  source: {src.name}  ({size} bytes)")

    MANIFEST.write_text("\n".join(sorted(manifest_lines)) + "\n",
                        encoding="utf-8")
    print(f"  manifest: {MANIFEST.relative_to(HERE)} "
          f"({len(manifest_lines)} entries)")

    # 2. derive the .scp flux via gw convert (build artifact).
    gw = resolve_gw()
    if gw is None:
        print("\ngen_corpus: `gw` not found (set $GW or put it on PATH) — "
              "source images + manifest regenerated, .scp derivation "
              "skipped. The differential test will skip until `gw` is "
              "available.")
        return 0

    print(f"\n  using gw: {gw}")
    failed = 0
    for name, suffix, _size, fmt in CORPUS_SPEC:
        src = SOURCES / f"{name}{suffix}"
        scp = FLUX / f"{name}.scp"
        try:
            r = subprocess.run(
                [gw, "convert", str(src), str(scp), f"--format={fmt}"],
                capture_output=True, timeout=180, check=False,
            )
        except (OSError, subprocess.TimeoutExpired) as e:
            print(f"  FLUX  {name}: gw convert failed to run: {e}")
            failed += 1
            continue
        if r.returncode != 0 or not scp.exists():
            print(f"  FLUX  {name}: gw convert exit {r.returncode}")
            sys.stderr.write(r.stderr.decode(errors="replace"))
            failed += 1
            continue
        print(f"  flux:   {scp.name}  ({scp.stat().st_size} bytes, "
              f"format {fmt})")

    if failed:
        print(f"\ngen_corpus: {failed}/{len(CORPUS_SPEC)} flux conversions "
              f"failed — see stderr above.")
        return 1
    print(f"\ngen_corpus: OK — {len(CORPUS_SPEC)} sources + {len(CORPUS_SPEC)} "
          f"flux streams.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
