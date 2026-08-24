#!/usr/bin/env python3
"""Sieht eine Uebersetzungseinheit `UFT_FMT_*` anders als die anderen? (MF-540)

── Was hier schiefstehen kann ───────────────────────────────────────────

Der Baum hat FUENF Header, die `uft_format_id_t` definieren, und alle
fuenf benutzen denselben Waechter `UFT_FORMAT_ID_T_DEFINED`:

    include/uft/core/uft_format_registry.h    enum, UFT_FMT_D64 = 20
    include/uft/core/uft_unified_types.h      enum, UFT_FMT_D64 = 4
    include/uft/formats/uft_format_params.h   enum, UFT_FMT_D64 = 6
    include/uft/core/uft_roundtrip.h          typedef uint32_t
    include/uft/uft_format_validate.h         typedef uint32_t
    (dazu src/core/unified/uft_format_registry.h, eine sechste Fassung)

Ein geteilter Waechter heisst: **wer zuerst kommt, gewinnt, und die
uebrigen werden still uebersprungen.** Welche Zahl `UFT_FMT_D64` in einer
Uebersetzungseinheit bedeutet, entscheidet damit die Include-Reihenfolge —
ohne Warnung, ohne Fehler.

Das ist keine Vermutung. `uft_roundtrip.h` sagt es selbst (Zeile 37 ff.,
MF-265): "the UFT_FORMAT_ID_T_DEFINED guard is shared so the two
definitions never conflict in the same TU". Der Satz beschreibt die
Absicht richtig — er verhindert einen Uebersetzungsfehler. Was er nicht
verhindert, ist der stille Bedeutungswechsel.

── Warum das Tor MISST statt zu verbieten ───────────────────────────────

Beim Anlegen (MF-540) wurde der Zustand gemessen, nicht angenommen:

    src/core/uft_unified_types.c            enum(34)   UFT_FMT_D64 = 4
    src/protection/uft_protection_stubs.c   enum(34)   UFT_FMT_D64 = 4
    src/formats/uft_format_extensions.c     kein uft_format_id_t sichtbar
    src/policy/uft_write_gate.c             kein uft_format_id_t sichtbar

(Die erste Fassung dieses Kommentars sagte "enum(299)". Das war der
Zaehlfehler des kaputten Regex, nicht der Baum — siehe die Erklaerung bei
TYPEDEF weiter unten. Beide Einheiten sehen `uft_unified_types.h` mit 34
Eintraegen.)

Der Rotbeweis, der dieses Tor traegt: eine einzige zusaetzliche
`#include "uft/core/uft_format_registry.h"`-Zeile ganz oben in
`src/protection/uft_protection_stubs.c` — und das Tor meldet

    UFT_FMT_D64 bedeutet in gebautem Code 2 verschiedene Zahlen:
    src/core/uft_unified_types.c sieht UFT_FMT_D64=4,
    src/protection/uft_protection_stubs.c sieht UFT_FMT_D64=20

Also: die Kollision ist **echt**, in gebautem Code aber **nicht scharf**.
Jede Einheit, die ueberhaupt ein Enum sieht, sieht dasselbe. Ein Verbot
waere hier falsch — es wuerde einen Zustand roeten, der heute richtig ist.

Was fehlt, ist der Waechter fuer den Tag, an dem sich das aendert: eine
neue `#include`-Zeile, ein umsortierter Header, ein neues Modul, das
`uft_format_registry.h` zuerst zieht. Dann bedeutet `UFT_FMT_D64` in
einer Einheit 4 und in der naechsten 20, und niemand merkt es, weil
nichts bricht — es wird nur das Falsche berechnet.

Genau diesen Tag faengt dieses Tor.

Das Tor **konsolidiert nicht**. Fuenf Enums zu einem zusammenzulegen ist
Arbeit am ABI und gehoert nicht in eine Release-Vorbereitung; sie steht
als offener Punkt in `docs/KNOWN_ISSUES.md`. Bis dahin gilt: die Falle
ist bekannt, beziffert und ueberwacht.

── Grenzen, die zur Sache gehoeren ──────────────────────────────────────

Gemessen wird mit dem Praeprozessor des lokalen gcc. Ist keiner
auffindbar, gibt das Tor **nichts** zurueck statt zu raten — ein Tor, das
ohne Messung urteilt, ist schlimmer als keines. Die CI misst mit ihrem
eigenen Compiler und faengt es dort.

Gemessen werden nur Uebersetzungseinheiten, die `UFT_FMT_`-Konstanten
tatsaechlich benutzen. Eine Einheit, die den Typ nur durchreicht, kann
sich nicht verrechnen.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# `[^{}]*` und NICHT `.*?`: der Rumpf darf keine Klammer enthalten.
#
# Die erste Fassung dieses Tors benutzte `.*?` mit re.S. Damit begann der
# Treffer beim ERSTEN `typedef enum {` der praeprozessierten Datei — oft
# einem voellig fremden — und lief bis zum `} uft_format_id_t;`, verschluckte
# also mehrere Enums am Stueck. Der Rumpf hatte dann 299 statt 34 Eintraege,
# und `UFT_FMT_D64` wurde an einer Position gefunden, die niemandem gehoert.
#
# Das Tor meldete daraufhin "alle Einheiten sehen dieselbe Zahl" — und der
# Rotbeweis, der es roeten sollte, feuerte nicht. Ein Tor, das nicht feuern
# kann, ist kein Tor. Aufgefallen ist es nur, weil der Rotbeweis ZUERST
# gelaufen ist.
TYPEDEF = re.compile(
    r"typedef\s+enum[^{]*\{([^{}]*)\}\s*uft_format_id_t\s*;", re.S)

# Die Konstante, an der gemessen wird. D64 steht in allen drei Enums und
# liegt in jedem an anderer Stelle — ein Unterschied faellt damit sicher
# auf. Waere sie in nur einem definiert, koennte das Tor nichts vergleichen.
PROBE = "UFT_FMT_D64"


def _value_of(body: str, name: str) -> int | None:
    """Wert eines Enumerators, explizite Zuweisungen beruecksichtigt."""
    val = -1
    for raw in body.split(","):
        tok = raw.split("/*")[0].split("//")[0].strip()
        if not tok:
            continue
        if "=" in tok:
            lhs, rhs = tok.split("=", 1)
            lhs = lhs.strip()
            try:
                val = int(rhs.strip(), 0)
            except ValueError:
                # Ausdruck statt Zahl — hier nicht aufloesbar. Weiterzaehlen
                # waere geraten, also aufgeben statt falsch antworten.
                return None
        else:
            lhs = tok
            val += 1
        if lhs == name:
            return val
    return None


def _find_gcc() -> str | None:
    for cand in (shutil.which("gcc"),
                 r"C:\Qt\Tools\mingw1310_64\bin\gcc.exe",
                 "/usr/bin/gcc"):
        if cand and Path(cand).exists():
            return cand
    return None


def _users(repo: Path) -> list[Path]:
    """Uebersetzungseinheiten, die UFT_FMT_-Konstanten benutzen."""
    pat = re.compile(r"\bUFT_FMT_[A-Z0-9_]+")
    out: list[Path] = []
    for sub in ("src",):
        for p in (repo / sub).rglob("*.c"):
            try:
                if pat.search(p.read_text(encoding="utf-8", errors="replace")):
                    out.append(p)
            except OSError:
                continue
    return sorted(out)


def check(repo: Path) -> list[str]:
    gcc = _find_gcc()
    if not gcc:
        # Kein Compiler -> keine Messung -> kein Urteil. Siehe Kopfkommentar.
        return []

    incs: list[str] = []
    for d in ("include", "include/uft", "include/uft/core",
              "include/uft/formats", "src", "."):
        incs += ["-I", str(repo / d)]

    seen: dict[str, tuple[Path, int]] = {}
    errors: list[str] = []

    for src in _users(repo):
        try:
            r = subprocess.run([gcc, "-E", "-P", str(src)] + incs,
                               capture_output=True, text=True,
                               errors="replace", timeout=120)
        except (OSError, subprocess.TimeoutExpired):
            continue
        if not r.stdout:
            continue
        m = TYPEDEF.search(r.stdout)
        if not m:
            continue                      # sieht kein Enum — kann nichts irren
        val = _value_of(m.group(1), PROBE)
        if val is None:
            continue

        key = str(val)
        if key in seen:
            continue
        seen[key] = (src, val)

    if len(seen) > 1:
        rel = lambda p: p.relative_to(repo).as_posix()
        parts = ", ".join(
            f"{rel(p)} sieht {PROBE}={v}" for p, v in seen.values())
        errors.append(
            f"{PROBE} bedeutet in gebautem Code {len(seen)} verschiedene "
            f"Zahlen: {parts}. Fuenf Header definieren uft_format_id_t unter "
            f"demselben Waechter UFT_FORMAT_ID_T_DEFINED — die "
            f"Include-Reihenfolge entscheidet still, welcher gilt. Beim "
            f"Anlegen dieses Tors (MF-540) war der Wert ueberall 4. Wer das "
            f"geaendert hat, hat eine Bedeutung verschoben, nicht nur eine "
            f"Zeile. Siehe docs/KNOWN_ISSUES.md, Abschnitt ID-1.")

    return errors


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    if not _find_gcc():
        print("kein gcc auffindbar — nicht gemessen, kein Urteil")
        return 0
    errs = check(repo)
    for src in _users(repo):
        print(f"  benutzt UFT_FMT_*: {src.relative_to(repo).as_posix()}")
    if errs:
        print("\nBEFUND:")
        for e in errs:
            print(f"  {e}")
        return 1
    print("\nOK: alle messbaren Uebersetzungseinheiten sehen dieselbe Zahl")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
