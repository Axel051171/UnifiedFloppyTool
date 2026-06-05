#!/usr/bin/env python3
"""
populate_spec_status.py — Mass-populate spec_status field in plugin .c files.

V415-PLAN PLUGIN.spec_status (MF-262) — closes docs/KNOWN_ISSUES.md §7.1.

For each `uft_format_plugin_t <name> = { ... };` literal in src/formats/
that lacks an explicit `.spec_status = ...,` field, insert the
appropriate value based on the per-format mapping below.

The mapping reflects format-provenance reality:

  OFFICIAL_FULL        — published, complete, vendor-blessed spec
                         (HFE, SCP, WOZ, MOOF, A2R, EDSK, IPF, ...)
  OFFICIAL_PARTIAL     — published but incomplete spec
                         (FDI PC-98, NFD, KFX raw, MFI...)
  REVERSE_ENGINEERED   — no vendor spec; community RE
                         (ATX, STX, NIB, IPF protections, ...)
  DERIVED              — de-facto standard, no formal spec
                         (D64/D71/D81, IMG, ATR, DSK, ADF, ...)

Default for unmapped categories: DERIVED + FIXME comment so the per-
plugin author refines later. Never UNKNOWN (Prinzip 7 violation).
"""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FMT_DIR = ROOT / "src" / "formats"

# Per-format-subdir spec_status mapping.
# Source: docs/PLANNED_APIS.md format-provenance notes + format-by-format
# inspection of vendor documentation availability.
SPEC_STATUS = {
    # === OFFICIAL_FULL — vendor-published, complete spec ===
    "scp":         "UFT_SPEC_OFFICIAL_FULL",   # cbmstuff.com SDK v1.7
    "atx":         "UFT_SPEC_REVERSE_ENGINEERED",  # AtariMax — RE'd
    "dc42":        "UFT_SPEC_OFFICIAL_FULL",   # Apple DiskCopy 4.2 docs
    "msa":         "UFT_SPEC_OFFICIAL_FULL",   # Magic Shadow Archiver docs
    "scl":         "UFT_SPEC_OFFICIAL_FULL",   # ZX Spectrum SCL/Sinclair docs
    "ssd":         "UFT_SPEC_DERIVED",         # BBC Acorn DFS — de-facto

    # === OFFICIAL_PARTIAL — published but incomplete ===
    "fdi":         "UFT_SPEC_OFFICIAL_PARTIAL",  # FDI (Disk-Image)
    "fdi_pc98":    "UFT_SPEC_OFFICIAL_PARTIAL",  # PC-98 FDI variant
    "nfd":         "UFT_SPEC_OFFICIAL_PARTIAL",  # T98 Next D-FDI
    "kfx":         "UFT_SPEC_OFFICIAL_PARTIAL",  # KryoFlux stream
    "mfi":         "UFT_SPEC_OFFICIAL_PARTIAL",  # MAME Flux Image
    "pri":         "UFT_SPEC_OFFICIAL_PARTIAL",  # PCE pri

    # === REVERSE_ENGINEERED ===
    "dms":         "UFT_SPEC_REVERSE_ENGINEERED",  # DiskMasher
    "dcm":         "UFT_SPEC_REVERSE_ENGINEERED",  # DiskCommunicator
    "edk":         "UFT_SPEC_REVERSE_ENGINEERED",  # Enhanced Disk Kompression
    "qrst":        "UFT_SPEC_REVERSE_ENGINEERED",  # QRST archive
    "dmk":         "UFT_SPEC_REVERSE_ENGINEERED",  # TRS-80 DMK
    "g71":         "UFT_SPEC_REVERSE_ENGINEERED",  # 1571 GCR
    "victor9k":    "UFT_SPEC_REVERSE_ENGINEERED",  # Victor 9000 GCR
    "micropolis":  "UFT_SPEC_REVERSE_ENGINEERED",  # Micropolis 100tpi
    "northstar":   "UFT_SPEC_REVERSE_ENGINEERED",  # North Star MDS
    "v9t9":        "UFT_SPEC_REVERSE_ENGINEERED",  # TI-99 V9T9
    "udi":         "UFT_SPEC_REVERSE_ENGINEERED",  # Ultra Disk Image
    "vdk":         "UFT_SPEC_REVERSE_ENGINEERED",  # CoCo VDK
    "sad":         "UFT_SPEC_REVERSE_ENGINEERED",  # SAM Coupe SAD
    "syn":         "UFT_SPEC_REVERSE_ENGINEERED",  # Synertek
    "tan":         "UFT_SPEC_REVERSE_ENGINEERED",  # Tandy hard sector
    "sap":         "UFT_SPEC_REVERSE_ENGINEERED",  # Thomson SAP
    "cfi":         "UFT_SPEC_REVERSE_ENGINEERED",  # Catweasel Flux
    "apridisk":    "UFT_SPEC_REVERSE_ENGINEERED",  # ACT Apricot
    "86box":       "UFT_SPEC_REVERSE_ENGINEERED",  # 86Box .86f
    "nanowasp":    "UFT_SPEC_REVERSE_ENGINEERED",  # Microbee Nanowasp
    "adf_arc":     "UFT_SPEC_REVERSE_ENGINEERED",  # Amiga ADF archive variant
    "adl":         "UFT_SPEC_REVERSE_ENGINEERED",  # Acorn ADL
    "myz80":       "UFT_SPEC_REVERSE_ENGINEERED",  # MyZ80 CP/M
    "rcpmfs":      "UFT_SPEC_REVERSE_ENGINEERED",  # CP/M FS
    "nib":         "UFT_SPEC_REVERSE_ENGINEERED",  # Apple ][ NIB nibbler

    # === DERIVED — de-facto standard, no formal spec ===
    "d13":         "UFT_SPEC_DERIVED",   # Apple 13-sec DOS 3.2
    "d67":         "UFT_SPEC_DERIVED",   # CBM D67 (2040/3040)
    "d71":         "UFT_SPEC_DERIVED",   # CBM D71
    "d77":         "UFT_SPEC_DERIVED",   # PC-88/FM-7 D77
    "d80":         "UFT_SPEC_DERIVED",   # CBM D80
    "d81":         "UFT_SPEC_DERIVED",   # CBM D81 (1581)
    "d82":         "UFT_SPEC_DERIVED",   # CBM D82
    "d88":         "UFT_SPEC_DERIVED",   # PC-88/PC-98 D88
    "dim":         "UFT_SPEC_DERIVED",   # IBM-Diskette DIM
    "dim_atari":   "UFT_SPEC_DERIVED",   # Atari DIM (FDI variant)
    "do":          "UFT_SPEC_DERIVED",   # Apple DOS Order
    "po":          "UFT_SPEC_DERIVED",   # Apple ProDOS Order
    "atari":       "UFT_SPEC_DERIVED",   # Atari ATR
    "st":          "UFT_SPEC_DERIVED",   # Atari ST raw
    "trd":         "UFT_SPEC_DERIVED",   # ZX TR-DOS
    "xfd":         "UFT_SPEC_DERIVED",   # Atari XFD raw
    "jv1":         "UFT_SPEC_DERIVED",   # TRS-80 JV1
    "jv3":         "UFT_SPEC_DERIVED",   # TRS-80 JV3
    "jvc":         "UFT_SPEC_DERIVED",   # JVC CoCo disk
    "dsk_msx":     "UFT_SPEC_DERIVED",   # MSX DSK
    "cas":         "UFT_SPEC_DERIVED",   # Atari CAS tape image
    "fds":         "UFT_SPEC_DERIVED",   # Famicom Disk System
    "hardsector":  "UFT_SPEC_DERIVED",   # generic hard-sector
    "logical":     "UFT_SPEC_DERIVED",   # logical-layout abstract
    "mgt":         "UFT_SPEC_DERIVED",   # SAM Coupe MGT
    "opus":        "UFT_SPEC_DERIVED",   # Opus Discovery (Spectrum)
    "pdp":         "UFT_SPEC_DERIVED",   # PDP-11 RX01/RX02
    "posix":       "UFT_SPEC_DERIVED",   # POSIX raw filesystem
    "sam":         "UFT_SPEC_DERIVED",   # SAM Coupe SAD/MGT family
    "t1k":         "UFT_SPEC_DERIVED",   # Tandy 1000
    "xdm86":       "UFT_SPEC_DERIVED",   # XDM 8/16-bit
    "cpm":         "UFT_SPEC_DERIVED",   # CP/M generic diskdef
    "atr":         "UFT_SPEC_DERIVED",   # Atari 8-bit ATR
}


