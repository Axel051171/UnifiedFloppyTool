#!/usr/bin/env python3
"""Praeprozessor-Schalter, die nur EIN Build-System kennt.

Warum es das gibt (MF-468). `UFT_HAS_LIBUSB` wurde ausschliesslich von
`CMakeLists.txt:155` gesetzt — also nur im Test-Build. Der qmake-Build, aus dem
die Releases entstehen, definierte es nirgends. Drei Dateien fragen es ab:

    src/hal/uft_scp_direct.c    die ganze libusb-Implementierung
    src/hal/uft_xum1541.c       dito
    src/hardwaretab.cpp:998     `_has_production_transport` fuer scp/xum1541

Der fertige, samdisk-portierte SCP-Lesepfad war im Release damit unerreichbar,
und die GUI meldete ihn folgerichtig als nicht produktiv — waehrend die Tests
gruen liefen, weil CMake den Schalter setzt.

Kein bestehender Waechter konnte das sehen: `verify_build_sources.py`
vergleicht QUELLDATEIEN, keine Defines. Ein Schalter, der in einem Build an und
im anderen aus ist, ist aber genau dieselbe Klasse Divergenz — nur unsichtbar,
weil beide Builds fehlerfrei durchlaufen.

Drei Regeln:

  A  Ein `UFT_*`-Define, das ein Build-System setzen KANN und das andere nicht.
     Ausgewertet wird die Faehigkeit, nicht das Ergebnis: beide Seiten stehen
     in Bedingungen (`packagesExist`, `if(LIBUSB_FOUND)`), die dieses Skript
     nicht ausfuehrt. Zwei Systeme, die denselben Schalter unter je eigenen
     Bedingungen setzen, gelten als paritaetisch — dass die Bedingungen
     dasselbe MEINEN, kann nur ein Mensch entscheiden.

  B  Ein `UFT_HAS_*`/`UFT_ENABLE_*`, das der Quellcode abfragt, das aber KEIN
     Build-System setzt. Ein Schalter, der nie an sein kann.

  C  Ein `UFT_*`, das ein Build-System setzt und das niemand abfragt. Ein
     Schalter ohne Verbraucher.

Bekannte, begruendete Abweichungen stehen in `scripts/define_parity_baseline.json`
— mit Begruendung je Eintrag. Ein Eintrag dort heisst "geprueft und so
gewollt", nicht "faellt uns nicht auf".

Grenze, ausdruecklich: Defines aus `tests/CMakeLists.txt` zaehlen nicht als
Build-System-Seite. Der Test-Build darf zusaetzliche Schalter setzen (Mocks,
Korpus-Pfade); er ist nicht das Release.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

BASELINE = "scripts/define_parity_baseline.json"
SKIP_DIRS = {".git", "build", "proto", ".claude", "release", "debug"}

# Nur projekteigene Schalter. Qt-/CRT-/Kompatibilitaets-Defines
# (_CRT_SECURE_NO_WARNINGS, strcasecmp=...) haben je Plattform eigene Gruende.
_OWNED = re.compile(r"^UFT_\w+$")

_PRO_DEFINES = re.compile(r"^\s*DEFINES\s*\+=\s*(.+)$", re.M)
_CMAKE_ADD = re.compile(
    r"(?:add_compile_definitions|add_definitions|target_compile_definitions)"
    r"\s*\(([^)]*)\)", re.S)

# `#ifdef X` / `#ifndef X`
_QUERY_DEF = re.compile(r"#\s*(ifdef|ifndef)\s+(\w+)")
# `#if ...` / `#elif ...` — der ganze Ausdruck. `#if UFT_HAS_ZLIB` ohne
# defined() ist genauso eine Abfrage wie `#if defined(UFT_HAS_ZLIB)`; die
# erste Fassung dieses Skripts sah nur die zweite und hielt ZLIB fuer tot.
_QUERY_EXPR = re.compile(r"#\s*(?:if|elif)\s+([^\n]*)")
_IDENT = re.compile(r"\b[A-Za-z_]\w*\b")
# Include-Guard: `#ifndef X`, unmittelbar gefolgt von `#define X`. Das ist
# keine Feature-Abfrage und darf Regel C nicht als "Verbraucher" gelten.
_GUARD = re.compile(r"#\s*ifndef\s+(\w+)\s*\n\s*#\s*define\s+\1\b")

_BLOCK = re.compile(r"/\*.*?\*/", re.S)
_LINE_C = re.compile(r"//[^\n]*")


def strip_hash_comments(text: str) -> str:
    """`#`-Kommentare aus CMake/qmake entfernen, Zeilen erhalten.

    Ohne das liest der Waechter seine eigene Begruendung als Code: der
    Kommentar, der erklaert warum UFT_HAS_SWITCH entfernt wurde, steht INNERHALB
    des target_compile_definitions()-Blocks und nennt den Namen — der Schalter
    galt danach weiter als gesetzt. Ein Waechter, der Kommentare fuer
    Anweisungen haelt, meldet Befunde, die er selbst erzeugt hat.
    """
    out = []
    for line in text.split(chr(10)):
        in_str = False
        cut = len(line)
        for i, ch in enumerate(line):
            if ch == '"':
                in_str = not in_str
            elif ch == "#" and not in_str:
                cut = i
                break
        out.append(line[:cut])
    return chr(10).join(out)


def _names(blob: str) -> set[str]:
    """Define-Namen aus einem Textstueck; `NAME=1` und `-DNAME` inbegriffen."""
    out: set[str] = set()
    for tok in blob.replace(",", " ").split():
        tok = tok.strip().strip('"').lstrip("-")
        if tok.startswith("D") and len(tok) > 1 and tok[1].isupper():
            tok = tok[1:]
        name = tok.split("=", 1)[0]
        if _OWNED.match(name):
            out.add(name)
    return out


def qmake_defines(repo: Path) -> dict[str, list[str]]:
    """Define -> Fundstellen, aus der .pro und allen .pri."""
    found: dict[str, list[str]] = {}
    for pat in ("*.pro", "*.pri", "src/*.pri"):
        for p in sorted(repo.glob(pat)):
            text = strip_hash_comments(
                p.read_text(encoding="utf-8", errors="replace"))
            rel = str(p.relative_to(repo)).replace("\\", "/")
            for m in _PRO_DEFINES.finditer(text):
                line = text[:m.start()].count(chr(10)) + 1
                for n in _names(m.group(1)):
                    found.setdefault(n, []).append(f"{rel}:{line}")
    return found


def cmake_defines(repo: Path) -> dict[str, list[str]]:
    """Define -> Fundstellen, aus allen CMakeLists AUSSER tests/."""
    found: dict[str, list[str]] = {}
    for p in sorted(repo.rglob("CMakeLists.txt")):
        if any(s in p.parts for s in SKIP_DIRS):
            continue
        rel = str(p.relative_to(repo)).replace("\\", "/")
        if rel.startswith("tests/"):
            continue          # Test-Build ist nicht das Release
        text = strip_hash_comments(
            p.read_text(encoding="utf-8", errors="replace"))
        for m in _CMAKE_ADD.finditer(text):
            line = text[:m.start()].count(chr(10)) + 1
            for n in _names(m.group(1)):
                found.setdefault(n, []).append(f"{rel}:{line}")
    return found


def scan_sources(repo: Path) -> tuple[dict[str, list[str]], set[str]]:
    """(Praeprozessor-Abfragen je Define, alle im Quellcode genannten Namen).

    Die zweite Menge ist absichtlich weiter: eine Konstante wie
    UFT_VERSION_STRING wird als WERT benutzt, nicht in einer Bedingung. Sie
    deshalb fuer verbraucherlos zu halten, waere ein Falschbefund.
    """
    queries: dict[str, list[str]] = {}
    mentioned: set[str] = set()

    for base in ("src", "include"):
        root = repo / base
        if not root.is_dir():
            continue
        for p in sorted(root.rglob("*")):
            if p.suffix.lower() not in {".c", ".cpp", ".h", ".hpp"}:
                continue
            if any(s in p.parts for s in SKIP_DIRS):
                continue
            rel = str(p.relative_to(repo)).replace("\\", "/")
            if rel.startswith("src/samdisk/") or rel.startswith("src/a8rawconv/"):
                continue      # Fremdbestand, wird nicht gebaut
            text = p.read_text(encoding="utf-8", errors="replace")
            text = _LINE_C.sub("", _BLOCK.sub("", text))

            guards = set(_GUARD.findall(text))

            for m in _IDENT.finditer(text):
                if _OWNED.match(m.group(0)):
                    mentioned.add(m.group(0))

            for m in _QUERY_DEF.finditer(text):
                name = m.group(2)
                if not _OWNED.match(name) or name in guards:
                    continue
                line = text[:m.start()].count(chr(10)) + 1
                queries.setdefault(name, []).append(f"{rel}:{line}")

            for m in _QUERY_EXPR.finditer(text):
                line = text[:m.start()].count(chr(10)) + 1
                for name in _IDENT.findall(m.group(1)):
                    if name == "defined" or not _OWNED.match(name):
                        continue
                    queries.setdefault(name, []).append(f"{rel}:{line}")

    return queries, mentioned


def _load_baseline(repo: Path) -> dict[str, str]:
    p = repo / BASELINE
    if not p.exists():
        return {}
    data = json.loads(p.read_text(encoding="utf-8"))
    return {k: v for k, v in data.get("accepted", {}).items()}


def check(repo: Path) -> list[str]:
    pro = qmake_defines(repo)
    cm = cmake_defines(repo)
    q, mentioned = scan_sources(repo)
    known = _load_baseline(repo)

    errors: list[str] = []
    seen: set[str] = set()

    def report(key: str, msg: str) -> None:
        seen.add(key)
        if key not in known:
            errors.append(msg)

    # Regel A — Schalter kennt nur eine Seite
    for name in sorted(set(pro) | set(cm)):
        if name in pro and name not in cm:
            report(f"A:{name}",
                   f"'{name}' setzt nur der qmake-Build ({pro[name][0]}), "
                   f"CMake kennt ihn nicht. Ein Schalter, der in einem Build an "
                   f"und im anderen aus ist, laesst beide fehlerfrei durchlaufen "
                   f"und trotzdem verschiedenen Code entstehen (MF-468). "
                   f"Entweder in CMakeLists.txt nachziehen, oder mit Begruendung "
                   f"in {BASELINE} aufnehmen")
        elif name in cm and name not in pro:
            report(f"A:{name}",
                   f"'{name}' setzt nur der CMake-Build ({cm[name][0]}), die "
                   f"UnifiedFloppyTool.pro kennt ihn nicht — das ist die Seite, "
                   f"aus der die Releases entstehen. Genau so war "
                   f"UFT_HAS_LIBUSB im Release tot (MF-468). Entweder in der "
                   f".pro nachziehen, oder mit Begruendung in {BASELINE} "
                   f"aufnehmen")

    # Regel B — abgefragt, aber von niemandem gesetzt
    for name in sorted(q):
        if not name.startswith(("UFT_HAS_", "UFT_ENABLE_")):
            continue          # nur Feature-Schalter; Include-Guards o.ae. nicht
        if name in pro or name in cm:
            continue
        report(f"B:{name}",
               f"'{name}' wird abgefragt ({q[name][0]}), aber von keinem "
               f"Build-System gesetzt — der Zweig kann nie aktiv werden. "
               f"Setzen, entfernen, oder mit Begruendung in {BASELINE}")

    # Regel C — gesetzt, aber im Quellcode nirgends genannt
    for name in sorted(set(pro) | set(cm)):
        if name in mentioned:
            continue
        where = (pro.get(name) or cm.get(name))[0]
        report(f"C:{name}",
               f"'{name}' wird gesetzt ({where}), aber nirgends abgefragt — "
               f"ein Schalter ohne Verbraucher. Entfernen, oder mit "
               f"Begruendung in {BASELINE}")

    for key in sorted(set(known) - seen):
        errors.append(f"{key} ist keine Abweichung mehr — Eintrag aus "
                      f"{BASELINE} entfernen")
    return errors


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    pro, cm = qmake_defines(repo), cmake_defines(repo)
    q, mentioned = scan_sources(repo)

    print(f"UFT_*-Defines: qmake {len(pro)}, CMake {len(cm)}, "
          f"im Quellcode abgefragt {len(q)}, genannt {len(mentioned)}")
    if "--verbose" in sys.argv:
        for name in sorted(set(pro) | set(cm) | set(q)):
            print(f"  {name:28s} qmake={'ja' if name in pro else '--':3s} "
                  f"cmake={'ja' if name in cm else '--':3s} "
                  f"abgefragt={'ja' if name in q else '--'}")

    errs = check(repo)
    for e in errs:
        print(f"  {e}")
    print(f"Abweichungen: {len(errs)}")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
