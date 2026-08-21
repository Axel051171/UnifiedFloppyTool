#!/usr/bin/env python3
"""Die POSIX-Shims bleiben Opt-in, und Endianness wird nie mit #ifdef gefragt.

Warum es das gibt (MF-455, ARCH-1). `include/uft/uft_platform.h` und
`include/uft/compat/uft_platform.h` trugen denselben Include-Guard. Folge: der
grosse Header band compat ein und bekam nie etwas davon — der Kommentar
"Include compatibility layer first for POSIX functions on Windows" beschrieb
eine Absicht, die nie wirksam war. Umgekehrt sahen die zwei .c-Dateien, die
compat direkt einbinden, den grossen Header nie.

Ein blosses Umbenennen des Guards brach 2026-08-18 (MF-416) 187 von 197 Tests,
weil compat nicht nur erkennt, sondern Standardnamen belegt:

    #define close   _close
    #define mkdir(path, mode) _mkdir(path)

Baumweit trifft `mkdir(path, mode)` jeden echten zweiargumentigen POSIX-Aufruf.

Aufgeloest durch Trennen: `uft_platform_base.h` traegt die gefahrlose Haelfte
und ist baumweit sichtbar, `uft_platform.h` (compat) behaelt die Shims und
einen eigenen Guard. Zwei Regeln halten das:

  A. `uft/compat/uft_platform.h` darf nur von .c/.cpp-Dateien eingebunden
     werden, die in der Liste unten stehen — nie von einem Header, denn ueber
     einen Header verbreiten sich die Shims unkontrolliert weiter.

  C. Das Compiler-Attribut-Vokabular (UFT_INLINE, UFT_NOINLINE, UFT_RESTRICT,
     UFT_THREAD_LOCAL, UFT_ALIGNED, UFT_ALIGNOF, UFT_CACHE_ALIGNED) wird nur in
     `include/uft/uft_compiler.h` definiert, die Architektur-Identitaet
     (UFT_ARCH_NAME, UFT_ARCH_BITS, UFT_CACHE_LINE_SIZE) nur in
     `include/uft/compat/uft_platform_base.h`.

     MF-456: dieselben Namen standen in bis zu vier Headern mit
     unterschiedlichen Ergebnissen. UFT_INLINE hiess `static inline`,
     `inline __attribute__((always_inline))` oder `static __inline` — die
     mittlere Fassung ohne `static` ist ein Link-Fehler, sobald der Compiler
     nicht inlinet. UFT_CACHE_LINE_SIZE war 32 auf ARM32 in einem Header und
     ungeschuetzt 64 in einem anderen. UFT_ARCH_NAME war "ARM64" oder "arm64".
     In uft_simd.h stand UFT_ALIGNED hinter `#ifndef UFT_LIKELY` — der
     Waechter nannte ein anderes Makro als das, was er schuetzte.

  B. Kein `#ifdef`/`#if defined` auf UFT_BIG_ENDIAN oder UFT_LITTLE_ENDIAN.
     Beide sind seit MF-455 IMMER 0 oder 1; vorher waren sie je nach Header
     "definiert oder abwesend" bzw. "0 oder 1", `#ifdef` antwortete also
     entgegengesetzt. Die richtige Frage ist `#if UFT_BIG_ENDIAN`.

Grenze: geprueft wird der Text der `#include`-Zeilen, nicht der
Praeprozessor-Graph. Ein Header, der compat ueber einen Zwischenschritt
einbindet, faellt hier nicht auf — deswegen zusaetzlich Regel A auf ALLE
Header, nicht nur auf die bekannten.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

SKIP_DIRS = {".git", "build", "proto", ".claude", "release", "debug"}

SHIM_HEADER = "uft/compat/uft_platform.h"

# Wer die Shims braucht. Beide binden compat als erste Zeile ein und ziehen
# uft/uft_platform.h nicht transitiv (geprueft mit gcc -MM in MF-455).
SHIM_CONSUMERS = {
    "src/formats/legacy/uft_imd.c",
    "src/hal/uft_greaseweazle_full.c",
}

_INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)

# Wer welches Vokabular besitzt (MF-456).
VOCAB_OWNER = {
    "include/uft/uft_compiler.h": {
        "UFT_INLINE", "UFT_FORCE_INLINE", "UFT_NOINLINE", "UFT_RESTRICT",
        "UFT_THREAD_LOCAL", "UFT_ALIGNED", "UFT_ALIGNOF", "UFT_CACHE_ALIGNED",
        "UFT_SSE_ALIGNED",
    },
    "include/uft/compat/uft_platform_base.h": {
        "UFT_ARCH_NAME", "UFT_ARCH_BITS", "UFT_CACHE_LINE_SIZE",
        "UFT_BIG_ENDIAN", "UFT_LITTLE_ENDIAN", "UFT_PATH_SEP",
        "UFT_PATH_SEP_STR",
    },
}
_ALL_VOCAB = {n: o for o, names in VOCAB_OWNER.items() for n in names}
_VOCAB_DEF = re.compile(
    r"^[ 	]*#[ 	]*define[ 	]+(UFT_[A-Z_0-9]+)", re.M)
_ENDIAN_IFDEF = re.compile(
    r"^\s*#\s*(?:ifdef|ifndef)\s+(UFT_BIG_ENDIAN|UFT_LITTLE_ENDIAN)\b|"
    r"^\s*#\s*(?:if|elif)\s+.*\bdefined\s*\(?\s*(UFT_BIG_ENDIAN|UFT_LITTLE_ENDIAN)\s*\)?",
    re.M)

_BLOCK = re.compile(r"/\*.*?\*/", re.S)
_LINE = re.compile(r"//[^\n]*")


def strip_comments(text: str) -> str:
    def blank(m):
        return "".join(c if c == chr(10) else " " for c in m.group(0))
    return _LINE.sub(blank, _BLOCK.sub(blank, text))


def sources(repo: Path):
    for p in sorted(repo.rglob("*")):
        if not p.is_file() or p.suffix.lower() not in {".c", ".cpp", ".h", ".hpp"}:
            continue
        if any(s in p.parts for s in SKIP_DIRS):
            continue
        yield p


def scan(repo: Path):
    shim_users: list[tuple[str, bool]] = []      # rel, is_header
    endian_ifdefs: list[tuple[str, int, str]] = []
    vocab_strays: list[tuple[str, int, str, str]] = []

    for p in sources(repo):
        rel = str(p.relative_to(repo)).replace("\\", "/")
        clean = strip_comments(p.read_text(encoding="utf-8", errors="replace"))

        for m in _INCLUDE.finditer(clean):
            if m.group(1).replace("\\", "/").endswith(SHIM_HEADER):
                shim_users.append((rel, p.suffix.lower() in {".h", ".hpp"}))

        for m in _VOCAB_DEF.finditer(clean):
            name = m.group(1)
            owner = _ALL_VOCAB.get(name)
            if owner and rel != owner:
                ln = clean[:m.start()].count(chr(10)) + 1
                vocab_strays.append((rel, ln, name, owner))

        # Die beiden Plattform-Header duerfen ueber sich selbst reden.
        if rel.endswith("compat/uft_platform_base.h"):
            continue
        for m in _ENDIAN_IFDEF.finditer(clean):
            name = m.group(1) or m.group(2)
            ln = clean[:m.start()].count(chr(10)) + 1
            endian_ifdefs.append((rel, ln, name))

    return shim_users, endian_ifdefs, vocab_strays


def check(repo: Path) -> list[str]:
    shim_users, endian_ifdefs, vocab_strays = scan(repo)
    errors: list[str] = []

    seen = set()
    for rel, is_header in shim_users:
        seen.add(rel)
        if is_header:
            errors.append(
                f"{rel} includes {SHIM_HEADER} from a HEADER. Die Datei belegt "
                f"close/read/write/mkdir und weitere Standardnamen; ueber einen "
                f"Header verbreitet sich das unkontrolliert weiter — genau das "
                f"brach in MF-416 187 von 197 Tests. Nimm "
                f"uft/compat/uft_platform_base.h")
        elif rel not in SHIM_CONSUMERS:
            errors.append(
                f"{rel} includes {SHIM_HEADER}. Die POSIX-Shims sind Opt-in fuer "
                f"{', '.join(sorted(SHIM_CONSUMERS))}. Wer nur Plattform-"
                f"Erkennung oder Endianness braucht, nimmt "
                f"uft/compat/uft_platform_base.h; wer die Shims wirklich "
                f"braucht, traegt sich in SHIM_CONSUMERS ein und begruendet es")

    for rel in sorted(SHIM_CONSUMERS - seen):
        errors.append(
            f"{rel} steht in SHIM_CONSUMERS, bindet {SHIM_HEADER} aber nicht "
            f"mehr ein — Eintrag entfernen, damit die Liste etwas bedeutet")

    for rel, ln, name, owner in vocab_strays:
        errors.append(
            f"{rel}:{ln} definiert {name}. Eigentuemer ist {owner} — bis MF-456 "
            f"stand dasselbe Vokabular in bis zu vier Headern mit "
            f"unterschiedlichen Ergebnissen, und welches galt, entschied die "
            f"Include-Reihenfolge. Header einbinden statt neu definieren")

    for rel, ln, name in endian_ifdefs:
        errors.append(
            f"{rel}:{ln} fragt {name} mit #ifdef/defined() ab. Seit MF-455 ist "
            f"der Wert IMMER 0 oder 1, die Abfrage also immer wahr. Richtig ist "
            f"#if {name}")
    return errors


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    shim_users, endian_ifdefs, vocab_strays = scan(repo)
    print(f"{SHIM_HEADER} eingebunden von : {len(shim_users)} Datei(en)")
    for rel, is_header in shim_users:
        print(f"    {'HEADER ' if is_header else ''}{rel}")
    print(f"#ifdef auf UFT_*_ENDIAN        : {len(endian_ifdefs)}")
    print(f"Vokabular ausserhalb des Eigentuemers: {len(vocab_strays)}")
    errs = check(repo)
    for e in errs:
        print(f"  {e}")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
