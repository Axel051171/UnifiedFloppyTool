#!/usr/bin/env python3
"""
Build-System Parity Check (MF-006, Praemisse korrigiert MF-458).

**Die Rollen haben sich umgedreht, seit dieses Skript geschrieben wurde.**

Urspruenglich globte `CMakeLists.txt` per `file(GLOB_RECURSE ...)`, waehrend
`UnifiedFloppyTool.pro` jede Datei einzeln auffuehrt — eine neue `.c` unter
`src/` landete also automatisch im CMake-Build und fehlte still im
Release-Build. Genau das fing dieses Skript ab.

Heute ist es andersherum: `CMakeLists.txt:202` ruft dieses Skript mit
`--emit-cmake-sources` auf und baut daraus seine Quellliste. Die `.pro` ist
damit die einzige Wahrheit, und CMake leitet ab. "CMake hat mehr als qmake"
kann strukturell nicht mehr vorkommen.

Was Gap A jetzt bedeutet: **eine Datei liegt unter `src/`, steht aber in
keinem Build.** Das ist eine andere und schwaechere Aussage — meistens ist es
Absicht (Referenzbestand, Opt-in-Feature, toter Code), gelegentlich ein
vergessener Eintrag.

MF-458 hat dabei einen echten Fehler in diesem Parser gefunden: er verband
erst Zeilenfortsetzungen und strippte dann Kommentare. In der `.pro` steht
eine auskommentierte Zeile, die selbst auf einem Backslash endet — danach galt der
gesamte Rest des SOURCES-Blocks als Kommentar. **30 Dateien fielen weg**,
darunter `src/flux/uft_flux_decoder.c`, `src/gui/uft_otdr_panel.cpp` und
`src/gui/uft_sector_editor.cpp`. Weil CMake seine Liste von hier bezieht,
wurde die CMake-Anwendung aus 559 statt 589 Quellen gebaut. Der
qmake-Release-Build war nie betroffen — der parst seine Datei selbst.

Was wir pruefen:
  (A) Dateien unter src/, die in keinem Build stehen       = evtl. vergessen
  (B) Dateien in .pro, die es auf der Platte nicht gibt    = tote Eintraege

A und B blockieren nur, wenn sie von der Baseline abweichen. Bewusst
ausgenommen (siehe NOT_BUILT_BY_DESIGN): `src/samdisk/` als Referenzbestand
und die Quellen hinter Opt-in-Flags.

Run:
  python3 scripts/verify_build_sources.py                  # against baseline
  python3 scripts/verify_build_sources.py --verbose
  python3 scripts/verify_build_sources.py --rebuild-baseline   # refresh after cleanup
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


# Exclusion regexes mirrored from CMakeLists.txt `list(FILTER ... EXCLUDE REGEX)`.
CMAKE_EXCLUSIONS: list[re.Pattern[str]] = [
    re.compile(r".*/tests?/.*"),
    re.compile(r".*/test_.*"),
    re.compile(r".*_example\.cpp$"),
    re.compile(r".*/formats_v2/.*"),
    re.compile(r"^src/main\.cpp$"),
]

SOURCE_EXTS = {".c", ".cpp"}

# MF-458: was unter src/ liegt und absichtlich in keinem Build steht.
#
# Das gehoert nicht in eine Baseline. Eine Baseline sagt "bekannter Mangel,
# noch nicht behoben"; hier ist der Zustand aber der richtige, und ihn Jahr um
# Jahr als 100 Zeilen Altlast mitzuschleppen macht die Zahl bedeutungslos.
NOT_BUILT_BY_DESIGN: list[re.Pattern[str]] = [
    # SAMdisk 4.0 (c) 2002-2024 Simon Owen, MIT. Referenzbestand, kein
    # Baubestandteil: beide Builds binden nur `src/samdisk` als Include-Pfad
    # ein, kompiliert wird nichts davon. Der Baum zitiert es als Spec-Quelle
    # mit Datei:Zeile — z.B. src/formats/td0/uft_td0.c:15
    # ("Authority: src/samdisk/td0.cpp:10") und
    # include/uft/hal/uft_scp_direct.h:101. Siehe src/samdisk/README.md.
    re.compile(r"^src/samdisk/"),
    # a8rawconv 0.95 (c) 2014-2023 Avery Lee, GPL-2.0-or-later. Zweiter
    # Referenzbestand, gleiche Rolle wie SAMdisk und ebenfalls kein
    # Baubestandteil — SAMdisk deckt PC/CPC/Sinclair ab, a8rawconv die
    # Atari-8-bit-Seite (ATX/VAPI, FM, Interleave) und Apple/Mac-GCR.
    # Anders als SAMdisk wird hier nicht einmal ein Include-Pfad gebunden.
    # Siehe src/a8rawconv/README.md.
    re.compile(r"^src/a8rawconv/"),
    # Opt-in-Features. Die .pro-Seite wird ueber _OPTIN_FLAGS uebersprungen,
    # also muss die Platten-Seite dasselbe tun — sonst meldet der Pruefer
    # seine eigene Auslassung als Befund.
    re.compile(r"^src/algorithms/(advanced/)?uft_kalman_pll(_v2)?\.c$"),
    re.compile(r"^src/flux/fdc_bitstream/vfo_experimental\.cpp$"),
]


def collect_cmake_globbed_sources(repo_root: Path) -> set[str]:
    """Replicate what CMake's GLOB_RECURSE picks up for UFT_SOURCES."""
    out: set[str] = set()
    src = repo_root / "src"
    if not src.is_dir():
        return out
    for path in src.rglob("*"):
        if path.suffix not in SOURCE_EXTS:
            continue
        if not path.is_file():
            continue
        rel = path.relative_to(repo_root).as_posix()
        if any(p.search(rel) for p in CMAKE_EXCLUSIONS):
            continue
        if any(p.search(rel) for p in NOT_BUILT_BY_DESIGN):
            continue
        out.add(rel)
    return out