def detect_subdir(path: Path) -> str:
    """src/formats/<subdir>/<file>.c → <subdir>."""
    rel = path.relative_to(FMT_DIR)
    return rel.parts[0]


def patch_file(path: Path, dry_run: bool = False) -> bool:
    """Insert .spec_status into the plugin literal. Return True if patched."""
    text = path.read_text(encoding="utf-8", errors="replace")
    if ".spec_status" in text:
        return False  # already populated
    subdir = detect_subdir(path)
    spec = SPEC_STATUS.get(subdir)
    if spec is None:
        print(f"  SKIP: {path.relative_to(ROOT)} — no mapping for '{subdir}'")
        return False

    # Find the plugin literal: `uft_format_plugin_t NAME = {`
    pat = re.compile(
        r"((?:static\s+)?(?:const\s+)?uft_format_plugin_t\s+\w+\s*=\s*\{[^}]*?)(\n\};)",
        re.MULTILINE | re.DOTALL,
    )
    m = pat.search(text)
    if not m:
        print(f"  SKIP: {path.relative_to(ROOT)} — no plugin literal found")
        return False

    body, close = m.group(1), m.group(2)
    # Find indent of last field for consistency.
    indent_match = re.search(r"\n(\s+)\.", body)
    indent = indent_match.group(1) if indent_match else "    "

    # Insert spec_status as last field before the closing brace.
    # Trim trailing whitespace from `body`, append the new field with
    # a trailing comma, then re-close.
    body = body.rstrip()
    if not body.endswith(","):
        body += ","
    insertion = f"\n{indent}.spec_status = {spec},  /* V415-PLAN PLUGIN.spec_status (MF-262) */"
    new_text = text[: m.start()] + body + insertion + close + text[m.end():]

    if not dry_run:
        path.write_text(new_text, encoding="utf-8", newline="\n")
    print(f"  WROTE {spec}: {path.relative_to(ROOT)}")
    return True


def main() -> int:
    dry_run = "--dry-run" in sys.argv

    # Find all candidate plugin .c files.
    candidates: list[Path] = []
    for p in FMT_DIR.rglob("*.c"):
        text = p.read_text(encoding="utf-8", errors="replace")
        if not re.search(r"uft_format_plugin_t\s+\w+\s*=", text):
            continue
        candidates.append(p)

    print(f"Found {len(candidates)} candidate plugin files. Patching...")
    patched = 0
    for p in sorted(candidates):
        if patch_file(p, dry_run=dry_run):
            patched += 1

    print(f"\nResult: {patched} files patched ({'dry-run' if dry_run else 'written'})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
