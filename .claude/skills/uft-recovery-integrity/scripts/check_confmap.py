#!/usr/bin/env python3
"""
check_confmap.py — verify confidence_map invariants on a recovery dump.

Reads a JSON dump from `uft recover --emit-confidence` and checks:

  - For every byte: if confidence > 0, it must come from a verified
    pass (source_pass != 0xFF, or flags has VERIFIED bit).
  - For every byte: if flags has RECONSTRUCTED, confidence must be < 255.
  - For every byte: if confidence == 0, the byte should be the 0xFF
    sentinel (or the dump explains why not).

Usage:
  python3 check_confmap.py <dump.json>

Exit:
  0 — all invariants hold
  1 — one or more violations found
  2 — file unreadable / wrong format

Dump JSON shape (canonical):
  {
    "track": int, "head": int, "sector": int,
    "data":            [byte, byte, ...],   # length = sector_size
    "confidence_map":  [byte, byte, ...],   # length = sector_size
    "source_pass":     [byte, byte, ...],   # length = sector_size
    "flags":           int,                  # bitfield, sector-level
    "per_byte_flags":  [int, int, ...]      # optional, byte-level
  }

flag bits (from include/uft/recovery/uft_forensic_types.h):
  0x01 UFT_FS_FLAG_WEAK
  0x02 UFT_FS_FLAG_DELETED
  0x04 UFT_FS_FLAG_RECONSTRUCTED
  0x08 UFT_FS_FLAG_VERIFIED
"""

import json
import sys

UFT_FS_FLAG_WEAK          = 0x01
UFT_FS_FLAG_DELETED       = 0x02
UFT_FS_FLAG_RECONSTRUCTED = 0x04
UFT_FS_FLAG_VERIFIED      = 0x08

UFT_RECONSTRUCTED_SENTINEL = 0xFF


def check(dump):
    violations = []

    data       = dump.get("data") or []
    confmap    = dump.get("confidence_map") or []
    source_p   = dump.get("source_pass") or []
    flags      = dump.get("flags", 0)
    byte_flags = dump.get("per_byte_flags") or [0] * len(data)

    if len(confmap) != len(data):
        return [f"length mismatch: data={len(data)}, confidence_map={len(confmap)}"]

    if len(source_p) != len(data) and len(source_p) != 0:
        return [f"length mismatch: data={len(data)}, source_pass={len(source_p)}"]

    sector_reconstructed = bool(flags & UFT_FS_FLAG_RECONSTRUCTED)

    for i, (byte, conf) in enumerate(zip(data, confmap)):
        bf = byte_flags[i] if i < len(byte_flags) else 0
        sp = source_p[i] if i < len(source_p) else 0xFF

        # Invariant 1: confidence > 0 must trace to a verified pass
        # (either source_pass set OR flags has VERIFIED OR sector-level
        # flags includes VERIFIED for the entire sector).
        if conf > 0:
            traceable = (
                sp != 0xFF
                or (bf & UFT_FS_FLAG_VERIFIED)
                or (flags & UFT_FS_FLAG_VERIFIED)
            )
            if not traceable:
                violations.append(
                    f"byte[{i}]: confidence={conf} but no verified source "
                    f"(source_pass=0xFF, no VERIFIED flag) — fabricated?"
                )

        # Invariant 2: RECONSTRUCTED bytes must have confidence < 255
        if (bf & UFT_FS_FLAG_RECONSTRUCTED) and conf == 255:
            violations.append(
                f"byte[{i}]: RECONSTRUCTED flag set but confidence=255 — "
                f"contradiction"
            )

        # Invariant 3: confidence == 0 means we don't know — byte should
        # be the sentinel, otherwise the dump is misleading.
        if conf == 0 and byte != UFT_RECONSTRUCTED_SENTINEL:
            # Soft warning — the user may have a domain-specific sentinel.
            # Only flag if the byte looks "real".
            if 0x20 <= byte < 0x7F:    # printable ASCII = looks real
                violations.append(
                    f"byte[{i}]: confidence=0 but byte=0x{byte:02x} "
                    f"({chr(byte)!r}) — claims unknown but emits real-looking value"
                )

    # Invariant 4: if sector-level RECONSTRUCTED is set, at least one
    # byte must have the per-byte RECONSTRUCTED flag.
    if sector_reconstructed:
        any_byte = any(bf & UFT_FS_FLAG_RECONSTRUCTED for bf in byte_flags)
        if not any_byte and byte_flags:
            violations.append(
                "sector flag has RECONSTRUCTED set, but no byte has "
                "the per-byte RECONSTRUCTED flag — inconsistent"
            )

    return violations


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <dump.json>", file=sys.stderr)
        sys.exit(2)

    try:
        with open(sys.argv[1]) as f:
            dump = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        print(f"error reading {sys.argv[1]}: {e}", file=sys.stderr)
        sys.exit(2)

    # Support either a single sector dump or a list of them
    if isinstance(dump, list):
        sectors = dump
    elif "data" in dump:
        sectors = [dump]
    else:
        sectors = dump.get("sectors", [])

    total_violations = 0
    for idx, sector in enumerate(sectors):
        v = check(sector)
        if v:
            label = (f"sector {idx} "
                     f"(t={sector.get('track','?')}, "
                     f"h={sector.get('head','?')}, "
                     f"s={sector.get('sector','?')})")
            print(f"{label}: {len(v)} violation(s)")
            for line in v:
                print(f"  {line}")
            total_violations += len(v)

    if total_violations == 0:
        print(f"check_confmap: clean ({len(sectors)} sector(s))")
        sys.exit(0)
    else:
        print(f"\ncheck_confmap: {total_violations} violation(s) total — "
              f"see SKILL.md invariants I1, I3, I5 for the rules.")
        sys.exit(1)


if __name__ == "__main__":
    main()