def parse_pro_sources(pro_path: Path) -> set[str]:
    """Return the SOURCES listed in the .pro file."""
    return _parse_pro_lists(pro_path, key="SOURCES", exts=(".c", ".cpp"))


def parse_pro_headers(pro_path: Path) -> set[str]:
    """Return the HEADERS listed in the .pro file."""
    return _parse_pro_lists(pro_path, key="HEADERS", exts=(".h", ".hpp"))


# Opt-in qmake feature flags. SOURCES/HEADERS inside `<flag> { ... }` blocks
# only ship when CONFIG+=<flag> is passed. CI does not enable any of these,
# so the parser must skip their contents to mirror the default qmake build.
_OPTIN_FLAGS: frozenset[str] = frozenset({
    "experimental_vfo",
    "kalman_pll",
    "switch_support",
})


def _parse_pro_lists(pro_path: Path, key: str, exts: tuple[str, ...]) -> set[str]:
    """Common parser for SOURCES/HEADERS — handles `+=`, line continuations,
    and skips opt-in feature blocks (kalman_pll, experimental_vfo, etc.).
    """
    if not pro_path.is_file():
        sys.stderr.write(f"verify_build_sources: {pro_path} not found\n")
        sys.exit(2)

    text = pro_path.read_text(encoding="utf-8", errors="replace")

    # MF-458: Kommentare ZUERST weg, dann Fortsetzungen verbinden — in dieser
    # Reihenfolge, weil qmake es so macht.
    #
    # Vorher wurden erst die Fortsetzungen verbunden. In der .pro steht eine
    # auskommentierte Zeile, die selbst auf einen Fortsetzungs-Backslash
    # endet (src/qmake_stubs/uft_protection_stubs.cpp, DISABLED). Nach dem
    # Verbinden war alles ab dem ersten '#' eine einzige Zeile, und das
    # anschliessende Kommentar-Strippen loeschte den REST des SOURCES-Blocks.
    # Sieben real gebaute Dateien — darunter src/flux/uft_flux_decoder.c,
    # src/gui/uft_otdr_panel.cpp und src/gui/uft_sector_editor.cpp — galten
    # damit als 'nicht im Release-Build'. Nachgeprueft mit `qmake6 -o` und
    # einem Blick ins erzeugte Makefile: qmake kompiliert sie sehr wohl.
    # Der Fehler war im Pruefer, nicht im Build, und er hat die Baseline
    # aufgeblaeht.
    text = re.sub(r"#[^\n]*", "", text)
    text = re.sub(r"\\\s*\n\s*", " ", text)  # join line continuations

    pat = re.compile(rf"^\s*{key}\s*\+?=\s*(.*?)\s*$")
    optin_open = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*\{\s*$")
    any_open = re.compile(r"\{\s*$")
    any_close = re.compile(r"^\s*\}")
    out: set[str] = set()
    skip_depth = 0   # nesting depth inside an opt-in block (0 = active)
    brace_depth = 0  # absolute brace depth, for tracking the matching close
    skip_origin = -1
    for line in text.splitlines():
        if skip_depth == 0:
            m_optin = optin_open.match(line)
            if m_optin and m_optin.group(1) in _OPTIN_FLAGS:
                skip_depth = 1
                skip_origin = brace_depth
                brace_depth += 1
                continue
        if any_open.search(line):
            brace_depth += 1
            if skip_depth > 0:
                skip_depth += 1
        if any_close.match(line):
            brace_depth = max(0, brace_depth - 1)
            if skip_depth > 0:
                skip_depth -= 1
                if skip_depth == 0 and brace_depth == skip_origin:
                    skip_origin = -1
            continue
        if skip_depth > 0:
            continue
        m = pat.match(line)
        if not m:
            continue
        rest = re.sub(r"#.*$", "", m.group(1))
        for tok in rest.split():
            if tok.startswith("$$") or tok == "+=":
                continue
            if tok.endswith(exts):
                out.add(tok)
    return out


def load_baseline(path: Path) -> tuple[set[str], set[str]]:
    """Return (in_cmake_not_in_pro, in_pro_not_on_disk) sets from TSV."""
    a: set[str] = set()
    b: set[str] = set()
    if not path.is_file():
        return a, b
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        kind, _, file_ = line.partition("\t")
        if kind == "A":
            a.add(file_)
        elif kind == "B":
            b.add(file_)
    return a, b


def write_baseline(path: Path, miss_a: set[str], miss_b: set[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        f.write(
            "# Build-system divergence baseline (MF-006).\n"
            "# Accepted historical gaps between .pro and CMake GLOB.\n"
            "# Format:  <kind>\\t<path>\n"
            "#   kind A = file on disk, absent from .pro (CMake compiles it, qmake skips)\n"
            "#   kind B = file listed in .pro but not on disk (stale entry)\n"
            "#\n"
            "# New divergence (not in this file) makes verify_build_sources.py fail.\n"
            "# This baseline shrinks as cleanup work (MF-004 stubs, samdisk split,\n"
            "# etc.) lands. Regenerate with:\n"
            "#   python3 scripts/verify_build_sources.py --rebuild-baseline\n\n"
        )
        for p in sorted(miss_a):
            f.write(f"A\t{p}\n")
        for p in sorted(miss_b):
            f.write(f"B\t{p}\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--rebuild-baseline", action="store_true",
                    help="Overwrite the baseline with current state. Use after "
                    "cleanup commits that legitimately resolve entries.")
    ap.add_argument("--root", type=Path,
                    default=Path(__file__).resolve().parent.parent)
    ap.add_argument("--emit-cmake-sources", action="store_true",
                    help="Print .pro SOURCES as semicolon-list for CMake "
                    "execute_process(). Filters to files that exist on disk.")
    ap.add_argument("--emit-cmake-headers", action="store_true",
                    help="Print .pro HEADERS as semicolon-list for CMake.")
    args = ap.parse_args()

    repo = args.root.resolve()
    pro = repo / "UnifiedFloppyTool.pro"
    baseline_path = repo / "data" / "build_system_baseline.tsv"

    if args.emit_cmake_sources or args.emit_cmake_headers:
        items = (parse_pro_sources(pro) if args.emit_cmake_sources
                 else parse_pro_headers(pro))
        # Drop stale entries (file not on disk) so CMake never tries to
        # compile a missing path.
        existing = sorted(p for p in items if (repo / p).is_file())
        sys.stdout.write(";".join(existing))
        return 0

    cmake_sources = collect_cmake_globbed_sources(repo)
    pro_sources = parse_pro_sources(pro)

    miss_a = cmake_sources - pro_sources   # on disk, not in .pro
    miss_b = {p for p in pro_sources if not (repo / p).is_file()}

    if args.rebuild_baseline:
        write_baseline(baseline_path, miss_a, miss_b)
        print(f"rebuilt baseline {baseline_path}:")
        print(f"  A: {len(miss_a)} files on disk, absent from .pro")
        print(f"  B: {len(miss_b)} stale .pro entries")
        return 0

    base_a, base_b = load_baseline(baseline_path)
    new_a = miss_a - base_a   # NEW silent misses (regression)
    new_b = miss_b - base_b   # NEW stale entries
    fixed_a = base_a - miss_a  # baseline entries that are now resolved
    fixed_b = base_b - miss_b

    print(f"Build-system parity check (MF-006)")
    print(f"  Quellen unter src/ (baubar) : {len(cmake_sources):4d}")
    print(f"  .pro SOURCES-Eintraege      : {len(pro_sources):4d}")
    print(f"  Baseline akzeptiert A/B     : {len(base_a):4d} / {len(base_b):4d}")
    print(f"  Aktuell A/B                 : {len(miss_a):4d} / {len(miss_b):4d}")
    print(f"  NEUE Abweichungen A/B       : {len(new_a):4d} / {len(new_b):4d}")
    print(f"  Baseline-Eintraege erledigt : {len(fixed_a):4d} / {len(fixed_b):4d}")

    if args.verbose or new_a or new_b:
        if new_a:
            print("\n  === NEW (A) on disk, missing from .pro ===")
            for p in sorted(new_a):
                print(f"    + {p}")
        if new_b:
            print("\n  === NEW (B) stale .pro entry ===")
            for p in sorted(new_b):
                print(f"    - {p}")

    if fixed_a or fixed_b:
        print("\n  NOTE: baseline entries resolved above — consider running")
        print("        `verify_build_sources.py --rebuild-baseline` to shrink")
        print("        the accepted-gap set.")

    if new_a or new_b:
        print("\nFAIL: new build-system divergence since baseline (MF-006).")
        print("      Add the file to UnifiedFloppyTool.pro (usual case) or")
        print("      extend CMakeLists.txt exclusions (if intentionally skipped).")
        return 1

    print("\nOK: no new divergence beyond the accepted baseline.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
